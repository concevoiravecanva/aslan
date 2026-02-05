// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear implementation (must be before nuklear.h include)
#define NK_IMPLEMENTATION
#include "ui_host.h"
#include "../document/aslan_table.h"
#include "../scripting/lua_host.h"
#include "mac_image.h"
#include "ui_canvas.h"
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
int open_image_file_dialog(char *out_path, size_t path_size) {
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
int copy_image_to_assets(const char *src_path, const char *doc_dir,
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
// Colors (exported for use by other UI modules)
// -----------------------------------------------------------------------------

struct nk_color COLOR_BG = {251, 251, 250, 255}; // #FBFBFA
struct nk_color COLOR_PANEL = {251, 251, 250, 255};
struct nk_color COLOR_BORDER = {237, 236, 233, 255}; // #EDECE9
struct nk_color COLOR_TEXT = {220, 220, 220, 255};
struct nk_color COLOR_TEXT_DARK = {55, 53, 47, 255}; // #37352F
struct nk_color COLOR_TEXT_DIM = {140, 140, 140, 255};
struct nk_color COLOR_ACCENT = {80, 140, 200, 255};
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

  // Enable text input globally for Nuklear widgets
  SDL_StartTextInput();

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

  // Initialize scroll state
  ui->scroll_y = 0;
  ui->scroll_target_y = 0;
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
  }
  // Only return if it's a typing event or return/esc/backspace for editing
  if (evt->type == SDL_TEXTINPUT ||
      (evt->type == SDL_KEYDOWN && (evt->key.keysym.sym == SDLK_RETURN ||
                                    evt->key.keysym.sym == SDLK_ESCAPE ||
                                    evt->key.keysym.sym == SDLK_BACKSPACE))) {
    return;
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

  // Handle canvas zoom or page scroll with mouse wheel
  if (evt->type == SDL_MOUSEWHEEL) {
    CanvasInteraction *cs = &ui->canvas_state;
    DesignInteraction *ds = &ui->design_state;

    // Initialize zoom if not set
    if (cs->zoom < 0.01f)
      cs->zoom = 1.0f;

    // Check for modifier keys
    SDL_Keymod mods = SDL_GetModState();
    bool cmd_or_ctrl = (mods & KMOD_GUI) || (mods & KMOD_CTRL);

    if (cs->mouse_in_canvas && cmd_or_ctrl) {
      // Zoom Canvas only when Cmd/Ctrl is held
      float old_zoom = cs->zoom;
      float zoom_factor = 1.1f;
      if (evt->wheel.y > 0)
        cs->zoom *= zoom_factor;
      else if (evt->wheel.y < 0)
        cs->zoom /= zoom_factor;

      if (cs->zoom < 0.1f)
        cs->zoom = 0.1f;
      if (cs->zoom > 5.0f)
        cs->zoom = 5.0f;

      float mouse_rel_x = ui->mouse_x - cs->bounds_x;
      float mouse_rel_y = ui->mouse_y - cs->bounds_y;
      float world_x = cs->cam_x + mouse_rel_x / old_zoom;
      float world_y = cs->cam_y + mouse_rel_y / old_zoom;
      cs->cam_x = world_x - mouse_rel_x / cs->zoom;
      cs->cam_y = world_y - mouse_rel_y / cs->zoom;
    } else {
      // Global Page Scroll (Default behavior)
      float scroll_amount = 60.0f;
      ui->scroll_target_y -= evt->wheel.y * scroll_amount;
      if (ui->scroll_target_y < 0)
        ui->scroll_target_y = 0;
    }
  }
}

// -----------------------------------------------------------------------------
// Top Bar - Block Insertion Menu
// -----------------------------------------------------------------------------

// Helper for icon buttons
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
          block_design_init(b, 800, 400); // Consistent with document.c default
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u design", page_id,
                   b->id);
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

void ui_render_expand_toggle(UIHost *ui, struct nk_context *ctx, Block *block) {
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
      printf("[DEBUG] Clicked Fullscreen EXIT for block %u\n", block->id);
      ui->expanded_block_id = 0;
    }
  } else {
    if (ui_icon_button(ui, ICON_FULLSCREEN)) {
      printf("[DEBUG] Clicked Fullscreen ENTER for block %u\n", block->id);
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

  int is_fullscreen = (ui->expanded_block_id == block->id);

  // --- Layout height calculation ---
  float toolbar_h = 52.0f;
  float display_h = 0;

  if (!is_fullscreen) {
    // Inline mode: scale to fit width, but cap height
    float scale_w = width / (float)dd->width;
    display_h = (float)dd->height * scale_w;
    if (display_h > 400.0f)
      display_h = 400.0f;
  } else {
    // Fullscreen: use window height
    display_h = nk_window_get_height(ctx) - 100.0f;
  }

  // Define layout
  if (is_fullscreen) {
    nk_layout_row_template_begin(ctx, display_h + toolbar_h);
    nk_layout_row_template_push_variable(ctx, 75);   // Workspace
    nk_layout_row_template_push_static(ctx, 250.0f); // Sidebar
    nk_layout_row_template_end(ctx);
  } else {
    nk_layout_row_dynamic(ctx, display_h + toolbar_h, 1);
  }

  // --- Rendering Workspace ---
  if (nk_group_begin(ctx, "design_content_group",
                     NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
    struct nk_rect ws_bounds = nk_window_get_content_region(ctx);
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    DesignInteraction *ds = &ui->design_state;

    // Pre-calculate toolbar flags for interaction logic
    float toolbar_w = 6 * 36.0f + 5 * 8.0f + 24.0f;
    float toolbar_x = ws_bounds.x + (ws_bounds.w - toolbar_w) / 2.0f;
    float toolbar_y = ws_bounds.y + ws_bounds.h - 52.0f - 20.0f; // toolbar_h=52

    int in_toolbar =
        (ui->mouse_x >= toolbar_x && ui->mouse_x < toolbar_x + toolbar_w &&
         ui->mouse_y >= toolbar_y && ui->mouse_y < toolbar_y + 52.0f);
    int in_forms_menu = ds->show_forms_menu;

    // Early check for active state
    int is_active_design = (ds->active_design_id == block->id);

    // Hardened Input Check: Sync Nuklear input state (only if inside workspace)
    if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
        ui->mouse_x >= ws_bounds.x && ui->mouse_x < ws_bounds.x + ws_bounds.w &&
        ui->mouse_y >= ws_bounds.y && ui->mouse_y < ws_bounds.y + ws_bounds.h) {
      ds->mouse_just_pressed = 1;
    }

    // 1. Draw Workspace background (Notion gray)
    nk_fill_rect(canvas_buf, ws_bounds, 0, nk_rgb(241, 241, 239));

    // 2. Calculate Artboard Scale & Centering
    float margin = 20.0f;
    float avail_w = ws_bounds.w - margin * 2;
    float avail_h = ws_bounds.h - margin * 2;

    float scale_w = avail_w / (float)dd->width;
    float scale_h = avail_h / (float)dd->height;
    float scale = (scale_w < scale_h) ? scale_w : scale_h;
    if (scale > 1.0f)
      scale = 1.0f; // Don't upscale past 100%

    float art_w = (float)dd->width * scale;
    float art_h = (float)dd->height * scale;
    float sx = ws_bounds.x + (ws_bounds.w - art_w) / 2.0f;
    float sy = ws_bounds.y + (ws_bounds.h - art_h) / 2.0f;

    // 3. Draw Shadow
    struct nk_rect shadow_rect = nk_rect(sx + 2, sy + 2, art_w, art_h);
    nk_fill_rect(canvas_buf, shadow_rect, 2, nk_rgba(0, 0, 0, 40));

    // 4. Draw Artboard (Page background)
    struct nk_rect art_rect = nk_rect(sx, sy, art_w, art_h);
    nk_fill_rect(canvas_buf, art_rect, 0,
                 nk_rgba((dd->pages[dd->current_page].bg_color >> 24) & 0xFF,
                         (dd->pages[dd->current_page].bg_color >> 16) & 0xFF,
                         (dd->pages[dd->current_page].bg_color >> 8) & 0xFF,
                         dd->pages[dd->current_page].bg_color & 0xFF));
    nk_stroke_rect(canvas_buf, art_rect, 0, 1.0f, nk_rgb(200, 200, 200));

    // 5. Setup Coordinate System and Clipping (Inside space)
    nk_layout_space_begin(ctx, NK_STATIC, ws_bounds.h, 1);
    nk_layout_space_push(
        ctx, nk_rect(sx - ws_bounds.x, sy - ws_bounds.y, art_w, art_h));

    if (nk_group_begin(ctx, "design_content", NK_WINDOW_NO_SCROLLBAR)) {
      struct nk_rect bounds = nk_window_get_content_region(ctx);
      // Coordinate note: bounds.x/y are now absolute artboard screen pos

      // Sync bounds and scale for interactions
      ds->bounds_x = bounds.x;
      ds->bounds_y = bounds.y;
      ds->bounds_w = bounds.w;
      ds->bounds_h = bounds.h;
      ds->scale = scale;

      // Track if this design is active
      // Track if this design is active
      is_active_design = (ds->active_design_id == block->id);

      // Auto-activate if clicked inside WORKSPACE (including toolbar)
      if (!is_active_design &&
          nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
          ui->mouse_x >= ws_bounds.x &&
          ui->mouse_x < ws_bounds.x + ws_bounds.w &&
          ui->mouse_y >= ws_bounds.y &&
          ui->mouse_y < ws_bounds.y + ws_bounds.h) {
        ds->active_design_id = block->id;
        ds->active_block = block;
        ui->editing_block_id = block->id;
        is_active_design = 1;
      }

      // --- Handle pending element creation ---
      if (is_active_design && ds->pending_create > 0) {
        float nx = (float)dd->width / 2.0f - 50.0f;
        float ny = (float)dd->height / 2.0f - 40.0f;
        Node *new_node = NULL;
        float def_w = 100.0f, def_h = 80.0f;

        if (ds->pending_create == 1) { // Body
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 200.0f, 40.0f, ds->selected_color);
          if (new_node)
            strncpy(new_node->text, "Body text", sizeof(new_node->text) - 1);
        } else if (ds->pending_create == 6) { // H1
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 300.0f, 60.0f, ds->selected_color);
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
        } else if (ds->pending_create == 3) { // Image
          char path[512] = {0};
          if (open_image_file_dialog(path, sizeof(path))) {
            char rel[256] = {0};
            if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
              new_node = (Node *)calloc(1, sizeof(Node));
              if (new_node && dd->root) {
                new_node->id = dd->next_node_id++;
                new_node->type = NODE_TYPE_IMAGE;
                new_node->x = nx;
                new_node->y = ny;

                // Maintain aspect ratio
                int img_w = 0, img_h = 0;
                int size_ok = mac_get_image_size(path, &img_w, &img_h);
                printf("[DEBUG] design image size for '%s': %d x %d (ok=%d)\n",
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
                new_node->parent = dd->root;
                strncpy(new_node->image_path, rel,
                        sizeof(new_node->image_path) - 1);

                // Add to tree
                if (!dd->root->first_child)
                  dd->root->first_child = new_node;
                else {
                  Node *l = dd->root->first_child;
                  while (l->next_sibling)
                    l = l->next_sibling;
                  l->next_sibling = new_node;
                }
              }
            }
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

          int drawn_image = 0;
          int has_fill = (node->fill_color & 0xFF) > 10;

          if (node->type == NODE_TYPE_STROKE && node->stroke_data) {
            Stroke *s = node->stroke_data;
            if (s->count > 1) {
              struct nk_color st_col =
                  nk_rgba((s->color >> 24) & 0xFF, (s->color >> 16) & 0xFF,
                          (s->color >> 8) & 0xFF, s->color & 0xFF);
              for (int k = 0; k < s->count - 1; k++) {
                Point p1 = s->points[k], p2 = s->points[k + 1];
                nk_stroke_line(canvas_buf, bounds.x + p1.x * scale,
                               bounds.y + p1.y * scale, bounds.x + p2.x * scale,
                               bounds.y + p2.y * scale, s->thickness * scale,
                               st_col);
              }
            }
            continue;
          }

          if (node->type == NODE_TYPE_IMAGE) {
            int drawn = 0;
            if (node->image_path[0]) {
              SDL_Texture *tex = get_image_texture(ui, node->image_path);
              if (tex) {
                struct nk_image img = nk_image_ptr(tex);
                nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
                drawn = 1;
              }
            }
            if (!drawn) {
              nk_fill_rect(canvas_buf, nr, 0, nk_rgb(200, 200, 200));
            }
            if (node->stroke_width > 0)
              nk_stroke_rect(canvas_buf, nr, node->corner_radius * scale,
                             node->stroke_width * scale, stroke);
          } else if (node->type == NODE_TYPE_RECT) {
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

            // Line Endings (Start/End Styles)
            // Only apply if it looks like a line (thin)
            if (node->height < 8.0f || node->width < 8.0f) {
              float cx = sx + sw / 2.0f;
              float cy = sy + sh / 2.0f;
              float rad = node->rotation * 3.14159f / 180.0f;
              float c = cosf(rad);
              float s = sinf(rad);

              // Calculate geometric start/end relative to center, rotated
              // Assume standard line is horizontal (width > height)
              float half_w = sw / 2.0f;

              // Start point (left)
              float start_x = cx - half_w * c;
              float start_y = cy - half_w * s;

              // End point (right)
              float end_x = cx + half_w * c;
              float end_y = cy + half_w * s;

              float size =
                  (node->stroke_width > 2.0f ? node->stroke_width : 2.0f) *
                  4.0f * scale;

              // Render Start Style
              if (node->line_start_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(start_x - size / 2, start_y - size / 2, size, size),
                    stroke);
              } else if (node->line_start_style ==
                         2) { // Arrow (pointing left/outward)
                // Rotate arrow points
                // Tip at start_x, start_y. Base towards center.
                float angle =
                    rad + 3.14159f; // Pointing away from center (left)
                float ac = cosf(angle);
                float as = sinf(angle);
                float tip_x = start_x; // Tip exactly at geometric start
                float tip_y = start_y;

                // Base points
                float base_angle1 = angle + 2.5f; // ~145 deg
                float base_angle2 = angle - 2.5f;

                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);

                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 stroke);
              }

              // Render End Style
              if (node->line_end_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(end_x - size / 2, end_y - size / 2, size, size),
                    stroke);
              } else if (node->line_end_style ==
                         2) {      // Arrow (pointing right/outward)
                                   // Tip at end_x, end_y. Base towards center.
                float angle = rad; // Pointing away from center (right)
                float ac = cosf(angle);
                float as = sinf(angle);
                float tip_x = end_x; // Tip exactly at geometric end
                float tip_y = end_y;

                // Base points
                float base_angle1 = angle + 2.5f;
                float base_angle2 = angle - 2.5f;

                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);

                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 stroke);
              }
            }
          } else if (node->type == NODE_TYPE_LINE) {
            // Render explicit LINE type as a horizontal stroke through the
            // center of its bounds
            float line_y = nr.y + nr.h / 2.0f;
            float line_w =
                (node->stroke_width > 0 ? node->stroke_width : 2.0f) * scale;
            struct nk_color line_col = stroke;

            // Draw the main line
            nk_stroke_line(canvas_buf, nr.x, line_y, nr.x + nr.w, line_y,
                           line_w, line_col);

            // Line Endings
            float size = (line_w > 2.0f ? line_w : 2.0f) * 4.0f;

            // Start Style (Left)
            if (node->line_start_style > 0) {
              if (node->line_start_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(nr.x - size / 2, line_y - size / 2, size, size),
                    line_col);
              } else if (node->line_start_style == 2) { // Arrow (Left <--)
                nk_fill_triangle(canvas_buf, nr.x, line_y, nr.x + size,
                                 line_y - size / 2, nr.x + size,
                                 line_y + size / 2, line_col);
              }
            }

            // End Style (Right)
            if (node->line_end_style > 0) {
              if (node->line_end_style == 1) { // Circle
                nk_fill_circle(canvas_buf,
                               nk_rect(nr.x + nr.w - size / 2,
                                       line_y - size / 2, size, size),
                               line_col);
              } else if (node->line_end_style == 2) { // Arrow (Right -->)
                nk_fill_triangle(canvas_buf, nr.x + nr.w, line_y,
                                 nr.x + nr.w - size, line_y - size / 2,
                                 nr.x + nr.w - size, line_y + size / 2,
                                 line_col);
              }
            }

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
                nk_stroke_curve(canvas_buf, sx + p1.x * scale,
                                sy + p1.y * scale, sx + p1.cp2x * scale,
                                sy + p1.cp2y * scale, sx + p2.cp1x * scale,
                                sy + p2.cp1y * scale, sx + p2.x * scale,
                                sy + p2.y * scale, node->stroke_width * scale,
                                stroke);
              else
                nk_stroke_line(canvas_buf, sx + p1.x * scale, sy + p1.y * scale,
                               sx + p2.x * scale, sy + p2.y * scale,
                               node->stroke_width * scale, stroke);
            }

            // Line Endings for Paths (only if 2 points)
            if (node->path_point_count == 2) {
              NodePathPoint p1 = node->path_points[0];
              NodePathPoint p2 = node->path_points[1];
              float start_x = sx + p1.x * scale;
              float start_y = sy + p1.y * scale;
              float end_x = sx + p2.x * scale;
              float end_y = sy + p2.y * scale;

              float dx = end_x - start_x;
              float dy = end_y - start_y;
              float angle = atan2f(dy, dx);
              float size =
                  (node->stroke_width > 2.0f ? node->stroke_width : 2.0f) *
                  4.0f * scale;

              // Start Style
              if (node->line_start_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(start_x - size / 2, start_y - size / 2, size, size),
                    stroke);
              } else if (node->line_start_style ==
                         2) { // Arrow (pointing away from line)
                // Tip at start_x, start_y. Pointing away means angle + PI.
                float arrow_angle = angle + 3.14159f;
                float tip_x = start_x; // Tip is exactly at start point
                float tip_y = start_y;

                // Move tip slightly out so it doesn't overlap line end exactly?
                // No, exact is fine. Actually, if we want the arrow tip to be
                // at start_x, we compute base from there.

                float base_angle1 = arrow_angle + 0.5f; // ~28 deg
                float base_angle2 = arrow_angle - 0.5f;

                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);

                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 stroke);
              }

              // End Style
              if (node->line_end_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(end_x - size / 2, end_y - size / 2, size, size),
                    stroke);
              } else if (node->line_end_style == 2) { // Arrow
                float tip_x = end_x;
                float tip_y = end_y;

                float base_angle1 = angle + 3.14159f + 0.5f;
                float base_angle2 = angle + 3.14159f - 0.5f;

                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);

                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 stroke);
              }
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
                float angle =
                    (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
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
                float angle =
                    (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
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
                           nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2),
                           2.0f, nk_rgba(66, 133, 244, 20));
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

              // Auto-resize text node to fit content
              if (display_text && strlen(display_text) > 0) {
                float text_w = font->width(font->userdata, font->height,
                                           display_text, strlen(display_text));
                float text_h = font->height;

                // Only update if dimensions differ significantly (prevent
                // jitter)
                if (fabsf(node->width - (text_w + 10.0f)) > 1.0f ||
                    fabsf(node->height - (text_h + 10.0f)) > 1.0f) {
                  node->width = text_w + 10.0f; // Add padding
                  node->height = text_h + 10.0f;
                }
              }

              struct nk_color txt_color = {(node->stroke_color >> 24) & 0xFF,
                                           (node->stroke_color >> 16) & 0xFF,
                                           (node->stroke_color >> 8) & 0xFF,
                                           node->stroke_color & 0xFF};

              nk_draw_text(canvas_buf, nr, display_text, strlen(display_text),
                           font, nk_rgba(0, 0, 0, 0), txt_color);
              if (is_editing) {
                float tw = font->width(font->userdata, font->height,
                                       display_text, strlen(display_text));
                int show_cursor = ((int)(SDL_GetTicks() / 500) % 2 == 0);
                if (show_cursor) {
                  nk_stroke_line(canvas_buf, nr.x + tw + 1, nr.y + 2,
                                 nr.x + tw + 1, nr.y + nr.h - 2, 1.5f,
                                 nk_rgb(66, 133, 244));
                }
              }
            }

            // Selection highlight
            if (node == ui->selected_node && is_active_design &&
                !ds->bezier_mode) {
              float sel_off = 2.0f + (node->stroke_width * scale) / 2.0f;
              nk_stroke_rect(canvas_buf,
                             nk_rect(sx - sel_off, sy - sel_off,
                                     sw + sel_off * 2, sh + sel_off * 2),
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
              nk_fill_rect(
                  canvas_buf,
                  nk_rect(h_right - hsz / 2, h_bottom - hsz / 2, hsz, hsz),
                  2.0f, hc);
              // SW
              nk_fill_rect(
                  canvas_buf,
                  nk_rect(h_left - hsz / 2, h_bottom - hsz / 2, hsz, hsz), 2.0f,
                  hc);
              // NW
              nk_fill_rect(canvas_buf,
                           nk_rect(h_left - hsz / 2, h_top - hsz / 2, hsz, hsz),
                           2.0f, hc);
              // NE
              nk_fill_rect(
                  canvas_buf,
                  nk_rect(h_right - hsz / 2, h_top - hsz / 2, hsz, hsz), 2.0f,
                  hc);
            }
          }
        }

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

        if (ui->selected_node && ui->selected_node->visible &&
            is_active_design) {
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
          else if (ui->mouse_x >= sx - hsize / 2 &&
                   ui->mouse_x < sx + hsize / 2 &&
                   ui->mouse_y >= sy + sh - hsize / 2 &&
                   ui->mouse_y < sy + sh + hsize / 2) {
            on_resize_handle = 1;
            hit_handle_index = 1;
          }
          // NW
          else if (ui->mouse_x >= sx - hsize / 2 &&
                   ui->mouse_x < sx + hsize / 2 &&
                   ui->mouse_y >= sy - hsize / 2 &&
                   ui->mouse_y < sy + hsize / 2) {
            on_resize_handle = 1;
            hit_handle_index = 2;
          }
          // NE
          else if (ui->mouse_x >= sx + sw - hsize / 2 &&
                   ui->mouse_x < sx + sw + hsize / 2 &&
                   ui->mouse_y >= sy - hsize / 2 &&
                   ui->mouse_y < sy + hsize / 2) {
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
            sel->x =
                ds->resize_start_node_x + (ds->resize_start_w - sel->width);
            break;
          case 2: // NW
            sel->width = fmaxf(20.0f, ds->resize_start_w - dx);
            sel->height = fmaxf(20.0f, ds->resize_start_h - dy);
            sel->x =
                ds->resize_start_node_x + (ds->resize_start_w - sel->width);
            sel->y =
                ds->resize_start_node_y + (ds->resize_start_h - sel->height);
            break;
          case 3: // NE
            sel->width = fmaxf(20.0f, ds->resize_start_w + dx);
            sel->height = fmaxf(20.0f, ds->resize_start_h - dy);
            sel->y =
                ds->resize_start_node_y + (ds->resize_start_h - sel->height);
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
            struct nk_rect r_anchor = nk_rect(px - 10 * scale, py - 10 * scale,
                                              20 * scale, 20 * scale);
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
            // Auto-deselect pen tool when clicking an existing node (same as
            // Canvas)
            if (ds->tool == CANVAS_TOOL_PEN) {
              ds->tool = CANVAS_TOOL_SELECT;
              ds->active_cat = CANVAS_CAT_SELECT;
            }

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
          ui->selected_node->x =
              fmaxf(0, fminf(ui->selected_node->x,
                             dd->width - ui->selected_node->width));
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

        // --- PEN TOOL logic (Adapted for Design) ---
        if (ds->tool == CANVAS_TOOL_PEN && is_active_design && !in_toolbar &&
            !in_forms_menu && ((ui->mouse_buttons & 1))) {
          float wx = (ui->mouse_x - bounds.x) / scale;
          float wy = (ui->mouse_y - bounds.y) / scale;

          if (ds->mouse_just_pressed) {
            ds->current_stroke = (Stroke *)calloc(1, sizeof(Stroke));
            ds->current_stroke->capacity = 128;
            ds->current_stroke->points = (Point *)calloc(128, sizeof(Point));
            if (ds->pen_is_marker) {
              ds->current_stroke->thickness = ds->pen_thickness * 10.0f;
              uint32_t c = ds->selected_color;
              uint8_t r = (c >> 24) & 0xFF;
              uint8_t g = (c >> 16) & 0xFF;
              uint8_t b = (c >> 8) & 0xFF;
              uint8_t a = c & 0xFF;
              // Reduce alpha to 30% for marker effect
              ds->current_stroke->color =
                  (r << 24) | (g << 16) | (b << 8) | (uint8_t)(a * 0.3f);
            } else {
              ds->current_stroke->thickness = ds->pen_thickness;
              ds->current_stroke->color = ds->selected_color;
            }
            ds->current_stroke->points[0].x = wx;
            ds->current_stroke->points[0].y = wy;
            ds->current_stroke->count = 1;

            // Wrap it in a Node so it appears in Layers
            Node *node = (Node *)calloc(1, sizeof(Node));
            if (node) {
              node->id = dd->next_node_id++;
              node->type = NODE_TYPE_STROKE;
              node->stroke_data = ds->current_stroke;
              node->visible = 1;
              node->parent = dd->root;

              // Add to document tree
              if (!dd->root->first_child) {
                dd->root->first_child = node;
              } else {
                Node *last = dd->root->first_child;
                while (last->next_sibling)
                  last = last->next_sibling;
                last->next_sibling = node;
              }
              ds->current_stroke_node = node;
              ds->stroke_block_id = block->id;

              // Initialize bounds
              node->x = wx;
              node->y = wy;
              node->width = 1.0f;
              node->height = 1.0f;
            }
          } else if (ds->current_stroke && ds->stroke_block_id == block->id) {
            if (ds->current_stroke->count >= ds->current_stroke->capacity) {
              ds->current_stroke->capacity *= 2;
              ds->current_stroke->points = (Point *)realloc(
                  ds->current_stroke->points,
                  ds->current_stroke->capacity * sizeof(Point));
            }
            Point last =
                ds->current_stroke->points[ds->current_stroke->count - 1];
            if ((wx - last.x) * (wx - last.x) + (wy - last.y) * (wy - last.y) >
                4.0f) {
              ds->current_stroke->points[ds->current_stroke->count].x = wx;
              ds->current_stroke->points[ds->current_stroke->count].y = wy;
              ds->current_stroke->count++;

              // Update node bounds for culling
              if (ds->current_stroke_node) {
                float min_x = ds->current_stroke_node->x;
                float min_y = ds->current_stroke_node->y;
                float max_x = min_x + ds->current_stroke_node->width;
                float max_y = min_y + ds->current_stroke_node->height;

                if (wx < min_x)
                  min_x = wx;
                if (wy < min_y)
                  min_y = wy;
                if (wx > max_x)
                  max_x = wx;
                if (wy > max_y)
                  max_y = wy;

                ds->current_stroke_node->x = min_x;
                ds->current_stroke_node->y = min_y;
                ds->current_stroke_node->width = max_x - min_x;
                ds->current_stroke_node->height = max_y - min_y;
              }
            }
          }
        } else if (!(ui->mouse_buttons & 1)) {
          ds->current_stroke = NULL;
          ds->stroke_block_id = 0;
        }

        // --- Active Stroke Rendering ---
        if (ds->current_stroke && ds->current_stroke->count > 1 &&
            ds->stroke_block_id == block->id) {
          if (ds->pen_is_marker) {
            for (int i = 0; i < ds->current_stroke->count - 1; i++) {
              Point p1 = ds->current_stroke->points[i];
              Point p2 = ds->current_stroke->points[i + 1];
              struct nk_color col =
                  nk_rgba((ds->current_stroke->color >> 24) & 0xFF,
                          (ds->current_stroke->color >> 16) & 0xFF,
                          (ds->current_stroke->color >> 8) & 0xFF,
                          ds->current_stroke->color & 0xFF);
              nk_stroke_line(canvas_buf, bounds.x + p1.x * scale,
                             bounds.y + p1.y * scale, bounds.x + p2.x * scale,
                             bounds.y + p2.y * scale,
                             ds->current_stroke->thickness * scale, col);
            }
          } else {
            for (int i = 0; i < ds->current_stroke->count - 1; i++) {
              Point p1 = ds->current_stroke->points[i];
              Point p2 = ds->current_stroke->points[i + 1];
              struct nk_color col =
                  nk_rgba((ds->current_stroke->color >> 24) & 0xFF,
                          (ds->current_stroke->color >> 16) & 0xFF,
                          (ds->current_stroke->color >> 8) & 0xFF,
                          ds->current_stroke->color & 0xFF);
              nk_stroke_line(canvas_buf, bounds.x + p1.x * scale,
                             bounds.y + p1.y * scale, bounds.x + p2.x * scale,
                             bounds.y + p2.y * scale,
                             ds->current_stroke->thickness * scale, col);
            }
          }
        }

        // --- Bezier Handles Rendering Overlay ---
        if (ds->bezier_mode && is_active_design && ui->selected_node &&
            ui->selected_node->type == NODE_TYPE_PATH) {
          Node *node = ui->selected_node;
          // Ensure node belongs to this design block
          // (Basic check: if it's selected and we are active, assume it is)
          {
            float sx = bounds.x + node->x * scale;
            float sy = bounds.y + node->y * scale;

            for (int i = 0; i < node->path_point_count; i++) {
              NodePathPoint *p = &node->path_points[i];
              float px = sx + p->x * scale;
              float py = sy + p->y * scale;

              // Anchor
              struct nk_rect anchor_rect = nk_rect(
                  px - 5 * scale, py - 5 * scale, 10 * scale, 10 * scale);
              nk_fill_rect(canvas_buf, anchor_rect, 0, nk_rgb(255, 255, 255));
              nk_stroke_rect(canvas_buf, anchor_rect, 0, 1.5f * scale,
                             nk_rgb(66, 133, 244));

              if (p->type == 1) { // Cubic
                float cp1x = sx + p->cp1x * scale;
                float cp1y = sy + p->cp1y * scale;
                float cp2x = sx + p->cp2x * scale;
                float cp2y = sy + p->cp2y * scale;

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
              }
            }
          }
        }

        // Close Design content group (Artboard)
        nk_group_end(ctx);
      }
      nk_layout_space_end(ctx);

      // --- Toolbar (same as Canvas) ---
      // Rendered in workspace coordinate space, not artboard space

      // Render Toolbar using Canvas logic (with state swapping)
      {
        CanvasInteraction temp_cs = ui->canvas_state;

        // Map Design state to Canvas state
        ui->canvas_state.tool = ds->tool;
        ui->canvas_state.active_cat = ds->active_cat;
        ui->canvas_state.show_sub_menu = ds->show_sub_menu;
        ui->canvas_state.pending_create = ds->pending_create;
        ui->canvas_state.mouse_just_pressed = ds->mouse_just_pressed;
        ui->canvas_state.space_held = ds->space_held;
        ui->canvas_state.pan_from_menu = ds->pan_from_menu;
        ui->canvas_state.bezier_mode = ds->bezier_mode;
        ui->canvas_state.dragging_point_idx = ds->dragging_point_idx;
        ui->canvas_state.dragging_cp_idx = ds->dragging_cp_idx;
        ui->canvas_state.selected_color = ds->selected_color;
        ui->canvas_state.pen_thickness = ds->pen_thickness;
        ui->canvas_state.pen_is_marker = ds->pen_is_marker;
        ui->canvas_state.show_color_picker_popup = ds->show_color_picker_popup;

        // Position toolbar at bottom of workspace
        canvas_render_toolbar(
            ui, ctx,
            nk_rect(ws_bounds.x, ws_bounds.y, ws_bounds.w, ws_bounds.h), block);

        // Copy back relevant state
        ds->tool = ui->canvas_state.tool;
        ds->active_cat = ui->canvas_state.active_cat;
        ds->show_sub_menu = ui->canvas_state.show_sub_menu;
        ds->pending_create = ui->canvas_state.pending_create;
        ds->space_held = ui->canvas_state.space_held;
        ds->pan_from_menu = ui->canvas_state.pan_from_menu;
        ds->bezier_mode = ui->canvas_state.bezier_mode;
        ds->dragging_point_idx = ui->canvas_state.dragging_point_idx;
        ds->dragging_cp_idx = ui->canvas_state.dragging_cp_idx;
        ds->selected_color = ui->canvas_state.selected_color;
        ds->pen_thickness = ui->canvas_state.pen_thickness;
        ds->pen_is_marker = ui->canvas_state.pen_is_marker;
        ds->show_color_picker_popup = ui->canvas_state.show_color_picker_popup;

        // Restore Canvas state
        ui->canvas_state = temp_cs;
      }

      // --- Page Navigation (next to toolbar) ---
      {
        float toolbar_w =
            6 * 36.0f + 5 * 8.0f + 24.0f; // Use standard 6-btn width
        float toolbar_x = ws_bounds.x + (ws_bounds.w - toolbar_w) / 2.0f;
        float toolbar_y = ws_bounds.y + ws_bounds.h - toolbar_h - 20.0f;

        float nav_x = toolbar_x + toolbar_w + 20.0f;
        float nav_btn_size = 24.0f;
        float nav_y = toolbar_y + (toolbar_h - nav_btn_size) / 2.0f;

        struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);

        // Previous page button
        struct nk_rect prev_btn =
            nk_rect(nav_x, nav_y, nav_btn_size, nav_btn_size);
        int prev_hover = (ui->mouse_x >= prev_btn.x &&
                          ui->mouse_x < prev_btn.x + prev_btn.w &&
                          ui->mouse_y >= prev_btn.y &&
                          ui->mouse_y < prev_btn.y + prev_btn.h);
        int prev_enabled = (dd->current_page > 0);

        if (prev_hover && prev_enabled)
          nk_fill_rect(canvas_buf, prev_btn, 4.0f, nk_rgba(0, 0, 0, 20));
        nk_draw_text(canvas_buf,
                     nk_rect(prev_btn.x + 6, prev_btn.y + 2, 16, 20), "<", 1,
                     ctx->style.font, nk_rgba(0, 0, 0, 0),
                     prev_enabled ? nk_rgb(60, 60, 60) : nk_rgb(180, 180, 180));

        if (prev_hover && ds->mouse_just_pressed && prev_enabled &&
            (ds->active_design_id == block->id)) {
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
        int next_hover = (ui->mouse_x >= next_btn.x &&
                          ui->mouse_x < next_btn.x + next_btn.w &&
                          ui->mouse_y >= next_btn.y &&
                          ui->mouse_y < next_btn.y + next_btn.h);
        int next_enabled = (dd->current_page < dd->page_count - 1);

        if (next_hover && next_enabled)
          nk_fill_rect(canvas_buf, next_btn, 4.0f, nk_rgba(0, 0, 0, 20));
        nk_draw_text(canvas_buf,
                     nk_rect(next_btn.x + 6, next_btn.y + 2, 16, 20), ">", 1,
                     ctx->style.font, nk_rgba(0, 0, 0, 0),
                     next_enabled ? nk_rgb(60, 60, 60) : nk_rgb(180, 180, 180));

        if (next_hover && ds->mouse_just_pressed && next_enabled &&
            (ds->active_design_id == block->id)) {
          block_design_goto_page(block, dd->current_page + 1);
          ui->selected_node = NULL;
        }

        // Add page button (+)
        struct nk_rect add_btn = nk_rect(next_btn.x + nav_btn_size + 8.0f,
                                         nav_y, nav_btn_size, nav_btn_size);
        int add_hover =
            (ui->mouse_x >= add_btn.x && ui->mouse_x < add_btn.x + add_btn.w &&
             ui->mouse_y >= add_btn.y && ui->mouse_y < add_btn.y + add_btn.h);

        if (add_hover)
          nk_fill_rect(canvas_buf, add_btn, 4.0f, nk_rgba(66, 133, 244, 30));
        nk_draw_text(canvas_buf, nk_rect(add_btn.x + 6, add_btn.y + 2, 16, 20),
                     "+", 1, ctx->style.font, nk_rgba(0, 0, 0, 0),
                     nk_rgb(66, 133, 244));

        if (add_hover && ds->mouse_just_pressed &&
            (ds->active_design_id == block->id)) {
          int new_idx = block_design_add_page(block);
          if (new_idx >= 0) {
            block_design_goto_page(block, new_idx);
            ui->selected_node = NULL;
          }
        }
      }

      if (ds->mouse_just_pressed &&
          !nk_input_is_mouse_hovering_rect(
              &ctx->input,
              nk_rect(ws_bounds.x, ws_bounds.y + ws_bounds.h - toolbar_h,
                      ws_bounds.w, toolbar_h))) {
        ds->show_forms_menu = 0;
      }

      // Close Workspace group (Column 1)
      nk_group_end(ctx);
    }

    // --- Column 2: Sidebar (only in fullscreen mode) ---
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
          ui_render_sidebar_properties(ui, engine, ctx, block);

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
}

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

  // Top bar (only in editor and not expanded)
  if (ui->screen == 1 && ui->expanded_block_id == 0) {
    ui_render_top_bar(ui, engine, ctx, win_w);
  }

  // Home or Content area
  if (ui->screen == 0) {
    // Hide editor windows when on home screen
    nk_window_show(ctx, "EditorMain", NK_HIDDEN);
    nk_window_show(ctx, "ContentGroup", NK_HIDDEN);
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
    // Normal editor view - needs a window context for layout calls
    // Show editor windows (re-enable after hiding on home)
    nk_window_show(ctx, "EditorMain", NK_SHOWN);
    if (nk_begin(ctx, "EditorMain",
                 nk_rect(0, 40, (float)win_w, (float)win_h - 40),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
      const float padding = 40.0f;
      const float content_h = (float)win_h - 40.0f;
      const float content_w = (float)win_w - (padding * 2);

      // Smooth Scrolling Interpolation
      float scroll_speed = 0.15f;
      if (fabsf(ui->scroll_target_y - ui->scroll_y) > 1.0f) {
        ui->scroll_y += (ui->scroll_target_y - ui->scroll_y) * scroll_speed;
      } else {
        ui->scroll_y = ui->scroll_target_y;
      }

      // Single column layout
      nk_layout_row_dynamic(ctx, content_h - 20, 1);

      // Main Content Area (as a group, not a window)
      if (nk_group_begin(ctx, "ContentGroup", NK_WINDOW_SCROLL_AUTO_HIDE)) {
        // --- Synchronize Scrollbar Interactivity ---
        nk_uint nx, ny;
        nk_group_get_scroll(ctx, "ContentGroup", &nx, &ny);

        // Detect manual scrollbar drag or other internal Nuklear scroll changes
        // If the Nuklear scroll position (ny) differs from our current position
        // (ui->scroll_y) and we are NOT currently in the middle of a smooth
        // scroll interpolation, it means the user likely dragged the scrollbar.
        float scroll_diff = fabsf((float)ny - ui->scroll_y);
        float interpolation_dist = fabsf(ui->scroll_target_y - ui->scroll_y);

        if (scroll_diff > 2.0f && interpolation_dist < 1.1f) {
          // Sync our state to Nuklear's internal state
          ui->scroll_y = (float)ny;
          ui->scroll_target_y = (float)ny;
        }

        // Apply smooth scroll (only if we are actively interpolating)
        if (interpolation_dist > 0.1f) {
          nk_group_set_scroll(ctx, "ContentGroup", 0, (nk_uint)ui->scroll_y);
        }

        Page *page = doc_page_get_active(engine->doc);

        if (page && page->block_count > 0) {
          for (Block *b = page->first_block; b; b = b->next) {
            // Center the content by adding margins inside the render
            ui_render_block(ui, engine, ctx, page, b, content_w - 60);

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
        nk_group_end(ctx);
      }
    }
    nk_end(ctx);

    // 3. Back to Top Button (Floating) - outside EditorMain
    if (ui->scroll_y > 300.0f) {
      struct nk_rect btt_rect =
          nk_rect((float)win_w - 60, (float)win_h - 60, 40, 40);
      if (nk_begin(ctx, "BackToTop", btt_rect,
                   NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(ctx, 30, 1);
        if (nk_button_label(ctx, "^")) {
          ui->scroll_target_y = 0;
        }
      }
      nk_end(ctx);
    }
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
