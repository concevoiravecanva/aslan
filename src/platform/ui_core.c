#include "ui_core.h"
#include "../core/engine.h"
#include "../document/document.h"
#include "recents.h" // For home screen access to recents
#include "ui_shared.h"
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

// -----------------------------------------------------------------------------
// Theme Application
// -----------------------------------------------------------------------------

void ui_apply_theme(UIHost *ui) {
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
