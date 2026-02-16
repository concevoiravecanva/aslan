#include "ui_core.h"
#include "../core/engine.h"
#include "../document/document.h"
#include "recents.h" // For home screen access to recents
#include "ui_shared.h"
#include <SDL.h>
#include <stdio.h>
// stdlib.h removed

#include <string.h>

// -----------------------------------------------------------------------------
// Helper: Icon Buttons
// -----------------------------------------------------------------------------

int ui_icon_button(UIHost *ui, const char *icon) {
  struct nk_context *ctx = ui->ctx;
  int ret = 0;
  if (ui->font_icons)
    nk_style_set_font(ctx, &ui->font_icons->handle);
  if (nk_button_label(ctx, icon))
    ret = 1;
  if (ui->font_inter)
    nk_style_set_font(ctx, &ui->font_inter->handle);
  return ret;
}

// -----------------------------------------------------------------------------
// Top Bar - Block Insertion Menu
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Document Toolbar - Floating Bottom Bar
// -----------------------------------------------------------------------------

void ui_render_document_toolbar(UIHost *ui, Engine *engine,
                                struct nk_context *ctx, int win_w, int win_h) {
  if (!ui || !engine || !ctx)
    return;

  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  const float toolbar_w = 4 * btn_size + 3 * btn_spacing + 24.0f;
  const float toolbar_h = 52.0f;

  float toolbar_x = ((float)win_w - toolbar_w) / 2.0f;
  float toolbar_y = (float)win_h - toolbar_h - 20.0f;

  struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
  struct nk_color toolbar_bg =
      ui->dark_mode ? nk_rgb(45, 45, 50) : nk_rgb(255, 255, 255);
  struct nk_color toolbar_border =
      ui->dark_mode ? nk_rgb(70, 70, 80) : nk_rgb(220, 220, 220);

  // Layered shadow for depth
  nk_fill_rect(canvas_buf,
               nk_rect(toolbar_x + 0, toolbar_y + 6, toolbar_w, toolbar_h),
               26.0f, nk_rgba(0, 0, 0, 12));
  nk_fill_rect(canvas_buf,
               nk_rect(toolbar_x + 1, toolbar_y + 3, toolbar_w, toolbar_h),
               26.0f, nk_rgba(0, 0, 0, 18));

  // Background
  nk_fill_rect(canvas_buf, nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h),
               26.0f, toolbar_bg);
  nk_stroke_rect(canvas_buf,
                 nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h), 26.0f,
                 1.0f, toolbar_border);

  float btn_x = toolbar_x + 12.0f;
  float btn_y = toolbar_y + (toolbar_h - btn_size) / 2.0f;

  const char *tool_icons[] = {ICON_TABLE, ICON_CANVAS, ICON_DESIGN, ICON_CHART};
  int tool_count = 4;

  Page *page = doc_page_get_active(engine->doc);
  uint32_t page_id = page ? page->id : 0;

  for (int i = 0; i < tool_count; i++) {
    struct nk_rect btn_rect = nk_rect(btn_x + i * (btn_size + btn_spacing),
                                      btn_y, btn_size, btn_size);

    int hovered = nk_input_is_mouse_hovering_rect(&ctx->input, btn_rect);

    if (hovered) {
      struct nk_color bg =
          ui->dark_mode ? nk_rgb(60, 60, 70) : nk_rgb(240, 240, 240);
      nk_fill_rect(canvas_buf, btn_rect, 6.0f, bg);
    }

    struct nk_color icon_color =
        ui->dark_mode ? nk_rgb(200, 200, 205) : nk_rgb(60, 60, 60);

    if (ui->font_icons) {
      nk_draw_text(canvas_buf,
                   nk_rect(btn_rect.x + 8, btn_rect.y + 8, btn_size - 16,
                           btn_size - 16),
                   tool_icons[i], strlen(tool_icons[i]),
                   &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), icon_color);
    }

    int clicked =
        (hovered && (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) ||
                     ui->canvas_state.mouse_just_pressed));

    if (clicked && page) {
      ASLAN_DEBUG("Toolbar click: button %d", i);
      static const char *block_types[] = {"table", "canvas", "design",
                                          "chart"};
      if (i >= 0 && i < 4) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "block_create %u %u %s", page_id,
                 page->next_block_id, block_types[i]);
        engine_execute_command(engine, cmd);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Bottom Bar - Page Tabs
// -----------------------------------------------------------------------------

void ui_render_bottom_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                          int win_w, int win_h) {
  // Function emptied as per user request to remove "old patterns"
  (void)ui;
  (void)engine;
  (void)ctx;
  (void)win_w;
  (void)win_h;
}

// -----------------------------------------------------------------------------
// Theme Application
// -----------------------------------------------------------------------------

void ui_apply_theme(UIHost *ui) {
  struct nk_context *ctx = ui->ctx;
  struct nk_style *s = &ctx->style;

  if (ui->dark_mode) {
    // Dark theme colors (Coda-style)
    struct nk_color bg = nk_rgb(25, 25, 28);
    struct nk_color text = nk_rgb(230, 230, 235);
    struct nk_color border = nk_rgb(48, 48, 55);
    struct nk_color button = nk_rgb(42, 42, 48);
    struct nk_color button_hover = nk_rgb(55, 55, 62);
    struct nk_color accent = nk_rgb(55, 120, 250);

    s->window.background = bg;
    s->window.fixed_background = nk_style_item_color(bg);
    s->window.border_color = bg;
    s->window.border = 0;
    s->window.spacing = nk_vec2(0, 1);
    s->window.group_border_color = nk_rgba(60, 60, 68, 80);
    s->window.group_border = 0.0f;

    s->button.normal = nk_style_item_color(button);
    s->button.hover = nk_style_item_color(button_hover);
    s->button.active = nk_style_item_color(accent);
    s->button.text_normal = text;
    s->button.text_hover = text;
    s->button.text_active = nk_rgb(255, 255, 255);
    s->button.border_color = border;

    s->text.color = text;

    s->edit.normal = nk_style_item_color(bg);
    s->edit.hover = nk_style_item_color(bg);
    s->edit.active = nk_style_item_color(bg);
    s->edit.text_normal = text;
    s->edit.text_hover = text;
    s->edit.text_active = text;
    s->edit.border_color = nk_rgba(0, 0, 0, 0);
    s->edit.cursor_normal = text;
    s->edit.cursor_hover = text;
    s->edit.cursor_text_normal = bg;
    s->edit.cursor_text_hover = bg;
    s->edit.cursor_text_hover = bg;
    s->edit.border = 0.0f;
    s->edit.padding = nk_vec2(0, 2); // Flush alignment with margin
  } else {
    // Light theme colors (Coda-style: pure white, clean)
    struct nk_color bg = nk_rgb(255, 255, 255);
    struct nk_color text = nk_rgb(36, 36, 39);
    struct nk_color border = nk_rgb(228, 228, 231);
    struct nk_color button = nk_rgb(246, 246, 249);
    struct nk_color button_hover = nk_rgb(238, 238, 242);
    struct nk_color accent = nk_rgb(55, 120, 250);

    s->window.background = bg;
    s->window.fixed_background = nk_style_item_color(bg);
    s->window.border_color = bg;
    s->window.border = 0;
    s->window.spacing = nk_vec2(0, 1);
    s->window.group_border_color = nk_rgba(230, 230, 233, 100);
    s->window.group_border = 0.0f;

    s->button.normal = nk_style_item_color(button);
    s->button.hover = nk_style_item_color(button_hover);
    s->button.active = nk_style_item_color(accent);
    s->button.text_normal = text;
    s->button.text_hover = text;
    s->button.text_active = nk_rgb(255, 255, 255);
    s->button.border_color = border;

    s->text.color = text;

    s->edit.normal = nk_style_item_color(bg);
    s->edit.hover = nk_style_item_color(bg);
    s->edit.active = nk_style_item_color(bg);
    s->edit.text_normal = text;
    s->edit.text_hover = text;
    s->edit.text_active = text;
    s->edit.border_color = nk_rgba(0, 0, 0, 0);
    s->edit.cursor_normal = text;
    s->edit.cursor_hover = text;
    s->edit.cursor_text_normal = bg;
    s->edit.cursor_text_hover = bg;
    s->edit.cursor_text_hover = bg;
    s->edit.border = 0.0f;
    s->edit.padding = nk_vec2(0, 2); // Flush alignment with margin
  }
}

// -----------------------------------------------------------------------------
// UI Cursors implementation
// -----------------------------------------------------------------------------

static SDL_Cursor *sdl_cursors[UI_CURSOR_COUNT] = {NULL};
static UICursorType last_committed_cursor = UI_CURSOR_COUNT;

void ui_set_cursor(UIHost *ui, UICursorType type) {
  if (ui && type >= 0 && type < UI_CURSOR_COUNT) {
    ui->cursor_type = type;
  }
}

void ui_commit_cursor(UIHost *ui) {
  if (!ui)
    return;
  UICursorType type = ui->cursor_type;
  if (type == last_committed_cursor)
    return;

  if (!sdl_cursors[type]) {
    SDL_SystemCursor sdl_id = SDL_SYSTEM_CURSOR_ARROW;
    switch (type) {
    case UI_CURSOR_ARROW:
      sdl_id = SDL_SYSTEM_CURSOR_ARROW;
      break;
    case UI_CURSOR_IBEAM:
      sdl_id = SDL_SYSTEM_CURSOR_IBEAM;
      break;
    case UI_CURSOR_WAIT:
      sdl_id = SDL_SYSTEM_CURSOR_WAIT;
      break;
    case UI_CURSOR_CROSSHAIR:
      sdl_id = SDL_SYSTEM_CURSOR_CROSSHAIR;
      break;
    case UI_CURSOR_WAITARROW:
      sdl_id = SDL_SYSTEM_CURSOR_WAITARROW;
      break;
    case UI_CURSOR_SIZENWSE:
      sdl_id = SDL_SYSTEM_CURSOR_SIZENWSE;
      break;
    case UI_CURSOR_SIZENESW:
      sdl_id = SDL_SYSTEM_CURSOR_SIZENESW;
      break;
    case UI_CURSOR_SIZEWE:
      sdl_id = SDL_SYSTEM_CURSOR_SIZEWE;
      break;
    case UI_CURSOR_SIZENS:
      sdl_id = SDL_SYSTEM_CURSOR_SIZENS;
      break;
    case UI_CURSOR_SIZEALL:
      sdl_id = SDL_SYSTEM_CURSOR_SIZEALL;
      break;
    case UI_CURSOR_NO:
      sdl_id = SDL_SYSTEM_CURSOR_NO;
      break;
    case UI_CURSOR_HAND:
      sdl_id = SDL_SYSTEM_CURSOR_HAND;
      break;
    default:
      break;
    }
    sdl_cursors[type] = SDL_CreateSystemCursor(sdl_id);
  }

  if (sdl_cursors[type]) {
    SDL_SetCursor(sdl_cursors[type]);
    last_committed_cursor = type;
  }
}
