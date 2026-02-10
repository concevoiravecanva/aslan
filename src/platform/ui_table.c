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

// Helper: Ensure popup stays inside viewport
static struct nk_rect table_smart_popup_rect(UIHost *ui, float x, float y,
                                             float w, float h) {
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
  const float max_col_w = 500.0f;
  const float row_num_w = 44.0f;

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
    nk_layout_row_dynamic(ctx, table_height + 20, 1);
    if (nk_group_begin(ctx, "table_scroll_group", NK_WINDOW_SCROLL_AUTO_HIDE)) {
      nk_style_push_vec2(ctx, &ctx->style.text.padding, nk_vec2(4, 2));

      // Re-calculate total width for static rows (consistent with group width)
      float static_total_w = total_w;

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
      nk_label(ctx, "#", NK_TEXT_CENTERED);

      // Column headers
      for (size_t c = 0; c < col_count; c++) {
        nk_layout_row_push(ctx, col_widths[c]);
        hdr_bounds = nk_widget_bounds(ctx);
        nk_fill_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, header_bg);
        nk_stroke_rect(nk_window_get_canvas(ctx), hdr_bounds, 0, 1,
                       border_color);

        // --- Interaction Detection ---
        int is_hovering_cell =
            nk_input_is_mouse_hovering_rect(&ctx->input, hdr_bounds);
        struct nk_rect resize_zone = nk_rect(hdr_bounds.x + hdr_bounds.w - 5,
                                             hdr_bounds.y, 10, hdr_bounds.h);
        int is_hovering_resize =
            nk_input_is_mouse_hovering_rect(&ctx->input, resize_zone);

        // A. Column Resize Initiation
        if (is_hovering_resize &&
            nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
            ui->table_state.resizing_col < 0) {
          ui->table_state.active_block_id = block->id;
          ui->table_state.resizing_col = (int)c;
          ui->table_state.resize_start_x = ctx->input.mouse.pos.x;
          ui->table_state.resize_start_w = col_widths[c];
          ui_request_redraw(ui);
        }

        // B. Sort Button (ONLY if not hovering resize)
        if (!is_hovering_resize && ui->table_state.resizing_col < 0) {
          struct nk_style_button hdr_btn_style = ctx->style.button;
          hdr_btn_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
          hdr_btn_style.hover = nk_style_item_color(nk_rgba(255, 255, 255, 20));
          hdr_btn_style.active =
              nk_style_item_color(nk_rgba(255, 255, 255, 30));
          hdr_btn_style.border = 0;
          hdr_btn_style.text_normal = header_text_col;
          hdr_btn_style.text_hover = header_text_col;
          hdr_btn_style.text_active = header_text_col;

          if (nk_button_label_styled(ctx, &hdr_btn_style, "")) {
            if (tbl->sort_column == (int)c) {
              if (tbl->sort_ascending)
                aslan_table_sort(tbl, (int)c, false);
              else
                aslan_table_sort_clear(tbl);
            } else {
              aslan_table_sort(tbl, (int)c, true);
            }
          }
        }

        // C. Settings Popup (Right-click)
        if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT,
                                            hdr_bounds)) {
          ui->table_state.active_block_id = block->id;
          ui->table_state.active_col_idx = (int)c;
          ui->table_state.show_col_settings =
              !ui->table_state.show_col_settings;
          ui->table_state.menu_pos_x = hdr_bounds.x;
          ui->table_state.menu_pos_y = hdr_bounds.y;
          strncpy(ui->table_state.rename_buffer, tbl->columns[c].name, 63);
          ui->table_state.rename_buffer[63] = '\0';
        }

        // --- Drawing ---
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        const char *icon = table_col_type_icon(tbl->columns[c].type);

        // Highlight cell if hovering (manual highlight for resize zone)
        if (is_hovering_cell && ui->table_state.resizing_col < 0) {
          nk_fill_rect(canvas, hdr_bounds, 0, nk_rgba(255, 255, 255, 20));
        }

        // Icon & Text
        if (ui->font_icons) {
          nk_style_set_font(ctx, &ui->font_icons->handle);
          nk_draw_text(canvas,
                       nk_rect(hdr_bounds.x + 6, hdr_bounds.y + 6, 20, 20),
                       icon, (int)strlen(icon), &ui->font_icons->handle,
                       nk_rgba(0, 0, 0, 0), header_icon_col);
          nk_style_set_font(ctx, &ui->font_inter->handle);
        }
        nk_draw_text(
            canvas,
            nk_rect(hdr_bounds.x + 32, hdr_bounds.y + 6, hdr_bounds.w - 36, 20),
            tbl->columns[c].name, (int)strlen(tbl->columns[c].name),
            &ui->font_inter->handle, nk_rgba(0, 0, 0, 0), header_text_col);

        // Sort indicator
        if (tbl->sort_column == (int)c) {
          const char *s_icon =
              tbl->sort_ascending ? ICON_SORT_ASC : ICON_SORT_DESC;
          if (ui->font_icons) {
            nk_style_set_font(ctx, &ui->font_icons->handle);
            nk_draw_text(canvas,
                         nk_rect(hdr_bounds.x + hdr_bounds.w - 20,
                                 hdr_bounds.y + 6, 16, 20),
                         s_icon, (int)strlen(s_icon), &ui->font_icons->handle,
                         nk_rgba(0, 0, 0, 0), COLOR_ACCENT);
            nk_style_set_font(ctx, &ui->font_inter->handle);
          }
        }

        // Divider & Resize Handle highlight
        nk_stroke_line(
            canvas, hdr_bounds.x + hdr_bounds.w - 1, hdr_bounds.y + 8,
            hdr_bounds.x + hdr_bounds.w - 1, hdr_bounds.y + hdr_bounds.h - 8,
            1.0f, nk_rgba(border_color.r, border_color.g, border_color.b, 100));

        if (is_hovering_resize ||
            (ui->table_state.resizing_col == (int)c &&
             ui->table_state.active_block_id == block->id)) {
          nk_stroke_line(canvas, hdr_bounds.x + hdr_bounds.w - 1, hdr_bounds.y,
                         hdr_bounds.x + hdr_bounds.w - 1,
                         hdr_bounds.y + hdr_bounds.h, 2.0f, COLOR_ACCENT);
          ui_set_cursor(ui, UI_CURSOR_SIZEWE);
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
      struct nk_color vert_sep =
          ui->dark_mode ? nk_rgba(60, 62, 66, 40)
                        : nk_rgba(237, 236, 233, 180); // Subtle guide
      struct nk_color row_hover_bg =
          ui->dark_mode ? nk_rgba(255, 255, 255, 20)
                        : nk_rgba(55, 53, 47, 10); // Slightly more subtle hover
      struct nk_color active_cell_outline = nk_rgb(35, 131, 226); // Notion blue

      for (size_t v = 0; v < (size_t)max_rows; v++) {
        uint64_t row_id = aslan_table_view_row_id(tbl, (int)v);
        if (row_id == 0)
          continue;

        // Data Row Layout using nk_layout_row_begin/push/end (consistent API)
        nk_layout_row_begin(ctx, NK_STATIC, row_h, (int)col_count + 2);

        // Calculate row hover state
        struct nk_rect row_bounds = nk_layout_widget_bounds(ctx);
        int is_row_hovered =
            nk_input_is_mouse_hovering_rect(&ctx->input, row_bounds);
        struct nk_color odd_row_bg =
            ui->dark_mode ? nk_rgb(29, 30, 35) : nk_rgb(245, 245, 243);
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
                       cell_bounds.y + cell_bounds.h - 1, 1.0f, notion_border);

        // Left accent stripe on hover (Notion-style)
        if (is_row_hovered) {
          nk_fill_rect(
              canvas,
              nk_rect(cell_bounds.x, cell_bounds.y + 2, 3, cell_bounds.h - 4),
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

          // Background
          nk_fill_rect(canvas, cell_bounds, 0, current_row_bg);

          // Borders (bottom only - subtle horizontal line)
          nk_stroke_line(
              canvas, cell_bounds.x, cell_bounds.y + cell_bounds.h - 1,
              cell_bounds.x + cell_bounds.w, cell_bounds.y + cell_bounds.h - 1,
              1.0f, notion_border);

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
                         sizeof(ui->table_state.date_buffer), "%Y-%m-%d",
                         tm_ptr);
              }

              // Style for active edit - make it look like an active field
              struct nk_style_item old_edit_normal_date =
                  ctx->style.edit.normal;
              struct nk_color old_edit_text_date = ctx->style.edit.text_normal;

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

              ctx->style.edit.normal = old_edit_normal_date;
              ctx->style.edit.text_normal = old_edit_text_date;

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
              date_btn_style.hover =
                  nk_style_item_color(nk_rgba(80, 80, 80, 40));
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
              ui->table_state.active_col_idx = (int)c;
              ui->table_state.active_row_idx = (int)v;
              ui->table_state.show_select_popup = 1;
              ui->table_state.select_popup_row = row_id;
              ui->table_state.menu_pos_x = btn_bounds.x;
              ui->table_state.menu_pos_y = btn_bounds.y + btn_bounds.h;
              ui->table_state.show_col_menu = 0;
              ui->table_state.show_footer_menu = 0;
              ui->table_state.show_col_settings = 0;
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
            chk_style.hover = nk_style_item_color(nk_rgba(80, 80, 80, 20));
            chk_style.active = nk_style_item_color(nk_rgba(60, 60, 60, 40));
            chk_style.padding = nk_vec2(0, 0);
            chk_style.border = 0;

            // Align the checkbox in the center
            if (nk_button_label_styled(ctx, &chk_style, "")) {
              aslan_table_set_checkbox(tbl, (int)c, row_id, !checked);
            }

            // Draw icon manually for better control
            const char *chk_icon =
                checked ? ICON_CHECK_FILL : ICON_COL_CHECKBOX;
            struct nk_color chk_col =
                checked ? active_cell_outline
                        : (ui->dark_mode ? muted_gray : nk_rgb(180, 180, 185));

            if (ui->font_icons) {
              nk_draw_text(nk_window_get_canvas(ctx),
                           nk_rect(cell_bounds.x + (cell_bounds.w - 18) / 2,
                                   cell_bounds.y + (cell_bounds.h - 18) / 2, 18,
                                   18),
                           chk_icon, strlen(chk_icon), &ui->font_icons->handle,
                           nk_rgba(0, 0, 0, 0), chk_col);
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
      nk_stroke_rect(nk_window_get_canvas(ctx), ftr_bounds, 0, 0.5f,
                     border_color);

      nk_layout_row_end(ctx);
    }

    nk_group_end(ctx);

    // Restore Global Styles
    nk_style_pop_vec2(ctx); // Pops ctx->style.text.padding
    ctx->style.edit.normal = old_edit_normal;
    ctx->style.edit.hover = old_edit_hover;
    ctx->style.edit.active = old_edit_active;
    ctx->style.edit.border = 0.0f; // Ensure border is reset if it was changed
    ctx->style.edit.padding = nk_vec2(6, 4); // Default edit padding

    ctx->style.window.padding = old_window_padding;
    ctx->style.window.group_padding = old_group_padding;
    ctx->style.window.spacing = old_window_spacing;
    ctx->style.button.padding = old_button_padding;

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
    cta_style.normal = nk_style_item_color(nk_rgba(35, 131, 226, 30));
    cta_style.hover = nk_style_item_color(nk_rgba(35, 131, 226, 60));
    cta_style.active = nk_style_item_color(nk_rgba(35, 131, 226, 90));
    cta_style.text_normal = nk_rgb(35, 131, 226);
    cta_style.text_hover = nk_rgb(55, 151, 246);
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
                                 ui->table_state.menu_pos_y + 4, 160, 200);
      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Add Column",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         popup_rect)) {
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

      // Smart Placement
      float menu_h = 250.0f;
      struct nk_rect popup_rect =
          table_smart_popup_rect(ui, ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y + 24, 180, menu_h);
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
      float popup_h = 350.0f;
      if (col->type == ASLAN_COL_SELECT) {
        popup_h = 450.0f + (col->option_count * 24); // Extra space for options
        if (popup_h > 600)
          popup_h = 600;
      }

      // Smart placement
      struct nk_rect popup_rect =
          table_smart_popup_rect(ui, ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y + 32, 220, popup_h);
      if (ui->table_state.menu_pos_y > ui->win_height - popup_h - 40) {
        popup_rect.y = ui->table_state.menu_pos_y - popup_h;
      }

      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Column Settings",
                         NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER,
                         popup_rect)) {
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
          for (int i = 0; i < col->option_count && i < ASLAN_MAX_SELECT_OPTIONS;
               i++) {
            nk_layout_row_template_begin(ctx, 24.0f);
            nk_layout_row_template_push_static(ctx, 20.0f); // Color
            nk_layout_row_template_push_dynamic(ctx);       // Name
            nk_layout_row_template_push_static(ctx, 20.0f); // Delete
            nk_layout_row_template_end(ctx);

            // Color box (cycle colors on click)
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

            if (nk_button_label(ctx, "x")) {
              aslan_table_remove_option(tbl, c, i);
            }
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

          uint32_t presets[] = {
              0x2383E2FF, // Blue
              0xE22353FF, // Red
              0x23E253FF, // Green
              0xE2B823FF, // Yellow
              0x8E23E2FF, // Purple
              0xE26A23FF  // Orange
          };
          uint32_t next_color = presets[col->option_count % 6];

          if (nk_button_label(ctx, "+ Add Option")) {
            if (ui->table_state.option_buffer[0] != '\0') {
              aslan_table_add_option(tbl, c, ui->table_state.option_buffer,
                                     next_color);
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

      struct nk_rect popup_rect =
          table_smart_popup_rect(ui, ui->table_state.menu_pos_x,
                                 ui->table_state.menu_pos_y, 200, popup_h);
      if (ui->table_state.menu_pos_y > ui->win_height - popup_h - 40) {
        popup_rect.y = ui->table_state.menu_pos_y - popup_h;
      }

      if (nk_popup_begin(ctx, NK_POPUP_STATIC, "Select Option",
                         NK_WINDOW_BORDER, popup_rect)) {
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
