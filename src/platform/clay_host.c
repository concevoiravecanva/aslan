// =============================================================================
// ASLAN Clay Host - SDL2 Backend + Layout Engine
// =============================================================================

// Clay single-header implementation (must be in exactly one .c file)
#define CLAY_IMPLEMENTATION
#include "../vendor/clay.h"

#include "clay_host.h"

#include "../vendor/nanovg/nanovg.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Internal types ───────────────────────────────────────────────────────────
#define ASLAN_FONT_INTER 0
#define ASLAN_FONT_BOLD 1
#define ASLAN_FONT_ICONS 2
#define ASLAN_FONT_COUNT 3

typedef struct {
  uint32_t fontId;
  int handle;
} AslanFont;

struct ClayHost {
  void *memory;
  NVGcontext *vg;
  AslanFont fonts[ASLAN_FONT_COUNT];
  AslanSplitRects last_rects;
  // Per-frame mouse state (set by clay_host_begin_frame)
  int mouse_x, mouse_y;
  bool mouse_pressed_this_frame;
  bool last_mouse_left_down;
};

// Rendering handled by clay_nanovg_render below

// Process a Clay render command array (rectangles, borders, scissor only)
static void clay_nanovg_render(NVGcontext *vg, Clay_RenderCommandArray cmds,
                               AslanFont *fonts) {
  for (int i = 0; i < (int)cmds.length; i++) {
    Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
    Clay_BoundingBox bb = cmd->boundingBox;

    switch (cmd->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleRenderData *rd = &cmd->renderData.rectangle;
      nvgBeginPath(vg);
      float radius = rd->cornerRadius.topLeft;
      if (radius > 0)
        nvgRoundedRect(vg, bb.x, bb.y, bb.width, bb.height, radius);
      else
        nvgRect(vg, bb.x, bb.y, bb.width, bb.height);
      nvgFillColor(vg, nvgRGBA(rd->backgroundColor.r, rd->backgroundColor.g,
                               rd->backgroundColor.b, rd->backgroundColor.a));
      nvgFill(vg);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextRenderData *td = &cmd->renderData.text;
      nvgFontFaceId(vg, fonts[td->fontId].handle);
      nvgFontSize(vg, td->fontSize);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
      nvgFillColor(vg, nvgRGBA(td->textColor.r, td->textColor.g,
                               td->textColor.b, td->textColor.a));

      char buf[1024];
      int len = td->stringContents.length < 1023
                    ? (int)td->stringContents.length
                    : 1023;
      memcpy(buf, td->stringContents.chars, len);
      buf[len] = '\0';
      nvgText(vg, bb.x, bb.y, buf, NULL);
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      nvgScissor(vg, bb.x, bb.y, bb.width, bb.height);
      break;
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      nvgResetScissor(vg);
      break;
    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
      Clay_BorderRenderData *bd = &cmd->renderData.border;
      if (bd->width.top > 0 || bd->width.bottom > 0 || bd->width.left > 0 ||
          bd->width.right > 0) {
        nvgBeginPath(vg);
        float radius = bd->cornerRadius.topLeft;
        if (radius > 0)
          nvgRoundedRect(vg, bb.x, bb.y, bb.width, bb.height, radius);
        else
          nvgRect(vg, bb.x, bb.y, bb.width, bb.height);
        nvgStrokeColor(
            vg, nvgRGBA(bd->color.r, bd->color.g, bd->color.b, bd->color.a));
        nvgStrokeWidth(vg, (float)bd->width.top);
        nvgStroke(vg);
      }
      break;
    }
    default:
      break;
    }
  }
}

// Public wrapper to render a custom Clay_RenderCommandArray
void clay_host_render_clay_commands(ClayHost *ch, Clay_RenderCommandArray cmds) {
  if (!ch)
    return;
  clay_nanovg_render(ch->vg, cmds, ch->fonts);
}

bool clay_host_mouse_pressed(ClayHost *ch) {
  if (!ch)
    return false;
  return ch->mouse_pressed_this_frame;
}

// =============================================================================
// Text measurement (required by Clay even if we don't use Clay text yet)
// =============================================================================

static Clay_Dimensions measure_text(Clay_StringSlice text,
                                    Clay_TextElementConfig *cfg,
                                    void *userData) {
  ClayHost *ch = (ClayHost *)userData;
  if (!ch || !ch->vg) {
    return (Clay_Dimensions){0.0f, 0.0f};
  }

  NVGcontext *vg = ch->vg;
  AslanFont *font_data = &ch->fonts[cfg->fontId];

  if (!font_data->handle) { // Assuming handle is the NanoVG font ID
    return (Clay_Dimensions){0.0f, 0.0f};
  }

  // NanoVG requires nvgBeginFrame/EndFrame to be called around drawing
  // operations. For text measurement, we can call nvgBeginFrame/EndFrame
  // internally or assume it's handled by the caller. Clay's measure_text is
  // called during layout, outside of the main rendering loop, so we should set
  // up the context. However, nvgTextBounds itself doesn't strictly require
  // begin/end frame if no actual drawing is happening, but it's good practice
  // to set up the state. For simplicity and to avoid nested begin/end frames if
  // Clay calls this during a frame, we'll just set the font properties and get
  // bounds. The `nvgTextBounds` function itself doesn't draw, so it's generally
  // safe.

  nvgFontSize(vg, cfg->fontSize);
  nvgFontFaceId(vg, font_data->handle);
  // NanoVG text alignment is important for bounds calculation.
  // Clay doesn't specify alignment for measurement, so let's use default.
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

  char buf[512] = {0};
  int len = text.length < 511 ? (int)text.length : 511;
  memcpy(buf, text.chars, len);
  buf[len] = '\0'; // Ensure null termination

  float bounds[4]; // x, y, width, height
  nvgTextBounds(vg, 0, 0, buf, NULL, bounds);

  return (Clay_Dimensions){.width = bounds[2] - bounds[0],
                           .height = bounds[3] - bounds[1]};
}

static void clay_error(Clay_ErrorData err) {
  fprintf(stderr, "[Clay] %.*s\n", (int)err.errorText.length,
          err.errorText.chars);
}

// =============================================================================
// Lifecycle
// =============================================================================

ClayHost *clay_host_init(NVGcontext *vg, int win_w, int win_h) {
  ClayHost *ch = (ClayHost *)calloc(1, sizeof(ClayHost));
  if (!ch)
    return NULL;

  ch->vg = vg;

  // Load fonts for NanoVG
  ch->fonts[ASLAN_FONT_INTER].fontId = ASLAN_FONT_INTER;
  ch->fonts[ASLAN_FONT_INTER].handle =
      nvgCreateFont(vg, "Inter", "assets/fonts/Inter-Regular.ttf");

  ch->fonts[ASLAN_FONT_BOLD].fontId = ASLAN_FONT_BOLD;
  ch->fonts[ASLAN_FONT_BOLD].handle =
      nvgCreateFont(vg, "InterBold", "assets/fonts/Inter_28pt-Bold.ttf");

  ch->fonts[ASLAN_FONT_ICONS].fontId = ASLAN_FONT_ICONS;
  ch->fonts[ASLAN_FONT_ICONS].handle =
      nvgCreateFont(vg, "Icons", "assets/remixicon.ttf");

  // Initialize Clay arena
  uint32_t mem_size = Clay_MinMemorySize();
  ch->memory = malloc(mem_size);
  if (!ch->memory) {
    free(ch);
    return NULL;
  }

  Clay_Arena arena =
      Clay_CreateArenaWithCapacityAndMemory(mem_size, ch->memory);
  Clay_Initialize(arena, (Clay_Dimensions){(float)win_w, (float)win_h},
                  (Clay_ErrorHandler){clay_error, NULL});
  Clay_SetMeasureTextFunction(measure_text, ch);

  return ch;
}

void clay_host_destroy(ClayHost *ch) {
  if (!ch)
    return;
  free(ch->memory);
  free(ch);
}

void clay_host_update_size(ClayHost *ch, int win_w, int win_h) {
  if (!ch)
    return;
  Clay_SetLayoutDimensions((Clay_Dimensions){(float)win_w, (float)win_h});
}

// =============================================================================
// Per-frame input update — call once at the start of each frame
// =============================================================================

void clay_host_begin_frame(ClayHost *ch) {
  if (!ch)
    return;
  int mx, my;
  Uint32 mstate = SDL_GetMouseState(&mx, &my);
  bool down = (mstate & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
  ch->mouse_pressed_this_frame = down && !ch->last_mouse_left_down;
  ch->last_mouse_left_down = down;
  ch->mouse_x = mx;
  ch->mouse_y = my;

  Clay_SetPointerState((Clay_Vector2){(float)mx, (float)my}, down);
}

// =============================================================================
// Top navigation bar (Step 4)
// =============================================================================

// Helper: render a single line of text centred vertically in a rect using
// NanoVG
static void nvg_draw_text(NVGcontext *vg, int font_handle, float fontSize,
                          const char *text, NVGcolor col, float x, float y,
                          float h) {
  if (!vg || font_handle < 0 || !text || !text[0])
    return;
  nvgFontFaceId(vg, font_handle);
  nvgFontSize(vg, fontSize);
  nvgFillColor(vg, col);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(vg, x, y + h * 0.5f, text, NULL);
}

// Helper: fill a rounded rectangle using NanoVG
static void nvg_fill_rounded_rect(NVGcontext *vg, float x, float y, float w,
                                  float h, float r, NVGcolor col) {
  nvgBeginPath(vg);
  if (r > 0)
    nvgRoundedRect(vg, x, y, w, h, r);
  else
    nvgRect(vg, x, y, w, h);
  nvgFillColor(vg, col);
  nvgFill(vg);
}

AslanNavbarResult clay_host_render_navbar(ClayHost *ch, bool dark_mode,
                                          int win_w, int win_h,
                                          const char *doc_title) {
  (void)win_h;
  AslanNavbarResult result = {0};
  if (!ch || !ch->vg)
    return result;

  NVGcontext *vg = ch->vg;

  // ── Colours ──────────────────────────────────────────────────────────────
  NVGcolor bg, sep, title_c, btn_bg, btn_bg_hov, btn_txt;
  if (dark_mode) {
    bg = nvgRGBA(24, 26, 32, 255);
    sep = nvgRGBA(42, 44, 52, 255);
    title_c = nvgRGBA(130, 132, 145, 255);
    btn_bg = nvgRGBA(40, 42, 52, 255);
    btn_bg_hov = nvgRGBA(54, 56, 68, 255);
    btn_txt = nvgRGBA(170, 172, 185, 255);
  } else {
    bg = nvgRGBA(250, 250, 252, 255);
    sep = nvgRGBA(220, 220, 226, 255);
    title_c = nvgRGBA(140, 140, 155, 255);
    btn_bg = nvgRGBA(238, 238, 244, 255);
    btn_bg_hov = nvgRGBA(225, 225, 232, 255);
    btn_txt = nvgRGBA(80, 82, 96, 255);
  }

  // ── Navbar background ────────────────────────────────────────────────────
  nvgBeginPath(vg);
  nvgRect(vg, 0, 0, (float)win_w, CLAY_NAVBAR_H);
  nvgFillColor(vg, bg);
  nvgFill(vg);

  // Bottom separator
  nvgBeginPath(vg);
  nvgRect(vg, 0, CLAY_NAVBAR_H - 1.0f, (float)win_w, 1.0f);
  nvgFillColor(vg, sep);
  nvgFill(vg);

  // ── Document title (left-aligned) ────────────────────────────────────────
  int font_h = ch->fonts[ASLAN_FONT_INTER].handle;
  if (doc_title && doc_title[0]) {
    nvg_draw_text(vg, font_h, 13.0f, doc_title, title_c, 16.0f, 0,
                  CLAY_NAVBAR_H);
  }

  // ── Theme toggle pill (right-aligned) ────────────────────────────────────
  const char *toggle_label = dark_mode ? "Light mode" : "Dark mode";
  float bounds[4];
  nvgFontFaceId(vg, font_h);
  nvgFontSize(vg, 13.0f);
  nvgTextBounds(vg, 0, 0, toggle_label, NULL, bounds);
  float tw = bounds[2] - bounds[0];

  float pill_w = tw + 22.0f;
  float pill_h = 24.0f;
  float pill_x = (float)win_w - pill_w - 12.0f;
  float pill_y = (CLAY_NAVBAR_H - pill_h) * 0.5f;

  bool pill_hov =
      (ch->mouse_x >= (int)pill_x && ch->mouse_x < (int)(pill_x + pill_w) &&
       ch->mouse_y >= (int)pill_y && ch->mouse_y < (int)(pill_y + pill_h));

  nvg_fill_rounded_rect(vg, pill_x, pill_y, pill_w, pill_h, 6.0f,
                        pill_hov ? btn_bg_hov : btn_bg);
  nvg_draw_text(vg, font_h, 13.0f, toggle_label, btn_txt, pill_x + 11.0f,
                pill_y, pill_h);

  if (pill_hov && ch->mouse_pressed_this_frame)
    result.theme_toggled = 1;

  return result;
}

// =============================================================================
// Per-frame split-view background rendering
// =============================================================================

AslanSplitRects clay_host_render_split(ClayHost *ch, bool dark_mode,
                                       float ratio, bool spl_hover,
                                       bool spl_drag, int win_w, int win_h) {
  if (!ch)
    return (AslanSplitRects){0};

  // ── Color palette ───────────────────────────────────────────────────────
  Clay_Color left_bg, right_bg, spl_bg;

  if (dark_mode) {
    left_bg = (Clay_Color){28, 30, 36, 255};
    right_bg = (Clay_Color){24, 26, 32, 255};
    spl_bg = (spl_hover || spl_drag) ? (Clay_Color){55, 57, 68, 255}
                                     : (Clay_Color){42, 44, 52, 255};
  } else {
    left_bg = (Clay_Color){252, 252, 253, 255};
    right_bg = (Clay_Color){255, 255, 255, 255};
    spl_bg = (spl_hover || spl_drag) ? (Clay_Color){218, 218, 225, 255}
                                     : (Clay_Color){235, 235, 238, 255};
  }

  // ── Compute pixel sizes ─────────────────────────────────────────────────
  if (ratio < 0.03f)
    ratio = 0.03f;
  if (ratio > 0.97f)
    ratio = 0.97f;

  const float spl_w = 6.0f;
  float left_w = (float)win_w * ratio;
  float right_w = (float)win_w - left_w - spl_w;
  if (left_w < 30.0f)
    left_w = 30.0f;
  if (right_w < 30.0f)
    right_w = 30.0f;

  // ── Clay layout ─────────────────────────────────────────────────────────
  Clay_SetLayoutDimensions((Clay_Dimensions){(float)win_w, (float)win_h});
  Clay_BeginLayout();

  CLAY(CLAY_ID("AslanRoot"),
       (Clay_ElementDeclaration){
           .layout =
               {
                   .sizing =
                       {
                           .width = CLAY_SIZING_FIXED((float)win_w),
                           .height = CLAY_SIZING_FIXED((float)win_h),
                       },
                   .layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .childGap = 0,
               },
       }) {
    CLAY(CLAY_ID("LeftPanel"),
         (Clay_ElementDeclaration){
             .layout =
                 {
                     .sizing =
                         {
                             .width = CLAY_SIZING_FIXED(left_w),
                             .height = CLAY_SIZING_GROW(),
                         },
                 },
             .backgroundColor = left_bg,
         }) {}

    CLAY(CLAY_ID("Splitter"),
         (Clay_ElementDeclaration){
             .layout =
                 {
                     .sizing =
                         {
                             .width = CLAY_SIZING_FIXED(spl_w),
                             .height = CLAY_SIZING_GROW(),
                         },
                 },
             .backgroundColor = spl_bg,
         }) {}

    CLAY(CLAY_ID("RightPanel"),
         (Clay_ElementDeclaration){
             .layout =
                 {
                     .sizing =
                         {
                             .width = CLAY_SIZING_GROW(),
                             .height = CLAY_SIZING_GROW(),
                         },
                 },
             .backgroundColor = right_bg,
         }) {}
  }

  Clay_RenderCommandArray cmds = Clay_EndLayout();
  clay_nanovg_render(ch->vg, cmds, ch->fonts);

  // ── Splitter handle dots (3 dots centred on splitter) ───────────────────
  if (ch->vg) {
    NVGcolor dot_col =
        dark_mode ? ((spl_hover || spl_drag) ? nvgRGBA(120, 122, 135, 255)
                                             : nvgRGBA(80, 82, 92, 255))
                  : ((spl_hover || spl_drag) ? nvgRGBA(140, 140, 150, 255)
                                             : nvgRGBA(180, 180, 188, 255));

    float dot_x = left_w + spl_w * 0.5f - 1.5f;
    float dot_y = (float)win_h * 0.5f - 10.0f;
    for (int d = 0; d < 3; d++) {
      nvg_fill_rounded_rect(ch->vg, dot_x, dot_y + (float)d * 7.0f, 3.0f, 3.0f,
                            0, dot_col);
    }
  }

  // ── Cache and return rects ───────────────────────────────────────────────
  AslanSplitRects rects = {
      .left_x = 0.0f,
      .left_w = left_w,
      .spl_x = left_w,
      .spl_w = spl_w,
      .right_x = left_w + spl_w,
      .right_w = right_w,
      .panel_h = (float)win_h,
      .tab_h = 0.0f,
  };
  ch->last_rects = rects;
  return rects;
}

// =============================================================================
// Left-panel tab bar (Step 2)
// =============================================================================

#define ASLAN_TAB_BAR_H 38.0f

float clay_host_render_tabs(ClayHost *ch, bool dark_mode, float panel_x,
                            float panel_w, int *active_tab, float mouse_x,
                            float mouse_y, bool mouse_clicked) {
  if (!ch || !active_tab || !ch->vg)
    return 0.0f;
  if (*active_tab < 0)
    *active_tab = 0;

  NVGcontext *vg = ch->vg;

  // ── Colour palette ───────────────────────────────────────────────────────
  NVGcolor tab_bar_bg, active_pill, active_txt, inactive_txt, border_col;
  if (dark_mode) {
    tab_bar_bg = nvgRGBA(28, 30, 36, 255);
    active_pill = nvgRGBA(50, 52, 62, 255);
    active_txt = nvgRGBA(220, 222, 230, 255);
    inactive_txt = nvgRGBA(110, 112, 125, 255);
    border_col = nvgRGBA(48, 50, 60, 255);
  } else {
    tab_bar_bg = nvgRGBA(252, 252, 253, 255);
    active_pill = nvgRGBA(255, 255, 255, 255);
    active_txt = nvgRGBA(30, 30, 40, 255);
    inactive_txt = nvgRGBA(130, 130, 145, 255);
    border_col = nvgRGBA(220, 220, 225, 255);
  }

  // ── Tab bar background ───────────────────────────────────────────────────
  nvgBeginPath(vg);
  nvgRect(vg, panel_x, 0.0f, panel_w, ASLAN_TAB_BAR_H);
  nvgFillColor(vg, tab_bar_bg);
  nvgFill(vg);

  // Bottom separator
  nvgBeginPath(vg);
  nvgRect(vg, panel_x, ASLAN_TAB_BAR_H - 1.0f, panel_w, 1.0f);
  nvgFillColor(vg, border_col);
  nvgFill(vg);

  // ── Tab pills ────────────────────────────────────────────────────────────
  const char *tab_labels[] = {"Table", "Gantt"};
  const int tab_count = 2;
  const float pill_h = 26.0f;
  const float pill_y = (ASLAN_TAB_BAR_H - pill_h) * 0.5f;
  const float pad_x = 14.0f;

  int font_h = ch->fonts[ASLAN_FONT_INTER].handle;

  float tab_x = panel_x + 8.0f;
  for (int i = 0; i < tab_count; i++) {
    float bounds[4];
    nvgFontFaceId(vg, font_h);
    nvgFontSize(vg, 13.0f);
    nvgTextBounds(vg, 0, 0, tab_labels[i], NULL, bounds);
    float tw = bounds[2] - bounds[0];

    float pill_w = tw + pad_x * 2.0f;

    // Hit test
    if (mouse_clicked && mouse_x >= tab_x && mouse_x < tab_x + pill_w &&
        mouse_y >= 0.0f && mouse_y < ASLAN_TAB_BAR_H) {
      *active_tab = i;
    }

    bool is_active = (*active_tab == i);

    // Pill background (active only)
    if (is_active) {
      nvg_fill_rounded_rect(vg, tab_x, pill_y, pill_w, pill_h, 6.0f,
                            active_pill);
    }

    // Tab label via NanoVG
    NVGcolor tc = is_active ? active_txt : inactive_txt;
    nvg_draw_text(vg, font_h, 13.0f, tab_labels[i], tc,
                  tab_x + (pill_w - tw) * 0.5f, pill_y, pill_h);

    tab_x += pill_w + 4.0f;
  }

  return ASLAN_TAB_BAR_H;
}

// =============================================================================
// Floating bottom toolbar (Step 3) — rendered AFTER nk_sdl_render() so it
// sits on top of all Nuklear content.
// =============================================================================

#define CLAY_TB_BTN 36.0f // button size
#define CLAY_TB_GAP 8.0f  // gap between buttons
#define CLAY_TB_PAD 12.0f // horizontal padding inside the pill
#define CLAY_TB_H 52.0f   // pill height
#define CLAY_TB_MB 20.0f  // margin from bottom of window

AslanToolbarResult clay_host_render_toolbar(ClayHost *ch, bool dark_mode,
                                            int win_w, int win_h,
                                            const char **icons_utf8,
                                            int btn_count) {
  AslanToolbarResult result = {-1};
  if (!ch || btn_count <= 0 || !ch->vg)
    return result;

  NVGcontext *vg = ch->vg;

  // ── Mouse state ──────────────────────────────────────────────────────────
  int mx = ch->mouse_x;
  int my = ch->mouse_y;
  bool mouse_pressed = ch->mouse_pressed_this_frame;

  // ── Geometry ─────────────────────────────────────────────────────────────
  float toolbar_w = (float)btn_count * CLAY_TB_BTN +
                    (float)(btn_count - 1) * CLAY_TB_GAP + CLAY_TB_PAD * 2.0f;
  float tx = ((float)win_w - toolbar_w) / 2.0f;
  float ty = (float)win_h - CLAY_TB_H - CLAY_TB_MB;

  // ── Colours ──────────────────────────────────────────────────────────────
  NVGcolor bg, border_c, icon_c, icon_hov, btn_hov_bg;
  if (dark_mode) {
    bg = nvgRGBA(42, 44, 52, 248);
    border_c = nvgRGBA(60, 62, 74, 255);
    icon_c = nvgRGBA(192, 194, 204, 255);
    icon_hov = nvgRGBA(230, 232, 242, 255);
    btn_hov_bg = nvgRGBA(56, 58, 70, 255);
  } else {
    bg = nvgRGBA(255, 255, 255, 248);
    border_c = nvgRGBA(215, 215, 222, 255);
    icon_c = nvgRGBA(70, 72, 84, 255);
    icon_hov = nvgRGBA(25, 27, 38, 255);
    btn_hov_bg = nvgRGBA(240, 240, 246, 255);
  }

  // ── Layered drop shadow ──────────────────────────────────────────────────
  nvg_fill_rounded_rect(vg, tx, ty + 4.0f, toolbar_w, CLAY_TB_H, 26.0f,
                        nvgRGBA(0, 0, 0, 40));

  // ── Border ───────────────────────────────────────────────────────────────
  nvg_fill_rounded_rect(vg, tx - 1, ty - 1, toolbar_w + 2, CLAY_TB_H + 2, 27.0f,
                        border_c);

  // ── Background pill ───────────────────────────────────────────────────────
  nvg_fill_rounded_rect(vg, tx, ty, toolbar_w, CLAY_TB_H, 26.0f, bg);

  // ── Buttons ───────────────────────────────────────────────────────────────
  int icon_font_h = ch->fonts[ASLAN_FONT_ICONS].handle;
  float btn_y = ty + (CLAY_TB_H - CLAY_TB_BTN) / 2.0f;

  for (int i = 0; i < btn_count; i++) {
    float bx = tx + CLAY_TB_PAD + (float)i * (CLAY_TB_BTN + CLAY_TB_GAP);

    bool hovered = ((float)mx >= bx && (float)mx < bx + CLAY_TB_BTN &&
                    (float)my >= btn_y && (float)my < btn_y + CLAY_TB_BTN);

    if (hovered) {
      nvg_fill_rounded_rect(vg, bx, btn_y, CLAY_TB_BTN, CLAY_TB_BTN, 8.0f,
                            btn_hov_bg);
      if (mouse_pressed)
        result.clicked_btn = i;
    }

    // Icon via icon font
    if (icons_utf8 && icons_utf8[i]) {
      NVGcolor ic = hovered ? icon_hov : icon_c;
      nvgFontFaceId(vg, icon_font_h);
      nvgFontSize(vg, 20.0f);
      nvgFillColor(vg, ic);
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(vg, bx + CLAY_TB_BTN * 0.5f, btn_y + CLAY_TB_BTN * 0.5f,
              icons_utf8[i], NULL);
    }
  }

  return result;
}
