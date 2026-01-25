// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear implementation (must be before nuklear.h include)
#define NK_IMPLEMENTATION
#include "ui_host.h"
#include "../scripting/lua_host.h"
#include "mac_image.h"
#include "../media/ffmpeg_pipe.h"
#include <SDL.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

// -----------------------------------------------------------------------------
// File Dialog Helper (macOS)
// -----------------------------------------------------------------------------

// Opens a native file dialog to select an image file
// Returns 1 on success, 0 on cancel/error. Path is written to out_path.
static int open_image_file_dialog(char *out_path, size_t path_size) {
  // Use osascript to open macOS native file dialog
  const char *script =
      "osascript -e 'set theFile to choose file of type {\"public.image\"} "
      "with prompt \"Select an image\"' -e 'POSIX path of theFile' 2>/dev/null";

  FILE *fp = popen(script, "r");
  if (!fp)
    return 0;

  char result[512] = {0};
  if (fgets(result, sizeof(result), fp) != NULL) {
    // Remove trailing newline
    size_t len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') {
      result[len - 1] = '\0';
      len--;
    }
    if (len > 0) {
      strncpy(out_path, result, path_size - 1);
      out_path[path_size - 1] = '\0';
      pclose(fp);
      return 1;
    }
  }

  pclose(fp);
  return 0;
}

// Copy file to assets directory, returns path relative to document
// Creates assets/ directory if needed
static int copy_image_to_assets(const char *src_path, const char *doc_dir,
                                char *out_rel_path, size_t rel_path_size) {
  // Create assets directory
  char assets_dir[512];
  snprintf(assets_dir, sizeof(assets_dir), "%s/assets", doc_dir);
  mkdir(assets_dir, 0755); // Ignore error if exists

  // Debug log
  FILE *log = fopen("ui_debug.log", "a");
  if (log) {
    fprintf(log, "Copying image: %s -> %s\n", src_path, assets_dir);
    fclose(log);
  }

  // Get filename from path
  const char *filename = strrchr(src_path, '/');
  filename = filename ? filename + 1 : src_path;

  // Generate unique filename if needed (add timestamp)
  char dest_filename[256];
  time_t now = time(NULL);
  snprintf(dest_filename, sizeof(dest_filename), "%ld_%s", (long)now, filename);

  // Full destination path
  char dest_path[512];
  snprintf(dest_path, sizeof(dest_path), "%s/%s", assets_dir, dest_filename);

  // Copy file
  FILE *src = fopen(src_path, "rb");
  if (!src)
    return 0;

  FILE *dst = fopen(dest_path, "wb");
  if (!dst) {
    fclose(src);
    return 0;
  }

  char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
    fwrite(buf, 1, n, dst);
  }

  fclose(src);
  fclose(dst);

  // Return relative path
  snprintf(out_rel_path, rel_path_size, "assets/%s", dest_filename);
  return 1;
}

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
  ui->renderer = renderer;
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

  // NOTE: mouse_just_pressed/released are reset in ui_render after processing

  // Track mouse position
  if (evt->type == SDL_MOUSEMOTION) {
    ui->mouse_x = (float)evt->motion.x;
    ui->mouse_y = (float)evt->motion.y;

    // Check if mouse is in canvas
    CanvasInteraction *cs = &ui->canvas_state;
    cs->mouse_in_canvas = (ui->mouse_x >= cs->bounds_x &&
                           ui->mouse_x < cs->bounds_x + cs->bounds_w &&
                           ui->mouse_y >= cs->bounds_y &&
                           ui->mouse_y < cs->bounds_y + cs->bounds_h);

    // Handle canvas panning
    if (cs->is_panning) {
      float dx = ui->mouse_x - cs->pan_start_x;
      float dy = ui->mouse_y - cs->pan_start_y;
      cs->cam_x = cs->cam_start_x - dx / cs->zoom;
      cs->cam_y = cs->cam_start_y - dy / cs->zoom;
    }

    // Handle node dragging
    if (cs->is_dragging_node && ui->selected_node) {
      float world_x = cs->cam_x + (ui->mouse_x - cs->bounds_x) / cs->zoom;
      float world_y = cs->cam_y + (ui->mouse_y - cs->bounds_y) / cs->zoom;
      ui->selected_node->x = world_x - cs->drag_offset_x;
      ui->selected_node->y = world_y - cs->drag_offset_y;
    }

    // Handle element creation preview
    if (cs->is_creating) {
      cs->create_end_x = cs->cam_x + (ui->mouse_x - cs->bounds_x) / cs->zoom;
      cs->create_end_y = cs->cam_y + (ui->mouse_y - cs->bounds_y) / cs->zoom;
    }
  }

  // Track mouse buttons
  if (evt->type == SDL_MOUSEBUTTONDOWN) {
    CanvasInteraction *cs = &ui->canvas_state;

    // IMPORTANT: Update mouse position from click event (not just motion)
    ui->mouse_x = (float)evt->button.x;
    ui->mouse_y = (float)evt->button.y;

    // Update mouse_in_canvas check
    cs->mouse_in_canvas = (ui->mouse_x >= cs->bounds_x &&
                           ui->mouse_x < cs->bounds_x + cs->bounds_w &&
                           ui->mouse_y >= cs->bounds_y &&
                           ui->mouse_y < cs->bounds_y + cs->bounds_h);

    if (evt->button.button == SDL_BUTTON_LEFT) {
      ui->mouse_buttons |= 1;
      cs->mouse_just_pressed = 1;

      // Start panning with space + left click
      if (cs->space_held && cs->mouse_in_canvas) {
        cs->is_panning = 1;
        cs->pan_start_x = ui->mouse_x;
        cs->pan_start_y = ui->mouse_y;
        cs->cam_start_x = cs->cam_x;
        cs->cam_start_y = cs->cam_y;
      }
    }

    if (evt->button.button == SDL_BUTTON_MIDDLE) {
      ui->mouse_buttons |= 2;
      // Start panning with middle mouse button
      if (cs->mouse_in_canvas) {
        cs->is_panning = 1;
        cs->pan_start_x = ui->mouse_x;
        cs->pan_start_y = ui->mouse_y;
        cs->cam_start_x = cs->cam_x;
        cs->cam_start_y = cs->cam_y;
      }
    }

    if (evt->button.button == SDL_BUTTON_RIGHT)
      ui->mouse_buttons |= 4;
  }

  if (evt->type == SDL_MOUSEBUTTONUP) {
    CanvasInteraction *cs = &ui->canvas_state;

    // Update mouse position from release event
    ui->mouse_x = (float)evt->button.x;
    ui->mouse_y = (float)evt->button.y;

    if (evt->button.button == SDL_BUTTON_LEFT) {
      ui->mouse_buttons &= ~1;
      cs->mouse_just_released = 1;

      // Stop panning if was space+left panning
      if (cs->space_held) {
        cs->is_panning = 0;
      }

      // Stop dragging node
      cs->is_dragging_node = 0;

      // Stop resizing
      cs->is_resizing = 0;
    }

    if (evt->button.button == SDL_BUTTON_MIDDLE) {
      ui->mouse_buttons &= ~2;
      cs->is_panning = 0;
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

  // Inline text editing keyboard handling (Freeform-style)
  if (ui->editing_node) {
    if (evt->type == SDL_KEYDOWN) {
      if (evt->key.keysym.sym == SDLK_RETURN || evt->key.keysym.sym == SDLK_KP_ENTER) {
        // Confirm edit and exit editing mode
        strncpy(ui->editing_node->text, ui->edit_buffer, sizeof(ui->editing_node->text) - 1);
        ui->editing_node->text[sizeof(ui->editing_node->text) - 1] = '\0';
        ui->editing_node = NULL;
      } else if (evt->key.keysym.sym == SDLK_ESCAPE) {
        // Cancel edit (revert to original)
        ui->editing_node = NULL;
      } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
        // Delete last character
        size_t len = strlen(ui->edit_buffer);
        if (len > 0) {
          ui->edit_buffer[len - 1] = '\0';
        }
      }
    } else if (evt->type == SDL_TEXTINPUT) {
      // Append typed text
      size_t len = strlen(ui->edit_buffer);
      size_t input_len = strlen(evt->text.text);
      if (len + input_len < sizeof(ui->edit_buffer) - 1) {
        strcat(ui->edit_buffer, evt->text.text);
      }
    }
    return; // Don't process other events while editing
  }

  // Delete selected node
  if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_BACKSPACE) {
    if (ui->selected_node && ui->canvas_state.active_block) {
      CanvasData *cd = &ui->canvas_state.active_block->data.canvas;
      // Remove node from tree (simplified - just hide it)
      ui->selected_node->visible = 0;
      ui->selected_node = NULL;
    }
  }

  // Mouse wheel zoom toward cursor
  if (evt->type == SDL_MOUSEWHEEL) {
    CanvasInteraction *cs = &ui->canvas_state;
    // Initialize zoom if not set
    if (cs->zoom < 0.01f)
      cs->zoom = 1.0f;
    if (cs->mouse_in_canvas) {
      float old_zoom = cs->zoom;
      float zoom_factor = 1.15f;

      if (evt->wheel.y > 0) {
        cs->zoom *= zoom_factor;
      } else if (evt->wheel.y < 0) {
        cs->zoom /= zoom_factor;
      }

      // Clamp zoom
      if (cs->zoom < 0.1f)
        cs->zoom = 0.1f;
      if (cs->zoom > 5.0f)
        cs->zoom = 5.0f;

      // Zoom toward mouse cursor
      float mouse_rel_x = ui->mouse_x - cs->bounds_x;
      float mouse_rel_y = ui->mouse_y - cs->bounds_y;

      // World position under mouse before zoom
      float world_x = cs->cam_x + mouse_rel_x / old_zoom;
      float world_y = cs->cam_y + mouse_rel_y / old_zoom;

      // Adjust camera so same world point stays under mouse
      cs->cam_x = world_x - mouse_rel_x / cs->zoom;
      cs->cam_y = world_y - mouse_rel_y / cs->zoom;
    }
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
                                 struct nk_rect bounds, float cam_x,
                                 float cam_y, float zoom) {
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
      nk_fill_circle(canvas,
                     nk_rect(screen_x - dot_radius, screen_y - dot_radius,
                             dot_radius * 2, dot_radius * 2),
                     dot_color);
    }
  }
}

// -----------------------------------------------------------------------------
// Canvas Helper: Check if point is in toolbar area
// -----------------------------------------------------------------------------

static int canvas_point_in_toolbar(struct nk_rect bounds, float mx, float my) {
  const float toolbar_w = 200.0f;
  const float toolbar_h = 40.0f;
  const float margin = 16.0f;
  float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
  float toolbar_y = bounds.y + bounds.h - toolbar_h - margin;
  return (mx >= toolbar_x && mx < toolbar_x + toolbar_w && my >= toolbar_y &&
          my < toolbar_y + toolbar_h);
}

// -----------------------------------------------------------------------------
// Canvas Helper: Render floating toolbar (Apple Freeform style)
// -----------------------------------------------------------------------------

// Check if tool is a form tool
static int is_form_tool(CanvasToolMode tool) {
  return tool == CANVAS_TOOL_FORM_RECT || tool == CANVAS_TOOL_FORM_ELLIPSE ||
         tool == CANVAS_TOOL_FORM_TRIANGLE || tool == CANVAS_TOOL_FORM_LINE;
}

static void canvas_render_toolbar(UIHost *ui, struct nk_context *ctx,
                                  struct nk_rect bounds, Block *block) {
  (void)block;

  // 5 buttons: Forms, Text, Note, Image, Pen
  const float btn_size = 32.0f;
  const float btn_spacing = 6.0f;
  const float toolbar_w = 5 * btn_size + 4 * btn_spacing + 24.0f;
  const float toolbar_h = 44.0f;
  const float margin = 16.0f;

  float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
  float toolbar_y = bounds.y + bounds.h - toolbar_h - margin;

  struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
  struct nk_color toolbar_bg = {255, 255, 255, 250};
  struct nk_color toolbar_border = {200, 200, 200, 255};

  CanvasInteraction *cs = &ui->canvas_state;

  // Shadow
  nk_fill_rect(canvas_buf,
               nk_rect(toolbar_x + 2, toolbar_y + 3, toolbar_w, toolbar_h),
               12.0f, nk_rgba(0, 0, 0, 25));

  // Background
  nk_fill_rect(canvas_buf, nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h),
               12.0f, toolbar_bg);
  nk_stroke_rect(canvas_buf,
                 nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h), 12.0f,
                 1.0f, toolbar_border);

  float btn_x = toolbar_x + 12.0f;
  float btn_y = toolbar_y + (toolbar_h - btn_size) / 2.0f;

  // Tool icons and modes
  // 0: Forms (shapes), 1: Text, 2: Note, 3: Image, 4: Pen
  // Icons: Shapes, A (letter for text), Sticky Note, Image, Pen (Quill)
  // Using "A" for Text because icon fonts don't render well for text icon
  const char *tool_icons[] = {ICON_SHAPE, "A", ICON_STICKY,
                              "\xEE\xB9\xA4", "\xEF\x86\x95"};
  int use_icon_font[] = {1, 0, 1, 1, 1}; // Text uses regular font (A)
  int tool_count = 5;

  for (int i = 0; i < tool_count; i++) {
    struct nk_rect btn = nk_rect(btn_x + i * (btn_size + btn_spacing), btn_y,
                                 btn_size, btn_size);

    int is_active = 0;
    if (i == 0)
      is_active = cs->show_forms_menu;
    else if (i == 1)
      is_active = 0; // Text never stays active
    else if (i == 2)
      is_active = 0; // Note never stays active
    else if (i == 3)
      is_active = 0; // Image never stays active
    else if (i == 4)
      is_active = (cs->tool == CANVAS_TOOL_PEN);

    int hovered = (ui->mouse_x >= btn.x && ui->mouse_x < btn.x + btn.w &&
                   ui->mouse_y >= btn.y && ui->mouse_y < btn.y + btn.h);

    struct nk_color bg_color = is_active ? nk_rgba(66, 133, 244, 80)
                               : hovered ? nk_rgba(0, 0, 0, 20)
                                         : nk_rgba(0, 0, 0, 0);
    nk_fill_rect(canvas_buf, btn, 6.0f, bg_color);

    // Draw icon or text
    struct nk_color icon_color =
        is_active ? nk_rgb(66, 133, 244) : nk_rgb(80, 80, 80);
    if (use_icon_font[i] && ui->font_icons) {
      nk_draw_text(canvas_buf,
                   nk_rect(btn.x + 6, btn.y + 6, btn_size - 12, btn_size - 12),
                   tool_icons[i], strlen(tool_icons[i]),
                   &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), icon_color);
    } else {
      // Use regular font for "T"
      nk_draw_text(canvas_buf,
                   nk_rect(btn.x + 8, btn.y + 4, btn_size - 16, btn_size - 8),
                   tool_icons[i], strlen(tool_icons[i]), ctx->style.font,
                   nk_rgba(0, 0, 0, 0), icon_color);
    }

    if (hovered && cs->mouse_just_pressed) {
      if (i == 0) {
        cs->show_forms_menu = !cs->show_forms_menu;
        // If switching to forms menu, ensure we are in select mode (exit pen)
        if (cs->show_forms_menu) {
            cs->tool = CANVAS_TOOL_SELECT;
        }
      } else if (i == 4) {
        // Pen tool - toggle
        if (cs->tool == CANVAS_TOOL_PEN) {
            cs->tool = CANVAS_TOOL_SELECT; // Toggle off
        } else {
            cs->tool = CANVAS_TOOL_PEN;
            cs->show_forms_menu = 0;
            if (cs->pen_thickness < 0.1f)
              cs->pen_thickness = 2.0f;
            if (cs->pen_color == 0)
              cs->pen_color = 0x000000FF;
        }
      } else {
        // Create element directly in center of visible canvas
        cs->show_forms_menu = 0;
        cs->pending_create = i; // 1=Text, 2=Note, 3=Image
        cs->tool = CANVAS_TOOL_SELECT;
      }
    }
  }

  // --- Pen Settings Menu (above toolbar) ---
  if (cs->tool == CANVAS_TOOL_PEN) {
    const float menu_w = 200.0f;
    const float menu_h = 100.0f;
    float menu_x = toolbar_x + (toolbar_w - menu_w) / 2.0f;
    float menu_y = toolbar_y - menu_h - 8.0f;

    // Shadow
    nk_fill_rect(canvas_buf, nk_rect(menu_x + 2, menu_y + 3, menu_w, menu_h),
                 10.0f, nk_rgba(0, 0, 0, 25));
    // Background
    nk_fill_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 10.0f,
                 toolbar_bg);
    nk_stroke_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 10.0f,
                   1.0f, toolbar_border);

    // Render Pen Settings manually (since we are inside canvas drawing)
    // 1. Color Palette
    uint32_t colors[] = {0x000000FF, 0xD93025FF, 0xF9AB00FF, 0x1E8E3EFF,
                         0x1A73E8FF, 0xA142F4FF};
    float cx = menu_x + 12.0f;
    float cy = menu_y + 12.0f;
    float sz = 24.0f;

    for (int c = 0; c < 6; c++) {
      struct nk_rect cr = nk_rect(cx + c * (sz + 6), cy, sz, sz);
      struct nk_color col = nk_rgba((colors[c] >> 24) & 0xFF,
                                    (colors[c] >> 16) & 0xFF,
                                    (colors[c] >> 8) & 0xFF, colors[c] & 0xFF);
      nk_fill_circle(canvas_buf, cr, col);

      // Selected indicator
      if (cs->pen_color == colors[c]) {
        nk_stroke_circle(canvas_buf, cr, 2.0f, nk_rgb(100, 100, 100));
        nk_fill_circle(canvas_buf,
                       nk_rect(cr.x + 8, cr.y + 8, cr.w - 16, cr.h - 16),
                       nk_rgb(255, 255, 255));
      }

      // Interaction
      if (ui->mouse_x >= cr.x && ui->mouse_x < cr.x + cr.w &&
          ui->mouse_y >= cr.y && ui->mouse_y < cr.y + cr.h) {
        if (cs->mouse_just_pressed) {
          cs->pen_color = colors[c];
        }
      }
    }

    // 2. Thickness Slider (Simulated)
    cy += sz + 16.0f;
    float slider_w = menu_w - 24.0f;
    float slider_h = 4.0f;
    nk_fill_rect(canvas_buf, nk_rect(cx, cy + 8, slider_w, slider_h), 2.0f,
                 nk_rgb(200, 200, 200));

    // Handle
    float t_min = 1.0f, t_max = 10.0f;
    float t_ratio = (cs->pen_thickness - t_min) / (t_max - t_min);
    float hx = cx + t_ratio * slider_w;
    struct nk_rect hr = nk_rect(hx - 8, cy, 16, 20);
    nk_fill_rect(canvas_buf, hr, 4.0f, nk_rgb(100, 100, 100));

    // Interaction for slider
    if (ui->mouse_buttons & 1) {
      if (ui->mouse_x >= cx && ui->mouse_x <= cx + slider_w &&
          ui->mouse_y >= cy - 5 && ui->mouse_y <= cy + 25) {
        float ratio = (ui->mouse_x - cx) / slider_w;
        if (ratio < 0)
          ratio = 0;
        if (ratio > 1)
          ratio = 1;
        cs->pen_thickness = t_min + ratio * (t_max - t_min);
      }
    }

    // 3. Marker Toggle
    cy += 30.0f;
    struct nk_rect mr = nk_rect(cx, cy, 80, 24);
    nk_fill_rect(canvas_buf, mr, 4.0f,
                 cs->pen_is_marker ? nk_rgb(200, 220, 255) : nk_rgb(240, 240, 240));
    nk_stroke_rect(canvas_buf, mr, 4.0f, 1.0f, nk_rgb(200, 200, 200));
    
    // Text "Marker"
    if (ui->font_inter)
       nk_draw_text(canvas_buf, nk_rect(cx + 10, cy + 4, 60, 16), "Marker", 6, &ui->font_inter->handle, nk_rgba(0,0,0,0), nk_rgb(50,50,50));

    if (cs->mouse_just_pressed) {
        if (ui->mouse_x >= mr.x && ui->mouse_x < mr.x + mr.w &&
            ui->mouse_y >= mr.y && ui->mouse_y < mr.y + mr.h) {
            cs->pen_is_marker = !cs->pen_is_marker;
        }
    }
  }

  // --- Forms submenu (above toolbar) ---
  if (cs->show_forms_menu) {
    const float menu_w = 4 * btn_size + 3 * btn_spacing + 16.0f;
    const float menu_h = 44.0f;
    float menu_x = toolbar_x + (toolbar_w - menu_w) / 2.0f;
    float menu_y = toolbar_y - menu_h - 8.0f;

    // Shadow
    nk_fill_rect(canvas_buf, nk_rect(menu_x + 2, menu_y + 3, menu_w, menu_h),
                 10.0f, nk_rgba(0, 0, 0, 25));
    // Background
    nk_fill_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 10.0f,
                 toolbar_bg);
    nk_stroke_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 10.0f,
                   1.0f, toolbar_border);

    // Form buttons: Rectangle, Ellipse, Triangle, Line
    CanvasToolMode form_tools[] = {
        CANVAS_TOOL_FORM_RECT, CANVAS_TOOL_FORM_ELLIPSE,
        CANVAS_TOOL_FORM_TRIANGLE, CANVAS_TOOL_FORM_LINE};

    float form_btn_x = menu_x + 8.0f;
    float form_btn_y = menu_y + 6.0f;

    for (int i = 0; i < 4; i++) {
      struct nk_rect fbtn = nk_rect(form_btn_x + i * (btn_size + btn_spacing),
                                    form_btn_y, btn_size, btn_size);

      int is_active = (cs->tool == form_tools[i]);
      int hovered = (ui->mouse_x >= fbtn.x && ui->mouse_x < fbtn.x + fbtn.w &&
                     ui->mouse_y >= fbtn.y && ui->mouse_y < fbtn.y + fbtn.h);

      struct nk_color bg = is_active ? nk_rgba(66, 133, 244, 80)
                           : hovered ? nk_rgba(0, 0, 0, 20)
                                     : nk_rgba(0, 0, 0, 0);
      nk_fill_rect(canvas_buf, fbtn, 6.0f, bg);

      // Draw shape directly using nk primitives
      struct nk_color shape_color =
          is_active ? nk_rgb(66, 133, 244) : nk_rgb(80, 80, 80);
      float cx = fbtn.x + btn_size / 2.0f;
      float cy = fbtn.y + btn_size / 2.0f;
      float sz = 10.0f;

      if (i == 0) {
        // Rectangle
        nk_stroke_rect(canvas_buf,
                       nk_rect(cx - sz, cy - sz * 0.7f, sz * 2, sz * 1.4f),
                       2.0f, 1.5f, shape_color);
      } else if (i == 1) {
        // Ellipse
        nk_stroke_circle(canvas_buf, nk_rect(cx - sz, cy - sz, sz * 2, sz * 2),
                         1.5f, shape_color);
      } else if (i == 2) {
        // Triangle
        nk_stroke_triangle(canvas_buf, cx, cy - sz, cx - sz, cy + sz * 0.7f,
                           cx + sz, cy + sz * 0.7f, 1.5f, shape_color);
      } else if (i == 3) {
        // Line
        nk_stroke_line(canvas_buf, cx - sz, cy + sz * 0.5f, cx + sz,
                       cy - sz * 0.5f, 1.5f, shape_color);
      }

      if (hovered && cs->mouse_just_pressed) {
        // Create form directly: 10=rect, 11=ellipse, 12=triangle, 13=line
        cs->pending_create = 10 + i;
        cs->show_forms_menu = 0;
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Canvas Helper: Create new node in canvas
// -----------------------------------------------------------------------------

// Node style types for creation
typedef enum {
  NODE_STYLE_RECT,
  NODE_STYLE_ELLIPSE,
  NODE_STYLE_TRIANGLE,
  NODE_STYLE_LINE,
  NODE_STYLE_TEXT, // Plain text, no background
  NODE_STYLE_NOTE  // Sticky note with background
} NodeStyleType;

static Node *canvas_create_styled_node(CanvasData *cd, NodeStyleType style,
                                       float x, float y, float w, float h) {
  if (!cd->root)
    return NULL;

  Node *node = (Node *)calloc(1, sizeof(Node));
  if (!node)
    return NULL;

  node->id = cd->next_node_id++;
  node->x = x;
  node->y = y;
  node->width = w;
  node->height = h;
  node->visible = 1;
  node->parent = cd->root;

  switch (style) {
  case NODE_STYLE_RECT:
    node->type = NODE_TYPE_RECT;
    node->fill_color = 0x4285F4FF; // Blue
    node->stroke_color = 0x1A73E8FF;
    node->stroke_width = 0.0f;
    node->corner_radius = 8.0f;
    break;

  case NODE_STYLE_ELLIPSE:
    node->type = NODE_TYPE_ELLIPSE;
    node->fill_color = 0x34A853FF; // Green
    node->stroke_color = 0x1E8E3EFF;
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_TRIANGLE:
    node->type = NODE_TYPE_TRIANGLE;
    node->fill_color = 0xFBBC04FF; // Yellow
    node->stroke_color = 0xF9A825FF;
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_LINE:
    node->type = NODE_TYPE_RECT;
    node->fill_color = 0x5F6368FF; // Gray
    node->stroke_color = 0x5F6368FF;
    node->stroke_width = 2.0f;
    node->corner_radius = 0.0f;
    node->height = 4.0f; // Thin line
    break;

  case NODE_STYLE_TEXT:
    node->type = NODE_TYPE_TEXT;
    node->fill_color = 0x00000000; // Transparent - no background
    node->stroke_color = 0x00000000;
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    strncpy(node->text, "Text", sizeof(node->text) - 1);
    break;

  case NODE_STYLE_NOTE:
    node->type = NODE_TYPE_TEXT;
    node->fill_color = 0xFFF9C4FF; // Light yellow
    node->stroke_color = 0xFFEB3BFF;
    node->stroke_width = 1.0f;
    node->corner_radius = 4.0f;
    strncpy(node->text, "Note", sizeof(node->text) - 1);
    break;
  }

  // Add to tree
  if (!cd->root->first_child) {
    cd->root->first_child = node;
  } else {
    Node *last = cd->root->first_child;
    while (last->next_sibling)
      last = last->next_sibling;
    last->next_sibling = node;
  }

  return node;
}

// -----------------------------------------------------------------------------
// Canvas Helper: Find node at world position
// -----------------------------------------------------------------------------

static Node *canvas_find_node_at(CanvasData *cd, float world_x, float world_y) {
  if (!cd->root)
    return NULL;

  // Iterate in reverse to get topmost node first
  Node *found = NULL;
  for (Node *node = cd->root->first_child; node; node = node->next_sibling) {
    if (!node->visible)
      continue;
    if (world_x >= node->x && world_x < node->x + node->width &&
        world_y >= node->y && world_y < node->y + node->height) {
      found = node; // Keep going to find topmost
    }
  }
  return found;
}

// -----------------------------------------------------------------------------
// Texture Cache (Simple LRU)
// -----------------------------------------------------------------------------

typedef struct {
  char path[512];
  SDL_Texture *tex;
  int width;
  int height;
  uint32_t last_used;
  int in_use;
} TextureCacheEntry;

#define TEX_CACHE_SIZE 64
static TextureCacheEntry tex_cache[TEX_CACHE_SIZE];
static uint32_t tex_cache_counter = 0;

static SDL_Texture *get_image_texture(UIHost *ui, const char *path) {
  if (!path || !path[0])
    return NULL;

  tex_cache_counter++;

  // Check cache
  for (int i = 0; i < TEX_CACHE_SIZE; i++) {
    if (tex_cache[i].in_use && strcmp(tex_cache[i].path, path) == 0) {
      tex_cache[i].last_used = tex_cache_counter;
      return tex_cache[i].tex;
    }
  }

  // Not in cache, load it using macOS native loader
  int w, h;
  void *pixels = mac_load_image_rgba(path, &w, &h);

  if (!pixels) {
    // Try relative to assets if not absolute
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "assets/%s", path);
    pixels = mac_load_image_rgba(full_path, &w, &h);
    
    // Debug
    FILE *f = fopen("ui_debug.log", "a");
    if (f) {
        fprintf(f, "get_image_texture failed for %s, trying %s -> %p\n", path, full_path, pixels);
        fclose(f);
    }
  }

  if (!pixels) return NULL;

  // Create texture
  SDL_Texture *tex = SDL_CreateTexture(ui->renderer,
                                       SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC,
                                       w, h);
  if (tex) {
    SDL_UpdateTexture(tex, NULL, pixels, w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  }

  free(pixels);

  if (!tex) return NULL;

  // Find slot in cache
  int slot = -1;
  // First pass: empty slot
  for (int i = 0; i < TEX_CACHE_SIZE; i++) {
    if (!tex_cache[i].in_use) {
      slot = i;
      break;
    }
  }
  // Second pass: LRU
  if (slot == -1) {
    uint32_t min_time = 0xFFFFFFFF;
    for (int i = 0; i < TEX_CACHE_SIZE; i++) {
      if (tex_cache[i].last_used < min_time) {
        min_time = tex_cache[i].last_used;
        slot = i;
      }
    }
    // Free old texture
    if (tex_cache[slot].tex) {
      SDL_DestroyTexture(tex_cache[slot].tex);
    }
  }

  // Store in cache
  strncpy(tex_cache[slot].path, path, sizeof(tex_cache[slot].path) - 1);
  tex_cache[slot].tex = tex;
  tex_cache[slot].width = w;
  tex_cache[slot].height = h;
  tex_cache[slot].last_used = tex_cache_counter;
  tex_cache[slot].in_use = 1;

  return tex;
}

// -----------------------------------------------------------------------------
// Canvas: Main render function
// -----------------------------------------------------------------------------

void ui_render_block_canvas(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
  (void)engine;
  (void)width;

  if (!block || block->type != BLOCK_CANVAS)
    return;

  CanvasData *cd = &block->data.canvas;
  CanvasInteraction *cs = &ui->canvas_state;

  // Sync UI state with canvas data on first render or when switching canvas
  if (cs->active_canvas_id != block->id) {
    cs->active_canvas_id = block->id;
    cs->active_block = block;
    cs->cam_x = cd->view_x;
    cs->cam_y = cd->view_y;
    cs->zoom = cd->zoom > 0.01f ? cd->zoom : 1.0f;
    cs->is_creating = 0;
    cs->is_dragging_node = 0;
    ui->selected_node = NULL;
  }
  cs->active_block = block;

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

  // Store bounds for event handling
  cs->bounds_x = bounds.x;
  cs->bounds_y = bounds.y;
  cs->bounds_w = bounds.w;
  cs->bounds_h = bounds.h;

  float cam_x = cs->cam_x;
  float cam_y = cs->cam_y;
  float zoom = cs->zoom;

  // Update mouse-in-canvas state
  cs->mouse_in_canvas =
      (ui->mouse_x >= bounds.x && ui->mouse_x < bounds.x + bounds.w &&
       ui->mouse_y >= bounds.y && ui->mouse_y < bounds.y + bounds.h);

  // --- Background: Pure white ---
  nk_fill_rect(canvas_buf, bounds, 0, nk_rgb(255, 255, 255));

  // --- Dot grid ---
  canvas_draw_dot_grid(canvas_buf, bounds, cam_x, cam_y, zoom);

  // --- Handle input ---
  int in_toolbar = canvas_point_in_toolbar(bounds, ui->mouse_x, ui->mouse_y);

  // Check if in forms menu area
  int in_forms_menu = 0;
  if (cs->show_forms_menu) {
    float menu_x = bounds.x + (bounds.w - 160.0f) / 2.0f;
    float menu_y = bounds.y + bounds.h - 40.0f - 16.0f - 44.0f - 8.0f;
    in_forms_menu = (ui->mouse_x >= menu_x && ui->mouse_x < menu_x + 160.0f &&
                     ui->mouse_y >= menu_y && ui->mouse_y < menu_y + 44.0f);
  }

  // Close forms menu if clicking elsewhere
  if (cs->mouse_just_pressed && !in_toolbar && !in_forms_menu) {
    cs->show_forms_menu = 0;
  }

  // --- Handle pending element creation (from toolbar button click) ---
  if (cs->pending_create > 0) {
    // Calculate center of visible canvas in world coords
    float center_world_x = cam_x + (bounds.w / 2.0f) / zoom;
    float center_world_y = cam_y + (bounds.h / 2.0f) / zoom;

    Node *new_node = NULL;
    float def_w = 100.0f;
    float def_h = 80.0f;
    float nx = center_world_x - def_w / 2.0f;
    float ny = center_world_y - def_h / 2.0f;

    if (cs->pending_create == 1) {
      // Text
      new_node =
          canvas_create_styled_node(cd, NODE_STYLE_TEXT, nx, ny, 120.0f, 30.0f);
    } else if (cs->pending_create == 2) {
      // Note
      new_node = canvas_create_styled_node(cd, NODE_STYLE_NOTE, nx, ny, 150.0f,
                                           100.0f);
    } else if (cs->pending_create == 3) {
      // Image - open file dialog
      char selected_path[512] = {0};
      if (open_image_file_dialog(selected_path, sizeof(selected_path))) {
        // Get document directory from engine (if available) or use current dir
        char doc_dir[256] = ".";
        // For now, use the current working directory
        // In a real implementation, this would come from the document's
        // filepath

        // Copy image to assets directory
        char rel_path[256] = {0};
        if (copy_image_to_assets(selected_path, doc_dir, rel_path,
                                 sizeof(rel_path))) {
          // Create image node with relative path
          Node *img_node = (Node *)calloc(1, sizeof(Node));
          if (img_node && cd->root) {
            img_node->id = cd->next_node_id++;
            img_node->type = NODE_TYPE_IMAGE;
            img_node->x = nx;
            img_node->y = ny;
            img_node->width = 200.0f;
            img_node->height = 150.0f;
            img_node->visible = 1;
            img_node->parent = cd->root;
            img_node->fill_color = 0xFFFFFFFF;
            img_node->stroke_color = 0xCCCCCCFF;
            img_node->stroke_width = 1.0f;
            strncpy(img_node->image_path, rel_path,
                    sizeof(img_node->image_path) - 1);
            // Get filename for display
            const char *fname = strrchr(selected_path, '/');
            fname = fname ? fname + 1 : selected_path;
            strncpy(img_node->text, fname, sizeof(img_node->text) - 1);

            if (!cd->root->first_child) {
              cd->root->first_child = img_node;
            } else {
              Node *last = cd->root->first_child;
              while (last->next_sibling)
                last = last->next_sibling;
              last->next_sibling = img_node;
            }
            new_node = img_node;
          }
        } else {
          // Copy failed, create placeholder
          snprintf(ui->status, sizeof(ui->status), "Failed to copy image");
        }
      }
      // If dialog cancelled, don't create anything
    } else if (cs->pending_create >= 10 && cs->pending_create <= 13) {
      // Forms: 10=rect, 11=ellipse, 12=triangle, 13=line
      NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
      int idx = cs->pending_create - 10;
      if (idx == 3) {
        // Line
        new_node =
            canvas_create_styled_node(cd, styles[idx], nx, ny, 150.0f, 4.0f);
      } else {
        new_node =
            canvas_create_styled_node(cd, styles[idx], nx, ny, def_w, def_h);
      }
    }

    if (new_node) {
      ui->selected_node = new_node;
    }
    cs->pending_create = 0;
  }

  // --- Check for resize handle click on selected node ---
  int on_resize_handle = 0;
  int hit_handle_index = 0;
  int on_property_menu = 0;

  if (ui->selected_node && ui->selected_node->visible) {
    Node *sel = ui->selected_node;
    float sx = bounds.x + (sel->x - cam_x) * zoom;
    float sy = bounds.y + (sel->y - cam_y) * zoom;
    float sw = sel->width * zoom;
    float sh = sel->height * zoom;

    // Check all 4 resize handles (0=SE, 1=SW, 2=NW, 3=NE)
    float hsize = 40.0f; // Interaction size

    // SE
    if (ui->mouse_x >= sx + sw - hsize/2 && ui->mouse_x < sx + sw + hsize/2 &&
        ui->mouse_y >= sy + sh - hsize/2 && ui->mouse_y < sy + sh + hsize/2) {
      on_resize_handle = 1; hit_handle_index = 0;
    }
    // SW
    else if (ui->mouse_x >= sx - hsize/2 && ui->mouse_x < sx + hsize/2 &&
             ui->mouse_y >= sy + sh - hsize/2 && ui->mouse_y < sy + sh + hsize/2) {
      on_resize_handle = 1; hit_handle_index = 1;
    }
    // NW
    else if (ui->mouse_x >= sx - hsize/2 && ui->mouse_x < sx + hsize/2 &&
             ui->mouse_y >= sy - hsize/2 && ui->mouse_y < sy + hsize/2) {
      on_resize_handle = 1; hit_handle_index = 2;
    }
    // NE
    else if (ui->mouse_x >= sx + sw - hsize/2 && ui->mouse_x < sx + sw + hsize/2 &&
             ui->mouse_y >= sy - hsize/2 && ui->mouse_y < sy + hsize/2) {
      on_resize_handle = 1; hit_handle_index = 3;
    }
    
    // Check property menu hit
    float menu_w = 220.0f;
    float menu_h = 45.0f;
    float menu_x = sx;
    float menu_y = sy - menu_h - 10.0f;

    // Keep within canvas bounds (Match logic with render)
    if (menu_x < bounds.x) menu_x = bounds.x;
    if (menu_x + menu_w > bounds.x + bounds.w) menu_x = bounds.x + bounds.w - menu_w;
    if (menu_y < bounds.y) menu_y = sy + sh + 10.0f; 

    if (ui->mouse_x >= menu_x && ui->mouse_x < menu_x + menu_w &&
        ui->mouse_y >= menu_y && ui->mouse_y < menu_y + menu_h) {
      on_property_menu = 1;
    }
  }

  // PRIORITY: Handle resize handles first (even if in toolbar area)
  if (cs->mouse_in_canvas && !cs->is_panning && cs->mouse_just_pressed &&
      on_resize_handle && ui->selected_node) {
    cs->is_resizing = 1;
    cs->resize_handle = hit_handle_index;
    cs->resize_start_w = ui->selected_node->width;
    cs->resize_start_h = ui->selected_node->height;
    cs->resize_start_x = ui->mouse_x;
    cs->resize_start_y = ui->mouse_y;
    cs->resize_start_node_x = ui->selected_node->x;
    cs->resize_start_node_y = ui->selected_node->y;
  }
  // Handle other mouse press in canvas (not toolbar, not forms menu, not resizing)
  else if (cs->mouse_in_canvas && !in_toolbar && !in_forms_menu && !cs->is_panning &&
           cs->mouse_just_pressed && !on_resize_handle) {

    if (on_property_menu) {
      // Let property menu handle the click
    } else {
      // Normal click: Try to select a node, or deselect if empty
      float world_x = cam_x + (ui->mouse_x - bounds.x) / zoom;
      float world_y = cam_y + (ui->mouse_y - bounds.y) / zoom;

      Node *hit = canvas_find_node_at(cd, world_x, world_y);

      // If we were editing and clicked elsewhere, save the text
      if (ui->editing_node && ui->editing_node != hit) {
        strncpy(ui->editing_node->text, ui->edit_buffer, sizeof(ui->editing_node->text) - 1);
        ui->editing_node->text[sizeof(ui->editing_node->text) - 1] = '\0';
        ui->editing_node = NULL;
      }

      if (hit) {
        // Check if clicking on text node to edit (already selected = start editing)
        if (hit->type == NODE_TYPE_TEXT && ui->selected_node == hit && !ui->editing_node) {
          // Start inline text editing
          ui->editing_node = hit;
          strncpy(ui->edit_buffer, hit->text, sizeof(ui->edit_buffer) - 1);
          ui->edit_buffer[sizeof(ui->edit_buffer) - 1] = '\0';
        } else if (ui->editing_node != hit) {
          // Selecting a different node, don't start editing
          ui->selected_node = hit;
          cs->is_dragging_node = 1;
          cs->drag_offset_x = world_x - hit->x;
          cs->drag_offset_y = world_y - hit->y;
        }

        if (!ui->editing_node) {
          ui->selected_node = hit;
          cs->is_dragging_node = 1;
          cs->drag_offset_x = world_x - hit->x;
          cs->drag_offset_y = world_y - hit->y;
        }
      } else {
        // Clicked empty space -> Deselect
        ui->selected_node = NULL;
        ui->editing_node = NULL;
      }
    }
  }

  // Handle resizing
  if (cs->is_resizing && ui->selected_node) {
    float dx = (ui->mouse_x - cs->resize_start_x) / zoom;
    float dy = (ui->mouse_y - cs->resize_start_y) / zoom;
    
    float start_w = cs->resize_start_w;
    float start_h = cs->resize_start_h;
    float new_w = start_w;
    float new_h = start_h;
    float new_x = cs->resize_start_node_x;
    float new_y = cs->resize_start_node_y;

    if (cs->resize_handle == 0) { // SE
        new_w = start_w + dx;
        new_h = start_h + dy;
    } else if (cs->resize_handle == 1) { // SW
        new_w = start_w - dx;
        new_h = start_h + dy;
    } else if (cs->resize_handle == 2) { // NW
        new_w = start_w - dx;
        new_h = start_h - dy;
    } else if (cs->resize_handle == 3) { // NE
        new_w = start_w + dx;
        new_h = start_h - dy;
    }

    // Minimum size
    if (new_w < 20.0f) new_w = 20.0f;
    if (new_h < 20.0f) new_h = 20.0f;
    
    // Adjust Position for Left/Top handles
    if (cs->resize_handle == 1 || cs->resize_handle == 2) { // SW or NW
        new_x = cs->resize_start_node_x + (start_w - new_w);
    }
    if (cs->resize_handle == 2 || cs->resize_handle == 3) { // NW or NE
        new_y = cs->resize_start_node_y + (start_h - new_h);
    }

    ui->selected_node->width = new_w;
    ui->selected_node->height = new_h;
    ui->selected_node->x = new_x;
    ui->selected_node->y = new_y;
  }

  // Stop resizing on mouse release
  if (cs->mouse_just_released) {
    cs->is_resizing = 0;
  }

  // --- INK DRAWING LOGIC ---

  // Pen Tool Active?
  if (cs->tool == CANVAS_TOOL_PEN) { // Active pen tool
    // If clicking in canvas (and not panning/resizing/toolbar)
    int can_draw = cs->mouse_in_canvas && !in_toolbar && !in_forms_menu &&
                   !cs->is_panning && !cs->is_resizing;

    if (can_draw && (ui->mouse_buttons & 1)) { // Left click held
      float world_x = cam_x + (ui->mouse_x - bounds.x) / zoom;
      float world_y = cam_y + (ui->mouse_y - bounds.y) / zoom;

      if (cs->mouse_just_pressed) {
        // Start new stroke
        cs->stroke_block_id = block->id;
        cs->current_stroke = (Stroke *)calloc(1, sizeof(Stroke));
        cs->current_stroke->capacity = 128;
        cs->current_stroke->points =
            (Point *)calloc(cs->current_stroke->capacity, sizeof(Point));
        cs->current_stroke->color = cs->pen_color;
        cs->current_stroke->thickness = cs->pen_thickness;
        cs->current_stroke->type = cs->pen_is_marker ? STROKE_TYPE_MARKER : STROKE_TYPE_PEN;

        // Apply alpha for markers if they are opaque
        if (cs->current_stroke->type == STROKE_TYPE_MARKER && (cs->current_stroke->color & 0xFF) == 0xFF) {
            cs->current_stroke->color = (cs->current_stroke->color & 0xFFFFFF00) | 0x64; // ~40% opacity
        }

        // Add first point
        cs->current_stroke->points[0].x = world_x;
        cs->current_stroke->points[0].y = world_y;
        cs->current_stroke->count = 1;

        // Add to block immediately
        block_canvas_add_stroke(block, cs->current_stroke);
      } else if (cs->current_stroke && cs->stroke_block_id == block->id) {
        // Continue stroke
        if (cs->current_stroke->count >= cs->current_stroke->capacity) {
          cs->current_stroke->capacity *= 2;
          cs->current_stroke->points = (Point *)realloc(
              cs->current_stroke->points, cs->current_stroke->capacity * sizeof(Point));
        }

        // Simple distance check to avoid too many points
        Point last = cs->current_stroke->points[cs->current_stroke->count - 1];
        float dx = world_x - last.x;
        float dy = world_y - last.y;
        if (dx * dx + dy * dy > 4.0f) { // Every 2 units
          cs->current_stroke->points[cs->current_stroke->count].x = world_x;
          cs->current_stroke->points[cs->current_stroke->count].y = world_y;
          cs->current_stroke->count++;
        }
      }
    } else {
      // Only reset if we are the owner of the stroke
      if (cs->current_stroke && cs->stroke_block_id == block->id) {
        cs->current_stroke = NULL;
        cs->stroke_block_id = 0;
      }
    }
  }

// --- RENDER STROKES ---

// Catmull-Rom Interpolation Helper
#define CATMULL_ROM(p0, p1, p2, p3, t)                                         \
  (0.5f * ((2.0f * p1) + (-p0 + p2) * t +                                      \
           (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t +                  \
           (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t))

  // Scissor to canvas bounds
  nk_push_scissor(canvas_buf, bounds);

  if (cd->strokes) {
    for (Stroke *s = cd->strokes; s; s = s->next) {
      if (s->count < 2)
        continue;

      struct nk_color col;
      col.r = (s->color >> 24) & 0xFF;
      col.g = (s->color >> 16) & 0xFF;
      col.b = (s->color >> 8) & 0xFF;
      col.a = s->color & 0xFF;

      // Convert points to screen space and draw
      // Simple polyline for now, upgrade to Catmull-Rom next step for
      // smoothness Allocating screen points array on stack if small, else heap

      if ((s->type == STROKE_TYPE_PEN || s->type == STROKE_TYPE_MARKER) && s->count > 3) {
        // Smooth render
        struct nk_vec2 *screen_pts = (struct nk_vec2 *)malloc(
            s->count * 10 * sizeof(struct nk_vec2)); // ~10 samples per segment
        int pt_idx = 0;

        for (int i = 0; i < s->count - 3; i++) {
          Point p0 = s->points[i];
          Point p1 = s->points[i + 1];
          Point p2 = s->points[i + 2];
          Point p3 = s->points[i + 3];

          for (int t_step = 0; t_step < 5; t_step++) {
            float t = t_step / 5.0f;
            float wx = CATMULL_ROM(p0.x, p1.x, p2.x, p3.x, t);
            float wy = CATMULL_ROM(p0.y, p1.y, p2.y, p3.y, t);
            screen_pts[pt_idx].x = bounds.x + (wx - cam_x) * zoom;
            screen_pts[pt_idx].y = bounds.y + (wy - cam_y) * zoom;
            pt_idx++;
          }
        }
        nk_stroke_polyline(canvas_buf, (float*)screen_pts, pt_idx, s->thickness * zoom, col);
        free(screen_pts);
      } else {
        // Fallback / Fast render
        struct nk_vec2 *screen_pts =
            (struct nk_vec2 *)malloc(s->count * sizeof(struct nk_vec2));
        for (int i = 0; i < s->count; i++) {
          screen_pts[i].x = bounds.x + (s->points[i].x - cam_x) * zoom;
          screen_pts[i].y = bounds.y + (s->points[i].y - cam_y) * zoom;
        }
        nk_stroke_polyline(canvas_buf, (float*)screen_pts, s->count,
                           s->thickness * zoom, col);
        free(screen_pts);
      }
    }
  }

  // Reset scissor to allow drawing outside canvas bounds (e.g. toolbar)
  nk_push_scissor(canvas_buf, nk_rect(0, 0, 100000, 100000));

  // --- Render nodes ---
  if (cd->root) {
    for (Node *node = cd->root->first_child; node; node = node->next_sibling) {
      if (!node->visible)
        continue;

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

      struct nk_color fill = {
          (node->fill_color >> 24) & 0xFF, (node->fill_color >> 16) & 0xFF,
          (node->fill_color >> 8) & 0xFF, node->fill_color & 0xFF};

      // Only draw background if not transparent
      int has_fill = (fill.a > 0);

      struct nk_color stroke = {
          (node->stroke_color >> 24) & 0xFF, (node->stroke_color >> 16) & 0xFF,
          (node->stroke_color >> 8) & 0xFF, node->stroke_color & 0xFF};

      // Draw based on node type
      if (node->type == NODE_TYPE_ELLIPSE) {
        if (has_fill) {
          nk_fill_circle(canvas_buf, nr, fill);
        }
        if (node->stroke_width > 0) {
          nk_stroke_circle(canvas_buf, nr, node->stroke_width * zoom, stroke);
        }
      } else if (node->type == NODE_TYPE_TRIANGLE) {
        // Triangle: top center, bottom left, bottom right
        float x1 = nr.x + nr.w / 2.0f;
        float y1 = nr.y;
        float x2 = nr.x;
        float y2 = nr.y + nr.h;
        float x3 = nr.x + nr.w;
        float y3 = nr.y + nr.h;
        if (has_fill) {
          nk_fill_triangle(canvas_buf, x1, y1, x2, y2, x3, y3, fill);
        }
        if (node->stroke_width > 0) {
          nk_stroke_triangle(canvas_buf, x1, y1, x2, y2, x3, y3,
                             node->stroke_width * zoom, stroke);
        }
      } else if (node->type == NODE_TYPE_IMAGE) {
        // Render Image
        SDL_Texture *tex = get_image_texture(ui, node->image_path);
        if (tex) {
          struct nk_image img = nk_image_ptr(tex);
          nk_draw_image(canvas_buf, nr, &img, nk_rgba(255, 255, 255, 255));
          
          // Draw border if selected
          if (ui->selected_node == node) {
             nk_stroke_rect(canvas_buf, nr, 0.0f, 2.0f, nk_rgb(66, 133, 244));
          }
        } else {
          // Placeholder
          if (has_fill) {
            nk_fill_rect(canvas_buf, nr, 4.0f, fill);
          }
          // Debug info for image loading failure
          if (ui->selected_node == node) {
             snprintf(ui->status, sizeof(ui->status), "Failed to load: %s", node->image_path);
          }
          nk_stroke_rect(canvas_buf, nr, 4.0f, 1.0f, stroke);
          // Draw image icon in center
          if (ui->font_icons) {
            nk_draw_text(
                canvas_buf,
                nk_rect(nr.x + nr.w / 2 - 12, nr.y + nr.h / 2 - 12, 24, 24),
                "\xEE\xB9\xA4", 3, &ui->font_icons->handle, nk_rgba(0, 0, 0, 0),
                nk_rgb(150, 150, 150));
          }
        }
      } else if (node->type == NODE_TYPE_TEXT && !has_fill) {
        // Plain text - inline editing like Freeform
        const char *display_text = node->text;
        int is_editing = (ui->editing_node == node);

        if (is_editing) {
          // Use edit buffer for display while editing
          display_text = ui->edit_buffer;

          // Draw subtle editing background
          nk_fill_rect(canvas_buf, nk_rect(nr.x - 4, nr.y - 2, nr.w + 8, nr.h + 4),
                       4.0f, nk_rgba(66, 133, 244, 20));
          nk_stroke_rect(canvas_buf, nk_rect(nr.x - 4, nr.y - 2, nr.w + 8, nr.h + 4),
                         4.0f, 1.5f, nk_rgb(66, 133, 244));
        }

        // Draw text
        struct nk_user_font *font = ui->font_inter ? &ui->font_inter->handle : ctx->style.font;
        nk_draw_text(canvas_buf, nr, display_text, strlen(display_text),
                     font, nk_rgba(0, 0, 0, 0), nk_rgb(30, 30, 30));

        // Draw blinking cursor when editing
        if (is_editing) {
          // Calculate cursor position (at end of text)
          float text_width = font->width(font->userdata, font->height,
                                         display_text, strlen(display_text));
          float cursor_x = nr.x + text_width + 2;
          float cursor_y = nr.y + 2;
          float cursor_h = nr.h - 4;

          // Blink cursor (simple toggle based on time)
          static int blink_counter = 0;
          blink_counter++;
          if ((blink_counter / 30) % 2 == 0) {
            nk_stroke_line(canvas_buf, cursor_x, cursor_y, cursor_x, cursor_y + cursor_h,
                           2.0f, nk_rgb(66, 133, 244));
          }
        }
      } else {
        if (has_fill) {
          nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, fill);
        }
        if (node->stroke_width > 0) {
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
        }
      }

      // Text content (for non-Text Node types)
      if (node->text[0] && node->type != NODE_TYPE_IMAGE &&
          node->type != NODE_TYPE_TEXT) {
        float text_pad = has_fill ? 8.0f : 0.0f;
        nk_draw_text(canvas_buf,
                     nk_rect(nr.x + text_pad, nr.y + text_pad,
                             nr.w - text_pad * 2, nr.h - text_pad * 2),
                     node->text, strlen(node->text), ctx->style.font,
                     nk_rgba(0, 0, 0, 0), nk_rgb(30, 30, 30));
      }

      // Selection highlight
      if (ui->selected_node == node) {
        nk_stroke_rect(canvas_buf,
                       nk_rect(nr.x - 2, nr.y - 2, nr.w + 4, nr.h + 4), 0, 2.0f,
                       nk_rgb(66, 133, 244));

        // Draw resize handles at corners (SE corner is interactive)
        float hsize = 10.0f; // Visual size (interaction size is larger)
        struct nk_color handle_fill = nk_rgb(255, 255, 255);
        struct nk_color handle_stroke = nk_rgb(66, 133, 244);

        // SE corner (bottom-right)
        float se_x = nr.x + nr.w - hsize / 2;
        float se_y = nr.y + nr.h - hsize / 2;
        nk_fill_rect(canvas_buf, nk_rect(se_x, se_y, hsize, hsize), 2.0f,
                     handle_fill);
        nk_stroke_rect(canvas_buf, nk_rect(se_x, se_y, hsize, hsize), 2.0f,
                       1.5f, handle_stroke);

        // NW corner (top-left)
        float nw_x = nr.x - hsize / 2;
        float nw_y = nr.y - hsize / 2;
        nk_fill_rect(canvas_buf, nk_rect(nw_x, nw_y, hsize, hsize), 2.0f,
                     handle_fill);
        nk_stroke_rect(canvas_buf, nk_rect(nw_x, nw_y, hsize, hsize), 2.0f,
                       1.5f, handle_stroke);

        // NE corner (top-right)
        float ne_x = nr.x + nr.w - hsize / 2;
        float ne_y = nr.y - hsize / 2;
        nk_fill_rect(canvas_buf, nk_rect(ne_x, ne_y, hsize, hsize), 2.0f,
                     handle_fill);
        nk_stroke_rect(canvas_buf, nk_rect(ne_x, ne_y, hsize, hsize), 2.0f,
                       1.5f, handle_stroke);

        // SW corner (bottom-left)
        float sw_x = nr.x - hsize / 2;
        float sw_y = nr.y + nr.h - hsize / 2;
        nk_fill_rect(canvas_buf, nk_rect(sw_x, sw_y, hsize, hsize), 2.0f,
                     handle_fill);
        nk_stroke_rect(canvas_buf, nk_rect(sw_x, sw_y, hsize, hsize), 2.0f,
                       1.5f, handle_stroke);
      }
    }
  }

  // --- Selected Node Property Menu ---
  if (ui->selected_node && ui->selected_node->visible) {
    Node *sel = ui->selected_node;
    float sx = bounds.x + (sel->x - cam_x) * zoom;
    float sy = bounds.y + (sel->y - cam_y) * zoom;
    // float sw = sel->width * zoom; // Unused
    // float sh = sel->height * zoom; // Unused

    // Position menu above the node
    float menu_w = 220.0f;
    float menu_h = 45.0f;
    float menu_x = sx;
    float menu_y = sy - menu_h - 10.0f;

    // Keep within canvas bounds
    if (menu_x < bounds.x) menu_x = bounds.x;
    if (menu_x + menu_w > bounds.x + bounds.w) menu_x = bounds.x + bounds.w - menu_w;
    if (menu_y < bounds.y) menu_y = sy + sel->height * zoom + 10.0f; // Show below if no space above

    // Draw menu background
    nk_fill_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 5.0f, nk_rgba(40, 40, 40, 230));
    nk_stroke_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 5.0f, 1.0f, nk_rgba(100, 100, 100, 255));

    // Colors to pick from
    uint32_t colors[] = {
      0xFFFFFFFF, // White
      0x000000FF, // Black
      0xFF0000FF, // Red
      0x00FF00FF, // Green
      0x0000FFFF, // Blue
      0xFFFF00FF, // Yellow
      0xFF00FFFF, // Magenta
      0x00FFFFFF  // Cyan
    };
    int num_colors = 8;
    float circle_size = 24.0f;
    float padding = 8.0f;
    float start_x = menu_x + 10.0f;
    float cy = menu_y + menu_h / 2.0f;

    for (int i = 0; i < num_colors; i++) {
      float cx = start_x + i * (circle_size + 4.0f);
      struct nk_rect circle_rect = nk_rect(cx, cy - circle_size/2, circle_size, circle_size);

      struct nk_color c = {
        (colors[i] >> 24) & 0xFF,
        (colors[i] >> 16) & 0xFF,
        (colors[i] >> 8) & 0xFF,
        colors[i] & 0xFF
      };

      // Draw color circle
      nk_fill_circle(canvas_buf, circle_rect, c);
      
      // Selection indicator
      if (sel->fill_color == colors[i]) {
         nk_stroke_circle(canvas_buf, circle_rect, 2.0f, nk_rgb(255, 255, 255));
      } else {
         nk_stroke_circle(canvas_buf, circle_rect, 1.0f, nk_rgba(255, 255, 255, 100));
      }

      // Interaction
      if (cs->mouse_in_canvas && !in_toolbar && !in_forms_menu &&
          ui->mouse_x >= circle_rect.x && ui->mouse_x < circle_rect.x + circle_rect.w &&
          ui->mouse_y >= circle_rect.y && ui->mouse_y < circle_rect.y + circle_rect.h &&
          cs->mouse_just_pressed) {
            
          // Set color (Fill and Stroke for now, or just Fill?)
          // User asked "changer sa couleur".
          sel->fill_color = colors[i];
          // Also set stroke to black or white for contrast if needed, or keep as is.
          // Let's keep stroke as is, or maybe default to black stroke if it was transparent?
          // For now just change fill.
      }
    }
  }

  // --- Floating toolbar ---
  canvas_render_toolbar(ui, ctx, bounds, block);

  // --- Status indicators ---
  if (cs->space_held && ui->font_icons) {
    nk_draw_text(canvas_buf, nk_rect(bounds.x + 10, bounds.y + 10, 24, 24),
                 ICON_HAND, strlen(ICON_HAND), &ui->font_icons->handle,
                 nk_rgba(0, 0, 0, 0), nk_rgb(100, 100, 100));
  }

  // Zoom level
  char zoom_label[32];
  snprintf(zoom_label, sizeof(zoom_label), "%.0f%%", zoom * 100.0f);
  nk_draw_text(canvas_buf,
               nk_rect(bounds.x + bounds.w - 60, bounds.y + 10, 50, 20),
               zoom_label, strlen(zoom_label), ctx->style.font,
               nk_rgba(0, 0, 0, 0), nk_rgb(100, 100, 100));

  // Status: show selected node info if any
  if (ui->selected_node) {
    char sel_info[64];
    snprintf(sel_info, sizeof(sel_info), "%.0fx%.0f", ui->selected_node->width,
             ui->selected_node->height);
    nk_draw_text(canvas_buf,
                 nk_rect(bounds.x + 10, bounds.y + bounds.h - 30, 100, 20),
                 sel_info, strlen(sel_info), ctx->style.font,
                 nk_rgba(0, 0, 0, 0), nk_rgb(100, 100, 100));
  }

  // Debug: show node count
  int node_count = 0;
  if (cd->root) {
    for (Node *n = cd->root->first_child; n; n = n->next_sibling) {
      if (n->visible)
        node_count++;
    }
  }
  char debug_label[64];
  snprintf(debug_label, sizeof(debug_label), "Nodes: %d", node_count);
  nk_draw_text(canvas_buf,
               nk_rect(bounds.x + bounds.w - 80, bounds.y + 30, 70, 20),
               debug_label, strlen(debug_label), ctx->style.font,
               nk_rgba(0, 0, 0, 0), nk_rgb(100, 100, 100));

  // Sync back to canvas data
  cd->view_x = cs->cam_x;
  cd->view_y = cs->cam_y;
  cd->zoom = cs->zoom;

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

  // Reset per-frame canvas input state (after all processing is done)
  ui->canvas_state.mouse_just_pressed = 0;
  ui->canvas_state.mouse_just_released = 0;

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
