#include "nvg_widgets.h"
#include <stdio.h>
#include <string.h>

void nvgw_init(NvgWidgetCtx *ctx, NVGcontext *vg) {
  memset(ctx, 0, sizeof(NvgWidgetCtx));
  ctx->vg = vg;
}

void nvgw_set_fonts(NvgWidgetCtx *ctx, int font_main, int font_icons) {
  ctx->font_main = font_main;
  ctx->font_icons = font_icons;
}

void nvgw_input_begin(NvgWidgetCtx *ctx) {
  ctx->hot_id = 0;
  // reset single frame inputs handled in event loop
  ctx->text_input[0] = '\0';
  ctx->key_backspace = false;
  ctx->key_enter = false;
  ctx->scroll_y = 0.0f;
  ctx->mouse_clicked = false;
  ctx->mouse_released = false;
}

void nvgw_input_mouse(NvgWidgetCtx *ctx, float mx, float my, bool mdown,
                      float scroll_y) {
  ctx->mouse_x = mx;
  ctx->mouse_y = my;
  ctx->mouse_down_prev = ctx->mouse_down;

  if (mdown && !ctx->mouse_down)
    ctx->mouse_clicked = true;
  if (!mdown && ctx->mouse_down)
    ctx->mouse_released = true;

  ctx->mouse_down = mdown;
  ctx->scroll_y += scroll_y;
}

void nvgw_input_keyboard(NvgWidgetCtx *ctx, const char *text_input,
                         bool backspace, bool enter) {
  if (text_input) {
    strncpy(ctx->text_input, text_input, sizeof(ctx->text_input) - 1);
    ctx->text_input[sizeof(ctx->text_input) - 1] = '\0';
  }
  ctx->key_backspace = backspace;
  ctx->key_enter = enter;
}

void nvgw_input_end(NvgWidgetCtx *ctx) {
  if (!ctx->mouse_down) {
    ctx->active_id = 0;
  } else {
    if (ctx->active_id == 0 && ctx->hot_id != 0) {
      ctx->active_id = ctx->hot_id;
    }
  }

  // Clicking outside unfocuses
  if (ctx->mouse_clicked && ctx->hot_id != ctx->focus_id) {
    ctx->focus_id = 0;
  }
}

// Jenkins one at a time hash
uint32_t nvgw_id(const void *data, int len) {
  const uint8_t *key = (const uint8_t *)data;
  uint32_t hash = 0;
  for (int i = 0; i < len; ++i) {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return hash ? hash : 1; // 0 is reserved
}

uint32_t nvgw_id_str(const char *str) { return nvgw_id(str, strlen(str)); }

void nvgw_set_offset(NvgWidgetCtx *ctx, float ox, float oy) {
  ctx->offset_x = ox;
  ctx->offset_y = oy;
}

static bool point_in_rect(float px, float py, float x, float y, float w,
                          float h, float ox, float oy) {
  float local_mx = px - ox;
  float local_my = py - oy;
  return local_mx >= x && local_mx <= x + w && local_my >= y && local_my <= y + h;
}

// --- Widgets ---

void nvgw_label(NvgWidgetCtx *ctx, float x, float y, float w, float h,
                const char *text, int align, NVGcolor color, float font_size) {
  if (!text || text[0] == '\0')
    return;
  NVGcontext *vg = ctx->vg;

  nvgSave(vg);
  nvgFontSize(vg, font_size);
  nvgFontFaceId(vg, ctx->font_main);
  nvgFillColor(vg, color);

  float ty = y + h * 0.5f;
  float tx = x;

  int nvgAlign = NVG_ALIGN_MIDDLE;
  if (align == 0) { // LEFT
    nvgAlign |= NVG_ALIGN_LEFT;
  } else if (align == 1) { // CENTER
    nvgAlign |= NVG_ALIGN_CENTER;
    tx = x + w * 0.5f;
  } else { // RIGHT
    nvgAlign |= NVG_ALIGN_RIGHT;
    tx = x + w;
  }

  nvgTextAlign(vg, nvgAlign);
  nvgText(vg, tx, ty, text, NULL);
  nvgRestore(vg);
}

bool nvgw_button(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                 float h, const char *text) {
  bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h, ctx->offset_x, ctx->offset_y);
  if (hovered) {
    ctx->hot_id = id;
  }

  bool clicked = false;

  if (ctx->active_id == id && ctx->mouse_released && hovered) {
    clicked = true;
  }
  if (ctx->hot_id == id && ctx->mouse_clicked) {
    ctx->active_id = id;
  }

  NVGcontext *vg = ctx->vg;

  NVGcolor bg_col = nvgRGBA(240, 240, 240, 255);
  NVGcolor text_col = nvgRGB(50, 50, 50);

  if (ctx->active_id == id) {
    bg_col = nvgRGBA(200, 200, 200, 255);
  } else if (ctx->hot_id == id) {
    bg_col = nvgRGBA(225, 225, 225, 255);
  }

  // Draw background
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgFillColor(vg, bg_col);
  nvgFill(vg);

  // Draw border
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 20));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  // Draw text
  nvgSave(vg);
  nvgFontSize(vg, 14.0f);
  nvgFontFaceId(vg, ctx->font_main);
  nvgFillColor(vg, text_col);
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, x + w * 0.5f, y + h * 0.5f, text, NULL);
  nvgRestore(vg);

  return clicked;
}

bool nvgw_button_icon(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                      float h, const char *icon_utf8) {
  bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h, ctx->offset_x, ctx->offset_y);
  if (hovered) {
    ctx->hot_id = id;
  }

  bool clicked = false;
  if (ctx->active_id == id && ctx->mouse_released && hovered) {
    clicked = true;
  }
  if (ctx->hot_id == id && ctx->mouse_clicked) {
    ctx->active_id = id;
  }

  NVGcontext *vg = ctx->vg;
  NVGcolor text_col = nvgRGB(100, 100, 100);

  if (ctx->active_id == id) {
    // Active bg
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, 4.0f);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 20));
    nvgFill(vg);
    text_col = nvgRGB(30, 30, 30);
  } else if (ctx->hot_id == id) {
    // Hover bg
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, 4.0f);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 10));
    nvgFill(vg);
    text_col = nvgRGB(50, 50, 50);
  }

  // Draw icon
  nvgSave(vg);
  nvgFontSize(vg, 16.0f); // Default icon size
  nvgFontFaceId(vg, ctx->font_icons);
  nvgFillColor(vg, text_col);
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, x + w * 0.5f, y + h * 0.5f, icon_utf8, NULL);
  nvgRestore(vg);

  return clicked;
}

void nvgw_panel(NvgWidgetCtx *ctx, float x, float y, float w, float h,
                NVGcolor bg_color, float corner_radius) {
  NVGcontext *vg = ctx->vg;
  nvgBeginPath(vg);
  if (corner_radius > 0.0f) {
    nvgRoundedRect(vg, x, y, w, h, corner_radius);
  } else {
    nvgRect(vg, x, y, w, h);
  }
  nvgFillColor(vg, bg_color);
  nvgFill(vg);
}

// Single-line Text Input
bool nvgw_text_input(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                     float h, char *buffer, size_t buffer_size,
                     float font_size) {
  bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h, ctx->offset_x, ctx->offset_y);
  if (hovered) {
    ctx->hot_id = id;
  }

  if (ctx->hot_id == id && ctx->mouse_clicked) {
    ctx->focus_id = id;
  }

  bool finished = false;
  bool is_focused = (ctx->focus_id == id);

  if (is_focused) {
    // Handle input string
    size_t len = strlen(buffer);
    if (ctx->text_input[0] != '\0' &&
        len + strlen(ctx->text_input) < buffer_size) {
      strcat(buffer, ctx->text_input);
    }

    if (ctx->key_backspace && len > 0) {
      buffer[len - 1] = '\0'; // simple backspace (ASCII only)
    }

    if (ctx->key_enter) {
      finished = true;
      ctx->focus_id = 0; // defocus
    }
  }

  NVGcontext *vg = ctx->vg;

  // Draw bg
  NVGcolor bg_col =
      is_focused ? nvgRGBA(255, 255, 255, 255) : nvgRGBA(250, 250, 250, 255);
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgFillColor(vg, bg_col);
  nvgFill(vg);

  // Draw border
  NVGcolor border_col =
      is_focused ? nvgRGBA(60, 120, 200, 255) : nvgRGBA(0, 0, 0, 30);
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgStrokeColor(vg, border_col);
  nvgStrokeWidth(vg, is_focused ? 1.5f : 1.0f);
  nvgStroke(vg);

  // Draw Text
  nvgSave(vg);
  nvgFontSize(vg, font_size);
  nvgFontFaceId(vg, ctx->font_main);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgFillColor(vg, nvgRGB(30, 30, 30));

  // Scissor to avoid text bleeding out
  nvgScissor(vg, x + 4, y, w - 8, h);

  float bounds[4];
  nvgTextBounds(vg, x + 6, y + h * 0.5f, buffer, NULL, bounds);
  float text_w = bounds[2] - bounds[0];

  float text_x = x + 6;
  if (text_w > w - 12) {
    // Shift text left so cursor is visible
    text_x = x + 6 - (text_w - (w - 12));
  }

  nvgText(vg, text_x, y + h * 0.5f, buffer, NULL);

  if (is_focused) {
    // Draw primitive cursor at the end
    nvgBeginPath(vg);
    nvgRect(vg, text_x + text_w + 1, y + h * 0.2f, 1.0f, h * 0.6f);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);
  }

  nvgResetScissor(vg);
  nvgRestore(vg);

  return finished;
}

bool nvgw_checkbox(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                   float h, const char *text, bool *checked) {
  bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h, ctx->offset_x, ctx->offset_y);
  if (hovered) {
    ctx->hot_id = id;
  }

  bool toggled = false;
  if (ctx->active_id == id && ctx->mouse_released && hovered) {
    *checked = !(*checked);
    toggled = true;
  }
  if (ctx->hot_id == id && ctx->mouse_clicked) {
    ctx->active_id = id;
  }

  NVGcontext *vg = ctx->vg;
  float box_size = 14.0f;
  float box_y = y + (h - box_size) * 0.5f;

  // Box bg
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, box_y, box_size, box_size, 3.0f);
  if (*checked) {
    nvgFillColor(vg, nvgRGB(26, 115, 232)); // Accent primary
  } else {
    nvgFillColor(vg, nvgRGB(240, 240, 240));
  }
  nvgFill(vg);

  // Box border
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, box_y, box_size, box_size, 3.0f);
  nvgStrokeColor(vg, *checked ? nvgRGB(20, 90, 180) : nvgRGBA(0, 0, 0, 50));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  // Checkmark
  if (*checked) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + 3.5f, box_y + 7.5f);
    nvgLineTo(vg, x + 6.0f, box_y + 10.0f);
    nvgLineTo(vg, x + 10.5f, box_y + 4.0f);
    nvgStrokeColor(vg, nvgRGB(255, 255, 255));
    nvgStrokeWidth(vg, 2.0f);
    nvgStroke(vg);
  }

  // Text
  if (text) {
    nvgw_label(ctx, x + box_size + 6, y, w - box_size - 6, h, text, 0,
               nvgRGB(50, 50, 50), 14.0f);
  }

  return toggled;
}

void nvgw_begin_scroll_area(NvgWidgetCtx *ctx, float x, float y, float w,
                            float h, float content_h, float *offset_y) {
  bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h, ctx->offset_x, ctx->offset_y);
  if (hovered && ctx->scroll_y != 0.0f) {
    *offset_y -= ctx->scroll_y * 10.0f;
  }

  if (*offset_y < 0.0f)
    *offset_y = 0.0f;
  if (content_h > h && *offset_y > content_h - h) {
    *offset_y = content_h - h;
  } else if (content_h <= h) {
    *offset_y = 0.0f;
  }

  NVGcontext *vg = ctx->vg;
  nvgSave(vg);
  nvgScissor(vg, x, y, w, h);
}

void nvgw_end_scroll_area(NvgWidgetCtx *ctx) {
  NVGcontext *vg = ctx->vg;
  nvgResetScissor(vg);
  nvgRestore(vg); // Retore the state pushed in begin
}

// --- New Widgets for Aslan Table ---

bool nvgw_color_picker(NvgWidgetCtx *ctx, uint32_t id, float x, float y,
                       float w, float h, NVGcolor *color) {
  bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h, ctx->offset_x, ctx->offset_y);
  if (hovered)
    ctx->hot_id = id;

  bool clicked = false;
  if (ctx->active_id == id && ctx->mouse_released && hovered) {
    clicked = true;
  }
  if (ctx->hot_id == id && ctx->mouse_clicked) {
    ctx->active_id = id;
  }

  NVGcontext *vg = ctx->vg;

  // Draw background/border
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgFillColor(vg, *color);
  nvgFill(vg);

  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 50));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  return clicked; // usually opens a popup in the host
}

bool nvgw_date_picker(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                      float h, int *year, int *month, int *day) {
  bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h, ctx->offset_x, ctx->offset_y);
  if (hovered)
    ctx->hot_id = id;

  bool clicked = false;
  if (ctx->active_id == id && ctx->mouse_released && hovered) {
    clicked = true;
  }
  if (ctx->hot_id == id && ctx->mouse_clicked) {
    ctx->active_id = id;
  }

  NVGcontext *vg = ctx->vg;

  // Draw bg
  NVGcolor bg_col = (ctx->active_id == id)
                        ? nvgRGBA(200, 200, 200, 255)
                        : (hovered ? nvgRGBA(225, 225, 225, 255)
                                   : nvgRGBA(240, 240, 240, 255));
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgFillColor(vg, bg_col);
  nvgFill(vg);

  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 30));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  char buf[32];
  if (*year > 0) {
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", *year, *month + 1, *day);
  } else {
    snprintf(buf, sizeof(buf), "YYYY-MM-DD");
  }

  nvgSave(vg);
  nvgFontSize(vg, 14.0f);
  nvgFontFaceId(vg, ctx->font_main);
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgFillColor(vg, nvgRGB(50, 50, 50));
  nvgText(vg, x + w * 0.5f, y + h * 0.5f, buf, NULL);
  nvgRestore(vg);

  return clicked; // Host handles popup logic
}

bool nvgw_dropdown_select(NvgWidgetCtx *ctx, uint32_t id, float x, float y,
                          float w, float h, const char **options,
                          int option_count, int *selected_index) {
  bool hovered = point_in_rect(ctx->mouse_x, ctx->mouse_y, x, y, w, h, ctx->offset_x, ctx->offset_y);
  if (hovered)
    ctx->hot_id = id;

  bool clicked = false;
  if (ctx->active_id == id && ctx->mouse_released && hovered) {
    clicked = true;
  }
  if (ctx->hot_id == id && ctx->mouse_clicked) {
    ctx->active_id = id;
  }

  NVGcontext *vg = ctx->vg;

  // Draw bg
  NVGcolor bg_col = (ctx->active_id == id)
                        ? nvgRGBA(200, 200, 200, 255)
                        : (hovered ? nvgRGBA(225, 225, 225, 255)
                                   : nvgRGBA(240, 240, 240, 255));
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgFillColor(vg, bg_col);
  nvgFill(vg);

  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 30));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  const char *text = (*selected_index >= 0 && *selected_index < option_count)
                         ? options[*selected_index]
                         : "Select...";

  nvgSave(vg);
  nvgFontSize(vg, 14.0f);
  nvgFontFaceId(vg, ctx->font_main);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgFillColor(vg, nvgRGB(50, 50, 50));
  nvgText(vg, x + 6, y + h * 0.5f, text, NULL);

  // Draw arrow
  nvgBeginPath(vg);
  nvgMoveTo(vg, x + w - 16, y + h * 0.5f - 2);
  nvgLineTo(vg, x + w - 8, y + h * 0.5f - 2);
  nvgLineTo(vg, x + w - 12, y + h * 0.5f + 4);
  nvgFillColor(vg, nvgRGB(100, 100, 100));
  nvgFill(vg);

  nvgRestore(vg);

  return clicked;
}
