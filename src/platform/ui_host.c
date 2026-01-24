// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear implementation (must be before nuklear.h include)
#define NK_IMPLEMENTATION
#include "ui_host.h"
#include "../scripting/lua_host.h"
#include <SDL.h>
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

  // Load fonts
  nk_sdl_font_stash_begin(&ui->atlas);

  // Try to load custom fonts, fall back to default
  ui->font_inter = nk_font_atlas_add_default(ui->atlas, 16.0f, NULL);
  ui->font_mono = nk_font_atlas_add_default(ui->atlas, 15.0f, NULL);
  ui->font_h1 = nk_font_atlas_add_default(ui->atlas, 28.0f, NULL);
  ui->font_h2 = nk_font_atlas_add_default(ui->atlas, 24.0f, NULL);
  ui->font_h3 = nk_font_atlas_add_default(ui->atlas, 18.0f, NULL);

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
  nk_sdl_handle_event(evt);
}

// -----------------------------------------------------------------------------
// Top Bar - Block Insertion Menu
// -----------------------------------------------------------------------------

void ui_render_top_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                       int win_w) {
  if (!ui || !engine || !ctx)
    return;

  const float bar_h = 40.0f;
  const float btn_w = 36.0f;
  const float btn_h = 28.0f;

  if (nk_begin(ctx, "TopBar", nk_rect(0, 0, (float)win_w, bar_h),
               NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {

    nk_layout_row_static(ctx, btn_h, (int)btn_w, 12);

    Page *page = doc_page_get_active(engine->doc);
    uint32_t page_id = page ? page->id : 0;

    // Text block
    if (nk_button_label(ctx, "T")) {
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

    // H1 block
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

    // Separator
    if (nk_button_label(ctx, "--")) {
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

    // Lua script
    if (nk_button_label(ctx, "Lua")) {
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

    // Table
    if (nk_button_label(ctx, "Tab")) {
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

    // Canvas
    if (nk_button_label(ctx, "Can")) {
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

    // Design
    if (nk_button_label(ctx, "Des")) {
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

    // Video
    if (nk_button_label(ctx, "Vid")) {
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

    // Callout
    if (nk_button_label(ctx, "!")) {
      if (page) {
        Block *b = block_create(page, BLOCK_CALLOUT);
        if (b) {
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u callout", page_id,
                   b->id);
          engine_execute_command(engine, cmd);
        }
      }
    }

    // Checklist
    if (nk_button_label(ctx, "[]")) {
      if (page) {
        Block *b = block_create(page, BLOCK_CHECKLIST);
        if (b) {
          page_append_block(page, b);
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_create %u %u checklist", page_id,
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
    if (nk_button_label(ctx, "Home")) {
      ui->screen = 0;
    }
  }
  nk_end(ctx);
}

// -----------------------------------------------------------------------------
// Block Renderers
// -----------------------------------------------------------------------------

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

void ui_render_block_canvas(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
  if (!block || block->type != BLOCK_CANVAS)
    return;

  const float canvas_h = 300.0f;
  nk_layout_row_dynamic(ctx, canvas_h, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Background
  nk_fill_rect(canvas, bounds, 0, COLOR_PANEL);
  nk_stroke_rect(canvas, bounds, 0, 1.0f, COLOR_BORDER);

  // Render nodes
  CanvasData *cd = &block->data.canvas;
  if (cd->root) {
    for (Node *node = cd->root->first_child; node; node = node->next_sibling) {
      if (!node->visible)
        continue;

      float sx = bounds.x + (node->x - cd->view_x) * cd->zoom;
      float sy = bounds.y + (node->y - cd->view_y) * cd->zoom;
      float sw = node->width * cd->zoom;
      float sh = node->height * cd->zoom;

      struct nk_rect nr = nk_rect(sx, sy, sw, sh);

      // Convert uint32_t RGBA to nk_color
      struct nk_color fill = {
          (node->fill_color >> 24) & 0xFF, (node->fill_color >> 16) & 0xFF,
          (node->fill_color >> 8) & 0xFF, node->fill_color & 0xFF};

      nk_fill_rect(canvas, nr, node->corner_radius, fill);

      if (node->stroke_width > 0) {
        struct nk_color stroke = {(node->stroke_color >> 24) & 0xFF,
                                  (node->stroke_color >> 16) & 0xFF,
                                  (node->stroke_color >> 8) & 0xFF,
                                  node->stroke_color & 0xFF};
        nk_stroke_rect(canvas, nr, node->corner_radius, node->stroke_width,
                       stroke);
      }

      // Text
      if (node->type == NODE_TYPE_TEXT && node->text[0]) {
        nk_draw_text(canvas, nr, node->text, strlen(node->text),
                     ctx->style.font, fill, COLOR_TEXT);
      }
    }
  }

  // Label
  nk_draw_text(canvas, nk_rect(bounds.x + 10, bounds.y + 10, 200, 20),
               "Canvas (Infinite)", 17, ctx->style.font, COLOR_PANEL,
               COLOR_TEXT_DIM);

  nk_spacing(ctx, 1);
}

void ui_render_block_design(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;
  float scale = width / (float)dd->width;
  if (scale > 1.0f)
    scale = 1.0f;

  float display_h = (float)dd->height * scale;
  if (display_h > 400.0f)
    display_h = 400.0f;

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

  nk_layout_row_dynamic(ctx, 200.0f, 1);

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
  nk_layout_row_static(ctx, 24.0f, 60, 2);
  if (nk_button_label(ctx, "Run")) {
    if (ui->L) {
      block_lua_execute(block, ui->L);
    }
  }
  if (nk_button_label(ctx, "Clear")) {
    block_lua_clear_output(block);
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

void ui_render_block_callout(UIHost *ui, struct nk_context *ctx, Block *block,
                             float width) {
  if (!block || block->type != BLOCK_CALLOUT)
    return;

  nk_layout_row_dynamic(ctx, 60.0f, 1);

  struct nk_rect bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Theme colors
  struct nk_color bg, border;
  switch (block->data.callout.theme) {
  case CALLOUT_ALERT:
    bg = nk_rgb(80, 40, 40);
    border = nk_rgb(200, 80, 80);
    break;
  case CALLOUT_STAR:
    bg = nk_rgb(80, 70, 30);
    border = nk_rgb(200, 180, 80);
    break;
  case CALLOUT_CHECK:
    bg = nk_rgb(30, 70, 50);
    border = nk_rgb(80, 180, 120);
    break;
  case CALLOUT_IDEA:
    bg = nk_rgb(50, 50, 80);
    border = nk_rgb(140, 140, 220);
    break;
  default:
    bg = nk_rgb(40, 50, 60);
    border = nk_rgb(80, 140, 200);
    break;
  }

  nk_fill_rect(canvas, bounds, 4.0f, bg);
  nk_stroke_rect(canvas, bounds, 4.0f, 2.0f, border);

  // Text
  char buf[1024];
  strncpy(buf, block->text ? block->text : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  nk_layout_row_dynamic(ctx, 40.0f, 1);
  nk_flags flags = NK_EDIT_FIELD;
  nk_edit_string_zero_terminated(ctx, flags, buf, (int)sizeof(buf) - 1,
                                 nk_filter_default);

  if (strcmp(buf, block->text ? block->text : "") != 0) {
    block_set_text(block, buf);
  }
}

void ui_render_block_checklist(UIHost *ui, Engine *engine,
                               struct nk_context *ctx, Block *block,
                               float width) {
  if (!block || block->type != BLOCK_CHECKLIST)
    return;

  nk_layout_row_static(ctx, 24.0f, 24, 1);

  // Checkbox
  int checked = block->data.checklist.is_checked ? 1 : 0;
  if (nk_checkbox_label(ctx, "", &checked)) {
    block->data.checklist.is_checked = checked != 0;
  }

  // Text
  nk_layout_row_dynamic(ctx, 24.0f, 1);

  char buf[512];
  strncpy(buf, block->text ? block->text : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  struct nk_color text_color =
      block->data.checklist.is_checked ? COLOR_TEXT_DIM : COLOR_TEXT;

  nk_flags flags = NK_EDIT_FIELD;
  nk_edit_string_zero_terminated(ctx, flags, buf, (int)sizeof(buf) - 1,
                                 nk_filter_default);

  if (strcmp(buf, block->text ? block->text : "") != 0) {
    block_set_text(block, buf);
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
  case BLOCK_CALLOUT:
    ui_render_block_callout(ui, ctx, block, width);
    break;
  case BLOCK_CHECKLIST:
    ui_render_block_checklist(ui, engine, ctx, block, width);
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

  // Top bar (only in editor)
  if (ui->screen == 1) {
    ui_render_top_bar(ui, engine, ctx, win_w);
  }

  // Home or Content area
  if (ui->screen == 0) {
    ui_render_home(ui, engine, ctx, win_w, win_h);
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
  ui_render_bottom_bar(ui, engine, ctx, win_w, win_h);

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
