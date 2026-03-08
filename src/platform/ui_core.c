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
// Document Toolbar - now rendered by clay_host_render_toolbar() in clay_host.c
// (after nk_sdl_render, so it floats on top of all NK content)
// -----------------------------------------------------------------------------

void ui_render_document_toolbar(UIHost *ui, Engine *engine,
                                struct nk_context *ctx, int win_w, int win_h) {
  // Replaced by Clay/SDL2 toolbar in clay_host.c — this stub is kept for ABI
  // compatibility only.
  (void)ui; (void)engine; (void)ctx; (void)win_w; (void)win_h;
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
    struct nk_color accent = COLOR_ACCENT;

    s->window.background = bg;
    s->window.fixed_background = nk_style_item_color(bg);
    s->window.border_color = border;
    s->window.border = 1.0f;
    s->window.rounding = 8.0f;
    s->window.spacing = nk_vec2(4, 0);
    s->window.padding = nk_vec2(8, 0);
    s->window.group_padding = nk_vec2(0, 0);
    s->window.group_border_color = nk_rgba(60, 60, 68, 80);
    s->window.group_border = 0.0f;
    s->window.popup_border = 1.0f;
    s->window.popup_border_color = border;
    s->window.popup_padding = nk_vec2(4, 2);

    s->button.normal = nk_style_item_color(button);
    s->button.hover = nk_style_item_color(button_hover);
    s->button.active = nk_style_item_color(button_hover); // No solid blue
    s->button.text_normal = text;
    s->button.text_hover = text;
    s->button.text_active = accent; // Use blue ONLY for text
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
    struct nk_color accent = COLOR_ACCENT;

    s->window.background = bg;
    s->window.fixed_background = nk_style_item_color(bg);
    s->window.border_color = border;
    s->window.border = 1.0f;
    s->window.rounding = 8.0f;
    s->window.spacing = nk_vec2(4, 0);
    s->window.padding = nk_vec2(8, 0);
    s->window.group_padding = nk_vec2(0, 0);
    s->window.group_border_color = nk_rgba(230, 230, 233, 100);
    s->window.group_border = 0.0f;
    s->window.popup_border = 1.0f;
    s->window.popup_border_color = border;
    s->window.popup_padding = nk_vec2(4, 2);

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

    // Neutral scrollbars
    s->scrollv.normal = nk_style_item_color(nk_rgb(245, 245, 248));
    s->scrollv.hover = nk_style_item_color(nk_rgb(240, 240, 245));
    s->scrollv.active = nk_style_item_color(nk_rgb(235, 235, 240));
    s->scrollv.cursor_normal = nk_style_item_color(nk_rgb(200, 200, 210));
    s->scrollv.cursor_hover = nk_style_item_color(nk_rgb(180, 180, 195));
    s->scrollv.cursor_active = nk_style_item_color(nk_rgb(160, 160, 180));
    s->scrollv.border = 0;
    s->scrollv.rounding = 4.0f;
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
