// =============================================================================
// ASLAN UI TABLE - Table Block Rendering and Logic
// =============================================================================

#include "ui_table.h"
#include "../document/aslan_table.h"
#include "ui_shared.h"
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

void ui_render_block_table(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width) {
  (void)engine;
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  AslanTable *tbl = td->columnar;
  // printf("[DEBUG] Render Table %u. Tbl: %p, Rows: %zu\n", block->id,
  // (void*)tbl, tbl ? tbl->row_count : 0);

  // Constants
  const float row_h = 28.0f;
  const float header_h = 32.0f;
  const float footer_h = 26.0f;
  const float min_col_w = 80.0f;
  const float max_col_w = 250.0f;
  const float row_num_w = 40.0f;

  // Colors - Dark header/footer for professional Notion-like look
  struct nk_color header_bg = nk_rgb(25, 25, 28); // Near-black in all modes
  struct nk_color border_color =
      ui->dark_mode ? nk_rgb(60, 62, 68) : nk_rgb(237, 236, 233); // #EDECE9
  struct nk_color footer_bg = nk_rgb(30, 30, 34); // Near-black in all modes
  struct nk_color muted_gray =
      ui->dark_mode ? nk_rgb(100, 100, 105) : nk_rgb(160, 160, 165);

  // Light text colors for dark header/footer
  struct nk_color header_text_col = nk_rgb(245, 245, 250); // White for dark bg
  struct nk_color header_icon_col =
      nk_rgba(245, 245, 250, 160); // Slightly transparent
  struct nk_color footer_text_col =
      nk_rgb(200, 200, 205); // Slightly muted white

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
      ui->table_state.active_row_idx = (ui->table_state.active_row_idx > 0)
                                           ? ui->table_state.active_row_idx - 1
                                           : 0;
    }
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_DOWN)) {
      if (ui->table_state.active_row_idx < view_count_nav - 1) {
        ui->table_state.active_row_idx++;
      }
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
    // Tab: advance to next cell (wrap to next row)
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_TAB)) {
      ui->table_state.active_col_idx++;
      if (ui->table_state.active_col_idx >= (int)tbl->column_count) {
        ui->table_state.active_col_idx = 0;
        if (ui->table_state.active_row_idx < view_count_nav - 1) {
          ui->table_state.active_row_idx++;
        }
      }
    }
    // Enter: start editing
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
      ui->table_state.is_editing_cell = 1;
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
    const struct nk_user_font *old_table_font = ctx->style.font;
    if (ui->font_inter) {
      nk_style_set_font(ctx, &ui->font_inter->handle);
    }

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
    const float add_col_w = 32.0f;

    // Save and Push Global Table Styles (Zero Padding/Spacing for "Simple
    // Grid")
    struct nk_style_item old_edit_normal = ctx->style.edit.normal;
    struct nk_style_item old_edit_hover = ctx->style.edit.hover;
    struct nk_style_item old_edit_active = ctx->style.edit.active;
    ctx->style.edit.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.edit.hover = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.edit.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ctx->style.edit.border = 0.0f;
    ctx->style.edit.padding = nk_vec2(6, 4); // Padding for interactivity

    struct nk_vec2 old_window_padding = ctx->style.window.padding;
    struct nk_vec2 old_group_padding = ctx->style.window.group_padding;
    struct nk_vec2 old_window_spacing = ctx->style.window.spacing;
    struct nk_vec2 old_button_padding = ctx->style.button.padding;
    ctx->style.window.padding = nk_vec2(0, 0);
    ctx->style.window.group_padding = nk_vec2(0, 0);
    ctx->style.window.spacing = nk_vec2(0, 0);
    ctx->style.button.padding = nk_vec2(8, 4); // Good padding for buttons
    nk_style_push_vec2(ctx, &ctx->style.text.padding, nk_vec2(4, 2));

    // HEADER ROW (+ Add Column button at end)
    nk_layout_row_begin(ctx, NK_STATIC, header_h, (int)col_count + 2);

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

      // Hidden button for settings/sorting - styled transparent
      struct nk_style_button hdr_btn_style = ctx->style.button;
      hdr_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      hdr_btn_style.hover = nk_style_item_color(nk_rgba(255, 255, 255, 20));
      hdr_btn_style.active = nk_style_item_color(nk_rgba(255, 255, 255, 30));
      hdr_btn_style.border = 0;
      hdr_btn_style.text_normal = header_text_col;
      hdr_btn_style.text_hover = header_text_col;
      hdr_btn_style.text_active = header_text_col;

      if (nk_button_label_styled(ctx, &hdr_btn_style, "")) {
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

      // Header text - use fixed light colors for dark background
      struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
      const char *icon = table_col_type_icon(tbl->columns[c].type);

      // Draw Icon and Text manually with font switching
      if (ui->font_icons) {
        nk_style_set_font(ctx, &ui->font_icons->handle);
        nk_draw_text(canvas,
                     nk_rect(hdr_bounds.x + 6, hdr_bounds.y + 6, 20, 20), icon,
                     strlen(icon), &ui->font_icons->handle, nk_rgba(0, 0, 0, 0),
                     header_icon_col);
        nk_style_set_font(ctx, &ui->font_inter->handle);
      }

      nk_draw_text(
          canvas,
          nk_rect(hdr_bounds.x + 32, hdr_bounds.y + 6, hdr_bounds.w - 36, 20),
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

    // Inline + Add Column header
    nk_layout_row_push(ctx, add_col_w);
    hdr_bounds = nk_widget_bounds(ctx);
    nk_fill_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, header_bg);
    nk_stroke_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, 1, border_color);

    struct nk_style_button add_col_btn = ctx->style.button;
    add_col_btn.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    add_col_btn.hover = nk_style_item_color(nk_rgba(255, 255, 255, 20));
    add_col_btn.text_normal = header_text_col;
    if (nk_button_label_styled(ctx, &add_col_btn, "+")) {
      ui->table_state.active_block_id = block->id;
      ui->table_state.show_col_menu = !ui->table_state.show_col_menu;
      ui->table_state.show_footer_menu = 0;
      ui->table_state.menu_pos_x = hdr_bounds.x;
      ui->table_state.menu_pos_y = hdr_bounds.y + hdr_bounds.h;
    }

    nk_layout_row_end(ctx);

    // -----------------------------------------------------------------------
    // DATA ROWS
    // -----------------------------------------------------------------------
    struct nk_color notion_row_bg =
        ui->dark_mode ? nk_rgb(25, 26, 30) : nk_rgb(251, 251, 250); // #FBFBFA
    struct nk_color notion_border = ui->dark_mode
                                        ? nk_rgba(60, 62, 66, 60)
                                        : nk_rgb(237, 236, 233); // #EDECE9
    struct nk_color notion_text =
        ui->dark_mode ? nk_rgb(220, 220, 225) : nk_rgb(55, 53, 47); // #37352F
    struct nk_color vert_sep =
        ui->dark_mode ? nk_rgba(60, 62, 66, 40)
                      : nk_rgba(237, 236, 233, 180); // Subtle guide
    struct nk_color row_hover_bg =
        ui->dark_mode ? nk_rgba(255, 255, 255, 20)
                      : nk_rgba(55, 53, 47, 10); // Slightly more subtle hover
    struct nk_color active_cell_outline = nk_rgb(35, 131, 226); // Notion blue

    for (size_t v = 0; v < view_count && v < 100; v++) { // Limit visible rows
      uint64_t row_id = aslan_table_view_row_id(tbl, (int)v);
      if (row_id == 0)
        continue;

      // Data Row Layout using nk_layout_row_begin/push/end (consistent API)
      nk_layout_row_begin(ctx, NK_STATIC, row_h, (int)col_count + 2);

      // Calculate row hover state
      struct nk_rect row_bounds = nk_layout_widget_bounds(ctx);
      int is_row_hovered =
          nk_input_is_mouse_hovering_rect(&ctx->input, row_bounds);
      struct nk_color current_row_bg =
          is_row_hovered ? row_hover_bg : notion_row_bg;
      struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

      // Row number cell
      nk_layout_row_push(ctx, row_num_w);
      struct nk_rect cell_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(canvas, cell_bounds, 0, current_row_bg);
      nk_stroke_line(canvas, cell_bounds.x, cell_bounds.y + cell_bounds.h - 1,
                     cell_bounds.x + cell_bounds.w,
                     cell_bounds.y + cell_bounds.h - 1, 1.0f, notion_border);

      char row_num_str[16];
      snprintf(row_num_str, sizeof(row_num_str), "%zu", v + 1);
      nk_label_colored(ctx, row_num_str, NK_TEXT_CENTERED, muted_gray);

      // Data cells - direct widget placement (no nk_group)
      for (size_t c = 0; c < col_count; c++) {
        nk_layout_row_push(ctx, col_widths[c]);
        cell_bounds = nk_widget_bounds(ctx);

        // Background
        nk_fill_rect(canvas, cell_bounds, 0, current_row_bg);

        // Borders (bottom only - subtle horizontal line)
        nk_stroke_line(canvas, cell_bounds.x, cell_bounds.y + cell_bounds.h - 1,
                       cell_bounds.x + cell_bounds.w,
                       cell_bounds.y + cell_bounds.h - 1, 1.0f, notion_border);

        // Vertical separator on hover only (Notion style)
        if (is_row_hovered) {
          nk_stroke_line(canvas, cell_bounds.x + cell_bounds.w - 1,
                         cell_bounds.y + 4, cell_bounds.x + cell_bounds.w - 1,
                         cell_bounds.y + cell_bounds.h - 4, 1.0f, vert_sep);
        }

        AslanColumnType type = tbl->columns[c].type;
        int is_active = (ui->table_state.active_row_idx == (int)v &&
                         ui->table_state.active_col_idx == (int)c);

        if (is_active) {
          // Blue outline for active cell (Notion-like edit focus)
          nk_stroke_rect(canvas,
                         nk_rect(cell_bounds.x + 1, cell_bounds.y + 1,
                                 cell_bounds.w - 2, cell_bounds.h - 2),
                         2.0f, 2.0f, active_cell_outline);
        }

        switch (type) {
        case ASLAN_COL_TEXT:
        case ASLAN_COL_FORMULA: {
          char buf[1024];
          const char *val = aslan_table_get_text(tbl, (int)c, row_id);
          strncpy(buf, val ? val : "", sizeof(buf) - 1);
          buf[sizeof(buf) - 1] = '\0';

          nk_flags res = nk_edit_string_zero_terminated(
              ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER, buf,
              sizeof(buf) - 1, nk_filter_default);

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

          nk_flags res = nk_edit_string_zero_terminated(
              ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER, buf,
              sizeof(buf) - 1, nk_filter_decimal);

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

          // Check if this cell is being edited
          int is_editing = (ui->table_state.active_row_idx == (int)v &&
                            ui->table_state.active_col_idx == (int)c &&
                            ui->table_state.is_editing_cell);

          if (is_editing) {
            // Editing mode: show editable field
            if (ui->table_state.date_buffer[0] == '\0' && ts > 0) {
              time_t t = (time_t)(ts / 1000);
              struct tm *tm_ptr = localtime(&t);
              strftime(ui->table_state.date_buffer,
                       sizeof(ui->table_state.date_buffer), "%Y-%m-%d", tm_ptr);
            }

            // Style for active edit - make it look like an active field
            struct nk_style_item old_edit_normal = ctx->style.edit.normal;
            struct nk_color old_edit_text = ctx->style.edit.text_normal;

            ctx->style.edit.normal = nk_style_item_color(
                ui->dark_mode ? nk_rgb(45, 47, 52) : nk_rgb(245, 245, 250));
            ctx->style.edit.text_normal =
                ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;
            ctx->style.edit.text_hover =
                ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;
            ctx->style.edit.text_active =
                ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;

            // NEW: Auto-focus on first frame
            if (ui->table_state.date_field_active) {
              nk_edit_focus(ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT |
                                     NK_EDIT_SIG_ENTER);
              ui->table_state.date_field_active = 0;
            }

            nk_flags res = nk_edit_string_zero_terminated(
                ctx, NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER,
                ui->table_state.date_buffer,
                sizeof(ui->table_state.date_buffer) - 1, nk_filter_ascii);

            ctx->style.edit.normal = old_edit_normal;
            ctx->style.edit.text_normal = old_edit_text;

            if (res & NK_EDIT_COMMITED) {
              ui->table_state.is_editing_cell = 0;
              // Parse and save - try multiple formats for robustness
              struct tm tm_val = {0};
              int y = 0, m = 1, d = 1;
              const char *db = ui->table_state.date_buffer;
              if (sscanf(db, "%d-%d-%d", &y, &m, &d) == 3 ||
                  sscanf(db, "%d/%d/%d", &y, &m, &d) == 3 ||
                  sscanf(db, "%d.%d.%d", &y, &m, &d) == 3 ||
                  sscanf(db, "%d %d %d", &y, &m, &d) == 3) {

                // Heuristic: If year is small (like 24), assume 2000s
                if (y < 100)
                  y += 2000;

                tm_val.tm_year = y - 1900;
                tm_val.tm_mon = m - 1;
                tm_val.tm_mday = d;
                tm_val.tm_isdst = -1; // Let mktime determine DST

                time_t new_t = mktime(&tm_val);
                if (new_t != -1) {
                  int64_t new_ts = (int64_t)new_t * 1000;
                  aslan_table_set_date(tbl, (int)c, row_id, new_ts);
                }
              }
              ui->table_state.date_buffer[0] = '\0'; // Clear for next edit
            }
            if (res & NK_EDIT_DEACTIVATED) {
              ui->table_state.is_editing_cell = 0;
              // Don't clear immediately, let it persist or re-init on next
              // click
            }
          } else {
            // Display mode: show date or placeholder
            char display[32];
            if (ts > 0) {
              time_t t = (time_t)(ts / 1000);
              struct tm *tm_ptr = localtime(&t);
              strftime(display, sizeof(display), "%Y-%m-%d", tm_ptr);
            } else {
              strncpy(display, "YYYY-MM-DD", sizeof(display));
            }

            // Button that enters edit mode on click
            struct nk_style_button date_btn_style = ctx->style.button;
            date_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
            date_btn_style.hover = nk_style_item_color(nk_rgba(80, 80, 80, 40));
            date_btn_style.active =
                nk_style_item_color(nk_rgba(60, 60, 60, 60));
            date_btn_style.border = 0;
            struct nk_color cell_txt =
                ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;
            date_btn_style.text_normal = ts > 0 ? cell_txt : muted_gray;
            date_btn_style.text_hover = cell_txt;
            date_btn_style.text_active = cell_txt;

            if (nk_button_label_styled(ctx, &date_btn_style, display)) {
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.is_editing_cell = 1;
              ui->table_state.date_field_active = 1; // Trigger focus
              ui->table_state.date_buffer[0] = '\0'; // Trigger re-init
            }
          }
          break;
        }
        case ASLAN_COL_SELECT: {
          AslanColumn *col = &tbl->columns[c];
          int sel = aslan_table_get_select(tbl, (int)c, row_id);

          // Display current selection or placeholder
          const char *display_text = "(click to select)";
          if (sel >= 0 && sel < col->option_count) {
            display_text = col->options[sel].label;
          } else if (col->option_count == 0) {
            display_text = "(+ add option)";
          }

          // Style for the select button
          struct nk_style_button sel_btn_style = ctx->style.button;
          sel_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
          sel_btn_style.hover = nk_style_item_color(nk_rgba(80, 80, 80, 40));
          sel_btn_style.active = nk_style_item_color(nk_rgba(60, 60, 60, 60));
          sel_btn_style.border = 0;
          struct nk_color sel_txt =
              ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;
          sel_btn_style.text_normal = (sel >= 0) ? sel_txt : muted_gray;
          sel_btn_style.text_hover = sel_txt;
          sel_btn_style.text_active = sel_txt;

          // Create a unique ID for this cell's popup
          char popup_id[64];
          snprintf(popup_id, sizeof(popup_id), "select_popup_%zu_%lu", c,
                   (unsigned long)row_id);

          struct nk_rect btn_bounds = nk_widget_bounds(ctx);
          if (nk_button_label_styled(ctx, &sel_btn_style, display_text)) {
            ui->table_state.active_block_id = block->id;
            ui->table_state.active_col_idx = (int)c;
            ui->table_state.active_row_idx = (int)v;
            ui->table_state.show_select_popup = 1;
            ui->table_state.select_popup_row = row_id;
            ui->table_state.menu_pos_x = btn_bounds.x;
            ui->table_state.menu_pos_y = btn_bounds.y + btn_bounds.h;
            // Close other menus
            ui->table_state.show_col_menu = 0;
            ui->table_state.show_footer_menu = 0;
            ui->table_state.show_col_settings = 0;
          }
          break;
        }
        case ASLAN_COL_CHECKBOX: {
          bool checked = aslan_table_get_checkbox(tbl, (int)c, row_id);

          struct nk_style_button chk_style = ctx->style.button;
          chk_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
          chk_style.hover = nk_style_item_color(nk_rgba(0, 0, 0, 0));
          chk_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
          chk_style.text_normal = notion_text;
          chk_style.text_hover = COLOR_ACCENT;
          chk_style.padding = nk_vec2(0, 0);

          if (nk_button_label_styled(ctx, &chk_style,
                                     checked ? "[x]" : "[ ]")) {
            aslan_table_set_checkbox(tbl, (int)c, row_id, !checked);
          }
          break;
        }
        default: {
          char buf[256];
          aslan_table_get_cell_display(tbl, (int)c, row_id, buf, sizeof(buf));
          nk_label(ctx, buf, NK_TEXT_LEFT);
          break;
        }
        }
      }

      // Fill a small empty space for the + column in data rows
      nk_layout_row_push(ctx, add_col_w);
      cell_bounds = nk_widget_bounds(ctx);
      nk_fill_rect(canvas, cell_bounds, 0, current_row_bg);
      if (is_row_hovered) {
        nk_stroke_line(canvas, cell_bounds.x, cell_bounds.y + cell_bounds.h - 1,
                       cell_bounds.x + cell_bounds.w,
                       cell_bounds.y + cell_bounds.h - 1, 1.0f, notion_border);
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
    ftr_add_style.hover = nk_style_item_color(nk_rgba(35, 131, 226, 40));
    ftr_add_style.text_normal = muted_gray;
    ftr_add_style.text_hover = nk_rgb(35, 131, 226);

    if (nk_button_label_styled(ctx, &ftr_add_style, "+")) {
      aslan_table_add_row(tbl);
    }

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

      // Style footer buttons as transparent with light text
      struct nk_style_button ftr_btn_style = ctx->style.button;
      ftr_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      ftr_btn_style.hover = nk_style_item_color(nk_rgba(255, 255, 255, 20));
      ftr_btn_style.active = nk_style_item_color(nk_rgba(255, 255, 255, 30));
      ftr_btn_style.border = 0;
      ftr_btn_style.text_normal = footer_text_col;
      ftr_btn_style.text_hover = nk_rgb(255, 255, 255);
      ftr_btn_style.text_active = nk_rgb(255, 255, 255);

      if (footer_text[0]) {
        if (nk_button_label_styled(ctx, &ftr_btn_style, footer_text)) {
          ui->table_state.active_block_id = block->id;
          ui->table_state.active_col_idx = (int)c;
          ui->table_state.show_footer_menu = !ui->table_state.show_footer_menu;
          ui->table_state.show_col_menu = 0;
          ui->table_state.menu_pos_x = ftr_bounds.x;
          ui->table_state.menu_pos_y = ftr_bounds.y;
        }
      } else {
        if (nk_button_label_styled(ctx, &ftr_btn_style, "...")) {
          ui->table_state.active_block_id = block->id;
          ui->table_state.active_col_idx = (int)c;
          ui->table_state.show_footer_menu = !ui->table_state.show_footer_menu;
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
    nk_stroke_rect(nk_window_get_canvas(ctx), ftr_bounds, 0, 0.5f,
                   border_color);

    nk_layout_row_end(ctx);

    // Restore styles AFTER the footer
    ctx->style.edit.normal = old_edit_normal;
    ctx->style.edit.hover = old_edit_hover;
    ctx->style.edit.active = old_edit_active;
    ctx->style.edit.border = 1.0f;
    ctx->style.edit.padding = nk_vec2(4, 4);

    ctx->style.window.padding = old_window_padding;
    ctx->style.window.group_padding = old_group_padding;
    ctx->style.window.spacing = old_window_spacing;
    ctx->style.button.padding = old_button_padding;
    nk_style_pop_vec2(ctx); // Pop text padding

    if (ui->font_inter) {
      nk_style_set_font(ctx, old_table_font);
    }

    // Row count info
    nk_layout_row_dynamic(ctx, 20, 1);
    char info[64];
    snprintf(info, sizeof(info), "%zu rows", view_count);
    nk_label(ctx, info, NK_TEXT_LEFT);
  } else {
    // Empty/uninitialized table
    nk_layout_row_dynamic(ctx, 30, 1);
    nk_label(ctx, "Empty table - click '+ Row' to add data", NK_TEXT_CENTERED);
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
      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Add Column",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         nk_rect(ui->table_state.menu_pos_x - 100,
                                 ui->table_state.menu_pos_y + 4, 160, 200))) {
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

      // Smart Placement (Simplified)
      float menu_h = 250.0f;
      float popup_y = ui->table_state.menu_pos_y - menu_h;
      if (popup_y < 0)
        popup_y = 0;

      struct nk_color menu_bg =
          ui->dark_mode ? nk_rgb(45, 47, 52) : nk_rgb(255, 255, 255);
      nk_style_push_style_item(ctx, &ctx->style.window.fixed_background,
                               nk_style_item_color(menu_bg));

      if (nk_popup_begin(
              ctx, NK_POPUP_STATIC, "Aggregation",
              NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
              nk_rect(ui->table_state.menu_pos_x, popup_y, 180, menu_h))) {

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

        if (nk_button_label_styled(ctx, &menu_btn, "# Count")) {
          aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_COUNT);
          ui->table_state.show_footer_menu = 0;
          nk_popup_close(ctx);
        }

        if (type == ASLAN_COL_NUMBER || type == ASLAN_COL_FORMULA) {
          if (nk_button_label_styled(ctx, &menu_btn, "Σ Sum")) {
            aslan_table_set_footer_agg(tbl, c, ASLAN_AGG_SUM);
            ui->table_state.show_footer_menu = 0;
            nk_popup_close(ctx);
          }
          if (nk_button_label_styled(ctx, &menu_btn, "μ Average")) {
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
          if (nk_button_label_styled(ctx, &menu_btn, "% Percent")) {
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
      float popup_h = 350.0f;
      if (col->type == ASLAN_COL_SELECT) {
        popup_h = 450.0f + (col->option_count * 24); // Extra space for options
        if (popup_h > 600)
          popup_h = 600;
      }

      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Column Settings",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         nk_rect(ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y + 32, 220,
                                 popup_h))) {
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

        // Select Options Management Section
        if (col->type == ASLAN_COL_SELECT) {
          nk_spacing(ctx, 1);
          nk_label(ctx, "--- Select Options ---", NK_TEXT_CENTERED);

          // Show existing options
          for (int i = 0; i < col->option_count && i < 10; i++) {
            char opt_label[80];
            snprintf(opt_label, sizeof(opt_label), "%d: %s", i + 1,
                     col->options[i].label);
            nk_label(ctx, opt_label, NK_TEXT_LEFT);
          }

          if (col->option_count == 0) {
            nk_label(ctx, "(No options yet)", NK_TEXT_LEFT);
          }

          // Add new option
          nk_spacing(ctx, 1);
          nk_label(ctx, "Add Option:", NK_TEXT_LEFT);
          nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
                                         ui->table_state.option_buffer, 63,
                                         nk_filter_default);
          if (nk_button_label(ctx, "+ Add Option")) {
            if (ui->table_state.option_buffer[0] != '\0') {
              aslan_table_add_option(tbl, c, ui->table_state.option_buffer,
                                     0x888888FF);
              ui->table_state.option_buffer[0] = '\0'; // Clear buffer
            }
          }
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
          60.0f + (col->option_count * 28) + 80; // options + add section
      if (popup_h > 400)
        popup_h = 400;
      if (popup_h < 150)
        popup_h = 150;

      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Select Option",
                         NK_WINDOW_BORDER,
                         nk_rect(ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y, 200, popup_h))) {
        nk_layout_row_dynamic(ctx, 24, 1);

        // Show existing options as clickable buttons
        for (int i = 0; i < col->option_count && i < 20; i++) {
          int is_selected = (aslan_table_get_select(tbl, c, row_id) == i);

          struct nk_style_button opt_btn_style = ctx->style.button;
          if (is_selected) {
            opt_btn_style.normal =
                nk_style_item_color(nk_rgb(60, 120, 240)); // Lucid Blue
            opt_btn_style.text_normal = nk_rgb(255, 255, 255);
            opt_btn_style.hover = nk_style_item_color(nk_rgb(80, 140, 255));
            opt_btn_style.text_hover = nk_rgb(255, 255, 255);
          } else {
            opt_btn_style.normal = nk_style_item_color(
                ui->dark_mode ? nk_rgb(40, 42, 48) : nk_rgb(240, 240, 245));
            struct nk_color txt = ui->dark_mode ? COLOR_TEXT : COLOR_TEXT_DARK;
            opt_btn_style.text_normal = txt;
            opt_btn_style.text_hover = txt;
            opt_btn_style.text_active = txt;
          }

          if (is_selected) {
            // Highlight selected option
            char label[80];
            snprintf(label, sizeof(label), "✓ %s", col->options[i].label);
            if (nk_button_label_styled(ctx, &opt_btn_style, label)) {
              // Deselect
              aslan_table_set_select(tbl, c, row_id, -1);
              ui->table_state.show_select_popup = 0;
              nk_popup_close(ctx);
            }
          } else {
            if (nk_button_label_styled(ctx, &opt_btn_style,
                                       col->options[i].label)) {
              aslan_table_set_select(tbl, c, row_id, i);
              ui->table_state.show_select_popup = 0;
              nk_popup_close(ctx);
            }
          }
        }

        if (col->option_count == 0) {
          nk_label(ctx, "(No options yet)", NK_TEXT_CENTERED);
        }

        // Separator
        nk_spacing(ctx, 1);
        nk_label(ctx, "── Add New ──", NK_TEXT_CENTERED);

        // Add new option field
        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD,
                                       ui->table_state.option_buffer, 63,
                                       nk_filter_default);
        if (nk_button_label(ctx, "+ Create Option")) {
          if (ui->table_state.option_buffer[0] != '\0') {
            int new_idx = aslan_table_add_option(
                tbl, c, ui->table_state.option_buffer, 0x888888FF);
            if (new_idx >= 0) {
              aslan_table_set_select(tbl, c, row_id,
                                     new_idx); // Auto-select new option
            }
            ui->table_state.option_buffer[0] = '\0'; // Clear buffer
            ui->table_state.show_select_popup = 0;
            nk_popup_close(ctx);
          }
        }

        nk_popup_end(ctx);
      } else {
        ui->table_state.show_select_popup = 0;
      }
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
