// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear implementation (must be before nuklear.h include)
#define NK_IMPLEMENTATION
#include "ui_host.h"
#include "../scripting/lua_host.h"
#include <SDL.h>
#include <math.h>
#include <stdlib.h>

// Nuklear SDL renderer backend (external)
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "nuklear_sdl_renderer.h"

// -----------------------------------------------------------------------------
// Colors
// -----------------------------------------------------------------------------

static struct nk_color COLOR_BG = {30, 32, 36, 255};
static struct nk_color COLOR_PANEL = {40, 42, 46, 255};
static struct nk_color COLOR_BORDER = {60, 62, 66, 255};
static struct nk_color COLOR_TEXT = {220, 220, 220, 255};
static struct nk_color COLOR_TEXT_DIM = {140, 140, 140, 255};
static struct nk_color COLOR_ACCENT = {80, 140, 200, 255};
static struct nk_color COLOR_HIGHLIGHT_YELLOW = {255, 240, 100, 80};
static struct nk_color COLOR_HIGHLIGHT_BLUE = {100, 180, 255, 80};
static struct nk_color COLOR_HIGHLIGHT_GREEN = {100, 220, 140, 80};

// -----------------------------------------------------------------------------
// UI Initialization
// -----------------------------------------------------------------------------

UIHost *ui_init(SDL_Window *win, SDL_Renderer *renderer, lua_State *L) {
  UIHost *ui = (UIHost *)calloc(1, sizeof(UIHost));
  if (!ui)
    return NULL;

  ui->L = L;
  ui->screen = 0; // Start in Dashboard
  ui->font_size_px = 16.0f;

  // Load recents
  recents_load(&ui->recents, "recents.log");

  // Initialize Nuklear with SDL renderer
  ui->ctx = nk_sdl_init(win, renderer);
  if (!ui->ctx) {
    free(ui);
    return NULL;
  }

  FILE *log = fopen("ui_debug.log", "w");
  if (log) {
    fprintf(log, "UI Init started\n");
    fflush(log);
  }

  // Load fonts
  nk_sdl_font_stash_begin(&ui->atlas);

  // Font paths
  const char *inter_path = "assets/fonts/Inter-Regular.ttf";
  const char *mono_path =
      "assets/fonts/JetBrains_Mono/JetBrainsMono-VariableFont_wght.ttf";
  const char *icons_path = "assets/remixicon.ttf";

  // Load Inter font for UI
  ui->font_inter =
      nk_font_atlas_add_from_file(ui->atlas, inter_path, 16.0f, NULL);
  if (!ui->font_inter) {
    ui->font_inter = nk_font_atlas_add_default(ui->atlas, 16.0f, NULL);
  }

  // Load JetBrains Mono for text blocks
  ui->font_mono =
      nk_font_atlas_add_from_file(ui->atlas, mono_path, 15.0f, NULL);
  if (!ui->font_mono) {
    ui->font_mono = nk_font_atlas_add_default(ui->atlas, 15.0f, NULL);
  }

  // Load heading fonts (Inter at different sizes)
  ui->font_h1 = nk_font_atlas_add_from_file(ui->atlas, inter_path, 28.0f, NULL);
  if (!ui->font_h1)
    ui->font_h1 = nk_font_atlas_add_default(ui->atlas, 28.0f, NULL);

  ui->font_h2 = nk_font_atlas_add_from_file(ui->atlas, inter_path, 22.0f, NULL);
  if (!ui->font_h2)
    ui->font_h2 = nk_font_atlas_add_default(ui->atlas, 22.0f, NULL);

  ui->font_h3 = nk_font_atlas_add_from_file(ui->atlas, inter_path, 18.0f, NULL);
  if (!ui->font_h3)
    ui->font_h3 = nk_font_atlas_add_default(ui->atlas, 18.0f, NULL);

  // Load RemixIcon font for icons (Unicode range 0xE000-0xF8FF)
  static const nk_rune icons_range[] = {0xE000, 0xF8FF, 0};
  struct nk_font_config icons_cfg = nk_font_config(20.0f);
  icons_cfg.range = icons_range;
  icons_cfg.merge_mode = nk_false;
  ui->font_icons =
      nk_font_atlas_add_from_file(ui->atlas, icons_path, 20.0f, &icons_cfg);
  if (log) {
    fprintf(log, "Font Inter: %p\n", (void *)ui->font_inter);
    fprintf(log, "Font Mono: %p\n", (void *)ui->font_mono);
    fprintf(log, "Font Icons: %p (path: %s)\n", (void *)ui->font_icons,
            icons_path);
    if (!ui->font_icons) {
      fprintf(log, "ERROR: Failed to load icons!\n");
    }
    fclose(log);
  }

  nk_sdl_font_stash_end();

  // Set default font
  if (ui->font_inter) {
    nk_style_set_font(ui->ctx, &ui->font_inter->handle);
  }

  // Style customization
  struct nk_context *ctx = ui->ctx;
  ctx->style.window.background = COLOR_BG;
  ctx->style.window.fixed_background = nk_style_item_color(COLOR_BG);
  ctx->style.window.border_color = COLOR_BORDER;
  ctx->style.window.border = 1.0f;
  ctx->style.window.padding = nk_vec2(8, 8);

  ctx->style.button.normal = nk_style_item_color(COLOR_PANEL);
  ctx->style.button.hover = nk_style_item_color(COLOR_ACCENT);
  ctx->style.button.active = nk_style_item_color(COLOR_ACCENT);
  ctx->style.button.border_color = COLOR_BORDER;
  ctx->style.button.text_normal = COLOR_TEXT;
  ctx->style.button.text_hover = COLOR_TEXT;
  ctx->style.button.text_active = COLOR_TEXT;

  ctx->style.edit.normal = nk_style_item_color(COLOR_PANEL);
  ctx->style.edit.hover = nk_style_item_color(COLOR_PANEL);
  ctx->style.edit.active = nk_style_item_color(COLOR_PANEL);
  ctx->style.edit.border_color = COLOR_BORDER;
  ctx->style.edit.text_normal = COLOR_TEXT;

  return ui;
}

void ui_shutdown(UIHost *ui) {
  if (!ui)
    return;
  nk_sdl_shutdown();
  free(ui);
}

// -----------------------------------------------------------------------------
// Event Handling
// -----------------------------------------------------------------------------

void ui_handle_event(UIHost *ui, SDL_Event *evt) {
  if (!ui || !evt)
    return;

  // Pass to Nuklear first
  nk_sdl_handle_event(evt);

  // Track mouse position
  if (evt->type == SDL_MOUSEMOTION) {
    ui->mouse_x = (float)evt->motion.x;
    ui->mouse_y = (float)evt->motion.y;

    // Handle canvas panning
    if (ui->canvas_state.is_panning) {
      float dx = ui->mouse_x - ui->canvas_state.pan_start_x;
      float dy = ui->mouse_y - ui->canvas_state.pan_start_y;

      // Move camera (inverse direction of mouse movement)
      ui->canvas_state.cam_x = ui->canvas_state.cam_start_x - dx / ui->canvas_state.zoom;
      ui->canvas_state.cam_y = ui->canvas_state.cam_start_y - dy / ui->canvas_state.zoom;
    }
  }

  // Track mouse buttons
  if (evt->type == SDL_MOUSEBUTTONDOWN) {
    if (evt->button.button == SDL_BUTTON_LEFT)
      ui->mouse_buttons |= 1;
    if (evt->button.button == SDL_BUTTON_MIDDLE)
      ui->mouse_buttons |= 2;
    if (evt->button.button == SDL_BUTTON_RIGHT)
      ui->mouse_buttons |= 4;

    // Start panning with middle mouse button
    if (evt->button.button == SDL_BUTTON_MIDDLE) {
      ui->canvas_state.is_panning = 1;
      ui->canvas_state.pan_start_x = ui->mouse_x;
      ui->canvas_state.pan_start_y = ui->mouse_y;
      ui->canvas_state.cam_start_x = ui->canvas_state.cam_x;
      ui->canvas_state.cam_start_y = ui->canvas_state.cam_y;
    }

    // Start panning with space + left click
    if (evt->button.button == SDL_BUTTON_LEFT && ui->canvas_state.space_held) {
      ui->canvas_state.is_panning = 1;
      ui->canvas_state.pan_start_x = ui->mouse_x;
      ui->canvas_state.pan_start_y = ui->mouse_y;
      ui->canvas_state.cam_start_x = ui->canvas_state.cam_x;
      ui->canvas_state.cam_start_y = ui->canvas_state.cam_y;
    }
  }

  if (evt->type == SDL_MOUSEBUTTONUP) {
    if (evt->button.button == SDL_BUTTON_LEFT) {
      ui->mouse_buttons &= ~1;
      if (ui->canvas_state.space_held) {
        ui->canvas_state.is_panning = 0;
      }
    }
    if (evt->button.button == SDL_BUTTON_MIDDLE) {
      ui->mouse_buttons &= ~2;
      ui->canvas_state.is_panning = 0;
    }
    if (evt->button.button == SDL_BUTTON_RIGHT)
      ui->mouse_buttons &= ~4;
  }

  // Track spacebar for pan mode
  if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_SPACE) {
    ui->canvas_state.space_held = 1;
  }
  if (evt->type == SDL_KEYUP && evt->key.keysym.sym == SDLK_SPACE) {
    ui->canvas_state.space_held = 0;
    ui->canvas_state.is_panning = 0;
  }

  // Mouse wheel zoom (toward cursor position)
  if (evt->type == SDL_MOUSEWHEEL) {
    float old_zoom = ui->canvas_state.zoom;
    float zoom_factor = 1.15f;

    if (evt->wheel.y > 0) {
      // Zoom in
      ui->canvas_state.zoom *= zoom_factor;
    } else if (evt->wheel.y < 0) {
      // Zoom out
      ui->canvas_state.zoom /= zoom_factor;
    }

    // Clamp zoom
    if (ui->canvas_state.zoom < 0.1f)
      ui->canvas_state.zoom = 0.1f;
    if (ui->canvas_state.zoom > 5.0f)
      ui->canvas_state.zoom = 5.0f;

    // Zoom toward mouse position
    // World position under mouse before zoom
    // Note: This requires knowing the canvas bounds which we don't have here
    // We'll use a simpler approach that zooms toward screen center
    // For proper zoom-to-cursor, we'd need to track canvas bounds

    // Approximate: adjust camera to zoom toward mouse
    // This is a simplified version - for precise zoom-to-cursor we'd need canvas bounds
    float new_zoom = ui->canvas_state.zoom;
    float zoom_ratio = new_zoom / old_zoom;

    // Shift camera to maintain relative mouse position (approximate)
    // Assuming mouse is somewhere in the middle area
    // For better accuracy, we'd pass canvas bounds to this function
  }
}

// -----------------------------------------------------------------------------
// Top Bar - Block Insertion Menu
// -----------------------------------------------------------------------------

// Helper for icon buttons
static int ui_icon_button(UIHost *ui, const char *icon) {
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

void ui_render_top_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                       int win_w) {
  if (!ui || !engine || !ctx)
    return;

  const float bar_h = 40.0f;
  const float btn_w = 36.0f;
  const float btn_h = 28.0f;

  if (nk_begin(ctx, "TopBar", nk_rect(0, 0, (float)win_w, bar_h),
               NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {

    nk_layout_row_static(ctx, btn_h, (int)btn_w, 14);

    Page *page = doc_page_get_active(engine->doc);
    uint32_t page_id = page ? page->id : 0;

    // Home button (icon)
    if (ui_icon_button(ui, ICON_HOME)) {
      ui->screen = 0;
    }

    // Spacer
    nk_spacing(ctx, 1);

    // Text block (icon)
    if (ui_icon_button(ui, ICON_TEXT)) {
      if (page) {
        Block *b = block_create(page, BLOCK_TEXT);
        if (b) {
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u text", page_id, b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // H1 block (keep text)
    if (nk_button_label(ctx, "H1")) {
      if (page) {
        Block *b = block_create(page, BLOCK_HEADING_1);
        if (b) {
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u h1", page_id, b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Separator (icon)
    if (ui_icon_button(ui, ICON_SEPARATOR)) {
      if (page) {
        Block *b = block_create(page, BLOCK_SEPARATOR);
        if (b) {
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u separator", page_id,
                   b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Lua script (icon)
    if (ui_icon_button(ui, ICON_LUA)) {
      if (page) {
        Block *b = block_create(page, BLOCK_LUA);
        if (b) {
          block_lua_init(b);
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u lua", page_id, b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Table (icon)
    if (ui_icon_button(ui, ICON_TABLE)) {
      if (page) {
        Block *b = block_create(page, BLOCK_TABLE);
        if (b) {
          block_table_init(b, 3, 3);
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u table", page_id,
                   b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Canvas (icon)
    if (ui_icon_button(ui, ICON_CANVAS)) {
      if (page) {
        Block *b = block_create(page, BLOCK_CANVAS);
        if (b) {
          block_canvas_init(b);
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u canvas", page_id,
                   b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Design (icon)
    if (ui_icon_button(ui, ICON_DESIGN)) {
      if (page) {
        Block *b = block_create(page, BLOCK_DESIGN);
        if (b) {
          block_design_init(b, 800, 600);
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u design", page_id,
                   b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Video (icon)
    if (ui_icon_button(ui, ICON_VIDEO)) {
      if (page) {
        Block *b = block_create(page, BLOCK_VIDEO);
        if (b) {
          block_video_init(b);
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u video", page_id,
                   b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }
  }
  nk_end(ctx);
}

// -----------------------------------------------------------------------------
// Bottom Bar - Page Tabs
// -----------------------------------------------------------------------------

void ui_render_bottom_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                          int win_w, int win_h) {
  if (!ui || !engine || !ctx)
    return;

  const float bar_h = 36.0f;
  const float tab_w = 100.0f;
  const float btn_w = 30.0f;

  if (nk_begin(ctx, "BottomBar",
               nk_rect(0, (float)win_h - bar_h, (float)win_w, bar_h),
               NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {

    int page_count = engine->doc->page_count;
    int col_count = page_count + 1; // +1 for [+] button

    nk_layout_row_static(ctx, bar_h - 8, (int)tab_w, col_count);

    // Render page tabs
    for (int i = 0; i < 32; i++) {
      if (!engine->doc->pages[i].in_use)
        continue;

      Page *p = &engine->doc->pages[i];
      int is_active = (engine->doc->active_page_idx == i);

      struct nk_style_button style = ctx->style.button;
      if (is_active) {
        style.normal = nk_style_item_color(COLOR_ACCENT);
        style.hover = nk_style_item_color(COLOR_ACCENT);
      }

      if (nk_button_label_styled(ctx, &style, p->name)) {
        doc_page_activate(engine->doc, p->id);
        ui->screen = 1; // Switch to editor
      }
    }

    // Add page button
    if (nk_button_label(ctx, "+")) {
      char name[64];
      snprintf(name, sizeof(name), "New Note %d", engine->doc->page_count + 1);
      Page *new_page = doc_page_create(engine->doc, name);
      if (new_page) {
        doc_page_activate(engine->doc, new_page->id);
        ui->screen = 1;

        // Add default H1 block
        Block *b = block_create(new_page, BLOCK_HEADING_1);
        if (b) {
          block_set_text(b, name);
          page_append_block(new_page, b);
        }

        char cmd[128];
        snprintf(cmd, sizeof(cmd), "page_create %u \"%s\"", new_page->id, name);
        engine_execute_command(engine, cmd);
        if (b) {
          snprintf(cmd, sizeof(cmd), "block_create %u %u h1", new_page->id,
                   b->id);
          engine_execute_command(engine, cmd);
          snprintf(cmd, sizeof(cmd), "block_text %u %u \"%s\"", new_page->id,
                   b->id, name);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Home button
    if (ui_icon_button(ui, ICON_HOME)) {
      ui->screen = 0;
    }
  }
  nk_end(ctx);
}

// -----------------------------------------------------------------------------
// Block Renderers
// -----------------------------------------------------------------------------

static void ui_render_expand_toggle(UIHost *ui, struct nk_context *ctx,
                                    Block *block) {
  int is_expanded = (ui->expanded_block_id == block->id);

  // Right-aligned button for both states
  nk_layout_row_template_begin(ctx, 28.0f);
  nk_layout_row_template_push_dynamic(ctx);       // Spacer
  nk_layout_row_template_push_static(ctx, 30.0f); // Button
  nk_layout_row_template_end(ctx);

  // First slot is empty (spacer)
  nk_spacing(ctx, 1);

  if (is_expanded) {
    if (ui_icon_button(ui, ICON_FULLSCREEN_EXIT)) {
      ui->expanded_block_id = 0;
    }
  } else {
    if (ui_icon_button(ui, ICON_FULLSCREEN)) {
      ui->expanded_block_id = block->id;
    }
  }
}

void ui_render_block_text(UIHost *ui, struct nk_context *ctx, Block *block,
                          float width) {
  if (!block)
    return;

  if (ui->font_mono) {
    nk_style_set_font(ctx, &ui->font_mono->handle);
  }

  const float row_h = 24.0f;
  nk_layout_row_dynamic(ctx, row_h, 1);

  // Highlight background
  if (block->highlight_color > 0) {
    struct nk_rect bounds = nk_widget_bounds(ctx);
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    struct nk_color hl = block->highlight_color == 1   ? COLOR_HIGHLIGHT_YELLOW
                         : block->highlight_color == 2 ? COLOR_HIGHLIGHT_BLUE
                                                       : COLOR_HIGHLIGHT_GREEN;
    nk_fill_rect(canvas, bounds, 0, hl);
  }

  char buf[4096];
  strncpy(buf, block->text ? block->text : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  nk_flags flags = NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT;
  nk_edit_string_zero_terminated(ctx, flags, buf, (int)sizeof(buf) - 1,
                                 nk_filter_default);

  // Update if changed
  if (strcmp(buf, block->text ? block->text : "") != 0) {
    block_set_text(block, buf);
  }

  if (ui->font_inter) {
    nk_style_set_font(ctx, &ui->font_inter->handle);
  }
}

void ui_render_block_heading(UIHost *ui, struct nk_context *ctx, Block *block,
                             float width, int level) {
  if (!block)
    return;

  struct nk_font *font = level == 1   ? ui->font_h1
                         : level == 2 ? ui->font_h2
                                      : ui->font_h3;

  float row_h = level == 1 ? 40.0f : level == 2 ? 34.0f : 28.0f;

  if (font) {
    nk_style_set_font(ctx, &font->handle);
  }

  nk_layout_row_dynamic(ctx, row_h, 1);

  char buf[1024];
  strncpy(buf, block->text ? block->text : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  nk_flags flags = NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT;
  nk_edit_string_zero_terminated(ctx, flags, buf, (int)sizeof(buf) - 1,
                                 nk_filter_default);

  if (strcmp(buf, block->text ? block->text : "") != 0) {
    block_set_text(block, buf);
  }

  if (ui->font_inter) {
    nk_style_set_font(ctx, &ui->font_inter->handle);
  }
}

void ui_render_block_separator(UIHost *ui, struct nk_context *ctx, Block *block,
                               float width) {
  nk_layout_row_dynamic(ctx, 20.0f, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  float y = bounds.y + bounds.h / 2.0f;
  nk_stroke_line(canvas, bounds.x + 20, y, bounds.x + bounds.w - 20, y, 1.0f,
                 COLOR_BORDER);

  nk_spacing(ctx, 1);
}

void ui_render_block_table(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width) {
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  if (td->rows <= 0 || td->cols <= 0)
    return;

  const float row_h = 26.0f;
  const float col_w = 120.0f;

  ui_render_expand_toggle(ui, ctx, block);

  for (int r = 0; r < td->rows; r++) {
    nk_layout_row_static(ctx, row_h, (int)col_w, td->cols);

    for (int c = 0; c < td->cols; c++) {
      char buf[256];
      const char *cell = block_table_get_cell(block, r, c);
      strncpy(buf, cell ? cell : "", sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';

      nk_flags flags = NK_EDIT_FIELD;
      nk_edit_string_zero_terminated(ctx, flags, buf, (int)sizeof(buf) - 1,
                                     nk_filter_default);

      if (strcmp(buf, cell ? cell : "") != 0) {
        block_table_set_cell(block, r, c, buf);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Canvas Helper: Draw infinite dot grid
// -----------------------------------------------------------------------------

static void canvas_draw_dot_grid(struct nk_command_buffer *canvas,
                                 struct nk_rect bounds, float cam_x, float cam_y,
                                 float zoom) {
  // Grid spacing in world units
  const float grid_spacing = 40.0f;
  // Dot appearance
  const float dot_radius = 1.5f;
  struct nk_color dot_color = {80, 82, 86, 255}; // Subtle gray

  // Calculate visible world bounds
  float world_left = cam_x;
  float world_top = cam_y;
  float world_right = cam_x + bounds.w / zoom;
  float world_bottom = cam_y + bounds.h / zoom;

  // Snap to grid
  float start_x = floorf(world_left / grid_spacing) * grid_spacing;
  float start_y = floorf(world_top / grid_spacing) * grid_spacing;

  // Limit iterations for performance
  int max_dots_x = (int)((bounds.w / (grid_spacing * zoom)) + 2);
  int max_dots_y = (int)((bounds.h / (grid_spacing * zoom)) + 2);
  if (max_dots_x > 100)
    max_dots_x = 100;
  if (max_dots_y > 100)
    max_dots_y = 100;

  for (int iy = 0; iy < max_dots_y; iy++) {
    float world_y = start_y + iy * grid_spacing;
    if (world_y > world_bottom)
      break;

    float screen_y = bounds.y + (world_y - cam_y) * zoom;

    for (int ix = 0; ix < max_dots_x; ix++) {
      float world_x = start_x + ix * grid_spacing;
      if (world_x > world_right)
        break;

      float screen_x = bounds.x + (world_x - cam_x) * zoom;

      // Draw dot as small filled circle
      nk_fill_circle(canvas, nk_rect(screen_x - dot_radius, screen_y - dot_radius,
                                      dot_radius * 2, dot_radius * 2),
                     dot_color);
    }
  }
}

// -----------------------------------------------------------------------------
// Canvas Helper: Render floating toolbar (Apple Freeform style)
// -----------------------------------------------------------------------------

static void canvas_render_toolbar(UIHost *ui, struct nk_context *ctx,
                                  struct nk_rect bounds, Block *block) {
  // Floating toolbar at bottom center of canvas
  const float toolbar_w = 200.0f;
  const float toolbar_h = 40.0f;
  const float margin = 16.0f;

  float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
  float toolbar_y = bounds.y + bounds.h - toolbar_h - margin;

  // Toolbar background (pure white with subtle shadow)
  struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
  struct nk_color toolbar_bg = {255, 255, 255, 245};
  struct nk_color toolbar_border = {220, 220, 220, 255};

  // Shadow
  struct nk_color shadow = {0, 0, 0, 30};
  nk_fill_rect(canvas_buf, nk_rect(toolbar_x + 2, toolbar_y + 2, toolbar_w, toolbar_h),
               12.0f, shadow);

  // Background
  nk_fill_rect(canvas_buf, nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h),
               12.0f, toolbar_bg);
  nk_stroke_rect(canvas_buf, nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h),
                 12.0f, 1.0f, toolbar_border);

  // Tool buttons (5 tools)
  const float btn_size = 32.0f;
  const float btn_spacing = 4.0f;
  const float total_btn_w = 5 * btn_size + 4 * btn_spacing;
  float btn_x = toolbar_x + (toolbar_w - total_btn_w) / 2.0f;
  float btn_y = toolbar_y + (toolbar_h - btn_size) / 2.0f;

  const char *tool_icons[] = {ICON_CURSOR, ICON_SHAPE, ICON_PENCIL, ICON_TEXT_TOOL,
                              ICON_STICKY};
  CanvasToolMode tools[] = {CANVAS_TOOL_SELECT, CANVAS_TOOL_RECT, CANVAS_TOOL_PEN,
                            CANVAS_TOOL_TEXT, CANVAS_TOOL_STICKY};

  // Save button style
  struct nk_style_button saved_style = ctx->style.button;

  // Minimal button style for toolbar
  ctx->style.button.normal = nk_style_item_color(nk_rgba(255, 255, 255, 0));
  ctx->style.button.hover = nk_style_item_color(nk_rgba(240, 240, 240, 255));
  ctx->style.button.active = nk_style_item_color(nk_rgba(230, 230, 230, 255));
  ctx->style.button.border_color = nk_rgba(0, 0, 0, 0);
  ctx->style.button.text_normal = nk_rgb(60, 60, 60);
  ctx->style.button.text_hover = nk_rgb(40, 40, 40);
  ctx->style.button.text_active = nk_rgb(20, 20, 20);
  ctx->style.button.rounding = 8.0f;

  for (int i = 0; i < 5; i++) {
    struct nk_rect btn_bounds = nk_rect(btn_x + i * (btn_size + btn_spacing), btn_y,
                                         btn_size, btn_size);

    // Highlight active tool
    if (ui->canvas_state.tool == tools[i]) {
      ctx->style.button.normal = nk_style_item_color(nk_rgba(66, 133, 244, 40));
      ctx->style.button.text_normal = nk_rgb(66, 133, 244);
    } else {
      ctx->style.button.normal = nk_style_item_color(nk_rgba(255, 255, 255, 0));
      ctx->style.button.text_normal = nk_rgb(60, 60, 60);
    }

    // Check if mouse is over this button
    float mx = ui->mouse_x;
    float my = ui->mouse_y;
    int hovered = (mx >= btn_bounds.x && mx < btn_bounds.x + btn_bounds.w &&
                   my >= btn_bounds.y && my < btn_bounds.y + btn_bounds.h);

    // Draw button manually since we're inside custom rendering
    if (hovered) {
      nk_fill_rect(canvas_buf, btn_bounds, 8.0f, nk_rgba(240, 240, 240, 255));
    } else if (ui->canvas_state.tool == tools[i]) {
      nk_fill_rect(canvas_buf, btn_bounds, 8.0f, nk_rgba(66, 133, 244, 40));
    }

    // Draw icon
    if (ui->font_icons) {
      struct nk_color icon_color =
          (ui->canvas_state.tool == tools[i]) ? nk_rgb(66, 133, 244) : nk_rgb(60, 60, 60);
      nk_draw_text(canvas_buf,
                   nk_rect(btn_bounds.x + 6, btn_bounds.y + 6, btn_size - 12, btn_size - 12),
                   tool_icons[i], strlen(tool_icons[i]), &ui->font_icons->handle,
                   nk_rgba(0, 0, 0, 0), icon_color);
    }

    // Handle click
    if (hovered && (ui->mouse_buttons & 1)) {
      ui->canvas_state.tool = tools[i];
    }
  }

  // Restore button style
  ctx->style.button = saved_style;
}

// -----------------------------------------------------------------------------
// Canvas: Main render function
// -----------------------------------------------------------------------------

void ui_render_block_canvas(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
  if (!block || block->type != BLOCK_CANVAS)
    return;

  CanvasData *cd = &block->data.canvas;

  // Sync UI state with canvas data on first render or when switching canvas
  if (ui->canvas_state.active_canvas_id != block->id) {
    ui->canvas_state.active_canvas_id = block->id;
    ui->canvas_state.cam_x = cd->view_x;
    ui->canvas_state.cam_y = cd->view_y;
    ui->canvas_state.zoom = cd->zoom > 0.01f ? cd->zoom : 1.0f;
  }

  // Toggle for expand/collapse
  ui_render_expand_toggle(ui, ctx, block);

  // Canvas height
  float canvas_h = 400.0f;
  if (ui->expanded_block_id == block->id) {
    canvas_h = nk_window_get_height(ctx) - 60.0f;
  }

  nk_layout_row_dynamic(ctx, canvas_h, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);

  // Get local camera state
  float cam_x = ui->canvas_state.cam_x;
  float cam_y = ui->canvas_state.cam_y;
  float zoom = ui->canvas_state.zoom;

  // --- Background: Pure white ---
  nk_fill_rect(canvas_buf, bounds, 0, nk_rgb(255, 255, 255));

  // --- Dot grid ---
  canvas_draw_dot_grid(canvas_buf, bounds, cam_x, cam_y, zoom);

  // --- Render nodes ---
  if (cd->root) {
    for (Node *node = cd->root->first_child; node; node = node->next_sibling) {
      if (!node->visible)
        continue;

      // Transform: ScreenPos = BoundsOrigin + (WorldPos - CamOffset) * Zoom
      float sx = bounds.x + (node->x - cam_x) * zoom;
      float sy = bounds.y + (node->y - cam_y) * zoom;
      float sw = node->width * zoom;
      float sh = node->height * zoom;

      // Clip to canvas bounds
      if (sx + sw < bounds.x || sx > bounds.x + bounds.w)
        continue;
      if (sy + sh < bounds.y || sy > bounds.y + bounds.h)
        continue;

      struct nk_rect nr = nk_rect(sx, sy, sw, sh);

      // Convert uint32_t RGBA to nk_color
      struct nk_color fill = {(node->fill_color >> 24) & 0xFF,
                              (node->fill_color >> 16) & 0xFF,
                              (node->fill_color >> 8) & 0xFF,
                              node->fill_color & 0xFF};

      // Draw based on node type
      if (node->type == NODE_TYPE_ELLIPSE) {
        nk_fill_circle(canvas_buf, nr, fill);
        if (node->stroke_width > 0) {
          struct nk_color stroke = {(node->stroke_color >> 24) & 0xFF,
                                    (node->stroke_color >> 16) & 0xFF,
                                    (node->stroke_color >> 8) & 0xFF,
                                    node->stroke_color & 0xFF};
          nk_stroke_circle(canvas_buf, nr, node->stroke_width * zoom, stroke);
        }
      } else {
        // Rectangle/Frame/etc
        nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, fill);
        if (node->stroke_width > 0) {
          struct nk_color stroke = {(node->stroke_color >> 24) & 0xFF,
                                    (node->stroke_color >> 16) & 0xFF,
                                    (node->stroke_color >> 8) & 0xFF,
                                    node->stroke_color & 0xFF};
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
        }
      }

      // Text content
      if (node->type == NODE_TYPE_TEXT && node->text[0]) {
        struct nk_color text_color = nk_rgb(30, 30, 30);
        nk_draw_text(canvas_buf, nr, node->text, strlen(node->text),
                     ctx->style.font, nk_rgba(0, 0, 0, 0), text_color);
      }

      // Selection highlight
      if (ui->selected_node == node) {
        struct nk_color sel_color = nk_rgb(66, 133, 244);
        nk_stroke_rect(canvas_buf, nk_rect(nr.x - 2, nr.y - 2, nr.w + 4, nr.h + 4),
                       0, 2.0f, sel_color);
      }
    }
  }

  // --- Floating toolbar ---
  canvas_render_toolbar(ui, ctx, bounds, block);

  // --- Pan indicator when space is held ---
  if (ui->canvas_state.space_held) {
    // Show hand cursor indicator
    if (ui->font_icons) {
      nk_draw_text(canvas_buf, nk_rect(bounds.x + 10, bounds.y + 10, 24, 24),
                   ICON_HAND, strlen(ICON_HAND), &ui->font_icons->handle,
                   nk_rgba(0, 0, 0, 0), nk_rgb(100, 100, 100));
    }
  }

  // --- Zoom level indicator ---
  char zoom_label[32];
  snprintf(zoom_label, sizeof(zoom_label), "%.0f%%", zoom * 100.0f);
  nk_draw_text(canvas_buf, nk_rect(bounds.x + bounds.w - 60, bounds.y + 10, 50, 20),
               zoom_label, strlen(zoom_label), ctx->style.font,
               nk_rgba(0, 0, 0, 0), nk_rgb(150, 150, 150));

  // Sync back to canvas data
  cd->view_x = ui->canvas_state.cam_x;
  cd->view_y = ui->canvas_state.cam_y;
  cd->zoom = ui->canvas_state.zoom;

  nk_spacing(ctx, 1);
}

void ui_render_block_design(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;

  ui_render_expand_toggle(ui, ctx, block);

  float scale = width / (float)dd->width;
  if (scale > 1.0f)
    scale = 1.0f;

  float display_h = (float)dd->height * scale;
  if (display_h > 400.0f)
    display_h = 400.0f;

  if (ui->expanded_block_id == block->id) {
    display_h = nk_window_get_height(ctx) - 40.0f;
  }

  nk_layout_row_dynamic(ctx, display_h, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Background color
  struct nk_color bg = {(dd->bg_color >> 24) & 0xFF,
                        (dd->bg_color >> 16) & 0xFF, (dd->bg_color >> 8) & 0xFF,
                        dd->bg_color & 0xFF};
  nk_fill_rect(canvas, bounds, 0, bg);
  nk_stroke_rect(canvas, bounds, 0, 2.0f, COLOR_ACCENT);

  // Render nodes (similar to canvas but bounded)
  if (dd->root) {
    for (Node *node = dd->root->first_child; node; node = node->next_sibling) {
      if (!node->visible)
        continue;

      float sx = bounds.x + node->x * scale;
      float sy = bounds.y + node->y * scale;
      float sw = node->width * scale;
      float sh = node->height * scale;

      struct nk_rect nr = nk_rect(sx, sy, sw, sh);

      struct nk_color fill = {
          (node->fill_color >> 24) & 0xFF, (node->fill_color >> 16) & 0xFF,
          (node->fill_color >> 8) & 0xFF, node->fill_color & 0xFF};

      nk_fill_rect(canvas, nr, node->corner_radius, fill);
    }
  }

  // Dimensions label
  char label[64];
  snprintf(label, sizeof(label), "Design %dx%d", dd->width, dd->height);
  nk_draw_text(canvas, nk_rect(bounds.x + 10, bounds.y + 10, 150, 20), label,
               strlen(label), ctx->style.font, bg, COLOR_TEXT_DIM);

  nk_spacing(ctx, 1);
}

void ui_render_block_video(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width) {
  if (!block || block->type != BLOCK_VIDEO)
    return;

  VideoData *vd = &block->data.video;

  ui_render_expand_toggle(ui, ctx, block);

  float height = 200.0f;
  if (ui->expanded_block_id == block->id) {
    height = nk_window_get_height(ctx) - 40.0f;
  }

  nk_layout_row_dynamic(ctx, height, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Video preview area
  nk_fill_rect(canvas, bounds, 0, nk_rgb(20, 20, 20));
  nk_stroke_rect(canvas, bounds, 0, 1.0f, COLOR_BORDER);

  // Timeline
  if (vd->clip_count > 0) {
    float timeline_y = bounds.y + bounds.h - 40.0f;
    float timeline_h = 30.0f;

    double total = block_video_total_duration(block);
    if (total < 0.1)
      total = 10.0;

    float clip_x = bounds.x + 10.0f;
    for (int i = 0; i < vd->clip_count; i++) {
      VideoClip *clip = &vd->clips[i];
      double dur = clip->end_time - clip->start_time;
      float clip_w = (float)(dur / total) * (bounds.w - 20.0f);

      struct nk_color clip_color =
          (i == vd->active_index) ? COLOR_ACCENT : nk_rgb(60, 80, 100);

      nk_fill_rect(canvas, nk_rect(clip_x, timeline_y, clip_w - 2, timeline_h),
                   4.0f, clip_color);
      clip_x += clip_w;
    }

    // Playhead
    float ph_x =
        bounds.x + 10.0f + (float)(vd->playhead / total) * (bounds.w - 20.0f);
    nk_stroke_line(canvas, ph_x, timeline_y - 5, ph_x,
                   timeline_y + timeline_h + 5, 2.0f, nk_rgb(255, 100, 100));
  }

  // Label
  char label[64];
  snprintf(label, sizeof(label), "Video - %d clips", vd->clip_count);
  nk_draw_text(canvas, nk_rect(bounds.x + 10, bounds.y + 10, 200, 20), label,
               strlen(label), ctx->style.font, nk_rgb(20, 20, 20), COLOR_TEXT);

  nk_spacing(ctx, 1);
}

void ui_render_block_lua(UIHost *ui, Engine *engine, struct nk_context *ctx,
                         Block *block, float width) {
  if (!block || block->type != BLOCK_LUA)
    return;

  LuaData *ld = &block->data.lua;

  // Code editor
  nk_layout_row_dynamic(ctx, 150.0f, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  nk_fill_rect(canvas, bounds, 0, nk_rgb(35, 35, 40));
  nk_stroke_rect(canvas, bounds, 0, 1.0f, COLOR_ACCENT);

  // Code editing
  if (ui->font_mono) {
    nk_style_set_font(ctx, &ui->font_mono->handle);
  }

  char buf[4096];
  strncpy(buf, ld->code ? ld->code : "-- Lua script\n", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  nk_layout_row_dynamic(ctx, 120.0f, 1);
  nk_flags flags = NK_EDIT_BOX | NK_EDIT_MULTILINE;
  nk_edit_string_zero_terminated(ctx, flags, buf, (int)sizeof(buf) - 1,
                                 nk_filter_default);

  if (strcmp(buf, ld->code ? ld->code : "") != 0) {
    block_lua_set_code(block, buf);
  }

  // Run button
  nk_layout_row_static(ctx, 24.0f, 32, 2);
  if (ui_icon_button(ui, ICON_PLAY)) {
    if (ui->L) {
      block_lua_execute(block, ui->L);
    }
  }
  if (ui_icon_button(ui, ICON_DELETE)) {
    block_lua_clear_output(block);
  }

  // Restore mono font for output
  if (ui->font_mono) {
    nk_style_set_font(ctx, &ui->font_mono->handle);
  }

  // Output
  if (ld->output && ld->output_len > 0) {
    nk_layout_row_dynamic(ctx, 60.0f, 1);
    struct nk_color out_color =
        ld->has_error ? nk_rgb(255, 100, 100) : COLOR_TEXT_DIM;
    nk_label_colored_wrap(ctx, ld->output, out_color);
  }

  if (ui->font_inter) {
    nk_style_set_font(ctx, &ui->font_inter->handle);
  }
}

// -----------------------------------------------------------------------------
// Block Dispatcher
// -----------------------------------------------------------------------------

void ui_render_block(UIHost *ui, Engine *engine, struct nk_context *ctx,
                     Page *page, Block *block, float width) {
  if (!block)
    return;

  switch (block->type) {
  case BLOCK_TEXT:
    ui_render_block_text(ui, ctx, block, width);
    break;
  case BLOCK_HEADING_1:
    ui_render_block_heading(ui, ctx, block, width, 1);
    break;
  case BLOCK_HEADING_2:
    ui_render_block_heading(ui, ctx, block, width, 2);
    break;
  case BLOCK_HEADING_3:
    ui_render_block_heading(ui, ctx, block, width, 3);
    break;
  case BLOCK_SEPARATOR:
    ui_render_block_separator(ui, ctx, block, width);
    break;
  case BLOCK_LUA:
    ui_render_block_lua(ui, engine, ctx, block, width);
    break;
  case BLOCK_TABLE:
    ui_render_block_table(ui, engine, ctx, block, width);
    break;
  case BLOCK_CANVAS:
    ui_render_block_canvas(ui, engine, ctx, block, width);
    break;
  case BLOCK_DESIGN:
    ui_render_block_design(ui, engine, ctx, block, width);
    break;
  case BLOCK_VIDEO:
    ui_render_block_video(ui, engine, ctx, block, width);
    break;
  default:
    break;
  }
}

// -----------------------------------------------------------------------------
// Main Render Function
// -----------------------------------------------------------------------------

void ui_render(UIHost *ui, Engine *engine, int win_w, int win_h) {
  if (!ui || !engine)
    return;

  struct nk_context *ctx = ui->ctx;

  // Note: nk_input_begin/end is called in main.c's event loop

  // Top bar (only in editor and not expanded)
  if (ui->screen == 1 && ui->expanded_block_id == 0) {
    ui_render_top_bar(ui, engine, ctx, win_w);
  }

  // Home or Content area
  if (ui->screen == 0) {
    ui_render_home(ui, engine, ctx, win_w, win_h);
  } else if (ui->expanded_block_id != 0) {
    // Expanded block view
    if (nk_begin(ctx, "ContentFull", nk_rect(0, 0, (float)win_w, (float)win_h),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
      Page *page = doc_page_get_active(engine->doc);
      if (page) {
        Block *block = page_find_block_by_id(page, ui->expanded_block_id);
        if (block) {
          ui_render_block(ui, engine, ctx, page, block, (float)win_w);
        } else {
          ui->expanded_block_id = 0; // Block not found/deleted
        }
      }
    }
    nk_end(ctx);
  } else {
    const float top_h = 40.0f;
    const float bottom_h = 36.0f;
    const float content_h = (float)win_h - top_h - bottom_h;
    const float padding = 40.0f;
    const float content_w = (float)win_w - padding * 2;

    if (nk_begin(ctx, "Content", nk_rect(padding, top_h, content_w, content_h),
                 NK_WINDOW_BORDER | NK_WINDOW_SCROLL_AUTO_HIDE)) {

      Page *page = doc_page_get_active(engine->doc);

      if (page && page->block_count > 0) {
        for (Block *b = page->first_block; b; b = b->next) {
          ui_render_block(ui, engine, ctx, page, b, content_w - 20);

          // Small spacing between blocks
          nk_layout_row_dynamic(ctx, 8.0f, 1);
          nk_spacing(ctx, 1);
        }
      } else {
        // Empty state
        nk_layout_row_dynamic(ctx, 100.0f, 1);
        nk_label(ctx, "No blocks yet. Use the toolbar above to add content.",
                 NK_TEXT_CENTERED);
      }
    }
    nk_end(ctx);
  }

  // Bottom bar
  if (ui->screen != 0 && ui->expanded_block_id == 0) {
    ui_render_bottom_bar(ui, engine, ctx, win_w, win_h);
  }

  // Render Nuklear
  nk_sdl_render(NK_ANTI_ALIASING_ON);
}

// -----------------------------------------------------------------------------
// Home Screen (Dashboard)
// -----------------------------------------------------------------------------

void ui_render_home(UIHost *ui, Engine *engine, struct nk_context *ctx,
                    int win_w, int win_h) {
  if (!ui || !engine || !ctx)
    return;

  // Use a clean background for the dashboard
  if (nk_begin(ctx, "Dashboard",
               nk_rect(0, 0, (float)win_w, (float)win_h - 36.0f),
               NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {

    nk_layout_row_dynamic(ctx, 100, 1);
    nk_spacing(ctx, 1);

    nk_layout_row_dynamic(ctx, 40, 1);
    nk_label(ctx, "Aslan Dashboard", NK_TEXT_CENTERED);

    nk_layout_row_dynamic(ctx, 60, 3);
    nk_spacing(ctx, 1);
    if (nk_button_label(ctx, "New Document")) {
      char name[64] = "New Note";
      Page *new_page = doc_page_create(engine->doc, name);
      if (new_page) {
        doc_page_activate(engine->doc, new_page->id);
        ui->screen = 1;

        // Default Note setup: H1 + Text
        Block *h1 = block_create(new_page, BLOCK_HEADING_1);
        if (h1) {
          block_set_text(h1, name);
          page_append_block(new_page, h1);
        }

        Block *txt = block_create(new_page, BLOCK_TEXT);
        if (txt) {
          block_set_text(txt, "Start writing here...");
          page_append_block(new_page, txt);
        }

        char cmd[128];
        snprintf(cmd, sizeof(cmd), "page_create %u \"%s\"", new_page->id, name);
        engine_execute_command(engine, cmd);
        if (h1) {
          snprintf(cmd, sizeof(cmd), "block_create %u %u h1", new_page->id,
                   h1->id);
          engine_execute_command(engine, cmd);
          snprintf(cmd, sizeof(cmd), "block_text %u %u \"%s\"", new_page->id,
                   h1->id, name);
          engine_execute_command(engine, cmd);
        }
        if (txt) {
          snprintf(cmd, sizeof(cmd), "block_create %u %u text", new_page->id,
                   txt->id);
          engine_execute_command(engine, cmd);
          snprintf(cmd, sizeof(cmd),
                   "block_text %u %u \"Start writing here...\"", new_page->id,
                   txt->id);
          engine_execute_command(engine, cmd);
        }
      }
    }
    nk_spacing(ctx, 1);

    nk_layout_row_dynamic(ctx, 40, 1);
    nk_spacing(ctx, 1);
    nk_label(ctx, "Recent Documents", NK_TEXT_CENTERED);

    // List recents
    for (int i = 0; i < ui->recents.count; i++) {
      nk_layout_row_dynamic(ctx, 30, 3);
      nk_spacing(ctx, 1);
      if (nk_button_label(ctx, ui->recents.paths[i])) {
        // Try to find the page by name for now
        for (int p = 0; p < 32; p++) {
          if (engine->doc->pages[p].in_use &&
              strcmp(engine->doc->pages[p].name, ui->recents.paths[i]) == 0) {
            doc_page_activate(engine->doc, engine->doc->pages[p].id);
            ui->screen = 1;
            break;
          }
        }
      }
      nk_spacing(ctx, 1);
    }

    if (ui->recents.count == 0) {
      nk_layout_row_dynamic(ctx, 30, 1);
      nk_label(ctx, "No recent notes found.", NK_TEXT_CENTERED);
    }
  }
  nk_end(ctx);
}
