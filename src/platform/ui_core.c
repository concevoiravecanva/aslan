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
  (void)ui; (void)icon;
  return 0;
}

// -----------------------------------------------------------------------------
// Top Bar - Block Insertion Menu
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Document Toolbar - now rendered by clay_host_render_toolbar() in clay_host.c
// (after nk_sdl_render, so it floats on top of all NK content)
// -----------------------------------------------------------------------------

void ui_render_document_toolbar(UIHost *ui, Engine *engine,
                                int win_w, int win_h) {
  // Replaced by Clay/SDL2 toolbar in clay_host.c — this stub is kept for ABI
  // compatibility only.
  (void)ui; (void)engine; (void)win_w; (void)win_h;
}

// -----------------------------------------------------------------------------
// Bottom Bar - Page Tabs
// -----------------------------------------------------------------------------

void ui_render_bottom_bar(UIHost *ui, Engine *engine,
                          int win_w, int win_h) {
  // Function emptied as per user request to remove "old patterns"
  (void)ui;
  (void)engine;
  (void)win_w;
  (void)win_h;
}

// -----------------------------------------------------------------------------
// Theme Application
// -----------------------------------------------------------------------------

void ui_apply_theme(UIHost *ui) {
  // Nuklear themes no longer required.
  (void)ui;
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
