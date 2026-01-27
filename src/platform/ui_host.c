// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear implementation (must be before nuklear.h include)
#define NK_IMPLEMENTATION
#include "ui_host.h"
#include "../document/aslan_table.h"
#include "../media/ffmpeg_pipe.h"
#include "../media/whisper.h"
#include "../scripting/lua_host.h"
#include "mac_image.h"
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

  // Initialize whisper for transcription (multilingual model)
  whisper_init("vendor/whisper.cpp/build/bin/whisper-cli",
               "vendor/whisper.cpp/models/ggml-small.bin");

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
      ui->design_state.mouse_just_pressed = 1; // Also track for design
      ui->video_state.mouse_just_pressed = 1;  // Also track for video

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

      // Stop panning if was space+left panning
      if (cs->space_held) {
        cs->is_panning = 0;
      }

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
    ui->canvas_state.space_held = 0;
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

  // Delete video transcript segment (Backspace or Delete)
  if (evt->type == SDL_KEYDOWN && (evt->key.keysym.sym == SDLK_BACKSPACE ||
                                   evt->key.keysym.sym == SDLK_DELETE)) {
    if (ui->video_state.active_block &&
        ui->video_state.active_block->type == BLOCK_VIDEO) {
      VideoData *vd = &ui->video_state.active_block->data.video;
      if (vd->selected_segment >= 0 &&
          vd->selected_segment < vd->transcript_count) {
        vd->transcript[vd->selected_segment].is_deleted = 1;
      }
    }
  }

  // Restore video transcript segment (R key)
  if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_r) {
    if (ui->video_state.active_block &&
        ui->video_state.active_block->type == BLOCK_VIDEO) {
      VideoData *vd = &ui->video_state.active_block->data.video;
      if (vd->selected_segment >= 0 &&
          vd->selected_segment < vd->transcript_count) {
        vd->transcript[vd->selected_segment].is_deleted = 0;
      }
    }
  }

  // Delete audio transcript segment (Backspace or Delete)
  if (evt->type == SDL_KEYDOWN && (evt->key.keysym.sym == SDLK_BACKSPACE ||
                                   evt->key.keysym.sym == SDLK_DELETE)) {
    if (ui->audio_state.active_block &&
        ui->audio_state.active_block->type == BLOCK_AUDIO) {
      AudioData *ad = &ui->audio_state.active_block->data.audio;
      if (ad->selected_segment >= 0 &&
          ad->selected_segment < ad->transcript_count) {
        ad->transcript[ad->selected_segment].is_deleted = 1;
      }
    }
  }

  // Restore audio transcript segment (R key)
  if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_r) {
    if (ui->audio_state.active_block &&
        ui->audio_state.active_block->type == BLOCK_AUDIO) {
      AudioData *ad = &ui->audio_state.active_block->data.audio;
      if (ad->selected_segment >= 0 &&
          ad->selected_segment < ad->transcript_count) {
        ad->transcript[ad->selected_segment].is_deleted = 0;
      }
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

    Page *page = doc_page_get_active(engine->doc);
    uint32_t page_id = page ? page->id : 0;

    // Calculate centering: 7 icons at 36px each = 252px total
    float icons_total_w = 7 * btn_w;
    float left_margin = ((float)win_w - icons_total_w) / 2.0f;
    if (left_margin < btn_w + 10)
      left_margin = btn_w + 10; // Min space for home

    // Use template layout: [Home] [flex space] [7 icons centered] [flex space]
    nk_layout_row_template_begin(ctx, btn_h);
    nk_layout_row_template_push_static(ctx, btn_w); // Home
    nk_layout_row_template_push_static(ctx,
                                       left_margin - btn_w - 5); // Left spacer
    for (int i = 0; i < 7; i++) {
      nk_layout_row_template_push_static(ctx, btn_w); // 7 icons
    }
    nk_layout_row_template_push_variable(ctx, 1); // Right spacer (fills rest)
    nk_layout_row_template_end(ctx);

    // Home button (icon)
    if (ui_icon_button(ui, ICON_HOME)) {
      ui->screen = 0;
    }

    // Left spacer (empty widget)
    nk_spacing(ctx, 1);

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

    // Audio (volume icon)
    if (ui_icon_button(ui, ICON_VOLUME)) {
      if (page) {
        Block *b = block_create(page, BLOCK_AUDIO);
        if (b) {
          block_audio_init(b);
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u audio", page_id,
                   b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Right spacer consumes remaining space
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
      struct nk_color header_text_col = nk_rgb(55, 53, 47);
      struct nk_color icon_col = nk_rgba(55, 53, 47, 128); // 50% opacity

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
    struct nk_color notion_white = nk_rgb(255, 255, 255);
    struct nk_color notion_border = nk_rgb(235, 236, 237);
    struct nk_color notion_text = nk_rgb(55, 53, 47);

    for (size_t v = 0; v < view_count && v < 100; v++) { // Limit visible rows
      uint64_t row_id = aslan_table_view_row_id(tbl, (int)v);
      if (row_id == 0)
        continue;

      // Notion style: Pure white background for everything
      struct nk_color row_bg = notion_white;

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
      // Bottom border only
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
                         1.0f,
                         nk_rgb(245, 246, 247)); // Faint vertical separator

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
  const float toolbar_w =
      4 * btn_size + 3 * btn_spacing + 24.0f; // 4 buttons now
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
  // 0: Forms (shapes), 1: Text, 2: Image, 3: Pen
  const char *tool_icons[] = {ICON_SHAPE, "A", "\xEE\xB9\xA4", "\xEF\x86\x95"};
  int use_icon_font[] = {1, 0, 1, 1}; // Text uses regular font (A)
  int tool_count = 4;

  for (int i = 0; i < tool_count; i++) {
    struct nk_rect btn = nk_rect(btn_x + i * (btn_size + btn_spacing), btn_y,
                                 btn_size, btn_size);

    int is_active = 0;
    if (i == 0)
      is_active = cs->show_forms_menu;
    else if (i == 1)
      is_active = 0; // Text never stays active
    else if (i == 2)
      is_active = 0; // Image never stays active
    else if (i == 3)
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
      } else if (i == 3) {
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
        cs->pending_create = i; // 1=Text, 2=Image
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
    uint32_t colors[] = {0x000000FF, 0xD93025FF, 0xF9AB00FF,
                         0x1E8E3EFF, 0x1A73E8FF, 0xA142F4FF};
    float cx = menu_x + 12.0f;
    float cy = menu_y + 12.0f;
    float sz = 24.0f;

    for (int c = 0; c < 6; c++) {
      struct nk_rect cr = nk_rect(cx + c * (sz + 6), cy, sz, sz);
      struct nk_color col =
          nk_rgba((colors[c] >> 24) & 0xFF, (colors[c] >> 16) & 0xFF,
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
                 cs->pen_is_marker ? nk_rgb(200, 220, 255)
                                   : nk_rgb(240, 240, 240));
    nk_stroke_rect(canvas_buf, mr, 4.0f, 1.0f, nk_rgb(200, 200, 200));

    // Text "Marker"
    if (ui->font_inter)
      nk_draw_text(canvas_buf, nk_rect(cx + 10, cy + 4, 60, 16), "Marker", 6,
                   &ui->font_inter->handle, nk_rgba(0, 0, 0, 0),
                   nk_rgb(50, 50, 50));

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
  NODE_STYLE_TEXT // Plain text, no background
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
      fprintf(f, "get_image_texture failed for %s, trying %s -> %p\n", path,
              full_path, pixels);
      fclose(f);
    }
  }

  if (!pixels)
    return NULL;

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
      // Image - open file dialog
      char selected_path[512] = {0};
      if (open_image_file_dialog(selected_path, sizeof(selected_path))) {
        // Get document directory from engine (if available) or use current
        // dir
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

    // Check property menu hit
    float menu_w = 220.0f;
    float menu_h = 45.0f;
    float menu_x = sx;
    float menu_y = sy - menu_h - 10.0f;

    // Keep within canvas bounds (Match logic with render)
    if (menu_x < bounds.x)
      menu_x = bounds.x;
    if (menu_x + menu_w > bounds.x + bounds.w)
      menu_x = bounds.x + bounds.w - menu_w;
    if (menu_y < bounds.y)
      menu_y = sy + sh + 10.0f;

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
  // Handle other mouse press in canvas (not toolbar, not forms menu, not
  // resizing)
  else if (cs->mouse_in_canvas && !in_toolbar && !in_forms_menu &&
           !cs->is_panning && cs->mouse_just_pressed && !on_resize_handle) {

    if (on_property_menu) {
      // Let property menu handle the click
    } else {
      // Normal click: Try to select a node, or deselect if empty
      float world_x = cam_x + (ui->mouse_x - bounds.x) / zoom;
      float world_y = cam_y + (ui->mouse_y - bounds.y) / zoom;

      Node *hit = canvas_find_node_at(cd, world_x, world_y);

      // If we were editing and clicked elsewhere, save the text
      if (ui->editing_node && ui->editing_node != hit) {
        strncpy(ui->editing_node->text, ui->edit_buffer,
                sizeof(ui->editing_node->text) - 1);
        ui->editing_node->text[sizeof(ui->editing_node->text) - 1] = '\0';
        ui->editing_node = NULL;
      }

      if (hit) {
        // Check if clicking on text node to edit (already selected = start
        // editing)
        if (hit->type == NODE_TYPE_TEXT && ui->selected_node == hit &&
            !ui->editing_node) {
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
    if (new_w < 20.0f)
      new_w = 20.0f;
    if (new_h < 20.0f)
      new_h = 20.0f;

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
        cs->current_stroke->type =
            cs->pen_is_marker ? STROKE_TYPE_MARKER : STROKE_TYPE_PEN;

        // Apply alpha for markers if they are opaque
        if (cs->current_stroke->type == STROKE_TYPE_MARKER &&
            (cs->current_stroke->color & 0xFF) == 0xFF) {
          cs->current_stroke->color =
              (cs->current_stroke->color & 0xFFFFFF00) | 0x64; // ~40% opacity
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
          cs->current_stroke->points =
              (Point *)realloc(cs->current_stroke->points,
                               cs->current_stroke->capacity * sizeof(Point));
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
      // smoothness Allocating screen points array on stack if small, else
      // heap

      if ((s->type == STROKE_TYPE_PEN || s->type == STROKE_TYPE_MARKER) &&
          s->count > 3) {
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
        nk_stroke_polyline(canvas_buf, (float *)screen_pts, pt_idx,
                           s->thickness * zoom, col);
        free(screen_pts);
      } else {
        // Fallback / Fast render
        struct nk_vec2 *screen_pts =
            (struct nk_vec2 *)malloc(s->count * sizeof(struct nk_vec2));
        for (int i = 0; i < s->count; i++) {
          screen_pts[i].x = bounds.x + (s->points[i].x - cam_x) * zoom;
          screen_pts[i].y = bounds.y + (s->points[i].y - cam_y) * zoom;
        }
        nk_stroke_polyline(canvas_buf, (float *)screen_pts, s->count,
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
            snprintf(ui->status, sizeof(ui->status), "Failed to load: %s",
                     node->image_path);
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
          nk_fill_rect(canvas_buf,
                       nk_rect(nr.x - 4, nr.y - 2, nr.w + 8, nr.h + 4), 4.0f,
                       nk_rgba(66, 133, 244, 20));
          nk_stroke_rect(canvas_buf,
                         nk_rect(nr.x - 4, nr.y - 2, nr.w + 8, nr.h + 4), 4.0f,
                         1.5f, nk_rgb(66, 133, 244));
        }

        // Draw text
        struct nk_user_font *font =
            ui->font_inter ? &ui->font_inter->handle : ctx->style.font;
        nk_draw_text(canvas_buf, nr, display_text, strlen(display_text), font,
                     nk_rgba(0, 0, 0, 0), nk_rgb(30, 30, 30));

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
            nk_stroke_line(canvas_buf, cursor_x, cursor_y, cursor_x,
                           cursor_y + cursor_h, 2.0f, nk_rgb(66, 133, 244));
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
    if (menu_x < bounds.x)
      menu_x = bounds.x;
    if (menu_x + menu_w > bounds.x + bounds.w)
      menu_x = bounds.x + bounds.w - menu_w;
    if (menu_y < bounds.y)
      menu_y = sy + sel->height * zoom + 10.0f; // Show below if no space above

    // Draw menu background
    nk_fill_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 5.0f,
                 nk_rgba(40, 40, 40, 230));
    nk_stroke_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 5.0f,
                   1.0f, nk_rgba(100, 100, 100, 255));

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
      struct nk_rect circle_rect =
          nk_rect(cx, cy - circle_size / 2, circle_size, circle_size);

      struct nk_color c = {(colors[i] >> 24) & 0xFF, (colors[i] >> 16) & 0xFF,
                           (colors[i] >> 8) & 0xFF, colors[i] & 0xFF};

      // Draw color circle
      nk_fill_circle(canvas_buf, circle_rect, c);

      // Selection indicator
      if (sel->fill_color == colors[i]) {
        nk_stroke_circle(canvas_buf, circle_rect, 2.0f, nk_rgb(255, 255, 255));
      } else {
        nk_stroke_circle(canvas_buf, circle_rect, 1.0f,
                         nk_rgba(255, 255, 255, 100));
      }

      // Interaction
      if (cs->mouse_in_canvas && !in_toolbar && !in_forms_menu &&
          ui->mouse_x >= circle_rect.x &&
          ui->mouse_x < circle_rect.x + circle_rect.w &&
          ui->mouse_y >= circle_rect.y &&
          ui->mouse_y < circle_rect.y + circle_rect.h &&
          cs->mouse_just_pressed) {

        // Set color (Fill and Stroke for now, or just Fill?)
        // User asked "changer sa couleur".
        sel->fill_color = colors[i];
        // Also set stroke to black or white for contrast if needed, or keep
        // as is. Let's keep stroke as is, or maybe default to black stroke if
        // it was transparent? For now just change fill.
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

// -----------------------------------------------------------------------------
// Design Block Helper: Create styled node for design block
// -----------------------------------------------------------------------------

static Node *design_create_styled_node(DesignData *dd, NodeStyleType style,
                                       float x, float y, float w, float h) {
  if (!dd->root)
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

  switch (style) {
  case NODE_STYLE_RECT:
    node->type = NODE_TYPE_RECT;
    node->fill_color = 0x4285F4FF;
    node->stroke_color = 0x1A73E8FF;
    node->stroke_width = 0.0f;
    node->corner_radius = 8.0f;
    break;
  case NODE_STYLE_ELLIPSE:
    node->type = NODE_TYPE_ELLIPSE;
    node->fill_color = 0x34A853FF;
    node->stroke_color = 0x1E8E3EFF;
    node->stroke_width = 0.0f;
    break;
  case NODE_STYLE_TRIANGLE:
    node->type = NODE_TYPE_TRIANGLE;
    node->fill_color = 0xFBBC04FF;
    node->stroke_color = 0xF9A825FF;
    node->stroke_width = 0.0f;
    break;
  case NODE_STYLE_LINE:
    node->type = NODE_TYPE_RECT;
    node->fill_color = 0x5F6368FF;
    node->stroke_color = 0x5F6368FF;
    node->stroke_width = 2.0f;
    node->height = 4.0f;
    break;
  case NODE_STYLE_TEXT:
    node->type = NODE_TYPE_TEXT;
    node->fill_color = 0x00000000;
    node->stroke_color = 0x00000000;
    node->stroke_width = 0.0f;
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
  float toolbar_h = 48.0f;

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

  // Include toolbar in layout height
  nk_layout_row_dynamic(ctx, display_h + toolbar_h + 20.0f, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);

  // Adjust bounds for actual design area (above toolbar)
  bounds.h = display_h;

  // Design state shorthand
  struct DesignInteraction *ds = &ui->design_state;

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
                        (dd->bg_color >> 16) & 0xFF, (dd->bg_color >> 8) & 0xFF,
                        dd->bg_color & 0xFF};
  nk_fill_rect(canvas_buf, bounds, 4.0f, bg);
  nk_stroke_rect(canvas_buf, bounds, 4.0f, 1.0f, nk_rgb(200, 200, 200));

  // --- Render nodes ---
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
      struct nk_color stroke = {
          (node->stroke_color >> 24) & 0xFF, (node->stroke_color >> 16) & 0xFF,
          (node->stroke_color >> 8) & 0xFF, node->stroke_color & 0xFF};

      int has_fill = (node->fill_color & 0xFF) > 10;

      if (node->type == NODE_TYPE_RECT) {
        if (has_fill)
          nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, fill);
        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * scale,
                         node->stroke_width, stroke);
      } else if (node->type == NODE_TYPE_ELLIPSE) {
        float cx = sx + sw / 2;
        float cy = sy + sh / 2;
        if (has_fill)
          nk_fill_circle(canvas_buf, nk_rect(sx, sy, sw, sh), fill);
        if (node->stroke_width > 0)
          nk_stroke_circle(canvas_buf, nk_rect(sx, sy, sw, sh),
                           node->stroke_width, stroke);
        (void)cx;
        (void)cy;
      } else if (node->type == NODE_TYPE_TRIANGLE) {
        float pts[6] = {sx + sw / 2, sy,       // Top
                        sx,          sy + sh,  // Bottom-left
                        sx + sw,     sy + sh}; // Bottom-right
        if (has_fill)
          nk_fill_polygon(canvas_buf, pts, 3, fill);
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, pts, 3, node->stroke_width, stroke);
      } else if (node->type == NODE_TYPE_TEXT) {
        // Text node
        const char *display_text = node->text;
        int is_editing = (ui->editing_node == node);
        if (is_editing) {
          display_text = ui->edit_buffer;
          nk_fill_rect(canvas_buf,
                       nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2), 2.0f,
                       nk_rgba(66, 133, 244, 20));
          nk_stroke_rect(canvas_buf,
                         nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2), 2.0f,
                         1.0f, nk_rgb(66, 133, 244));
        }
        if (display_text && strlen(display_text) > 0) {
          struct nk_user_font *font =
              ui->font_inter ? &ui->font_inter->handle : ctx->style.font;
          nk_draw_text(canvas_buf, nr, display_text, strlen(display_text), font,
                       nk_rgba(0, 0, 0, 0), nk_rgb(0, 0, 0));
          if (is_editing) {
            float tw = font->width(font->userdata, font->height, display_text,
                                   strlen(display_text));
            int show_cursor = ((int)(SDL_GetTicks() / 500) % 2 == 0);
            if (show_cursor) {
              nk_stroke_line(canvas_buf, nr.x + tw + 1, nr.y + 2, nr.x + tw + 1,
                             nr.y + nr.h - 2, 1.5f, nk_rgb(66, 133, 244));
            }
          }
        }
      } else if (node->type == NODE_TYPE_IMAGE) {
        nk_fill_rect(canvas_buf, nr, 4.0f, nk_rgb(240, 240, 240));
        nk_stroke_rect(canvas_buf, nr, 4.0f, 1.0f, nk_rgb(180, 180, 180));
        const char *fname = node->text[0] ? node->text : "Image";
        nk_draw_text(canvas_buf,
                     nk_rect(nr.x + 4, nr.y + nr.h / 2 - 8, nr.w - 8, 16),
                     fname, strlen(fname), ctx->style.font, nk_rgba(0, 0, 0, 0),
                     nk_rgb(100, 100, 100));
      }

      // Selection highlight
      if (node == ui->selected_node && is_active_design) {
        nk_stroke_rect(canvas_buf, nk_rect(sx - 2, sy - 2, sw + 4, sh + 4),
                       2.0f, 2.0f, nk_rgb(66, 133, 244));

        // Resize handles
        float hsz = 8.0f;
        struct nk_color hc = nk_rgb(66, 133, 244);
        // SE
        nk_fill_rect(canvas_buf,
                     nk_rect(sx + sw - hsz / 2, sy + sh - hsz / 2, hsz, hsz),
                     2.0f, hc);
        // SW
        nk_fill_rect(canvas_buf,
                     nk_rect(sx - hsz / 2, sy + sh - hsz / 2, hsz, hsz), 2.0f,
                     hc);
        // NW
        nk_fill_rect(canvas_buf, nk_rect(sx - hsz / 2, sy - hsz / 2, hsz, hsz),
                     2.0f, hc);
        // NE
        nk_fill_rect(canvas_buf,
                     nk_rect(sx + sw - hsz / 2, sy - hsz / 2, hsz, hsz), 2.0f,
                     hc);
      }
    }
  }

  // --- Toolbar (expanded for page navigation) ---
  float toolbar_w = 3 * 40.0f + 4 * 12.0f + 150.0f; // Extra space for page nav
  float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
  float toolbar_y = bounds.y + bounds.h + 8.0f;
  struct nk_color toolbar_bg = nk_rgba(255, 255, 255, 245);
  struct nk_color toolbar_border = nk_rgba(200, 200, 200, 200);

  nk_fill_rect(canvas_buf,
               nk_rect(toolbar_x - 2, toolbar_y + 2, toolbar_w, toolbar_h),
               12.0f, nk_rgba(0, 0, 0, 20));
  nk_fill_rect(canvas_buf, nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h),
               12.0f, toolbar_bg);
  nk_stroke_rect(canvas_buf,
                 nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h), 12.0f,
                 1.0f, toolbar_border);

  float btn_size = 32.0f;
  float btn_spacing = 8.0f;
  float btn_x = toolbar_x + 12.0f;
  float btn_y = toolbar_y + (toolbar_h - btn_size) / 2.0f;

  // Tool icons: 0: Forms, 1: Text, 2: Image
  const char *tool_icons[] = {ICON_SHAPE, "A", "\xEE\xB9\xA4"};
  int use_icon_font[] = {1, 0, 1};
  int tool_count = 3;

  for (int i = 0; i < tool_count; i++) {
    struct nk_rect btn = nk_rect(btn_x + i * (btn_size + btn_spacing), btn_y,
                                 btn_size, btn_size);

    int is_active = (i == 0 && ds->show_forms_menu);
    int hovered = (ui->mouse_x >= btn.x && ui->mouse_x < btn.x + btn.w &&
                   ui->mouse_y >= btn.y && ui->mouse_y < btn.y + btn.h);

    struct nk_color bg_color = is_active ? nk_rgba(66, 133, 244, 80)
                               : hovered ? nk_rgba(0, 0, 0, 20)
                                         : nk_rgba(0, 0, 0, 0);
    nk_fill_rect(canvas_buf, btn, 6.0f, bg_color);

    struct nk_color icon_color =
        is_active ? nk_rgb(66, 133, 244) : nk_rgb(80, 80, 80);
    if (use_icon_font[i] && ui->font_icons) {
      nk_draw_text(canvas_buf,
                   nk_rect(btn.x + 6, btn.y + 6, btn_size - 12, btn_size - 12),
                   tool_icons[i], strlen(tool_icons[i]),
                   &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), icon_color);
    } else {
      nk_draw_text(canvas_buf,
                   nk_rect(btn.x + 8, btn.y + 4, btn_size - 16, btn_size - 8),
                   tool_icons[i], strlen(tool_icons[i]), ctx->style.font,
                   nk_rgba(0, 0, 0, 0), icon_color);
    }

    if (hovered && ds->mouse_just_pressed && is_active_design) {
      if (i == 0) {
        ds->show_forms_menu = !ds->show_forms_menu;
      } else {
        ds->show_forms_menu = 0;
        ds->pending_create = i; // 1=Text, 2=Image
      }
    }
  }

  // --- Page Navigation (right side of toolbar) ---
  {
    float nav_x = toolbar_x + toolbar_w - 140.0f;
    float nav_btn_size = 24.0f;
    float nav_y = toolbar_y + (toolbar_h - nav_btn_size) / 2.0f;

    // Previous page button
    struct nk_rect prev_btn = nk_rect(nav_x, nav_y, nav_btn_size, nav_btn_size);
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
    nk_draw_text(canvas_buf, nk_rect(add_btn.x + 6, add_btn.y + 2, 16, 20), "+",
                 1, ctx->style.font, nk_rgba(0, 0, 0, 0), nk_rgb(66, 133, 244));

    if (add_hover && ds->mouse_just_pressed && is_active_design) {
      int new_idx = block_design_add_page(block);
      if (new_idx >= 0) {
        block_design_goto_page(block, new_idx);
        ui->selected_node = NULL;
      }
    }
  }

  // --- Forms Dropdown Menu ---
  int in_forms_menu = 0;
  if (ds->show_forms_menu) {
    float menu_w = 160.0f;
    float menu_h = 140.0f;
    float menu_x = toolbar_x;
    float menu_y = toolbar_y - menu_h - 8.0f;

    in_forms_menu = (ui->mouse_x >= menu_x && ui->mouse_x < menu_x + menu_w &&
                     ui->mouse_y >= menu_y && ui->mouse_y < menu_y + menu_h);

    nk_fill_rect(canvas_buf, nk_rect(menu_x + 2, menu_y + 3, menu_w, menu_h),
                 10.0f, nk_rgba(0, 0, 0, 25));
    nk_fill_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 10.0f,
                 toolbar_bg);
    nk_stroke_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 10.0f,
                   1.0f, toolbar_border);

    const char *form_labels[] = {"Rectangle", "Ellipse", "Triangle", "Line"};
    float item_h = 30.0f;

    for (int i = 0; i < 4; i++) {
      struct nk_rect item_rect =
          nk_rect(menu_x + 8, menu_y + 8 + i * item_h, menu_w - 16, item_h);
      int item_hover = (ui->mouse_x >= item_rect.x &&
                        ui->mouse_x < item_rect.x + item_rect.w &&
                        ui->mouse_y >= item_rect.y &&
                        ui->mouse_y < item_rect.y + item_rect.h);

      if (item_hover)
        nk_fill_rect(canvas_buf, item_rect, 4.0f, nk_rgba(66, 133, 244, 30));

      nk_draw_text(
          canvas_buf,
          nk_rect(item_rect.x + 8, item_rect.y + 6, item_rect.w - 16, 20),
          form_labels[i], strlen(form_labels[i]), ctx->style.font,
          nk_rgba(0, 0, 0, 0), nk_rgb(60, 60, 60));

      if (item_hover && ds->mouse_just_pressed && is_active_design) {
        ds->pending_create =
            10 + i; // 10=rect, 11=ellipse, 12=triangle, 13=line
        ds->show_forms_menu = 0;
      }
    }
  }

  // Check toolbar area
  int in_toolbar =
      (ui->mouse_x >= toolbar_x && ui->mouse_x < toolbar_x + toolbar_w &&
       ui->mouse_y >= toolbar_y && ui->mouse_y < toolbar_y + toolbar_h);

  // Check mouse in design area
  int mouse_in_design =
      (ui->mouse_x >= bounds.x && ui->mouse_x < bounds.x + bounds.w &&
       ui->mouse_y >= bounds.y && ui->mouse_y < bounds.y + bounds.h);

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
  // Handle click in design area
  else if (mouse_in_design && !in_toolbar && !in_forms_menu &&
           ds->mouse_just_pressed && !on_resize_handle && is_active_design) {
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
    ui->selected_node->y = fmaxf(
        0, fminf(ui->selected_node->y, dd->height - ui->selected_node->height));

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
      // Text
      new_node = design_create_styled_node(dd, NODE_STYLE_TEXT, center_x,
                                           center_y, 120.0f, 30.0f);
    } else if (ds->pending_create == 2) {
      // Image - for now create placeholder
      Node *img = (Node *)calloc(1, sizeof(Node));
      if (img && dd->root) {
        img->id = dd->next_node_id++;
        img->type = NODE_TYPE_IMAGE;
        img->x = center_x;
        img->y = center_y;
        img->width = 150.0f;
        img->height = 100.0f;
        img->visible = 1;
        img->parent = dd->root;
        img->fill_color = 0xFFFFFFFF;
        strncpy(img->text, "Image", sizeof(img->text) - 1);
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
    } else if (ds->pending_create >= 10 && ds->pending_create <= 13) {
      NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
      int idx = ds->pending_create - 10;
      if (idx == 3) {
        new_node = design_create_styled_node(dd, styles[idx], center_x,
                                             center_y, 150.0f, 4.0f);
      } else {
        new_node = design_create_styled_node(dd, styles[idx], center_x,
                                             center_y, def_w, def_h);
      }
    }

    if (new_node) {
      ui->selected_node = new_node;
    }
    ds->pending_create = 0;
  }

  // Reset per-frame mouse state
  ds->mouse_just_pressed = 0;
  ds->mouse_just_released = 0;

  nk_spacing(ctx, 1);
}

void ui_render_block_video(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width) {
  (void)engine;
  (void)width;
  if (!block || block->type != BLOCK_VIDEO)
    return;

  VideoData *vd = &block->data.video;

  // Poll for async transcription completion
  if (vd->is_transcribing && ui->video_state.active_block == block) {
    TranscriptResult *result = whisper_get_async_result();
    if (result) {
      // Transcription finished - copy results
      if (result->success) {
        free(vd->transcript);
        vd->transcript_capacity = result->segment_count;
        vd->transcript = (TranscriptSeg *)calloc(vd->transcript_capacity,
                                                 sizeof(TranscriptSeg));
        vd->transcript_count = 0;
        for (int i = 0; i < result->segment_count; i++) {
          vd->transcript[i].start_ms = result->segments[i].start_ms;
          vd->transcript[i].end_ms = result->segments[i].end_ms;
          strncpy(vd->transcript[i].text, result->segments[i].text,
                  sizeof(vd->transcript[i].text) - 1);
          vd->transcript[i].is_deleted = 0;
          vd->transcript_count++;
        }
        strncpy(vd->language, result->language, sizeof(vd->language) - 1);
      }
      whisper_free_result(result);
      vd->is_transcribing = 0;
    }
  }

  ui_render_expand_toggle(ui, ctx, block);

  // Calculate heights based on content
  float transcript_h = 0.0f;
  if (vd->transcript_count > 0) {
    transcript_h = vd->transcript_count * 28.0f + 20.0f;
    if (transcript_h > 300.0f)
      transcript_h = 300.0f;
  }

  float total_height = 160.0f + transcript_h;
  if (ui->expanded_block_id == block->id) {
    total_height = nk_window_get_height(ctx) - 40.0f;
  }

  nk_layout_row_dynamic(ctx, total_height, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Background
  nk_fill_rect(canvas, bounds, 4.0f, nk_rgb(250, 250, 250));
  nk_stroke_rect(canvas, bounds, 4.0f, 1.0f, nk_rgb(220, 220, 220));

  float y_offset = bounds.y + 12.0f;

  // --- Header with controls ---
  {
    // Title
    const char *title = vd->clip_count > 0 ? "Video Montage" : "Video Block";
    nk_draw_text(canvas, nk_rect(bounds.x + 16, y_offset, 150, 24), title,
                 strlen(title), ctx->style.font, nk_rgba(0, 0, 0, 0),
                 nk_rgb(60, 60, 60));

    // Import button
    float btn_x = bounds.x + bounds.w - 220.0f;
    struct nk_rect import_btn = nk_rect(btn_x, y_offset - 2, 80, 26);
    int import_hover = (ui->mouse_x >= import_btn.x &&
                        ui->mouse_x < import_btn.x + import_btn.w &&
                        ui->mouse_y >= import_btn.y &&
                        ui->mouse_y < import_btn.y + import_btn.h);
    nk_fill_rect(canvas, import_btn, 4.0f,
                 import_hover ? nk_rgb(66, 133, 244) : nk_rgb(100, 100, 100));
    nk_draw_text(canvas, nk_rect(import_btn.x + 12, import_btn.y + 5, 60, 16),
                 "Import", 6, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 nk_rgb(255, 255, 255));

    // Transcribe button (only if video imported)
    if (vd->clip_count > 0 && !vd->is_transcribing) {
      struct nk_rect trans_btn = nk_rect(btn_x + 90, y_offset - 2, 100, 26);
      int trans_hover = (ui->mouse_x >= trans_btn.x &&
                         ui->mouse_x < trans_btn.x + trans_btn.w &&
                         ui->mouse_y >= trans_btn.y &&
                         ui->mouse_y < trans_btn.y + trans_btn.h);
      nk_fill_rect(canvas, trans_btn, 4.0f,
                   trans_hover ? nk_rgb(52, 168, 83) : nk_rgb(80, 160, 80));
      const char *trans_label =
          vd->transcript_count > 0 ? "Re-transcribe" : "Transcribe";
      nk_draw_text(canvas, nk_rect(trans_btn.x + 8, trans_btn.y + 5, 90, 16),
                   trans_label, strlen(trans_label), ctx->style.font,
                   nk_rgba(0, 0, 0, 0), nk_rgb(255, 255, 255));
    } else if (vd->is_transcribing) {
      nk_draw_text(canvas, nk_rect(btn_x + 90, y_offset + 3, 100, 16),
                   "Transcribing...", 15, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(150, 150, 150));
    }

    y_offset += 36.0f;
  }

  // --- Video info / placeholder ---
  if (vd->clip_count == 0) {
    struct nk_rect placeholder =
        nk_rect(bounds.x + 16, y_offset, bounds.w - 32, 80);
    nk_fill_rect(canvas, placeholder, 8.0f, nk_rgb(240, 240, 240));
    nk_stroke_rect(canvas, placeholder, 8.0f, 1.0f, nk_rgb(200, 200, 200));
    nk_draw_text(canvas,
                 nk_rect(placeholder.x + placeholder.w / 2 - 60,
                         placeholder.y + 30, 120, 20),
                 "Drop video here", 15, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 nk_rgb(150, 150, 150));
    y_offset += 90.0f;
  } else {
    // Show video path
    char info[128];
    snprintf(info, sizeof(info), "Source: %s", vd->clips[0].source_path);
    nk_draw_text(canvas, nk_rect(bounds.x + 16, y_offset, bounds.w - 32, 18),
                 info, strlen(info), ctx->style.font, nk_rgba(0, 0, 0, 0),
                 nk_rgb(100, 100, 100));
    y_offset += 24.0f;
  }

  // --- Transcript (Descript-style) ---
  if (vd->transcript_count > 0) {
    nk_draw_text(canvas, nk_rect(bounds.x + 16, y_offset, 100, 18),
                 "Transcript:", 11, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 nk_rgb(80, 80, 80));
    y_offset += 24.0f;

    // Transcript segments
    struct nk_rect transcript_area =
        nk_rect(bounds.x + 16, y_offset, bounds.w - 32, transcript_h - 44.0f);
    nk_fill_rect(canvas, transcript_area, 4.0f, nk_rgb(255, 255, 255));
    nk_stroke_rect(canvas, transcript_area, 4.0f, 1.0f, nk_rgb(230, 230, 230));

    float seg_y = transcript_area.y + 8.0f;
    for (int i = 0; i < vd->transcript_count &&
                    seg_y < transcript_area.y + transcript_area.h - 20.0f;
         i++) {
      TranscriptSeg *seg = &vd->transcript[i];

      // Timestamp
      char ts[32];
      int mins = seg->start_ms / 60000;
      int secs = (seg->start_ms / 1000) % 60;
      snprintf(ts, sizeof(ts), "%02d:%02d", mins, secs);
      nk_draw_text(canvas, nk_rect(transcript_area.x + 8, seg_y, 40, 18), ts,
                   strlen(ts), ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(150, 150, 150));

      // Text (with strikethrough if deleted)
      struct nk_rect text_rect =
          nk_rect(transcript_area.x + 55, seg_y, transcript_area.w - 70, 20);

      int is_selected = (vd->selected_segment == i);
      int is_hovered = (ui->mouse_x >= text_rect.x &&
                        ui->mouse_x < text_rect.x + text_rect.w &&
                        ui->mouse_y >= text_rect.y &&
                        ui->mouse_y < text_rect.y + text_rect.h);

      // Highlight selected/hovered
      if (is_selected) {
        nk_fill_rect(canvas,
                     nk_rect(text_rect.x - 4, text_rect.y - 2, text_rect.w + 8,
                             text_rect.h + 4),
                     2.0f, nk_rgba(66, 133, 244, 30));
      } else if (is_hovered) {
        nk_fill_rect(canvas,
                     nk_rect(text_rect.x - 4, text_rect.y - 2, text_rect.w + 8,
                             text_rect.h + 4),
                     2.0f, nk_rgba(0, 0, 0, 10));
      }

      struct nk_color text_color =
          seg->is_deleted ? nk_rgb(180, 180, 180) : nk_rgb(40, 40, 40);
      nk_draw_text(canvas, text_rect, seg->text, strlen(seg->text),
                   ctx->style.font, nk_rgba(0, 0, 0, 0), text_color);

      // Strikethrough for deleted segments
      if (seg->is_deleted) {
        float tw = ctx->style.font->width(ctx->style.font->userdata,
                                          ctx->style.font->height, seg->text,
                                          strlen(seg->text));
        if (tw > text_rect.w - 10)
          tw = text_rect.w - 10;
        nk_stroke_line(canvas, text_rect.x, text_rect.y + 9, text_rect.x + tw,
                       text_rect.y + 9, 1.0f, nk_rgb(180, 180, 180));
      }

      seg_y += 26.0f;
    }

    // Instructions
    nk_draw_text(
        canvas,
        nk_rect(bounds.x + 16, bounds.y + bounds.h - 24, bounds.w - 32, 16),
        "Click segment to select, Delete/Backspace to cut, R to restore", 61,
        ctx->style.font, nk_rgba(0, 0, 0, 0), nk_rgb(150, 150, 150));
  }

  // --- Click handling ---
  if (ui->video_state.mouse_just_pressed) {
    float btn_x = bounds.x + bounds.w - 220.0f;

    // Check import button
    struct nk_rect import_btn = nk_rect(btn_x, bounds.y + 10.0f, 80, 26);
    if (ui->mouse_x >= import_btn.x &&
        ui->mouse_x < import_btn.x + import_btn.w &&
        ui->mouse_y >= import_btn.y &&
        ui->mouse_y < import_btn.y + import_btn.h) {
      // Open video file dialog
      char video_path[512];
      if (open_video_file_dialog(video_path, sizeof(video_path))) {
        if (vd->clip_count == 0) {
          block_video_add_clip(block, video_path);
        } else {
          // Replace existing
          strncpy(vd->clips[0].source_path, video_path,
                  sizeof(vd->clips[0].source_path) - 1);
        }
        // Clear old transcript
        free(vd->transcript);
        vd->transcript = NULL;
        vd->transcript_count = 0;
        vd->selected_segment = -1;
      }
    }

    // Check transcribe button - START async transcription
    if (vd->clip_count > 0 && !vd->is_transcribing) {
      struct nk_rect trans_btn = nk_rect(btn_x + 90, bounds.y + 10.0f, 100, 26);
      if (ui->mouse_x >= trans_btn.x &&
          ui->mouse_x < trans_btn.x + trans_btn.w &&
          ui->mouse_y >= trans_btn.y &&
          ui->mouse_y < trans_btn.y + trans_btn.h) {
        // Start async transcription (non-blocking)
        if (whisper_transcribe_async(vd->clips[0].source_path)) {
          vd->is_transcribing = 1;
          ui->video_state.active_block = block;
        }
      }
    }

    // Check transcript segment clicks
    if (vd->transcript_count > 0) {
      float transcript_start_y = bounds.y + 96.0f;
      if (vd->clip_count == 0)
        transcript_start_y += 66.0f;

      float seg_y = transcript_start_y + 8.0f;
      for (int i = 0; i < vd->transcript_count; i++) {
        struct nk_rect text_rect =
            nk_rect(bounds.x + 71, seg_y, bounds.w - 102, 20);
        if (ui->mouse_x >= text_rect.x &&
            ui->mouse_x < text_rect.x + text_rect.w &&
            ui->mouse_y >= text_rect.y &&
            ui->mouse_y < text_rect.y + text_rect.h) {
          vd->selected_segment = i;
          break;
        }
        seg_y += 26.0f;
      }
    }
  }

  // Reset mouse_just_pressed at end of frame
  ui->video_state.mouse_just_pressed = 0;

  nk_spacing(ctx, 1);
}

// -----------------------------------------------------------------------------
// Audio Block - Voice Recording with Transcription
// -----------------------------------------------------------------------------

// SDL audio recording state (using queue-based capture, not callbacks)
static SDL_AudioDeviceID audio_recording_device = 0;
static Block *audio_recording_block = NULL;

static void start_audio_recording(Block *block) {
  if (!block || block->type != BLOCK_AUDIO)
    return;
  AudioData *ad = &block->data.audio;

  // Don't start if already recording
  if (ad->is_recording)
    return;

  // List available recording devices
  int num_devices = SDL_GetNumAudioDevices(1); // 1 = recording devices
  fprintf(stderr, "[audio] Found %d recording devices:\n", num_devices);
  for (int i = 0; i < num_devices; i++) {
    fprintf(stderr, "[audio]   %d: %s\n", i, SDL_GetAudioDeviceName(i, 1));
  }

  // Use queue-based recording (no callback)
  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = 16000; // 16kHz for whisper
  want.format = AUDIO_F32;
  want.channels = 1;
  want.samples = 1024;
  want.callback = NULL; // Queue-based capture

  // Open default recording device (NULL = default)
  audio_recording_device =
      SDL_OpenAudioDevice(NULL, 1, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
  if (audio_recording_device == 0) {
    fprintf(stderr, "[audio] Failed to open audio device: %s\n",
            SDL_GetError());
    return;
  }

  fprintf(stderr, "[audio] Opened device %d: freq=%d, format=%d, channels=%d\n",
          audio_recording_device, have.freq, have.format, have.channels);

  // Clear old samples
  free(ad->samples);
  ad->samples = NULL;
  ad->sample_count = 0;
  ad->sample_capacity = 0;

  audio_recording_block = block;
  ad->is_recording = 1;
  SDL_PauseAudioDevice(audio_recording_device, 0); // Start recording
}

// Call this every frame to collect recorded samples
static void poll_audio_recording(void) {
  if (!audio_recording_block || audio_recording_device == 0)
    return;

  AudioData *ad = &audio_recording_block->data.audio;
  if (!ad->is_recording)
    return;

  // Check how much audio is queued
  Uint32 queued = SDL_GetQueuedAudioSize(audio_recording_device);
  if (queued == 0)
    return;

  int sample_count = queued / sizeof(float);

  // Ensure capacity
  if (ad->sample_count + sample_count > ad->sample_capacity) {
    int new_cap = ad->sample_capacity == 0 ? 16000 : ad->sample_capacity * 2;
    while (new_cap < ad->sample_count + sample_count)
      new_cap *= 2;
    ad->samples = (float *)realloc(ad->samples, new_cap * sizeof(float));
    ad->sample_capacity = new_cap;
  }

  // Dequeue recorded audio
  SDL_DequeueAudio(audio_recording_device, ad->samples + ad->sample_count,
                   queued);
  ad->sample_count += sample_count;
}

static void stop_audio_recording(Block *block) {
  if (!block || block->type != BLOCK_AUDIO)
    return;
  AudioData *ad = &block->data.audio;

  if (!ad->is_recording)
    return;

  SDL_PauseAudioDevice(audio_recording_device, 1);
  SDL_CloseAudioDevice(audio_recording_device);
  audio_recording_device = 0;
  audio_recording_block = NULL;
  ad->is_recording = 0;

  // Save to temp file for whisper
  if (ad->sample_count > 0) {
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/aslan_audio_%u.wav", block->id);

    // Write simple WAV file
    FILE *f = fopen(tmp_path, "wb");
    if (f) {
      // WAV header
      int data_size = ad->sample_count * sizeof(int16_t);
      int file_size = 36 + data_size;

      fwrite("RIFF", 1, 4, f);
      fwrite(&file_size, 4, 1, f);
      fwrite("WAVE", 1, 4, f);
      fwrite("fmt ", 1, 4, f);
      int fmt_size = 16;
      fwrite(&fmt_size, 4, 1, f);
      short audio_format = 1; // PCM
      fwrite(&audio_format, 2, 1, f);
      short channels = 1;
      fwrite(&channels, 2, 1, f);
      int sample_rate = 16000;
      fwrite(&sample_rate, 4, 1, f);
      int byte_rate = sample_rate * channels * 2;
      fwrite(&byte_rate, 4, 1, f);
      short block_align = channels * 2;
      fwrite(&block_align, 2, 1, f);
      short bits_per_sample = 16;
      fwrite(&bits_per_sample, 2, 1, f);
      fwrite("data", 1, 4, f);
      fwrite(&data_size, 4, 1, f);

      // Convert float to int16
      for (int i = 0; i < ad->sample_count; i++) {
        float s = ad->samples[i];
        if (s > 1.0f)
          s = 1.0f;
        if (s < -1.0f)
          s = -1.0f;
        int16_t val = (int16_t)(s * 32767.0f);
        fwrite(&val, 2, 1, f);
      }
      fclose(f);
      strncpy(ad->audio_path, tmp_path, sizeof(ad->audio_path) - 1);
    }
  }
}

void ui_render_block_audio(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width) {
  (void)engine;
  (void)width;
  if (!block || block->type != BLOCK_AUDIO)
    return;

  AudioData *ad = &block->data.audio;

  // Poll for recorded audio samples (if recording)
  if (ad->is_recording && audio_recording_block == block) {
    poll_audio_recording();
  }

  // Poll for async transcription completion
  if (ad->is_transcribing && ui->audio_state.active_block == block) {
    TranscriptResult *result = whisper_get_async_result();
    if (result) {
      if (result->success) {
        free(ad->transcript);
        ad->transcript_capacity = result->segment_count;
        ad->transcript = (TranscriptSeg *)calloc(ad->transcript_capacity,
                                                 sizeof(TranscriptSeg));
        ad->transcript_count = 0;
        for (int i = 0; i < result->segment_count; i++) {
          ad->transcript[i].start_ms = result->segments[i].start_ms;
          ad->transcript[i].end_ms = result->segments[i].end_ms;
          strncpy(ad->transcript[i].text, result->segments[i].text,
                  sizeof(ad->transcript[i].text) - 1);
          ad->transcript[i].is_deleted = 0;
          ad->transcript_count++;
        }
      }
      whisper_free_result(result);
      ad->is_transcribing = 0;
    }
  }

  // Track active audio block for keyboard handling
  if (ad->selected_segment >= 0 || ad->is_recording) {
    ui->audio_state.active_block = block;
  }

  // Calculate heights based on content
  float transcript_h = 0.0f;
  if (ad->transcript_count > 0) {
    transcript_h = ad->transcript_count * 28.0f + 20.0f;
    if (transcript_h > 250.0f)
      transcript_h = 250.0f;
  }

  float total_height = 140.0f + transcript_h;
  nk_layout_row_dynamic(ctx, total_height, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Background - light blue tint for audio
  nk_fill_rect(canvas, bounds, 4.0f, nk_rgb(248, 250, 255));
  nk_stroke_rect(canvas, bounds, 4.0f, 1.0f, nk_rgb(200, 210, 230));

  float y_offset = bounds.y + 12.0f;

  // --- Header with controls ---
  {
    // Title with microphone indicator
    const char *title = ad->is_recording ? "Recording..." : "Voice Note";
    struct nk_color title_color =
        ad->is_recording ? nk_rgb(220, 50, 50) : nk_rgb(60, 60, 60);
    nk_draw_text(canvas, nk_rect(bounds.x + 16, y_offset, 150, 24), title,
                 strlen(title), ctx->style.font, nk_rgba(0, 0, 0, 0),
                 title_color);

    // Record/Stop button
    float btn_x = bounds.x + bounds.w - 220.0f;
    struct nk_rect rec_btn = nk_rect(btn_x, y_offset - 2, 80, 26);
    int rec_hover =
        (ui->mouse_x >= rec_btn.x && ui->mouse_x < rec_btn.x + rec_btn.w &&
         ui->mouse_y >= rec_btn.y && ui->mouse_y < rec_btn.y + rec_btn.h);

    if (ad->is_recording) {
      // Stop button (red)
      nk_fill_rect(canvas, rec_btn, 4.0f,
                   rec_hover ? nk_rgb(200, 50, 50) : nk_rgb(220, 70, 70));
      nk_draw_text(canvas, nk_rect(rec_btn.x + 18, rec_btn.y + 5, 50, 16),
                   "Stop", 4, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(255, 255, 255));
    } else {
      // Record button (blue)
      nk_fill_rect(canvas, rec_btn, 4.0f,
                   rec_hover ? nk_rgb(66, 133, 244) : nk_rgb(100, 140, 200));
      nk_draw_text(canvas, nk_rect(rec_btn.x + 10, rec_btn.y + 5, 60, 16),
                   "Record", 6, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(255, 255, 255));
    }

    // Transcribe button (only if audio recorded and not transcribing)
    if (ad->audio_path[0] != '\0' && !ad->is_recording &&
        !ad->is_transcribing) {
      struct nk_rect trans_btn = nk_rect(btn_x + 90, y_offset - 2, 100, 26);
      int trans_hover = (ui->mouse_x >= trans_btn.x &&
                         ui->mouse_x < trans_btn.x + trans_btn.w &&
                         ui->mouse_y >= trans_btn.y &&
                         ui->mouse_y < trans_btn.y + trans_btn.h);
      nk_fill_rect(canvas, trans_btn, 4.0f,
                   trans_hover ? nk_rgb(52, 168, 83) : nk_rgb(80, 160, 80));
      const char *trans_label =
          ad->transcript_count > 0 ? "Re-transcribe" : "Transcribe";
      nk_draw_text(canvas, nk_rect(trans_btn.x + 8, trans_btn.y + 5, 90, 16),
                   trans_label, strlen(trans_label), ctx->style.font,
                   nk_rgba(0, 0, 0, 0), nk_rgb(255, 255, 255));
    } else if (ad->is_transcribing) {
      nk_draw_text(canvas, nk_rect(btn_x + 90, y_offset + 3, 100, 16),
                   "Transcribing...", 15, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(150, 150, 150));
    }

    y_offset += 36.0f;
  }

  // --- Audio info / waveform placeholder ---
  {
    struct nk_rect waveform =
        nk_rect(bounds.x + 16, y_offset, bounds.w - 32, 50);
    nk_fill_rect(canvas, waveform, 8.0f, nk_rgb(240, 243, 250));
    nk_stroke_rect(canvas, waveform, 8.0f, 1.0f, nk_rgb(200, 210, 230));

    if (ad->is_recording) {
      // Animated recording indicator
      nk_fill_circle(
          canvas,
          nk_rect(waveform.x + waveform.w / 2 - 8, waveform.y + 17, 16, 16),
          nk_rgb(220, 50, 50));
    } else if (ad->sample_count > 0) {
      // Simple waveform visualization
      float step = (float)ad->sample_count / (waveform.w - 20);
      float center_y = waveform.y + waveform.h / 2;
      for (float x = 0; x < waveform.w - 20; x += 2) {
        int idx = (int)(x * step);
        if (idx >= ad->sample_count)
          break;
        float amp = ad->samples[idx] * 20.0f;
        nk_stroke_line(canvas, waveform.x + 10 + x, center_y - amp,
                       waveform.x + 10 + x, center_y + amp, 1.0f,
                       nk_rgb(100, 140, 200));
      }
    } else {
      nk_draw_text(
          canvas,
          nk_rect(waveform.x + waveform.w / 2 - 60, waveform.y + 17, 120, 16),
          "Click Record to start", 21, ctx->style.font, nk_rgba(0, 0, 0, 0),
          nk_rgb(150, 150, 170));
    }

    // Duration display
    if (ad->sample_count > 0) {
      int duration_ms = (ad->sample_count * 1000) / 16000;
      int mins = duration_ms / 60000;
      int secs = (duration_ms / 1000) % 60;
      char dur[32];
      snprintf(dur, sizeof(dur), "%02d:%02d", mins, secs);
      nk_draw_text(canvas,
                   nk_rect(waveform.x + waveform.w - 50,
                           waveform.y + waveform.h - 20, 40, 16),
                   dur, strlen(dur), ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(100, 100, 120));
    }

    y_offset += 60.0f;
  }

  // --- Transcript (Descript-style) ---
  if (ad->transcript_count > 0) {
    nk_draw_text(canvas, nk_rect(bounds.x + 16, y_offset, 100, 18),
                 "Transcript:", 11, ctx->style.font, nk_rgba(0, 0, 0, 0),
                 nk_rgb(80, 80, 100));
    y_offset += 24.0f;

    struct nk_rect transcript_area =
        nk_rect(bounds.x + 16, y_offset, bounds.w - 32, transcript_h - 44.0f);
    nk_fill_rect(canvas, transcript_area, 4.0f, nk_rgb(255, 255, 255));
    nk_stroke_rect(canvas, transcript_area, 4.0f, 1.0f, nk_rgb(220, 225, 235));

    float seg_y = transcript_area.y + 8.0f;
    for (int i = 0; i < ad->transcript_count &&
                    seg_y < transcript_area.y + transcript_area.h - 20.0f;
         i++) {
      TranscriptSeg *seg = &ad->transcript[i];

      // Timestamp
      char ts[32];
      int mins = seg->start_ms / 60000;
      int secs = (seg->start_ms / 1000) % 60;
      snprintf(ts, sizeof(ts), "%02d:%02d", mins, secs);
      nk_draw_text(canvas, nk_rect(transcript_area.x + 8, seg_y, 40, 18), ts,
                   strlen(ts), ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(140, 140, 160));

      // Text (with strikethrough if deleted)
      struct nk_rect text_rect =
          nk_rect(transcript_area.x + 55, seg_y, transcript_area.w - 70, 20);

      int is_selected = (ad->selected_segment == i);
      int is_hovered = (ui->mouse_x >= text_rect.x &&
                        ui->mouse_x < text_rect.x + text_rect.w &&
                        ui->mouse_y >= text_rect.y &&
                        ui->mouse_y < text_rect.y + text_rect.h);

      if (is_selected) {
        nk_fill_rect(canvas,
                     nk_rect(text_rect.x - 4, text_rect.y - 2, text_rect.w + 8,
                             text_rect.h + 4),
                     2.0f, nk_rgba(100, 140, 200, 30));
      } else if (is_hovered) {
        nk_fill_rect(canvas,
                     nk_rect(text_rect.x - 4, text_rect.y - 2, text_rect.w + 8,
                             text_rect.h + 4),
                     2.0f, nk_rgba(0, 0, 0, 10));
      }

      struct nk_color text_color =
          seg->is_deleted ? nk_rgb(170, 170, 180) : nk_rgb(40, 40, 50);
      nk_draw_text(canvas, text_rect, seg->text, strlen(seg->text),
                   ctx->style.font, nk_rgba(0, 0, 0, 0), text_color);

      // Strikethrough for deleted
      if (seg->is_deleted) {
        float tw = ctx->style.font->width(ctx->style.font->userdata,
                                          ctx->style.font->height, seg->text,
                                          strlen(seg->text));
        if (tw > text_rect.w - 10)
          tw = text_rect.w - 10;
        nk_stroke_line(canvas, text_rect.x, text_rect.y + 9, text_rect.x + tw,
                       text_rect.y + 9, 1.0f, nk_rgb(170, 170, 180));
      }

      seg_y += 26.0f;
    }
  }

  // --- Click handling ---
  int mouse_in_bounds =
      (ui->mouse_x >= bounds.x && ui->mouse_x < bounds.x + bounds.w &&
       ui->mouse_y >= bounds.y && ui->mouse_y < bounds.y + bounds.h);
  static int audio_mouse_was_down = 0;
  int mouse_down = (ui->mouse_buttons & 1);
  int mouse_just_pressed =
      mouse_down && !audio_mouse_was_down && mouse_in_bounds;
  audio_mouse_was_down = mouse_down;

  if (mouse_just_pressed) {
    float btn_x = bounds.x + bounds.w - 220.0f;

    // Check record/stop button
    struct nk_rect rec_btn = nk_rect(btn_x, bounds.y + 10.0f, 80, 26);
    if (ui->mouse_x >= rec_btn.x && ui->mouse_x < rec_btn.x + rec_btn.w &&
        ui->mouse_y >= rec_btn.y && ui->mouse_y < rec_btn.y + rec_btn.h) {
      if (ad->is_recording) {
        stop_audio_recording(block);
      } else {
        start_audio_recording(block);
      }
    }

    // Check transcribe button
    if (ad->audio_path[0] != '\0' && !ad->is_recording &&
        !ad->is_transcribing) {
      struct nk_rect trans_btn = nk_rect(btn_x + 90, bounds.y + 10.0f, 100, 26);
      if (ui->mouse_x >= trans_btn.x &&
          ui->mouse_x < trans_btn.x + trans_btn.w &&
          ui->mouse_y >= trans_btn.y &&
          ui->mouse_y < trans_btn.y + trans_btn.h) {
        // Start async transcription (non-blocking)
        if (whisper_transcribe_async(ad->audio_path)) {
          ad->is_transcribing = 1;
          ui->audio_state.active_block = block;
        }
      }
    }

    // Check transcript segment clicks
    if (ad->transcript_count > 0) {
      float transcript_start_y = bounds.y + 120.0f;
      float seg_y = transcript_start_y + 8.0f;
      for (int i = 0; i < ad->transcript_count; i++) {
        struct nk_rect text_rect =
            nk_rect(bounds.x + 71, seg_y, bounds.w - 102, 20);
        if (ui->mouse_x >= text_rect.x &&
            ui->mouse_x < text_rect.x + text_rect.w &&
            ui->mouse_y >= text_rect.y &&
            ui->mouse_y < text_rect.y + text_rect.h) {
          ad->selected_segment = i;
          break;
        }
        seg_y += 26.0f;
      }
    }
  }

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
  case BLOCK_VIDEO:
    ui_render_block_video(ui, engine, ctx, block, width);
    break;
  case BLOCK_AUDIO:
    ui_render_block_audio(ui, engine, ctx, block, width);
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
    const float bottom_h = 36.0f;
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

// Helper: Create new document with default setup
static void home_create_new_document(UIHost *ui, Engine *engine) {
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
    for (int p = 0; p < 32; p++) {
      if (!engine->doc->pages[p].in_use)
        continue;

      struct nk_rect item_rect =
          nk_rect(center_x, center_y, col_center_w - 60, 44);
      int item_hover = (ui->mouse_x >= item_rect.x &&
                        ui->mouse_x < item_rect.x + item_rect.w &&
                        ui->mouse_y >= item_rect.y &&
                        ui->mouse_y < item_rect.y + item_rect.h);

      if (item_hover) {
        nk_fill_rect(canvas, item_rect, 6.0f, hover_bg);
      }

      // Document icon (simple rectangle)
      nk_fill_rect(canvas, nk_rect(item_rect.x + 12, item_rect.y + 10, 24, 24),
                   4.0f, accent);

      // Document name
      nk_draw_text(
          canvas,
          nk_rect(item_rect.x + 48, item_rect.y + 13, item_rect.w - 60, 20),
          engine->doc->pages[p].name, strlen(engine->doc->pages[p].name),
          ctx->style.font, nk_rgba(0, 0, 0, 0), text_primary);

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

    // =======================================================================
    // CLICK HANDLING
    // =======================================================================
    static int home_mouse_was_down = 0;
    int mouse_down = (ui->mouse_buttons & 1);
    int mouse_just_pressed = mouse_down && !home_mouse_was_down;
    home_mouse_was_down = mouse_down;

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

      // Recent documents
      float check_y = top_margin + 35;
      for (int p = 0; p < 32; p++) {
        if (!engine->doc->pages[p].in_use)
          continue;

        struct nk_rect item_rect =
            nk_rect(center_x, check_y, col_center_w - 60, 44);
        if (ui->mouse_x >= item_rect.x &&
            ui->mouse_x < item_rect.x + item_rect.w &&
            ui->mouse_y >= item_rect.y &&
            ui->mouse_y < item_rect.y + item_rect.h) {
          doc_page_activate(engine->doc, engine->doc->pages[p].id);
          ui->screen = 1;
          break;
        }
        check_y += 48;
        if (check_y > top_margin + 35 + 48 * 10)
          break;
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
