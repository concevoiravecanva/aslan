// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear implementation (must be before nuklear.h include)
#define NK_IMPLEMENTATION
#include "ui_host.h"
#include "../document/aslan_table.h"

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
// TODO: Enable when video block is implemented
__attribute__((unused)) static int open_video_file_dialog(char *out_path,
                                                          size_t path_size) {
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
// Opens a native file save dialog for CSV
// Returns 1 on success, 0 on cancel/error. Path is written to out_path.
int save_csv_file_dialog(char *out_path, size_t path_size,
                         const char *default_name) {
  char script[1024];
  snprintf(
      script, sizeof(script),
      "osascript -e 'set theName to \"%s\"' "
      "-e 'set theFile to choose file name default name theName default "
      "location (path to desktop folder) with prompt \"Export Table as CSV\"' "
      "-e 'POSIX path of theFile' 2>/dev/null",
      default_name ? default_name : "Export.csv");

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

int copy_image_to_assets(const char *src_path, const char *doc_dir,
                         char *out_rel_path, size_t rel_path_size) {
  // Create assets directory
  char assets_dir[512];
  snprintf(assets_dir, sizeof(assets_dir), "%s/assets", doc_dir);
  mkdir(assets_dir, 0755); // Ignore error if exists

  ASLAN_DEBUG("Copying image: %s -> %s", src_path, assets_dir);

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

struct nk_color COLOR_BG = {255, 255, 255, 255}; // Pure white
struct nk_color COLOR_PANEL = {255, 255, 255, 255};
struct nk_color COLOR_BORDER = {228, 228, 231, 255}; // Subtle border
struct nk_color COLOR_TEXT = {220, 220, 220, 255};
struct nk_color COLOR_TEXT_DARK = {36, 36, 39, 255}; // Near-black
struct nk_color COLOR_TEXT_DIM = {140, 140, 145, 255};
struct nk_color COLOR_ACCENT = {55, 120, 250, 255};
static struct nk_color COLOR_HIGHLIGHT_YELLOW = {255, 240, 100, 60};
static struct nk_color COLOR_HIGHLIGHT_BLUE = {100, 180, 255, 60};
static struct nk_color COLOR_HIGHLIGHT_GREEN = {100, 220, 140, 60};

// -----------------------------------------------------------------------------
// UI Initialization
// -----------------------------------------------------------------------------

UIHost *ui_init(SDL_Window *win, SDL_Renderer *renderer, lua_State *L) {
  UIHost *ui = (UIHost *)calloc(1, sizeof(UIHost));
  if (!ui)
    return NULL;

  ui->L = L;
  ui->screen = 1; // Start in Editor (Skip Dashboard)
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

  ASLAN_INFO("UI Init started (Aslan v0.1.7)");

  // Load fonts
  nk_sdl_font_stash_begin(&ui->atlas);

  // Font paths
  const char *inter_path = "assets/fonts/Inter-Regular.ttf";
  const char *inter_bold_path = "assets/fonts/Inter_28pt-Bold.ttf";
  const char *inter_extrabold_path = "assets/fonts/Inter_28pt-ExtraBold.ttf";
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

  // Load heading fonts (Inter Bold and Extra Bold)
  ui->font_title =
      nk_font_atlas_add_from_file(ui->atlas, inter_extrabold_path, 34.0f, NULL);
  if (!ui->font_title)
    ui->font_title = nk_font_atlas_add_default(ui->atlas, 34.0f, NULL);

  ui->font_h1 =
      nk_font_atlas_add_from_file(ui->atlas, inter_bold_path, 24.0f, NULL);
  if (!ui->font_h1)
    ui->font_h1 = nk_font_atlas_add_default(ui->atlas, 24.0f, NULL);

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
  ASLAN_INFO("Font Inter: %p", (void *)ui->font_inter);
  ASLAN_INFO("Font Mono: %p", (void *)ui->font_mono);
  ASLAN_INFO("Font Icons: %p (path: %s)", (void *)ui->font_icons, icons_path);
  if (!ui->font_icons) {
    ASLAN_ERROR("Failed to load icons font!");
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

void ui_request_redraw(UIHost *ui) {
  if (ui)
    ui->needs_redraw = 1;
}

// -----------------------------------------------------------------------------
// Event Handling
// -----------------------------------------------------------------------------

void ui_host_handle_file_drop(UIHost *ui, const char *file_path, int mx,
                              int my) {
  if (!ui || !file_path)
    return;

  ASLAN_DEBUG("File dropped: %s at (%d, %d)", file_path, mx, my);

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
  ASLAN_DEBUG("Document directory: %s", doc_dir);

  // 1. Try Canvas blocks
  CanvasInteraction *cs = &ui->canvas_state;
  ASLAN_DEBUG("Canvas bounds: x=%.1f y=%.1f w=%.1f h=%.1f", cs->bounds_x,
              cs->bounds_y, cs->bounds_w, cs->bounds_h);

  if (mx >= cs->bounds_x && mx < cs->bounds_x + cs->bounds_w &&
      my >= cs->bounds_y && my < cs->bounds_y + cs->bounds_h) {
    ASLAN_DEBUG("Mouse is in canvas bounds");
    if (cs->active_block && cs->active_block->type == BLOCK_CANVAS) {
      ASLAN_DEBUG("Active canvas block found");
      CanvasData *cd = &cs->active_block->data.canvas;
      float wx = cs->cam_x + (mx - cs->bounds_x) / cs->zoom;
      float wy = cs->cam_y + (my - cs->bounds_y) / cs->zoom;
      ASLAN_DEBUG("World coords: wx=%.1f wy=%.1f (cam: %.1f,%.1f zoom: %.2f)",
                  wx, wy, cs->cam_x, cs->cam_y, cs->zoom);
      Node *hit = canvas_find_node_at(cd, wx, wy);
      if (hit) {
        ASLAN_DEBUG("Hit node found, is_container=%d", hit->is_container);
        if (hit->is_container) {
          char rel[256];
          if (copy_image_to_assets(file_path, doc_dir, rel, sizeof(rel))) {
            strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
            ASLAN_DEBUG("Set container image: %s", hit->image_path);
          } else {
            ASLAN_WARN("Failed to copy image to assets");
          }
          return;
        }
      } else {
        ASLAN_DEBUG("No node hit at world coords");
      }
    } else {
      ASLAN_DEBUG("No active canvas block");
    }
  } else {
    ASLAN_DEBUG("Mouse NOT in canvas bounds");
  }

  // 2. Try Design blocks
  DesignInteraction *ds = &ui->design_state;
  ASLAN_DEBUG("Design bounds: x=%.1f y=%.1f w=%.1f h=%.1f", ds->bounds_x,
              ds->bounds_y, ds->bounds_w, ds->bounds_h);

  if (mx >= ds->bounds_x && mx < ds->bounds_x + ds->bounds_w &&
      my >= ds->bounds_y && my < ds->bounds_y + ds->bounds_h) {
    ASLAN_DEBUG("Mouse is in design bounds");
    if (ds->active_block && ds->active_block->type == BLOCK_DESIGN) {
      ASLAN_DEBUG("Active design block found");
      DesignData *dd = &ds->active_block->data.design;
      float scale = ds->scale;
      float design_x = (mx - ds->bounds_x) / (scale > 0 ? scale : 1.0f);
      float design_y = (my - ds->bounds_y) / (scale > 0 ? scale : 1.0f);
      ASLAN_DEBUG("Design coords: x=%.1f y=%.1f (scale: %.2f)", design_x,
                  design_y, scale);
      Node *hit = design_find_node_at(dd, design_x, design_y);
      if (hit) {
        ASLAN_DEBUG("Hit node found, is_container=%d", hit->is_container);
        if (hit->is_container) {
          char rel[256];
          if (copy_image_to_assets(file_path, doc_dir, rel, sizeof(rel))) {
            strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
            ASLAN_DEBUG("Set container image: %s", hit->image_path);
          } else {
            ASLAN_WARN("Failed to copy image to assets");
          }
          return;
        }
      } else {
        ASLAN_DEBUG("No node hit at design coords");
      }
    } else {
      ASLAN_DEBUG("No active design block");
    }
  } else {
    ASLAN_DEBUG("Mouse NOT in design bounds");
  }

  ASLAN_DEBUG("File drop not handled");
}

void ui_handle_event(UIHost *ui, SDL_Event *evt) {
  if (!ui || !evt)
    return;

  // Log mouse events at debug level (silenced by default)
  if (evt->type == SDL_MOUSEBUTTONDOWN) {
    ASLAN_DEBUG("Button Down: button=%d at (%d, %d)", evt->button.button,
                evt->button.x, evt->button.y);
  } else if (evt->type == SDL_MOUSEBUTTONUP) {
    ASLAN_DEBUG("Button Up: button=%d at (%d, %d)", evt->button.button,
                evt->button.x, evt->button.y);
  }

  // Pass to Nuklear first
  nk_sdl_handle_event(evt);

  // NOTE: mouse_just_pressed/released are reset in ui_render after processing

  // Track mouse position
  if (evt->type == SDL_MOUSEMOTION) {
    ui->mouse_x = (float)evt->motion.x;
    ui->mouse_y = (float)evt->motion.y;

    // Block drag tracking
    if (ui->block_drag.is_active) {
      ui->block_drag.mouse_x = ui->mouse_x;
      ui->block_drag.mouse_y = ui->mouse_y;
      // Dead zone: require 6px movement before committing to drag
      if (!ui->block_drag.threshold_met) {
        float dx = ui->mouse_x - ui->block_drag.start_x;
        float dy = ui->mouse_y - ui->block_drag.start_y;
        if (dx * dx + dy * dy > 36.0f) { // 6px radius
          ui->block_drag.threshold_met = 1;
        }
      }
      if (ui->block_drag.threshold_met) {
        ui->block_drag.drop_valid = 0; // Reset, recalc in render
      }
      ui_request_redraw(ui);
    }

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

    // Handle design panning (no_limits mode)
    DesignInteraction *ds = &ui->design_state;
    if (ds->is_panning) {
      float dx = ui->mouse_x - ds->pan_start_x;
      float dy = ui->mouse_y - ds->pan_start_y;
      float zm = ds->zoom > 0.01f ? ds->zoom : 1.0f;
      ds->cam_x = ds->cam_start_x - dx / zm;
      ds->cam_y = ds->cam_start_y - dy / zm;
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

      ASLAN_DEBUG("SDL click down: (%d, %d)", evt->button.x, evt->button.y);

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

      // Block drag-and-drop: execute drop on mouse release
      if (ui->block_drag.is_active) {
        if (ui->block_drag.threshold_met && ui->block_drag.drop_valid &&
            ui->engine) {
          Page *page = doc_page_get_active(ui->engine->doc);
          if (page) {
            Block *dragged =
                page_find_block_by_id(page, ui->block_drag.dragged_block_id);
            Block *target =
                page_find_block_by_id(page, ui->block_drag.drop_target_id);
            if (dragged && target && dragged != target) {
              int pos = ui->block_drag.drop_position;
              uint16_t target_row = target->layout_row;

              // Remember old row of dragged block for cleanup
              uint16_t old_row = dragged->layout_row;

              // Remove dragged from its current position
              page_remove_block(page, dragged);

              // Check if old row is now empty (no blocks left in that row)
              // If so, compact: decrement all rows above old_row
              int old_row_empty = 1;
              for (Block *r = page->first_block; r; r = r->next) {
                if (r->layout_row == old_row) {
                  old_row_empty = 0;
                  break;
                }
              }
              if (old_row_empty) {
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row > old_row)
                    r->layout_row--;
                }
                // Adjust target_row if it was affected
                if (target_row > old_row)
                  target_row--;
              }

              // Reassign layout_col in old row (may have shifted)
              if (!old_row_empty) {
                uint8_t ci = 0;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == old_row)
                    r->layout_col = ci++;
                }
              }

              if (pos == 0) {
                // Above: insert before first block in target row, own new row
                // Shift target row and everything after
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row >= target_row)
                    r->layout_row++;
                }
                // Find first block in target row (now target_row+1)
                Block *first_in_row = NULL;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == target_row + 1) {
                    first_in_row = r;
                    break;
                  }
                }
                if (first_in_row)
                  page_insert_block_before(page, first_in_row, dragged);
                else
                  page_insert_block_after(page, target, dragged);
                dragged->layout_row = target_row;
                dragged->layout_col = 0;

              } else if (pos == 1) {
                // Below: insert after last block in target row, own new row
                Block *last_in_row = NULL;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == target_row)
                    last_in_row = r;
                }
                uint16_t new_row = target_row + 1;
                // Shift everything after target row
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row >= new_row)
                    r->layout_row++;
                }
                if (last_in_row)
                  page_insert_block_after(page, last_in_row, dragged);
                else
                  page_insert_block_after(page, target, dragged);
                dragged->layout_row = new_row;
                dragged->layout_col = 0;

              } else if (pos == 2) {
                // Left: place in same row as target, before target
                page_insert_block_before(page, target, dragged);
                dragged->layout_row = target_row;
                // Reassign layout_col for all blocks in this row
                uint8_t ci = 0;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == target_row)
                    r->layout_col = ci++;
                }

              } else if (pos == 3) {
                // Right: place in same row as target, after target
                page_insert_block_after(page, target, dragged);
                dragged->layout_row = target_row;
                uint8_t ci = 0;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == target_row)
                    r->layout_col = ci++;
                }
              }
            }
          }
        }
        ui->block_drag.is_active = 0;
        ui->block_drag.threshold_met = 0;
        ui->block_drag.drop_valid = 0;
        ui_request_redraw(ui);
      }

      // Stop panning
      cs->is_panning = 0;

      // Stop dragging node
      cs->is_dragging_node = 0;

      // Stop resizing
      cs->is_resizing = 0;

      // Stop design dragging/resizing/panning
      ui->design_state.is_dragging_node = 0;
      ui->design_state.is_resizing = 0;
      ui->design_state.is_panning = 0;
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
    ASLAN_DEBUG("Spacebar DOWN");
  }
  if (evt->type == SDL_KEYUP && evt->key.keysym.sym == SDLK_SPACE) {
    // Only clear space_held if pan wasn't activated from menu
    if (!ui->canvas_state.pan_from_menu) {
      ui->canvas_state.space_held = 0;
    }
    ui->canvas_state.is_panning = 0;
    ASLAN_DEBUG("Spacebar UP");
  }

  // Inline text editing keyboard handling (Freeform-style)
  if (ui->editing_node) {
    if (evt->type == SDL_KEYDOWN) {
      if (evt->key.keysym.sym == SDLK_RETURN ||
          evt->key.keysym.sym == SDLK_KP_ENTER) {
        strncpy(ui->editing_node->text, ui->edit_buffer,
                sizeof(ui->editing_node->text) - 1);
        ui->editing_node->text[sizeof(ui->editing_node->text) - 1] = '\0';
        ui->editing_node = NULL;
        SDL_StopTextInput();
      } else if (evt->key.keysym.sym == SDLK_ESCAPE) {
        ui->editing_node = NULL;
        SDL_StopTextInput();
      } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
        size_t len = strlen(ui->edit_buffer);
        if (len > 0)
          ui->edit_buffer[len - 1] = '\0';
      }
    } else if (evt->type == SDL_TEXTINPUT) {
      size_t len = strlen(ui->edit_buffer);
      size_t input_len = strlen(evt->text.text);
      if (len + input_len < sizeof(ui->edit_buffer) - 1)
        strcat(ui->edit_buffer, evt->text.text);
    }
    return; // Consume all keyboard events while editing text
  }

  // --- Canvas/Design keyboard shortcuts (only when NOT editing text) ---
  if (evt->type == SDL_KEYDOWN) {
    SDL_Keymod mods = SDL_GetModState();
    int cmd_held = (mods & KMOD_GUI) || (mods & KMOD_CTRL);
    int shift_held = (mods & KMOD_SHIFT);
    SDL_Keycode key = evt->key.keysym.sym;

    // Determine active block (canvas or design)
    Block *active_block = NULL;
    int is_canvas = 0, is_design = 0;
    if (ui->canvas_state.active_block &&
        ui->canvas_state.active_canvas_id != 0) {
      active_block = ui->canvas_state.active_block;
      is_canvas = 1;
    } else if (ui->design_state.active_block &&
               ui->design_state.active_design_id != 0) {
      active_block = ui->design_state.active_block;
      is_design = 1;
    }

    // --- Escape: close table popups first ---
    if (key == SDLK_ESCAPE) {
      if (ui->table_state.show_col_menu ||
          ui->table_state.show_col_settings ||
          ui->table_state.show_footer_menu ||
          ui->table_state.show_select_popup ||
          ui->table_state.show_date_picker ||
          ui->table_state.show_formula_help) {
        ui->table_state.show_col_menu = 0;
        ui->table_state.show_col_settings = 0;
        ui->table_state.show_footer_menu = 0;
        ui->table_state.show_select_popup = 0;
        ui->table_state.show_date_picker = 0;
        ui->table_state.show_formula_help = 0;
        return;
      }
      if (ui->selected_node) {
        ui->selected_node = NULL;
      }
      if (is_canvas) {
        ui->canvas_state.bezier_mode = 0;
        ui->canvas_state.tool = CANVAS_TOOL_SELECT;
        ui->canvas_state.active_cat = CANVAS_CAT_SELECT;
        ui->canvas_state.show_sub_menu = 0;
      }
      if (is_design) {
        ui->design_state.bezier_mode = 0;
        ui->design_state.tool = CANVAS_TOOL_SELECT;
        ui->design_state.active_cat = CANVAS_CAT_SELECT;
        ui->design_state.show_sub_menu = 0;
      }
      return;
    }

    // --- Delete / Backspace: delete selected node ---
    if ((key == SDLK_DELETE || key == SDLK_BACKSPACE) && ui->selected_node &&
        active_block) {
      uint32_t node_id = ui->selected_node->id;
      if (is_canvas)
        block_canvas_delete_node(active_block, node_id);
      else if (is_design)
        block_design_delete_node(active_block, node_id);
      ui->selected_node = NULL;
      return;
    }

    // --- Arrow keys: nudge selected node ---
    if ((key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT ||
         key == SDLK_RIGHT) &&
        ui->selected_node) {
      float step = shift_held ? 10.0f : 1.0f;
      if (key == SDLK_UP)
        ui->selected_node->y -= step;
      else if (key == SDLK_DOWN)
        ui->selected_node->y += step;
      else if (key == SDLK_LEFT)
        ui->selected_node->x -= step;
      else if (key == SDLK_RIGHT)
        ui->selected_node->x += step;

      // Clamp for design (finite artboard)
      if (is_design && active_block) {
        DesignData *dd = &active_block->data.design;
        if (ui->selected_node->x < 0)
          ui->selected_node->x = 0;
        if (ui->selected_node->y < 0)
          ui->selected_node->y = 0;
        if (ui->selected_node->x + ui->selected_node->width > dd->width)
          ui->selected_node->x = dd->width - ui->selected_node->width;
        if (ui->selected_node->y + ui->selected_node->height > dd->height)
          ui->selected_node->y = dd->height - ui->selected_node->height;
      }
      return;
    }

    // --- Cmd+D: duplicate selected node ---
    if (cmd_held && key == SDLK_d && ui->selected_node && active_block) {
      Node *src = ui->selected_node;
      Node *dup = NULL;

      if (is_canvas) {
        dup = node_create(active_block->data.canvas.next_node_id++, src->type,
                          src->name);
      } else if (is_design) {
        dup = node_create(active_block->data.design.next_node_id++, src->type,
                          src->name);
      }

      if (dup) {
        // Copy visual properties
        dup->x = src->x + 20.0f;
        dup->y = src->y + 20.0f;
        dup->width = src->width;
        dup->height = src->height;
        dup->rotation = src->rotation;
        dup->fill_color = src->fill_color;
        dup->stroke_color = src->stroke_color;
        dup->stroke_width = src->stroke_width;
        dup->corner_radius = src->corner_radius;
        dup->font_size = src->font_size;
        dup->font_type = src->font_type;
        dup->line_start_style = src->line_start_style;
        dup->line_end_style = src->line_end_style;
        dup->visible = 1;
        dup->is_container = src->is_container;
        strncpy(dup->text, src->text, sizeof(dup->text) - 1);
        strncpy(dup->image_path, src->image_path, sizeof(dup->image_path) - 1);

        // Add to tree
        Node *root = NULL;
        if (is_canvas)
          root = active_block->data.canvas.root;
        else if (is_design)
          root = active_block->data.design.root;

        if (root) {
          dup->parent = root;
          node_add_child(root, dup);
          ui->selected_node = dup;
        }
      }
      return;
    }
  }

  // Handle canvas zoom or page scroll with mouse wheel
  if (evt->type == SDL_MOUSEWHEEL) {
    CanvasInteraction *cs = &ui->canvas_state;

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
      ASLAN_DEBUG("Canvas Zoom: %.2f (wheel.y=%d)", cs->zoom, evt->wheel.y);
    } else {
      // Global Page Scroll (Default behavior)
      float scroll_amount = 60.0f;
      ui->scroll_target_y -= evt->wheel.y * scroll_amount;
      if (ui->scroll_target_y < 0)
        ui->scroll_target_y = 0;
      ASLAN_DEBUG("Page Scroll: %.1f (wheel.y=%d)", ui->scroll_target_y,
                  evt->wheel.y);
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

// Overlay toolbar: draws compact icons at (x_right, y_top) without consuming
// layout space. Elegant floating pill with subtle border.
static void ui_render_block_overlay_icons(UIHost *ui, Engine *engine,
                                          struct nk_context *ctx, Page *page,
                                          Block *block, float x_right,
                                          float y_top) {
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
  const struct nk_user_font *icon_font =
      ui->font_icons ? &ui->font_icons->handle : &ui->font_inter->handle;

  float btn_w = 22.0f, btn_h = 22.0f, gap = 2.0f;
  float mx = ctx->input.mouse.pos.x;
  float my = ctx->input.mouse.pos.y;
  int pressed = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT);
  int mouse_down = nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT);

  int is_expanded = (ui->expanded_block_id == block->id);
  int allow_fullscreen =
      (block->type != BLOCK_TEXT && block->type != BLOCK_HEADING_1);

  int show_trash = 1; // Allow deleting any block, including H1
  int allow_export =
      (block->type == BLOCK_TABLE || block->type == BLOCK_CANVAS ||
       block->type == BLOCK_DESIGN);
  int allow_resize =
      (block->type == BLOCK_TABLE || block->type == BLOCK_CANVAS ||
       block->type == BLOCK_DESIGN || block->type == BLOCK_CHART);

  // Count buttons
  int n = 1; // hand always
  if (block->type == BLOCK_TABLE)
    n++;
  if (allow_export)
    n++;
  if (allow_resize)
    n += 2; // shrink + grow
  if (allow_fullscreen)
    n++;
  if (show_trash)
    n++;

  float pad = 5.0f;
  float total_w = (float)n * btn_w + (float)(n - 1) * gap + pad * 2;
  float total_h = btn_h + 4.0f;
  float bx = x_right - total_w - 4.0f;
  float by = y_top + 6.0f;

  // Elegant pill: solid bg + subtle border + slight shadow
  struct nk_color pill_bg =
      ui->dark_mode ? nk_rgba(35, 36, 42, 230) : nk_rgba(255, 255, 255, 240);
  struct nk_color pill_brd =
      ui->dark_mode ? nk_rgba(65, 67, 75, 200) : nk_rgba(210, 210, 215, 220);
  // Shadow
  nk_fill_rect(canvas, nk_rect(bx + 1, by + 2, total_w, total_h), 7.0f,
               nk_rgba(0, 0, 0, ui->dark_mode ? 30 : 15));
  // Background
  nk_fill_rect(canvas, nk_rect(bx, by, total_w, total_h), 7.0f, pill_bg);
  // Border
  nk_stroke_rect(canvas, nk_rect(bx, by, total_w, total_h), 7.0f, 1.0f,
                 pill_brd);

  float ix = bx + pad;
  float iy = by + 2.0f;

  struct nk_color icon_col =
      ui->dark_mode ? nk_rgb(180, 182, 195) : nk_rgb(100, 100, 110);
  struct nk_color hover_bg =
      ui->dark_mode ? nk_rgba(70, 72, 85, 180) : nk_rgba(235, 235, 240, 220);
  struct nk_color hover_icon =
      ui->dark_mode ? nk_rgb(230, 230, 240) : nk_rgb(40, 40, 50);

// Helper macro: draw one compact icon button
#define OVERLAY_ICON_BTN(icon_str, icon_len)                                   \
  do {                                                                         \
    struct nk_rect r = nk_rect(ix, iy, btn_w, btn_h);                          \
    int hovered =                                                              \
        (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h);          \
    if (hovered)                                                               \
      nk_fill_rect(canvas, r, 4.0f, hover_bg);                                 \
    float tw = icon_font->width(icon_font->userdata, icon_font->height,        \
                                icon_str, icon_len);                           \
    struct nk_color ic = hovered ? hover_icon : icon_col;                      \
    nk_draw_text(canvas, nk_rect(ix + (btn_w - tw) / 2, iy + 2, btn_w, btn_h), \
                 icon_str, icon_len, icon_font, nk_rgba(0, 0, 0, 0), ic);      \
    _btn_hovered = hovered;                                                    \
    _btn_clicked = (hovered && pressed);                                       \
    ix += btn_w + gap;                                                         \
  } while (0)

  int _btn_hovered, _btn_clicked;

  // 1. Hand icon (drag — backup for 6-dot handle)
  OVERLAY_ICON_BTN(ICON_HAND, strlen(ICON_HAND));
  if (_btn_clicked && !ui->block_drag.is_active) {
    ui->block_drag.is_active = 1;
    ui->block_drag.dragged_block_id = block->id;
    ui->block_drag.start_x = mx;
    ui->block_drag.start_y = my;
    ui->block_drag.mouse_x = mx;
    ui->block_drag.mouse_y = my;
    ui->block_drag.threshold_met = 0;
    ui->block_drag.drop_valid = 0;
  }
  if (_btn_hovered && mouse_down && !ui->block_drag.is_active) {
    ui->block_drag.is_active = 1;
    ui->block_drag.dragged_block_id = block->id;
    ui->block_drag.start_x = mx;
    ui->block_drag.start_y = my;
    ui->block_drag.mouse_x = mx;
    ui->block_drag.mouse_y = my;
    ui->block_drag.threshold_met = 0;
    ui->block_drag.drop_valid = 0;
  }

  // 2. Search icon (TABLE only)
  if (block->type == BLOCK_TABLE) {
    OVERLAY_ICON_BTN(ICON_SEARCH, strlen(ICON_SEARCH));
    if (_btn_clicked) {
      block->data.table.show_search = !block->data.table.show_search;
      ui->table_state.active_block_id = block->id;
    }
  }

  // 3. Export icon
  if (allow_export) {
    OVERLAY_ICON_BTN(ICON_EXPORT, strlen(ICON_EXPORT));
    if (_btn_clicked) {
      if (block->type == BLOCK_TABLE) {
        char filename[1024];
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        // Clean up table name for default filename
        char clean_name[64];
        if (block->data.table.columnar &&
            block->data.table.columnar->name[0] != '\0') {
          strncpy(clean_name, block->data.table.columnar->name,
                  sizeof(clean_name) - 1);
          clean_name[sizeof(clean_name) - 1] = '\0';
        } else {
          strcpy(clean_name, "Table");
        }

        for (char *p = clean_name; *p; p++) {
          if (*p == ' ' || *p == '/' || *p == '\\')
            *p = '_';
        }

        char default_name[256];
        snprintf(default_name, sizeof(default_name), "%s_%04d%02d%02d.csv",
                 clean_name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

        // Open Save Dialog
        // Open Save Dialog
        if (save_csv_file_dialog(filename, sizeof(filename), default_name)) {
          // aslan_table_export_csv removed to fix compilation
          ASLAN_DEBUG("Export to CSV not implemented yet: %s", filename);
        } else {
          ASLAN_DEBUG("Export cancelled by user");
        }
      } else {
        ASLAN_DEBUG("Export requested for block %u (Type: %d)", block->id,
                    block->type);
      }
    }
  }

  // 4. Resize buttons (shrink / grow)
  if (allow_resize) {
    const struct nk_user_font *text_font =
        ui->font_inter ? &ui->font_inter->handle : icon_font;

    // Shrink (-)
    {
      struct nk_rect r = nk_rect(ix, iy, btn_w, btn_h);
      int hovered =
          (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h);
      if (hovered)
        nk_fill_rect(canvas, r, 4.0f, hover_bg);
      struct nk_color ic = hovered ? hover_icon : icon_col;
      float tw = text_font->width(text_font->userdata, text_font->height,
                                  "-", 1);
      nk_draw_text(canvas,
                   nk_rect(ix + (btn_w - tw) / 2, iy + 1, btn_w, btn_h), "-",
                   1, text_font, nk_rgba(0, 0, 0, 0), ic);
      if (hovered && pressed) {
        float vs = block->view_scale;
        if (vs < 0.1f)
          vs = 1.0f;
        vs -= 0.15f;
        if (vs < 0.3f)
          vs = 0.3f;
        block->view_scale = vs;
      }
      ix += btn_w + gap;
    }

    // Grow (+)
    {
      struct nk_rect r = nk_rect(ix, iy, btn_w, btn_h);
      int hovered =
          (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h);
      if (hovered)
        nk_fill_rect(canvas, r, 4.0f, hover_bg);
      struct nk_color ic = hovered ? hover_icon : icon_col;
      float tw = text_font->width(text_font->userdata, text_font->height,
                                  "+", 1);
      nk_draw_text(canvas,
                   nk_rect(ix + (btn_w - tw) / 2, iy + 1, btn_w, btn_h), "+",
                   1, text_font, nk_rgba(0, 0, 0, 0), ic);
      if (hovered && pressed) {
        float vs = block->view_scale;
        if (vs < 0.1f)
          vs = 1.0f;
        vs += 0.15f;
        if (vs > 3.0f)
          vs = 3.0f;
        block->view_scale = vs;
      }
      ix += btn_w + gap;
    }
  }

  // 5. Fullscreen icon
  if (allow_fullscreen) {
    if (is_expanded) {
      OVERLAY_ICON_BTN(ICON_FULLSCREEN_EXIT, strlen(ICON_FULLSCREEN_EXIT));
      if (_btn_clicked)
        ui->expanded_block_id = 0;
    } else {
      OVERLAY_ICON_BTN(ICON_FULLSCREEN, strlen(ICON_FULLSCREEN));
      if (_btn_clicked)
        ui->expanded_block_id = block->id;
    }
  }

  // 6. Trash icon
  if (show_trash) {
    OVERLAY_ICON_BTN(ICON_DELETE, strlen(ICON_DELETE));
    if (_btn_clicked) {
      char cmd[128];
      snprintf(cmd, sizeof(cmd), "block_delete %u %u", page->id, block->id);
      engine_execute_command(engine, cmd);
    }
  }

#undef OVERLAY_ICON_BTN
}

// Legacy wrapper (kept for header compatibility, but now a no-op for layout)
void ui_render_expand_toggle(UIHost *ui, struct nk_context *ctx, Block *block) {
  (void)ui;
  (void)ctx;
  (void)block;
  // Icons are now drawn as overlays in ui_render_block — no layout consumed
  // here
}

void ui_render_block_text(UIHost *ui, Engine *engine, struct nk_context *ctx,
                          Page *page, Block *block, float width) {
  (void)engine;
  (void)page;
  (void)width;
  if (!block)
    return;

  if (ui->font_inter) {
    nk_style_set_font(ctx, &ui->font_inter->handle);
  }

  // Header with Hand/Expand icons
  ui_render_expand_toggle(ui, ctx, block);

  const float line_h = 22.0f;

  // Calculate number of lines for multiline support
  int line_count = 1;
  if (block->text) {
    for (const char *p = block->text; *p; p++) {
      if (*p == '\n')
        line_count++;
    }
  }

  // Dynamic height: exactly content lines (reduced from line_count+1)
  float edit_height = line_h * line_count;
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
  nk_flags flags =
      NK_EDIT_BOX | NK_EDIT_MULTILINE | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD;
  nk_flags state = nk_edit_string_zero_terminated(
      ctx, flags, buf, (int)sizeof(buf) - 1, nk_filter_default);

  // Activate block on click/edit
  if (state & (NK_EDIT_ACTIVE | NK_EDIT_ACTIVATED)) {
    if (ui->active_block_id != block->id) {
      ui->active_block_id = block->id;
      ui_request_redraw(ui);
    }
  }

  if (state & NK_EDIT_ACTIVE) {
    // Subtle left-accent bar (Notion-style active indicator)
    nk_fill_rect(canvas,
                 nk_rect(bounds.x - 3, bounds.y + 2, 2.5f, bounds.h - 4), 1.0f,
                 nk_rgba(55, 120, 250, 140));
  }

  // Draw placeholder
  if (buf[0] == '\0' && !(state & NK_EDIT_ACTIVE)) {
    struct nk_color placeholder_color =
        ui->dark_mode ? nk_rgb(100, 100, 110) : nk_rgb(180, 180, 185);
    const char *placeholder = "Write something...";
    float text_x = bounds.x; // Flush with margin (was +8)
    float text_y = bounds.y + (bounds.h - line_h) / 2 + 4;
    const struct nk_user_font *ph_font =
        ui->font_inter ? &ui->font_inter->handle : &ui->font_mono->handle;
    nk_draw_text(canvas, nk_rect(text_x, text_y, bounds.w - 16, line_h),
                 placeholder, strlen(placeholder), ph_font, nk_rgba(0, 0, 0, 0),
                 placeholder_color);
  }

  // Update if changed
  if (strcmp(buf, block->text ? block->text : "") != 0) {
    block_set_text(block, buf);
  }
}

void ui_render_block_heading(UIHost *ui, struct nk_context *ctx, Block *block,
                             float width, int level) {
  (void)width;
  if (!block)
    return;

  // Determine if this is the page title (first H1 block of the page)
  int is_page_title = 0;
  Page *active_page = doc_page_get_active(ui->engine->doc);
  if (active_page && block == active_page->first_block && level == 1) {
    is_page_title = 1;
  }

  struct nk_font *font = is_page_title ? ui->font_title : ui->font_h1;

  float row_h =
      is_page_title ? 60.0f : 50.0f; // Taller row to accommodate large fonts

  // Header with Hand/Expand icons (skip for page title)
  // Render BEFORE setting H1 font so icons use normal font
  if (!is_page_title) {
    ui_render_expand_toggle(ui, ctx, block);
  }

  if (font) {
    nk_style_set_font(ctx, &font->handle);
  }

  if (is_page_title) {
    // Template for title + extreme right settings icon
    nk_layout_row_template_begin(ctx, row_h);
    nk_layout_row_template_push_variable(ctx, 100); // Title
    nk_layout_row_template_push_static(ctx, 40);    // Settings button
    nk_layout_row_template_end(ctx);
  } else {
    nk_layout_row_dynamic(ctx, row_h, 1);
  }

  char buf[1024];
  strncpy(buf, block->text ? block->text : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  nk_flags flags = NK_EDIT_BOX | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD;
  nk_flags state = nk_edit_string_zero_terminated(
      ctx, flags, buf, (int)sizeof(buf) - 1, nk_filter_default);

  // Settings icon on the extreme right
  if (is_page_title) {
    struct nk_rect bounds = nk_layout_widget_bounds(ctx);
    // Use icons font for settings icon
    if (ui->font_icons) {
      nk_style_set_font(ctx, &ui->font_icons->handle);
    }

    struct nk_style_button b_style = ctx->style.button;
    b_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    b_style.hover = nk_style_item_color(nk_rgba(0, 0, 0, 10));
    b_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 20));
    b_style.text_normal =
        ui->dark_mode ? nk_rgb(180, 180, 180) : nk_rgb(100, 100, 100);
    b_style.border = 0;
    b_style.rounding = 4.0f;

    if (nk_button_label_styled(ctx, &b_style, ICON_MORE)) {
      ui->show_doc_menu = !ui->show_doc_menu;
      ui->doc_menu_rect = bounds;
    }

    // Restore heading font
    if (font) {
      nk_style_set_font(ctx, &font->handle);
    }
  }

  // Activate block on click/edit
  if (state & (NK_EDIT_ACTIVE | NK_EDIT_ACTIVATED)) {
    if (ui->active_block_id != block->id) {
      ui->active_block_id = block->id;
      ui_request_redraw(ui);
    }
  }

  // Restore placeholder logic using bounds of the widget we just drew
  struct nk_rect bounds = nk_layout_widget_bounds(ctx);
  if (buf[0] == '\0' && !(state & NK_EDIT_ACTIVE)) {
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    struct nk_color placeholder_color =
        ui->dark_mode ? nk_rgb(100, 100, 110) : nk_rgb(170, 170, 180);
    const char *placeholder = "Heading 1";
    nk_draw_text(canvas, bounds, placeholder, strlen(placeholder),
                 &font->handle, nk_rgba(0, 0, 0, 0), placeholder_color);
  }

  // Debug H1 editing
  if (level == 1 &&
      (state & (NK_EDIT_ACTIVE | NK_EDIT_ACTIVATED | NK_EDIT_DEACTIVATED))) {
    ASLAN_DEBUG("H1 edit state: active=%d activated=%d deactivated=%d "
                "commited=%d buf='%s'",
                (state & NK_EDIT_ACTIVE) ? 1 : 0,
                (state & NK_EDIT_ACTIVATED) ? 1 : 0,
                (state & NK_EDIT_DEACTIVATED) ? 1 : 0,
                (state & NK_EDIT_COMMITED) ? 1 : 0, buf);
  }

  if (strcmp(buf, block->text ? block->text : "") != 0) {
    block_set_text(block, buf);
    if (level == 1) {
      ASLAN_DEBUG("H1 text updated to: '%s'", buf);
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

  // Record Y before block renders (for click detection)
  float y_before = nk_layout_widget_bounds(ctx).y;

  ASLAN_DEBUG("Rendering block %u (Type: %d)", block->id, block->type);
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
  case BLOCK_TABLE:
    ui_render_block_table(ui, engine, ctx, block, width);
    break;
  case BLOCK_CANVAS:
    ui_render_block_canvas(ui, engine, ctx, block, width);
    break;
  case BLOCK_DESIGN:
    ui_render_block_design(ui, engine, ctx, block, width);
    break;
  case BLOCK_CHART:
    ui_render_block_chart(ui, engine, ctx, page, block, width);
    break;
  default:
    break;
  }

  // Post-render: compute actual block bounds
  float y_after = nk_layout_widget_bounds(ctx).y;
  float block_h = y_after - y_before;
  if (block_h < 20.0f)
    block_h = 100.0f;

  struct nk_rect content = nk_window_get_content_region(ctx);
  float block_x = content.x;
  float mx = ctx->input.mouse.pos.x;
  float my = ctx->input.mouse.pos.y;

  // Detect click inside this block to set active_block_id
  if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
    if (mx >= block_x && mx <= block_x + width && my >= y_before &&
        my < y_before + block_h) {
      if (ui->active_block_id != block->id) {
        ui->active_block_id = block->id;
        ui_request_redraw(ui);
      }
    }
  }

  // --- Hover detection ---
  int is_hovered = (mx >= block_x - 30 && mx <= block_x + width + 10 &&
                    my >= y_before && my < y_before + block_h);
  int is_active = (ui->active_block_id == block->id);

  // Skip page-title H1 (no handle/icons needed)
  int is_page_title =
      (block->type == BLOCK_HEADING_1 && page && block == page->first_block);

  // --- 6-dot drag handle (Notion-style grip) on hover ---
  if ((is_hovered || is_active) && !ui->block_drag.is_active) {
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    float hx = block_x - 20.0f;
    float hy = y_before + (block_h > 50 ? block_h * 0.5f - 9.0f : 8.0f);
    struct nk_color dot_col =
        is_hovered ? (ui->dark_mode ? nk_rgba(130, 132, 145, 200)
                                    : nk_rgba(155, 155, 165, 220))
                   : (ui->dark_mode ? nk_rgba(90, 92, 100, 120)
                                    : nk_rgba(190, 190, 198, 150));

    // Draw 6 dots: 2 columns x 3 rows
    for (int dr = 0; dr < 3; dr++) {
      for (int dc = 0; dc < 2; dc++) {
        nk_fill_rect(
            canvas,
            nk_rect(hx + (float)dc * 5.0f, hy + (float)dr * 6.0f, 3.0f, 3.0f),
            1.5f, dot_col);
      }
    }

    // Click on handle area to initiate drag
    // Increased hit area for easier grabbing (34x34) and slightly overlapping
    // block
    struct nk_rect handle_area = nk_rect(hx - 8, hy - 8, 34, 34);
    if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
        mx >= handle_area.x && mx < handle_area.x + handle_area.w &&
        my >= handle_area.y && my < handle_area.y + handle_area.h &&
        !ui->block_drag.is_active) {
      ui->block_drag.is_active = 1;
      ui->block_drag.dragged_block_id = block->id;
      ui->block_drag.start_x = mx;
      ui->block_drag.start_y = my;
      ui->block_drag.mouse_x = mx;
      ui->block_drag.mouse_y = my;
      ui->block_drag.threshold_met = 0;
      ui->block_drag.drop_valid = 0;
    }
  }

  // Draw overlay icons if this block is active (fullscreen, search, trash)
  if (is_active && !is_page_title) {
    ui_render_block_overlay_icons(ui, engine, ctx, page, block, block_x + width,
                                  y_before);
  }
}

// -----------------------------------------------------------------------------
// Theme Application
// -----------------------------------------------------------------------------

// ui_apply_theme moved to ui_core.c

// -----------------------------------------------------------------------------
// Block Height Measurement (Adaptive Layout)
// -----------------------------------------------------------------------------

float ui_measure_block_height(UIHost *ui, Block *block, float width) {
  if (!block)
    return 100.0f;

  // Get block scale factor (default 1.0)
  float vs = block->view_scale;
  if (vs < 0.1f)
    vs = 1.0f;

  switch (block->type) {
  case BLOCK_TEXT: {
    int lines = 1;
    if (block->text) {
      for (const char *p = block->text; *p; p++)
        if (*p == '\n')
          lines++;
    }
    // Line height 22 + padding (tightened from +4 to +2)
    return (float)lines * 22.0f + 2.0f;
  }
  case BLOCK_HEADING_1:
    // H1 renders at 42px + group padding (reduced from 46)
    return 44.0f;

  case BLOCK_TABLE: {
    // Header 34 + Footer 30 + Padding 10 = 74
    float h = 74.0f;

    // Add row heights
    if (block->data.table.columnar) {
      // Use visible_rows if set, otherwise active rows in table
      int rows = block->data.table.visible_rows;
      if (rows <= 0) {
        rows = (int)aslan_table_view_count(block->data.table.columnar);
      }
      // Cap max auto-height to avoid massive tables
      if (rows > 15)
        rows = 15;
      if (rows < 3)
        rows = 3; // Min 3 rows visual

      h += (float)rows * 30.0f; // 30px per row
    } else {
      h += 3 * 30.0f; // Default
    }

    if (block->data.table.show_search) {
      h += 30.0f;
    }

    return h * vs;
  }

  case BLOCK_CANVAS:
    return 400.0f * vs;

  case BLOCK_DESIGN: {
    // Design renders at aspect ratio, max 400.0f + 52.0f toolbar
    float aspect_h = 400.0f;
    if (block->data.design.width > 0 && width > 0) {
      float scale = width / (float)block->data.design.width;
      aspect_h = (float)block->data.design.height * scale;
      if (aspect_h > 400.0f)
        aspect_h = 400.0f;
    }
    return (aspect_h + 52.0f) * vs;
  }

  case BLOCK_CHART:
    return 420.0f * vs;

  default:
    return 100.0f;
  }
}

// =============================================================================
// SPLIT VIEW — Data-Driven Layout
// =============================================================================

// Find first block of given type in page
static Block *page_find_first_block_of_type(Page *page, BlockType type) {
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == type)
      return b;
  }
  return NULL;
}

// Render view tabs bar for a panel
// active_view_ptr: pointer to the panel's view index
// panel_id: "L" or "R" for unique Nuklear IDs
static void ui_render_view_tabs(UIHost *ui, struct nk_context *ctx,
                                float width, int *active_view_ptr,
                                const char *panel_id) {
  const char *tab_labels[] = {"Table", "Design", "Kanban", "Calendar", "Chart"};
  const char *tab_icons[] = {ICON_TABLE, ICON_DESIGN, ICON_KANBAN,
                             ICON_CALENDAR, ICON_CHART};
  int tab_count = 5;
  (void)panel_id;

  struct nk_color active_col = nk_rgb(55, 120, 235);
  struct nk_color inactive_col =
      ui->dark_mode ? nk_rgb(140, 142, 150) : nk_rgb(130, 130, 138);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  float tab_w = width / (float)tab_count;
  if (tab_w > 120.0f)
    tab_w = 120.0f;

  nk_layout_row_begin(ctx, NK_STATIC, 32, tab_count);
  for (int i = 0; i < tab_count; i++) {
    nk_layout_row_push(ctx, tab_w);
    int is_active = (*active_view_ptr == i);

    struct nk_style_button tab_style = ctx->style.button;
    tab_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    tab_style.hover = nk_style_item_color(
        ui->dark_mode ? nk_rgba(255, 255, 255, 10) : nk_rgba(0, 0, 0, 8));
    tab_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    tab_style.border = 0;
    tab_style.rounding = 0;
    tab_style.text_normal = is_active ? active_col : inactive_col;
    tab_style.text_hover = active_col;
    tab_style.text_active = active_col;
    tab_style.padding = nk_vec2(4, 4);

    struct nk_rect tb = nk_widget_bounds(ctx);
    if (nk_button_label_styled(ctx, &tab_style, tab_labels[i])) {
      *active_view_ptr = i;
    }

    if (ui->font_icons) {
      const char *icon = tab_icons[i];
      struct nk_color ic = is_active ? active_col : inactive_col;
      nk_draw_text(canvas, nk_rect(tb.x + 4, tb.y + 7, 18, 18), icon,
                   strlen(icon), &ui->font_icons->handle, nk_rgba(0, 0, 0, 0),
                   ic);
    }

    if (is_active) {
      nk_fill_rect(canvas, nk_rect(tb.x + 4, tb.y + 29, tb.w - 8, 2.5f),
                   1.0f, active_col);
    }
  }
  nk_layout_row_end(ctx);

  // Bottom separator
  nk_layout_row_dynamic(ctx, 1, 1);
  struct nk_rect sep = nk_widget_bounds(ctx);
  nk_fill_rect(canvas, sep, 0,
               ui->dark_mode ? nk_rgb(55, 57, 65) : nk_rgb(228, 228, 232));
  nk_spacing(ctx, 1);
}

// Forward declarations for view renderers used by panel dispatch
static void ui_render_view_kanban(UIHost *ui, Engine *engine,
                                  struct nk_context *ctx, Block *table,
                                  float w, float h);
static void ui_render_view_calendar(UIHost *ui, Engine *engine,
                                    struct nk_context *ctx, Block *table,
                                    float w, float h);

// Render a single panel view (generic dispatch)
// view: 0=Table, 1=Design, 2=Kanban, 3=Calendar, 4=Chart
// prefix: "L" or "R" for unique group names
static void ui_render_panel_view(UIHost *ui, Engine *engine,
                                 struct nk_context *ctx, Page *page,
                                 int view, float view_w, float view_h,
                                 const char *prefix) {
  char grp_name[64];

  if (view == 0) {
    // Table
    Block *table = page_find_first_block_of_type(page, BLOCK_TABLE);
    if (table) {
      ui_render_block_table(ui, engine, ctx, table, view_w - 8);
    } else {
      nk_layout_row_dynamic(ctx, 60, 1);
      nk_label(ctx, "No table on this page", NK_TEXT_CENTERED);
      nk_layout_row_dynamic(ctx, 32, 1);
      if (nk_button_label(ctx, "+ New Table")) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "block_create %u %u table",
                 page->id, page->next_block_id);
        engine_execute_command(engine, cmd);
      }
    }
  } else if (view == 1) {
    // Design
    Block *design = page_find_first_block_of_type(page, BLOCK_DESIGN);
    if (design) {
      snprintf(grp_name, sizeof(grp_name), "%sDesignView", prefix);
      nk_layout_row_dynamic(ctx, view_h, 1);
      if (nk_group_begin(ctx, grp_name, NK_WINDOW_NO_SCROLLBAR)) {
        ui_render_block_design(ui, engine, ctx, design, view_w);
        nk_group_end(ctx);
      }
    } else {
      nk_layout_row_dynamic(ctx, 60, 1);
      nk_label(ctx, "No Design block on this page", NK_TEXT_CENTERED);
      nk_layout_row_dynamic(ctx, 32, 1);
      if (nk_button_label(ctx, "+ Create Design")) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "block_create %u %u design",
                 page->id, page->next_block_id);
        engine_execute_command(engine, cmd);
      }
    }
  } else if (view == 2) {
    // Kanban
    Block *table = page_find_first_block_of_type(page, BLOCK_TABLE);
    snprintf(grp_name, sizeof(grp_name), "%sKanbanView", prefix);
    nk_layout_row_dynamic(ctx, view_h, 1);
    if (nk_group_begin(ctx, grp_name, 0)) {
      if (table) {
        ui_render_view_kanban(ui, engine, ctx, table, view_w, view_h);
      } else {
        nk_layout_row_dynamic(ctx, 60, 1);
        nk_label(ctx, "Create a Table with a Select column for Kanban",
                 NK_TEXT_CENTERED);
      }
      nk_group_end(ctx);
    }
  } else if (view == 3) {
    // Calendar
    Block *table = page_find_first_block_of_type(page, BLOCK_TABLE);
    snprintf(grp_name, sizeof(grp_name), "%sCalendarView", prefix);
    nk_layout_row_dynamic(ctx, view_h, 1);
    if (nk_group_begin(ctx, grp_name, NK_WINDOW_NO_SCROLLBAR)) {
      if (table) {
        ui_render_view_calendar(ui, engine, ctx, table, view_w, view_h);
      } else {
        nk_layout_row_dynamic(ctx, 60, 1);
        nk_label(ctx, "Create a Table with a Date column for Calendar",
                 NK_TEXT_CENTERED);
      }
      nk_group_end(ctx);
    }
  } else if (view == 4) {
    // Chart
    Block *chart = page_find_first_block_of_type(page, BLOCK_CHART);
    if (chart) {
      snprintf(grp_name, sizeof(grp_name), "%sChartView", prefix);
      nk_layout_row_dynamic(ctx, view_h, 1);
      if (nk_group_begin(ctx, grp_name, NK_WINDOW_NO_SCROLLBAR)) {
        ui_render_block_chart(ui, engine, ctx, page, chart, view_w);
        nk_group_end(ctx);
      }
    } else {
      nk_layout_row_dynamic(ctx, 60, 1);
      nk_label(ctx, "No Chart block on this page", NK_TEXT_CENTERED);
      nk_layout_row_dynamic(ctx, 32, 1);
      if (nk_button_label(ctx, "+ Create Chart")) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "block_create %u %u chart",
                 page->id, page->next_block_id);
        engine_execute_command(engine, cmd);
      }
    }
  }
}

// Render Kanban view from a table
static void ui_render_view_kanban(UIHost *ui, Engine *engine,
                                  struct nk_context *ctx, Block *table_block,
                                  float width, float height) {
  (void)engine;
  if (!table_block || table_block->type != BLOCK_TABLE)
    return;
  AslanTable *tbl = table_block->data.table.columnar;
  if (!tbl)
    return;

  // Find first SELECT column for kanban grouping
  int select_col = -1;
  int title_col = -1;
  for (size_t c = 0; c < tbl->column_count; c++) {
    if (tbl->columns[c].type == ASLAN_COL_SELECT && select_col < 0)
      select_col = (int)c;
    if (tbl->columns[c].type == ASLAN_COL_TEXT && title_col < 0)
      title_col = (int)c;
  }

  if (select_col < 0) {
    nk_layout_row_dynamic(ctx, 60, 1);
    nk_label(ctx, "Add a Select column to enable Kanban view", NK_TEXT_CENTERED);
    return;
  }

  AslanColumn *sel_col = &tbl->columns[select_col];
  int opt_count = sel_col->option_count;
  if (opt_count <= 0) {
    nk_layout_row_dynamic(ctx, 60, 1);
    nk_label(ctx, "Add options to the Select column", NK_TEXT_CENTERED);
    return;
  }

  // Collect row IDs
  size_t row_count = aslan_table_view_count(tbl);

  // Calculate column widths
  float gap = 8.0f;
  float col_w = (width - (float)(opt_count + 1) * gap) / (float)opt_count;
  if (col_w < 140.0f)
    col_w = 140.0f;

  // Kanban columns
  float cols[16];
  int n_cols = 0;
  for (int i = 0; i < opt_count && i < 16; i++) {
    if (n_cols > 0) {
      cols[n_cols++] = gap; // spacer
    }
    cols[n_cols++] = col_w;
  }

  nk_layout_row(ctx, NK_STATIC, height - 45, n_cols, cols);

  for (int opt = 0; opt < opt_count && opt < 16; opt++) {
    if (opt > 0)
      nk_spacing(ctx, 1); // spacer

    uint32_t opt_color = sel_col->options[opt].color;
    struct nk_color header_col =
        nk_rgba((opt_color >> 24) & 0xFF, (opt_color >> 16) & 0xFF,
                (opt_color >> 8) & 0xFF, 255);

    char grp_name[32];
    snprintf(grp_name, sizeof(grp_name), "kanban_%d", opt);

    struct nk_color col_bg =
        ui->dark_mode ? nk_rgb(38, 40, 46) : nk_rgb(245, 245, 248);
    struct nk_style_item saved_bg = ctx->style.window.fixed_background;
    ctx->style.window.fixed_background = nk_style_item_color(col_bg);

    if (nk_group_begin(ctx, grp_name, NK_WINDOW_BORDER)) {
      // Column header with color accent
      nk_layout_row_dynamic(ctx, 3, 1);
      struct nk_rect accent_rect = nk_widget_bounds(ctx);
      nk_fill_rect(nk_window_get_canvas(ctx), accent_rect, 0, header_col);
      nk_spacing(ctx, 1);

      nk_layout_row_dynamic(ctx, 22, 1);
      char header_label[80];
      snprintf(header_label, sizeof(header_label), "%s",
               sel_col->options[opt].label);
      nk_label(ctx, header_label, NK_TEXT_CENTERED);

      nk_layout_row_dynamic(ctx, 4, 1);
      nk_spacing(ctx, 1);

      // Cards for rows matching this option
      for (size_t r = 0; r < row_count; r++) {
        uint64_t row_id = aslan_table_view_row_id(tbl, r);
        int sel_val = aslan_table_get_select(tbl, select_col, row_id);
        if (sel_val != opt)
          continue;

        // Card
        nk_layout_row_dynamic(ctx, 52, 1);
        struct nk_rect card_rect = nk_widget_bounds(ctx);
        struct nk_command_buffer *cv = nk_window_get_canvas(ctx);

        // Card background
        struct nk_color card_bg =
            ui->dark_mode ? nk_rgb(48, 50, 58) : nk_rgb(255, 255, 255);
        nk_fill_rect(cv, card_rect, 6.0f, card_bg);
        // Card shadow
        nk_fill_rect(cv, nk_rect(card_rect.x + 1, card_rect.y + 2,
                                  card_rect.w, card_rect.h),
                     6.0f, nk_rgba(0, 0, 0, ui->dark_mode ? 20 : 10));
        nk_fill_rect(cv, card_rect, 6.0f, card_bg);

        // Card border on hover
        int is_selected = (ui->split_view.selected_row == (int)r);
        if (is_selected) {
          nk_stroke_rect(cv, card_rect, 6.0f, 1.5f, nk_rgb(55, 120, 235));
        }

        // Card title
        char title[128] = "";
        if (title_col >= 0) {
          const char *txt = aslan_table_get_text(tbl, title_col, row_id);
          if (txt)
            snprintf(title, sizeof(title), "%s", txt);
        }
        if (title[0] == '\0')
          snprintf(title, sizeof(title), "Row %d", (int)r + 1);

        struct nk_color txt_col =
            ui->dark_mode ? nk_rgb(220, 220, 225) : nk_rgb(40, 40, 50);
        nk_draw_text(cv, nk_rect(card_rect.x + 10, card_rect.y + 8,
                                  card_rect.w - 20, 20),
                     title, strlen(title), &ui->font_inter->handle,
                     nk_rgba(0, 0, 0, 0), txt_col);

        // Click to select
        float mx = ctx->input.mouse.pos.x;
        float my = ctx->input.mouse.pos.y;
        if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
            mx >= card_rect.x && mx < card_rect.x + card_rect.w &&
            my >= card_rect.y && my < card_rect.y + card_rect.h) {
          ui->split_view.selected_row = (int)r;
        }

        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 4, 1);
        nk_spacing(ctx, 1);
      }
      nk_group_end(ctx);
    }
    ctx->style.window.fixed_background = saved_bg;
  }
}

// Render Calendar view from a table
static void ui_render_view_calendar(UIHost *ui, Engine *engine,
                                    struct nk_context *ctx, Block *table_block,
                                    float width, float height) {
  (void)engine;
  (void)width;
  if (!table_block || table_block->type != BLOCK_TABLE)
    return;
  AslanTable *tbl = table_block->data.table.columnar;
  if (!tbl)
    return;

  // Find first DATE column
  int date_col = -1;
  int title_col = -1;
  for (size_t c = 0; c < tbl->column_count; c++) {
    if (tbl->columns[c].type == ASLAN_COL_DATE && date_col < 0)
      date_col = (int)c;
    if (tbl->columns[c].type == ASLAN_COL_TEXT && title_col < 0)
      title_col = (int)c;
  }

  if (date_col < 0) {
    nk_layout_row_dynamic(ctx, 60, 1);
    nk_label(ctx, "Add a Date column to enable Calendar view", NK_TEXT_CENTERED);
    return;
  }

  // Use a static month/year state (persists across frames)
  static int cal_year = 0, cal_month = 0;
  if (cal_year == 0) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    cal_year = tm->tm_year + 1900;
    cal_month = tm->tm_mon;
  }

  struct nk_color txt_col =
      ui->dark_mode ? nk_rgb(220, 220, 225) : nk_rgb(40, 40, 50);
  struct nk_color dim_col =
      ui->dark_mode ? nk_rgb(100, 102, 110) : nk_rgb(160, 162, 168);
  struct nk_color accent = nk_rgb(55, 120, 235);

  // Month navigation
  struct nk_style_button nav_btn = ctx->style.button;
  nav_btn.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
  nav_btn.hover = nk_style_item_color(
      ui->dark_mode ? nk_rgba(255, 255, 255, 15) : nk_rgba(0, 0, 0, 8));
  nav_btn.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
  nav_btn.border = 0;
  nav_btn.rounding = 4;
  nav_btn.text_normal = txt_col;
  nav_btn.text_hover = accent;

  nk_layout_row_begin(ctx, NK_STATIC, 32, 3);
  nk_layout_row_push(ctx, 40);
  if (nk_button_label_styled(ctx, &nav_btn, "<")) {
    cal_month--;
    if (cal_month < 0) {
      cal_month = 11;
      cal_year--;
    }
  }

  static const char *month_names[] = {"January",   "February", "March",
                                      "April",     "May",      "June",
                                      "July",      "August",   "September",
                                      "October",   "November", "December"};
  char title[64];
  snprintf(title, sizeof(title), "%s %d", month_names[cal_month], cal_year);
  nk_layout_row_push(ctx, width - 100);
  nk_label(ctx, title, NK_TEXT_CENTERED);

  nk_layout_row_push(ctx, 40);
  if (nk_button_label_styled(ctx, &nav_btn, ">")) {
    cal_month++;
    if (cal_month > 11) {
      cal_month = 0;
      cal_year++;
    }
  }
  nk_layout_row_end(ctx);

  // Day headers
  nk_layout_row_dynamic(ctx, 20, 7);
  static const char *dow[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  for (int i = 0; i < 7; i++) {
    nk_label_colored(ctx, dow[i], NK_TEXT_CENTERED, dim_col);
  }

  // Calculate first day and days in month
  struct tm first_tm = {0};
  first_tm.tm_year = cal_year - 1900;
  first_tm.tm_mon = cal_month;
  first_tm.tm_mday = 1;
  first_tm.tm_isdst = -1;
  mktime(&first_tm);
  int start_offset = (first_tm.tm_wday + 6) % 7;

  struct tm next_m = {0};
  next_m.tm_year = cal_year - 1900;
  next_m.tm_mon = cal_month + 1;
  next_m.tm_mday = 0;
  next_m.tm_isdst = -1;
  mktime(&next_m);
  int days_in_month = next_m.tm_mday;

  // Collect events for this month
  size_t row_count = aslan_table_view_count(tbl);

  // Calendar grid (cell height adapts to available space)
  float cell_h = (height - 90) / 6.0f;
  if (cell_h < 50)
    cell_h = 50;
  if (cell_h > 100)
    cell_h = 100;

  for (int row = 0; row < 6; row++) {
    nk_layout_row_dynamic(ctx, cell_h, 7);
    for (int col = 0; col < 7; col++) {
      int cell = row * 7 + col;
      int day = cell - start_offset + 1;

      struct nk_rect cell_rect = nk_widget_bounds(ctx);
      struct nk_command_buffer *cv = nk_window_get_canvas(ctx);

      if (day < 1 || day > days_in_month) {
        nk_spacing(ctx, 1);
        continue;
      }

      // Cell background
      struct nk_color cell_bg =
          ui->dark_mode ? nk_rgba(42, 44, 50, 180) : nk_rgba(255, 255, 255, 200);
      nk_fill_rect(cv, nk_rect(cell_rect.x + 1, cell_rect.y + 1,
                                cell_rect.w - 2, cell_rect.h - 2),
                   4.0f, cell_bg);

      // Day number
      char day_str[4];
      snprintf(day_str, sizeof(day_str), "%d", day);

      // Today highlight
      time_t now = time(NULL);
      struct tm *now_tm = localtime(&now);
      int is_today = (day == now_tm->tm_mday && cal_month == now_tm->tm_mon &&
                      cal_year == now_tm->tm_year + 1900);
      struct nk_color day_col = is_today ? accent : dim_col;
      nk_draw_text(cv,
                   nk_rect(cell_rect.x + 4, cell_rect.y + 2, 30, 16),
                   day_str, strlen(day_str), &ui->font_inter->handle,
                   nk_rgba(0, 0, 0, 0), day_col);

      // Events for this day
      float ev_y = cell_rect.y + 20;
      for (size_t r = 0; r < row_count && ev_y < cell_rect.y + cell_h - 4; r++) {
        uint64_t row_id = aslan_table_view_row_id(tbl, r);
        int64_t ts = aslan_table_get_date(tbl, date_col, row_id);
        if (ts <= 0)
          continue;

        time_t t = (time_t)(ts / 1000);
        struct tm *etm = localtime(&t);
        if (etm->tm_mday != day || etm->tm_mon != cal_month ||
            etm->tm_year + 1900 != cal_year)
          continue;

        // Event pill
        char ev_label[64] = "";
        if (title_col >= 0) {
          const char *txt = aslan_table_get_text(tbl, title_col, row_id);
          if (txt)
            snprintf(ev_label, sizeof(ev_label), "%s", txt);
        }
        if (ev_label[0] == '\0')
          snprintf(ev_label, sizeof(ev_label), "Event");

        struct nk_color pill_bg = nk_rgba(55, 120, 235, 40);
        struct nk_color pill_txt = accent;
        int is_sel = (ui->split_view.selected_row == (int)r);
        if (is_sel) {
          pill_bg = nk_rgba(55, 120, 235, 120);
          pill_txt = nk_rgb(255, 255, 255);
        }

        nk_fill_rect(cv, nk_rect(cell_rect.x + 2, ev_y, cell_rect.w - 4, 14),
                     3.0f, pill_bg);
        nk_draw_text(
            cv, nk_rect(cell_rect.x + 5, ev_y + 1, cell_rect.w - 10, 12),
            ev_label, strlen(ev_label), &ui->font_inter->handle,
            nk_rgba(0, 0, 0, 0), pill_txt);

        ev_y += 16;
      }

      // Click on cell
      float mx = ctx->input.mouse.pos.x;
      float my = ctx->input.mouse.pos.y;
      if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
          mx >= cell_rect.x && mx < cell_rect.x + cell_rect.w &&
          my >= cell_rect.y && my < cell_rect.y + cell_rect.h) {
        // Select first event on this day
        for (size_t r = 0; r < row_count; r++) {
          uint64_t row_id = aslan_table_view_row_id(tbl, r);
          int64_t ts = aslan_table_get_date(tbl, date_col, row_id);
          if (ts <= 0)
            continue;
          time_t t = (time_t)(ts / 1000);
          struct tm *etm = localtime(&t);
          if (etm->tm_mday == day && etm->tm_mon == cal_month &&
              etm->tm_year + 1900 == cal_year) {
            ui->split_view.selected_row = (int)r;
            break;
          }
        }
      }

      nk_spacing(ctx, 1);
    }
    if ((row + 1) * 7 - start_offset > days_in_month)
      break;
  }
}

// Render the split view layout (symmetric: both panels can show any view)
static void ui_render_split_view(UIHost *ui, Engine *engine,
                                 struct nk_context *ctx, int win_w,
                                 int win_h) {
  Page *page = doc_page_get_active(engine->doc);
  if (!page)
    return;

  float ratio = ui->split_view.ratio;
  if (ratio < 0.03f)
    ratio = 0.03f;
  if (ratio > 0.97f)
    ratio = 0.97f;

  float splitter_w = 6.0f;
  float left_w = (float)win_w * ratio - splitter_w * 0.5f;
  float right_w = (float)win_w - left_w - splitter_w;
  if (left_w < 30)
    left_w = 30;
  if (right_w < 30)
    right_w = 30;

  float full_h = (float)win_h;

  // --- Splitter interaction ---
  float splitter_x = left_w;
  float mx = ctx->input.mouse.pos.x;
  float my = ctx->input.mouse.pos.y;
  int mouse_in_splitter = (mx >= splitter_x - 4 && mx <= splitter_x + splitter_w + 4 &&
                           my >= 0 && my < full_h);

  if (mouse_in_splitter)
    ui->cursor_type = UI_CURSOR_SIZEWE;

  if (mouse_in_splitter &&
      nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
    ui->split_view.is_dragging_splitter = 1;
  }

  if (ui->split_view.is_dragging_splitter) {
    if (nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
      float new_ratio = mx / (float)win_w;
      if (new_ratio < 0.03f)
        new_ratio = 0.03f;
      if (new_ratio > 0.97f)
        new_ratio = 0.97f;
      ui->split_view.ratio = new_ratio;
      ui->cursor_type = UI_CURSOR_SIZEWE;
    } else {
      ui->split_view.is_dragging_splitter = 0;
    }
  }

  // Double-click on splitter: cycle modes
  if (mouse_in_splitter &&
      nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
    static float last_click_time = 0;
    float now = (float)SDL_GetTicks() / 1000.0f;
    if (now - last_click_time < 0.35f) {
      if (ui->split_view.mode == 0) {
        ui->split_view.mode = 1;
        ui->split_view.ratio = 0.03f;
      } else if (ui->split_view.mode == 1) {
        ui->split_view.mode = 2;
        ui->split_view.ratio = 0.97f;
      } else {
        ui->split_view.mode = 0;
        ui->split_view.ratio = 0.5f;
      }
    }
    last_click_time = now;
  }

  // Recalculate after potential ratio change
  ratio = ui->split_view.ratio;
  left_w = (float)win_w * ratio - splitter_w * 0.5f;
  right_w = (float)win_w - left_w - splitter_w;
  if (left_w < 30)
    left_w = 30;
  if (right_w < 30)
    right_w = 30;

  // --- Layout: 3 columns (left | splitter | right) ---
  float cols[3] = {left_w, splitter_w, right_w};
  nk_layout_row(ctx, NK_STATIC, full_h - 10, 3, cols);

  // ===================== LEFT PANEL =====================
  struct nk_color panel_bg =
      ui->dark_mode ? nk_rgb(28, 30, 36) : nk_rgb(252, 252, 253);
  struct nk_style_item saved_bg = ctx->style.window.fixed_background;
  ctx->style.window.fixed_background = nk_style_item_color(panel_bg);
  struct nk_vec2 saved_pad = ctx->style.window.group_padding;
  ctx->style.window.group_padding = nk_vec2(4, 4);

  if (nk_group_begin(ctx, "SplitLeft", NK_WINDOW_NO_SCROLLBAR)) {
    float view_w = left_w - 12;
    float view_h = full_h - 55;
    ui_render_view_tabs(ui, ctx, view_w, &ui->split_view.left_view, "L");
    ui_render_panel_view(ui, engine, ctx, page,
                         ui->split_view.left_view, view_w, view_h, "L");
    nk_group_end(ctx);
  }
  ctx->style.window.fixed_background = saved_bg;
  ctx->style.window.group_padding = saved_pad;

  // ===================== SPLITTER =====================
  {
    struct nk_rect sp = nk_widget_bounds(ctx);
    struct nk_command_buffer *cv = nk_window_get_canvas(ctx);
    struct nk_color sp_bg =
        ui->dark_mode ? nk_rgb(42, 44, 52) : nk_rgb(235, 235, 238);
    struct nk_color sp_dot =
        ui->dark_mode ? nk_rgb(80, 82, 92) : nk_rgb(180, 180, 188);
    if (mouse_in_splitter || ui->split_view.is_dragging_splitter) {
      sp_bg = ui->dark_mode ? nk_rgb(55, 57, 68) : nk_rgb(218, 218, 225);
      sp_dot = ui->dark_mode ? nk_rgb(120, 122, 135) : nk_rgb(140, 140, 150);
    }
    nk_fill_rect(cv, sp, 0, sp_bg);
    float dot_y = sp.y + sp.h * 0.5f - 10;
    for (int d = 0; d < 3; d++) {
      nk_fill_rect(cv,
                   nk_rect(sp.x + sp.w * 0.5f - 1.5f, dot_y + (float)d * 7,
                           3, 3),
                   1.5f, sp_dot);
    }
    nk_spacing(ctx, 1);
  }

  // ===================== RIGHT PANEL =====================
  struct nk_color right_bg =
      ui->dark_mode ? nk_rgb(24, 26, 32) : nk_rgb(255, 255, 255);
  ctx->style.window.fixed_background = nk_style_item_color(right_bg);
  ctx->style.window.group_padding = nk_vec2(6, 4);

  if (nk_group_begin(ctx, "SplitRight", NK_WINDOW_NO_SCROLLBAR)) {
    float view_w = right_w - 12;
    float view_h = full_h - 55;
    ui_render_view_tabs(ui, ctx, view_w, &ui->split_view.right_view, "R");
    ui_render_panel_view(ui, engine, ctx, page,
                         ui->split_view.right_view, view_w, view_h, "R");
    nk_group_end(ctx);
  }
  ctx->style.window.fixed_background = saved_bg;
  ctx->style.window.group_padding = saved_pad;
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

  // Initialize cursor for this frame (will be committed at the end)
  ui->cursor_type = UI_CURSOR_ARROW;

  // Apply current theme
  ui_apply_theme(ui);

  // Top bar removed (now floating at bottom)

  // Main content area - always Editor
  if (ui->expanded_block_id != 0) {
    // Expanded block view
    struct nk_vec2 old_padding = ctx->style.window.padding;
    struct nk_vec2 old_group_padding = ctx->style.window.group_padding;
    ctx->style.window.padding = nk_vec2(0, 0);
    ctx->style.window.group_padding = nk_vec2(0, 0);

    if (nk_begin(ctx, "ContentFull", nk_rect(0, 0, (float)win_w, (float)win_h),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
      Page *page = doc_page_get_active(engine->doc);
      if (page) {
        Block *block = page_find_block_by_id(page, ui->expanded_block_id);
        if (block) {
          ui_render_block(ui, engine, ctx, page, block, (float)win_w);
        }
      }
      ui_render_document_menu(ui, ctx);
    }
    nk_end(ctx);

    ctx->style.window.padding = old_padding;
    ctx->style.window.group_padding = old_group_padding;
  } else {
    // Split view mode (Data-Driven) — default and only editor mode
    // Auto-activate split view with default ratio if needed
    if (ui->split_view.ratio < 0.01f) {
      ui->split_view.ratio = 0.5f;
      ui->split_view.left_view = 0;   // Table
      ui->split_view.right_view = 1;  // Design
    }
    ui->split_view.active = 1;

    struct nk_vec2 old_padding_sv = ctx->style.window.padding;
    ctx->style.window.padding = nk_vec2(0, 0);

    if (nk_begin(ctx, "SplitMain",
                 nk_rect(0, 0, (float)win_w, (float)win_h),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
      ui_render_split_view(ui, engine, ctx, win_w, win_h);
    }
    nk_end(ctx);
    ctx->style.window.padding = old_padding_sv;
  }

  // Reset per-frame input state (after all processing is done)
  ui->canvas_state.mouse_just_pressed = 0;
  ui->canvas_state.mouse_just_released = 0;
  ui->design_state.mouse_just_pressed = 0;
  ui->design_state.mouse_just_released = 0;

  // Safety net: Reset table resizing state if mouse is released globally
  if (ui->table_state.resizing_col >= 0 &&
      !nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
    ui->table_state.resizing_col = -1;
  }
  // Safety net: Reset column drag state if mouse is released globally
  if (ui->table_state.dragging_col >= 0 &&
      !nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
    ui->table_state.dragging_col = -1;
    ui->table_state.drop_target_col = -1;
  }

  // Deselect column header when clicking outside a table header area
  // (The specific deselection on click happens in ui_table.c, but we also
  //  reset here if no table block is active)
  if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
      ui->table_state.selected_col_idx >= 0) {
    // If user clicked somewhere, deselect after 1 frame
    // (The table render code handles more precise deselection)
  }

  // Override cursor during block drag (only when past dead zone)
  if (ui->block_drag.is_active && ui->block_drag.threshold_met) {
    ui->cursor_type = UI_CURSOR_HAND;
  }

  // Finalize cursor for this frame
  ui_commit_cursor(ui);

  // Render Nuklear
  nk_sdl_render(NK_ANTI_ALIASING_ON);
}

// -----------------------------------------------------------------------------
// Home Screen (Dashboard)
// -----------------------------------------------------------------------------

// Helper: Create new document with default setup
// home_create_new_document moved to ui_core.c

// ui_render_home moved to ui_core.c
void ui_render_document_menu(UIHost *ui, struct nk_context *ctx) {
  if (!ui->show_doc_menu)
    return;

  // Use floating popup near the settings icon
  struct nk_rect bounds = ui->doc_menu_rect;
  struct nk_vec2 pos = nk_vec2(bounds.x + bounds.w, bounds.y + bounds.h);
  struct nk_rect popup_rect = nk_rect(pos.x - 200, pos.y + 4, 200, 140);

  // Close if clicking outside the popup AND the button that opens it
  if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
    if (!nk_input_is_mouse_hovering_rect(&ctx->input, popup_rect) &&
        !nk_input_is_mouse_hovering_rect(&ctx->input, bounds)) {
      ui->show_doc_menu = 0;
      return;
    }
  }

  // Style the popup
  struct nk_style_item old_bg = ctx->style.window.fixed_background;
  ctx->style.window.fixed_background = nk_style_item_color(
      ui->dark_mode ? nk_rgb(45, 46, 50) : nk_rgb(255, 255, 255));

  if (nk_popup_begin(ctx, NK_POPUP_STATIC, "DocSettings",
                     NK_WINDOW_NO_SCROLLBAR,
                     nk_rect(pos.x - 200, pos.y + 4, 200, 140))) {
    nk_layout_row_dynamic(ctx, 32, 1);

    if (nk_button_label(ctx, "Save (.aslan)")) {
      char path[1024];
      if (ui->engine->doc_path[0] != '\0') {
        snprintf(path, sizeof(path), "%s/snapshot.aslan", ui->engine->doc_path);
        engine_save_snapshot_as_aslan(ui->engine, path);
        ASLAN_INFO("Application saved to %s", path);
      } else {
        ui->show_save_modal = 1;
      }
      ui->show_doc_menu = 0;
      nk_popup_close(ctx);
    }

    if (nk_button_label(ctx, "Save As... (.aslan)")) {
      ui->show_save_modal = 1;
      ui->show_doc_menu = 0;
      nk_popup_close(ctx);
    }

    char theme_label[64];
    snprintf(theme_label, sizeof(theme_label), "Mode: %s",
             ui->dark_mode ? "Light" : "Dark");
    if (nk_button_label(ctx, theme_label)) {
      ui->dark_mode = !ui->dark_mode;
      ui_apply_theme(ui);
      ui->show_doc_menu = 0;
      nk_popup_close(ctx);
    }

    nk_popup_end(ctx);
  } else {
    ui->show_doc_menu = 0;
  }
  ctx->style.window.fixed_background = old_bg;
}
