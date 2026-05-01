#include "ui_toolbar.h"
#include "ui_canvas.h"
#include "ui_shared.h"
#include "ui_host.h"

void canvas_render_sub_menu_dropdown(UIHost *ui, float x, float y, CanvasCategory cat) {
  const float item_h = 32.0f;
  const float menu_padding = 8.0f;
  const float menu_w = 140.0f;
  int item_count = 0;

  const char *items[8];
  const char *icons[8];
  CanvasSubOption sub_opts[8];

  if (cat == CANVAS_CAT_SHAPES) {
    items[0] = "Rectangle"; icons[0] = ICON_SHAPE; sub_opts[0] = CANVAS_SUB_SHAPE_RECT;
    items[1] = "Ellipse"; icons[1] = "\xEE\xAD\xA4"; sub_opts[1] = CANVAS_SUB_SHAPE_ELLIPSE;
    items[2] = "Triangle"; icons[2] = ICON_SHAPE; sub_opts[2] = CANVAS_SUB_SHAPE_TRIANGLE;
    items[3] = "Line"; icons[3] = "\xEF\x84\xA5"; sub_opts[3] = CANVAS_SUB_SHAPE_LINE;
    item_count = 4;
  } else if (cat == CANVAS_CAT_TEXT) {
    items[0] = "Heading"; icons[0] = ICON_H1; sub_opts[0] = CANVAS_SUB_TEXT_HEADING;
    items[1] = "Body"; icons[1] = ICON_TEXT; sub_opts[1] = CANVAS_SUB_TEXT_BODY;
    items[2] = "Sticky"; icons[2] = ICON_STICKY; sub_opts[2] = CANVAS_SUB_TEXT_STICKY;
    item_count = 3;
  } else if (cat == CANVAS_CAT_MEDIA) {
    items[0] = "Image"; icons[0] = ICON_IMAGE; sub_opts[0] = CANVAS_SUB_MEDIA_IMAGE;
    item_count = 1;
  }
  if (item_count == 0) return;

  float menu_h = item_count * item_h + 2 * menu_padding;
  float menu_x = x - menu_w / 2.0f + 14.0f;
  float menu_y = y + 28.0f + 12.0f; // Drop DOWN

  // Shadow
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, menu_x + 2, menu_y + 4, menu_w, menu_h, 12.0f);
  nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 30));
  nvgFill(ui->vg);

  // Background
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, menu_x, menu_y, menu_w, menu_h, 12.0f);
  nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
  nvgFill(ui->vg);
  nvgStrokeColor(ui->vg, nvgRGBA(230, 230, 230, 255));
  nvgStrokeWidth(ui->vg, 1.0f);
  nvgStroke(ui->vg);

  for (int i = 0; i < item_count; i++) {
    float ir_x = menu_x + menu_padding, ir_y = menu_y + menu_padding + i * item_h;
    float ir_w = menu_w - 2 * menu_padding, ir_h = item_h;
    int hovered = (ui->mouse_x >= ir_x && ui->mouse_x < ir_x + ir_w && ui->mouse_y >= ir_y && ui->mouse_y < ir_y + ir_h);

    if (hovered) {
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, ir_x, ir_y, ir_w, ir_h, 6.0f);
      nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 15));
      nvgFill(ui->vg);

      if (ui->input.mouse_just_pressed) {
        if (cat == CANVAS_CAT_SHAPES) {
          ui->input.space_held = 0; ui->input.pan_from_menu = 0; ui->input.mode = INTERACTION_NONE;
          if (sub_opts[i] == CANVAS_SUB_SHAPE_RECT) ui->input.pending_create = 10;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_ELLIPSE) ui->input.pending_create = 11;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_TRIANGLE) ui->input.pending_create = 12;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_LINE) ui->input.pending_create = 13;
        } else if (cat == CANVAS_CAT_TEXT) {
          ui->editor.active_tool = CANVAS_TOOL_SELECT;
          ui->input.space_held = 0; ui->input.pan_from_menu = 0; ui->input.mode = INTERACTION_NONE;
          if (sub_opts[i] == CANVAS_SUB_TEXT_HEADING) ui->input.pending_create = 6;
          else if (sub_opts[i] == CANVAS_SUB_TEXT_BODY) ui->input.pending_create = 1;
          else if (sub_opts[i] == CANVAS_SUB_TEXT_STICKY) ui->input.pending_create = 2;
        } else if (cat == CANVAS_CAT_MEDIA) {
          ui->editor.active_tool = CANVAS_TOOL_SELECT;
          ui->input.space_held = 0; ui->input.pan_from_menu = 0; ui->input.mode = INTERACTION_NONE;
          if (sub_opts[i] == CANVAS_SUB_MEDIA_IMAGE) ui->input.pending_create = 3;
        }
        ui->editor.show_sub_menu = 0;
      }
    }

    if (ui->font_icons) {
      nvgFontSize(ui->vg, 20.0f);
      nvgFontFace(ui->vg, "Icons");
      nvgFillColor(ui->vg, nvgRGBA(80, 80, 80, 255));
      nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ui->vg, ir_x + 8, ir_y + 16, icons[i], NULL);
    }
    if (ui->font_inter) {
      nvgFontSize(ui->vg, ui->font_size_px);
      nvgFontFace(ui->vg, "Inter");
      nvgFillColor(ui->vg, nvgRGBA(40, 40, 40, 255));
      nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ui->vg, ir_x + 36, ir_y + 16, items[i], NULL);
    }
  }
}

int topbar_point_in_tools(UIHost *ui, float right_x, float top_y, float top_h, float mx, float my) {
  if (ui->editor.show_sub_menu) {
    const float menu_w = 140.0f;
    const float item_h = 32.0f;
    const float menu_padding = 8.0f;
    int items = (ui->editor.active_category == CANVAS_CAT_SHAPES) ? 4 : ((ui->editor.active_category == CANVAS_CAT_TEXT) ? 3 : 1);
    float menu_h = items * item_h + 2 * menu_padding;
    if (my >= top_y + top_h && my <= top_y + top_h + menu_h + 20.0f) {
      if (mx >= right_x - 300.0f && mx <= right_x) return 1;
    }
  }
  return 0;
}

void ui_render_design_topbar_tools(UIHost *ui, float right_x, float top_y, float top_h) {
  NVGcontext *vg = ui->vg;
  const float btn_size   = 28.0f;
  const float btn_gap    = 2.0f;
  const float pad        = 6.0f;
  const int   tool_count = 5;
  const float grp_w = tool_count * btn_size + (tool_count - 1) * btn_gap + 2.0f * pad;
  const float grp_h = btn_size + 2.0f * pad;
  const float grp_y = top_y + (top_h - grp_h) / 2.0f;
  const float grp_x = right_x - grp_w;

  NVGcolor ic_active   = nvgRGBA(255, 255, 255, 230);
  NVGcolor ic_idle     = nvgRGBA(255, 255, 255, 110);
  NVGcolor pill_active = nvgRGBA(255, 255, 255, 38);
  NVGcolor hover_bg    = nvgRGBA(255, 255, 255, 18);
  NVGcolor press_bg    = nvgRGBA(255, 255, 255, 55);

  float sep_x = grp_x - 10.0f;
  float sep_y1 = top_y + top_h * 0.25f;
  float sep_y2 = top_y + top_h * 0.75f;
  nvgBeginPath(vg);
  nvgMoveTo(vg, sep_x, sep_y1);
  nvgLineTo(vg, sep_x, sep_y2);
  nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 30));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  nvgBeginPath(vg);
  nvgRoundedRect(vg, grp_x, grp_y, grp_w, grp_h, 8.0f);
  nvgFillColor(vg, nvgRGBA(255, 255, 255, 12));
  nvgFill(vg);

  const char *tool_icons[] = { ICON_SHAPE, ICON_TEXT, ICON_IMAGE, ICON_CHART, "\xEF\x87\x9E" };
  const char *tool_ids[]   = { "tool_shape", "tool_text", "tool_image", "tool_chart", "tool_table" };

  float bx = grp_x + pad;
  float by = grp_y + pad;

  for (int i = 0; i < tool_count; i++) {
    float ix = bx + i * (btn_size + btn_gap);

    int is_active = (ui->editor.active_category == (CanvasCategory)(i + 1));
    if (i == 3) is_active = (ui->input.pending_create == 11);
    if (i == 4) is_active = 0;

    if (is_active) {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, ix, by, btn_size, btn_size, 6.0f);
      nvgFillColor(vg, pill_active);
      nvgFill(vg);
    }

    NVGcolor col = is_active ? ic_active : ic_idle;
    if (nvgw_button_icon_ex(&ui->nvgw, nvgw_id_str(tool_ids[i]),
                            ix, by, btn_size, btn_size,
                            tool_icons[i], 15.0f, col, hover_bg, press_bg)) {
      CanvasCategory cat = (CanvasCategory)(i + 1);
      if (cat == CANVAS_CAT_SHAPES) {
        ui->input.pending_create = 10;
        ui->editor.show_sub_menu = 0;
      } else if (cat == CANVAS_CAT_TEXT) {
        ui->input.pending_create = 1;
        ui->editor.show_sub_menu = 0;
      } else if (cat == CANVAS_CAT_MEDIA) {
        ui->input.pending_create = 3;
        ui->editor.show_sub_menu = 0;
      } else if (i == 3) {
        ui->input.pending_create = 11;
        ui->editor.show_sub_menu = 0;
      } else if (i == 4) {
        ui->input.pending_create = 30;
        ui->editor.show_sub_menu = 0;
      } else if (ui->editor.active_category == cat) {
        ui->editor.show_sub_menu = !ui->editor.show_sub_menu;
      } else {
        ui->editor.active_category = cat;
        ui->editor.show_sub_menu = 1;
      }
      ui_request_redraw(ui);
    }

    if (is_active && ui->editor.show_sub_menu) {
      canvas_render_sub_menu_dropdown(ui, ix, by, (CanvasCategory)(i + 1));
    }
  }
}
