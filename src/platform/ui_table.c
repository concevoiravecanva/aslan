// =============================================================================
// ASLAN UI TABLE - NanoVG-based Table Block Rendering
// =============================================================================

#include "ui_table.h"
#include "../document/aslan_table.h"
#include "ui_shared.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Coordinate helpers
typedef struct { float x, y, w, h; } TblRect;


void ui_render_block_table(UIHost *ui, Engine *engine, Block *block,
                           float width, float height) {
    if (!ui || !engine || !block || block->type != BLOCK_TABLE) return;

    AslanTable *table = block->data.table.columnar;
    if (!table) return;

    NVGcontext *vg = ui->vg;
    float col_w = 120.0f;
    float row_h = 32.0f;
    float header_h = 36.0f;

    float mx = ui->mouse_x - ui->abs_offset_x;
    float my = ui->mouse_y - ui->abs_offset_y;

    NvgWidgetCtx *wc = &ui->nvgw;
    nvgw_set_offset(wc, ui->abs_offset_x, ui->abs_offset_y);

    // Header Background
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, width, header_h);
    nvgFillColor(vg, ui->dark_mode ? nvgRGBA(45, 45, 50, 255) : nvgRGBA(245, 245, 248, 255));
    nvgFill(vg);

    // Header Content
    for (size_t c = 0; c < table->column_count; c++) {
        float cx = (float)c * col_w;
        AslanColumn *col = &table->columns[c];

        // Column border
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx + col_w, 0);
        nvgLineTo(vg, cx + col_w, height);
        nvgStrokeColor(vg, ui->dark_mode ? nvgRGBA(60, 60, 65, 255) : nvgRGBA(220, 220, 225, 255));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        // Icon
        nvgFontSize(vg, 18.0f);
        nvgFontFace(vg, "Icons");
        nvgFillColor(vg, ui->dark_mode ? nvgRGBA(160, 160, 170, 255) : nvgRGBA(110, 110, 120, 255));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, cx + 10, header_h / 2, table_col_type_icon(col->type), NULL);

        // Label
        nvgFontSize(vg, 14.0f);
        nvgFontFace(vg, "Inter");
        nvgFillColor(vg, ui->dark_mode ? nvgRGBA(220, 220, 230, 255) : nvgRGBA(40, 40, 45, 255));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, cx + 32, header_h / 2, col->name, NULL);

        // Right click for column menu
        if ((ui->mouse_buttons & 2) && mx >= cx && mx < cx + col_w && my >= 0 && my < header_h) {
            ui->show_context_menu = 1;
            ui->context_menu_x = ui->mouse_x;
            ui->context_menu_y = ui->mouse_y;
            ui->table_state.context_menu_mode = 1; // Column
            ui->table_state.context_menu_col = (int)c;
            ui->table_state.active_block_id = block->id;
            ui_request_redraw(ui);
        }
    }

    // --- Column Addition (+) button ---
    float add_col_x = (float)table->column_count * col_w;
    uint32_t add_col_id = nvgw_id(&block->id, sizeof(block->id)) ^ nvgw_id_str("add_col");
    if (nvgw_button_icon(wc, add_col_id, add_col_x + 5, 4, 32.0f, header_h - 8, ICON_ADD)) {
        ui->table_state.show_type_menu = 1;
        ui->table_state.type_menu_x = ui->mouse_x;
        ui->table_state.type_menu_y = ui->mouse_y;
        ui->table_state.active_block_id = block->id;
        ui_request_redraw(ui);
    }

    // Horizontal line after header
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, header_h);
    nvgLineTo(vg, width, header_h);
    nvgStrokeColor(vg, ui->dark_mode ? nvgRGBA(80, 80, 85, 255) : nvgRGBA(200, 200, 205, 255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Rows + Ghost Row
    size_t count = aslan_table_view_count(table);
    for (size_t i = 0; i <= count; i++) {
        float ry = header_h + (float)i * row_h;
        if (ry > height) break;

        int is_ghost = (i == count);
        uint64_t row_id = is_ghost ? 0 : aslan_table_view_row_id(table, (int)i);

        // Striping
        if (i % 2 == 1 && !is_ghost) {
            nvgBeginPath(vg);
            nvgRect(vg, 0, ry, width, row_h);
            nvgFillColor(vg, ui->dark_mode ? nvgRGBA(255, 255, 255, 5) : nvgRGBA(0, 0, 0, 5));
            nvgFill(vg);
        }

        // Ghost row hint
        if (is_ghost) {
            nvgFontSize(vg, 13.0f);
            nvgFontFace(vg, "Inter");
            nvgFillColor(vg, ui->dark_mode ? nvgRGBA(120, 120, 130, 150) : nvgRGBA(160, 160, 170, 150));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, 10, ry + row_h / 2, "+ New Row...", NULL);
        }

        // Right click for row menu
        if (!is_ghost && (ui->mouse_buttons & 2) && mx >= 0 && mx < width && my >= ry && my < ry + row_h) {
            ui->show_context_menu = 1;
            ui->context_menu_x = ui->mouse_x;
            ui->context_menu_y = ui->mouse_y;
            ui->table_state.context_menu_mode = 2; // Row
            ui->table_state.context_menu_row = row_id;
            ui->table_state.active_block_id = block->id;
            ui_request_redraw(ui);
        }

        for (size_t c = 0; c < table->column_count; c++) {
            float cx = (float)c * col_w;
            AslanColumn *col = &table->columns[c];
            uint32_t cell_id = nvgw_id(&row_id, sizeof(row_id)) ^ nvgw_id(&c, sizeof(c)) ^ nvgw_id(&block->id, sizeof(block->id));

            // Specialized interaction for CHECKBOX (direct toggle, no focus state needed)
            if (!is_ghost && col->type == ASLAN_COL_CHECKBOX) {
                bool checked = aslan_table_get_checkbox(table, (int)c, row_id);
                if (nvgw_checkbox(wc, cell_id, cx+ col_w/2 - 7, ry + row_h/2 - 7, 14, 14, NULL, &checked)) {
                    aslan_table_set_checkbox(table, (int)c, row_id, checked);
                    ui_request_redraw(ui);
                }
                continue;
            }

            // Logic: Click to edit (also works for Ghost Row)
            if (ui->nvgw.mouse_clicked && mx >= cx && mx < cx + col_w && my >= ry && my < ry + row_h) {
                if (is_ghost) {
                   row_id = aslan_table_add_row(table);
                   count++; // update count so next frame doesn't draw double ghost
                }
                ui->table_state.active_block_id = block->id;
                ui->table_state.active_col_idx = (int)c;
                ui->table_state.select_popup_row = row_id;
                ui->table_state.is_editing_cell = 1;
                
                // Copy current value to buffer
                char current[1024] = "";
                if (!is_ghost) aslan_table_get_cell_display(table, (int)c, row_id, current, sizeof(current));
                strncpy(ui->table_state.rename_buffer, current, sizeof(ui->table_state.rename_buffer)-1);
                ui_request_redraw(ui);
            }

            if (is_ghost) continue;

            // Inline Editor
            if (ui->table_state.is_editing_cell && ui->table_state.active_col_idx == (int)c && ui->table_state.select_popup_row == row_id && ui->table_state.active_block_id == block->id) {
                if (col->type == ASLAN_COL_SELECT) {
                    // Dropdown UI
                    const char *opts_labels[ASLAN_MAX_SELECT_OPTIONS];
                    for(int o=0; o<col->option_count; o++) opts_labels[o] = col->options[o].label;
                    int sel = aslan_table_get_select(table, (int)c, row_id);
                    if (nvgw_dropdown_select(wc, cell_id, cx, ry, col_w, row_h, opts_labels, col->option_count, &sel)) {
                        aslan_table_set_select(table, (int)c, row_id, sel);
                        ui->table_state.is_editing_cell = 0;
                        ui_request_redraw(ui);
                    }
                } else if (col->type == ASLAN_COL_DATE) {
                    // Date Picker Placeholder
                    int y=2024, m=0, d=1; // hardcoded for demo, would pull from value
                    if (nvgw_date_picker(wc, cell_id, cx, ry, col_w, row_h, &y, &m, &d)) {
                         // set date logic...
                         ui->table_state.is_editing_cell = 0;
                         ui_request_redraw(ui);
                    }
                } else {
                    // Standard Text Input
                    if (nvgw_text_input(wc, cell_id, cx, ry, col_w, row_h, ui->table_state.rename_buffer, sizeof(ui->table_state.rename_buffer), 14.0f)) {
                        aslan_table_set_cell(table, (int)c, row_id, ui->table_state.rename_buffer);
                        ui->table_state.is_editing_cell = 0;
                        ui_request_redraw(ui);
                    }
                }
            } else {
                char buf[1024];
                aslan_table_get_cell_display(table, (int)c, row_id, buf, sizeof(buf));

                nvgFontSize(vg, 14.0f);
                nvgFontFace(vg, "Inter");
                nvgFillColor(vg, ui->dark_mode ? nvgRGBA(200, 200, 210, 255) : nvgRGBA(60, 60, 65, 255));
                nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                nvgText(vg, cx + 10, ry + row_h / 2, buf, NULL);
            }
        }

        // Horizontal line
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, ry + row_h);
        nvgLineTo(vg, width, ry + row_h);
        nvgStrokeColor(vg, ui->dark_mode ? nvgRGBA(60, 60, 65, 255) : nvgRGBA(230, 230, 235, 255));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);
    }

    // --- Context Menu Rendering ---
    if (ui->show_context_menu && ui->table_state.active_block_id == block->id && !ui->context_menu_node) {
        float menu_w = 150.0f;
        float item_hh = 28.0f;
        int is_col = ui->table_state.context_menu_mode == 1;
        int item_cnt = 2; 
        float menu_h = item_cnt * item_hh + 12.0f;
        float menu_x = ui->context_menu_x - ui->abs_offset_x;
        float menu_y = ui->context_menu_y - ui->abs_offset_y;

        nvgw_panel(wc, menu_x, menu_y, menu_w, menu_h, ui->dark_mode ? nvgRGBA(50, 50, 55, 255) : nvgRGBA(255, 255, 255, 255), 8.0f);

        const char *col_items[] = {"Rename Column", "Delete Column"};
        const char *row_items[] = {"Duplicate Row", "Delete Row"};
        const char **items = is_col ? col_items : row_items;

        for (int m = 0; m < item_cnt; m++) {
            float ix = menu_x + 4, iy = menu_y + 6 + m * item_hh;
            float iw = menu_w - 8;
            uint32_t mid = nvgw_id(&m, sizeof(m)) ^ nvgw_id_str("tbl_ctx");
            if (nvgw_button(wc, mid, ix, iy, iw, item_hh, items[m])) {
                 if (is_col) {
                    if (m == 1) aslan_table_remove_column(table, ui->table_state.context_menu_col);
                } else {
                    if (m == 0) aslan_table_duplicate_row(table, ui->table_state.context_menu_row);
                    else if (m == 1) aslan_table_delete_row(table, ui->table_state.context_menu_row);
                }
                ui->show_context_menu = 0;
                ui_request_redraw(ui);
            }
        }
    }

    // --- Column Type Selection Menu ---
    if (ui->table_state.show_type_menu && ui->table_state.active_block_id == block->id) {
        float menu_w = 160.0f;
        float item_hh = 28.0f;
        int item_cnt = 5; // Text, Number, Select, Date, Checkbox
        float menu_h = item_cnt * item_hh + 12.0f;
        float menu_x = ui->table_state.type_menu_x - ui->abs_offset_x;
        float menu_y = ui->table_state.type_menu_y - ui->abs_offset_y;

        nvgw_panel(wc, menu_x, menu_y, menu_w, menu_h, ui->dark_mode ? nvgRGBA(50, 50, 55, 255) : nvgRGBA(255, 255, 255, 255), 8.0f);

        const char *type_names[] = {"Text", "Number", "Select", "Date", "Checkbox"};
        AslanColumnType types[] = {ASLAN_COL_TEXT, ASLAN_COL_NUMBER, ASLAN_COL_SELECT, ASLAN_COL_DATE, ASLAN_COL_CHECKBOX};

        for (int m = 0; m < item_cnt; m++) {
            float ix = menu_x + 4, iy = menu_y + 6 + m * item_hh;
            float iw = menu_w - 8;
            uint32_t mid = nvgw_id(&m, sizeof(m)) ^ nvgw_id_str("type_menu");
            if (nvgw_button(wc, mid, ix, iy, iw, item_hh, type_names[m])) {
                aslan_table_add_column(table, type_names[m], types[m]);
                if (types[m] == ASLAN_COL_SELECT) {
                   // Add some default options for demo
                   int cidx = (int)table->column_count - 1;
                   aslan_table_add_option(table, cidx, "Option 1", 0x3498dbff);
                   aslan_table_add_option(table, cidx, "Option 2", 0x2ecc71ff);
                }
                ui->table_state.show_type_menu = 0;
                ui_request_redraw(ui);
            }
        }
        
        if (ui->nvgw.mouse_clicked) {
            bool in_menu = (mx >= menu_x && mx < menu_x + menu_w && my >= menu_y && my < menu_y + menu_h);
            if (!in_menu) ui->table_state.show_type_menu = 0;
        }
    }
}

const char *table_col_type_icon(AslanColumnType type) {
  switch (type) {
  case ASLAN_COL_TEXT: return ICON_COL_TEXT;
  case ASLAN_COL_NUMBER: return ICON_COL_NUMBER;
  case ASLAN_COL_CHECKBOX: return ICON_COL_CHECKBOX;
  case ASLAN_COL_SELECT: return ICON_COL_SELECT;
  case ASLAN_COL_DATE: return ICON_COL_DATE;
  case ASLAN_COL_FORMULA: return ICON_COL_FORMULA;
  case ASLAN_COL_FILE: return ICON_COL_FILE;
  case ASLAN_COL_IMAGE: return ICON_COL_IMAGE;
  case ASLAN_COL_LINK: return ICON_COL_LINK;
  case ASLAN_COL_DURATION: return "\xEF\x8C\xB8"; 
  default: return ICON_COL_TEXT;
  }
}

const char *table_col_type_name(AslanColumnType type) {
  switch (type) {
  case ASLAN_COL_TEXT: return "Text";
  case ASLAN_COL_NUMBER: return "Number";
  case ASLAN_COL_CHECKBOX: return "Checkbox";
  case ASLAN_COL_SELECT: return "Select";
  case ASLAN_COL_DATE: return "Date";
  case ASLAN_COL_FORMULA: return "Formula";
  case ASLAN_COL_FILE: return "File";
  case ASLAN_COL_IMAGE: return "Image";
  case ASLAN_COL_LINK: return "Link";
  case ASLAN_COL_DURATION: return "Duration";
  default: return "Text";
  }
}
