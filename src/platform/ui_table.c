// =============================================================================
// ASLAN UI TABLE - Table Block Rendering and Logic
// =============================================================================

#include "ui_table.h"
#include "../document/aslan_table.h"
#include "ui_shared.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Helper: Get icon for column type
const char *table_col_type_icon(AslanColumnType type) {
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
const char *table_col_type_name(AslanColumnType type) {
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

// Helper: Render popup header with title and close button. Returns 1 if close
// was clicked.
static int table_popup_header(UIHost *ui, struct nk_context *ctx,
                              const char *title) {
  struct nk_color title_col =
      ui->dark_mode ? nk_rgb(220, 220, 225) : nk_rgb(40, 40, 45);
  struct nk_color dim_col =
      ui->dark_mode ? nk_rgb(130, 130, 140) : nk_rgb(140, 140, 148);

  nk_layout_row_template_begin(ctx, 24);
  nk_layout_row_template_push_variable(ctx, 60);
  nk_layout_row_template_push_static(ctx, 24);
  nk_layout_row_template_end(ctx);

  nk_label_colored(ctx, title, NK_TEXT_LEFT, title_col);

  struct nk_style_button close_style = ctx->style.button;
  close_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
  close_style.hover = nk_style_item_color(
      ui->dark_mode ? nk_rgba(255, 255, 255, 20) : nk_rgba(0, 0, 0, 10));
  close_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
  close_style.border = 0;
  close_style.rounding = 4;
  close_style.text_normal = dim_col;
  close_style.text_hover =
      ui->dark_mode ? nk_rgb(255, 255, 255) : nk_rgb(40, 40, 45);
  close_style.text_active = dim_col;
  close_style.padding = nk_vec2(0, 0);
  int closed = nk_button_label_styled(ctx, &close_style, "x");

  // Separator line
  nk_layout_row_dynamic(ctx, 1, 1);
  struct nk_rect sep = nk_widget_bounds(ctx);
  nk_fill_rect(nk_window_get_canvas(ctx), sep, 0,
               ui->dark_mode ? nk_rgb(60, 62, 68) : nk_rgb(230, 230, 235));
  nk_spacing(ctx, 1);

  nk_layout_row_dynamic(ctx, 4, 1);
  nk_spacing(ctx, 1);

  return closed;
}

// Helper: Ensure popup stays inside viewport
struct nk_rect table_smart_popup_rect(UIHost *ui, float x, float y, float w,
                                      float h) {
  struct nk_rect r = nk_rect(x, y, w, h);
  if (r.x + r.w > ui->win_width - 20)
    r.x = ui->win_width - r.w - 20;
  if (r.y + r.h > ui->win_height - 20)
    r.y = ui->win_height - r.h - 20;
  if (r.x < 10)
    r.x = 10;
  if (r.y < 10)
    r.y = 10;
  return r;
}

// Helper: Formula Bar UI
static void ui_render_formula_bar(UIHost *ui, struct nk_context *ctx,
                                  Block *block) {
  if (!block || block->type != BLOCK_TABLE)
    return;
  TableData *td = &block->data.table;
  AslanTable *tbl = td->columnar;
  if (!tbl)
    return;

  int c_idx = ui->table_state.active_col_idx;
  if (c_idx < 0 || c_idx >= (int)tbl->column_count) {
    return;
  }

  // Only show for formula columns
  if (tbl->columns[c_idx].type != ASLAN_COL_FORMULA) {
    return;
  }

  // Draw formula bar background with accent
  nk_layout_row_dynamic(ctx, 34, 1);
  struct nk_rect bar_bounds = nk_widget_bounds(ctx);
  struct nk_command_buffer *bar_canvas = nk_window_get_canvas(ctx);

  // Accent background for formula bar
  struct nk_color bar_bg =
      ui->dark_mode ? nk_rgb(28, 35, 50) : nk_rgb(235, 242, 255);
  nk_fill_rect(bar_canvas, bar_bounds, 4.0f, bar_bg);

  // Left accent stripe
  struct nk_color accent = nk_rgb(45, 127, 249);
  nk_fill_rect(bar_canvas,
               nk_rect(bar_bounds.x, bar_bounds.y + 4, 3, bar_bounds.h - 8),
               1.5f, accent);

  // Draw "fx" label in accent color
  if (ui->font_inter) {
    struct nk_color fx_col = accent;
    nk_draw_text(bar_canvas,
                 nk_rect(bar_bounds.x + 10, bar_bounds.y + 8, 20, 18), "fx", 2,
                 &ui->font_inter->handle, nk_rgba(0, 0, 0, 0), fx_col);
  }

  // Consume the dynamic row widget
  nk_label(ctx, "", NK_TEXT_LEFT);

  // Bar layout: spacer for fx + input field
  nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
  nk_layout_row_push(ctx, 34); // space for fx icon
  nk_label(ctx, "", NK_TEXT_LEFT);

  // Input field
  float width = nk_window_get_content_region(ctx).w - 44;
  nk_layout_row_push(ctx, width > 10 ? width : 300);

  // Initial population: load formula or show "="
  if (ui->table_state.formula_buffer[0] == '\0' &&
      tbl->columns[c_idx].formula) {
    snprintf(ui->table_state.formula_buffer,
             sizeof(ui->table_state.formula_buffer), "=%s",
             tbl->columns[c_idx].formula);
  } else if (ui->table_state.formula_buffer[0] == '\0' &&
             !tbl->columns[c_idx].formula) {
    snprintf(ui->table_state.formula_buffer,
             sizeof(ui->table_state.formula_buffer), "=");
  }

  // Style the edit field
  struct nk_style_edit old_edit = ctx->style.edit;
  ctx->style.edit.normal = nk_style_item_color(bar_bg);
  ctx->style.edit.hover = nk_style_item_color(bar_bg);
  ctx->style.edit.active = nk_style_item_color(bar_bg);
  ctx->style.edit.border = 0;
  ctx->style.edit.text_normal =
      ui->dark_mode ? nk_rgb(220, 225, 235) : nk_rgb(30, 30, 40);
  ctx->style.edit.text_hover = ctx->style.edit.text_normal;
  ctx->style.edit.text_active = ctx->style.edit.text_normal;

  nk_flags res = nk_edit_string_zero_terminated(
      ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER | NK_EDIT_SELECTABLE,
      ui->table_state.formula_buffer,
      sizeof(ui->table_state.formula_buffer) - 1, nk_filter_default);

  ctx->style.edit = old_edit;

  // Track if formula bar is being edited (for click-to-reference)
  // Keep is_editing_formula active even when bar loses focus briefly
  if (res & NK_EDIT_ACTIVE) {
    ui->table_state.is_editing_formula = 1;
  }

  nk_layout_row_end(ctx);

  // Hint text below formula bar when editing
  if (ui->table_state.is_editing_formula) {
    nk_layout_row_dynamic(ctx, 16, 1);
    struct nk_color hint_col = ui->dark_mode ? nk_rgba(130, 150, 200, 180)
                                             : nk_rgba(80, 100, 160, 180);
    nk_label_colored(ctx,
                     "  Click column headers to insert [ColName] references",
                     NK_TEXT_LEFT, hint_col);
  }

  if (res & NK_EDIT_COMMITED) {
    // Parse leading '='
    char *expr = ui->table_state.formula_buffer;
    while (*expr == ' ' || *expr == '\t')
      expr++;
    if (*expr == '=')
      expr++;

    aslan_table_set_formula(tbl, c_idx, expr);

    // Re-evaluate all formulas immediately
    aslan_table_eval_formulas(tbl);
    ui->table_state.is_editing_formula = 0;
    ui_request_redraw(ui);
  }

  // Separator line
  nk_layout_row_dynamic(ctx, 2, 1);
  struct nk_rect sep = nk_widget_bounds(ctx);
  nk_fill_rect(bar_canvas, sep, 0,
               ui->dark_mode ? nk_rgba(255, 255, 255, 15)
                             : nk_rgba(0, 0, 0, 8));
  nk_label(ctx, "", NK_TEXT_LEFT);
}

// Helper: Formula Help Popup
static void ui_render_formula_help_popup(UIHost *ui, struct nk_context *ctx) {
  if (!ui->table_state.show_formula_help)
    return;

  struct nk_rect s = {0, 0, (float)ui->win_width, (float)ui->win_height};
  if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Formula Help",
                     NK_WINDOW_CLOSABLE | NK_WINDOW_TITLE | NK_WINDOW_BORDER,
                     nk_rect(s.w / 2 - 250, s.h / 2 - 225, 500, 450))) {

    nk_layout_row_dynamic(ctx, 20, 1);
    nk_label(ctx, "Formula Syntax", NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 15, 1);
    nk_label(ctx, "• Start with = sign", NK_TEXT_LEFT);
    nk_label(ctx, "• Reference columns with [ColumnName]", NK_TEXT_LEFT);
    nk_label(ctx, "• Click column headers while editing to insert reference",
             NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 10, 1); // spacer
    nk_label(ctx, "", NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 20, 1);
    nk_label(ctx, "Operators", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 15, 1);
    nk_label(ctx, "+ - * / ^  (add, subtract, multiply, divide, power)",
             NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 10, 1); // spacer
    nk_label(ctx, "", NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 20, 1);
    nk_label(ctx, "Functions", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 15, 1);
    nk_label(ctx, "abs(x)      - Absolute value", NK_TEXT_LEFT);
    nk_label(ctx, "sqrt(x)     - Square root", NK_TEXT_LEFT);
    nk_label(ctx, "pow(x,y)    - x to the power of y", NK_TEXT_LEFT);
    nk_label(ctx, "sin(x)      - Sine", NK_TEXT_LEFT);
    nk_label(ctx, "cos(x)      - Cosine", NK_TEXT_LEFT);
    nk_label(ctx, "tan(x)      - Tangent", NK_TEXT_LEFT);
    nk_label(ctx, "ln(x)       - Natural logarithm", NK_TEXT_LEFT);
    nk_label(ctx, "log10(x)    - Base-10 logarithm", NK_TEXT_LEFT);
    nk_label(ctx, "exp(x)      - e to the power of x", NK_TEXT_LEFT);
    nk_label(ctx, "floor(x)    - Round down", NK_TEXT_LEFT);
    nk_label(ctx, "ceil(x)     - Round up", NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 10, 1); // spacer
    nk_label(ctx, "", NK_TEXT_LEFT);

    nk_layout_row_dynamic(ctx, 20, 1);
    nk_label(ctx, "Examples", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 15, 1);
    nk_label(ctx, "=[Price] * 1.2", NK_TEXT_LEFT);
    nk_label(ctx, "=[Quantity] * [Price]", NK_TEXT_LEFT);
    nk_label(ctx, "=sqrt([Value])", NK_TEXT_LEFT);
    nk_label(ctx, "=([A] + [B]) / 2", NK_TEXT_LEFT);

    nk_popup_end(ctx);
  } else {
    ui->table_state.show_formula_help = 0;
  }
}

void ui_render_block_table(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width) {
  (void)width;
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  AslanTable *tbl = td->columnar;

  // --- Style Save State (for Global Restoration) ---
  const struct nk_user_font *old_table_font = ctx->style.font;
  struct nk_style_item old_edit_normal = ctx->style.edit.normal;
  struct nk_style_item old_edit_hover = ctx->style.edit.hover;
  struct nk_style_item old_edit_active = ctx->style.edit.active;
  struct nk_vec2 old_window_padding = ctx->style.window.padding;
  struct nk_vec2 old_group_padding = ctx->style.window.group_padding;
  struct nk_vec2 old_window_spacing = ctx->style.window.spacing;
  struct nk_vec2 old_button_padding = ctx->style.button.padding;
  // printf("[DEBUG] Render Table %u. Tbl: %p, Rows: %zu\n", block->id,
  // (void*)tbl, tbl ? tbl->row_count : 0);

  // Constants
  const float row_h = 30.0f;
  const float header_h = 34.0f;
  const float footer_h = 30.0f;
  const float min_col_w = 80.0f;
  const float max_col_w = 500.0f;
  const float row_num_w = 44.0f;
  // Colors — Airtable-inspired warm palette
  struct nk_color header_bg =
      ui->dark_mode ? nk_rgb(38, 39, 44) : nk_rgb(245, 245, 244);
  struct nk_color border_color =
      ui->dark_mode ? nk_rgba(60, 62, 68, 100) : nk_rgba(228, 228, 226, 180);
  struct nk_color footer_bg =
      ui->dark_mode ? nk_rgb(32, 33, 38) : nk_rgb(250, 250, 249);
  struct nk_color muted_gray =
      ui->dark_mode ? nk_rgb(110, 112, 118) : nk_rgb(155, 155, 158);

  // Text colors adapted to light header/footer
  struct nk_color header_text_col =
      ui->dark_mode ? nk_rgb(210, 212, 218) : nk_rgb(68, 68, 68);
  struct nk_color header_icon_col =
      ui->dark_mode ? nk_rgba(180, 182, 190, 180) : nk_rgba(120, 120, 125, 200);
  struct nk_color footer_text_col =
      ui->dark_mode ? nk_rgb(140, 142, 148) : nk_rgb(140, 140, 145);

  // Expand/collapse toggle
  ui_render_expand_toggle(ui, ctx, block);

  if (block->is_collapsed)
    return;

  // =========================================================================
  // KEYBOARD NAVIGATION (Phase 8 + Phase 11)
  // =========================================================================
  int view_count_nav = (int)aslan_table_view_count(tbl);

  if (ui->table_state.active_block_id == block->id &&
      !ui->table_state.is_editing_cell) {
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_UP)) {
      if (ui->table_state.active_row_idx > 0) {
        ui->table_state.active_row_idx--;
        ui_request_redraw(ui);
      }
    }
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_DOWN)) {
      if (ui->table_state.active_row_idx < view_count_nav - 1) {
        ui->table_state.active_row_idx++;
        ui_request_redraw(ui);
      }
    }
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_LEFT)) {
      if (ui->table_state.active_col_idx > 0) {
        ui->table_state.active_col_idx--;
      } else if (ui->table_state.active_row_idx > 0) {
        ui->table_state.active_row_idx--;
        ui->table_state.active_col_idx = (int)tbl->column_count - 1;
      }
      ui_request_redraw(ui);
    }
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_RIGHT)) {
      if (ui->table_state.active_col_idx < (int)tbl->column_count - 1) {
        ui->table_state.active_col_idx++;
      } else if (ui->table_state.active_row_idx < view_count_nav - 1) {
        ui->table_state.active_row_idx++;
        ui->table_state.active_col_idx = 0;
      }
      ui_request_redraw(ui);
    }
    // Tab: advance to next cell (wrap to next row)
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_TAB)) {
      if (ui->table_state.active_col_idx < (int)tbl->column_count - 1) {
        ui->table_state.active_col_idx++;
      } else if (ui->table_state.active_row_idx < view_count_nav - 1) {
        ui->table_state.active_row_idx++;
        ui->table_state.active_col_idx = 0;
      } else {
        ui->table_state.active_col_idx = 0;
        ui->table_state.active_row_idx = 0;
      }
      ui_request_redraw(ui);
    }
    // Enter: start editing
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
      ui->table_state.is_editing_cell = 1;
      ui_request_redraw(ui);
    }
  }
  // Escape: cancel edit mode (works globally while editing)
  if (ui->table_state.is_editing_cell &&
      nk_input_is_key_pressed(&ctx->input, NK_KEY_TEXT_RESET_MODE)) {
    ui->table_state.is_editing_cell = 0;
  }

  // =========================================================================
  // COLUMNAR TABLE RENDERING
  // =========================================================================
  if (tbl && tbl->column_count > 0) {
    size_t col_count = tbl->column_count;
    size_t view_count = aslan_table_view_count(tbl);

    // Set Inter font for the entire table block - Manual save/restore to avoid
    // stack issues
    if (ui->font_inter) {
      nk_style_set_font(ctx, &ui->font_inter->handle);
    }

    // Calculate column widths (no auto-scaling to allow horizontal scroll)
    float *col_widths = (float *)alloca(col_count * sizeof(float));
    float total_w = row_num_w + 32.0f; // Row num + Add Column button
    for (size_t c = 0; c < col_count; c++) {
      float w = (float)tbl->columns[c].width_px;
      if (w < min_col_w)
        w = min_col_w;
      if (w > max_col_w)
        w = max_col_w;
      col_widths[c] = w;
      total_w += w;
    }
    const float add_col_w = 32.0f;

    // Save and Push Global Table Styles (Zero Padding/Spacing for "Simple
    // Grid")
    ctx->style.edit.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.edit.hover = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.edit.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.edit.border = 0.0f;
    ctx->style.edit.padding = nk_vec2(6, 4); // Padding for interactivity

    ctx->style.window.padding = nk_vec2(0, 0);
    ctx->style.window.group_padding = nk_vec2(0, 0);
    ctx->style.window.spacing = nk_vec2(0, 0);
    ctx->style.button.padding = nk_vec2(8, 4); // Good padding for buttons

    // Visible rows logic (moved up for height calculation)
    int max_rows = td->visible_rows > 0 ? td->visible_rows : (int)view_count;
    if (max_rows > (int)view_count)
      max_rows = (int)view_count;

    // Start a scrollable group for the table to handle horizontal overflow
    // The height needs to account for search, header, rows, and potential
    // scrollbar
    float table_height = header_h + row_h * (float)max_rows + footer_h + 10;
    if (td->show_search)
      table_height += 30;

    // Use a slightly larger height to avoid vertical scroll if possible,
    // or let it vertical scroll too if it exceeds a limit.

    // FORMULA BAR (renders above the table grid inside the card)
    ui_render_formula_bar(ui, ctx, block);

    // FORMULA HELP POPUP
    ui_render_formula_help_popup(ui, ctx);

    nk_layout_row_dynamic(ctx, table_height + 20, 1);

    // General click detection for the table block
    struct nk_rect table_bounds = nk_widget_bounds(ctx);
    if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                        table_bounds)) {
      ui->active_block_id = block->id;
      ui->table_state.active_block_id = block->id;
    }

    // Background rendering removed for clean Coda-style appearance

    nk_style_push_vec2(ctx, &ctx->style.text.padding, nk_vec2(4, 2));

    if (nk_group_begin(ctx, "table_scroll_group", NK_WINDOW_SCROLL_AUTO_HIDE)) {

      (void)total_w; // Used for group width calculation by linter

      // Column resize drag handling
      if (ui->table_state.resizing_col >= 0 &&
          ui->table_state.active_block_id == block->id) {
        if (nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
          float dx = ctx->input.mouse.pos.x - ui->table_state.resize_start_x;
          float new_w = ui->table_state.resize_start_w + dx;
          if (new_w < min_col_w)
            new_w = min_col_w;
          if (new_w > max_col_w)
            new_w = max_col_w;
          int rc = ui->table_state.resizing_col;
          if (rc >= 0 && rc < (int)tbl->column_count) {
            tbl->columns[rc].width_px = new_w;
            col_widths[rc] = new_w;
          }
        } else {
          // Mouse released — emit col_width command for persistence
          int rc = ui->table_state.resizing_col;
          if (rc >= 0 && rc < (int)tbl->column_count) {
            Page *pg = doc_page_get_active(engine->doc);
            if (pg) {
              char cmd[128];
              snprintf(cmd, sizeof(cmd), "block_table_col_width %u %u %d %.0f",
                       pg->id, block->id, rc, tbl->columns[rc].width_px);
              engine_execute_command(engine, cmd);
            }
          }
          ui->table_state.resizing_col = -1;
        }
      }

      // SEARCH / FILTER BAR
      if (td->show_search) {
        nk_layout_row_dynamic(ctx, 30, 1);
        struct nk_rect fb = nk_widget_bounds(ctx);
        struct nk_command_buffer *fcanvas = nk_window_get_canvas(ctx);
        struct nk_color filter_bg =
            ui->dark_mode ? nk_rgb(35, 36, 40) : nk_rgb(248, 248, 248);
        nk_fill_rect(fcanvas, fb, 0, filter_bg);
        // Bottom border
        nk_stroke_line(fcanvas, fb.x, fb.y + fb.h - 1, fb.x + fb.w,
                       fb.y + fb.h - 1, 1.0f, border_color);

        // Draw search icon
        if (ui->font_icons) {
          nk_draw_text(fcanvas, nk_rect(fb.x + 8, fb.y + 6, 18, 18),
                       ICON_SEARCH, strlen(ICON_SEARCH),
                       &ui->font_icons->handle, nk_rgba(0, 0, 0, 0),
                       muted_gray);
        }

        // Edit field with left offset for the icon
        struct nk_style_edit old_es = ctx->style.edit;
        ctx->style.edit.normal = nk_style_item_color(filter_bg);
        ctx->style.edit.hover = nk_style_item_color(filter_bg);
        ctx->style.edit.active = nk_style_item_color(filter_bg);
        ctx->style.edit.border = 0;
        ctx->style.edit.padding = nk_vec2(28, 6);
        ctx->style.edit.text_normal =
            ui->dark_mode ? nk_rgb(200, 200, 205) : nk_rgb(80, 80, 85);
        ctx->style.edit.text_hover = ctx->style.edit.text_normal;
        ctx->style.edit.text_active = ctx->style.edit.text_normal;

        nk_edit_string_zero_terminated(
            ctx, NK_EDIT_FIELD, ui->table_state.filter_buffer,
            sizeof(ui->table_state.filter_buffer) - 1, nk_filter_default);

        ctx->style.edit = old_es;

        // Live filtering: sync filter text and rebuild on change
        if (strcmp(tbl->filter_text, ui->table_state.filter_buffer) != 0) {
          strncpy(tbl->filter_text, ui->table_state.filter_buffer, 127);
          tbl->filter_text[127] = '\0';
          aslan_table_filter_rebuild(tbl);
          view_count = aslan_table_view_count(tbl);
        }

        // Placeholder text when empty
        if (ui->table_state.filter_buffer[0] == '\0') {
          nk_draw_text(fcanvas,
                       nk_rect(fb.x + 30, fb.y + 6, fb.w - 40, fb.h - 12),
                       "Search...", 9, &ui->font_inter->handle,
                       nk_rgba(0, 0, 0, 0), muted_gray);
        }
      }

      // HEADER ROW (+ Add Column button at end)
      nk_layout_row_begin(ctx, NK_STATIC, header_h, (int)col_count + 2);

      // Row number header
      nk_layout_row_push(ctx, row_num_w);
      struct nk_rect hdr_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, header_bg);
      // Bottom border
      nk_stroke_line(nk_window_get_canvas(ctx), hdr_bounds.x,
                     hdr_bounds.y + hdr_bounds.h - 1,
                     hdr_bounds.x + hdr_bounds.w,
                     hdr_bounds.y + hdr_bounds.h - 1, 1.0f, border_color);
      nk_label_colored(ctx, "#", NK_TEXT_CENTERED, header_icon_col);

      // --- Keyboard handling for inline rename ---
      if (ui->table_state.renaming_col_idx >= 0 &&
          ui->table_state.active_block_id == block->id) {
        int ren_col = ui->table_state.renaming_col_idx;
        struct nk_input *in = &ctx->input;
        // Enter → commit rename
        if (nk_input_is_key_pressed(in, NK_KEY_ENTER)) {
          if (ren_col >= 0 && ren_col < (int)tbl->column_count) {
            aslan_table_rename_column(tbl, ren_col,
                                      ui->table_state.rename_buffer);
          }
          ui->table_state.renaming_col_idx = -1;
          ui->table_state.selected_col_idx = -1;
          ui->table_state.rename_buffer[0] = '\0';
          ui_request_redraw(ui);
        }
        // Escape → cancel rename
        else if (nk_input_is_key_pressed(in, NK_KEY_TEXT_UNDO)) {
          ui->table_state.renaming_col_idx = -1;
          ui->table_state.selected_col_idx = -1;
          ui->table_state.rename_buffer[0] = '\0';
          ui_request_redraw(ui);
        }
        // Backspace → delete last char
        else if (nk_input_is_key_pressed(in, NK_KEY_BACKSPACE)) {
          size_t len = strlen(ui->table_state.rename_buffer);
          if (len > 0)
            ui->table_state.rename_buffer[len - 1] = '\0';
          ui_request_redraw(ui);
        }
        // Typed characters → append to buffer
        else if (in->keyboard.text_len > 0) {
          size_t cur_len = strlen(ui->table_state.rename_buffer);
          for (int k = 0; k < in->keyboard.text_len && cur_len < 62; k++) {
            ui->table_state.rename_buffer[cur_len++] = in->keyboard.text[k];
          }
          ui->table_state.rename_buffer[cur_len] = '\0';
          ui_request_redraw(ui);
        }
      }

      // Column headers
      for (size_t c = 0; c < col_count; c++) {
        nk_layout_row_push(ctx, col_widths[c]);
        hdr_bounds = nk_widget_bounds(ctx);
        {
          struct nk_color hbg = header_bg;
          uint32_t cbg = tbl->columns[c].bg_color;
          if (cbg != 0) {
            float a = (float)(cbg & 0xFF) / 255.0f;
            hbg.r = (nk_byte)((float)((cbg >> 24) & 0xFF) * a +
                              (float)hbg.r * (1.0f - a));
            hbg.g = (nk_byte)((float)((cbg >> 16) & 0xFF) * a +
                              (float)hbg.g * (1.0f - a));
            hbg.b = (nk_byte)((float)((cbg >> 8) & 0xFF) * a +
                              (float)hbg.b * (1.0f - a));
          }
          nk_fill_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, hbg);
        }
        // Subtle bottom border only (no full rect stroke)
        nk_stroke_line(nk_window_get_canvas(ctx), hdr_bounds.x,
                       hdr_bounds.y + hdr_bounds.h - 1,
                       hdr_bounds.x + hdr_bounds.w,
                       hdr_bounds.y + hdr_bounds.h - 1, 1.0f, border_color);

        // --- Interaction Detection ---
        int is_hovering_cell =
            nk_input_is_mouse_hovering_rect(&ctx->input, hdr_bounds);

        // Resize zone: 10px wide centered on right border
        // We ensure it doesn't overlap excessively with the next column's icon
        // by checking order
        struct nk_rect resize_zone = nk_rect(hdr_bounds.x + hdr_bounds.w - 8,
                                             hdr_bounds.y, 16, hdr_bounds.h);
        int is_hovering_resize =
            nk_input_is_mouse_hovering_rect(&ctx->input, resize_zone);

        // A. Column Resize Initiation
        // Check this FIRST to consume the input
        if (is_hovering_resize &&
            nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
            ui->table_state.resizing_col < 0) {
          ui->table_state.active_block_id = block->id;
          ui->active_block_id = block->id;
          ui->table_state.resizing_col = (int)c;
          ui->table_state.resize_start_x = ctx->input.mouse.pos.x;
          ui->table_state.resize_start_w = col_widths[c];
          ui_request_redraw(ui);
        }

        // B. Column Drag-and-Drop Initiation
        if (!is_hovering_resize && ui->table_state.resizing_col < 0 &&
            ui->table_state.dragging_col < 0 &&
            nk_input_is_mouse_hovering_rect(&ctx->input, hdr_bounds) &&
            nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
          ui->table_state.dragging_col = (int)c;
          ui->table_state.drag_col_start_x = ctx->input.mouse.pos.x;
          ui->table_state.drop_target_col = -1;
        }

        // Zones for click handling
        // Icon: Left 28px
        struct nk_rect icon_zone =
            nk_rect(hdr_bounds.x, hdr_bounds.y, 28, hdr_bounds.h);

        // Text: Rest of the cell MINUS resize area
        struct nk_rect text_zone = nk_rect(
            hdr_bounds.x + 28, hdr_bounds.y,
            (hdr_bounds.w > 38) ? (hdr_bounds.w - 38) : 0, hdr_bounds.h);

        int clicked_icon = nk_input_is_mouse_click_in_rect(
            &ctx->input, NK_BUTTON_LEFT, icon_zone);
        int clicked_text = nk_input_is_mouse_click_in_rect(
            &ctx->input, NK_BUTTON_LEFT, text_zone);

        if (!is_hovering_resize && ui->table_state.resizing_col < 0) {
          // Consume the widget slot (invisible button for layout)
          struct nk_style_button hdr_btn_style = ctx->style.button;
          hdr_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
          hdr_btn_style.hover = nk_style_item_color(nk_rgba(255, 255, 255, 20));
          hdr_btn_style.active =
              nk_style_item_color(nk_rgba(255, 255, 255, 30));
          hdr_btn_style.border = 0;
          hdr_btn_style.text_normal = header_text_col;
          hdr_btn_style.text_hover = header_text_col;
          hdr_btn_style.text_active = header_text_col;
          nk_button_label_styled(ctx, &hdr_btn_style, "");

          // CLICK-TO-REFERENCE: If formula bar is active, insert column name
          if (ui->table_state.is_editing_formula &&
              (clicked_text || clicked_icon)) {
            char ref[128];
            snprintf(ref, sizeof(ref), "[%s]", tbl->columns[c].name);
            size_t buf_len = strlen(ui->table_state.formula_buffer);
            size_t ref_len = strlen(ref);
            if (buf_len + ref_len <
                sizeof(ui->table_state.formula_buffer) - 1) {
              strcat(ui->table_state.formula_buffer, ref);
            }
            // Keep formula mode active and request redraw for immediate
            // feedback
            ui->table_state.is_editing_formula = 1;
            ui_request_redraw(ui);
          } else {
            // --- New interaction model ---

            // A) Click on ICON → Sort + Select column (show X)
            if (clicked_icon) {
              // Toggle sort: ASC → DESC → OFF
              if (tbl->sort_column == (int)c) {
                if (tbl->sort_ascending)
                  aslan_table_sort(tbl, (int)c, false);
                else
                  aslan_table_sort_clear(tbl);
              } else {
                aslan_table_sort(tbl, (int)c, true);
              }
              ui->table_state.selected_col_idx = (int)c;
              ui->table_state.renaming_col_idx = -1; // not renaming
              ui->table_state.active_block_id = block->id;
              ui_request_redraw(ui);
            }

            // B) Click on NAME → Inline rename + Select column (show X)
            if (clicked_text) {
              ui->table_state.selected_col_idx = (int)c;
              ui->table_state.renaming_col_idx = (int)c;
              strncpy(ui->table_state.rename_buffer, tbl->columns[c].name,
                      sizeof(ui->table_state.rename_buffer) - 1);
              ui->table_state
                  .rename_buffer[sizeof(ui->table_state.rename_buffer) - 1] =
                  '\0';
              ui->table_state.active_block_id = block->id;
              ui_request_redraw(ui);
            }
          }
        }

        // --- Drawing ---
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        const char *icon = table_col_type_icon(tbl->columns[c].type);

        // Highlight cell if hovering
        if (is_hovering_cell && ui->table_state.resizing_col < 0) {
          struct nk_color hdr_hover =
              ui->dark_mode ? nk_rgba(255, 255, 255, 15) : nk_rgba(0, 0, 0, 8);
          nk_fill_rect(canvas, hdr_bounds, 0, hdr_hover);
        }

        // Draw Icon
        if (ui->font_icons) {
          nk_style_set_font(ctx, &ui->font_icons->handle);
          nk_draw_text(canvas,
                       nk_rect(hdr_bounds.x + 6, hdr_bounds.y + 6, 20, 20),
                       icon, (int)strlen(icon), &ui->font_icons->handle,
                       nk_rgba(0, 0, 0, 0), header_icon_col);
          nk_style_set_font(ctx, &ui->font_inter->handle);
        }

        // Draw column name OR inline edit field
        if (ui->table_state.renaming_col_idx == (int)c &&
            ui->table_state.active_block_id == block->id) {
          // --- INLINE RENAME MODE ---
          // Use nk_draw_text to show the rename buffer content in an edit-like
          // way We draw a simple text + underline to indicate editing
          nk_draw_text(canvas,
                       nk_rect(hdr_bounds.x + 32, hdr_bounds.y + 8,
                               hdr_bounds.w - 40, 20),
                       ui->table_state.rename_buffer,
                       (int)strlen(ui->table_state.rename_buffer),
                       &ui->font_inter->handle, nk_rgba(0, 0, 0, 0),
                       header_text_col);
          // Underline to show editing state
          nk_stroke_line(
              canvas, hdr_bounds.x + 32, hdr_bounds.y + 28,
              hdr_bounds.x + 32 +
                  ((hdr_bounds.w - 44) < 100 ? (hdr_bounds.w - 44) : 100),
              hdr_bounds.y + 28, 1.0f, COLOR_ACCENT);
        } else {
          // --- DISPLAY MODE ---
          nk_draw_text(canvas,
                       nk_rect(hdr_bounds.x + 32, hdr_bounds.y + 8,
                               hdr_bounds.w - 36, 20),
                       tbl->columns[c].name, (int)strlen(tbl->columns[c].name),
                       &ui->font_inter->handle, nk_rgba(0, 0, 0, 0),
                       header_text_col);
        }

        // Delete "X" icon — shown when this column is selected
        if (ui->table_state.selected_col_idx == (int)c &&
            ui->table_state.active_block_id == block->id) {
          struct nk_rect del_rect = nk_rect(hdr_bounds.x + hdr_bounds.w - 18,
                                            hdr_bounds.y + 2, 16, 16);

          // Check click on X → delete column
          if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                              del_rect)) {
            block_table_delete_col(block, (int)c);
            ui->table_state.selected_col_idx = -1;
            ui->table_state.renaming_col_idx = -1;
            ui_request_redraw(ui);
            nk_group_end(ctx);
            return; // exit to avoid crash on shifted indices
          }

          // Draw the X
          nk_draw_text(canvas, del_rect, "x", 1, &ui->font_inter->handle,
                       nk_rgba(0, 0, 0, 0), nk_rgb(180, 80, 80));
        }

        // Divider & Resize Handle highlight
        nk_stroke_line(
            canvas, hdr_bounds.x + hdr_bounds.w - 1, hdr_bounds.y + 8,
            hdr_bounds.x + hdr_bounds.w - 1, hdr_bounds.y + hdr_bounds.h - 8,
            1.0f, nk_rgba(border_color.r, border_color.g, border_color.b, 100));

        if (is_hovering_resize ||
            (ui->table_state.resizing_col == (int)c &&
             ui->table_state.active_block_id == block->id)) {
          nk_stroke_line(canvas, hdr_bounds.x + hdr_bounds.w - 1,
                         hdr_bounds.y + 4, hdr_bounds.x + hdr_bounds.w - 1,
                         hdr_bounds.y + hdr_bounds.h, 2.0f, COLOR_ACCENT);
          ui_set_cursor(ui, UI_CURSOR_SIZEWE);
        }
      }

      // Inline + Add Column header
      nk_layout_row_push(ctx, add_col_w);
      hdr_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, header_bg);
      nk_stroke_line(nk_window_get_canvas(ctx), hdr_bounds.x,
                     hdr_bounds.y + hdr_bounds.h - 1,
                     hdr_bounds.x + hdr_bounds.w,
                     hdr_bounds.y + hdr_bounds.h - 1, 1.0f, border_color);

      struct nk_style_button add_col_btn = ctx->style.button;
      add_col_btn.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      add_col_btn.hover = nk_style_item_color(nk_rgba(255, 255, 255, 20));
      add_col_btn.text_normal = header_text_col;
      add_col_btn.rounding = 0.0f;
      if (nk_button_label_styled(ctx, &add_col_btn, "+")) {
        ui->table_state.active_block_id = block->id;
        ui->active_block_id = block->id;
        ui->table_state.show_col_menu = !ui->table_state.show_col_menu;
        ui->table_state.show_footer_menu = 0;
        ui->table_state.menu_pos_x = hdr_bounds.x;
        ui->table_state.menu_pos_y = hdr_bounds.y + hdr_bounds.h;
      }

      nk_layout_row_end(ctx);

      // --- Column Drag-and-Drop tracking & visual indicator ---
      if (ui->table_state.dragging_col >= 0 &&
          ui->table_state.active_block_id == block->id) {
        float mx = ctx->input.mouse.pos.x;
        float dx = mx - ui->table_state.drag_col_start_x;
        int is_dragging_active = (dx > 5.0f || dx < -5.0f);

        if (!nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
          // Mouse released — perform drop
          if (is_dragging_active && ui->table_state.drop_target_col >= 0 &&
              ui->table_state.drop_target_col != ui->table_state.dragging_col) {
            aslan_table_swap_columns(tbl, ui->table_state.dragging_col,
                                     ui->table_state.drop_target_col);
            ui->table_state.active_col_idx = ui->table_state.drop_target_col;
          }
          ui->table_state.dragging_col = -1;
          ui->table_state.drop_target_col = -1;
          ui_request_redraw(ui);
        } else if (is_dragging_active) {
          // Track which column mouse is over
          ui->table_state.drop_target_col = -1;
          // Recalculate column positions for drop target
          float acc_x = 0;
          for (int dc = 0; dc < (int)tbl->column_count; dc++) {
            float cw = col_widths[dc];
            float cx = acc_x;
            if (mx >= cx && mx < cx + cw) {
              ui->table_state.drop_target_col = dc;
              break;
            }
            acc_x += cw;
          }

          // Draw drop indicator line
          if (ui->table_state.drop_target_col >= 0 &&
              ui->table_state.drop_target_col != ui->table_state.dragging_col) {
            float ind_x = 0;
            for (int dc = 0; dc < ui->table_state.drop_target_col; dc++) {
              ind_x += col_widths[dc];
            }
            struct nk_command_buffer *dd_canvas = nk_window_get_canvas(ctx);
            nk_stroke_line(dd_canvas, ind_x, hdr_bounds.y, ind_x,
                           hdr_bounds.y + hdr_bounds.h, 3.0f, COLOR_ACCENT);
          }
          ui_request_redraw(ui);
        }
      }

      // -----------------------------------------------------------------------
      // DATA ROWS
      // -----------------------------------------------------------------------
      // Airtable-style row colors
      struct nk_color notion_row_bg =
          ui->dark_mode ? nk_rgb(28, 29, 33) : nk_rgb(255, 255, 255);
      struct nk_color notion_border =
          ui->dark_mode ? nk_rgba(55, 57, 62, 50) : nk_rgba(232, 232, 230, 200);
      struct nk_color vert_sep =
          ui->dark_mode ? nk_rgba(55, 57, 62, 30) : nk_rgba(230, 230, 228, 140);
      struct nk_color row_hover_bg =
          ui->dark_mode ? nk_rgba(255, 255, 255, 12)
                        : nk_rgb(246, 248, 252); // Warm blueish hover
      struct nk_color active_cell_outline =
          nk_rgb(45, 127, 249); // Airtable blue

      for (size_t v = 0; v < (size_t)max_rows; v++) {
        // Deselect column header when user interacts with data cells
        if (v == 0 && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
            ui->table_state.selected_col_idx >= 0 &&
            ui->table_state.active_block_id == block->id) {
          // Commit any pending rename
          int ren = ui->table_state.renaming_col_idx;
          if (ren >= 0 && ren < (int)tbl->column_count &&
              ui->table_state.rename_buffer[0] != '\0') {
            aslan_table_rename_column(tbl, ren, ui->table_state.rename_buffer);
          }
          ui->table_state.selected_col_idx = -1;
          ui->table_state.renaming_col_idx = -1;
          ui->table_state.rename_buffer[0] = '\0';
        }
        uint64_t row_id = aslan_table_view_row_id(tbl, (int)v);
        if (row_id == 0)
          continue;

        // Data Row Layout using nk_layout_row_begin/push/end (consistent API)
        nk_layout_row_begin(ctx, NK_STATIC, row_h, (int)col_count + 2);

        // Calculate row hover state
        struct nk_rect row_bounds = nk_layout_widget_bounds(ctx);
        int is_row_hovered =
            nk_input_is_mouse_hovering_rect(&ctx->input, row_bounds);
        // Very subtle zebra stripe
        struct nk_color odd_row_bg =
            ui->dark_mode ? nk_rgb(31, 32, 37) : nk_rgb(251, 251, 250);
        struct nk_color base_row_bg = (v % 2 == 0) ? notion_row_bg : odd_row_bg;
        struct nk_color current_row_bg =
            is_row_hovered ? row_hover_bg : base_row_bg;
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

        // Row number cell — show delete "x" on hover + accent stripe
        nk_layout_row_push(ctx, row_num_w);
        struct nk_rect cell_bounds = nk_widget_bounds(ctx);
        nk_fill_rect(canvas, cell_bounds, 0, current_row_bg);
        nk_stroke_line(canvas, cell_bounds.x, cell_bounds.y + cell_bounds.h - 1,
                       cell_bounds.x + cell_bounds.w,
                       cell_bounds.y + cell_bounds.h - 1, 0.5f, notion_border);

        // Left accent stripe on hover
        if (is_row_hovered) {
          nk_fill_rect(canvas,
                       nk_rect(cell_bounds.x, cell_bounds.y + 4, 2.5f,
                               cell_bounds.h - 8),
                       1.5f, active_cell_outline);
        }

        // Right-click on row number = duplicate row
        if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT,
                                            cell_bounds)) {
          aslan_table_duplicate_row(tbl, row_id);
        }

        if (is_row_hovered) {
          struct nk_style_button del_style = ctx->style.button;
          del_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
          del_style.hover = nk_style_item_color(nk_rgba(220, 60, 60, 40));
          del_style.active = nk_style_item_color(nk_rgba(220, 60, 60, 80));
          del_style.text_normal = nk_rgb(180, 60, 60);
          del_style.text_hover = nk_rgb(240, 60, 60);
          del_style.text_active = nk_rgb(255, 80, 80);
          del_style.border = 0;
          del_style.rounding = 0;
          if (nk_button_label_styled(ctx, &del_style, "x")) {
            aslan_table_delete_row(tbl, row_id);
          }
        } else {
          char row_num_str[16];
          snprintf(row_num_str, sizeof(row_num_str), "%zu", v + 1);
          nk_label_colored(ctx, row_num_str, NK_TEXT_CENTERED, muted_gray);
        }

        // Data cells - direct widget placement (no nk_group)
        for (size_t c = 0; c < col_count; c++) {
          nk_layout_row_push(ctx, col_widths[c]);
          cell_bounds = nk_widget_bounds(ctx);

          // --- Selection Logic ---
          if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                              cell_bounds)) {
            ui->table_state.active_block_id = block->id;
            ui->active_block_id = block->id;
            ui->table_state.active_row_idx = (int)v;
            ui->table_state.active_col_idx = (int)c;
            ui->table_state.is_editing_cell =
                0; // Select but don't edit by default
            ui_request_redraw(ui);
          }

          // Background (with optional column tint)
          {
            struct nk_color cell_bg = current_row_bg;
            uint32_t cbg = tbl->columns[c].bg_color;
            if (cbg != 0) {
              uint8_t tr = (cbg >> 24) & 0xFF;
              uint8_t tg = (cbg >> 16) & 0xFF;
              uint8_t tb = (cbg >> 8) & 0xFF;
              uint8_t ta = cbg & 0xFF;
              // Blend tint over row bg
              float a = (float)ta / 255.0f;
              cell_bg.r =
                  (nk_byte)((float)tr * a + (float)cell_bg.r * (1.0f - a));
              cell_bg.g =
                  (nk_byte)((float)tg * a + (float)cell_bg.g * (1.0f - a));
              cell_bg.b =
                  (nk_byte)((float)tb * a + (float)cell_bg.b * (1.0f - a));
            }
            nk_fill_rect(canvas, cell_bounds, 0, cell_bg);
          }

          // Borders (bottom only - very subtle)
          nk_stroke_line(
              canvas, cell_bounds.x, cell_bounds.y + cell_bounds.h - 1,
              cell_bounds.x + cell_bounds.w, cell_bounds.y + cell_bounds.h - 1,
              0.5f, notion_border);

          // Vertical separator on hover only
          if (is_row_hovered) {
            nk_stroke_line(canvas, cell_bounds.x + cell_bounds.w - 1,
                           cell_bounds.y + 6, cell_bounds.x + cell_bounds.w - 1,
                           cell_bounds.y + cell_bounds.h - 6, 0.5f, vert_sep);
          }

          AslanColumnType type = tbl->columns[c].type;
          int is_active = (ui->table_state.active_row_idx == (int)v &&
                           ui->table_state.active_col_idx == (int)c);

          if (is_active) {
            // Blue outline for active cell (Airtable-style focus ring)
            nk_stroke_rect(canvas,
                           nk_rect(cell_bounds.x + 1, cell_bounds.y + 1,
                                   cell_bounds.w - 2, cell_bounds.h - 2),
                           2.5f, 1.5f, active_cell_outline);
          }

          switch (type) {
          case ASLAN_COL_FORMULA: {
            // Evaluate formula freshly for this row
            aslan_table_eval_formula(tbl, (int)c, row_id);

            // Read-only computed value display
            char val_str[64];
            aslan_table_get_cell_display(tbl, (int)c, row_id, val_str,
                                         sizeof(val_str));

            // Allow selecting the cell to view/edit formula in the bar
            if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                                cell_bounds)) {
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.active_block_id = block->id;
              ui->active_block_id = block->id;
              ui->table_state.is_editing_cell = 0;    // Not inline editing
              ui->table_state.is_editing_formula = 1; // Activate formula mode

              // Populate formula buffer for the bar
              const char *current = tbl->columns[c].formula;
              snprintf(ui->table_state.formula_buffer,
                       sizeof(ui->table_state.formula_buffer), "=%s",
                       current ? current : "");
              ui_request_redraw(ui);
            }

            // Show formula result or placeholder via a read-only edit widget.
            // IMPORTANT: Must use nk_edit_string_zero_terminated (not
            // nk_label) to keep Nuklear's edit sequence counter stable.
            // Using nk_label here would shift win->edit.seq for all
            // subsequent edit widgets, corrupting their cursor state.
            {
              char display_buf[64];
              struct nk_color txt_col;
              if (tbl->columns[c].formula && tbl->columns[c].formula[0]) {
                strncpy(display_buf, val_str, sizeof(display_buf) - 1);
                display_buf[sizeof(display_buf) - 1] = '\0';
                txt_col =
                    ui->dark_mode ? nk_rgb(180, 200, 240) : nk_rgb(40, 70, 140);
              } else {
                strncpy(display_buf, "= formula", sizeof(display_buf) - 1);
                display_buf[sizeof(display_buf) - 1] = '\0';
                txt_col = ui->dark_mode ? nk_rgba(100, 100, 120, 150)
                                        : nk_rgba(150, 150, 170, 150);
              }

              // Style edit to look like a transparent label
              struct nk_style_item transp =
                  nk_style_item_color(nk_rgba(0, 0, 0, 0));
              struct nk_style_item sv_n = ctx->style.edit.normal;
              struct nk_style_item sv_h = ctx->style.edit.hover;
              struct nk_style_item sv_a = ctx->style.edit.active;
              struct nk_color sv_tn = ctx->style.edit.text_normal;
              struct nk_color sv_th = ctx->style.edit.text_hover;
              struct nk_color sv_ta = ctx->style.edit.text_active;
              float sv_brd = ctx->style.edit.border;

              ctx->style.edit.normal = transp;
              ctx->style.edit.hover = transp;
              ctx->style.edit.active = transp;
              ctx->style.edit.text_normal = txt_col;
              ctx->style.edit.text_hover = txt_col;
              ctx->style.edit.text_active = txt_col;
              ctx->style.edit.border = 0;

              nk_edit_string_zero_terminated(
                  ctx, NK_EDIT_FIELD | NK_EDIT_READ_ONLY, display_buf,
                  sizeof(display_buf) - 1, nk_filter_default);

              ctx->style.edit.normal = sv_n;
              ctx->style.edit.hover = sv_h;
              ctx->style.edit.active = sv_a;
              ctx->style.edit.text_normal = sv_tn;
              ctx->style.edit.text_hover = sv_th;
              ctx->style.edit.text_active = sv_ta;
              ctx->style.edit.border = sv_brd;
            }
            break;
          }
          case ASLAN_COL_TEXT: {
            char buf[1024];
            const char *val = aslan_table_get_text(tbl, (int)c, row_id);
            strncpy(buf, val ? val : "", sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';

            nk_flags res = nk_edit_string_zero_terminated(
                ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER,
                buf, sizeof(buf) - 1, nk_filter_default);

            if (res & NK_EDIT_ACTIVATED) {
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.is_editing_cell = 1;
            }
            if (res & NK_EDIT_COMMITED)
              ui->table_state.is_editing_cell = 0;

            if (strcmp(buf, val ? val : "") != 0) {
              aslan_table_set_cell(tbl, (int)c, row_id, buf);
            }
            break;
          }
          case ASLAN_COL_NUMBER: {
            char buf[64], val_str[64];
            aslan_table_get_cell_display(tbl, (int)c, row_id, val_str,
                                         sizeof(val_str));
            strncpy(buf, val_str, sizeof(buf) - 1);

            // Right-align number edit field
            struct nk_vec2 old_edit_pad = ctx->style.edit.padding;
            ctx->style.edit.padding = nk_vec2(6, 4);

            nk_flags res = nk_edit_string_zero_terminated(
                ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER,
                buf, sizeof(buf) - 1, nk_filter_decimal);

            ctx->style.edit.padding = old_edit_pad;

            if (res & NK_EDIT_ACTIVATED) {
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.is_editing_cell = 1;
            }
            if (res & NK_EDIT_COMMITED)
              ui->table_state.is_editing_cell = 0;

            if (strcmp(buf, val_str) != 0) {
              aslan_table_set_cell(tbl, (int)c, row_id, buf);
            }
            break;
          }
          case ASLAN_COL_DATE: {
            int64_t ts = aslan_table_get_date(tbl, (int)c, row_id);

            // Date cells always use a read-only edit widget for display
            // (keeps Nuklear edit seq stable) + open date picker on click.
            char display[32];
            if (ts > 0) {
              time_t t = (time_t)(ts / 1000);
              struct tm *tm_ptr = localtime(&t);
              strftime(display, sizeof(display), "%Y-%m-%d", tm_ptr);
            } else {
              strncpy(display, "YYYY-MM-DD", sizeof(display));
            }

            // Style edit to look like a clickable label
            struct nk_style_item transp_d =
                nk_style_item_color(nk_rgba(0, 0, 0, 0));
            struct nk_style_item sv_n_d = ctx->style.edit.normal;
            struct nk_style_item sv_h_d = ctx->style.edit.hover;
            struct nk_style_item sv_a_d = ctx->style.edit.active;
            struct nk_color sv_tn_d = ctx->style.edit.text_normal;
            float sv_brd_d = ctx->style.edit.border;

            struct nk_color cell_txt_d =
                ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;
            ctx->style.edit.normal = transp_d;
            ctx->style.edit.hover =
                nk_style_item_color(nk_rgba(80, 80, 80, 40));
            ctx->style.edit.active = transp_d;
            ctx->style.edit.text_normal = ts > 0 ? cell_txt_d : muted_gray;
            ctx->style.edit.border = 0;

            nk_edit_string_zero_terminated(
                ctx, NK_EDIT_FIELD | NK_EDIT_READ_ONLY, display,
                sizeof(display) - 1, nk_filter_default);

            ctx->style.edit.normal = sv_n_d;
            ctx->style.edit.hover = sv_h_d;
            ctx->style.edit.active = sv_a_d;
            ctx->style.edit.text_normal = sv_tn_d;
            ctx->style.edit.border = sv_brd_d;

            // Open date picker popup on click
            if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                                cell_bounds)) {
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.active_block_id = block->id;
              ui->active_block_id = block->id;

              // Initialize date picker to current date or today
              if (ts > 0) {
                time_t t = (time_t)(ts / 1000);
                struct tm *tm_ptr = localtime(&t);
                ui->table_state.date_picker_year = tm_ptr->tm_year + 1900;
                ui->table_state.date_picker_month = tm_ptr->tm_mon;
              } else {
                time_t now = time(NULL);
                struct tm *tm_now = localtime(&now);
                ui->table_state.date_picker_year = tm_now->tm_year + 1900;
                ui->table_state.date_picker_month = tm_now->tm_mon;
              }
              ui->table_state.date_picker_row = row_id;
              ui->table_state.date_picker_col = (int)c;
              ui->table_state.date_picker_x = cell_bounds.x;
              ui->table_state.date_picker_y = cell_bounds.y + cell_bounds.h;
              ui->table_state.show_date_picker =
                  !ui->table_state.show_date_picker;
              ui_request_redraw(ui);
            }
            break;
          }
          case ASLAN_COL_SELECT: {
            AslanColumn *col = &tbl->columns[c];
            int sel = aslan_table_get_select(tbl, (int)c, row_id);

            // Transparent button for click detection
            struct nk_style_button sel_btn_style = ctx->style.button;
            sel_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
            sel_btn_style.hover = nk_style_item_color(nk_rgba(80, 80, 80, 20));
            sel_btn_style.active = nk_style_item_color(nk_rgba(60, 60, 60, 40));
            sel_btn_style.border = 0;
            sel_btn_style.text_normal = nk_rgba(0, 0, 0, 0); // hidden text
            sel_btn_style.text_hover = nk_rgba(0, 0, 0, 0);
            sel_btn_style.text_active = nk_rgba(0, 0, 0, 0);

            struct nk_rect btn_bounds = nk_widget_bounds(ctx);
            if (nk_button_label_styled(ctx, &sel_btn_style, "")) {
              ui->table_state.active_block_id = block->id;
              ui->active_block_id = block->id;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.show_select_popup = 1;
              ui->table_state.select_popup_row = row_id;
              ui->table_state.menu_pos_x = btn_bounds.x;
              ui->table_state.menu_pos_y = btn_bounds.y + btn_bounds.h;
              ui->table_state.show_col_menu = 0;
              ui->table_state.show_footer_menu = 0;
              ui->table_state.show_col_settings = 0;
              ui->table_state.show_date_picker = 0;
              ui->table_state.option_buffer[0] = '\0'; // Clear buffer on open
              ui->table_state.active_color_picker_option =
                  -1; // Reset color picker
            }

            // Draw color tag pill on top
            struct nk_command_buffer *sel_canvas = nk_window_get_canvas(ctx);
            if (sel >= 0 && sel < col->option_count) {
              uint32_t tag_rgba = col->options[sel].color;
              uint8_t tr = (tag_rgba >> 24) & 0xFF;
              uint8_t tg = (tag_rgba >> 16) & 0xFF;
              uint8_t tb = (tag_rgba >> 8) & 0xFF;

              // Tinted pill background (semi-transparent)
              struct nk_color pill_bg = ui->dark_mode ? nk_rgba(tr, tg, tb, 40)
                                                      : nk_rgba(tr, tg, tb, 30);
              struct nk_color pill_txt =
                  ui->dark_mode
                      ? nk_rgb(tr > 80 ? tr + 60 : 200, tg > 80 ? tg + 60 : 200,
                               tb > 80 ? tb + 60 : 200)
                      : nk_rgb(tr > 128 ? tr - 60 : tr + 20,
                               tg > 128 ? tg - 60 : tg + 20,
                               tb > 128 ? tb - 60 : tb + 20);

              float text_w = ui->font_inter->handle.width(
                  ui->font_inter->handle.userdata,
                  ui->font_inter->handle.height, col->options[sel].label,
                  strlen(col->options[sel].label));
              float pill_w = text_w + 16;
              if (pill_w > btn_bounds.w - 8)
                pill_w = btn_bounds.w - 8;

              struct nk_rect pill_rect =
                  nk_rect(btn_bounds.x + 6,
                          btn_bounds.y + (btn_bounds.h - 20) / 2, pill_w, 20);
              nk_fill_rect(sel_canvas, pill_rect, 4.0f, pill_bg);
              nk_draw_text(
                  sel_canvas,
                  nk_rect(pill_rect.x + 8, pill_rect.y + 2, pill_rect.w - 16,
                          16),
                  col->options[sel].label, strlen(col->options[sel].label),
                  &ui->font_inter->handle, nk_rgba(0, 0, 0, 0), pill_txt);
            } else {
              // Placeholder
              const char *ph = col->option_count > 0 ? "Select..." : "+ option";
              nk_draw_text(sel_canvas,
                           nk_rect(btn_bounds.x + 6, btn_bounds.y + 5,
                                   btn_bounds.w - 12, btn_bounds.h - 10),
                           ph, strlen(ph), &ui->font_inter->handle,
                           nk_rgba(0, 0, 0, 0), muted_gray);
            }
            break;
          }
          case ASLAN_COL_CHECKBOX: {
            bool checked = aslan_table_get_checkbox(tbl, (int)c, row_id);

            struct nk_style_button chk_style = ctx->style.button;
            chk_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
            chk_style.hover = nk_style_item_color(
                nk_rgba(0, 0, 0, 0)); // No fill on button, we draw custom
            chk_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
            chk_style.padding = nk_vec2(0, 0);
            chk_style.border = 0;

            if (nk_button_label_styled(ctx, &chk_style, "")) {
              aslan_table_set_checkbox(tbl, (int)c, row_id, !checked);
            }

            struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
            float chk_v = 17.0f;
            struct nk_rect chk_rect = nk_rect(
                cell_bounds.x + (cell_bounds.w - chk_v) / 2,
                cell_bounds.y + (cell_bounds.h - chk_v) / 2, chk_v, chk_v);

            if (checked) {
              // Airtable style: Filled blue (solid color only, no icon)
              nk_fill_rect(canvas, chk_rect, 3.0f, active_cell_outline);
            } else {
              // Unchecked: Bordered square
              struct nk_color b_col =
                  ui->dark_mode ? nk_rgb(80, 82, 88) : nk_rgb(205, 205, 202);
              if (nk_input_is_mouse_hovering_rect(&ctx->input, cell_bounds)) {
                b_col = active_cell_outline;
                nk_fill_rect(canvas, chk_rect, 3.0f,
                             nk_rgba(active_cell_outline.r,
                                     active_cell_outline.g,
                                     active_cell_outline.b, 25));
              }
              nk_stroke_rect(canvas, chk_rect, 3.0f, 1.2f, b_col);
            }
            break;
          }
          default: {
            char buf[256];
            aslan_table_get_cell_display(tbl, (int)c, row_id, buf, sizeof(buf));
            nk_label(ctx, buf, NK_TEXT_LEFT);
            break;
          }
          } // end switch

          // --- Focus Highlight (Drawn LAST to be on top) ---
          if (ui->table_state.active_block_id == block->id &&
              ui->table_state.active_row_idx == (int)v &&
              ui->table_state.active_col_idx == (int)c) {
            nk_stroke_rect(canvas, cell_bounds, 0, 2.0f, active_cell_outline);
          }
        } // end col loop

        // Fill a small empty space for the + column in data rows
        nk_layout_row_push(ctx, add_col_w);
        cell_bounds = nk_widget_bounds(ctx);
        nk_fill_rect(canvas, cell_bounds, 0, current_row_bg);
        if (is_row_hovered) {
          nk_stroke_line(
              canvas, cell_bounds.x, cell_bounds.y + cell_bounds.h - 1,
              cell_bounds.x + cell_bounds.w, cell_bounds.y + cell_bounds.h - 1,
              1.0f, notion_border);
        }

        nk_layout_row_end(ctx);
      }

      // -----------------------------------------------------------------------
      // FOOTER ROW (Aggregations)
      // -----------------------------------------------------------------------
      nk_layout_row_begin(ctx, NK_STATIC, footer_h, (int)col_count + 2);

      // Footer row number cell - Use as + Add Row button
      nk_layout_row_push(ctx, row_num_w);
      struct nk_rect ftr_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(nk_window_get_canvas(ctx), ftr_bounds, 0, footer_bg);

      struct nk_style_button ftr_add_style = ctx->style.button;
      ftr_add_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      ftr_add_style.hover = nk_style_item_color(nk_rgba(45, 127, 249, 30));
      ftr_add_style.text_normal = muted_gray;
      ftr_add_style.text_hover = nk_rgb(45, 127, 249);
      ftr_add_style.rounding = 0.0f;

      if (nk_button_label_styled(ctx, &ftr_add_style, "+")) {
        aslan_table_add_row(tbl);
      }

      // Footer cells with aggregations
      for (size_t c = 0; c < col_count; c++) {
        nk_layout_row_push(ctx, col_widths[c]);
        ftr_bounds = nk_widget_bounds(ctx);
        nk_fill_rect(nk_window_get_canvas(ctx), ftr_bounds, 0, footer_bg);
        // Top border only
        nk_stroke_line(nk_window_get_canvas(ctx), ftr_bounds.x, ftr_bounds.y,
                       ftr_bounds.x + ftr_bounds.w, ftr_bounds.y, 0.5f,
                       border_color);

        char footer_text[64];
        aslan_table_get_footer_display(tbl, (int)c, footer_text,
                                       sizeof(footer_text));

        // Style footer buttons as transparent with light text
        struct nk_style_button ftr_btn_style = ctx->style.button;
        ftr_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
        ftr_btn_style.hover = nk_style_item_color(
            ui->dark_mode ? nk_rgba(255, 255, 255, 12) : nk_rgba(0, 0, 0, 6));
        ftr_btn_style.active = nk_style_item_color(
            ui->dark_mode ? nk_rgba(255, 255, 255, 18) : nk_rgba(0, 0, 0, 10));
        ftr_btn_style.border = 0;
        ftr_btn_style.text_normal = footer_text_col;
        ftr_btn_style.text_hover =
            ui->dark_mode ? nk_rgb(200, 202, 208) : nk_rgb(68, 68, 68);
        ftr_btn_style.text_active = ftr_btn_style.text_hover;
        ftr_btn_style.rounding = 4.0f;

        if (footer_text[0]) {
          if (nk_button_label_styled(ctx, &ftr_btn_style, footer_text)) {
            ui->table_state.active_block_id = block->id;
            ui->active_block_id = block->id;
            ui->table_state.active_col_idx = (int)c;
            ui->table_state.show_footer_menu =
                !ui->table_state.show_footer_menu;
            ui->table_state.show_col_menu = 0;
            ui->table_state.menu_pos_x = ftr_bounds.x;
            ui->table_state.menu_pos_y = ftr_bounds.y;
          }
        } else {
          ftr_btn_style.text_normal = nk_rgba(
              footer_text_col.r, footer_text_col.g, footer_text_col.b, 100);
          if (nk_button_label_styled(ctx, &ftr_btn_style, "Calculate")) {
            ui->table_state.active_block_id = block->id;
            ui->active_block_id = block->id;
            ui->table_state.active_col_idx = (int)c;
            ui->table_state.show_footer_menu =
                !ui->table_state.show_footer_menu;
            ui->table_state.show_col_menu = 0;
            ui->table_state.menu_pos_x = ftr_bounds.x;
            ui->table_state.menu_pos_y = ftr_bounds.y;
          }
        }
      }

      // Small empty space for the + column in footer
      nk_layout_row_push(ctx, add_col_w);
      ftr_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(nk_window_get_canvas(ctx), ftr_bounds, 0, footer_bg);
      nk_stroke_line(nk_window_get_canvas(ctx), ftr_bounds.x, ftr_bounds.y,
                     ftr_bounds.x + ftr_bounds.w, ftr_bounds.y, 0.5f,
                     border_color);

      nk_layout_row_end(ctx);
      nk_group_end(ctx);
    }

    // =========================================================================
    // GLOBAL RESTORATION (Outside of group and conditional blocks)
    // =========================================================================

    // Restore Global Table Styles (MUST match push/save at lines 311-344)
    nk_style_pop_vec2(ctx);

    ctx->style.edit.normal = old_edit_normal;
    ctx->style.edit.hover = old_edit_hover;
    ctx->style.edit.active = old_edit_active;
    ctx->style.edit.border = 0.0f;
    ctx->style.edit.padding = nk_vec2(6, 4);

    ctx->style.window.padding = old_window_padding;
    ctx->style.window.group_padding = old_group_padding;
    ctx->style.window.spacing = old_window_spacing;
    ctx->style.button.padding = old_button_padding;

    // Restore font
    // Restore font
    nk_style_set_font(ctx, old_table_font);
  } else {
    // Empty/uninitialized table — CTA
    nk_layout_row_dynamic(ctx, 60, 1);
    struct nk_rect empty_bounds = nk_widget_bounds(ctx);
    struct nk_command_buffer *ec = nk_window_get_canvas(ctx);
    struct nk_color empty_bg =
        ui->dark_mode ? nk_rgb(30, 31, 35) : nk_rgb(250, 250, 248);
    nk_fill_rect(ec, empty_bounds, 6.0f, empty_bg);
    nk_stroke_rect(ec, empty_bounds, 6.0f, 1.0f, border_color);

    // Draw table icon + message
    if (ui->font_icons) {
      nk_draw_text(ec,
                   nk_rect(empty_bounds.x + empty_bounds.w / 2 - 60,
                           empty_bounds.y + 10, 20, 20),
                   ICON_TABLE_LINE, strlen(ICON_TABLE_LINE),
                   &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), muted_gray);
    }
    if (ui->font_inter) {
      nk_draw_text(ec,
                   nk_rect(empty_bounds.x + empty_bounds.w / 2 - 36,
                           empty_bounds.y + 10, 200, 20),
                   "Add your first row", 18, &ui->font_inter->handle,
                   nk_rgba(0, 0, 0, 0), muted_gray);
    }

    // Add row button
    nk_layout_row_dynamic(ctx, 28, 1);
    struct nk_style_button cta_style = ctx->style.button;
    cta_style.normal = nk_style_item_color(nk_rgba(45, 127, 249, 25));
    cta_style.hover = nk_style_item_color(nk_rgba(45, 127, 249, 50));
    cta_style.active = nk_style_item_color(nk_rgba(45, 127, 249, 80));
    cta_style.text_normal = nk_rgb(45, 127, 249);
    cta_style.text_hover = nk_rgb(70, 150, 255);
    cta_style.rounding = 4;
    cta_style.border = 0;
    if (nk_button_label_styled(ctx, &cta_style, "+ New Row")) {
      if (tbl) {
        aslan_table_add_row(tbl);
      }
    }
  }

  // =========================================================================
  // RENDER PENDING POPUPS (To avoid clipping inside layouts)
  // =========================================================================
  if (ui->table_state.active_block_id == block->id) {
    if (ui->table_state.show_col_menu) {
      struct nk_color menu_text =
          ui->dark_mode ? COLOR_TEXT : nk_rgb(30, 30, 35);
      struct nk_color menu_bg =
          ui->dark_mode ? nk_rgb(45, 47, 52) : nk_rgb(255, 255, 255);

      // Push menu style
      struct nk_style_button menu_btn = ctx->style.button;
      menu_btn.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      menu_btn.hover = nk_style_item_color(
          ui->dark_mode ? nk_rgba(255, 255, 255, 15) : nk_rgba(0, 0, 0, 10));
      menu_btn.active = nk_style_item_color(
          ui->dark_mode ? nk_rgba(255, 255, 255, 25) : nk_rgba(0, 0, 0, 20));
      menu_btn.border = 0.0f;
      menu_btn.rounding = 4.0f;

      nk_style_push_style_item(ctx, &ctx->style.window.fixed_background,
                               nk_style_item_color(menu_bg));
      struct nk_rect popup_rect =
          table_smart_popup_rect(ui, ui->table_state.menu_pos_x - 100,
                                 ui->table_state.menu_pos_y + 4, 180, 290);
      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Add Column",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         popup_rect)) {
        if (table_popup_header(ui, ctx, "Add Column")) {
          ui->table_state.show_col_menu = 0;
          nk_popup_close(ctx);
        }
        nk_layout_row_dynamic(ctx, 28, 1);
        const char *types[] = {"Text",   "Number", "Checkbox",
                               "Select", "Date",   "Formula"};
        AslanColumnType type_vals[] = {ASLAN_COL_TEXT,     ASLAN_COL_NUMBER,
                                       ASLAN_COL_CHECKBOX, ASLAN_COL_SELECT,
                                       ASLAN_COL_DATE,     ASLAN_COL_FORMULA};
        for (int i = 0; i < 6; i++) {
          struct nk_rect item_rect = nk_widget_bounds(ctx);
          if (nk_button_label_styled(ctx, &menu_btn, "")) {
            if (tbl) {
              char col_name[64];
              int sequence = (int)tbl->column_count + 1;

              // Find a unique name (e.g., "Column 3", "Column 4"...)
              while (1) {
                snprintf(col_name, sizeof(col_name), "Column %d", sequence);
                if (aslan_table_find_column(tbl, col_name) < 0) {
                  break; // Found unique name
                }
                sequence++;
              }

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
                         nk_rect(item_rect.x + 8, item_rect.y + 4, 20, 20),
                         icon, strlen(icon), &ui->font_icons->handle,
                         nk_rgba(0, 0, 0, 0), menu_text);
          }
          nk_draw_text(
              canvas,
              nk_rect(item_rect.x + 32, item_rect.y + 4, item_rect.w - 36, 20),
              types[i], strlen(types[i]), &ui->font_inter->handle,
              nk_rgba(0, 0, 0, 0), menu_text);
        }
        nk_popup_end(ctx);
      } else {
        ui->table_state.show_col_menu = 0;
      }
      nk_style_pop_style_item(ctx);
    }

    if (ui->table_state.show_footer_menu &&
        ui->table_state.active_col_idx >= 0 &&
        ui->table_state.active_col_idx < (int)tbl->column_count) {

      // Smart Placement
      float menu_h = 290.0f;
      struct nk_rect popup_rect =
          table_smart_popup_rect(ui, ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y + 24, 190, menu_h);
      // Flip up if at bottom
      if (ui->table_state.menu_pos_y > ui->win_height - 300) {
        popup_rect.y = ui->table_state.menu_pos_y - menu_h;
      }

      struct nk_color menu_bg =
          ui->dark_mode ? nk_rgb(45, 47, 52) : nk_rgb(255, 255, 255);
      nk_style_push_style_item(ctx, &ctx->style.window.fixed_background,
                               nk_style_item_color(menu_bg));

      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Aggregation",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         popup_rect)) {
        if (table_popup_header(ui, ctx, "Aggregation")) {
          ui->table_state.show_footer_menu = 0;
          nk_popup_close(ctx);
        }

        struct nk_color menu_text =
            ui->dark_mode ? COLOR_TEXT : nk_rgb(30, 30, 35);
        struct nk_style_button menu_btn = ctx->style.button;
        menu_btn.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
        menu_btn.hover = nk_style_item_color(
            ui->dark_mode ? nk_rgba(255, 255, 255, 15) : nk_rgba(0, 0, 0, 10));
        menu_btn.active = nk_style_item_color(
            ui->dark_mode ? nk_rgba(255, 255, 255, 25) : nk_rgba(0, 0, 0, 20));
        menu_btn.border = 0.0f;
        menu_btn.rounding = 4.0f;
        menu_btn.text_normal = menu_text;
        menu_btn.text_hover = menu_text;
        menu_btn.text_active = menu_text;

        nk_layout_row_dynamic(ctx, 28, 1);
        int c = ui->table_state.active_col_idx;
        AslanColumnType type = tbl->columns[c].type;

        if (nk_button_label_styled(ctx, &menu_btn, "None")) {
          aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_NONE);
          ui->table_state.show_footer_menu = 0;
          nk_popup_close(ctx);
        }

        if (nk_button_label_styled(ctx, &menu_btn, "Count")) {
          aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_COUNT);
          ui->table_state.show_footer_menu = 0;
          nk_popup_close(ctx);
        }

        if (type == ASLAN_COL_NUMBER || type == ASLAN_COL_FORMULA) {
          if (nk_button_label_styled(ctx, &menu_btn, "Sum")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_SUM);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
          if (nk_button_label_styled(ctx, &menu_btn, "Average")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_AVG);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
          if (nk_button_label_styled(ctx, &menu_btn, "Min")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_MIN);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
          if (nk_button_label_styled(ctx, &menu_btn, "Max")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_MAX);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
        }

        if (type == ASLAN_COL_CHECKBOX) {
          if (nk_button_label_styled(ctx, &menu_btn, "Percent")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_PERCENT);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
        }

        nk_popup_end(ctx);
      } else {
        ui->table_state.show_footer_menu = 0;
      }
      nk_style_pop_style_item(ctx);
    }

    if (ui->table_state.show_col_settings &&
        ui->table_state.active_col_idx >= 0 &&
        ui->table_state.active_col_idx < (int)tbl->column_count) {
      int c = ui->table_state.active_col_idx;
      AslanColumn *col = &tbl->columns[c];

      // Calculate popup height based on column type
      float popup_h = 300.0f;
      if (col->type == ASLAN_COL_SELECT)
        popup_h += 80.0f + (col->option_count * 24);
      if (col->type == ASLAN_COL_FORMULA)
        popup_h += 170.0f;
      if (popup_h > 600)
        popup_h = 600;

      // Smart placement
      struct nk_rect popup_rect =
          table_smart_popup_rect(ui, ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y + 32, 240, popup_h);
      if (ui->table_state.menu_pos_y > ui->win_height - popup_h - 40) {
        popup_rect.y = ui->table_state.menu_pos_y - popup_h;
      }

      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Column Settings",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         popup_rect)) {
        char col_title[80];
        snprintf(col_title, sizeof(col_title), "Column: %s", col->name);
        if (table_popup_header(ui, ctx, col_title)) {
          ui->table_state.show_col_settings = 0;
          nk_popup_close(ctx);
        }
        // --- Column Name ---
        nk_layout_row_dynamic(ctx, 24, 1);
        nk_label(ctx, "Column Name:", NK_TEXT_LEFT);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
                                       ui->table_state.rename_buffer, 63,
                                       nk_filter_default);

        struct nk_style_button popup_btn = ctx->style.button;
        popup_btn.rounding = 4.0f;
        popup_btn.padding = nk_vec2(4, 4);

        if (nk_button_label_styled(ctx, &popup_btn, "Apply Name")) {
          strncpy(tbl->columns[c].name, ui->table_state.rename_buffer, 63);
          tbl->columns[c].name[63] = '\0';
          ui->table_state.show_col_settings = 0;
          nk_popup_close(ctx);
        }

        // --- Column Background Color ---
        nk_spacing(ctx, 1);
        nk_label(ctx, "Background Color:", NK_TEXT_LEFT);
        {
          uint32_t bg_presets[] = {
              0x00000000, // None (transparent)
              0x2383E230, // Blue
              0xE2235330, // Red
              0x23E25330, // Green
              0xE2B82330, // Yellow
              0x8E23E230, // Purple
              0xE26A2330, // Orange
              0xE2A0B830, // Pink
          };
          int preset_count = 8;
          nk_layout_row_static(ctx, 24, 24, preset_count);
          for (int i = 0; i < preset_count; i++) {
            uint32_t pc = bg_presets[i];
            struct nk_color swatch;
            if (pc == 0)
              swatch =
                  ui->dark_mode ? nk_rgb(40, 40, 44) : nk_rgb(240, 240, 238);
            else
              swatch = nk_rgba((pc >> 24) & 0xFF, (pc >> 16) & 0xFF,
                               (pc >> 8) & 0xFF, 180);

            struct nk_rect sw_bounds = nk_widget_bounds(ctx);
            int is_selected = (col->bg_color == pc);
            if (nk_button_color(ctx, swatch)) {
              col->bg_color = pc;
            }
            if (is_selected) {
              struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
              nk_stroke_rect(canvas, sw_bounds, 3.0f, 2.0f, COLOR_ACCENT);
            }
          }
          nk_layout_row_dynamic(ctx, 24, 1);
        }

        // --- Select Options (for SELECT columns) ---
        if (col->type == ASLAN_COL_SELECT) {
          nk_spacing(ctx, 1);
          nk_label(ctx, "--- Select Options ---", NK_TEXT_CENTERED);

          for (int i = 0; i < col->option_count && i < ASLAN_MAX_SELECT_OPTIONS;
               i++) {
            nk_layout_row_template_begin(ctx, 24.0f);
            nk_layout_row_template_push_static(ctx, 20.0f);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_push_static(ctx, 20.0f);
            nk_layout_row_template_end(ctx);

            uint32_t c_rgba = col->options[i].color;
            struct nk_color c_color =
                nk_rgba((c_rgba >> 24) & 0xFF, (c_rgba >> 16) & 0xFF,
                        (c_rgba >> 8) & 0xFF, 255);
            uint32_t presets[] = {0x2383E2FF, 0xE22353FF, 0x23E253FF,
                                  0xE2B823FF, 0x8E23E2FF, 0xE26A23FF};

            if (nk_button_color(ctx, c_color)) {
              int preset_idx = 0;
              for (int p = 0; p < 6; p++) {
                if (presets[p] == c_rgba) {
                  preset_idx = (p + 1) % 6;
                  break;
                }
              }
              aslan_table_set_option_color(tbl, c, i, presets[preset_idx]);
            }

            nk_label(ctx, col->options[i].label, NK_TEXT_LEFT);

            if (nk_button_label_styled(ctx, &popup_btn, "x")) {
              aslan_table_remove_option(tbl, c, i);
            }
          }

          if (col->option_count == 0) {
            nk_label(ctx, "(No options yet)", NK_TEXT_LEFT);
          }

          nk_spacing(ctx, 1);
          nk_label(ctx, "Add Option:", NK_TEXT_LEFT);
          nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
                                         ui->table_state.option_buffer, 63,
                                         nk_filter_default);

          uint32_t presets[] = {0x2383E2FF, 0xE22353FF, 0x23E253FF,
                                0xE2B823FF, 0x8E23E2FF, 0xE26A23FF};
          uint32_t next_color = presets[col->option_count % 6];

          if (nk_button_label_styled(ctx, &popup_btn, "+ Add Option")) {
            if (ui->table_state.option_buffer[0] != '\0') {
              aslan_table_add_option(tbl, c, ui->table_state.option_buffer,
                                     next_color);
              ui->table_state.option_buffer[0] = '\0';
            }
          }
        }

        // --- Formula editor (for FORMULA columns) ---
        if (col->type == ASLAN_COL_FORMULA) {
          nk_spacing(ctx, 1);
          nk_label(ctx, "Formula:", NK_TEXT_LEFT);

          {
            struct nk_color hint_col = nk_rgb(120, 120, 130);
            nk_label_colored(ctx, "[Col] * [Col] (Excel syntax)", NK_TEXT_LEFT,
                             hint_col);

            // Add Column Reference Buttons
            nk_label(ctx, "Add Reference:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 24, 3);
            for (size_t ref_c = 0; ref_c < tbl->column_count; ref_c++) {
              if ((int)ref_c == c)
                continue; // Skip self
              const char *ref_name = tbl->columns[ref_c].name;
              char btn_label[80];
              snprintf(btn_label, sizeof(btn_label), "[%s]", ref_name);
              if (nk_button_label(ctx, btn_label)) {
                size_t len = strlen(ui->table_state.formula_buffer);
                size_t btn_len = strlen(btn_label);
                if (len + btn_len <
                    sizeof(ui->table_state.formula_buffer) - 1) {
                  // Append to buffer
                  strcat(ui->table_state.formula_buffer, btn_label);
                }
              }
            }
          }

          nk_layout_row_dynamic(ctx, 60, 1);
          nk_edit_string_zero_terminated(
              ctx, NK_EDIT_BOX | NK_EDIT_MULTILINE,
              ui->table_state.formula_buffer,
              sizeof(ui->table_state.formula_buffer) - 1, nk_filter_default);
          nk_layout_row_dynamic(ctx, 24, 1);

          if (nk_button_label_styled(ctx, &popup_btn, "Apply Formula")) {
            aslan_table_set_formula(tbl, c, ui->table_state.formula_buffer);

            // Persist formula to AOF
            Page *pg = doc_page_get_active(engine->doc);
            if (pg) {
              char cmd_buf[1200];
              snprintf(cmd_buf, sizeof(cmd_buf),
                       "block_table_col_formula %u %u %d \"%s\"", pg->id,
                       block->id, c, ui->table_state.formula_buffer);
              engine_execute_command(engine, cmd_buf);
            }

            ui->table_state.show_col_settings = 0;
            nk_popup_close(ctx);
          }

          if (nk_button_label_styled(ctx, &popup_btn, "? Formula Help")) {
            ui->table_state.show_formula_help = 1;
          }
        }

        nk_spacing(ctx, 1);
        struct nk_style_button del_btn = popup_btn;
        del_btn.normal = nk_style_item_color(nk_rgba(200, 50, 50, 20));
        del_btn.hover = nk_style_item_color(nk_rgba(200, 50, 50, 40));
        del_btn.active = nk_style_item_color(nk_rgba(200, 50, 50, 60));
        del_btn.text_normal = nk_rgb(200, 50, 50);
        del_btn.text_hover = nk_rgb(255, 100, 100);

        if (nk_button_label_styled(ctx, &del_btn, "Delete Column")) {
          aslan_table_remove_column(tbl, c);
          ui->table_state.show_col_settings = 0;
          nk_popup_close(ctx);
        }
        nk_popup_end(ctx);
      } else {
        ui->table_state.show_col_settings = 0;
      }
    }

    // =========================================================================
    // SELECT CELL POPUP (Options + Add Option)
    // =========================================================================
    if (ui->table_state.show_select_popup &&
        ui->table_state.active_col_idx >= 0 &&
        ui->table_state.active_col_idx < (int)tbl->column_count) {
      int c = ui->table_state.active_col_idx;
      AslanColumn *col = &tbl->columns[c];
      uint64_t row_id = ui->table_state.select_popup_row;

      // Calculate popup height based on options count
      float popup_h =
          100.0f + (col->option_count * 28) + 80; // header + options + add
      if (popup_h > 420)
        popup_h = 420;
      if (popup_h < 180)
        popup_h = 180;

      struct nk_rect popup_rect =
          table_smart_popup_rect(ui, ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y, 220, popup_h);
      if (ui->table_state.menu_pos_y > ui->win_height - popup_h - 40) {
        popup_rect.y = ui->table_state.menu_pos_y - popup_h;
      }

      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Select Option",
                         NK_WINDOW_BORDER, popup_rect)) {
        int close_popup = 0; // Flag to defer closing

        if (table_popup_header(ui, ctx, "Select")) {
          close_popup = 1;
        }

        // Show existing options in a scrollable group
        nk_layout_row_dynamic(ctx, popup_h - 130, 1);
        if (nk_group_begin(ctx, "select_options_list",
                           NK_WINDOW_NO_SCROLLBAR)) { // Scrollbar if needed
          for (int i = 0; i < col->option_count; i++) {
            nk_layout_row_template_begin(ctx, 24);
            nk_layout_row_template_push_static(ctx, 24); // Color button
            nk_layout_row_template_push_dynamic(ctx);    // Option label
            nk_layout_row_template_end(ctx);

            // 1. Color Button
            uint32_t opt_col = col->options[i].color;
            struct nk_color c_bg =
                nk_rgba((opt_col >> 24) & 0xFF, (opt_col >> 16) & 0xFF,
                        (opt_col >> 8) & 0xFF, 255);

            struct nk_style_button color_btn_style = ctx->style.button;
            color_btn_style.normal = nk_style_item_color(c_bg);
            color_btn_style.hover = nk_style_item_color(c_bg);
            color_btn_style.active = nk_style_item_color(c_bg);
            color_btn_style.rounding = 12.0f; // Circle
            color_btn_style.border = 1.0f;
            color_btn_style.border_color = nk_rgba(0, 0, 0, 50);

            if (nk_button_label_styled(ctx, &color_btn_style, "")) {
              if (ui->table_state.active_color_picker_option == i) {
                ui->table_state.active_color_picker_option = -1;
              } else {
                ui->table_state.active_color_picker_option = i;
              }
            }

            // 2. Option Label (Select/Deselect)
            int is_selected = (aslan_table_get_select(tbl, c, row_id) == i);
            struct nk_style_button opt_btn_style = ctx->style.button;

            // Background for label part
            struct nk_color label_bg = nk_rgba(c_bg.r, c_bg.g, c_bg.b, 40);
            if (is_selected) {
              label_bg = nk_rgba(c_bg.r, c_bg.g, c_bg.b, 200);
            }

            if (is_selected) {
              opt_btn_style.normal = nk_style_item_color(label_bg);
              opt_btn_style.text_normal = nk_rgb(255, 255, 255);
              opt_btn_style.hover = nk_style_item_color(label_bg);
              opt_btn_style.text_hover = nk_rgb(255, 255, 255);
            } else {
              opt_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
              struct nk_color txt =
                  ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;
              opt_btn_style.text_normal = txt;
              opt_btn_style.text_hover = txt;
              opt_btn_style.hover = nk_style_item_color(label_bg);
            }
            opt_btn_style.rounding = 4.0f;
            opt_btn_style.text_alignment = NK_TEXT_LEFT;

            char label[80];
            if (is_selected) {
              snprintf(label, sizeof(label), "  %s  (✓)",
                       col->options[i].label);
            } else {
              snprintf(label, sizeof(label), "  %s", col->options[i].label);
            }

            if (nk_button_label_styled(ctx, &opt_btn_style, label)) {
              if (is_selected) {
                aslan_table_set_select(tbl, c, row_id, -1);
              } else {
                aslan_table_set_select(tbl, c, row_id, i);
              }
              close_popup = 1;
            }

            // 3. Color Picker Palette (if active)
            if (ui->table_state.active_color_picker_option == i) {
              nk_layout_row_dynamic(ctx, 24, 9);
              uint32_t palette[] = {
                  0x888888FF, // Gray
                  0x8D6E63FF, // Brown
                  0xFFA726FF, // Orange
                  0xFFEE58FF, // Yellow
                  0x66BB6AFF, // Green
                  0x42A5F5FF, // Blue
                  0xAB47BCFF, // Purple
                  0xEC407AFF, // Pink
                  0xEF5350FF  // Red
              };

              for (int p = 0; p < 9; p++) {
                struct nk_color p_col = nk_rgba((palette[p] >> 24) & 0xFF,
                                                (palette[p] >> 16) & 0xFF,
                                                (palette[p] >> 8) & 0xFF, 255);

                struct nk_style_button pal_style = ctx->style.button;
                pal_style.normal = nk_style_item_color(p_col);
                pal_style.hover = nk_style_item_color(p_col);
                pal_style.active = nk_style_item_color(p_col);
                pal_style.rounding = 12.0f;
                pal_style.border =
                    (col->options[i].color == palette[p]) ? 2.0f : 0.0f;
                pal_style.border_color = nk_rgb(255, 255, 255);

                if (nk_button_label_styled(ctx, &pal_style, "")) {
                  aslan_table_update_option_color(tbl, c, i, palette[p]);
                  ui->table_state.active_color_picker_option = -1;
                }
              }
            }
          }
          if (col->option_count == 0) {
            nk_label(ctx, "No options yet", NK_TEXT_CENTERED);
          }
          nk_group_end(ctx);
        }

        // Separator
        nk_layout_row_dynamic(ctx, 1, 1);
        struct nk_rect sep_rect = nk_widget_bounds(ctx);
        nk_fill_rect(nk_window_get_canvas(ctx), sep_rect, 0,
                     nk_rgb(100, 100, 100));

        // Add new option field
        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 60);
        nk_layout_row_template_end(ctx);

        // Edit box styling
        struct nk_style_edit old_es = ctx->style.edit;
        ctx->style.edit.normal = nk_style_item_color(
            ui->dark_mode ? nk_rgb(40, 42, 48) : nk_rgb(240, 240, 245));
        ctx->style.edit.active = ctx->style.edit.normal;
        ctx->style.edit.hover = ctx->style.edit.normal;
        ctx->style.edit.border = 1.0f;
        ctx->style.edit.border_color = nk_rgb(100, 100, 100);
        ctx->style.edit.rounding = 4.0f;

        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
                                       ui->table_state.option_buffer, 63,
                                       nk_filter_default);

        ctx->style.edit = old_es; // Restore

        // Check for Enter key in text field
        int enter_pressed = nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER);
        int click_add = nk_button_label(ctx, "Add");

        if ((click_add || enter_pressed) &&
            ui->table_state.option_buffer[0] != '\0') {
          // Generate pseudo-random color based on name hash
          uint32_t hash = 5381;
          const char *str = ui->table_state.option_buffer;
          int c_char;
          while ((c_char = *str++))
            hash = ((hash << 5) + hash) + c_char;

          // HSV to RGB conversion for nice pastel colors
          float h = (hash % 360);
          float s = 0.6f;
          float v = 0.85f;

          float c_hsv = v * s;
          float x_hsv = c_hsv * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
          float m_hsv = v - c_hsv;
          float r = 0, g = 0, b = 0;
          if (h < 60) {
            r = c_hsv;
            g = x_hsv;
            b = 0;
          } else if (h < 120) {
            r = x_hsv;
            g = c_hsv;
            b = 0;
          } else if (h < 180) {
            r = 0;
            g = c_hsv;
            b = x_hsv;
          } else if (h < 240) {
            r = 0;
            g = x_hsv;
            b = c_hsv;
          } else if (h < 300) {
            r = x_hsv;
            g = 0;
            b = c_hsv;
          } else {
            r = c_hsv;
            g = 0;
            b = x_hsv;
          }

          uint32_t final_color = ((uint8_t)((r + m_hsv) * 255) << 24) |
                                 ((uint8_t)((g + m_hsv) * 255) << 16) |
                                 ((uint8_t)((b + m_hsv) * 255) << 8) | 255;

          int new_idx = aslan_table_add_option(
              tbl, c, ui->table_state.option_buffer, final_color);

          if (new_idx >= 0) {
            aslan_table_set_select(tbl, c, row_id,
                                   new_idx); // Auto-select new option
          }
          ui->table_state.option_buffer[0] = '\0'; // Clear buffer
          close_popup = 1;
        }

        if (close_popup) {
          ui->table_state.show_select_popup = 0;
          nk_popup_close(ctx);
        }

        nk_popup_end(ctx);
      } else {
        ui->table_state.show_select_popup = 0;
      }
    }

    // -----------------------------------------------------------------
    // DATE PICKER POPUP (Notion-style calendar grid)
    // -----------------------------------------------------------------
    if (ui->table_state.show_date_picker &&
        ui->table_state.date_picker_col >= 0 &&
        ui->table_state.date_picker_col < (int)tbl->column_count &&
        tbl->columns[ui->table_state.date_picker_col].type == ASLAN_COL_DATE) {

      struct nk_color dp_bg =
          ui->dark_mode ? nk_rgb(38, 40, 46) : nk_rgb(255, 255, 255);
      nk_style_push_style_item(ctx, &ctx->style.window.fixed_background,
                               nk_style_item_color(dp_bg));

      float dp_w = 270, dp_h = 330;
      struct nk_rect dp_rect =
          table_smart_popup_rect(ui, ui->table_state.date_picker_x,
                                 ui->table_state.date_picker_y + 2, dp_w, dp_h);

      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Date Picker",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER, dp_rect)) {
        if (table_popup_header(ui, ctx, "Date")) {
          ui->table_state.show_date_picker = 0;
          nk_popup_close(ctx);
        }

        int dp_year = ui->table_state.date_picker_year;
        int dp_month = ui->table_state.date_picker_month; // 0-11

        struct nk_color dp_text = ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;
        struct nk_color dp_muted =
            ui->dark_mode ? nk_rgb(100, 102, 110) : nk_rgb(160, 162, 168);
        struct nk_color dp_accent = nk_rgb(45, 127, 249);

        // --- Month/Year navigation bar ---
        struct nk_style_button nav_btn = ctx->style.button;
        nav_btn.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
        nav_btn.hover = nk_style_item_color(
            ui->dark_mode ? nk_rgba(255, 255, 255, 20) : nk_rgba(0, 0, 0, 10));
        nav_btn.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
        nav_btn.border = 0;
        nav_btn.rounding = 4;
        nav_btn.text_normal = dp_text;
        nav_btn.text_hover = dp_accent;
        nav_btn.text_active = dp_text;

        nk_layout_row_begin(ctx, NK_STATIC, 28, 3);

        // Previous month
        nk_layout_row_push(ctx, 30);
        if (nk_button_label_styled(ctx, &nav_btn, "<")) {
          ui->table_state.date_picker_month--;
          if (ui->table_state.date_picker_month < 0) {
            ui->table_state.date_picker_month = 11;
            ui->table_state.date_picker_year--;
          }
          ui_request_redraw(ui);
        }

        // Month/Year title
        static const char *month_names[] = {"January", "February", "March",
                                            "April",   "May",      "June",
                                            "July",    "August",   "September",
                                            "October", "November", "December"};
        char title[64];
        snprintf(title, sizeof(title), "%s %d", month_names[dp_month], dp_year);
        nk_layout_row_push(ctx, dp_w - 80);
        struct nk_style_button title_btn = nav_btn;
        title_btn.text_normal = dp_text;
        nk_button_label_styled(ctx, &title_btn, title);

        // Next month
        nk_layout_row_push(ctx, 30);
        if (nk_button_label_styled(ctx, &nav_btn, ">")) {
          ui->table_state.date_picker_month++;
          if (ui->table_state.date_picker_month > 11) {
            ui->table_state.date_picker_month = 0;
            ui->table_state.date_picker_year++;
          }
          ui_request_redraw(ui);
        }
        nk_layout_row_end(ctx);

        // --- Day-of-week headers ---
        nk_layout_row_dynamic(ctx, 20, 7);
        static const char *dow[] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
        for (int i = 0; i < 7; i++) {
          nk_label_colored(ctx, dow[i], NK_TEXT_CENTERED, dp_muted);
        }

        // --- Calendar grid ---
        // Calculate first day of month (Monday=0 .. Sunday=6)
        struct tm first_tm = {0};
        first_tm.tm_year = dp_year - 1900;
        first_tm.tm_mon = dp_month;
        first_tm.tm_mday = 1;
        first_tm.tm_isdst = -1;
        mktime(&first_tm);                 // normalize
        int first_wday = first_tm.tm_wday; // 0=Sunday
        // Convert to Monday=0 system
        int start_offset = (first_wday + 6) % 7;

        // Days in this month
        int days_in_month;
        {
          struct tm next_m = {0};
          next_m.tm_year = dp_year - 1900;
          next_m.tm_mon = dp_month + 1;
          next_m.tm_mday = 0; // day 0 of next month = last day of this month
          next_m.tm_isdst = -1;
          mktime(&next_m);
          days_in_month = next_m.tm_mday;
        }

        // Get current stored date for highlighting
        int64_t stored_ts =
            aslan_table_get_date(tbl, ui->table_state.date_picker_col,
                                 ui->table_state.date_picker_row);
        int sel_day = 0, sel_month = -1, sel_year = 0;
        if (stored_ts > 0) {
          time_t st = (time_t)(stored_ts / 1000);
          struct tm *stm = localtime(&st);
          sel_day = stm->tm_mday;
          sel_month = stm->tm_mon;
          sel_year = stm->tm_year + 1900;
        }

        // Today for highlighting
        time_t now_t = time(NULL);
        struct tm *now_tm = localtime(&now_t);
        int today_day = now_tm->tm_mday;
        int today_month = now_tm->tm_mon;
        int today_year = now_tm->tm_year + 1900;

        // Render 6 rows of 7 days
        struct nk_style_button day_btn = nav_btn;
        day_btn.padding = nk_vec2(0, 0);

        for (int row = 0; row < 6; row++) {
          nk_layout_row_dynamic(ctx, 28, 7);
          for (int col = 0; col < 7; col++) {
            int cell = row * 7 + col;
            int day = cell - start_offset + 1;

            if (day < 1 || day > days_in_month) {
              // Empty cell
              nk_label(ctx, "", NK_TEXT_CENTERED);
            } else {
              char day_str[4];
              snprintf(day_str, sizeof(day_str), "%d", day);

              int is_selected = (day == sel_day && dp_month == sel_month &&
                                 dp_year == sel_year);
              int is_today = (day == today_day && dp_month == today_month &&
                              dp_year == today_year);

              struct nk_style_button d = day_btn;
              if (is_selected) {
                d.normal = nk_style_item_color(dp_accent);
                d.hover = nk_style_item_color(dp_accent);
                d.text_normal = nk_rgb(255, 255, 255);
                d.text_hover = nk_rgb(255, 255, 255);
                d.rounding = 14;
              } else if (is_today) {
                d.normal = nk_style_item_color(nk_rgba(45, 127, 249, 30));
                d.hover = nk_style_item_color(nk_rgba(45, 127, 249, 50));
                d.text_normal = dp_accent;
                d.text_hover = dp_accent;
                d.rounding = 14;
              } else {
                d.text_normal = dp_text;
                d.text_hover = dp_accent;
              }

              if (nk_button_label_styled(ctx, &d, day_str)) {
                // Build timestamp and set date
                struct tm new_tm = {0};
                new_tm.tm_year = dp_year - 1900;
                new_tm.tm_mon = dp_month;
                new_tm.tm_mday = day;
                new_tm.tm_hour = 12; // noon to avoid DST issues
                new_tm.tm_isdst = -1;
                time_t new_time = mktime(&new_tm);
                if (new_time != -1) {
                  int64_t new_ts = (int64_t)new_time * 1000;
                  aslan_table_set_date(tbl, ui->table_state.date_picker_col,
                                       ui->table_state.date_picker_row, new_ts);
                }
                ui->table_state.show_date_picker = 0;
                nk_popup_close(ctx);
                ui_request_redraw(ui);
              }
            }
          }
          // Stop rendering rows if we've passed all days
          if ((row + 1) * 7 - start_offset > days_in_month)
            break;
        }

        // --- "Today" shortcut + Clear button ---
        nk_layout_row_dynamic(ctx, 4, 1);
        nk_spacing(ctx, 1);
        nk_layout_row_dynamic(ctx, 24, 2);

        struct nk_style_button today_btn = nav_btn;
        today_btn.text_normal = dp_accent;
        today_btn.text_hover = dp_accent;
        if (nk_button_label_styled(ctx, &today_btn, "Today")) {
          time_t now = time(NULL);
          int64_t now_ms = (int64_t)now * 1000;
          aslan_table_set_date(tbl, ui->table_state.date_picker_col,
                               ui->table_state.date_picker_row, now_ms);
          ui->table_state.show_date_picker = 0;
          nk_popup_close(ctx);
          ui_request_redraw(ui);
        }

        struct nk_style_button clear_btn = nav_btn;
        clear_btn.text_normal =
            ui->dark_mode ? nk_rgb(200, 100, 100) : nk_rgb(180, 60, 60);
        clear_btn.text_hover = nk_rgb(220, 80, 80);
        if (nk_button_label_styled(ctx, &clear_btn, "Clear")) {
          aslan_table_set_date(tbl, ui->table_state.date_picker_col,
                               ui->table_state.date_picker_row, 0);
          ui->table_state.show_date_picker = 0;
          nk_popup_close(ctx);
          ui_request_redraw(ui);
        }

        nk_popup_end(ctx);
      } else {
        ui->table_state.show_date_picker = 0;
      }
      nk_style_pop_style_item(ctx);
    }
  }
}

// =============================================================================
// Test Function
// =============================================================================

int test_ui_table(void) {
  ASLAN_INFO("Running ui_table tests...");

  // Test 1: Column type icon mapping
  ASLAN_ASSERT(table_col_type_icon(ASLAN_COL_TEXT) != NULL);
  ASLAN_ASSERT(table_col_type_icon(ASLAN_COL_NUMBER) != NULL);
  ASLAN_ASSERT(table_col_type_icon(ASLAN_COL_CHECKBOX) != NULL);

  // Test 2: Column type name mapping
  ASLAN_ASSERT(table_col_type_name(ASLAN_COL_TEXT) != NULL);
  ASLAN_ASSERT(table_col_type_name(ASLAN_COL_DATE) != NULL);

  ASLAN_INFO("ui_table tests PASSED");
  return 0;
}
