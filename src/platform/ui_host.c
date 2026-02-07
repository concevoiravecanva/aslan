// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear implementation (must be before nuklear.h include)
#define NK_IMPLEMENTATION
#include "ui_host.h"
// aslan_table.h removed

#include "../scripting/lua_host.h"
// mac_image.h removed

#include "ui_canvas.h"
#include "ui_core.h"
#include "ui_design.h"
#include <SDL.h>
// errno.h removed

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
// ui_icon_button moved to ui_core.c

// ui_render_top_bar moved to ui_core.c

// -----------------------------------------------------------------------------
// Bottom Bar - Page Tabs
// -----------------------------------------------------------------------------

// ui_render_bottom_bar moved to ui_core.c

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

void ui_render_block_text(UIHost *ui, Engine *engine, struct nk_context *ctx,
                          Page *page, Block *block, float width) {
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

  // Capture state
  nk_flags flags = NK_EDIT_FIELD | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD;
  nk_flags state = nk_edit_string_zero_terminated(
      ctx, flags, buf, (int)sizeof(buf) - 1, nk_filter_default);

  if (state & NK_EDIT_ACTIVE) {
    nk_stroke_rect(canvas, nk_rect(bounds.x, bounds.y, bounds.w, bounds.h), 0,
                   1.0f, nk_rgba(55, 120, 250, 80));
  }

  // Draw placeholder
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

// -----------------------------------------------------------------------------
// Block Dispatcher
// -----------------------------------------------------------------------------

void ui_render_block(UIHost *ui, Engine *engine, struct nk_context *ctx,
                     Page *page, Block *block, float width) {
  if (!block)
    return;

  printf("[DEBUG] Rendering block %u (Type: %d)\n", block->id, block->type);
  switch (block->type) {
  case BLOCK_TEXT:
    ui_render_block_text(ui, engine, ctx, page, block, width);
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

// ui_apply_theme moved to ui_core.c

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

  // Top bar removed (now floating at bottom)

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
    if (nk_begin(ctx, "EditorMain", nk_rect(0, 0, (float)win_w, (float)win_h),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
      const float padding = 40.0f;
      const float content_h = (float)win_h;
      const float content_w = (float)win_w - (padding * 2);

      // Smooth Scrolling Interpolation
      float scroll_speed = 0.15f;
      if (fabsf(ui->scroll_target_y - ui->scroll_y) > 1.0f) {
        ui->scroll_y += (ui->scroll_target_y - ui->scroll_y) * scroll_speed;
      } else {
        ui->scroll_y = ui->scroll_target_y;
      }

      // Responsive margin calculation (Notion-like) - Doubled margins
      float target_w = 700.0f;
      float canvas_padding = 80.0f;
      float margin = (content_w - target_w) / 2.0f;
      if (margin < canvas_padding)
        margin = canvas_padding;

      struct nk_vec2 old_group_padding = ctx->style.window.group_padding;
      ctx->style.window.group_padding = nk_vec2(margin, 20.0f);

      // Single column layout
      nk_layout_row_dynamic(ctx, content_h - 100, 1);

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
            ui_render_block(ui, engine, ctx, page, b, content_w);
            // printf("[DEBUG] Render loop: Block %u processed\n", b->id);

            // Small spacing between blocks
            nk_layout_row_dynamic(ctx, 8.0f, 1);
            nk_spacing(ctx, 1);
          }
        } else {
          // Empty state
          nk_layout_row_dynamic(ctx, 100.0f, 1);
          nk_label(ctx, "No blocks yet. Use the toolbar below to add content.",
                   NK_TEXT_CENTERED);
        }
        nk_group_end(ctx);
      }
      ctx->style.window.group_padding = old_group_padding;
      // Render Toolbar inside EditorMain window context
      if (ui->expanded_block_id == 0) {
        ui_render_document_toolbar(ui, engine, ctx, win_w, win_h);
      }
    }
    nk_end(ctx);
  }

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
// home_create_new_document moved to ui_core.c

// ui_render_home moved to ui_core.c
