// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear implementation (must be before nuklear.h include)
#define NK_IMPLEMENTATION
#include "ui_host.h"
#include "../document/aslan_table.h"
#include "../scripting/lua_host.h"
#include "mac_image.h"
#include <SDL.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

// Forward declarations for internal node search functions
static Node *canvas_find_node_at(CanvasData *cd, float world_x, float world_y);
static Node *design_find_node_at(DesignData *dd, float x, float y);

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

// Opens a native file dialog to select a video file
static int open_video_file_dialog(char *out_path, size_t path_size) {
  const char *script =
      "osascript -e 'set theFile to choose file of type {\"public.movie\"} "
      "with prompt \"Select a video\"' -e 'POSIX path of theFile' 2>/dev/null";

  FILE *fp = popen(script, "r");
  if (!fp)
    return 0;

  char result[512] = {0};
  if (fgets(result, sizeof(result), fp) != NULL) {
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

static const float UI_SIDE_PADDING = 40.0f;

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------
static void node_convert_to_path(Node *node);

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
  ui->sidebar_mode = 0; // Standard sidebar (Properties/Layers)

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
    fprintf(log, "UI Init started (Aslan v0.1.5)\n");
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

  // Load variable sizes for Text tool
  float sizes[] = {12.0f, 14.0f, 16.0f, 20.0f, 24.0f,
                   32.0f, 48.0f, 64.0f, 96.0f};
  for (int i = 0; i < 9; i++) {
    ui->font_inter_sizes[i] =
        nk_font_atlas_add_from_file(ui->atlas, inter_path, sizes[i], NULL);
    if (!ui->font_inter_sizes[i])
      ui->font_inter_sizes[i] =
          nk_font_atlas_add_default(ui->atlas, sizes[i], NULL);
  }

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

  ui->canvas_state.selected_color = 0x000000FF; // Default black
  ui->canvas_state.pen_thickness = 2.0f;
  ui->canvas_state.pen_is_marker = 0;
  ui->design_state.selected_color = 0x000000FF;
  ui->design_state.pen_thickness = 2.0f;
  ui->design_state.pen_is_marker = 0;

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

void ui_host_handle_file_drop(UIHost *ui, const char *file_path, int mx,
                              int my) {
  if (!ui || !file_path)
    return;

  printf("[DEBUG] File dropped: %s at (%d, %d)\n", file_path, mx, my);

  // Get document directory (use current working directory as fallback)
  char doc_dir[512] = ".";
  if (ui->engine && ui->engine->doc && ui->engine->doc->filepath[0]) {
    // Extract directory from document path
    strncpy(doc_dir, ui->engine->doc->filepath, sizeof(doc_dir) - 1);
    char *last_slash = strrchr(doc_dir, '/');
    if (last_slash) {
      *last_slash = '\0';
    } else {
      strcpy(doc_dir, ".");
    }
  }
  printf("[DEBUG] Document directory: %s\n", doc_dir);

  // 1. Try Canvas blocks
  CanvasInteraction *cs = &ui->canvas_state;
  printf("[DEBUG] Canvas bounds: x=%.1f y=%.1f w=%.1f h=%.1f\n", cs->bounds_x,
         cs->bounds_y, cs->bounds_w, cs->bounds_h);

  if (mx >= cs->bounds_x && mx < cs->bounds_x + cs->bounds_w &&
      my >= cs->bounds_y && my < cs->bounds_y + cs->bounds_h) {
    printf("[DEBUG] Mouse is in canvas bounds\n");
    if (cs->active_block && cs->active_block->type == BLOCK_CANVAS) {
      printf("[DEBUG] Active canvas block found\n");
      CanvasData *cd = &cs->active_block->data.canvas;
      float wx = cs->cam_x + (mx - cs->bounds_x) / cs->zoom;
      float wy = cs->cam_y + (my - cs->bounds_y) / cs->zoom;
      printf(
          "[DEBUG] World coords: wx=%.1f wy=%.1f (cam: %.1f,%.1f zoom: %.2f)\n",
          wx, wy, cs->cam_x, cs->cam_y, cs->zoom);
      Node *hit = canvas_find_node_at(cd, wx, wy);
      if (hit) {
        printf("[DEBUG] Hit node found, is_container=%d\n", hit->is_container);
        if (hit->is_container) {
          char rel[256];
          if (copy_image_to_assets(file_path, doc_dir, rel, sizeof(rel))) {
            strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
            printf("[DEBUG] Set container image: %s\n", hit->image_path);
          } else {
            printf("[DEBUG] Failed to copy image to assets\n");
          }
          return;
        }
      } else {
        printf("[DEBUG] No node hit at world coords\n");
      }
    } else {
      printf("[DEBUG] No active canvas block\n");
    }
  } else {
    printf("[DEBUG] Mouse NOT in canvas bounds\n");
  }

  // 2. Try Design blocks
  DesignInteraction *ds = &ui->design_state;
  printf("[DEBUG] Design bounds: x=%.1f y=%.1f w=%.1f h=%.1f\n", ds->bounds_x,
         ds->bounds_y, ds->bounds_w, ds->bounds_h);

  if (mx >= ds->bounds_x && mx < ds->bounds_x + ds->bounds_w &&
      my >= ds->bounds_y && my < ds->bounds_y + ds->bounds_h) {
    printf("[DEBUG] Mouse is in design bounds\n");
    if (ds->active_block && ds->active_block->type == BLOCK_DESIGN) {
      printf("[DEBUG] Active design block found\n");
      DesignData *dd = &ds->active_block->data.design;
      float scale = ds->scale;
      float design_x = (mx - ds->bounds_x) / (scale > 0 ? scale : 1.0f);
      float design_y = (my - ds->bounds_y) / (scale > 0 ? scale : 1.0f);
      printf("[DEBUG] Design coords: x=%.1f y=%.1f (scale: %.2f)\n", design_x,
             design_y, scale);
      Node *hit = design_find_node_at(dd, design_x, design_y);
      if (hit) {
        printf("[DEBUG] Hit node found, is_container=%d\n", hit->is_container);
        if (hit->is_container) {
          char rel[256];
          if (copy_image_to_assets(file_path, doc_dir, rel, sizeof(rel))) {
            strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
            printf("[DEBUG] Set container image: %s\n", hit->image_path);
          } else {
            printf("[DEBUG] Failed to copy image to assets\n");
          }
          return;
        }
      } else {
        printf("[DEBUG] No node hit at design coords\n");
      }
    } else {
      printf("[DEBUG] No active design block\n");
    }
  } else {
    printf("[DEBUG] Mouse NOT in design bounds\n");
  }

  printf("[DEBUG] File drop not handled\n");
}

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
      ui->design_state.mouse_just_pressed = 1; // Also track for design

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
      ui->design_state.mouse_just_released = 1; // Also track for design

      // Stop panning
      cs->is_panning = 0;

      // Stop dragging node
      cs->is_dragging_node = 0;

      // Stop resizing
      cs->is_resizing = 0;

      // Stop design dragging/resizing
      ui->design_state.is_dragging_node = 0;
      ui->design_state.is_resizing = 0;
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
    // Only clear space_held if pan wasn't activated from menu
    if (!ui->canvas_state.pan_from_menu) {
      ui->canvas_state.space_held = 0;
    }
    ui->canvas_state.is_panning = 0;
  }

  // Inline text editing keyboard handling (Freeform-style)
  if (ui->editing_node) {
    if (evt->type == SDL_KEYDOWN) {
      if (evt->key.keysym.sym == SDLK_RETURN ||
          evt->key.keysym.sym == SDLK_KP_ENTER) {
        // Confirm edit and exit editing mode
        strncpy(ui->editing_node->text, ui->edit_buffer,
                sizeof(ui->editing_node->text) - 1);
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
      (void)cd;
      // Remove node from tree (simplified - just hide it)
      ui->selected_node->visible = 0;
      ui->selected_node = NULL;
    }
  }

  // Handle canvas zoom with mouse wheel
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

    Page *page = doc_page_get_active(engine->doc);
    uint32_t page_id = page ? page->id : 0;

    // Layout: [FLEX spacer] [7 Icons Cluster] [FLEX spacer]
    nk_layout_row_template_begin(ctx, btn_h);
    nk_layout_row_template_push_variable(ctx, 1); // Left centering spacer
    for (int i = 0; i < 7; i++) {
      nk_layout_row_template_push_static(ctx, btn_w); // 7 icons
    }
    nk_layout_row_template_push_variable(ctx, 1); // Right centering spacer
    nk_layout_row_template_end(ctx);

    // Left FLEX spacer
    nk_spacing(ctx, 1);

    // Home button (icon) - Now first in centered cluster
    if (ui_icon_button(ui, ICON_HOME)) {
      ui->screen = 0;
    }

    // Text block
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

    // Lua script (icon) - Now last
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

    // Right FLEX spacer
    nk_spacing(ctx, 1);
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

    // Use padding aligned with document content
    nk_layout_row_template_begin(ctx, bar_h - 8);
    nk_layout_row_template_push_static(ctx, UI_SIDE_PADDING);
    for (int i = 0; i < col_count; i++) {
      nk_layout_row_template_push_static(ctx, tab_w);
    }
    nk_layout_row_template_push_variable(ctx, 1); // Right spacer
    nk_layout_row_template_end(ctx);

    // margin spacer
    nk_spacing(ctx, 1);

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

    // Home removed from bottom per request
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

  const float line_h = 24.0f;

  // Calculate number of lines for multiline support
  int line_count = 1;
  if (block->text) {
    for (const char *p = block->text; *p; p++) {
      if (*p == '\n')
        line_count++;
    }
  }

  // Dynamic height based on content (minimum 1 line)
  float edit_height = line_h * (line_count > 0 ? line_count : 1);
  if (edit_height < line_h)
    edit_height = line_h;

  nk_layout_row_dynamic(ctx, edit_height, 1);

  // Get bounds before edit for placeholder
  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Highlight background
  if (block->highlight_color > 0) {
    struct nk_color hl = block->highlight_color == 1   ? COLOR_HIGHLIGHT_YELLOW
                         : block->highlight_color == 2 ? COLOR_HIGHLIGHT_BLUE
                                                       : COLOR_HIGHLIGHT_GREEN;
    nk_fill_rect(canvas, bounds, 0, hl);
  }

  char buf[4096];
  strncpy(buf, block->text ? block->text : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  // NK_EDIT_BOX for multiline (Enter = newline), NK_EDIT_SELECTABLE for text
  // selection
  nk_flags flags = NK_EDIT_BOX | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD;
  nk_flags state = nk_edit_string_zero_terminated(
      ctx, flags, buf, (int)sizeof(buf) - 1, nk_filter_default);

  // Draw placeholder text if buffer is empty and not active (check buf AFTER
  // edit)
  if (buf[0] == '\0' && !(state & NK_EDIT_ACTIVE)) {
    struct nk_color placeholder_color =
        ui->dark_mode ? nk_rgb(100, 100, 110) : nk_rgb(180, 180, 185);
    const char *placeholder = "Write something...";
    float text_x = bounds.x + 8;
    float text_y = bounds.y + (bounds.h - line_h) / 2 + 4;
    nk_draw_text(canvas, nk_rect(text_x, text_y, bounds.w - 16, line_h),
                 placeholder, strlen(placeholder), &ui->font_mono->handle,
                 nk_rgba(0, 0, 0, 0), placeholder_color);
  }

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

// Helper: Get icon for column type
static const char *table_col_type_icon(AslanColumnType type) {
  switch (type) {
  case ASLAN_COL_TEXT:
    return ICON_COL_TEXT;
  case ASLAN_COL_NUMBER:
    return ICON_COL_NUMBER;
  case ASLAN_COL_CHECKBOX:
    return ICON_COL_CHECKBOX;
  case ASLAN_COL_SELECT:
    return ICON_COL_SELECT;
  case ASLAN_COL_DATE:
    return ICON_COL_DATE;
  case ASLAN_COL_RELATION:
    return ICON_COL_RELATION;
  case ASLAN_COL_FORMULA:
    return ICON_COL_FORMULA;
  default:
    return ICON_COL_TEXT;
  }
}

// Helper: Get column type name
static const char *table_col_type_name(AslanColumnType type) {
  switch (type) {
  case ASLAN_COL_TEXT:
    return "Text";
  case ASLAN_COL_NUMBER:
    return "Number";
  case ASLAN_COL_CHECKBOX:
    return "Checkbox";
  case ASLAN_COL_SELECT:
    return "Select";
  case ASLAN_COL_DATE:
    return "Date";
  case ASLAN_COL_RELATION:
    return "Relation";
  case ASLAN_COL_FORMULA:
    return "Formula";
  default:
    return "Text";
  }
}

void ui_render_block_table(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width) {
  (void)engine;
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  AslanTable *tbl = td->columnar;

  // Constants
  const float row_h = 28.0f;
  const float header_h = 32.0f;
  const float footer_h = 26.0f;
  const float min_col_w = 80.0f;
  const float max_col_w = 250.0f;
  const float row_num_w = 40.0f;
  const float toolbar_h = 32.0f;

  // Colors
  struct nk_color header_bg =
      ui->dark_mode ? nk_rgb(15, 15, 18) : nk_rgb(180, 180, 185);
  struct nk_color row_alt =
      ui->dark_mode ? nk_rgb(35, 37, 42) : nk_rgb(252, 252, 253);
  struct nk_color row_normal =
      ui->dark_mode ? nk_rgb(30, 32, 36) : nk_rgb(255, 255, 255);
  struct nk_color border_color =
      ui->dark_mode ? nk_rgb(60, 62, 68) : nk_rgb(220, 220, 225);
  struct nk_color footer_bg =
      ui->dark_mode ? nk_rgb(40, 42, 48) : nk_rgb(248, 248, 250);

  // Expand/collapse toggle
  ui_render_expand_toggle(ui, ctx, block);

  if (block->is_collapsed)
    return;

  // =========================================================================
  // KEYBOARD NAVIGATION (Phase 8)
  // =========================================================================
  if (ui->table_state.active_block_id == block->id &&
      !ui->table_state.is_editing_cell) {
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_UP)) {
      ui->table_state.active_row_idx = (ui->table_state.active_row_idx > 0)
                                           ? ui->table_state.active_row_idx - 1
                                           : 0;
    }
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_DOWN)) {
      // Bounds check should be against view_count, but loose check is okay for
      // now
      ui->table_state.active_row_idx++;
    }
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_LEFT)) {
      ui->table_state.active_col_idx = (ui->table_state.active_col_idx > 0)
                                           ? ui->table_state.active_col_idx - 1
                                           : 0;
    }
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_RIGHT)) {
      ui->table_state.active_col_idx =
          (ui->table_state.active_col_idx < (int)tbl->column_count - 1)
              ? ui->table_state.active_col_idx + 1
              : (int)tbl->column_count - 1;
    }
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
      ui->table_state.is_editing_cell = 1;
    }
  }
  if (nk_input_is_key_pressed(&ctx->input, NK_KEY_TEXT_RESET_MODE)) {
    ui->table_state.is_editing_cell = 0;
  }

  // =========================================================================
  // TOOLBAR: Add Column, Add Row
  // =========================================================================
  nk_layout_row_dynamic(ctx, toolbar_h, 1);
  if (nk_group_begin(ctx, "table_toolbar", NK_WINDOW_NO_SCROLLBAR)) {
    nk_layout_row_template_begin(ctx, 24);
    nk_layout_row_template_push_static(ctx, 100);
    nk_layout_row_template_push_static(ctx, 100);
    nk_layout_row_template_push_variable(ctx, 10);
    nk_layout_row_template_end(ctx);

    // Add Row button
    if (nk_button_label(ctx, "+ Row")) {
      if (tbl) {
        aslan_table_add_row(tbl);
      } else {
        // Legacy: add row at end
        block_table_insert_row(block, td->rows);
      }
    }

    // Add Column button (opens type menu)
    if (nk_button_label(ctx, "+ Column")) {
      struct nk_rect b = nk_widget_bounds(ctx);
      ui->table_state.active_block_id = block->id;
      ui->table_state.show_col_menu = !ui->table_state.show_col_menu;
      ui->table_state.show_footer_menu = 0;
      ui->table_state.menu_pos_x = b.x;
      ui->table_state.menu_pos_y = b.y;
    }

    nk_spacing(ctx, 1);

    nk_group_end(ctx);
  }

  // =========================================================================
  // COLUMNAR TABLE RENDERING
  // =========================================================================
  if (tbl && tbl->column_count > 0) {
    size_t col_count = tbl->column_count;
    size_t view_count = aslan_table_view_count(tbl);

    // Calculate column widths
    float *col_widths = (float *)alloca(col_count * sizeof(float));
    float total_w = row_num_w;
    for (size_t c = 0; c < col_count; c++) {
      float w = (float)tbl->columns[c].width_px;
      if (w < min_col_w)
        w = min_col_w;
      if (w > max_col_w)
        w = max_col_w;
      col_widths[c] = w;
      total_w += w;
    }

    // Scale if needed to fit width
    if (total_w > width - 20) {
      float scale = (width - 20 - row_num_w) / (total_w - row_num_w);
      for (size_t c = 0; c < col_count; c++) {
        col_widths[c] *= scale;
        if (col_widths[c] < 60)
          col_widths[c] = 60;
      }
    }

    // -----------------------------------------------------------------------
    // HEADER ROW
    // -----------------------------------------------------------------------
    nk_layout_row_begin(ctx, NK_STATIC, header_h, (int)col_count + 1);

    // Row number header
    nk_layout_row_push(ctx, row_num_w);
    struct nk_rect hdr_bounds = nk_widget_bounds(ctx);
    nk_fill_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, header_bg);
    nk_label(ctx, "#", NK_TEXT_CENTERED);

    // Column headers
    for (size_t c = 0; c < col_count; c++) {
      nk_layout_row_push(ctx, col_widths[c]);
      hdr_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, header_bg);
      nk_stroke_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, 1, border_color);

      // Hidden button for settings/sorting
      if (nk_button_label(ctx, "")) {
        ui->table_state.active_block_id = block->id;
        ui->table_state.active_col_idx = (int)c;
        ui->table_state.show_col_settings = !ui->table_state.show_col_settings;
        ui->table_state.show_col_menu = 0;
        ui->table_state.show_footer_menu = 0;
        ui->table_state.menu_pos_x = hdr_bounds.x;
        ui->table_state.menu_pos_y = hdr_bounds.y;
        strncpy(ui->table_state.rename_buffer, tbl->columns[c].name, 63);
        ui->table_state.rename_buffer[63] = '\0';
      }

      // Notion Style Header: Anthracite gray text, slightly transparent icons
      struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
      const char *icon = table_col_type_icon(tbl->columns[c].type);
      struct nk_color header_text_col =
          ui->dark_mode ? nk_rgb(220, 220, 225) : nk_rgb(55, 53, 47);
      struct nk_color icon_col = ui->dark_mode ? nk_rgba(220, 220, 225, 128)
                                               : nk_rgba(55, 53, 47, 128);

      // Draw Icon and Text manually with font switching
      if (ui->font_icons) {
        nk_style_set_font(ctx, &ui->font_icons->handle);
        nk_draw_text(canvas,
                     nk_rect(hdr_bounds.x + 8, hdr_bounds.y + 6, 20, 20), icon,
                     strlen(icon), &ui->font_icons->handle, nk_rgba(0, 0, 0, 0),
                     icon_col);
        nk_style_set_font(ctx, &ui->font_inter->handle);
      }

      nk_draw_text(
          canvas,
          nk_rect(hdr_bounds.x + 30, hdr_bounds.y + 6, hdr_bounds.w - 34, 20),
          tbl->columns[c].name, strlen(tbl->columns[c].name),
          &ui->font_inter->handle, nk_rgba(0, 0, 0, 0), header_text_col);

      // Sort indicator
      if (tbl->sort_column == (int)c) {
        const char *sort_icon =
            tbl->sort_ascending ? ICON_SORT_ASC : ICON_SORT_DESC;
        if (ui->font_icons) {
          nk_style_set_font(ctx, &ui->font_icons->handle);
          nk_draw_text(canvas,
                       nk_rect(hdr_bounds.x + hdr_bounds.w - 20,
                               hdr_bounds.y + 6, 16, 20),
                       sort_icon, strlen(sort_icon), &ui->font_icons->handle,
                       nk_rgba(0, 0, 0, 0), COLOR_ACCENT);
          nk_style_set_font(ctx, &ui->font_inter->handle);
        }
      }
    }
    nk_layout_row_end(ctx);

    // -----------------------------------------------------------------------
    // DATA ROWS
    // -----------------------------------------------------------------------
    struct nk_color notion_row_bg =
        ui->dark_mode ? nk_rgb(30, 32, 36) : nk_rgb(255, 255, 255);
    struct nk_color notion_border =
        ui->dark_mode ? nk_rgb(60, 62, 68) : nk_rgb(235, 236, 237);
    struct nk_color notion_text =
        ui->dark_mode ? nk_rgb(220, 220, 225) : nk_rgb(55, 53, 47);
    struct nk_color vert_sep =
        ui->dark_mode ? nk_rgb(45, 47, 52) : nk_rgb(245, 246, 247);

    for (size_t v = 0; v < view_count && v < 100; v++) { // Limit visible rows
      uint64_t row_id = aslan_table_view_row_id(tbl, (int)v);
      if (row_id == 0)
        continue;

      // Notion style: Pure white background for everything
      struct nk_color row_bg = notion_row_bg;

      // Data Row Layout Template
      nk_layout_row_template_begin(ctx, row_h);
      nk_layout_row_template_push_static(ctx, row_num_w);
      for (size_t c = 0; c < col_count; c++) {
        nk_layout_row_template_push_static(ctx, col_widths[c]);
      }
      nk_layout_row_template_end(ctx);

      // Row number
      struct nk_rect row_num_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(nk_window_get_canvas(ctx), row_num_bounds, 0, row_bg);
      // Bottom border only: Notion style
      nk_stroke_line(nk_window_get_canvas(ctx), row_num_bounds.x,
                     row_num_bounds.y + row_num_bounds.h,
                     row_num_bounds.x + row_num_bounds.w,
                     row_num_bounds.y + row_num_bounds.h, 1.0f, notion_border);

      char row_num[16];
      snprintf(row_num, sizeof(row_num), "%zu", v + 1);
      nk_label(ctx, row_num, NK_TEXT_CENTERED);

      // Cells
      for (size_t c = 0; c < col_count; c++) {
        char cell_id[64];
        snprintf(cell_id, sizeof(cell_id), "cell_%llu_%zu",
                 (unsigned long long)row_id, c);

        // Scope each cell with a group to avoid widget ID conflicts
        if (nk_group_begin(ctx, cell_id, NK_WINDOW_NO_SCROLLBAR)) {
          nk_layout_row_dynamic(ctx, row_h, 1);
          struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

          // Notion Aesthetic: Only horizontal bottom border
          nk_stroke_line(canvas, 0, row_h - 1, col_widths[c], row_h - 1, 1.0f,
                         notion_border);

          AslanColumnType type = tbl->columns[c].type;
          int is_active = (ui->table_state.active_row_idx == (int)v &&
                           ui->table_state.active_col_idx == (int)c);
          // Interaction: Updates active cell based on edit widget focus

          // Interaction: Updates active cell based on edit widget focus
          // We rely on nk_edit_string returning NK_EDIT_ACTIVE/ACTIVATED

          // Active Cell Highlight (Notion-blue tint)
          if (is_active) {
            nk_fill_rect(canvas, nk_layout_space_bounds(ctx), 0,
                         nk_rgba(35, 131, 226, 20)); // Subtle blue
          }

          // Notion Aesthetic: Horizontal bottom border + faint vertical lines
          nk_stroke_line(canvas, 0, row_h - 1, col_widths[c], row_h - 1, 1.0f,
                         notion_border);
          nk_stroke_line(canvas, col_widths[c] - 1, 0, col_widths[c] - 1, row_h,
                         1.0f, vert_sep); // Faint vertical separator

          // Global padding for cell content: 10px left
          nk_style_push_vec2(ctx, &ctx->style.text.padding, nk_vec2(10, 0));

          switch (type) {
          case ASLAN_COL_TEXT: {
            char buf[256];
            const char *val = aslan_table_get_text(tbl, (int)c, row_id);
            strncpy(buf, val ? val : "", sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';

            nk_flags res = nk_edit_string_zero_terminated(
                ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT, buf, sizeof(buf) - 1,
                nk_filter_default);

            if (res & NK_EDIT_ACTIVATED) {
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.is_editing_cell = 1;
            }

            if (strcmp(buf, val ? val : "") != 0) {
              aslan_table_set_text(tbl, (int)c, row_id, buf);
            }
            break;
          }

          case ASLAN_COL_NUMBER: {
            char buf[64];
            double val = aslan_table_get_number(tbl, (int)c, row_id);
            if (val == (int64_t)val) {
              snprintf(buf, sizeof(buf), "%lld", (long long)val);
            } else {
              snprintf(buf, sizeof(buf), "%.2f", val);
            }

            nk_flags res = nk_edit_string_zero_terminated(
                ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT, buf, sizeof(buf) - 1,
                nk_filter_decimal);

            if (res & NK_EDIT_ACTIVATED) {
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.is_editing_cell = 1;
            }

            double new_val = strtod(buf, NULL);
            if (new_val != val) {
              aslan_table_set_number(tbl, (int)c, row_id, new_val);
            }
            break;
          }

          case ASLAN_COL_CHECKBOX: {
            // Checkbox logic (Notion style: simple square)
            bool is_checked = aslan_table_get_checkbox(tbl, (int)c, row_id);
            struct nk_rect b = nk_widget_bounds(ctx);
            float size = 18.0f;
            struct nk_rect box =
                nk_rect(b.x + 10, b.y + (b.h - size) / 2, size, size);

            if (nk_button_label(ctx, "")) {
              aslan_table_set_checkbox(tbl, (int)c, row_id, !is_checked);
            }

            struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
            if (is_checked) {
              nk_fill_rect(canvas, box, 2.0f, nk_rgb(35, 131, 226)); // Blue
              if (ui->font_icons) {
                nk_style_set_font(ctx, &ui->font_icons->handle);
                nk_draw_text(canvas,
                             nk_rect(box.x + 2, box.y + 2, size - 4, size - 4),
                             ICON_COL_CHECKBOX, strlen(ICON_COL_CHECKBOX),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0),
                             nk_rgb(255, 255, 255));
                nk_style_set_font(ctx, &ui->font_inter->handle);
              }
            } else {
              nk_stroke_rect(canvas, box, 2.0f, 1.5f, border_color);
            }
            break;
          }

          case ASLAN_COL_SELECT: {
            AslanColumn *col = &tbl->columns[c];
            int sel = aslan_table_get_select(tbl, (int)c, row_id);
            char options[512] = "";
            char *p = options;
            for (int i = 0; i < col->option_count && i < 10; i++) {
              if (i > 0)
                *p++ = '\0';
              strncpy(p, col->options[i].label, 60);
              p += strlen(p);
            }
            if (col->option_count > 0)
              *p++ = '\0';

            if (col->option_count > 0) {
              int new_sel = sel < 0 ? 0 : sel;
              new_sel =
                  nk_combo_string(ctx, options, new_sel, col->option_count,
                                  (int)row_h, nk_vec2(col_widths[c], 150));
              if (new_sel != sel) {
                aslan_table_set_select(tbl, (int)c, row_id, new_sel);
              }
            } else {
              nk_label(ctx, "(no options)", NK_TEXT_LEFT);
            }
            break;
          }

          case ASLAN_COL_DATE: {
            char buf[32];
            int64_t ts = aslan_table_get_date(tbl, (int)c, row_id);
            if (ts > 0) {
              time_t t = (time_t)(ts / 1000);
              struct tm *tm = localtime(&t);
              strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
            } else {
              buf[0] = '\0';
            }

            if (buf[0] == '\0') {
              // Placeholder
              struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
              nk_draw_text(canvas, nk_rect(14, 4, col_widths[c] - 8, row_h - 8),
                           "0000-00-00", 10, &ui->font_inter->handle,
                           nk_rgba(0, 0, 0, 0), nk_rgb(200, 200, 200));
            }

            nk_flags res = nk_edit_string_zero_terminated(
                ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT, buf, sizeof(buf) - 1,
                nk_filter_ascii);

            if (res & NK_EDIT_ACTIVATED) {
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.is_editing_cell = 1;
            }

            if (res & NK_EDIT_COMMITED || (buf[0] && strlen(buf) == 10)) {
              struct tm tm = {0};
              if (sscanf(buf, "%d-%d-%d", &tm.tm_year, &tm.tm_mon,
                         &tm.tm_mday) == 3) {
                tm.tm_year -= 1900;
                tm.tm_mon -= 1;
                time_t new_t = mktime(&tm);
                if (new_t != -1) {
                  int64_t new_ts = (int64_t)new_t * 1000;
                  if (new_ts != ts)
                    aslan_table_set_date(tbl, (int)c, row_id, new_ts);
                }
              }
            }
            break;
          }

          case ASLAN_COL_RELATION: {
            char buf[64];
            aslan_table_get_cell_display(tbl, (int)c, row_id, buf, sizeof(buf));
            if (ui->font_icons) {
              nk_style_set_font(ctx, &ui->font_icons->handle);
              nk_draw_text(nk_window_get_canvas(ctx), nk_rect(10, 6, 20, 20),
                           ICON_COL_RELATION, strlen(ICON_COL_RELATION),
                           &ui->font_icons->handle, nk_rgba(0, 0, 0, 0),
                           notion_text);
              nk_style_set_font(ctx, &ui->font_inter->handle);
              nk_draw_text(nk_window_get_canvas(ctx),
                           nk_rect(30, 6, col_widths[c] - 34, 20),
                           buf[0] ? buf : "-", buf[0] ? strlen(buf) : 1,
                           &ui->font_inter->handle, nk_rgba(0, 0, 0, 0),
                           notion_text);
            } else {
              nk_label(ctx, buf[0] ? buf : "-", NK_TEXT_LEFT);
            }
            break;
          }

          case ASLAN_COL_FORMULA: {
            char buf[64];
            aslan_table_get_cell_display(tbl, (int)c, row_id, buf, sizeof(buf));
            nk_label(ctx, buf, NK_TEXT_LEFT);
            break;
          }

          default: {
            char buf[256];
            aslan_table_get_cell_display(tbl, (int)c, row_id, buf, sizeof(buf));
            nk_label(ctx, buf, NK_TEXT_LEFT);
            break;
          }
          }
          nk_style_pop_vec2(ctx); // Pop cell content padding
          nk_group_end(ctx);
        } else {
          nk_spacing(ctx, 1);
        }
      }
    }

    // -----------------------------------------------------------------------
    // FOOTER ROW (Aggregations)
    // -----------------------------------------------------------------------
    nk_layout_row_begin(ctx, NK_STATIC, footer_h, (int)col_count + 1);

    // Footer row number cell
    nk_layout_row_push(ctx, row_num_w);
    struct nk_rect ftr_bounds = nk_widget_bounds(ctx);
    nk_fill_rect(nk_window_get_canvas(ctx), ftr_bounds, 0, footer_bg);
    nk_label(ctx, "", NK_TEXT_CENTERED);

    // Footer cells with aggregations
    for (size_t c = 0; c < col_count; c++) {
      nk_layout_row_push(ctx, col_widths[c]);
      ftr_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(nk_window_get_canvas(ctx), ftr_bounds, 0, footer_bg);
      nk_stroke_rect(nk_window_get_canvas(ctx), ftr_bounds, 0, 0.5f,
                     border_color);

      char footer_text[64];
      aslan_table_get_footer_display(tbl, (int)c, footer_text,
                                     sizeof(footer_text));

      if (footer_text[0]) {
        // Show aggregation value
        if (nk_button_label(ctx, footer_text)) {
          struct nk_rect b = nk_widget_bounds(ctx);
          ui->table_state.active_block_id = block->id;
          ui->table_state.active_col_idx = (int)c;
          ui->table_state.show_footer_menu = !ui->table_state.show_footer_menu;
          ui->table_state.show_col_menu = 0;
          ui->table_state.menu_pos_x = b.x;
          ui->table_state.menu_pos_y = b.y;
        }
      } else {
        // Click to set aggregation
        if (nk_button_label(ctx, "...")) {
          struct nk_rect b = nk_widget_bounds(ctx);
          ui->table_state.active_block_id = block->id;
          ui->table_state.active_col_idx = (int)c;
          ui->table_state.show_footer_menu = !ui->table_state.show_footer_menu;
          ui->table_state.show_col_menu = 0;
          ui->table_state.menu_pos_x = b.x;
          ui->table_state.menu_pos_y = b.y;
        }
      }
    }
    nk_layout_row_end(ctx);

    // Row count info
    nk_layout_row_dynamic(ctx, 20, 1);
    char info[64];
    snprintf(info, sizeof(info), "%zu rows", view_count);
    nk_label(ctx, info, NK_TEXT_LEFT);
  } else {
    // =========================================================================
    // LEGACY TABLE RENDERING (fallback for old tables)
    // =========================================================================
    if (td->rows <= 0 || td->cols <= 0) {
      nk_layout_row_dynamic(ctx, 30, 1);
      nk_label(ctx, "Empty table - click '+ Row' to add data",
               NK_TEXT_CENTERED);
      return;
    }

    for (int r = 0; r < td->rows; r++) {
      nk_layout_row_static(ctx, row_h, 120, td->cols);

      for (int c = 0; c < td->cols; c++) {
        char buf[256];
        const char *cell = block_table_get_cell(block, r, c);
        strncpy(buf, cell ? cell : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, buf, sizeof(buf) - 1,
                                       nk_filter_default);

        if (strcmp(buf, cell ? cell : "") != 0) {
          block_table_set_cell(block, r, c, buf);
        }
      }
    }
  }

  // =========================================================================
  // RENDER PENDING POPUPS (To avoid clipping inside layouts)
  // =========================================================================
  if (ui->table_state.active_block_id == block->id) {
    if (ui->table_state.show_col_menu) {
      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Add Column",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         nk_rect(ui->table_state.menu_pos_x - 100,
                                 ui->table_state.menu_pos_y + 30, 160, 250))) {
        nk_layout_row_dynamic(ctx, 24, 1);
        const char *types[] = {"Text",   "Number", "Checkbox",
                               "Select", "Date",   "Formula"};
        AslanColumnType type_vals[] = {ASLAN_COL_TEXT,     ASLAN_COL_NUMBER,
                                       ASLAN_COL_CHECKBOX, ASLAN_COL_SELECT,
                                       ASLAN_COL_DATE,     ASLAN_COL_FORMULA};
        for (int i = 0; i < 6; i++) {
          struct nk_rect item_rect = nk_widget_bounds(ctx);
          if (nk_button_label(ctx, "")) {
            if (tbl) {
              char col_name[32];
              snprintf(col_name, sizeof(col_name), "Column %d",
                       (int)tbl->column_count + 1);
              aslan_table_add_column(tbl, col_name, type_vals[i]);
            }
            ui->table_state.show_col_menu = 0;
            nk_popup_close(ctx);
          }
          // Draw icon and text on top of button
          struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
          const char *icon = table_col_type_icon(type_vals[i]);
          if (ui->font_icons) {
            nk_draw_text(canvas,
                         nk_rect(item_rect.x + 4, item_rect.y + 4, 20, 20),
                         icon, strlen(icon), &ui->font_icons->handle,
                         nk_rgba(0, 0, 0, 0), COLOR_TEXT);
          }
          nk_draw_text(
              canvas,
              nk_rect(item_rect.x + 28, item_rect.y + 4, item_rect.w - 32, 20),
              types[i], strlen(types[i]), &ui->font_inter->handle,
              nk_rgba(0, 0, 0, 0), COLOR_TEXT);
        }
        nk_popup_end(ctx);
      } else {
        ui->table_state.show_col_menu = 0;
      }
    }

    if (ui->table_state.show_footer_menu &&
        ui->table_state.active_col_idx >= 0 &&
        ui->table_state.active_col_idx < (int)tbl->column_count) {

      // Smart Placement: Check if there is space below
      struct nk_rect b =
          nk_rect(ui->table_state.menu_pos_x, ui->table_state.menu_pos_y, 0,
                  0); // Origin button
      float win_height = (float)ui->win_height;
      if (win_height == 0)
        win_height = 800; // Fallback

      float menu_h = 250.0f;
      float popup_y = ui->table_state.menu_pos_y;

      // Hotfix v0.1.6: Position Aggregation Menu ALWAYS ABOVE the footer
      popup_y = ui->table_state.menu_pos_y - menu_h;
      if (popup_y < 0)
        popup_y = 0;

      if (nk_popup_begin(
              ctx, NK_POPUP_STATIC, "Aggregation",
              NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
              nk_rect(ui->table_state.menu_pos_x, popup_y, 180, menu_h))) {
        nk_layout_row_dynamic(ctx, 28, 1);
        int c = ui->table_state.active_col_idx;
        AslanColumnType type = tbl->columns[c].type;

        if (nk_button_label(ctx, "None")) {
          aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_NONE);
          ui->table_state.show_footer_menu = 0;
          nk_popup_close(ctx);
        }

        if (nk_button_label(ctx, "# Count")) {
          aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_COUNT);
          ui->table_state.show_footer_menu = 0;
          nk_popup_close(ctx);
        }

        if (type == ASLAN_COL_NUMBER || type == ASLAN_COL_FORMULA) {
          if (nk_button_label(ctx, "Σ Sum")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_SUM);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
          if (nk_button_label(ctx, "μ Average")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_AVG);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
          if (nk_button_label(ctx, "Min")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_MIN);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
          if (nk_button_label(ctx, "Max")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_MAX);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
        }

        if (type == ASLAN_COL_CHECKBOX) {
          if (nk_button_label(ctx, "% Percent")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_PERCENT);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
        }

        if (type == ASLAN_COL_CHECKBOX) {
          if (nk_button_label(ctx, "% Percent")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_PERCENT);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
        }

        if (nk_button_label(ctx, "# Count")) {
          aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_COUNT);
          ui->table_state.show_footer_menu = 0;
          nk_popup_close(ctx);
        }
      }
    }

    if (ui->table_state.show_col_settings &&
        ui->table_state.active_col_idx >= 0 &&
        ui->table_state.active_col_idx < (int)tbl->column_count) {
      int c = ui->table_state.active_col_idx;
      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Column Settings",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         nk_rect(ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y + 32, 200, 350))) {
        nk_layout_row_dynamic(ctx, 24, 1);
        nk_label(ctx, "Name:", NK_TEXT_LEFT);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
                                       ui->table_state.rename_buffer, 63,
                                       nk_filter_default);
        if (nk_button_label(ctx, "Apply Name")) {
          strncpy(tbl->columns[c].name, ui->table_state.rename_buffer, 63);
          ui->table_state.show_col_settings = 0;
          nk_popup_close(ctx);
        }

        nk_label(ctx, "Change Type:", NK_TEXT_LEFT);
        const char *types[] = {"Text",   "Number", "Checkbox",
                               "Select", "Date",   "Formula"};
        AslanColumnType type_vals[] = {ASLAN_COL_TEXT,     ASLAN_COL_NUMBER,
                                       ASLAN_COL_CHECKBOX, ASLAN_COL_SELECT,
                                       ASLAN_COL_DATE,     ASLAN_COL_FORMULA};
        for (int i = 0; i < 6; i++) {
          struct nk_rect item_rect = nk_widget_bounds(ctx);
          if (nk_button_label(ctx, "")) {
            tbl->columns[c].type = type_vals[i];
            ui->table_state.show_col_settings = 0;
            nk_popup_close(ctx);
          }
          struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
          const char *icon = table_col_type_icon(type_vals[i]);
          if (ui->font_icons) {
            nk_draw_text(canvas,
                         nk_rect(item_rect.x + 4, item_rect.y + 4, 20, 20),
                         icon, strlen(icon), &ui->font_icons->handle,
                         nk_rgba(0, 0, 0, 0), COLOR_TEXT);
          }
          nk_draw_text(
              canvas,
              nk_rect(item_rect.x + 28, item_rect.y + 4, item_rect.w - 32, 20),
              types[i], strlen(types[i]), &ui->font_inter->handle,
              nk_rgba(0, 0, 0, 0), COLOR_TEXT);
        }

        nk_spacing(ctx, 1);
        if (nk_button_label(ctx, "Delete Column")) {
          aslan_table_remove_column(tbl, c);
          ui->table_state.show_col_settings = 0;
          nk_popup_close(ctx);
        }
        nk_popup_end(ctx);
      } else {
        ui->table_state.show_col_settings = 0;
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

static int canvas_point_in_toolbar(struct nk_rect bounds, float mx, float my,
                                   CanvasInteraction *cs) {
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  const float toolbar_w = 3 * btn_size + 2 * btn_spacing + 24.0f; // 3 buttons
  const float toolbar_h = 52.0f;
  const float margin = 20.0f;

  float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
  float toolbar_y = bounds.y + bounds.h - toolbar_h - margin;

  // Basic toolbar hit
  if (mx >= toolbar_x && mx < toolbar_x + toolbar_w && my >= toolbar_y &&
      my < toolbar_y + toolbar_h)
    return 1;

  // Sub-menu hit
  if (cs->show_sub_menu) {
    const float menu_w = 140.0f;
    const float item_h = 32.0f;
    const float menu_padding = 8.0f;
    int items = (cs->active_cat == CANVAS_CAT_SHAPES) ? 4 : 3;
    float menu_h = items * item_h + 2 * menu_padding;

    // Calculate which button is active to find menu_x
    float btn_x_start = toolbar_x + 12.0f;
    float active_btn_x =
        btn_x_start + (int)cs->active_cat * (btn_size + btn_spacing);
    float menu_x = active_btn_x - menu_w / 2.0f + 16.0f;
    float menu_y = toolbar_y - menu_h - 12.0f;

    if (mx >= menu_x && mx < menu_x + menu_w && my >= menu_y &&
        my < menu_y + menu_h)
      return 1;
  }

  return 0;
}

// -----------------------------------------------------------------------------
// Canvas Helper: Render floating toolbar (Apple Freeform style)
// -----------------------------------------------------------------------------

// Check if tool is a form tool
static int is_form_tool(CanvasToolMode tool) {
  return tool == CANVAS_TOOL_FORM_RECT || tool == CANVAS_TOOL_FORM_ELLIPSE ||
         tool == CANVAS_TOOL_FORM_TRIANGLE || tool == CANVAS_TOOL_FORM_LINE;
}

static void canvas_render_sub_menu(UIHost *ui, struct nk_context *ctx,
                                   struct nk_command_buffer *canvas_buf,
                                   float x, float y, CanvasCategory cat) {
  CanvasInteraction *cs = &ui->canvas_state;
  const float item_h = 32.0f;
  const float menu_padding = 8.0f;
  const float menu_w = 140.0f;
  int item_count = 0;

  // Define sub-options based on category
  const char *items[8];
  const char *icons[8];
  CanvasSubOption sub_opts[8];

  if (cat == CANVAS_CAT_SELECT) {
    // Pan is now automatic when dragging background, so no menu needed
    item_count = 0; // Hide the menu entirely
  } else if (cat == CANVAS_CAT_SHAPES) {
    items[0] = "Rectangle";
    icons[0] = ICON_SHAPE;
    sub_opts[0] = CANVAS_SUB_SHAPE_RECT;
    items[1] = "Ellipse";
    icons[1] = "\xEE\xAD\xA4";
    sub_opts[1] = CANVAS_SUB_SHAPE_ELLIPSE; // Re-use shape for now
    items[2] = "Triangle";
    icons[2] = ICON_SHAPE;
    sub_opts[2] = CANVAS_SUB_SHAPE_TRIANGLE;
    items[3] = "Line";
    icons[3] = "\xEF\x84\xA5";
    sub_opts[3] = CANVAS_SUB_SHAPE_LINE;
    item_count = 4;
  } else if (cat == CANVAS_CAT_TEXT) {
    items[0] = "Heading";
    icons[0] = ICON_H1;
    sub_opts[0] = CANVAS_SUB_TEXT_HEADING;
    items[1] = "Body";
    icons[1] = ICON_TEXT;
    sub_opts[1] = CANVAS_SUB_TEXT_BODY;
    items[2] = "Sticky";
    icons[2] = ICON_STICKY;
    sub_opts[2] = CANVAS_SUB_TEXT_STICKY;
    item_count = 3;
  } else if (cat == CANVAS_CAT_MEDIA) {
    items[0] = "Image";
    icons[0] = ICON_IMAGE;
    sub_opts[0] = CANVAS_SUB_MEDIA_IMAGE;
    item_count = 1;
  }

  if (item_count == 0)
    return;

  float menu_h = item_count * item_h + 2 * menu_padding;
  float menu_x = x - menu_w / 2.0f + 16.0f; // Center relative to button
  float menu_y = y - menu_h - 12.0f;

  // Shadow
  nk_fill_rect(canvas_buf, nk_rect(menu_x + 2, menu_y + 4, menu_w, menu_h),
               12.0f, nk_rgba(0, 0, 0, 30));
  // Background
  nk_fill_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 12.0f,
               nk_rgb(255, 255, 255));
  nk_stroke_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 12.0f,
                 1.0f, nk_rgb(230, 230, 230));

  for (int i = 0; i < item_count; i++) {
    struct nk_rect item_r =
        nk_rect(menu_x + menu_padding, menu_y + menu_padding + i * item_h,
                menu_w - 2 * menu_padding, item_h);
    int hovered =
        (ui->mouse_x >= item_r.x && ui->mouse_x < item_r.x + item_r.w &&
         ui->mouse_y >= item_r.y && ui->mouse_y < item_r.y + item_r.h);

    if (hovered) {
      nk_fill_rect(canvas_buf, item_r, 6.0f, nk_rgba(0, 0, 0, 15));
      if (cs->mouse_just_pressed) {
        // Handle sub-option selection
        if (cat == CANVAS_CAT_SHAPES) {
          // Set pending_create to 10-13 for shapes
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          if (sub_opts[i] == CANVAS_SUB_SHAPE_RECT)
            cs->pending_create = 10;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_ELLIPSE)
            cs->pending_create = 11;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_TRIANGLE)
            cs->pending_create = 12;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_LINE)
            cs->pending_create = 13;
        } else if (cat == CANVAS_CAT_TEXT) {
          cs->tool = CANVAS_TOOL_SELECT;
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          // H1=6, Body=1, Sticky=2
          if (sub_opts[i] == CANVAS_SUB_TEXT_HEADING)
            cs->pending_create = 6;
          else if (sub_opts[i] == CANVAS_SUB_TEXT_BODY)
            cs->pending_create = 1;
          else if (sub_opts[i] == CANVAS_SUB_TEXT_STICKY)
            cs->pending_create = 2;
        } else if (cat == CANVAS_CAT_MEDIA) {
          cs->tool = CANVAS_TOOL_SELECT;
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          if (sub_opts[i] == CANVAS_SUB_MEDIA_IMAGE)
            cs->pending_create = 3;
        }
        cs->show_sub_menu = 0;
      }
    }

    // Icon
    if (ui->font_icons) {
      nk_draw_text(canvas_buf, nk_rect(item_r.x + 8, item_r.y + 6, 20, 20),
                   icons[i], strlen(icons[i]), &ui->font_icons->handle,
                   nk_rgba(0, 0, 0, 0), nk_rgb(80, 80, 80));
    }
    // Label
    nk_draw_text(canvas_buf,
                 nk_rect(item_r.x + 36, item_r.y + 6, item_r.w - 40, 20),
                 items[i], strlen(items[i]), ctx->style.font,
                 nk_rgba(0, 0, 0, 0), nk_rgb(40, 40, 40));
  }
}

// -----------------------------------------------------------------------------
// Sidebar Helpers
// -----------------------------------------------------------------------------

static void ui_render_sidebar_properties(UIHost *ui, struct nk_context *ctx,
                                         Block *block) {
  if (!ui->selected_node) {
    nk_layout_row_dynamic(ctx, 30, 1);
    nk_label(ctx, "No element selected", NK_TEXT_LEFT);
    return;
  }

  Node *node = ui->selected_node;

  // --- SPECIAL HANDLING FOR TEXT NODES ---
  if (node->type == NODE_TYPE_TEXT) {
    // 1. Text Content
    nk_layout_row_dynamic(ctx, 25, 1);
    nk_label(ctx, "Text Content", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 60, 1);
    nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX, node->text,
                                   sizeof(node->text) - 1, nk_filter_default);

    // 2. Font Type
    nk_layout_row_dynamic(ctx, 25, 1);
    nk_label(ctx, "Font", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 30, 1);
    const char *fonts[] = {"Inter (Body)", "Heading 1", "Heading 2",
                           "Heading 3", "Monospace"};
    node->font_type =
        nk_combo(ctx, fonts, 5, node->font_type, 30, nk_vec2(200, 200));

    // 3. Font Size (More responsive step)
    // Auto-fix invalid font size for legacy/broken nodes
    if (node->font_size < 8.0f) {
      node->font_size = 12.0f;
    }
    nk_layout_row_dynamic(ctx, 30, 1);
    nk_property_float(ctx, "Size:", 8.0f, &node->font_size, 200.0f, 1.0f, 1.0f);

    // 4. Color (Text Color -> stroke_color)
    nk_layout_row_dynamic(ctx, 25, 1);
    nk_label(ctx, "Color", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 30, 1);
    struct nk_color combo_color = nk_rgba(
        (node->stroke_color >> 24) & 0xFF, (node->stroke_color >> 16) & 0xFF,
        (node->stroke_color >> 8) & 0xFF, node->stroke_color & 0xFF);
    if (nk_combo_begin_color(ctx, combo_color, nk_vec2(200, 400))) {
      struct nk_colorf combo_colorf = nk_color_cf(combo_color);
      nk_layout_row_dynamic(ctx, 180, 1);
      combo_colorf = nk_color_picker(ctx, combo_colorf, NK_RGBA);
      nk_layout_row_dynamic(ctx, 25, 1);
      combo_colorf.r =
          nk_propertyf(ctx, "#R:", 0, combo_colorf.r, 1, 0.01f, 0.005f);
      combo_colorf.g =
          nk_propertyf(ctx, "#G:", 0, combo_colorf.g, 1, 0.01f, 0.005f);
      combo_colorf.b =
          nk_propertyf(ctx, "#B:", 0, combo_colorf.b, 1, 0.01f, 0.005f);
      combo_colorf.a =
          nk_propertyf(ctx, "#A:", 0, combo_colorf.a, 1, 0.01f, 0.005f);
      nk_combo_end(ctx);
      combo_color = nk_rgba(
          (int)(combo_colorf.r * 255.0f), (int)(combo_colorf.g * 255.0f),
          (int)(combo_colorf.b * 255.0f), (int)(combo_colorf.a * 255.0f));
      node->stroke_color =
          ((uint32_t)combo_color.r << 24) | ((uint32_t)combo_color.g << 16) |
          ((uint32_t)combo_color.b << 8) | (uint32_t)combo_color.a;
    }

    return;
  }

  // Node Name
  nk_layout_row_dynamic(ctx, 25, 1);
  nk_label(ctx, "Name", NK_TEXT_LEFT);
  nk_layout_row_dynamic(ctx, 30, 1);
  nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                                 node->name, sizeof(node->name) - 1,
                                 nk_filter_default);

  // Shape Type
  nk_layout_row_dynamic(ctx, 25, 1);
  nk_label(ctx, "Shape Type", NK_TEXT_LEFT);
  nk_layout_row_dynamic(ctx, 30, 1);
  const char *type_name = node_type_to_string(node->type);
  if (nk_button_label(ctx, type_name ? type_name : "Unknown")) {
    ui->sidebar_mode = 1; // Switch to shape selection
  }

  // Fill Color (Removed Width/Height)

  // Fill Color
  nk_layout_row_dynamic(ctx, 25, 1);
  nk_label(ctx, "Fill Color", NK_TEXT_LEFT);
  nk_layout_row_dynamic(ctx, 30, 1);
  struct nk_color combo_color =
      nk_rgba((node->fill_color >> 24) & 0xFF, (node->fill_color >> 16) & 0xFF,
              (node->fill_color >> 8) & 0xFF, node->fill_color & 0xFF);
  if (nk_combo_begin_color(ctx, combo_color, nk_vec2(200, 400))) {
    struct nk_colorf combo_colorf = nk_color_cf(combo_color);
    nk_layout_row_dynamic(ctx, 180, 1);
    combo_colorf = nk_color_picker(ctx, combo_colorf, NK_RGBA);
    nk_layout_row_dynamic(ctx, 25, 1);
    combo_colorf.r =
        nk_propertyf(ctx, "#R:", 0, combo_colorf.r, 1, 0.01f, 0.005f);
    combo_colorf.g =
        nk_propertyf(ctx, "#G:", 0, combo_colorf.g, 1, 0.01f, 0.005f);
    combo_colorf.b =
        nk_propertyf(ctx, "#B:", 0, combo_colorf.b, 1, 0.01f, 0.005f);
    combo_colorf.a =
        nk_propertyf(ctx, "#A:", 0, combo_colorf.a, 1, 0.01f, 0.005f);
    nk_combo_end(ctx);
    combo_color =
        nk_rgba((int)(combo_colorf.r * 255.0f), (int)(combo_colorf.g * 255.0f),
                (int)(combo_colorf.b * 255.0f), (int)(combo_colorf.a * 255.0f));
    node->fill_color = ((uint32_t)combo_color.r << 24) |
                       ((uint32_t)combo_color.g << 16) |
                       ((uint32_t)combo_color.b << 8) | (uint32_t)combo_color.a;
  }

  // Stroke
  nk_layout_row_dynamic(ctx, 30, 1);
  nk_property_float(ctx, "Thickness:", 0.0f, &node->stroke_width, 100.0f, 0.5f,
                    0.25f);

  nk_layout_row_dynamic(ctx, 25, 1);
  nk_label(ctx, "Stroke Color", NK_TEXT_LEFT);
  nk_layout_row_dynamic(ctx, 30, 1);
  struct nk_color stroke_combo_color = nk_rgba(
      (node->stroke_color >> 24) & 0xFF, (node->stroke_color >> 16) & 0xFF,
      (node->stroke_color >> 8) & 0xFF, node->stroke_color & 0xFF);
  if (nk_combo_begin_color(ctx, stroke_combo_color, nk_vec2(200, 400))) {
    struct nk_colorf combo_colorf = nk_color_cf(stroke_combo_color);
    nk_layout_row_dynamic(ctx, 180, 1);
    combo_colorf = nk_color_picker(ctx, combo_colorf, NK_RGBA);
    nk_layout_row_dynamic(ctx, 25, 1);
    combo_colorf.r =
        nk_propertyf(ctx, "#R:", 0, combo_colorf.r, 1, 0.01f, 0.005f);
    combo_colorf.g =
        nk_propertyf(ctx, "#G:", 0, combo_colorf.g, 1, 0.01f, 0.005f);
    combo_colorf.b =
        nk_propertyf(ctx, "#B:", 0, combo_colorf.b, 1, 0.01f, 0.005f);
    combo_colorf.a =
        nk_propertyf(ctx, "#A:", 0, combo_colorf.a, 1, 0.01f, 0.005f);
    nk_combo_end(ctx);
    stroke_combo_color =
        nk_rgba((int)(combo_colorf.r * 255.0f), (int)(combo_colorf.g * 255.0f),
                (int)(combo_colorf.b * 255.0f), (int)(combo_colorf.a * 255.0f));
    node->stroke_color = ((uint32_t)stroke_combo_color.r << 24) |
                         ((uint32_t)stroke_combo_color.g << 16) |
                         ((uint32_t)stroke_combo_color.b << 8) |
                         (uint32_t)stroke_combo_color.a;
  }

  // Shape Mode (Standard / Container)
  nk_layout_row_dynamic(ctx, 25, 1);
  nk_label(ctx, "Shape Mode", NK_TEXT_LEFT);
  nk_layout_row_dynamic(ctx, 30, 1);
  const char *modes[] = {"Standard", "Container"};
  int old_mode = node->is_container ? 1 : 0;
  int new_mode = nk_combo(ctx, modes, 2, old_mode, 30, nk_vec2(200, 200));
  if (new_mode != old_mode) {
    node->is_container = new_mode;
    if (new_mode == 1) { // If switched to container
      // Optionally reset image or keep it
    }
  }

  // Node-specific Text
  if (node->type == NODE_TYPE_TEXT) {
    nk_layout_row_dynamic(ctx, 25, 1);
    nk_label(ctx, "Text Content", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 60, 1);
    nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX, node->text,
                                   sizeof(node->text) - 1, nk_filter_default);
  }
}

static void ui_render_sidebar_layers(UIHost *ui, struct nk_context *ctx,
                                     Block *block) {
  Node *root = NULL;
  if (block->type == BLOCK_CANVAS)
    root = block->data.canvas.root;
  else if (block->type == BLOCK_DESIGN)
    root = block->data.design.root;

  if (!root)
    return;

  // List nodes in reverse order (drawn top to bottom in UI)
  Node *nodes[1024];
  int count = 0;
  for (Node *node = root->first_child; node && count < 1024;
       node = node->next_sibling) {
    nodes[count++] = node;
  }

  for (int i = count - 1; i >= 0; i--) {
    Node *node = nodes[i];

    nk_layout_row_template_begin(ctx, 30);
    nk_layout_row_template_push_static(ctx, 25);   // Eye (Visibility)
    nk_layout_row_template_push_variable(ctx, 80); // Name
    nk_layout_row_template_push_static(ctx, 25);   // Up
    nk_layout_row_template_push_static(ctx, 25);   // Down
    nk_layout_row_template_push_static(ctx, 25);   // Delete
    nk_layout_row_template_end(ctx);

    // Visibility toggle
    if (ui_icon_button(ui, node->visible ? ICON_EYE : ICON_EYE_OFF)) {
      node->visible = !node->visible;
    }

    // Selection / Name
    int is_selected = (ui->selected_node == node);
    struct nk_style_button bstyle = ctx->style.button;
    if (is_selected) {
      bstyle.normal = nk_style_item_color(COLOR_ACCENT);
      bstyle.hover = nk_style_item_color(COLOR_ACCENT);
      bstyle.active = nk_style_item_color(COLOR_ACCENT);
    } else {
      bstyle.normal = ctx->style.button.normal;
    }
    bstyle.text_normal = COLOR_TEXT;
    bstyle.text_hover = COLOR_TEXT;
    bstyle.text_active = COLOR_TEXT;
    bstyle.padding = nk_vec2(4, 4);

    const char *node_display_name = node->name[0] ? node->name : "Untitled";
    if (!node->name[0]) {
      if (node->type == NODE_TYPE_RECT)
        node_display_name = "Rectangle";
      else if (node->type == NODE_TYPE_ELLIPSE)
        node_display_name = "Ellipse";
      else if (node->type == NODE_TYPE_TEXT)
        node_display_name = "Text";
      else if (node->type == NODE_TYPE_IMAGE)
        node_display_name = "Image";
      else if (node->type == NODE_TYPE_PATH)
        node_display_name = "Path";
      else if (node->type == NODE_TYPE_STROKE) {
        if (node->stroke_data && node->stroke_data->type == STROKE_TYPE_MARKER)
          node_display_name = "Marker Stroke";
        else
          node_display_name = "Pen Stroke";
      }
    }

    if (nk_button_label_styled(ctx, &bstyle, node_display_name)) {
      ui->selected_node = node;
    }

    // Reordering
    if (ui_icon_button(ui, ICON_COLLAPSE)) { // Up arrow
      node_move_up(node);
    }
    if (ui_icon_button(ui, ICON_EXPAND)) { // Down arrow
      node_move_down(node);
    }

    // Delete
    if (ui_icon_button(ui, ICON_DELETE)) {
      if (block->type == BLOCK_CANVAS)
        block_canvas_delete_node(block, node->id);
      else if (block->type == BLOCK_DESIGN)
        block_design_delete_node(block, node->id);
      if (ui->selected_node == node)
        ui->selected_node = NULL;
    }
  }
}

static void ui_render_sidebar_shape_selection(UIHost *ui,
                                              struct nk_context *ctx) {
  if (!ui->selected_node) {
    ui->sidebar_mode = 0;
    return;
  }

  nk_layout_row_dynamic(ctx, 30, 1);
  if (nk_button_label(ctx, "< Back to Properties")) {
    ui->sidebar_mode = 0;
  }

  nk_layout_row_dynamic(ctx, 20, 1);
  nk_spacing(ctx, 1);

  nk_layout_row_dynamic(ctx, 30, 1);
  nk_label(ctx, "Choose Shape:", NK_TEXT_LEFT);

  // Grid: 3 items per row
  NodeType types[] = {NODE_TYPE_RECT,    NODE_TYPE_ELLIPSE,  NODE_TYPE_TRIANGLE,
                      NODE_TYPE_OCTAGON, NODE_TYPE_PENTAGON, NODE_TYPE_HEXAGON,
                      NODE_TYPE_HEART,   NODE_TYPE_LINE};
  const char *labels[] = {"Rect",     "Ellipse", "Triangle", "Octagon",
                          "Pentagon", "Hexagon", "Heart",    "Line"};

  nk_layout_row_dynamic(ctx, 45, 3);
  for (int i = 0; i < 8; i++) {
    if (nk_button_label(ctx, labels[i])) {
      ui->selected_node->type = types[i];
      ui->sidebar_mode = 0;
    }
  }
}

static void recursive_convert_to_path(Node *node) {
  if (!node)
    return;

  // Convert current node only if it's a basic geometry shape
  // Note: We avoid converting TEXT or IMAGE or existing PATHs
  if (node->type == NODE_TYPE_RECT || node->type == NODE_TYPE_ELLIPSE ||
      node->type == NODE_TYPE_TRIANGLE || node->type == NODE_TYPE_OCTAGON ||
      node->type == NODE_TYPE_TRIANGLE || node->type == NODE_TYPE_OCTAGON ||
      node->type == NODE_TYPE_HEART || node->type == NODE_TYPE_PENTAGON ||
      node->type == NODE_TYPE_HEXAGON) {
    node_convert_to_path(node);
  }

  // Recurse to children
  if (node->first_child) {
    recursive_convert_to_path(node->first_child);
  }

  // Recurse to siblings
  if (node->next_sibling) {
    recursive_convert_to_path(node->next_sibling);
  }
}

static void canvas_render_toolbar(UIHost *ui, struct nk_context *ctx,
                                  struct nk_rect bounds, Block *block) {
  (void)block;

  // 6 buttons: Shapes, Text, Image, Brush, Color, Bezier
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  const float toolbar_w = 6 * btn_size + 5 * btn_spacing + 24.0f;
  const float toolbar_h = 52.0f;
  const float margin = 20.0f;

  float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
  float toolbar_y = bounds.y + bounds.h - toolbar_h - margin;

  struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
  struct nk_color toolbar_bg = {255, 255, 255, 255};
  struct nk_color toolbar_border = {220, 220, 220, 255};

  CanvasInteraction *cs = &ui->canvas_state;

  // Shadow
  nk_fill_rect(canvas_buf,
               nk_rect(toolbar_x + 2, toolbar_y + 4, toolbar_w, toolbar_h),
               26.0f, nk_rgba(0, 0, 0, 20));

  // Background
  nk_fill_rect(canvas_buf, nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h),
               26.0f, toolbar_bg);
  nk_stroke_rect(canvas_buf,
                 nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h), 26.0f,
                 1.0f, toolbar_border);

  float btn_x = toolbar_x + 12.0f;
  float btn_y = toolbar_y + (toolbar_h - btn_size) / 2.0f;

  // 6 buttons: Shapes, Text, Image, Brush, Color, Bezier
  // Brush icon changes based on marker mode
  const char *brush_icon = cs->pen_is_marker ? ICON_MARKER : ICON_BRUSH;
  const char *tool_icons[] = {ICON_SHAPE, ICON_TEXT, ICON_IMAGE,
                              brush_icon, "",        ICON_MAGIC};
  int tool_count = 6;

  // Preset colors for color swatch cycling
  static uint32_t preset_colors[] = {
      0x000000FF, // Black
      0xFF0000FF, // Red
      0x0000FFFF, // Blue
      0x00FF00FF, // Green
      0xFFFF00FF, // Yellow
      0xFF00FFFF, // Magenta
      0xFF8800FF, // Orange
      0xFF69B4FF  // Pink
  };
  static int preset_count = 8;

  for (int i = 0; i < tool_count; i++) {
    struct nk_rect btn = nk_rect(btn_x + i * (btn_size + btn_spacing), btn_y,
                                 btn_size, btn_size);

    // Map button index to category (skip CANVAS_CAT_SELECT)
    int is_active = (cs->active_cat == (CanvasCategory)(i + 1));
    if (i == 3)
      is_active = (cs->tool == CANVAS_TOOL_PEN); // Brush/Marker combined
    if (i == 4)
      is_active = (cs->active_cat == CANVAS_CAT_COLOR);
    if (i == 5)
      is_active = cs->bezier_mode;
    int hovered = (ui->mouse_x >= btn.x && ui->mouse_x < btn.x + btn.w &&
                   ui->mouse_y >= btn.y && ui->mouse_y < btn.y + btn.h);

    struct nk_color bg_color = is_active ? nk_rgba(66, 133, 244, 25)
                               : hovered ? nk_rgba(0, 0, 0, 10)
                                         : nk_rgba(0, 0, 0, 0);
    nk_fill_rect(canvas_buf, btn, 8.0f, bg_color);

    struct nk_color icon_color =
        is_active ? nk_rgb(66, 133, 244) : nk_rgb(60, 60, 60);

    if (ui->font_icons) {
      nk_draw_text(canvas_buf,
                   nk_rect(btn.x + 8, btn.y + 8, btn_size - 16, btn_size - 16),
                   tool_icons[i], strlen(tool_icons[i]),
                   &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), icon_color);
    }

    if (hovered && cs->mouse_just_pressed) {
      if (i == 5) {
        printf("[DEBUG] Bezier button hovered and mouse_just_pressed=%d\n",
               cs->mouse_just_pressed);
      }
      if (i ==
          3) { // Brush button - toggles between brush, marker and deselected
        printf("[DEBUG] Clicked Brush/Marker button. Current tool: %d, marker: "
               "%d\n",
               cs->tool, cs->pen_is_marker);
        if (cs->tool == CANVAS_TOOL_PEN) {
          if (!cs->pen_is_marker) {
            // State 1 (Pen) -> State 2 (Marker)
            cs->pen_is_marker = 1;
            printf("[DEBUG] Switched to Marker mode\n");
          } else {
            // State 2 (Marker) -> State 3 (None/Select)
            cs->tool = CANVAS_TOOL_SELECT;
            cs->pen_is_marker = 0; // Reset for next time
            printf("[DEBUG] Deselected pen tool\n");
          }
        } else {
          // State 3 (None) -> State 1 (Pen)
          cs->tool = CANVAS_TOOL_PEN;
          cs->pen_is_marker = 0;
          printf("[DEBUG] Set tool to CANVAS_TOOL_PEN\n");
        }
        cs->active_cat =
            (cs->tool == CANVAS_TOOL_PEN)
                ? (cs->pen_is_marker ? CANVAS_CAT_MARKER : CANVAS_CAT_BRUSH)
                : CANVAS_CAT_SELECT;
        cs->show_sub_menu = 0;
      } else if (i == 4) { // Color swatch button
        // Cycle through preset colors
        int current_idx = 0;
        for (int c = 0; c < preset_count; c++) {
          if (cs->selected_color == preset_colors[c]) {
            current_idx = c;
            break;
          }
        }
        cs->selected_color = preset_colors[(current_idx + 1) % preset_count];

        // Also apply to current selection if any
        if (ui->selected_node) {
          ui->selected_node->fill_color = cs->selected_color;
          if (ui->selected_node->type == NODE_TYPE_RECT &&
              ui->selected_node->height < 10) {
            ui->selected_node->stroke_color = cs->selected_color; // For lines
          }
        }
      } else if (i == 5) { // Bezier (Magic) button
        cs->bezier_mode = !cs->bezier_mode;

        // Convert ALL shapes in the current document to paths
        if (cs->bezier_mode && block->type == BLOCK_CANVAS &&
            block->data.canvas.root) {
          recursive_convert_to_path(block->data.canvas.root);
        }

        cs->active_cat = CANVAS_CAT_SELECT;
        cs->show_sub_menu = 0;
      } else {
        CanvasCategory cat = (CanvasCategory)(i + 1); // Skip CANVAS_CAT_SELECT
        if (cat == CANVAS_CAT_SHAPES) {
          // DEFAULT: Just create a rectangle
          cs->pending_create = 10;
          cs->show_sub_menu = 0;
        } else if (cat == CANVAS_CAT_TEXT) {
          // DIRECT ACTION: Create paragraph
          cs->pending_create = 1; // 1 = Body Text
          cs->show_sub_menu = 0;
        } else if (cat == CANVAS_CAT_MEDIA) {
          // DIRECT ACTION: Open image dialog
          cs->pending_create = 3; // 3 = Image
          cs->show_sub_menu = 0;
        } else if (cs->active_cat == cat) {
          cs->show_sub_menu = !cs->show_sub_menu;
        } else {
          cs->active_cat = cat;
          cs->show_sub_menu = 1; // Open sub-menu for others
        }
      }
    }

    // Special rendering for color swatch (button 4)
    if (i == 4) {
      // Draw color swatch instead of icon
      struct nk_color swatch_color = nk_rgba(
          (cs->selected_color >> 24) & 0xFF, (cs->selected_color >> 16) & 0xFF,
          (cs->selected_color >> 8) & 0xFF, cs->selected_color & 0xFF);
      struct nk_rect swatch_rect =
          nk_rect(btn.x + 8, btn.y + 8, btn_size - 16, btn_size - 16);
      nk_fill_circle(canvas_buf, swatch_rect, swatch_color);
      nk_stroke_circle(canvas_buf, swatch_rect, 1.5f, nk_rgb(100, 100, 100));
    }

    // Render sub-menu if active
    if (is_active && cs->show_sub_menu) {
      canvas_render_sub_menu(ui, ctx, canvas_buf, btn.x, btn.y,
                             (CanvasCategory)(i + 1)); // Skip CANVAS_CAT_SELECT
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
  NODE_STYLE_TEXT // Plain text, no background
} NodeStyleType;

static Node *design_create_styled_node(DesignData *dd, NodeStyleType style,
                                       float x, float y, float w, float h,
                                       uint32_t color) {
  if (!dd || !dd->root)
    return NULL;

  Node *node = (Node *)calloc(1, sizeof(Node));
  if (!node)
    return NULL;

  node->id = dd->next_node_id++;
  node->x = x;
  node->y = y;
  node->width = w;
  node->height = h;
  node->visible = 1;
  node->parent = dd->root;

  // Use the same styles as canvas for consistency
  switch (style) {
  case NODE_STYLE_RECT:
    node->type = NODE_TYPE_RECT;
    node->fill_color = color;
    node->stroke_color = 0x1A73E8FF;
    node->stroke_width = 2.0f;
    node->corner_radius = 8.0f;
    break;

  case NODE_STYLE_ELLIPSE:
    node->type = NODE_TYPE_ELLIPSE;
    node->fill_color = color;
    node->stroke_color = 0x1E8E3EFF;
    node->stroke_width = 2.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_TRIANGLE:
    node->type = NODE_TYPE_TRIANGLE;
    node->fill_color = color;
    node->stroke_color = 0xF9A825FF;
    node->stroke_width = 2.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_LINE:
    node->type = NODE_TYPE_RECT;
    node->fill_color = color;
    node->stroke_color = color;
    node->stroke_width = 2.0f;
    node->corner_radius = 0.0f;
    node->height = 4.0f; // Thin line
    break;

  case NODE_STYLE_TEXT:
    node->type = NODE_TYPE_TEXT;
    node->fill_color = 0x00000000;   // Transparent
    node->stroke_color = 0x000000FF; // Black text
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    node->font_size = 12.0f;
    node->font_type = 0;
    strncpy(node->text, "Text", sizeof(node->text) - 1);
    break;
  }

  // Add to tree
  if (!dd->root->first_child) {
    dd->root->first_child = node;
  } else {
    Node *last = dd->root->first_child;
    while (last->next_sibling)
      last = last->next_sibling;
    last->next_sibling = node;
  }

  return node;
}

static Node *canvas_create_styled_node(CanvasData *cd, NodeStyleType style,
                                       float x, float y, float w, float h,
                                       uint32_t color) {
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
    node->fill_color = color;
    node->stroke_color = 0x1A73E8FF;
    node->stroke_width = 0.0f;
    node->corner_radius = 8.0f;
    break;

  case NODE_STYLE_ELLIPSE:
    node->type = NODE_TYPE_ELLIPSE;
    node->fill_color = color;
    node->stroke_color = 0x1E8E3EFF;
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_TRIANGLE:
    node->type = NODE_TYPE_TRIANGLE;
    node->fill_color = color;
    node->stroke_color = 0xF9A825FF;
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_LINE:
    node->type = NODE_TYPE_RECT;
    node->fill_color = color;
    node->stroke_color = color;
    node->stroke_width = 2.0f;
    node->corner_radius = 0.0f;
    node->height = 4.0f; // Thin line
    break;

  case NODE_STYLE_TEXT:
    node->type = NODE_TYPE_TEXT;
    node->fill_color = 0x00000000;   // Transparent
    node->stroke_color = 0x000000FF; // Black text
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    node->font_size = 12.0f;
    node->font_type = 0;
    strncpy(node->text, "Text", sizeof(node->text) - 1);
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

  printf("[DEBUG] get_image_texture: trying path '%s' -> %p\n", path, pixels);

  if (!pixels) {
    // Try relative to assets if not already starting with assets/
    if (strncmp(path, "assets/", 7) != 0) {
      char full_path[512];
      snprintf(full_path, sizeof(full_path), "assets/%s", path);
      pixels = mac_load_image_rgba(full_path, &w, &h);
      printf("[DEBUG] get_image_texture: trying full_path '%s' -> %p\n",
             full_path, pixels);
    }
  }

  fflush(stdout);

  if (!pixels) {
    fprintf(stderr, "[ERROR] get_image_texture failed to load: %s\n", path);
    return NULL;
  }

  // Create texture
  SDL_Texture *tex = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, w, h);
  if (tex) {
    SDL_UpdateTexture(tex, NULL, pixels, w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  }

  free(pixels);

  if (!tex)
    return NULL;

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

static int tessellate_bezier(float x1, float y1, float cp1x, float cp1y,
                             float cp2x, float cp2y, float x2, float y2,
                             float *out_pts, int max_pts, int include_last) {
  int segments = 12;
  int count = 0;
  for (int i = 0; i < segments + (include_last ? 1 : 0); i++) {
    if (count >= max_pts)
      break;
    float t = (float)i / (float)segments;
    float mt = 1.0f - t;
    float mt2 = mt * mt;
    float mt3 = mt2 * mt;
    float t2 = t * t;
    float t3 = t2 * t;

    out_pts[count * 2] =
        mt3 * x1 + 3 * mt2 * t * cp1x + 3 * mt * t2 * cp2x + t3 * x2;
    out_pts[count * 2 + 1] =
        mt3 * y1 + 3 * mt2 * t * cp1y + 3 * mt * t2 * cp2y + t3 * y2;
    count++;
  }
  return count;
}

static int node_get_path_polygon(Node *node, float *out_pts, int max_pts,
                                 float sx, float sy, float zoom) {
  if (!node || node->type != NODE_TYPE_PATH || !node->path_points)
    return 0;
  int total_count = 0;
  for (int i = 0; i < node->path_point_count; i++) {
    int next = (i + 1) % node->path_point_count;
    NodePathPoint p1 = node->path_points[i];
    NodePathPoint p2 = node->path_points[next];

    if (p1.type == 1) { // Cubic Bezier
      int c = tessellate_bezier(
          sx + p1.x * zoom, sy + p1.y * zoom, sx + p1.cp2x * zoom,
          sy + p1.cp2y * zoom, sx + p2.cp1x * zoom, sy + p2.cp1y * zoom,
          sx + p2.x * zoom, sy + p2.y * zoom, &out_pts[total_count * 2],
          max_pts - total_count, 0);
      total_count += c;
    } else {
      if (total_count < max_pts) {
        out_pts[total_count * 2] = sx + p1.x * zoom;
        out_pts[total_count * 2 + 1] = sy + p1.y * zoom;
        total_count++;
      }
    }
  }
  return total_count;
}

static void node_update_path_bounds(Node *node) {
  if (!node || node->type != NODE_TYPE_PATH || !node->path_points ||
      node->path_point_count == 0)
    return;

  float min_x = node->path_points[0].x;
  float min_y = node->path_points[0].y;
  float max_x = min_x;
  float max_y = min_y;

  for (int i = 0; i < node->path_point_count; i++) {
    NodePathPoint p = node->path_points[i];
    if (p.x < min_x)
      min_x = p.x;
    if (p.y < min_y)
      min_y = p.y;
    if (p.x > max_x)
      max_x = p.x;
    if (p.y > max_y)
      max_y = p.y;
    if (p.type == 1) {
      if (p.cp1x < min_x)
        min_x = p.cp1x;
      if (p.cp1y < min_y)
        min_y = p.cp1y;
      if (p.cp1x > max_x)
        max_x = p.cp1x;
      if (p.cp1y > max_y)
        max_y = p.cp1y;
      if (p.cp2x < min_x)
        min_x = p.cp2x;
      if (p.cp2y < min_y)
        min_y = p.cp2y;
      if (p.cp2x > max_x)
        max_x = p.cp2x;
      if (p.cp2y > max_y)
        max_y = p.cp2y;
    }
  }

  // Offset correction to keep points stable while moving the node's origin
  float dx = min_x;
  float dy = min_y;

  if (fabsf(dx) > 0.0001f || fabsf(dy) > 0.0001f) {
    node->x += dx;
    node->y += dy;
    for (int i = 0; i < node->path_point_count; i++) {
      node->path_points[i].x -= dx;
      node->path_points[i].y -= dy;
      node->path_points[i].cp1x -= dx;
      node->path_points[i].cp1y -= dy;
      node->path_points[i].cp2x -= dx;
      node->path_points[i].cp2y -= dy;
    }
    max_x -= dx;
    max_y -= dy;
  }

  node->width = max_x;
  node->height = max_y;
}

static void node_convert_to_path(Node *node) {
  if (!node || node->type == NODE_TYPE_PATH)
    return;

  // Define points based on shape type
  NodePathPoint *pts = NULL;
  int count = 0;

  if (node->type == NODE_TYPE_RECT) {
    count = 4;
    pts = (NodePathPoint *)calloc(count, sizeof(NodePathPoint));
    pts[0] = (NodePathPoint){0, 0, 0, 0, 0, 0, 0};
    pts[1] = (NodePathPoint){node->width, 0, node->width, 0, node->width, 0, 0};
    pts[2] = (NodePathPoint){node->width,
                             node->height,
                             node->width,
                             node->height,
                             node->width,
                             node->height,
                             0};
    pts[3] =
        (NodePathPoint){0, node->height, 0, node->height, 0, node->height, 0};
  } else if (node->type == NODE_TYPE_TRIANGLE) {
    count = 3;
    pts = (NodePathPoint *)calloc(count, sizeof(NodePathPoint));
    pts[0] = (NodePathPoint){
        node->width / 2, 0, node->width / 2, 0, node->width / 2, 0, 0};
    pts[1] =
        (NodePathPoint){0, node->height, 0, node->height, 0, node->height, 0};
    pts[2] = (NodePathPoint){node->width,
                             node->height,
                             node->width,
                             node->height,
                             node->width,
                             node->height,
                             0};
  } else if (node->type == NODE_TYPE_ELLIPSE) {
    count = 4;
    pts = (NodePathPoint *)calloc(count, sizeof(NodePathPoint));
    float kappa = 0.552284749831f;
    float ox = node->width / 2.0f;
    float oy = node->height / 2.0f;
    float rx = node->width / 2.0f;
    float ry = node->height / 2.0f;

    // Top
    pts[0] = (NodePathPoint){ox, 0, ox - rx * kappa, 0, ox + rx * kappa, 0, 1};
    // Right
    pts[1] = (NodePathPoint){
        node->width,     oy, node->width, oy - ry * kappa, node->width,
        oy + ry * kappa, 1};
    // Bottom
    pts[2] = (NodePathPoint){ox,
                             node->height,
                             ox + rx * kappa,
                             node->height,
                             ox - rx * kappa,
                             node->height,
                             1};
    // Left
    pts[3] = (NodePathPoint){0, oy, 0, oy + ry * kappa, 0, oy - ry * kappa, 1};
  }

  if (pts) {
    if (node->path_points)
      free(node->path_points);
    node->path_points = pts;
    node->path_point_count = count;
    node->type = NODE_TYPE_PATH;
  }
}

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

  float cam_x = cs->cam_x;
  float cam_y = cs->cam_y;
  float zoom = cs->zoom;

  // Toggle for expand/collapse
  ui_render_expand_toggle(ui, ctx, block);

  // Canvas height
  float total_h = 400.0f;
  float preview_h = 400.0f;
  float editor_h = 0.0f;
  int is_expanded = (ui->expanded_block_id == block->id);

  if (is_expanded) {
    total_h = nk_window_get_height(ctx) - 80.0f;
    preview_h = total_h; // Use 100% for canvas in fullscreen
    editor_h = 0.0f;     // No editor in fullscreen
  }

  // Preview Area (Canvas) - 2 columns in fullscreen, 1 column otherwise
  if (is_expanded) {
    // Fullscreen: Canvas + Sidebar layout
    nk_layout_row_template_begin(ctx, preview_h);
    nk_layout_row_template_push_variable(ctx, 75);   // Canvas (flexible)
    nk_layout_row_template_push_static(ctx, 250.0f); // Sidebar (fixed 250px)
    nk_layout_row_template_end(ctx);
  } else {
    // Normal mode: Single column
    nk_layout_row_dynamic(ctx, preview_h, 1);
  }

  struct nk_rect bounds = nk_widget_bounds(ctx);

  // --- 1. Interaction Logic (Calculations) ---
  cs->bounds_x = bounds.x;
  cs->bounds_y = bounds.y;
  cs->bounds_w = bounds.w;
  cs->bounds_h = bounds.h;
  cs->mouse_in_canvas =
      (ui->mouse_x >= bounds.x && ui->mouse_x < bounds.x + bounds.w &&
       ui->mouse_y >= bounds.y && ui->mouse_y < bounds.y + bounds.h);

  // Explicitly exclude sidebar area in fullscreen/expanded mode
  if (is_expanded && ui->mouse_x > nk_window_get_width(ctx) - 250.0f) {
    cs->mouse_in_canvas = 0;
  }

  int in_toolbar =
      canvas_point_in_toolbar(bounds, ui->mouse_x, ui->mouse_y, cs);
  int in_forms_menu =
      (cs->active_cat == CANVAS_CAT_SHAPES && cs->show_sub_menu);

  // Close sub-menu if clicking elsewhere
  if (cs->mouse_just_pressed && !in_toolbar) {
    cs->show_sub_menu = 0;
  }

  // --- 2. Handle pending element creation ---
  if (cs->pending_create > 0) {
    float center_world_x = cam_x + (bounds.w / 2.0f) / zoom;
    float center_world_y = cam_y + (bounds.h / 2.0f) / zoom;
    Node *new_node = NULL;
    float def_w = 100.0f, def_h = 80.0f;
    float nx = center_world_x - def_w / 2.0f,
          ny = center_world_y - def_h / 2.0f;

    if (cs->pending_create == 1) {
      // Body text
      new_node = canvas_create_styled_node(cd, NODE_STYLE_TEXT, nx, ny, 200.0f,
                                           40.0f, cs->selected_color);
      if (new_node) {
        strncpy(new_node->text, "Body text", sizeof(new_node->text) - 1);
      }
    } else if (cs->pending_create == 6) {
      // H1 Heading
      new_node = canvas_create_styled_node(cd, NODE_STYLE_TEXT, nx, ny, 300.0f,
                                           60.0f, cs->selected_color);
      if (new_node) {
        strncpy(new_node->text, "Heading", sizeof(new_node->text) - 1);
      }
    } else if (cs->pending_create == 2) {
      new_node = canvas_create_styled_node(cd, NODE_STYLE_RECT, nx, ny, 120.0f,
                                           120.0f, cs->selected_color);
      if (new_node) {
        new_node->fill_color = 0xFFF9C4FF;
        new_node->stroke_color = 0xFBC02DFF;
        new_node->type = NODE_TYPE_TEXT;
        strncpy(new_node->text, "Note...", sizeof(new_node->text) - 1);
      }
    } else if (cs->pending_create == 3) {
      char path[512] = {0};
      if (open_image_file_dialog(path, sizeof(path))) {
        char rel[256] = {0};
        if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
          new_node = (Node *)calloc(1, sizeof(Node));
          if (new_node && cd->root) {
            new_node->id = cd->next_node_id++;
            new_node->type = NODE_TYPE_IMAGE;
            new_node->x = nx;
            new_node->y = ny;

            fflush(stdout);

            // Maintain aspect ratio
            int img_w = 0, img_h = 0;
            int size_ok = mac_get_image_size(path, &img_w, &img_h);
            printf("[DEBUG] canvas image size for '%s': %d x %d (ok=%d)\n",
                   path, img_w, img_h, size_ok);

            if (size_ok && img_w > 0 && img_h > 0) {
              float aspect = (float)img_w / (float)img_h;
              if (aspect > 1.0f) {
                new_node->width = 300.0f;
                new_node->height = 300.0f / aspect;
              } else {
                new_node->height = 300.0f;
                new_node->width = 300.0f * aspect;
              }
            } else {
              new_node->width = 200.0f;
              new_node->height = 150.0f;
            }

            new_node->visible = 1;
            new_node->parent = cd->root;
            strncpy(new_node->image_path, rel,
                    sizeof(new_node->image_path) - 1);
            printf("[DEBUG] set image_path to: %s\n", new_node->image_path);
            if (!cd->root->first_child)
              cd->root->first_child = new_node;
            else {
              Node *l = cd->root->first_child;
              while (l->next_sibling)
                l = l->next_sibling;
              l->next_sibling = new_node;
            }
          }
        }
      }
    } else if (cs->pending_create == 4) {
      new_node = canvas_create_styled_node(cd, NODE_STYLE_RECT, nx, ny, 200.0f,
                                           120.0f, cs->selected_color);
      if (new_node)
        new_node->type = NODE_TYPE_IMAGE;
    } else if (cs->pending_create >= 10 && cs->pending_create <= 13) {
      NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
      new_node =
          canvas_create_styled_node(cd, styles[cs->pending_create - 10], nx, ny,
                                    def_w, def_h, cs->selected_color);
    }
    if (new_node)
      ui->selected_node = new_node;
    cs->pending_create = 0;
  }

  // --- 3. Resize and Select logic ---
  int on_resize_handle = 0, hit_handle_index = 0, on_prop_menu = 0;
  if (ui->selected_node && ui->selected_node->visible) {
    Node *sel = ui->selected_node;
    float sx = bounds.x + (sel->x - cam_x) * zoom,
          sy = bounds.y + (sel->y - cam_y) * zoom;
    float sw = sel->width * zoom, sh = sel->height * zoom, hsize = 40.0f;
    if (ui->mouse_x >= sx + sw - hsize / 2 &&
        ui->mouse_x < sx + sw + hsize / 2 &&
        ui->mouse_y >= sy + sh - hsize / 2 &&
        ui->mouse_y < sy + sh + hsize / 2) {
      on_resize_handle = 1;
      hit_handle_index = 0;
    } else if (ui->mouse_x >= sx - hsize / 2 && ui->mouse_x < sx + hsize / 2 &&
               ui->mouse_y >= sy + sh - hsize / 2 &&
               ui->mouse_y < sy + sh + hsize / 2) {
      on_resize_handle = 1;
      hit_handle_index = 1;
    } else if (ui->mouse_x >= sx - hsize / 2 && ui->mouse_x < sx + hsize / 2 &&
               ui->mouse_y >= sy - hsize / 2 && ui->mouse_y < sy + hsize / 2) {
      on_resize_handle = 1;
      hit_handle_index = 2;
    } else if (ui->mouse_x >= sx + sw - hsize / 2 &&
               ui->mouse_x < sx + sw + hsize / 2 &&
               ui->mouse_y >= sy - hsize / 2 && ui->mouse_y < sy + hsize / 2) {
      on_resize_handle = 1;
      hit_handle_index = 3;
    }

    float mw = 220.0f, mh = 45.0f, mx = sx, my = sy - mh - 10.0f;
    if (mx < bounds.x)
      mx = bounds.x;
    if (mx + mw > bounds.x + bounds.w)
      mx = bounds.x + bounds.w - mw;
    if (my < bounds.y)
      my = sy + sh + 10.0f;
    if (ui->mouse_x >= mx && ui->mouse_x < mx + mw && ui->mouse_y >= my &&
        ui->mouse_y < my + mh)
      on_prop_menu = 1;
  }

  if (cs->mouse_in_canvas && !cs->is_panning && cs->mouse_just_pressed) {
    // 1. Prioritize Bezier handle selection
    int handle_hit = 0;
    if (cs->bezier_mode && ui->selected_node &&
        ui->selected_node->type == NODE_TYPE_PATH) {
      Node *sel = ui->selected_node;
      float nsx = bounds.x + (sel->x - cam_x) * zoom;
      float nsy = bounds.y + (sel->y - cam_y) * zoom;
      for (int i = 0; i < sel->path_point_count; i++) {
        NodePathPoint *p = &sel->path_points[i];
        float px = nsx + p->x * zoom, py = nsy + p->y * zoom;
        struct nk_rect r_anchor =
            nk_rect(px - 10 * zoom, py - 10 * zoom, 20 * zoom, 20 * zoom);
        if (nk_input_is_mouse_hovering_rect(&ctx->input, r_anchor)) {
          cs->dragging_point_idx = i;
          cs->dragging_cp_idx = 0;
          handle_hit = 1;
          break;
        }
        if (p->type == 1) {
          float cp1x = nsx + p->cp1x * zoom, cp1y = nsy + p->cp1y * zoom;
          float cp2x = nsx + p->cp2x * zoom, cp2y = nsy + p->cp2y * zoom;
          struct nk_rect r1 =
              nk_rect(cp1x - 8 * zoom, cp1y - 8 * zoom, 16 * zoom, 16 * zoom);
          struct nk_rect r2 =
              nk_rect(cp2x - 8 * zoom, cp2y - 8 * zoom, 16 * zoom, 16 * zoom);
          if (nk_input_is_mouse_hovering_rect(&ctx->input, r1)) {
            cs->dragging_point_idx = i;
            cs->dragging_cp_idx = 1;
            handle_hit = 1;
            break;
          }
          if (nk_input_is_mouse_hovering_rect(&ctx->input, r2)) {
            cs->dragging_point_idx = i;
            cs->dragging_cp_idx = 2;
            handle_hit = 1;
            break;
          }
        }
      }
    }

    if (handle_hit) {
      // Bezier handle hit, don't drag node
    } else if (on_resize_handle) {
      cs->is_resizing = 1;
      cs->resize_handle = hit_handle_index;
      cs->resize_start_w = ui->selected_node->width;
      cs->resize_start_h = ui->selected_node->height;
      cs->resize_start_x = ui->mouse_x;
      cs->resize_start_y = ui->mouse_y;
      cs->resize_start_node_x = ui->selected_node->x;
      cs->resize_start_node_y = ui->selected_node->y;
    } else if (!in_toolbar && !in_forms_menu) {
      float wx = cam_x + (ui->mouse_x - bounds.x) / zoom;
      float wy = cam_y + (ui->mouse_y - bounds.y) / zoom;
      Node *hit = canvas_find_node_at(cd, wx, wy);
      if (hit) {
        if (hit->type == NODE_TYPE_TEXT && ui->selected_node == hit &&
            !ui->editing_node) {
          ui->editing_node = hit;
          strncpy(ui->edit_buffer, hit->text, sizeof(ui->edit_buffer) - 1);
        } else {
          // If we were in pen mode, deselect it when clicking a node
          if (cs->tool == CANVAS_TOOL_PEN) {
            cs->tool = CANVAS_TOOL_SELECT;
            cs->active_cat = CANVAS_CAT_SELECT;
            printf("[DEBUG] Auto-deselected pen tool due to node click\n");
          }
          ui->selected_node = hit;
          cs->is_dragging_node = 1;
          cs->drag_offset_x = wx - hit->x;
          cs->drag_offset_y = wy - hit->y;
        }
      } else {
        // Clicked on background - start panning automatically
        ui->selected_node = NULL;
        if (ui->editing_node) {
          strncpy(ui->editing_node->text, ui->edit_buffer,
                  sizeof(ui->editing_node->text) - 1);
          ui->editing_node = NULL;
          SDL_StopTextInput();
        }
        // Start pan mode automatically if not in pen tool
        if (cs->tool != CANVAS_TOOL_PEN) {
          cs->is_panning = 1;
          cs->pan_start_x = ui->mouse_x;
          cs->pan_start_y = ui->mouse_y;
          cs->cam_start_x = cs->cam_x;
          cs->cam_start_y = cs->cam_y;
        }
      }
    }
  }

  // Handle Bezier point dragging
  if (cs->bezier_mode && ui->selected_node &&
      ui->selected_node->type == NODE_TYPE_PATH) {
    Node *node = ui->selected_node;
    if (cs->dragging_point_idx != -1 && (ui->mouse_buttons & 1)) {
      float dx = ctx->input.mouse.delta.x / zoom;
      float dy = ctx->input.mouse.delta.y / zoom;
      NodePathPoint *p = &node->path_points[cs->dragging_point_idx];
      if (cs->dragging_cp_idx == 0) {
        p->x += dx;
        p->y += dy;
      } else if (cs->dragging_cp_idx == 1) {
        p->cp1x += dx;
        p->cp1y += dy;
      } else if (cs->dragging_cp_idx == 2) {
        p->cp2x += dx;
        p->cp2y += dy;
      }
      node_update_path_bounds(node);
    } else {
      cs->dragging_point_idx = -1;
    }
  }

  if (cs->is_resizing && ui->selected_node) {
    float dx = (ui->mouse_x - cs->resize_start_x) / zoom,
          dy = (ui->mouse_y - cs->resize_start_y) / zoom;
    float nw = cs->resize_start_w, nh = cs->resize_start_h,
          nx = cs->resize_start_node_x, ny = cs->resize_start_node_y;
    if (cs->resize_handle == 0) {
      nw += dx;
      nh += dy;
    } else if (cs->resize_handle == 1) {
      nw -= dx;
      nh += dy;
      nx += (cs->resize_start_w - nw);
    } else if (cs->resize_handle == 2) {
      nw -= dx;
      nh -= dy;
      nx += (cs->resize_start_w - nw);
      ny += (cs->resize_start_h - nh);
    } else if (cs->resize_handle == 3) {
      nw += dx;
      nh -= dy;
      ny += (cs->resize_start_h - nh);
    }
    if (nw < 20.0f)
      nw = 20.0f;
    if (nh < 20.0f)
      nh = 20.0f;
    ui->selected_node->width = nw;
    ui->selected_node->height = nh;
    ui->selected_node->x = nx;
    ui->selected_node->y = ny;
  }
  if (cs->mouse_just_released) {
    cs->is_resizing = 0;
    cs->is_dragging_node = 0;
  }

  // --- 4. PEN TOOL logic ---
  if (cs->tool == CANVAS_TOOL_PEN && cs->mouse_in_canvas && !in_toolbar &&
      !in_forms_menu && !cs->is_panning && (ui->mouse_buttons & 1)) {
    float wx = cam_x + (ui->mouse_x - bounds.x) / zoom,
          wy = cam_y + (ui->mouse_y - bounds.y) / zoom;
    if (cs->mouse_just_pressed) {
      cs->current_stroke = (Stroke *)calloc(1, sizeof(Stroke));
      cs->current_stroke->capacity = 128;
      cs->current_stroke->points = (Point *)calloc(128, sizeof(Point));
      if (cs->pen_is_marker) {
        cs->current_stroke->thickness = cs->pen_thickness * 10.0f;
        uint32_t c = cs->selected_color;
        uint8_t r = (c >> 24) & 0xFF;
        uint8_t g = (c >> 16) & 0xFF;
        uint8_t b = (c >> 8) & 0xFF;
        uint8_t a = c & 0xFF;
        // Reduce alpha to 30% for marker effect
        cs->current_stroke->color =
            (r << 24) | (g << 16) | (b << 8) | (uint8_t)(a * 0.3f);
      } else {
        cs->current_stroke->thickness = cs->pen_thickness;
        cs->current_stroke->color = cs->selected_color;
      }
      cs->current_stroke->points[0].x = wx;
      cs->current_stroke->points[0].y = wy;
      cs->current_stroke->count = 1;

      // Wrap it in a Node so it appears in Layers
      Node *node = (Node *)calloc(1, sizeof(Node));
      if (node) {
        node->id = cd->next_node_id++;
        node->type = NODE_TYPE_STROKE;
        node->stroke_data = cs->current_stroke;
        node->visible = 1;
        node->parent = cd->root;

        // Add to document tree
        if (!cd->root->first_child) {
          cd->root->first_child = node;
        } else {
          Node *last = cd->root->first_child;
          while (last->next_sibling)
            last = last->next_sibling;
          last->next_sibling = node;
        }
        cs->current_stroke_node = node;

        // Initialize bounds
        node->x = wx;
        node->y = wy;
        node->width = 1.0f;
        node->height = 1.0f;
      }
    } else if (cs->current_stroke) {
      if (cs->current_stroke->count >= cs->current_stroke->capacity) {
        cs->current_stroke->capacity *= 2;
        cs->current_stroke->points =
            (Point *)realloc(cs->current_stroke->points,
                             cs->current_stroke->capacity * sizeof(Point));
      }
      Point last = cs->current_stroke->points[cs->current_stroke->count - 1];
      if ((wx - last.x) * (wx - last.x) + (wy - last.y) * (wy - last.y) >
          4.0f) {
        cs->current_stroke->points[cs->current_stroke->count].x = wx;
        cs->current_stroke->points[cs->current_stroke->count].y = wy;
        cs->current_stroke->count++;

        // Update node bounds for culling
        if (cs->current_stroke_node) {
          float min_x = cs->current_stroke_node->x;
          float min_y = cs->current_stroke_node->y;
          float max_x = min_x + cs->current_stroke_node->width;
          float max_y = min_y + cs->current_stroke_node->height;

          if (wx < min_x)
            min_x = wx;
          if (wy < min_y)
            min_y = wy;
          if (wx > max_x)
            max_x = wx;
          if (wy > max_y)
            max_y = wy;

          cs->current_stroke_node->x = min_x;
          cs->current_stroke_node->y = min_y;
          cs->current_stroke_node->width = max_x - min_x;
          cs->current_stroke_node->height = max_y - min_y;
        }
      }
    }
  } else if (!(ui->mouse_buttons & 1))
    cs->current_stroke = NULL;

  // --- 5. RENDER ---
  if (nk_group_begin(ctx, "canvas_preview", NK_WINDOW_NO_SCROLLBAR)) {
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    nk_fill_rect(canvas_buf, bounds, 0, nk_rgb(255, 255, 255));
    canvas_draw_dot_grid(canvas_buf, bounds, cam_x, cam_y, zoom);
    nk_push_scissor(canvas_buf, bounds);

    // Nodes (including strokes)
    for (Node *node = cd->root->first_child; node; node = node->next_sibling) {
      if (!node->visible)
        continue;
      float sx = bounds.x + (node->x - cam_x) * zoom,
            sy = bounds.y + (node->y - cam_y) * zoom;
      float sw = node->width * zoom, sh = node->height * zoom;
      if (sx + sw < bounds.x || sx > bounds.x + bounds.w ||
          sy + sh < bounds.y || sy > bounds.y + bounds.h)
        continue;
      struct nk_rect nr = nk_rect(sx, sy, sw, sh);
      struct nk_color fill = nk_rgba(
          (node->fill_color >> 24) & 0xFF, (node->fill_color >> 16) & 0xFF,
          (node->fill_color >> 8) & 0xFF, node->fill_color & 0xFF);
      struct nk_color stroke = nk_rgba(
          (node->stroke_color >> 24) & 0xFF, (node->stroke_color >> 16) & 0xFF,
          (node->stroke_color >> 8) & 0xFF, node->stroke_color & 0xFF);

      int drawn_image = 0;
      if (node->is_container && node->image_path[0]) {
        SDL_Texture *tex = get_image_texture(ui, node->image_path);
        if (tex) {
          struct nk_image img = nk_image_ptr(tex);
          nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
          drawn_image = 1;
        }
      }

      if (node->type == NODE_TYPE_STROKE && node->stroke_data) {
        Stroke *s = node->stroke_data;
        if (s->count >= 2) {
          struct nk_color sc =
              nk_rgba((s->color >> 24) & 0xFF, (s->color >> 16) & 0xFF,
                      (s->color >> 8) & 0xFF, s->color & 0xFF);
          struct nk_vec2 *pts =
              (struct nk_vec2 *)malloc(s->count * sizeof(struct nk_vec2));
          for (int i = 0; i < s->count; i++) {
            pts[i].x = bounds.x + (s->points[i].x - cam_x) * zoom;
            pts[i].y = bounds.y + (s->points[i].y - cam_y) * zoom;
          }
          nk_stroke_polyline(canvas_buf, (float *)pts, s->count,
                             s->thickness * zoom, sc);
          free(pts);
        }
      } else if (node->type == NODE_TYPE_RECT) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, cbg);
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        if (!drawn_image) {
          nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_ELLIPSE) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          nk_fill_circle(canvas_buf, nr, cbg);
          nk_stroke_circle(canvas_buf, nr, 1.0f, cst);
          // Draw ICON_ADD in center
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        if (!drawn_image) {
          nk_fill_circle(canvas_buf, nr, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_circle(canvas_buf, nr, node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_TRIANGLE) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          struct nk_vec2 vpts[3];
          vpts[0] = nk_vec2(nr.x + nr.w / 2, nr.y);
          vpts[1] = nk_vec2(nr.x, nr.y + nr.h);
          vpts[2] = nk_vec2(nr.x + nr.w, nr.y + nr.h);
          nk_fill_polygon(canvas_buf, (const float *)vpts, 3, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vpts, 3, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 5, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        struct nk_vec2 pts[3];
        pts[0] = nk_vec2(nr.x + nr.w / 2, nr.y);
        pts[1] = nk_vec2(nr.x, nr.y + nr.h);
        pts[2] = nk_vec2(nr.x + nr.w, nr.y + nr.h);
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)pts, 3, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)pts, 3,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_PENTAGON) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          struct nk_vec2 vpts[5];
          for (int i = 0; i < 5; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
            vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          nk_fill_polygon(canvas_buf, (const float *)vpts, 5, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vpts, 5, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        struct nk_vec2 pts[5];
        for (int i = 0; i < 5; i++) {
          float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
          pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
          pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
        }
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)pts, 5, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)pts, 5,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_HEXAGON) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          struct nk_vec2 vpts[6];
          for (int i = 0; i < 6; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 6.0f;
            vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          nk_fill_polygon(canvas_buf, (const float *)vpts, 6, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vpts, 6, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        struct nk_vec2 pts[6];
        for (int i = 0; i < 6; i++) {
          float angle = (float)i * 2.0f * 3.14159f / 6.0f;
          pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
          pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
        }
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)pts, 6, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)pts, 6,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_OCTAGON) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          struct nk_vec2 vpts[8];
          for (int i = 0; i < 8; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
            vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          nk_fill_polygon(canvas_buf, (const float *)vpts, 8, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vpts, 8, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        struct nk_vec2 pts[8];
        for (int i = 0; i < 8; i++) {
          float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
          pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
          pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
        }
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)pts, 8, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)pts, 8,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_HEART) {
        int drawn_image = 0;
        if (node->is_container && node->image_path[0]) {
          SDL_Texture *tex = get_image_texture(ui, node->image_path);
          if (tex) {
            struct nk_image img = nk_image_ptr(tex);
            nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
            drawn_image = 1;
          }
        }
        // Parametric heart: (16sin^3(t), 13cos(t) - 5cos(2t) - 2cos(3t) -
        // cos(4t))
        const int heart_pts_count = 48;
        struct nk_vec2 hpts[48];
        for (int i = 0; i < heart_pts_count; i++) {
          float t = (float)i * 2.0f * 3.14159f / (float)heart_pts_count;
          float hx = 16.0f * powf(sinf(t), 3.0f);
          float hy = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                       2.0f * cosf(3.0f * t) - cosf(4.0f * t));
          // Normalize to (-16, 16) and (-13, 17) approx, then scale to nr
          hpts[i].x = nr.x + nr.w / 2.0f + (hx / 32.0f) * nr.w;
          hpts[i].y = nr.y + nr.h * 0.45f + (hy / 32.0f) * nr.h;
        }
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)hpts, heart_pts_count,
                          fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)hpts, heart_pts_count,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_PATH && node->path_points &&
                 node->path_point_count > 1) {
        // Fill path
        if (fill.a > 0) {
          float poly_pts[512 * 2];
          int poly_count =
              node_get_path_polygon(node, poly_pts, 512, sx, sy, zoom);
          if (poly_count > 2) {
            nk_fill_polygon(canvas_buf, poly_pts, poly_count, fill);
          }
        }

        // Stroke path
        for (int i = 0; i < node->path_point_count; i++) {
          int next = (i + 1) % node->path_point_count;
          NodePathPoint p1 = node->path_points[i];
          NodePathPoint p2 = node->path_points[next];

          if (p1.type == 1) { // Cubic Bezier
            nk_stroke_curve(canvas_buf, sx + p1.x * zoom, sy + p1.y * zoom,
                            sx + p1.cp2x * zoom, sy + p1.cp2y * zoom,
                            sx + p2.cp1x * zoom, sy + p2.cp1y * zoom,
                            sx + p2.x * zoom, sy + p2.y * zoom,
                            node->stroke_width * zoom, stroke);
          } else {
            nk_stroke_line(canvas_buf, sx + p1.x * zoom, sy + p1.y * zoom,
                           sx + p2.x * zoom, sy + p2.y * zoom,
                           node->stroke_width * zoom, stroke);
          }
        }
      } else if (node->type == NODE_TYPE_IMAGE) {
        SDL_Texture *tex = get_image_texture(ui, node->image_path);
        if (tex) {
          struct nk_image img = nk_image_ptr(tex);
          nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
        } else
          nk_fill_rect(canvas_buf, nr, 0, nk_rgb(200, 200, 200));

        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_TEXT) {
        // Render background for sticky notes / text with bg
        if (fill.a > 0)
          nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, fill);

        const char *display_text = node->text;
        int is_editing = (ui->editing_node == node);
        if (is_editing)
          display_text = ui->edit_buffer;

        if (display_text && strlen(display_text) > 0) {
          struct nk_user_font *font =
              ui->font_inter ? &ui->font_inter->handle : ctx->style.font;

          // Select font based on type and size
          if (node->font_type == 4 && ui->font_mono) { // Mono
            font = &ui->font_mono->handle;
          } else { // Inter variants (Body, H1, H2, H3)
            // Find closest size in font_inter_sizes
            float target_size = node->font_size;
            float best_diff = 1000.0f;
            int best_idx = -1;
            float available_sizes[] = {12.0f, 14.0f, 16.0f, 20.0f, 24.0f,
                                       32.0f, 48.0f, 64.0f, 96.0f};

            for (int i = 0; i < 9; i++) {
              float diff = fabsf(available_sizes[i] - target_size);
              if (diff < best_diff) {
                best_diff = diff;
                best_idx = i;
              }
            }

            if (best_idx >= 0 && ui->font_inter_sizes[best_idx]) {
              font = &ui->font_inter_sizes[best_idx]->handle;
            } else if (ui->font_inter) {
              font = &ui->font_inter->handle;
            }
          }

          struct nk_color txt_color = {(node->stroke_color >> 24) & 0xFF,
                                       (node->stroke_color >> 16) & 0xFF,
                                       (node->stroke_color >> 8) & 0xFF,
                                       node->stroke_color & 0xFF};

          nk_draw_text(canvas_buf, nr, display_text, strlen(display_text), font,
                       nk_rgba(0, 0, 0, 0), txt_color);

          if (is_editing) {
            float tw = font->width(font->userdata, font->height, display_text,
                                   strlen(display_text));
            int show_cursor = ((int)(SDL_GetTicks() / 500) % 2 == 0);
            if (show_cursor) {
              nk_stroke_line(canvas_buf, nr.x + tw + 1, nr.y + 2, nr.x + tw + 1,
                             nr.y + nr.h - 2, 1.5f * zoom,
                             nk_rgb(66, 133, 244));
            }
          }
        }

        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
      } else {
        nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, fill);
        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
      }

      // Removed global unconditional stroke here
      if (ui->selected_node == node && !cs->bezier_mode) {
        float sel_off = 2.0f + (node->stroke_width * zoom) / 2.0f;
        nk_stroke_rect(canvas_buf,
                       nk_rect(nr.x - sel_off, nr.y - sel_off,
                               nr.w + sel_off * 2, nr.h + sel_off * 2),
                       0, 2.0f, nk_rgb(66, 133, 244));
      }

      // Bezier handles interaction and rendering
      // Show handles for ALL path nodes when bezier mode is active
      if (cs->bezier_mode && node->type == NODE_TYPE_PATH) {
        for (int i = 0; i < node->path_point_count; i++) {
          NodePathPoint *p = &node->path_points[i];
          float px = sx + p->x * zoom;
          float py = sy + p->y * zoom;

          // Anchor
          struct nk_rect anchor_rect =
              nk_rect(px - 5 * zoom, py - 5 * zoom, 10 * zoom, 10 * zoom);
          nk_fill_rect(canvas_buf, anchor_rect, 0, nk_rgb(255, 255, 255));
          nk_stroke_rect(canvas_buf, anchor_rect, 0, 1.5f * zoom,
                         nk_rgb(66, 133, 244));

          if (cs->mouse_just_pressed &&
              nk_input_is_mouse_hovering_rect(&ctx->input, anchor_rect)) {
            cs->dragging_point_idx = i;
            cs->dragging_cp_idx = 0;
          }

          if (p->type == 1) { // Cubic
            float cp1x = sx + p->cp1x * zoom;
            float cp1y = sy + p->cp1y * zoom;
            float cp2x = sx + p->cp2x * zoom;
            float cp2y = sy + p->cp2y * zoom;

            nk_stroke_line(canvas_buf, px, py, cp1x, cp1y, 1,
                           nk_rgb(200, 200, 200));
            nk_stroke_line(canvas_buf, px, py, cp2x, cp2y, 1,
                           nk_rgb(200, 200, 200));

            struct nk_rect r1 =
                nk_rect(cp1x - 4 * zoom, cp1y - 4 * zoom, 8 * zoom, 8 * zoom);
            struct nk_rect r2 =
                nk_rect(cp2x - 4 * zoom, cp2y - 4 * zoom, 8 * zoom, 8 * zoom);
            nk_fill_circle(canvas_buf, r1, nk_rgb(66, 133, 244));
            nk_fill_circle(canvas_buf, r2, nk_rgb(66, 133, 244));

            if (cs->mouse_just_pressed &&
                nk_input_is_mouse_hovering_rect(&ctx->input, r1)) {
              cs->dragging_point_idx = i;
              cs->dragging_cp_idx = 1;
            }
            if (cs->mouse_just_pressed &&
                nk_input_is_mouse_hovering_rect(&ctx->input, r2)) {
              cs->dragging_point_idx = i;
              cs->dragging_cp_idx = 2;
            }
          }
        }
      }
    }

    nk_push_scissor(canvas_buf, nk_rect(0, 0, 100000, 100000));
    canvas_render_toolbar(ui, ctx, bounds, block);
    nk_group_end(ctx);
  }

  // Right sidebar (only in fullscreen mode)
  if (is_expanded) {
    if (nk_group_begin(ctx, "canvas_sidebar",
                       NK_WINDOW_BORDER | NK_WINDOW_SCROLL_AUTO_HIDE)) {
      if (ui->sidebar_mode == 1) {
        ui_render_sidebar_shape_selection(ui, ctx);
      } else {
        // Properties section
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Properties", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);
        ui_render_sidebar_properties(ui, ctx, block);

        // Spacing between sections
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_spacing(ctx, 1);

        // Layers section
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Layers", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);
        ui_render_sidebar_layers(ui, ctx, block);
      }

      nk_group_end(ctx);
    }
  }

  // Update canvas view state
  cd->view_x = cs->cam_x;
  cd->view_y = cs->cam_y;
  cd->zoom = cs->zoom;
  nk_spacing(ctx, 1);
}

// -----------------------------------------------------------------------------
// Design Block Helper: Find node at position
// -----------------------------------------------------------------------------

static Node *design_find_node_at(DesignData *dd, float x, float y) {
  if (!dd->root)
    return NULL;

  Node *hit = NULL;
  for (Node *node = dd->root->first_child; node; node = node->next_sibling) {
    if (!node->visible)
      continue;
    if (x >= node->x && x < node->x + node->width && y >= node->y &&
        y < node->y + node->height) {
      hit = node;
    }
  }
  return hit;
}

void ui_render_block_design(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
  (void)engine;
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;

  // Initialize pages array if needed (for backwards compatibility)
  if (!dd->pages || dd->page_count == 0) {
    dd->page_capacity = 4;
    dd->pages = (DesignPage *)calloc(dd->page_capacity, sizeof(DesignPage));
    dd->page_count = 1;
    dd->current_page = 0;
    dd->pages[0].root = (Node *)calloc(1, sizeof(Node));
    if (dd->pages[0].root) {
      dd->pages[0].root->type = NODE_TYPE_ROOT;
      dd->pages[0].root->visible = 1;
    }
    dd->pages[0].bg_color = dd->bg_color;
    dd->root = dd->pages[0].root;
  }

  // Ensure root points to current page
  if (dd->current_page >= 0 && dd->current_page < dd->page_count) {
    dd->root = dd->pages[dd->current_page].root;
  }

  ui_render_expand_toggle(ui, ctx, block);

  // Calculate scale to fit design in display width
  float scale = width / (float)dd->width;
  if (scale > 1.0f)
    scale = 1.0f;

  float display_h = (float)dd->height * scale;
  float toolbar_h = 52.0f;

  // Limit height unless expanded
  if (ui->expanded_block_id != block->id) {
    if (display_h > 400.0f)
      display_h = 400.0f;
  } else {
    display_h = nk_window_get_height(ctx) - 80.0f;
    scale = display_h / (float)dd->height;
    if (scale > 1.0f)
      scale = 1.0f;
  }

  // Include toolbar in layout height - 2 columns in fullscreen, 1 column
  // otherwise
  int is_fullscreen = (ui->expanded_block_id == block->id);
  if (is_fullscreen) {
    // Fullscreen: Design + Sidebar layout
    nk_layout_row_template_begin(ctx, display_h + toolbar_h + 20.0f);
    nk_layout_row_template_push_variable(ctx, 75); // Design (flexible)
    nk_layout_row_template_push_static(ctx,
                                       250.0f); // Sidebar (fixed 250px)
    nk_layout_row_template_end(ctx);
  } else {
    // Normal mode: Single column
    nk_layout_row_dynamic(ctx, display_h + toolbar_h + 20.0f, 1);
  }

  // Get bounds BEFORE opening the group (like Canvas does)
  struct nk_rect bounds = nk_widget_bounds(ctx);

  // Wrap all Design content in a group (like Canvas)
  if (nk_group_begin(ctx, "design_content", NK_WINDOW_NO_SCROLLBAR)) {
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);

    // Adjust bounds for actual design area (above toolbar)
    bounds.h = display_h;

    // Design state shorthand
    DesignInteraction *ds = &ui->design_state;

    // Sync bounds and scale for interactions (like file drop)
    ds->bounds_x = bounds.x;
    ds->bounds_y = bounds.y;
    ds->bounds_w = bounds.w;
    ds->bounds_h = bounds.h;
    ds->scale = scale;

    // Track if this design is active
    int is_active_design = (ds->active_design_id == block->id);
    if (!is_active_design && ui->mouse_x >= bounds.x &&
        ui->mouse_x < bounds.x + bounds.w && ui->mouse_y >= bounds.y &&
        ui->mouse_y < bounds.y + bounds.h + toolbar_h) {
      // Mouse entered this design
      ds->active_design_id = block->id;
      ds->active_block = block;
      is_active_design = 1;
    }

    // Background
    struct nk_color bg = {(dd->bg_color >> 24) & 0xFF,
                          (dd->bg_color >> 16) & 0xFF,
                          (dd->bg_color >> 8) & 0xFF, dd->bg_color & 0xFF};
    nk_fill_rect(canvas_buf, bounds, 4.0f, bg);
    nk_stroke_rect(canvas_buf, bounds, 4.0f, 1.0f, nk_rgb(200, 200, 200));

    // --- Handle pending element creation ---
    if (is_active_design && ds->pending_create > 0) {
      float nx = (float)dd->width / 2.0f - 50.0f;
      float ny = (float)dd->height / 2.0f - 40.0f;
      Node *new_node = NULL;
      float def_w = 100.0f, def_h = 80.0f;

      if (ds->pending_create == 1) { // Body
        new_node = design_create_styled_node(dd, NODE_STYLE_TEXT, nx, ny,
                                             200.0f, 40.0f, ds->selected_color);
        if (new_node)
          strncpy(new_node->text, "Body text", sizeof(new_node->text) - 1);
      } else if (ds->pending_create == 6) { // H1
        new_node = design_create_styled_node(dd, NODE_STYLE_TEXT, nx, ny,
                                             300.0f, 60.0f, ds->selected_color);
        if (new_node)
          strncpy(new_node->text, "Heading", sizeof(new_node->text) - 1);
      } else if (ds->pending_create == 2) { // Sticky
        new_node = design_create_styled_node(
            dd, NODE_STYLE_RECT, nx, ny, 120.0f, 120.0f, ds->selected_color);
        if (new_node) {
          new_node->fill_color = 0xFFF9C4FF;
          new_node->stroke_color = 0xFBC02DFF;
          new_node->type = NODE_TYPE_TEXT;
          strncpy(new_node->text, "Note...", sizeof(new_node->text) - 1);
        }
      } else if (ds->pending_create >= 10 && ds->pending_create <= 13) {
        NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                  NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
        new_node =
            design_create_styled_node(dd, styles[ds->pending_create - 10], nx,
                                      ny, def_w, def_h, ds->selected_color);
      }

      if (new_node)
        ui->selected_node = new_node;
      ds->pending_create = 0;
    }

    // Handle Bezier point dragging
    if (ds->bezier_mode && ui->selected_node &&
        ui->selected_node->type == NODE_TYPE_PATH) {
      Node *node = ui->selected_node;
      if (ds->dragging_point_idx != -1 && (ui->mouse_buttons & 1)) {
        float dx = ctx->input.mouse.delta.x / scale;
        float dy = ctx->input.mouse.delta.y / scale;
        NodePathPoint *p = &node->path_points[ds->dragging_point_idx];
        if (ds->dragging_cp_idx == 0) {
          p->x += dx;
          p->y += dy;
        } else if (ds->dragging_cp_idx == 1) {
          p->cp1x += dx;
          p->cp1y += dy;
        } else if (ds->dragging_cp_idx == 2) {
          p->cp2x += dx;
          p->cp2y += dy;
        }
        node_update_path_bounds(node);
      } else {
        ds->dragging_point_idx = -1;
      }
    }

    // --- Render nodes ---
    if (dd->root) {
      for (Node *node = dd->root->first_child; node;
           node = node->next_sibling) {
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
        struct nk_color stroke = {(node->stroke_color >> 24) & 0xFF,
                                  (node->stroke_color >> 16) & 0xFF,
                                  (node->stroke_color >> 8) & 0xFF,
                                  node->stroke_color & 0xFF};

        int has_fill = (node->fill_color & 0xFF) > 10;

        int drawn_image = 0;
        if (node->is_container && node->image_path[0]) {
          SDL_Texture *tex = get_image_texture(ui, node->image_path);
          if (tex) {
            struct nk_image img = nk_image_ptr(tex);
            nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
            drawn_image = 1;
          }
        }

        if (node->type == NODE_TYPE_RECT) {
          if (node->is_container && !node->image_path[0]) {
            struct nk_color cbg = nk_rgba(66, 133, 244, 40);
            struct nk_color cst = nk_rgba(66, 133, 244, 255);
            nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, cbg);
            nk_stroke_rect(canvas_buf, nr, node->corner_radius * scale, 1.0f,
                           cst);
            if (ui->font_icons) {
              struct nk_rect ir =
                  nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
              nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                           &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
            }
            drawn_image = 1;
          }
          if (!drawn_image && has_fill) {
            nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, fill);
          }
          if (node->stroke_width > 0)
            nk_stroke_rect(canvas_buf, nr, node->corner_radius * scale,
                           node->stroke_width, stroke);
        } else if (node->type == NODE_TYPE_PATH && node->path_points &&
                   node->path_point_count > 1) {
          // Fill path
          if (fill.a > 0) {
            float poly_pts[512 * 2];
            int poly_count =
                node_get_path_polygon(node, poly_pts, 512, sx, sy, scale);
            if (poly_count > 2) {
              nk_fill_polygon(canvas_buf, poly_pts, poly_count, fill);
            }
          }

          // Stroke path
          for (int k = 0; k < node->path_point_count; k++) {
            int nxt = (k + 1) % node->path_point_count;
            NodePathPoint p1 = node->path_points[k],
                          p2 = node->path_points[nxt];
            if (p1.type == 1) // Cubic Bezier
              nk_stroke_curve(canvas_buf, sx + p1.x * scale, sy + p1.y * scale,
                              sx + p1.cp2x * scale, sy + p1.cp2y * scale,
                              sx + p2.cp1x * scale, sy + p2.cp1y * scale,
                              sx + p2.x * scale, sy + p2.y * scale,
                              node->stroke_width * scale, stroke);
            else
              nk_stroke_line(canvas_buf, sx + p1.x * scale, sy + p1.y * scale,
                             sx + p2.x * scale, sy + p2.y * scale,
                             node->stroke_width * scale, stroke);
          }
        } else if (node->type == NODE_TYPE_ELLIPSE) {
          if (node->is_container && !node->image_path[0]) {
            struct nk_color cbg = nk_rgba(66, 133, 244, 40);
            struct nk_color cst = nk_rgba(66, 133, 244, 255);
            nk_fill_circle(canvas_buf, nr, cbg);
            nk_stroke_circle(canvas_buf, nr, 1.0f, cst);
            if (ui->font_icons) {
              struct nk_rect ir =
                  nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
              nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                           &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
            }
            drawn_image = 1;
          }
          if (!drawn_image && has_fill)
            nk_fill_circle(canvas_buf, nr, fill);
          if (node->stroke_width > 0)
            nk_stroke_circle(canvas_buf, nr, node->stroke_width, stroke);
        } else if (node->type == NODE_TYPE_TRIANGLE) {
          if (node->is_container && !node->image_path[0]) {
            struct nk_color cbg = nk_rgba(66, 133, 244, 40);
            struct nk_color cst = nk_rgba(66, 133, 244, 255);
            float vpts[6] = {sx + sw / 2, sy, sx, sy + sh, sx + sw, sy + sh};
            nk_fill_polygon(canvas_buf, vpts, 3, cbg);
            nk_stroke_polygon(canvas_buf, vpts, 3, 1.0f, cst);
            if (ui->font_icons) {
              struct nk_rect ir =
                  nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 5, 20, 20);
              nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                           &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
            }
            drawn_image = 1;
          }
          float pts[6] = {sx + sw / 2, sy,       // Top
                          sx,          sy + sh,  // Bottom-left
                          sx + sw,     sy + sh}; // Bottom-right
          if (!drawn_image && has_fill)
            nk_fill_polygon(canvas_buf, pts, 3, fill);
          if (node->stroke_width > 0)
            nk_stroke_polygon(canvas_buf, pts, 3, node->stroke_width, stroke);
        } else if (node->type == NODE_TYPE_PENTAGON) {
          if (node->is_container && !node->image_path[0]) {
            struct nk_color cbg = nk_rgba(66, 133, 244, 40);
            struct nk_color cst = nk_rgba(66, 133, 244, 255);
            struct nk_vec2 vpts[5];
            for (int i = 0; i < 5; i++) {
              float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
              vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
              vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
            }
            nk_fill_polygon(canvas_buf, (const float *)vpts, 5, cbg);
            nk_stroke_polygon(canvas_buf, (const float *)vpts, 5, 1.0f, cst);
            if (ui->font_icons) {
              struct nk_rect ir =
                  nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
              nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                           &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
            }
            drawn_image = 1;
          }
          struct nk_vec2 pts[5];
          for (int i = 0; i < 5; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
            pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          if (!drawn_image && has_fill)
            nk_fill_polygon(canvas_buf, (const float *)pts, 5, fill);
          if (node->stroke_width > 0)
            nk_stroke_polygon(canvas_buf, (const float *)pts, 5,
                              node->stroke_width, stroke);
        } else if (node->type == NODE_TYPE_HEXAGON) {
          if (node->is_container && !node->image_path[0]) {
            struct nk_color cbg = nk_rgba(66, 133, 244, 40);
            struct nk_color cst = nk_rgba(66, 133, 244, 255);
            struct nk_vec2 vpts[6];
            for (int i = 0; i < 6; i++) {
              float angle = (float)i * 2.0f * 3.14159f / 6.0f;
              vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
              vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
            }
            nk_fill_polygon(canvas_buf, (const float *)vpts, 6, cbg);
            nk_stroke_polygon(canvas_buf, (const float *)vpts, 6, 1.0f, cst);
            if (ui->font_icons) {
              struct nk_rect ir =
                  nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
              nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                           &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
            }
            drawn_image = 1;
          }
          struct nk_vec2 pts[6];
          for (int i = 0; i < 6; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 6.0f;
            pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          if (!drawn_image && has_fill)
            nk_fill_polygon(canvas_buf, (const float *)pts, 6, fill);
          if (node->stroke_width > 0)
            nk_stroke_polygon(canvas_buf, (const float *)pts, 6,
                              node->stroke_width, stroke);
        } else if (node->type == NODE_TYPE_OCTAGON) {
          if (node->is_container && !node->image_path[0]) {
            struct nk_color cbg = nk_rgba(66, 133, 244, 40);
            struct nk_color cst = nk_rgba(66, 133, 244, 255);
            struct nk_vec2 vpts[8];
            for (int i = 0; i < 8; i++) {
              float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
              vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
              vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
            }
            nk_fill_polygon(canvas_buf, (const float *)vpts, 8, cbg);
            nk_stroke_polygon(canvas_buf, (const float *)vpts, 8, 1.0f, cst);
            if (ui->font_icons) {
              struct nk_rect ir =
                  nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
              nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                           &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
            }
            drawn_image = 1;
          }
          struct nk_vec2 pts[8];
          for (int i = 0; i < 8; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
            pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          if (!drawn_image && has_fill)
            nk_fill_polygon(canvas_buf, (const float *)pts, 8, fill);
          if (node->stroke_width > 0)
            nk_stroke_polygon(canvas_buf, (const float *)pts, 8,
                              node->stroke_width, stroke);
        } else if (node->type == NODE_TYPE_HEART) {
          if (node->is_container && !node->image_path[0]) {
            struct nk_color cbg = nk_rgba(66, 133, 244, 40);
            struct nk_color cst = nk_rgba(66, 133, 244, 255);
            const int vhcount = 48;
            struct nk_vec2 vhpts[48];
            for (int i = 0; i < vhcount; i++) {
              float t = (float)i * 2.0f * 3.14159f / (float)vhcount;
              float hx = 16.0f * powf(sinf(t), 3.0f);
              float hy = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                           2.0f * cosf(3.0f * t) - cosf(4.0f * t));
              vhpts[i].x = nr.x + nr.w / 2.0f + (hx / 32.0f) * nr.w;
              vhpts[i].y = nr.y + nr.h * 0.45f + (hy / 32.0f) * nr.h;
            }
            nk_fill_polygon(canvas_buf, (const float *)vhpts, vhcount, cbg);
            nk_stroke_polygon(canvas_buf, (const float *)vhpts, vhcount, 1.0f,
                              cst);
            if (ui->font_icons) {
              struct nk_rect ir =
                  nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
              nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                           &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
            }
            drawn_image = 1;
          }
          const int hcount = 48;
          struct nk_vec2 hpts[48];
          for (int i = 0; i < hcount; i++) {
            float t = (float)i * 2.0f * 3.14159f / (float)hcount;
            float hx = 16.0f * powf(sinf(t), 3.0f);
            float hy = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                         2.0f * cosf(3.0f * t) - cosf(4.0f * t));
            hpts[i].x = nr.x + nr.w / 2.0f + (hx / 32.0f) * nr.w;
            hpts[i].y = nr.y + nr.h * 0.45f + (hy / 32.0f) * nr.h;
          }
          if (!drawn_image && has_fill)
            nk_fill_polygon(canvas_buf, (const float *)hpts, hcount, fill);
          if (node->stroke_width > 0)
            nk_stroke_polygon(canvas_buf, (const float *)hpts, hcount,
                              node->stroke_width, stroke);
        } else if (node->type == NODE_TYPE_TEXT) {
          // Text node

          // Render background for sticky notes / text with bg
          if (fill.a > 0)
            nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, fill);

          const char *display_text = node->text;
          int is_editing = (ui->editing_node == node);
          if (is_editing) {
            display_text = ui->edit_buffer;
            nk_fill_rect(canvas_buf,
                         nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2), 2.0f,
                         nk_rgba(66, 133, 244, 20));
            nk_stroke_rect(canvas_buf,
                           nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2),
                           2.0f, 1.0f, nk_rgb(66, 133, 244));
          }
          if (display_text && strlen(display_text) > 0) {
            struct nk_user_font *font =
                ui->font_inter ? &ui->font_inter->handle : ctx->style.font;

            // Select font based on type and size
            if (node->font_type == 4 && ui->font_mono) { // Mono
              font = &ui->font_mono->handle;
            } else { // Inter variants (Body, H1, H2, H3)
              // Find closest size in font_inter_sizes
              float target_size = node->font_size;
              float best_diff = 1000.0f;
              int best_idx = -1;
              float available_sizes[] = {12.0f, 14.0f, 16.0f, 20.0f, 24.0f,
                                         32.0f, 48.0f, 64.0f, 96.0f};

              for (int i = 0; i < 9; i++) {
                float diff = fabsf(available_sizes[i] - target_size);
                if (diff < best_diff) {
                  best_diff = diff;
                  best_idx = i;
                }
              }

              if (best_idx >= 0 && ui->font_inter_sizes[best_idx]) {
                font = &ui->font_inter_sizes[best_idx]->handle;
              } else if (ui->font_inter) {
                font = &ui->font_inter->handle;
              }
            }
            struct nk_color txt_color = {(node->stroke_color >> 24) & 0xFF,
                                         (node->stroke_color >> 16) & 0xFF,
                                         (node->stroke_color >> 8) & 0xFF,
                                         node->stroke_color & 0xFF};

            nk_draw_text(canvas_buf, nr, display_text, strlen(display_text),
                         font, nk_rgba(0, 0, 0, 0), txt_color);
            if (is_editing) {
              float tw = font->width(font->userdata, font->height, display_text,
                                     strlen(display_text));
              int show_cursor = ((int)(SDL_GetTicks() / 500) % 2 == 0);
              if (show_cursor) {
                nk_stroke_line(canvas_buf, nr.x + tw + 1, nr.y + 2,
                               nr.x + tw + 1, nr.y + nr.h - 2, 1.5f,
                               nk_rgb(66, 133, 244));
              }
            }
          }
        } else if (node->type == NODE_TYPE_IMAGE) {
          nk_fill_rect(canvas_buf, nr, 4.0f, nk_rgb(240, 240, 240));
          nk_stroke_rect(canvas_buf, nr, 4.0f, 1.0f, nk_rgb(180, 180, 180));
          const char *fname = node->text[0] ? node->text : "Image";
          nk_draw_text(canvas_buf,
                       nk_rect(nr.x + 4, nr.y + nr.h / 2 - 8, nr.w - 8, 16),
                       fname, strlen(fname), ctx->style.font,
                       nk_rgba(0, 0, 0, 0), nk_rgb(100, 100, 100));
        }

        // Selection highlight
        if (node == ui->selected_node && is_active_design && !ds->bezier_mode) {
          float sel_off = 2.0f + (node->stroke_width * scale) / 2.0f;
          nk_stroke_rect(canvas_buf,
                         nk_rect(sx - sel_off, sy - sel_off, sw + sel_off * 2,
                                 sh + sel_off * 2),
                         2.0f, 2.0f, nk_rgb(66, 133, 244));

          // Resize handles
          float hsz = 8.0f;
          struct nk_color hc = nk_rgb(66, 133, 244);

          // Helper for corners accounting for offset
          float h_left = sx - sel_off;
          float h_top = sy - sel_off;
          float h_right = sx + sw + sel_off;
          float h_bottom = sy + sh + sel_off;

          // SE
          nk_fill_rect(canvas_buf,
                       nk_rect(h_right - hsz / 2, h_bottom - hsz / 2, hsz, hsz),
                       2.0f, hc);
          // SW
          nk_fill_rect(canvas_buf,
                       nk_rect(h_left - hsz / 2, h_bottom - hsz / 2, hsz, hsz),
                       2.0f, hc);
          // NW
          nk_fill_rect(canvas_buf,
                       nk_rect(h_left - hsz / 2, h_top - hsz / 2, hsz, hsz),
                       2.0f, hc);
          // NE
          nk_fill_rect(canvas_buf,
                       nk_rect(h_right - hsz / 2, h_top - hsz / 2, hsz, hsz),
                       2.0f, hc);
        }

        // Bezier handles interaction and rendering
        // Show handles for ALL path nodes when bezier mode is active
        if (ds->bezier_mode && node->type == NODE_TYPE_PATH) {
          for (int i = 0; i < node->path_point_count; i++) {
            NodePathPoint *p = &node->path_points[i];
            float px = sx + p->x * scale, py = sy + p->y * scale;
            struct nk_rect anchor_rect =
                nk_rect(px - 5 * scale, py - 5 * scale, 10 * scale, 10 * scale);
            nk_fill_rect(canvas_buf, anchor_rect, 0, nk_rgb(255, 255, 255));
            nk_stroke_rect(canvas_buf, anchor_rect, 0, 1.5f * scale,
                           nk_rgb(66, 133, 244));
            if (ds->mouse_just_pressed &&
                nk_input_is_mouse_hovering_rect(&ctx->input, anchor_rect)) {
              ds->dragging_point_idx = i;
              ds->dragging_cp_idx = 0;
            }
            if (p->type == 1) {
              float cp1x = sx + p->cp1x * scale, cp1y = sy + p->cp1y * scale;
              float cp2x = sx + p->cp2x * scale, cp2y = sy + p->cp2y * scale;
              nk_stroke_line(canvas_buf, px, py, cp1x, cp1y, 1,
                             nk_rgb(200, 200, 200));
              nk_stroke_line(canvas_buf, px, py, cp2x, cp2y, 1,
                             nk_rgb(200, 200, 200));
              struct nk_rect r1 = nk_rect(cp1x - 4 * scale, cp1y - 4 * scale,
                                          8 * scale, 8 * scale);
              struct nk_rect r2 = nk_rect(cp2x - 4 * scale, cp2y - 4 * scale,
                                          8 * scale, 8 * scale);
              nk_fill_circle(canvas_buf, r1, nk_rgb(66, 133, 244));
              nk_fill_circle(canvas_buf, r2, nk_rgb(66, 133, 244));
              if (ds->mouse_just_pressed &&
                  nk_input_is_mouse_hovering_rect(&ctx->input, r1)) {
                ds->dragging_point_idx = i;
                ds->dragging_cp_idx = 1;
              }
              if (ds->mouse_just_pressed &&
                  nk_input_is_mouse_hovering_rect(&ctx->input, r2)) {
                ds->dragging_point_idx = i;
                ds->dragging_cp_idx = 2;
              }
            }
          }
        }
      }
    }

    // --- Toolbar (same as Canvas) ---
    float toolbar_w =
        4 * 36.0f + 3 * 8.0f + 24.0f; // Standard canvas toolbar width
    float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
    float toolbar_y = bounds.y + bounds.h;

    // Render Toolbar using Canvas logic (with state swapping)
    {
      CanvasInteraction temp_cs = ui->canvas_state;

      // Map Design state to Canvas state
      ui->canvas_state.active_cat = ds->active_cat;
      ui->canvas_state.show_sub_menu = ds->show_sub_menu;
      ui->canvas_state.pending_create = ds->pending_create;
      ui->canvas_state.mouse_just_pressed = ds->mouse_just_pressed;
      ui->canvas_state.space_held = ds->space_held;
      ui->canvas_state.pan_from_menu = ds->pan_from_menu;
      ui->canvas_state.bezier_mode = ds->bezier_mode;
      ui->canvas_state.dragging_point_idx = ds->dragging_point_idx;
      ui->canvas_state.dragging_cp_idx = ds->dragging_cp_idx;
      ui->canvas_state.selected_color =
          ds->selected_color; // Sync selected color

      // Check if mouse in toolbar for Design interaction
      canvas_render_toolbar(
          ui, ctx, nk_rect(bounds.x, bounds.y, bounds.w, bounds.h + toolbar_h),
          block);

      // Copy back relevant state
      ds->active_cat = ui->canvas_state.active_cat;
      ds->show_sub_menu = ui->canvas_state.show_sub_menu;
      ds->pending_create = ui->canvas_state.pending_create;
      ds->space_held = ui->canvas_state.space_held;
      ds->pan_from_menu = ui->canvas_state.pan_from_menu;
      ds->bezier_mode = ui->canvas_state.bezier_mode;
      ds->dragging_point_idx = ui->canvas_state.dragging_point_idx;
      ds->dragging_cp_idx = ui->canvas_state.dragging_cp_idx;
      ds->selected_color =
          ui->canvas_state.selected_color; // Sync selected color

      // Restore Canvas state
      ui->canvas_state = temp_cs;
    }

    // --- Page Navigation (right side of toolbar) ---
    {
      float nav_x = toolbar_x + toolbar_w - 140.0f;
      float nav_btn_size = 24.0f;
      float nav_y = toolbar_y + (toolbar_h - nav_btn_size) / 2.0f;

      // Previous page button
      struct nk_rect prev_btn =
          nk_rect(nav_x, nav_y, nav_btn_size, nav_btn_size);
      int prev_hover =
          (ui->mouse_x >= prev_btn.x && ui->mouse_x < prev_btn.x + prev_btn.w &&
           ui->mouse_y >= prev_btn.y && ui->mouse_y < prev_btn.y + prev_btn.h);
      int prev_enabled = (dd->current_page > 0);

      if (prev_hover && prev_enabled)
        nk_fill_rect(canvas_buf, prev_btn, 4.0f, nk_rgba(0, 0, 0, 20));
      nk_draw_text(canvas_buf, nk_rect(prev_btn.x + 6, prev_btn.y + 2, 16, 20),
                   "<", 1, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   prev_enabled ? nk_rgb(60, 60, 60) : nk_rgb(180, 180, 180));

      if (prev_hover && ds->mouse_just_pressed && prev_enabled &&
          is_active_design) {
        block_design_goto_page(block, dd->current_page - 1);
        ui->selected_node = NULL;
      }

      // Page counter (e.g., "1 / 3")
      char page_str[32];
      snprintf(page_str, sizeof(page_str), "%d / %d", dd->current_page + 1,
               dd->page_count);
      float counter_x = nav_x + nav_btn_size + 4.0f;
      float counter_w = 50.0f;
      nk_draw_text(canvas_buf, nk_rect(counter_x, nav_y + 2, counter_w, 20),
                   page_str, strlen(page_str), ctx->style.font,
                   nk_rgba(0, 0, 0, 0), nk_rgb(80, 80, 80));

      // Next page button
      struct nk_rect next_btn =
          nk_rect(counter_x + counter_w, nav_y, nav_btn_size, nav_btn_size);
      int next_hover =
          (ui->mouse_x >= next_btn.x && ui->mouse_x < next_btn.x + next_btn.w &&
           ui->mouse_y >= next_btn.y && ui->mouse_y < next_btn.y + next_btn.h);
      int next_enabled = (dd->current_page < dd->page_count - 1);

      if (next_hover && next_enabled)
        nk_fill_rect(canvas_buf, next_btn, 4.0f, nk_rgba(0, 0, 0, 20));
      nk_draw_text(canvas_buf, nk_rect(next_btn.x + 6, next_btn.y + 2, 16, 20),
                   ">", 1, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   next_enabled ? nk_rgb(60, 60, 60) : nk_rgb(180, 180, 180));

      if (next_hover && ds->mouse_just_pressed && next_enabled &&
          is_active_design) {
        block_design_goto_page(block, dd->current_page + 1);
        ui->selected_node = NULL;
      }

      // Add page button (+)
      struct nk_rect add_btn = nk_rect(next_btn.x + nav_btn_size + 8.0f, nav_y,
                                       nav_btn_size, nav_btn_size);
      int add_hover =
          (ui->mouse_x >= add_btn.x && ui->mouse_x < add_btn.x + add_btn.w &&
           ui->mouse_y >= add_btn.y && ui->mouse_y < add_btn.y + add_btn.h);

      if (add_hover)
        nk_fill_rect(canvas_buf, add_btn, 4.0f, nk_rgba(66, 133, 244, 30));
      nk_draw_text(canvas_buf, nk_rect(add_btn.x + 6, add_btn.y + 2, 16, 20),
                   "+", 1, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(66, 133, 244));

      if (add_hover && ds->mouse_just_pressed && is_active_design) {
        int new_idx = block_design_add_page(block);
        if (new_idx >= 0) {
          block_design_goto_page(block, new_idx);
          ui->selected_node = NULL;
        }
      }
    }

    // Check if in toolbar area (using existing toolbar_x, toolbar_y,
    // toolbar_w)
    int in_toolbar =
        (ui->mouse_x >= toolbar_x && ui->mouse_x < toolbar_x + toolbar_w &&
         ui->mouse_y >= toolbar_y && ui->mouse_y < toolbar_y + toolbar_h);
    int in_forms_menu = ds->show_forms_menu;

    // Check mouse in design area
    int mouse_in_design =
        (ui->mouse_x >= bounds.x && ui->mouse_x < bounds.x + bounds.w &&
         ui->mouse_y >= bounds.y && ui->mouse_y < bounds.y + bounds.h);

    // Explicitly exclude sidebar area in fullscreen
    if (is_fullscreen && ui->mouse_x > nk_window_get_width(ctx) - 250.0f) {
      mouse_in_design = 0;
    }

    // --- Handle resize ---
    int on_resize_handle = 0;
    int hit_handle_index = 0;

    if (ui->selected_node && ui->selected_node->visible && is_active_design) {
      Node *sel = ui->selected_node;
      float sx = bounds.x + sel->x * scale;
      float sy = bounds.y + sel->y * scale;
      float sw = sel->width * scale;
      float sh = sel->height * scale;
      float hsize = 30.0f;

      // SE
      if (ui->mouse_x >= sx + sw - hsize / 2 &&
          ui->mouse_x < sx + sw + hsize / 2 &&
          ui->mouse_y >= sy + sh - hsize / 2 &&
          ui->mouse_y < sy + sh + hsize / 2) {
        on_resize_handle = 1;
        hit_handle_index = 0;
      }
      // SW
      else if (ui->mouse_x >= sx - hsize / 2 && ui->mouse_x < sx + hsize / 2 &&
               ui->mouse_y >= sy + sh - hsize / 2 &&
               ui->mouse_y < sy + sh + hsize / 2) {
        on_resize_handle = 1;
        hit_handle_index = 1;
      }
      // NW
      else if (ui->mouse_x >= sx - hsize / 2 && ui->mouse_x < sx + hsize / 2 &&
               ui->mouse_y >= sy - hsize / 2 && ui->mouse_y < sy + hsize / 2) {
        on_resize_handle = 1;
        hit_handle_index = 2;
      }
      // NE
      else if (ui->mouse_x >= sx + sw - hsize / 2 &&
               ui->mouse_x < sx + sw + hsize / 2 &&
               ui->mouse_y >= sy - hsize / 2 && ui->mouse_y < sy + hsize / 2) {
        on_resize_handle = 1;
        hit_handle_index = 3;
      }
    }

    // Start resize
    if (mouse_in_design && ds->mouse_just_pressed && on_resize_handle &&
        ui->selected_node && is_active_design) {
      ds->is_resizing = 1;
      ds->resize_handle = hit_handle_index;
      ds->resize_start_w = ui->selected_node->width;
      ds->resize_start_h = ui->selected_node->height;
      ds->resize_start_x = ui->mouse_x;
      ds->resize_start_y = ui->mouse_y;
      ds->resize_start_node_x = ui->selected_node->x;
      ds->resize_start_node_y = ui->selected_node->y;
    }
    // Handle resize dragging
    else if (ds->is_resizing && ui->selected_node && is_active_design) {
      float dx = (ui->mouse_x - ds->resize_start_x) / scale;
      float dy = (ui->mouse_y - ds->resize_start_y) / scale;
      Node *sel = ui->selected_node;

      switch (ds->resize_handle) {
      case 0: // SE
        sel->width = fmaxf(20.0f, ds->resize_start_w + dx);
        sel->height = fmaxf(20.0f, ds->resize_start_h + dy);
        break;
      case 1: // SW
        sel->width = fmaxf(20.0f, ds->resize_start_w - dx);
        sel->height = fmaxf(20.0f, ds->resize_start_h + dy);
        sel->x = ds->resize_start_node_x + (ds->resize_start_w - sel->width);
        break;
      case 2: // NW
        sel->width = fmaxf(20.0f, ds->resize_start_w - dx);
        sel->height = fmaxf(20.0f, ds->resize_start_h - dy);
        sel->x = ds->resize_start_node_x + (ds->resize_start_w - sel->width);
        sel->y = ds->resize_start_node_y + (ds->resize_start_h - sel->height);
        break;
      case 3: // NE
        sel->width = fmaxf(20.0f, ds->resize_start_w + dx);
        sel->height = fmaxf(20.0f, ds->resize_start_h - dy);
        sel->y = ds->resize_start_node_y + (ds->resize_start_h - sel->height);
        break;
      }

      if (ds->mouse_just_released) {
        ds->is_resizing = 0;
      }
    }
    // 4. Handle Bezier interaction priority
    int handle_hit = 0;
    if (ds->bezier_mode && ui->selected_node &&
        ui->selected_node->type == NODE_TYPE_PATH && is_active_design) {
      Node *sel = ui->selected_node;
      float nsx = bounds.x + sel->x * scale;
      float nsy = bounds.y + sel->y * scale;
      for (int i = 0; i < sel->path_point_count; i++) {
        NodePathPoint *p = &sel->path_points[i];
        float px = nsx + p->x * scale, py = nsy + p->y * scale;
        struct nk_rect r_anchor =
            nk_rect(px - 10 * scale, py - 10 * scale, 20 * scale, 20 * scale);
        if (nk_input_is_mouse_hovering_rect(&ctx->input, r_anchor)) {
          if (ds->mouse_just_pressed) {
            ds->dragging_point_idx = i;
            ds->dragging_cp_idx = 0;
          }
          handle_hit = 1;
          break;
        }
        if (p->type == 1) {
          float cp1x = nsx + p->cp1x * scale, cp1y = nsy + p->cp1y * scale;
          float cp2x = nsx + p->cp2x * scale, cp2y = nsy + p->cp2y * scale;
          struct nk_rect r1 = nk_rect(cp1x - 8 * scale, cp1y - 8 * scale,
                                      16 * scale, 16 * scale);
          struct nk_rect r2 = nk_rect(cp2x - 8 * scale, cp2y - 8 * scale,
                                      16 * scale, 16 * scale);
          if (nk_input_is_mouse_hovering_rect(&ctx->input, r1)) {
            if (ds->mouse_just_pressed) {
              ds->dragging_point_idx = i;
              ds->dragging_cp_idx = 1;
            }
            handle_hit = 1;
            break;
          }
          if (nk_input_is_mouse_hovering_rect(&ctx->input, r2)) {
            if (ds->mouse_just_pressed) {
              ds->dragging_point_idx = i;
              ds->dragging_cp_idx = 2;
            }
            handle_hit = 1;
            break;
          }
        }
      }
    }

    // Handle click in design area
    if (handle_hit) {
      // Handle hit, skip node selection
    } else if (mouse_in_design && !in_toolbar && !in_forms_menu &&
               ds->mouse_just_pressed && !on_resize_handle &&
               is_active_design) {
      // Convert to design coordinates
      float design_x = (ui->mouse_x - bounds.x) / scale;
      float design_y = (ui->mouse_y - bounds.y) / scale;

      Node *hit = design_find_node_at(dd, design_x, design_y);
      if (hit) {
        // Check for double-click on text node
        if (hit == ui->selected_node && hit->type == NODE_TYPE_TEXT) {
          // Start inline editing
          ui->editing_node = hit;
          strncpy(ui->edit_buffer, hit->text, sizeof(ui->edit_buffer) - 1);
          SDL_StartTextInput();
        }
        ui->selected_node = hit;
        ds->is_dragging_node = 1;
        ds->drag_offset_x = design_x - hit->x;
        ds->drag_offset_y = design_y - hit->y;
      } else {
        ui->selected_node = NULL;
        if (ui->editing_node) {
          strncpy(ui->editing_node->text, ui->edit_buffer,
                  sizeof(ui->editing_node->text) - 1);
          ui->editing_node = NULL;
          SDL_StopTextInput();
        }
      }
    }

    // Handle node dragging
    if (ds->is_dragging_node && ui->selected_node && is_active_design) {
      float design_x = (ui->mouse_x - bounds.x) / scale;
      float design_y = (ui->mouse_y - bounds.y) / scale;
      ui->selected_node->x = design_x - ds->drag_offset_x;
      ui->selected_node->y = design_y - ds->drag_offset_y;

      // Clamp to design bounds
      ui->selected_node->x = fmaxf(
          0, fminf(ui->selected_node->x, dd->width - ui->selected_node->width));
      ui->selected_node->y =
          fmaxf(0, fminf(ui->selected_node->y,
                         dd->height - ui->selected_node->height));

      if (ds->mouse_just_released) {
        ds->is_dragging_node = 0;
      }
    }

    // Close forms menu if clicking elsewhere
    if (ds->mouse_just_pressed && !in_toolbar && !in_forms_menu &&
        is_active_design) {
      ds->show_forms_menu = 0;
    }

    // --- Handle pending element creation ---
    if (ds->pending_create > 0 && is_active_design) {
      float center_x = dd->width / 2.0f - 50.0f;
      float center_y = dd->height / 2.0f - 40.0f;
      Node *new_node = NULL;
      float def_w = 100.0f;
      float def_h = 80.0f;

      if (ds->pending_create == 1) {
        // Text (Paragraph)
        new_node =
            design_create_styled_node(dd, NODE_STYLE_TEXT, center_x, center_y,
                                      200.0f, 40.0f, ds->selected_color);
        if (new_node) {
          strncpy(new_node->text, "Paragraph text", sizeof(new_node->text) - 1);
        }
      } else if (ds->pending_create == 3) {
        // Image - Open file dialog
        char path[512] = {0};
        if (open_image_file_dialog(path, sizeof(path))) {
          char rel[256] = {0};
          if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
            Node *img = (Node *)calloc(1, sizeof(Node));
            if (img && dd->root) {
              img->id = dd->next_node_id++;
              img->type = NODE_TYPE_IMAGE;
              img->x = center_x;
              img->y = center_y;

              // Maintain aspect ratio
              int img_w = 0, img_h = 0;
              int size_ok = mac_get_image_size(path, &img_w, &img_h);
              if (size_ok && img_w > 0 && img_h > 0) {
                float aspect = (float)img_w / (float)img_h;
                if (aspect > 1.0f) {
                  img->width = 300.0f;
                  img->height = 300.0f / aspect;
                } else {
                  img->height = 300.0f;
                  img->width = 300.0f * aspect;
                }
              } else {
                img->width = 200.0f;
                img->height = 150.0f;
              }

              printf(
                  "[DEBUG] design image size for '%s': %.1f x %.1f (ok=%d)\n",
                  path, img->width, img->height, size_ok);
              fflush(stdout);

              img->visible = 1;
              img->parent = dd->root;
              strncpy(img->image_path, rel, sizeof(img->image_path) - 1);
              if (!dd->root->first_child) {
                dd->root->first_child = img;
              } else {
                Node *last = dd->root->first_child;
                while (last->next_sibling)
                  last = last->next_sibling;
                last->next_sibling = img;
              }
              new_node = img;
            }
          }
        }
      } else if (ds->pending_create >= 10 && ds->pending_create <= 13) {
        NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                  NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
        int idx = ds->pending_create - 10;
        if (idx == 3) {
          new_node =
              design_create_styled_node(dd, styles[idx], center_x, center_y,
                                        150.0f, 4.0f, ds->selected_color);
        } else {
          new_node =
              design_create_styled_node(dd, styles[idx], center_x, center_y,
                                        100.0f, 80.0f, ds->selected_color);
        }
      }

      if (new_node) {
        ui->selected_node = new_node;
      }
      ds->pending_create = 0;
    }

    // Close Design content group
    nk_group_end(ctx);
  }

  // Right sidebar (only in fullscreen mode)
  if (is_fullscreen) {
    if (nk_group_begin(ctx, "design_sidebar",
                       NK_WINDOW_BORDER | NK_WINDOW_SCROLL_AUTO_HIDE)) {
      if (ui->sidebar_mode == 1) {
        ui_render_sidebar_shape_selection(ui, ctx);
      } else {
        // Properties section
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Properties", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);
        ui_render_sidebar_properties(ui, ctx, block);

        // Spacing between sections
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_spacing(ctx, 1);

        // Layers section
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Layers", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);
        ui_render_sidebar_layers(ui, ctx, block);
      }

      nk_group_end(ctx);
    }
  }

  nk_spacing(ctx, 1);
}

// -----------------------------------------------------------------------------
// Audio Block - Voice Recording with Transcription
// -----------------------------------------------------------------------------

// SDL audio recording state (using queue-based capture, not callbacks)
static SDL_AudioDeviceID audio_recording_device = 0;
static Block *audio_recording_block = NULL;

// -----------------------------------------------------------------------------
// Lua Host (scripting_run_visual and scripting_load_plugins are in
// lua_host.c)
// -----------------------------------------------------------------------------

// Global pointer to the LuaData of the currently running script, for
// callbacks This is defined in lua_host.c extern LuaData
// *g_current_running_lua;

// This function is defined in lua_host.c
// int scripting_run_visual(lua_State *L, void *lua_data);

void ui_render_block_lua(UIHost *ui, Engine *engine, struct nk_context *ctx,
                         Block *block, float width) {
  if (!block || block->type != BLOCK_LUA)
    return;

  LuaData *ld = &block->data.lua;

  // Header/Toolbar
  nk_layout_row_static(ctx, 24.0f, 32, 4);
  if (ui_icon_button(ui, ld->is_running ? ICON_STOP : ICON_PLAY)) {
    if (ui->L) {
      block_lua_execute(block, ui->L);
    }
  }
  if (ui_icon_button(ui, ICON_DELETE)) {
    block_lua_clear_output(block);
    lua_output_buffer[0] = '\0'; // Clear global buffer too
  }
  // Expand/Collapse Button
  if (ui_icon_button(ui, ui->expanded_block_id == block->id
                             ? ICON_FULLSCREEN_EXIT
                             : ICON_FULLSCREEN)) {
    if (ui->expanded_block_id == block->id) {
      ui->expanded_block_id = 0; // Exit fullscreen
      ui->editing_block_id = 0;
    } else {
      ui->expanded_block_id = block->id; // Enter fullscreen
      ui->editing_block_id = block->id;
    }
  }

  // Visual Mode Toggle
  if (nk_button_label(ctx, ld->is_visual ? "Visual: ON" : "Visual: OFF")) {
    ld->is_visual = !ld->is_visual;
  }

  // --- LAYOUT LOGIC ---
  int is_fullscreen = (ui->expanded_block_id == block->id);
  float total_h = is_fullscreen ? (float)ui->win_height - 100 : 250.0f;

  if (is_fullscreen) {
    // === FULLSCREEN MODE: Stacked (Preview 80%, Editor 20%) ===

    // 1. Top: Preview Area (80%)
    float preview_h = total_h * 0.8f;
    nk_layout_row_dynamic(ctx, preview_h, 1);

    if (nk_group_begin(ctx, "lua_render", NK_WINDOW_BORDER)) {
      struct nk_rect r = nk_window_get_content_region(ctx);
      struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

      // Black background
      nk_fill_rect(canvas, r, 0, nk_rgb(10, 10, 15));

      if (ld->is_visual && ui->L) {
        scripting_run_visual(ui->L, ld);
      } else {
        if (lua_output_buffer[0] != '\0') {
          if (ui->font_mono)
            nk_style_push_font(ctx, &ui->font_mono->handle);
          struct nk_color out_color =
              ld->has_error ? nk_rgb(255, 100, 100) : nk_rgb(100, 255, 150);

          // Use a dynamic row that can accommodate multiple labels or a
          // single wrapped one We use a large enough height or just wrap
          nk_layout_row_dynamic(ctx, 18.0f * 10, 1); // Room for ~10 lines
          nk_label_colored_wrap(ctx, lua_output_buffer, out_color);
          if (ui->font_mono)
            nk_style_pop_font(ctx);
        } else {
          nk_layout_row_dynamic(ctx, 20, 1);
          nk_draw_text(canvas, nk_rect(r.x + 10, r.y + 10, r.w, 20),
                       "Preview Area (Empty)", 20, &ui->font_inter->handle,
                       nk_rgba(0, 0, 0, 0), nk_rgb(70, 70, 80));
        }
      }
      nk_group_end(ctx);
    }

    // 2. Bottom: Editor Area (20%)
    float editor_h = total_h * 0.2f;
    nk_layout_row_dynamic(ctx, editor_h, 1);

    if (nk_group_begin(ctx, "lua_editor_fs", NK_WINDOW_BORDER)) {
      nk_layout_row_dynamic(ctx, editor_h - 10, 1);
      char buf[4096];
      strncpy(buf, ld->code ? ld->code : "", sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';

      if (ui->font_mono)
        nk_style_push_font(ctx, &ui->font_mono->handle);
      nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX | NK_EDIT_MULTILINE, buf,
                                     sizeof(buf) - 1, nk_filter_default);
      if (ui->font_mono)
        nk_style_pop_font(ctx);

      if (strcmp(buf, ld->code ? ld->code : "") != 0) {
        block_lua_set_code(block, buf);
      }
      nk_group_end(ctx);
    }

  } else {
    // === STANDARD MODE: Split (Editor 60%, Preview 40%) ===
    nk_layout_row_begin(ctx, NK_STATIC, total_h, 2);
    nk_layout_row_push(ctx, width * 0.6f);

    // Group for Editor
    if (nk_group_begin(ctx, "lua_editor", NK_WINDOW_NO_SCROLLBAR)) {
      nk_layout_row_dynamic(ctx, total_h - 10, 1);
      char buf[4096];
      strncpy(buf, ld->code ? ld->code : "", sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';

      if (ui->font_mono)
        nk_style_push_font(ctx, &ui->font_mono->handle);
      nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX | NK_EDIT_MULTILINE, buf,
                                     sizeof(buf) - 1, nk_filter_default);
      if (ui->font_mono)
        nk_style_pop_font(ctx);

      if (strcmp(buf, ld->code ? ld->code : "") != 0) {
        block_lua_set_code(block, buf);
      }
      nk_group_end(ctx);
    }

    nk_layout_row_push(ctx, width * 0.35f);
    // Group for Rendering Preview
    if (nk_group_begin(ctx, "lua_render", NK_WINDOW_BORDER)) {
      struct nk_rect r = nk_window_get_content_region(ctx);
      struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

      // Black background
      nk_fill_rect(canvas, r, 0, nk_rgb(10, 10, 15));

      if (ld->is_visual && ui->L) {
        scripting_run_visual(ui->L, ld);
      } else {
        if (lua_output_buffer[0] != '\0') {
          if (ui->font_mono)
            nk_style_push_font(ctx, &ui->font_mono->handle);
          struct nk_color out_color =
              ld->has_error ? nk_rgb(255, 100, 100) : nk_rgb(100, 255, 150);

          nk_layout_row_dynamic(ctx, total_h * 0.7f, 1);
          nk_label_colored_wrap(ctx, lua_output_buffer, out_color);
          if (ui->font_mono)
            nk_style_pop_font(ctx);
        } else {
          nk_draw_text(canvas, nk_rect(r.x + 10, r.y + 10, r.w, 20),
                       "Preview Area", 12, &ui->font_inter->handle,
                       nk_rgba(0, 0, 0, 0), nk_rgb(70, 70, 80));
        }
      }
      nk_group_end(ctx);
    }
    nk_layout_row_end(ctx);
  }

  // --- CONSOLE (Bottom) ---
  if (ld->output && ld->output_len > 0) {
    nk_layout_row_dynamic(ctx, 80.0f, 1);
    if (nk_group_begin(ctx, "lua_console", NK_WINDOW_BORDER)) {
      nk_layout_row_dynamic(ctx, 15.0f, 1);
      if (ui->font_mono)
        nk_style_push_font(ctx, &ui->font_mono->handle);

      struct nk_color out_color =
          ld->has_error ? nk_rgb(255, 100, 100) : nk_rgb(200, 200, 200);

      // Simple multiline display (could be improved with a list)
      nk_label_colored_wrap(ctx, ld->output, out_color);

      if (ui->font_mono)
        nk_style_pop_font(ctx);
      nk_group_end(ctx);
    }
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
    // Sync page name with first H1 block
    if (page && block == page->first_block && block->text && block->text[0]) {
      if (strcmp(page->name, block->text) != 0) {
        strncpy(page->name, block->text, sizeof(page->name) - 1);
        page->name[sizeof(page->name) - 1] = '\0';
      }
    }
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
  default:
    break;
  }
}

// -----------------------------------------------------------------------------
// Theme Application
// -----------------------------------------------------------------------------

static void ui_apply_theme(UIHost *ui) {
  struct nk_context *ctx = ui->ctx;
  struct nk_style *s = &ctx->style;

  if (ui->dark_mode) {
    // Dark theme colors
    struct nk_color bg = nk_rgb(30, 30, 35);
    struct nk_color bg_alt = nk_rgb(40, 40, 48);
    struct nk_color text = nk_rgb(230, 230, 235);
    struct nk_color border = nk_rgb(55, 55, 65);
    struct nk_color button = nk_rgb(50, 50, 58);
    struct nk_color button_hover = nk_rgb(60, 60, 70);
    struct nk_color accent = nk_rgb(55, 120, 250);

    s->window.background = bg;
    s->window.fixed_background = nk_style_item_color(bg);
    s->window.border_color = bg; // Match background to hide borders
    s->window.border = 0;

    s->button.normal = nk_style_item_color(button);
    s->button.hover = nk_style_item_color(button_hover);
    s->button.active = nk_style_item_color(accent);
    s->button.text_normal = text;
    s->button.text_hover = text;
    s->button.text_active = nk_rgb(255, 255, 255);
    s->button.border_color = border;

    s->text.color = text;

    s->edit.normal = nk_style_item_color(bg_alt);
    s->edit.hover = nk_style_item_color(bg_alt);
    s->edit.active = nk_style_item_color(bg_alt);
    s->edit.text_normal = text;
    s->edit.text_hover = text;
    s->edit.text_active = text;
    s->edit.border_color = border;
    s->edit.cursor_normal = text;
    s->edit.cursor_hover = text;
    s->edit.cursor_text_normal = bg;
    s->edit.cursor_text_hover = bg;
  } else {
    // Light theme colors
    struct nk_color bg = nk_rgb(252, 252, 253);
    struct nk_color bg_alt = nk_rgb(255, 255, 255);
    struct nk_color text = nk_rgb(40, 40, 45);
    struct nk_color border = nk_rgb(220, 220, 225);
    struct nk_color button = nk_rgb(245, 245, 248);
    struct nk_color button_hover = nk_rgb(235, 235, 240);
    struct nk_color accent = nk_rgb(55, 120, 250);

    s->window.background = bg;
    s->window.fixed_background = nk_style_item_color(bg);
    s->window.border_color = bg; // Match background to hide borders
    s->window.border = 0;

    s->button.normal = nk_style_item_color(button);
    s->button.hover = nk_style_item_color(button_hover);
    s->button.active = nk_style_item_color(accent);
    s->button.text_normal = text;
    s->button.text_hover = text;
    s->button.text_active = nk_rgb(255, 255, 255);
    s->button.border_color = border;

    s->text.color = text;

    s->edit.normal = nk_style_item_color(bg_alt);
    s->edit.hover = nk_style_item_color(bg_alt);
    s->edit.active = nk_style_item_color(bg_alt);
    s->edit.text_normal = text;
    s->edit.text_hover = text;
    s->edit.text_active = text;
    s->edit.border_color = border;
    s->edit.cursor_normal = text;
    s->edit.cursor_hover = text;
    s->edit.cursor_text_normal = bg_alt;
    s->edit.cursor_text_hover = bg_alt;
  }
}

// -----------------------------------------------------------------------------
// Main Render Function
// -----------------------------------------------------------------------------

void ui_render(UIHost *ui, Engine *engine, int win_w, int win_h) {
  // Update window dims for smart placement
  ui->win_width = win_w;
  ui->win_height = win_h;
  if (!ui || !engine)
    return;

  // Store engine reference for file drop handling
  ui->engine = engine;

  struct nk_context *ctx = ui->ctx;

  // Apply current theme
  ui_apply_theme(ui);

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
    const float bottom_h = 0.0f; // Bottom bar removed
    const float content_h = (float)win_h - top_h - bottom_h;
    const float padding = 40.0f;
    const float content_w = (float)win_w - padding * 2;

    if (nk_begin(ctx, "Content", nk_rect(padding, top_h, content_w, content_h),
                 NK_WINDOW_SCROLL_AUTO_HIDE)) {

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

  // Bottom bar removed in single-page mode

  // Reset per-frame input state (after all processing is done)
  ui->canvas_state.mouse_just_pressed = 0;
  ui->canvas_state.mouse_just_released = 0;
  ui->design_state.mouse_just_pressed = 0;
  ui->design_state.mouse_just_released = 0;

  // Render Nuklear
  nk_sdl_render(NK_ANTI_ALIASING_ON);
}

// -----------------------------------------------------------------------------
// Home Screen (Dashboard)
// -----------------------------------------------------------------------------

// Helper: Create new document with default setup
static void home_create_new_document(UIHost *ui, Engine *engine) {
  // Clear current document state and history
  engine_execute_command(engine, "clear");

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
      // Empty text block - placeholder will show visually
      page_append_block(new_page, txt);
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "page_create %u \"%s\"", new_page->id, name);
    engine_execute_command(engine, cmd);

    // Add to recents if matched to a file (for now just use general log)
    recents_add(&ui->recents, "history.log");
    recents_save(&ui->recents, "recents.log");

    if (h1) {
      snprintf(cmd, sizeof(cmd), "block_create %u %u h1", new_page->id, h1->id);
      engine_execute_command(engine, cmd);
    }
    if (txt) {
      snprintf(cmd, sizeof(cmd), "block_create %u %u text", new_page->id,
               txt->id);
      engine_execute_command(engine, cmd);
    }
  }
}

void ui_render_home(UIHost *ui, Engine *engine, struct nk_context *ctx,
                    int win_w, int win_h) {
  if (!ui || !engine || !ctx)
    return;

  float content_h = (float)win_h - 36.0f;

  // Full-screen background
  if (nk_begin(ctx, "HomeBackground", nk_rect(0, 0, (float)win_w, content_h),
               NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {

    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    struct nk_rect bounds = nk_rect(0, 0, (float)win_w, content_h);

    // =======================================================================
    // CLICK DETECTION (Early for use in items)
    // =======================================================================
    static int home_mouse_was_down = 0;
    int mouse_down = (ui->mouse_buttons & 1);
    int mouse_just_pressed = mouse_down && !home_mouse_was_down;
    home_mouse_was_down = mouse_down;

    // Theme colors
    struct nk_color bg_color =
        ui->dark_mode ? nk_rgb(30, 30, 35) : nk_rgb(252, 252, 253);
    struct nk_color text_primary =
        ui->dark_mode ? nk_rgb(240, 240, 245) : nk_rgb(50, 50, 55);
    struct nk_color text_secondary =
        ui->dark_mode ? nk_rgb(140, 140, 150) : nk_rgb(150, 150, 150);
    struct nk_color text_muted =
        ui->dark_mode ? nk_rgb(100, 100, 110) : nk_rgb(180, 180, 185);
    struct nk_color card_bg =
        ui->dark_mode ? nk_rgb(40, 40, 48) : nk_rgb(248, 248, 250);
    struct nk_color card_border =
        ui->dark_mode ? nk_rgb(55, 55, 65) : nk_rgb(230, 230, 235);
    struct nk_color btn_secondary_bg =
        ui->dark_mode ? nk_rgb(50, 50, 58) : nk_rgb(245, 245, 248);
    struct nk_color btn_secondary_hover =
        ui->dark_mode ? nk_rgb(60, 60, 70) : nk_rgb(235, 235, 240);
    struct nk_color btn_secondary_border =
        ui->dark_mode ? nk_rgb(70, 70, 80) : nk_rgb(210, 210, 215);
    struct nk_color btn_secondary_text =
        ui->dark_mode ? nk_rgb(200, 200, 210) : nk_rgb(80, 80, 85);
    struct nk_color hover_bg =
        ui->dark_mode ? nk_rgb(50, 52, 60) : nk_rgb(245, 247, 250);
    struct nk_color accent = nk_rgb(55, 120, 250);
    struct nk_color accent_hover = nk_rgb(45, 105, 220);

    // Background
    nk_fill_rect(canvas, bounds, 0, bg_color);

    // Column dimensions
    float col_left_w = 220.0f;
    float col_right_w = 200.0f;
    float col_center_w = (float)win_w - col_left_w - col_right_w;
    float padding = 40.0f;
    float top_margin = 60.0f;

    // =======================================================================
    // LEFT COLUMN: ACTION
    // =======================================================================
    float left_x = padding;
    float left_y = top_margin;

    // Section title
    nk_draw_text(canvas, nk_rect(left_x, left_y, col_left_w - padding, 20),
                 "ACTION", 6, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 text_secondary);
    left_y += 30;

    // --- New Document Button (Large) ---
    struct nk_rect new_btn = nk_rect(left_x, left_y, col_left_w - padding, 50);
    int new_hover =
        (ui->mouse_x >= new_btn.x && ui->mouse_x < new_btn.x + new_btn.w &&
         ui->mouse_y >= new_btn.y && ui->mouse_y < new_btn.y + new_btn.h);
    nk_fill_rect(canvas, new_btn, 8.0f, new_hover ? accent_hover : accent);
    nk_draw_text(canvas,
                 nk_rect(new_btn.x + 20, new_btn.y + 15, new_btn.w - 40, 20),
                 "+ New Document", 14, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 nk_rgb(255, 255, 255));
    left_y += 60;

    // --- Open Button ---
    struct nk_rect open_btn = nk_rect(left_x, left_y, col_left_w - padding, 40);
    int open_hover =
        (ui->mouse_x >= open_btn.x && ui->mouse_x < open_btn.x + open_btn.w &&
         ui->mouse_y >= open_btn.y && ui->mouse_y < open_btn.y + open_btn.h);
    nk_fill_rect(canvas, open_btn, 6.0f,
                 open_hover ? btn_secondary_hover : btn_secondary_bg);
    nk_stroke_rect(canvas, open_btn, 6.0f, 1.0f, btn_secondary_border);
    nk_draw_text(
        canvas, nk_rect(open_btn.x + 16, open_btn.y + 11, open_btn.w - 32, 18),
        "Open...", 7, ctx->style.font, nk_rgba(0, 0, 0, 0), btn_secondary_text);
    left_y += 60;

    // --- Templates Section ---
    nk_draw_text(canvas, nk_rect(left_x, left_y, col_left_w - padding, 18),
                 "TEMPLATES", 9, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 text_secondary);
    left_y += 28;

    struct nk_rect templates_box =
        nk_rect(left_x, left_y, col_left_w - padding, 100);
    nk_fill_rect(canvas, templates_box, 6.0f, card_bg);
    nk_stroke_rect(canvas, templates_box, 6.0f, 1.0f, card_border);
    nk_draw_text(canvas,
                 nk_rect(templates_box.x + 12, templates_box.y + 40,
                         templates_box.w - 24, 18),
                 "No templates yet", 16, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 text_muted);

    // =======================================================================
    // CENTER COLUMN: HISTORY (Recent Documents)
    // =======================================================================
    float center_x = col_left_w + 20;
    float center_y = top_margin;

    nk_draw_text(canvas, nk_rect(center_x, center_y, col_center_w - 40, 20),
                 "RECENT", 6, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 text_secondary);
    center_y += 35;

    // Recent documents list
    int recent_count = 0;
    for (int i = 0; i < ui->recents.count; i++) {
      const char *path = ui->recents.paths[i];
      const char *filename = strrchr(path, '/');
      filename = filename ? filename + 1 : path;

      struct nk_rect item_rect =
          nk_rect(center_x, center_y, col_center_w - 60, 44);
      int item_hover = (ui->mouse_x >= item_rect.x &&
                        ui->mouse_x < item_rect.x + item_rect.w &&
                        ui->mouse_y >= item_rect.y &&
                        ui->mouse_y < item_rect.y + item_rect.h);

      if (item_hover) {
        nk_fill_rect(canvas, item_rect, 6.0f, hover_bg);
        if (mouse_just_pressed) {
          // TODO: Load this specific file
          // For now, it stays in history.log and just switches to editor
          ui->screen = 1;
        }
      }

      // Document icon (simple rectangle)
      nk_fill_rect(canvas, nk_rect(item_rect.x + 12, item_rect.y + 10, 24, 24),
                   4.0f, accent);

      // Document name
      nk_draw_text(
          canvas,
          nk_rect(item_rect.x + 48, item_rect.y + 13, item_rect.w - 60, 20),
          filename, strlen(filename), ctx->style.font, nk_rgba(0, 0, 0, 0),
          text_primary);

      center_y += 48;
      recent_count++;

      // Max 10 items visible
      if (recent_count >= 10)
        break;
    }

    if (recent_count == 0) {
      nk_draw_text(canvas,
                   nk_rect(center_x + 12, center_y + 20, col_center_w - 60, 20),
                   "No documents yet. Create one!", 29, ctx->style.font,
                   nk_rgba(0, 0, 0, 0), text_secondary);
    }

    // =======================================================================
    // RIGHT COLUMN: APPEARANCE
    // =======================================================================
    float right_x = (float)win_w - col_right_w;
    float right_bottom = content_h - padding;

    // --- Dark/Light Mode Toggle ---
    float toggle_y = right_bottom - 60;
    nk_draw_text(canvas, nk_rect(right_x, toggle_y, col_right_w - padding, 16),
                 "APPEARANCE", 10, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 ui->dark_mode ? nk_rgb(120, 120, 130) : nk_rgb(150, 150, 150));
    toggle_y += 24;

    struct nk_rect light_btn = nk_rect(right_x, toggle_y, 70, 32);
    struct nk_rect dark_btn = nk_rect(right_x + 75, toggle_y, 70, 32);

    int light_hover =
        (ui->mouse_x >= light_btn.x &&
         ui->mouse_x < light_btn.x + light_btn.w &&
         ui->mouse_y >= light_btn.y && ui->mouse_y < light_btn.y + light_btn.h);
    int dark_hover =
        (ui->mouse_x >= dark_btn.x && ui->mouse_x < dark_btn.x + dark_btn.w &&
         ui->mouse_y >= dark_btn.y && ui->mouse_y < dark_btn.y + dark_btn.h);

    // Light button
    struct nk_color light_bg =
        !ui->dark_mode
            ? nk_rgb(55, 120, 250)
            : (light_hover ? nk_rgb(60, 60, 70) : nk_rgb(50, 50, 58));
    struct nk_color light_text =
        !ui->dark_mode
            ? nk_rgb(255, 255, 255)
            : (ui->dark_mode ? nk_rgb(180, 180, 190) : nk_rgb(80, 80, 85));
    nk_fill_rect(canvas, light_btn, 4.0f, light_bg);
    nk_draw_text(canvas, nk_rect(light_btn.x + 14, light_btn.y + 8, 50, 16),
                 "Light", 5, ctx->style.font, nk_rgba(0, 0, 0, 0), light_text);

    // Dark button
    struct nk_color dark_bg =
        ui->dark_mode
            ? nk_rgb(55, 120, 250)
            : (dark_hover ? nk_rgb(235, 235, 240) : nk_rgb(245, 245, 248));
    struct nk_color dark_text =
        ui->dark_mode ? nk_rgb(255, 255, 255) : nk_rgb(80, 80, 85);
    nk_fill_rect(canvas, dark_btn, 4.0f, dark_bg);
    nk_draw_text(canvas, nk_rect(dark_btn.x + 16, dark_btn.y + 8, 50, 16),
                 "Dark", 4, ctx->style.font, nk_rgba(0, 0, 0, 0), dark_text);

    if (mouse_just_pressed) {
      // New Document button
      if (ui->mouse_x >= new_btn.x && ui->mouse_x < new_btn.x + new_btn.w &&
          ui->mouse_y >= new_btn.y && ui->mouse_y < new_btn.y + new_btn.h) {
        home_create_new_document(ui, engine);
      }

      // Open button (TODO: file dialog)
      if (ui->mouse_x >= open_btn.x && ui->mouse_x < open_btn.x + open_btn.w &&
          ui->mouse_y >= open_btn.y && ui->mouse_y < open_btn.y + open_btn.h) {
        // TODO: Open file dialog
      }

      // Light mode button
      if (ui->mouse_x >= light_btn.x &&
          ui->mouse_x < light_btn.x + light_btn.w &&
          ui->mouse_y >= light_btn.y &&
          ui->mouse_y < light_btn.y + light_btn.h) {
        ui->dark_mode = 0;
      }

      // Dark mode button
      if (ui->mouse_x >= dark_btn.x && ui->mouse_x < dark_btn.x + dark_btn.w &&
          ui->mouse_y >= dark_btn.y && ui->mouse_y < dark_btn.y + dark_btn.h) {
        ui->dark_mode = 1;
      }
    }
  }
  nk_end(ctx);
}
