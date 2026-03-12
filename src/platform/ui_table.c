// =============================================================================
// ASLAN UI TABLE - NanoVG-based Table Block Rendering
// =============================================================================

#include "ui_table.h"
#include "../document/aslan_table.h"
#include "ui_shared.h"
#include "../core/engine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Coordinate helpers
#include <SDL.h>
#include <time.h>
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

        // When formula popup is active, highlight headers to show they're clickable
        if (ui->table_state.show_formula_popup) {
            bool hdr_hovered = (mx >= cx && mx < cx + col_w && my >= 0 && my < header_h);
            nvgBeginPath(vg);
            nvgRect(vg, cx + 1, 1, col_w - 2, header_h - 2);
            nvgFillColor(vg, hdr_hovered ? nvgRGBA(80, 140, 255, 60) : nvgRGBA(80, 140, 255, 20));
            nvgFill(vg);
            if (hdr_hovered) {
                nvgBeginPath(vg);
                nvgRect(vg, cx + 1, 1, col_w - 2, header_h - 2);
                nvgStrokeColor(vg, nvgRGBA(80, 140, 255, 200));
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);
            }
        }

        // Left click for formula reference
        if (ui->table_state.show_formula_popup && ui->nvgw.mouse_clicked && mx >= cx && mx < cx + col_w && my >= 0 && my < header_h) {
            char ref[128];
            snprintf(ref, sizeof(ref), "[%s]", col->name);
            // Append to buffer if there's space
            size_t current_len = strlen(ui->table_state.formula_buffer);
            size_t ref_len = strlen(ref);
            if (current_len + ref_len < sizeof(ui->table_state.formula_buffer) - 1) {
                strcat(ui->table_state.formula_buffer, ref);
            }
            ui_request_redraw(ui);
        }

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

    if (nvgw_button(wc, add_col_id, add_col_x + 5, 4, 60.0f, header_h - 8, "+ Add")) {
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

    // Prevent cell clicks from bleeding through open popups
    bool mouse_in_popup = false;
    if (ui->table_state.show_date_picker && ui->table_state.active_block_id == block->id) {
        float px = ui->table_state.date_picker_x - ui->abs_offset_x;
        float py = ui->table_state.date_picker_y - ui->abs_offset_y;
        if (mx >= px && mx < px + 200.0f && my >= py && my < py + 240.0f) mouse_in_popup = true;
    } else if (ui->table_state.show_select_popup && ui->table_state.active_block_id == block->id) {
        AslanColumn *scol = &table->columns[ui->table_state.select_popup_col];
        int item_cnt = scol->option_count;
        float ph = (item_cnt > 0 ? item_cnt : 1) * 28.0f + 12.0f + 40.0f;
        float px = ui->table_state.select_popup_x - ui->abs_offset_x;
        float py = ui->table_state.select_popup_y - ui->abs_offset_y;
        if (mx >= px && mx < px + 160.0f && my >= py && my < py + ph) mouse_in_popup = true;
    } else if (ui->table_state.show_type_menu && ui->table_state.active_block_id == block->id) {
        float px = ui->table_state.type_menu_x - ui->abs_offset_x;
        float py = ui->table_state.type_menu_y - ui->abs_offset_y;
        if (mx >= px && mx < px + 160.0f && my >= py && my < py + 152.0f) mouse_in_popup = true;
    } else if (ui->table_state.show_formula_popup && ui->table_state.active_block_id == block->id) {
        float fw = 400.0f, fh = 120.0f; // Slightly taller
        float fpx = (ui->win_width - fw) * 0.5f;
        float fpy = (ui->win_height - fh) * 0.5f;
        // Use absolute coordinates for window-centered popup
        if (ui->mouse_x >= fpx && ui->mouse_x < fpx + fw && ui->mouse_y >= fpy && ui->mouse_y < fpy + fh) mouse_in_popup = true;
    }

    bool saved_clicked = ui->nvgw.mouse_clicked;
    if (mouse_in_popup) {
        ui->nvgw.mouse_clicked = false;
    }

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
                    aslan_table_eval_formulas(table);
                    ui_request_redraw(ui);
                }
                continue;
            }

            // Logic: Click to edit (also works for Ghost Row)
            if (ui->nvgw.mouse_clicked && mx >= cx && mx < cx + col_w && my >= ry && my < ry + row_h) {
                bool already_editing = (ui->table_state.is_editing_cell && ui->table_state.active_col_idx == (int)c && ui->table_state.select_popup_row == row_id);
                if (!already_editing) {
                    // Commit previous edit if there is one
                    if (ui->table_state.is_editing_cell) {
                        aslan_table_set_cell(table, ui->table_state.active_col_idx, ui->table_state.select_popup_row, ui->table_state.rename_buffer);
                    }
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

                    // Force text input to focus immediately
                    uint32_t real_cell_id = nvgw_id(&row_id, sizeof(row_id)) ^ nvgw_id(&c, sizeof(c)) ^ nvgw_id(&block->id, sizeof(block->id));
                    ui->nvgw.focus_id = real_cell_id;

                    ui_request_redraw(ui);
                }
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
                        ui->table_state.show_select_popup = 1;
                        ui->table_state.select_popup_x = ui->mouse_x;
                        ui->table_state.select_popup_y = ui->mouse_y;
                        ui->table_state.select_popup_col = (int)c;
                        ui->table_state.select_popup_row = row_id;
                        ui->table_state.editing_select_option = -1;
                        ui->table_state.active_block_id = block->id;
                        ui_request_redraw(ui);
                    }
                } else if (col->type == ASLAN_COL_DATE) {
                    // Date Picker Placeholder
                    int y=2024, m=0, d=1; // hardcoded for demo, would pull from value
                    if (nvgw_date_picker(wc, cell_id, cx, ry, col_w, row_h, &y, &m, &d)) {
                         ui->table_state.show_date_picker = 1;
                         ui->table_state.date_picker_x = ui->mouse_x;
                         ui->table_state.date_picker_y = ui->mouse_y;
                         ui->table_state.date_picker_col = (int)c;
                         ui->table_state.date_picker_row = row_id;
                         ui->table_state.active_block_id = block->id;
                         ui_request_redraw(ui);
                    }
                } else if (col->type == ASLAN_COL_FORMULA) {
                    // Formula cells: open formula editor popup
                    ui->table_state.show_formula_popup = 1;
                    ui->table_state.formula_popup_col = (int)c;
                    const char *f = aslan_table_get_formula(table, (int)c);
                    strncpy(ui->table_state.formula_buffer, f ? f : "", sizeof(ui->table_state.formula_buffer)-1);
                    ui->table_state.is_editing_cell = 0; // Not a text edit
                    ui->nvgw.mouse_clicked = false; // Consume click to prevent auto-close same frame
                    ui_request_redraw(ui);
                } else {
                    // Standard Text Input - Filter for numeric columns
                    if (col->type == ASLAN_COL_NUMBER || col->type == ASLAN_COL_DURATION) {
                        char filtered[32] = "";
                        size_t fi = 0;
                        for(size_t i=0; i<strlen(ui->nvgw.text_input) && fi < 31; i++) {
                            char ch = ui->nvgw.text_input[i];
                            if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == ',' || ch == '+') {
                                filtered[fi++] = ch;
                            }
                        }
                        filtered[fi] = '\0';
                        strncpy(ui->nvgw.text_input, filtered, sizeof(ui->nvgw.text_input)-1);
                    }

                    if (nvgw_text_input(wc, cell_id, cx, ry, col_w, row_h, ui->table_state.rename_buffer, sizeof(ui->table_state.rename_buffer), 14.0f)) {
                        aslan_table_set_cell(table, (int)c, row_id, ui->table_state.rename_buffer);
                        aslan_table_eval_formulas(table);
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

    // Restore click state for popups to process
    ui->nvgw.mouse_clicked = saved_clicked;

    // --- Context Menu Rendering ---
    if (ui->show_context_menu && ui->table_state.active_block_id == block->id && !ui->context_menu_node) {
        float menu_w = 150.0f;
        float item_hh = 28.0f;
        int is_col = ui->table_state.context_menu_mode == 1;
        int item_cnt = 2; 
        float menu_x = ui->context_menu_x - ui->abs_offset_x;
        float menu_y = ui->context_menu_y - ui->abs_offset_y;


        const char *col_items[3] = {"Rename Column", "Delete Column", "Edit Formula"};
        const char *row_items[2] = {"Duplicate Row", "Delete Row"};
        const char **items = is_col ? col_items : row_items;
        
        int actual_item_cnt = item_cnt;
        if (is_col) {
            AslanColumn *cobj = &table->columns[ui->table_state.context_menu_col];
            if (cobj->type == ASLAN_COL_FORMULA) actual_item_cnt = 3;
        }
        
        float menu_h = actual_item_cnt * item_hh + 12.0f;
        nvgw_panel(wc, menu_x, menu_y, menu_w, menu_h, ui->dark_mode ? nvgRGBA(50, 50, 55, 255) : nvgRGBA(255, 255, 255, 255), 8.0f);

        for (int m = 0; m < actual_item_cnt; m++) {
            float ix = menu_x + 4, iy = menu_y + 6 + m * item_hh;
            float iw = menu_w - 8;
            uint32_t mid = nvgw_id(&m, sizeof(m)) ^ nvgw_id_str("tbl_ctx");
            if (nvgw_button(wc, mid, ix, iy, iw, item_hh, items[m])) {
                 if (is_col) {
                    if (m == 0) {
                        ui->table_state.renaming_col_idx = ui->table_state.context_menu_col;
                        strncpy(ui->table_state.rename_buffer, table->columns[ui->table_state.context_menu_col].name, sizeof(ui->table_state.rename_buffer)-1);
                    } else if (m == 1) {
                        aslan_table_remove_column(table, ui->table_state.context_menu_col);
                    } else if (m == 2) {
                        ui->table_state.show_formula_popup = 1;
                        ui->table_state.formula_popup_col = ui->table_state.context_menu_col;
                        const char *f = aslan_table_get_formula(table, ui->table_state.formula_popup_col);
                        strncpy(ui->table_state.formula_buffer, f ? f : "", sizeof(ui->table_state.formula_buffer)-1);
                    }
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
        int item_cnt = 10; 
        float menu_h = item_cnt * item_hh + 12.0f;
        float menu_x = ui->table_state.type_menu_x - ui->abs_offset_x;
        float menu_y = ui->table_state.type_menu_y - ui->abs_offset_y;

        nvgw_panel(wc, menu_x, menu_y, menu_w, menu_h, ui->dark_mode ? nvgRGBA(50, 50, 55, 255) : nvgRGBA(255, 255, 255, 255), 8.0f);

        const char *type_names[] = {"Text", "Number", "Check", "Select", "Date", "Formula", "File", "Link", "Image", "Duration"};
        AslanColumnType types[] = {ASLAN_COL_TEXT, ASLAN_COL_NUMBER, ASLAN_COL_CHECKBOX, ASLAN_COL_SELECT, ASLAN_COL_DATE, ASLAN_COL_FORMULA, ASLAN_COL_FILE, ASLAN_COL_LINK, ASLAN_COL_IMAGE, ASLAN_COL_DURATION};

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
            bool in_button = (mx >= add_col_x + 5 && mx < add_col_x + 5 + 60.0f && my >= 4 && my < header_h - 4);
            if (!in_menu && !in_button) {
                ui->table_state.show_type_menu = 0;
            }
        }
    }

    // --- Select Cell Popup Menu ---
    if (ui->table_state.show_select_popup && ui->table_state.active_block_id == block->id) {
        int cidx = ui->table_state.select_popup_col;
        uint64_t rrid = ui->table_state.select_popup_row;
        AslanColumn *col = &table->columns[cidx];
        
        float menu_w = 160.0f;
        float item_hh = 28.0f;
        int item_cnt = col->option_count;
        float menu_h = (item_cnt > 0 ? item_cnt : 1) * item_hh + 12.0f + 40.0f; // +40 for add option row
        // Popups are drawn in absolute screen coordinates to float over everything
        float menu_x = ui->table_state.select_popup_x - ui->abs_offset_x;
        float menu_y = ui->table_state.select_popup_y - ui->abs_offset_y;

        nvgw_panel(wc, menu_x, menu_y, menu_w, menu_h, ui->dark_mode ? nvgRGBA(50, 50, 55, 255) : nvgRGBA(255, 255, 255, 255), 8.0f);

        float cur_y = menu_y + 6;

        if (item_cnt == 0) {
             nvgFontSize(vg, 14.0f); nvgFontFace(vg, "Inter");
             nvgFillColor(vg, ui->dark_mode ? nvgRGBA(150, 150, 160, 255) : nvgRGBA(100, 100, 110, 255));
             nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
             nvgText(vg, menu_x + menu_w/2, cur_y + item_hh/2, "No options", NULL);
             cur_y += item_hh;
        } else {
            for (int m = 0; m < item_cnt; m++) {
                float ix = menu_x + 4, iy = cur_y;
                float iw = menu_w - 8;
                
                if (ui->table_state.editing_select_option == m) {
                    uint32_t inp_id = nvgw_id("edit_opt", 8) ^ nvgw_id(&m, sizeof(m));
                    nvgw_text_input(wc, inp_id, ix, iy, iw - 32, item_hh, ui->table_state.option_buffer, sizeof(ui->table_state.option_buffer), 14.0f);
                    
                    uint32_t ok_id = nvgw_id("ok_opt", 6) ^ nvgw_id(&m, sizeof(m));
                    nvgFontFace(vg, "Inter");
                    if (nvgw_button(wc, ok_id, ix + iw - 30, iy, 30, item_hh, "OK")) {
                        if (strlen(ui->table_state.option_buffer) == 0) {
                             // Empty name means delete
                             aslan_table_remove_option(table, cidx, m);
                        } else {
                             aslan_table_rename_option(table, cidx, m, ui->table_state.option_buffer);
                        }
                        ui->table_state.editing_select_option = -1;
                        ui->table_state.option_buffer[0] = '\0';
                        ui_request_redraw(ui);
                    }
                } else {
                    uint32_t mid = nvgw_id(&m, sizeof(m)) ^ nvgw_id_str("sel_opt");
                    if (nvgw_button(wc, mid, ix, iy, iw, item_hh, col->options[m].label)) {
                        uint32_t now = SDL_GetTicks();
                        if (ui->table_state.last_option_click_idx == m && (now - ui->table_state.last_option_click_time) < 400) {
                            // Double Click detected -> Edit Mode
                            ui->table_state.editing_select_option = m;
                            strncpy(ui->table_state.option_buffer, col->options[m].label, sizeof(ui->table_state.option_buffer)-1);
                            ui->table_state.option_buffer[sizeof(ui->table_state.option_buffer)-1] = '\0';
                            ui->table_state.last_option_click_idx = -1; // Reset
                        } else {
                            // Single Click -> Select Option
                            aslan_table_set_select(table, cidx, rrid, m);
                            aslan_table_eval_formulas(table);
                            ui->table_state.editing_select_option = -1;
                            ui->table_state.last_option_click_idx = m;
                            ui->table_state.last_option_click_time = now;
                        }
                        ui_request_redraw(ui);
                    }
                }
                cur_y += item_hh;
            }
        }
        
        // Add option input row
        cur_y += 4;
        uint32_t opt_in_id = nvgw_id("opt_in", 6) ^ nvgw_id(&cidx, sizeof(cidx));
        nvgw_text_input(wc, opt_in_id, menu_x + 4, cur_y, menu_w - 44, 28, ui->table_state.option_buffer, sizeof(ui->table_state.option_buffer), 14.0f);
        
        uint32_t opt_add_id = nvgw_id("opt_add", 7) ^ nvgw_id(&cidx, sizeof(cidx));
        if (nvgw_button(wc, opt_add_id, menu_x + menu_w - 36, cur_y, 32, 28, "+")) {
             if (strlen(ui->table_state.option_buffer) > 0) {
                 aslan_table_add_option(table, cidx, ui->table_state.option_buffer, 0x95a5a6ff);
                 ui->table_state.option_buffer[0] = '\0'; // clear buffer
                 ui_request_redraw(ui);
             }
        }
        
        if (ui->nvgw.mouse_clicked) {
            bool in_menu = (mx >= menu_x && mx < menu_x + menu_w && my >= menu_y && my < menu_y + menu_h);
            if (!in_menu) {
                ui->table_state.show_select_popup = 0;
                ui->table_state.is_editing_cell = 0;
            }
        }
    }

    // --- Date Cell Popup Menu ---
    if (ui->table_state.show_date_picker && ui->table_state.active_block_id == block->id) {
        int cidx = ui->table_state.date_picker_col;
        uint64_t rrid = ui->table_state.date_picker_row;
        
        float menu_w = 200.0f;
        float menu_h = 240.0f;
        float menu_x = ui->table_state.date_picker_x - ui->abs_offset_x;
        float menu_y = ui->table_state.date_picker_y - ui->abs_offset_y;

        nvgw_panel(wc, menu_x, menu_y, menu_w, menu_h, ui->dark_mode ? nvgRGBA(50, 50, 55, 255) : nvgRGBA(255, 255, 255, 255), 8.0f);

        // Fetch current date
        int64_t ts = aslan_table_get_date(table, cidx, rrid);
        int year = 2024, month = 0, day = 1;
        if (ts > 0) {
            time_t epoch = ts / 1000;
            struct tm *tm_info = localtime(&epoch);
            if (tm_info) {
               year = tm_info->tm_year + 1900;
               month = tm_info->tm_mon;
               day = tm_info->tm_mday;
            }
        }

        nvgFontSize(vg, 16.0f); nvgFontFace(vg, "Inter");
        nvgFillColor(vg, ui->dark_mode ? nvgRGBA(220, 220, 230, 255) : nvgRGBA(40, 40, 45, 255));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        char date_str[64];
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", year, month + 1, day);
        nvgText(vg, menu_x + menu_w/2, menu_y + 20, date_str, NULL);

        // Very simplified UI: +/- year, month, day and an OK button
        uint32_t b_y_dec = nvgw_id_str("b_y_dec"), b_y_inc = nvgw_id_str("b_y_inc");
        uint32_t b_m_dec = nvgw_id_str("b_m_dec"), b_m_inc = nvgw_id_str("b_m_inc");
        uint32_t b_d_dec = nvgw_id_str("b_d_dec"), b_d_inc = nvgw_id_str("b_d_inc");
        uint32_t b_ok = nvgw_id_str("b_ok");

        bool changed = false;
        if (nvgw_button(wc, b_y_dec, menu_x + 10, menu_y + 50, 40, 30, "<")) { year--; changed = true; }
        nvgFillColor(vg, ui->dark_mode ? nvgRGBA(255, 255, 255, 255) : nvgRGBA(30, 30, 35, 255));
        nvgText(vg, menu_x + menu_w/2, menu_y + 65, "Year", NULL);
        if (nvgw_button(wc, b_y_inc, menu_x + menu_w - 50, menu_y + 50, 40, 30, ">")) { year++; changed = true; }

        if (nvgw_button(wc, b_m_dec, menu_x + 10, menu_y + 90, 40, 30, "<")) { month--; changed = true; }
        nvgFillColor(vg, ui->dark_mode ? nvgRGBA(255, 255, 255, 255) : nvgRGBA(30, 30, 35, 255));
        nvgText(vg, menu_x + menu_w/2, menu_y + 105, "Month", NULL);
        if (nvgw_button(wc, b_m_inc, menu_x + menu_w - 50, menu_y + 90, 40, 30, ">")) { month++; changed = true; }

        if (nvgw_button(wc, b_d_dec, menu_x + 10, menu_y + 130, 40, 30, "<")) { day--; changed = true; }
        nvgFillColor(vg, ui->dark_mode ? nvgRGBA(255, 255, 255, 255) : nvgRGBA(30, 30, 35, 255));
        nvgText(vg, menu_x + menu_w/2, menu_y + 145, "Day", NULL);
        if (nvgw_button(wc, b_d_inc, menu_x + menu_w - 50, menu_y + 130, 40, 30, ">")) { day++; changed = true; }

        if (changed) {
            if (month < 0) { month = 11; year--; }
            if (month > 11) { month = 0; year++; }
            if (day < 1) day = 1;
            if (day > 31) day = 31;
            
            struct tm t = {0};
            t.tm_year = year - 1900;
            t.tm_mon = month;
            t.tm_mday = day;
            t.tm_hour = 12; // Avoid DST and timezone shifts ruining the day
            t.tm_isdst = -1;
            int64_t new_ts = (int64_t)mktime(&t) * 1000LL;
            aslan_table_set_date(table, cidx, rrid, new_ts);
            aslan_table_eval_formulas(table);
            ui_request_redraw(ui);
        }

        if (nvgw_button(wc, b_ok, menu_x + 20, menu_y + 190, menu_w - 40, 30, "Confirm")) {
            ui->table_state.show_date_picker = 0;
            ui->table_state.is_editing_cell = 0;
            ui_request_redraw(ui);
        }

        if (ui->nvgw.mouse_clicked) {
            bool in_menu = (mx >= menu_x && mx < menu_x + menu_w && my >= menu_y && my < menu_y + menu_h);
            if (!in_menu) {
                ui->table_state.show_date_picker = 0;
                ui->table_state.is_editing_cell = 0;
            }
        }
    }

    // --- Formula Editor Popup ---
    if (ui->table_state.show_formula_popup && ui->table_state.active_block_id == block->id) {
        nvgw_set_offset(wc, 0, 0); // Window-absolute positioning
        
        float menu_w = 400.0f;
        float menu_h = 120.0f;
        float menu_x = (ui->win_width - menu_w) * 0.5f;
        float menu_y = (ui->win_height - menu_h) * 0.5f;

        nvgw_panel(wc, menu_x, menu_y, menu_w, menu_h, ui->dark_mode ? nvgRGBA(45, 45, 50, 255) : nvgRGBA(245, 245, 250, 255), 10.0f);
        
        nvgFontSize(vg, 16.0f); nvgFontFace(vg, "Inter");
        nvgFillColor(vg, ui->dark_mode ? nvgRGBA(220, 220, 230, 255) : nvgRGBA(40, 40, 45, 255));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(vg, menu_x + 15, menu_y + 15, "Edit Formula", NULL);

        if (nvgw_text_input(wc, nvgw_id_str("formula_input"), menu_x + 15, menu_y + 40, menu_w - 30, 30, ui->table_state.formula_buffer, sizeof(ui->table_state.formula_buffer), 14.0f)) {
            aslan_table_set_formula(table, ui->table_state.formula_popup_col, ui->table_state.formula_buffer);
            aslan_table_eval_formulas(table);
            // Persist to AOF log
            char cmd[1200];
            Page *pg = doc_page_get_active(engine->doc);
            snprintf(cmd, sizeof(cmd), "block_table_col_formula %u %u %d \"%s\"",
                     pg ? pg->id : 1, block->id,
                     ui->table_state.formula_popup_col, ui->table_state.formula_buffer);
            engine_execute_command(engine, cmd);
            ui->table_state.show_formula_popup = 0;
            ui_request_redraw(ui);
        }

        if (nvgw_button(wc, nvgw_id_str("formula_cancel"), menu_x + menu_w - 180, menu_y + menu_h - 35, 80, 25, "Cancel")) {
            ui->table_state.show_formula_popup = 0;
            ui_request_redraw(ui);
        }

        if (nvgw_button(wc, nvgw_id_str("formula_ok"), menu_x + menu_w - 95, menu_y + menu_h - 35, 80, 25, "Apply")) {
            aslan_table_set_formula(table, ui->table_state.formula_popup_col, ui->table_state.formula_buffer);
            aslan_table_eval_formulas(table);
            // Persist to AOF log
            char cmd[1200];
            Page *pg2 = doc_page_get_active(engine->doc);
            snprintf(cmd, sizeof(cmd), "block_table_col_formula %u %u %d \"%s\"",
                     pg2 ? pg2->id : 1, block->id,
                     ui->table_state.formula_popup_col, ui->table_state.formula_buffer);
            engine_execute_command(engine, cmd);
            ui->table_state.show_formula_popup = 0;
            ui_request_redraw(ui);
        }

        nvgw_set_offset(wc, ui->abs_offset_x, ui->abs_offset_y); // Restore for next blocks
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
