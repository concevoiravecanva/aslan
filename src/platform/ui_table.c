// =============================================================================
// ASLAN UI TABLE — NanoVG-based Definitive Table Block Rendering
// =============================================================================

#include "ui_table.h"
#include "../document/aslan_table.h"
#include "ui_shared.h"
#include "../core/engine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

#define TBL_HEADER_H      34.0f
#define TBL_ROW_H         32.0f
#define TBL_DEFAULT_COL_W 140.0f
#define TBL_MIN_COL_W      40.0f
#define TBL_MAX_COL_W     600.0f
#define TBL_RESIZE_ZONE     6.0f   // px from column right edge that triggers resize
#define TBL_SCROLLBAR_W    10.0f

// -----------------------------------------------------------------------------
// Theme helpers (inline, adapts to dark_mode)
// -----------------------------------------------------------------------------

static inline NVGcolor tbl_bg(int dark)      { return dark ? nvgRGBA(28,  29, 33, 255) : nvgRGBA(255,255,255,255); }
static inline NVGcolor tbl_hdr(int dark)     { return dark ? nvgRGBA(36,  37, 42, 255) : nvgRGBA(246,247,250,255); }
static inline NVGcolor tbl_border(int dark)  { return dark ? nvgRGBA(55,  57, 65, 255) : nvgRGBA(218,220,226,255); }
static inline NVGcolor tbl_text(int dark)    { return dark ? nvgRGBA(210,212,220,255)  : nvgRGBA(40, 42, 50, 255); }
static inline NVGcolor tbl_dim(int dark)     { return dark ? nvgRGBA(100,102,115,255)  : nvgRGBA(150,152,165,255); }
static inline NVGcolor tbl_stripe(int dark)  { return dark ? nvgRGBA(255,255,255,  4)  : nvgRGBA(0,   0,  0,   4); }
static inline NVGcolor tbl_hover(int dark)   { return dark ? nvgRGBA(255,255,255, 10)  : nvgRGBA(0,   0,  0,   8); }
static inline NVGcolor tbl_sel(int dark)     { return dark ? nvgRGBA(60, 120,255, 40)  : nvgRGBA(60, 120,255, 30); }
static inline NVGcolor tbl_accent(void)      { return nvgRGBA(60, 120, 255, 255); }
static inline NVGcolor tbl_ghost(int dark)   { return dark ? nvgRGBA(100,102,115,130)  : nvgRGBA(160,163,180,130); }

// Popup item button (custom for dark-mode compatibility)
static bool tbl_item_button(NVGcontext *vg, NvgWidgetCtx *wc, uint32_t id,
                             float x, float y, float w, float h,
                             const char *text, int dark) {
    float mx = wc->mouse_x - wc->offset_x;
    float my = wc->mouse_y - wc->offset_y;
    bool hovered = mx >= x && mx < x + w && my >= y && my < y + h;
    if (hovered) wc->hot_id = id;

    bool clicked = false;
    if (wc->active_id == id && wc->mouse_released && hovered) clicked = true;
    if (wc->hot_id == id && wc->mouse_clicked) wc->active_id = id;

    if (hovered || wc->active_id == id) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x, y, w, h, 4.0f);
        nvgFillColor(vg, wc->active_id == id
            ? nvgRGBA(60,120,255,60)
            : (dark ? nvgRGBA(255,255,255,12) : nvgRGBA(0,0,0,8)));
        nvgFill(vg);
    }

    nvgFontSize(vg, 13.0f);
    nvgFontFace(vg, "Inter");
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, dark ? nvgRGBA(210,212,220,255) : nvgRGBA(40,42,50,255));
    nvgText(vg, x + 10, y + h * 0.5f, text, NULL);
    return clicked;
}

// Inline text input with dark mode
static bool tbl_text_input(NVGcontext *vg, NvgWidgetCtx *wc, uint32_t id,
                            float x, float y, float w, float h,
                            char *buf, size_t bufsz, float fsz, int dark) {
    float mx = wc->mouse_x - wc->offset_x;
    float my = wc->mouse_y - wc->offset_y;
    bool hovered = mx >= x && mx < x + w && my >= y && my < y + h;
    if (hovered) wc->hot_id = id;
    if (wc->hot_id == id && wc->mouse_clicked) wc->focus_id = id;

    bool is_focused = (wc->focus_id == id);
    bool finished = false;

    if (is_focused) {
        size_t len = strlen(buf);
        if (wc->text_input[0] != '\0' && len + strlen(wc->text_input) < bufsz) {
            strncat(buf, wc->text_input, bufsz - len - 1);
            wc->text_input[0] = '\0';
        }
        if (wc->key_backspace && len > 0) {
            // UTF-8 safe: backspace full multi-byte code point
            unsigned char *p = (unsigned char *)buf + len - 1;
            while (p > (unsigned char *)buf && (*p & 0xC0) == 0x80) p--;
            *p = '\0';
            wc->key_backspace = false;
        }
        if (wc->key_enter) {
            finished = true;
            wc->focus_id = 0;
        }
    }

    // Background
    NVGcolor bg = is_focused
        ? (dark ? nvgRGBA(40,44,55,255) : nvgRGBA(255,255,255,255))
        : (dark ? nvgRGBA(30,32,40,255) : nvgRGBA(252,252,254,255));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y + 2, w, h - 4, 4.0f);
    nvgFillColor(vg, bg);
    nvgFill(vg);

    // Border
    NVGcolor border = is_focused
        ? tbl_accent()
        : (dark ? nvgRGBA(60,62,75,255) : nvgRGBA(210,212,220,255));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y + 2, w, h - 4, 4.0f);
    nvgStrokeColor(vg, border);
    nvgStrokeWidth(vg, is_focused ? 1.5f : 1.0f);
    nvgStroke(vg);

    // Text + cursor
    nvgSave(vg);
    nvgFontSize(vg, fsz);
    nvgFontFace(vg, "Inter");
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, tbl_text(dark));
    nvgScissor(vg, x + 4, y, w - 8, h);

    float bounds[4];
    nvgTextBounds(vg, x + 6, y + h * 0.5f, buf, NULL, bounds);
    float tw = bounds[2] - bounds[0];
    float tx = x + 6;
    if (tw > w - 12) tx = x + 6 - (tw - (w - 12));
    nvgText(vg, tx, y + h * 0.5f, buf, NULL);

    if (is_focused) {
        nvgBeginPath(vg);
        nvgRect(vg, tx + tw + 1, y + h * 0.2f, 1.5f, h * 0.6f);
        nvgFillColor(vg, tbl_accent());
        nvgFill(vg);
    }
    nvgResetScissor(vg);
    nvgRestore(vg);

    return finished;
}

// -----------------------------------------------------------------------------
// Column width helpers
// -----------------------------------------------------------------------------

static float col_x(UIHost *ui, AslanTable *table, int idx) {
    (void)table;
    float x = 0;
    for (int i = 0; i < idx; i++) {
        float w = ui->table_state.col_widths[i];
        x += (w > 0 ? w : TBL_DEFAULT_COL_W);
    }
    return x;
}

static float col_w(UIHost *ui, int idx) {
    float w = ui->table_state.col_widths[idx];
    return (w > 0 ? w : TBL_DEFAULT_COL_W);
}

static float table_total_w(UIHost *ui, AslanTable *table) {
    float total = 0;
    for (size_t c = 0; c < table->column_count; c++)
        total += col_w(ui, (int)c);
    return total;
}

// -----------------------------------------------------------------------------
// Header Rendering & Interaction
// -----------------------------------------------------------------------------

static void render_header(UIHost *ui, Engine *engine, AslanTable *table,
                           Block *block, float width, float header_h) {
    NVGcontext *vg = ui->vg;
    NvgWidgetCtx *wc = &ui->nvgw;
    int dark = ui->dark_mode;

    float mx = ui->mouse_x - ui->abs_offset_x;
    float my = ui->mouse_y - ui->abs_offset_y;
    bool mdown = (ui->mouse_buttons & 1) != 0;

    // Background
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, width, header_h);
    nvgFillColor(vg, tbl_hdr(dark));
    nvgFill(vg);

    for (size_t c = 0; c < table->column_count; c++) {
        float cx = col_x(ui, table, (int)c);
        float cw = col_w(ui, (int)c);
        AslanColumn *col_ptr = &table->columns[c];

        // Column vertical border
        nvgBeginPath(vg);
        nvgMoveTo(vg, cx + cw, 0);
        nvgLineTo(vg, cx + cw, header_h);
        nvgStrokeColor(vg, tbl_border(dark));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        // Resize zone detection
        bool near_resize = (mx > cx + cw - TBL_RESIZE_ZONE && mx <= cx + cw
                            && my >= 0 && my < header_h);
        if (near_resize) {
            ui_set_cursor(ui, UI_CURSOR_SIZEWE);
        }

        // Start column resize drag
        if (near_resize && wc->mouse_clicked) {
            ui->table_state.resizing_col = (int)c;
            ui->table_state.resize_start_x = mx;
            ui->table_state.resize_start_w = cw;
        }

        // Inline rename text input
        if (ui->table_state.renaming_col_idx == (int)c) {
            uint32_t rid = nvgw_id_str("hdr_rename") ^ nvgw_id(&c, sizeof(c));
            // Force focus first frame
            if (wc->focus_id != rid) wc->focus_id = rid;
            bool done = tbl_text_input(vg, wc, rid, cx + 2, 2, cw - 4, header_h - 4,
                                       ui->table_state.rename_buffer,
                                       sizeof(ui->table_state.rename_buffer),
                                       13.0f, dark);
            if (done) {
                if (strlen(ui->table_state.rename_buffer) > 0) {
                    aslan_table_rename_column(table, (int)c, ui->table_state.rename_buffer);
                    char cmd[256];
                    Page *pg = doc_page_get_active(engine->doc);
                    snprintf(cmd, sizeof(cmd), "block_table_col_rename %u %u %d \"%s\"",
                             pg ? pg->id : 1, block->id, (int)c,
                             ui->table_state.rename_buffer);
                    engine_execute_command(engine, cmd);
                }
                ui->table_state.renaming_col_idx = -1;
                ui_request_redraw(ui);
            }
            // Escape to cancel rename
            // (handled externally via Escape key in event handler)
            continue;
        }

        // Icon
        nvgFontSize(vg, 14.0f);
        nvgFontFace(vg, "Icons");
        nvgFillColor(vg, tbl_dim(dark));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, cx + 8, header_h * 0.5f, table_col_type_icon(col_ptr->type), NULL);

        // Column name (clipped)
        nvgSave(vg);
        nvgScissor(vg, cx + 28, 0, cw - 52, header_h);
        nvgFontSize(vg, 13.0f);
        nvgFontFace(vg, "Inter");
        nvgFillColor(vg, tbl_text(dark));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, cx + 28, header_h * 0.5f, col_ptr->name, NULL);
        nvgResetScissor(vg);
        nvgRestore(vg);

        // Sort indicator
        bool is_sorted = (table->sort_column == (int)c);
        if (is_sorted) {
            float sx = cx + cw - 20;
            float sy = header_h * 0.5f;
            nvgFontSize(vg, 12.0f);
            nvgFontFace(vg, "Icons");
            nvgFillColor(vg, tbl_accent());
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, sx, sy,
                    table->sort_ascending ? ICON_SORT_ASC : ICON_SORT_DESC, NULL);
        }

        // Header formula reference highlight
        if (ui->table_state.show_formula_popup) {
            bool hdr_hovered = (mx >= cx && mx < cx + cw && my >= 0 && my < header_h);
            nvgBeginPath(vg);
            nvgRect(vg, cx + 1, 1, cw - 2, header_h - 2);
            nvgFillColor(vg, hdr_hovered ? nvgRGBA(80,140,255,60) : nvgRGBA(80,140,255,18));
            nvgFill(vg);
            if (hdr_hovered) {
                nvgBeginPath(vg);
                nvgRect(vg, cx + 1, 1, cw - 2, header_h - 2);
                nvgStrokeColor(vg, nvgRGBA(80,140,255,200));
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);
            }
            if (ui->nvgw.mouse_clicked && hdr_hovered) {
                char ref[128];
                snprintf(ref, sizeof(ref), "[%s]", col_ptr->name);
                size_t curlen = strlen(ui->table_state.formula_buffer);
                size_t reflen = strlen(ref);
                if (curlen + reflen < sizeof(ui->table_state.formula_buffer) - 1)
                    strcat(ui->table_state.formula_buffer, ref);
                ui_request_redraw(ui);
            }
        }

        // Left click: single = sort, double = rename inline
        if (!near_resize && !ui->table_state.show_formula_popup
            && wc->mouse_clicked
            && mx >= cx && mx < cx + cw
            && my >= 0 && my < header_h
            && ui->table_state.renaming_col_idx < 0) {
            uint32_t now = SDL_GetTicks();
            bool is_double = (ui->table_state.last_hdr_click_col == (int)c
                              && now - ui->table_state.last_hdr_click_time < 400);
            if (is_double) {
                // Double-click: start inline rename
                ui->table_state.renaming_col_idx = (int)c;
                strncpy(ui->table_state.rename_buffer, table->columns[c].name,
                        sizeof(ui->table_state.rename_buffer) - 1);
                ui->table_state.rename_buffer[sizeof(ui->table_state.rename_buffer)-1] = '\0';
                ui->table_state.last_hdr_click_col = -1;
            } else {
                // Single click: sort
                ui->table_state.last_hdr_click_time = now;
                ui->table_state.last_hdr_click_col = (int)c;
                if (table->sort_column == (int)c) {
                    if (table->sort_ascending) {
                        aslan_table_sort(table, (int)c, false);
                    } else {
                        aslan_table_sort_clear(table);
                    }
                } else {
                    aslan_table_sort(table, (int)c, true);
                }
            }
            ui_request_redraw(ui);
        }

        // Right click: column context menu
        if ((ui->mouse_buttons & 4) && mx >= cx && mx < cx + cw
            && my >= 0 && my < header_h) {
            ui->show_context_menu = 1;
            ui->context_menu_x = ui->mouse_x;
            ui->context_menu_y = ui->mouse_y;
            ui->table_state.context_menu_mode = 1;
            ui->table_state.context_menu_col = (int)c;
            ui->table_state.active_block_id = block->id;
            ui_request_redraw(ui);
        }
    }

    // Process active column resize
    if (ui->table_state.resizing_col >= 0) {
        ui_set_cursor(ui, UI_CURSOR_SIZEWE);
        float delta = mx - ui->table_state.resize_start_x;
        float new_w = ui->table_state.resize_start_w + delta;
        if (new_w < TBL_MIN_COL_W) new_w = TBL_MIN_COL_W;
        if (new_w > TBL_MAX_COL_W) new_w = TBL_MAX_COL_W;
        ui->table_state.col_widths[ui->table_state.resizing_col] = new_w;
        if (!mdown) {
            ui->table_state.resizing_col = -1;
        }
        ui_request_redraw(ui);
    }

    // "+" Add column button
    float add_x = table_total_w(ui, table);
    uint32_t add_id = nvgw_id(&block->id, sizeof(block->id)) ^ nvgw_id_str("add_col");
    float add_w = 56.0f;

    // Draw "+" button manually (dark-mode compatible)
    float bx = add_x + 4, by = 4, bw = add_w, bh = header_h - 8;
    float amx = wc->mouse_x - wc->offset_x;
    float amy = wc->mouse_y - wc->offset_y;
    bool ahov = (amx >= bx && amx < bx + bw && amy >= by && amy < by + bh);
    if (ahov) wc->hot_id = add_id;
    bool add_clicked = (wc->active_id == add_id && wc->mouse_released && ahov);
    if (wc->hot_id == add_id && wc->mouse_clicked) wc->active_id = add_id;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, bx, by, bw, bh, 4.0f);
    nvgFillColor(vg, ahov
        ? (dark ? nvgRGBA(255,255,255,14) : nvgRGBA(0,0,0,10))
        : (dark ? nvgRGBA(255,255,255,6)  : nvgRGBA(0,0,0,4)));
    nvgFill(vg);
    nvgFontSize(vg, 13.0f);
    nvgFontFace(vg, "Inter");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, tbl_dim(dark));
    nvgText(vg, bx + bw * 0.5f, by + bh * 0.5f, "+ Add", NULL);

    if (add_clicked) {
        bool was_open = (ui->table_state.show_type_menu != 0);
        ui->table_state.show_type_menu = was_open ? 0 : 1;
        if (ui->table_state.show_type_menu) {
            ui->table_state.type_menu_x = ui->mouse_x;
            ui->table_state.type_menu_y = ui->mouse_y;
            ui->table_state.active_block_id = block->id;
            wc->mouse_clicked = false; // consume — prevents same-frame close
        }
        ui_request_redraw(ui);
    }

    // Bottom border
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, header_h);
    nvgLineTo(vg, width, header_h);
    nvgStrokeColor(vg, tbl_border(dark));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
}

// -----------------------------------------------------------------------------
// Cell display helpers
// -----------------------------------------------------------------------------

// Draw a SELECT badge/pill in a cell
static void draw_select_badge(NVGcontext *vg, float cx, float ry,
                               float cw, float row_h,
                               const char *label, uint32_t color_rgba,
                               int dark) {
    (void)dark;
    float r = (color_rgba >> 24) & 0xFF;
    float g = (color_rgba >> 16) & 0xFF;
    float b = (color_rgba >>  8) & 0xFF;
    NVGcolor bg = nvgRGBAf(r/255.f, g/255.f, b/255.f, 0.25f);
    NVGcolor fg = nvgRGBAf(r/255.f * 0.6f, g/255.f * 0.6f, b/255.f * 0.6f, 1.0f);

    // Measure text width to size the pill
    float bw[4];
    nvgFontSize(vg, 12.0f);
    nvgFontFace(vg, "Inter");
    nvgTextBounds(vg, 0, 0, label, NULL, bw);
    float tw = bw[2] - bw[0];
    float ph = 18.0f, pw = tw + 14.0f;
    if (pw > cw - 10) pw = cw - 10;
    float px = cx + 8;
    float py = ry + (row_h - ph) * 0.5f;

    nvgBeginPath(vg);
    nvgRoundedRect(vg, px, py, pw, ph, ph * 0.5f);
    nvgFillColor(vg, bg);
    nvgFill(vg);

    nvgSave(vg);
    nvgScissor(vg, px + 2, py, pw - 4, ph);
    nvgFontSize(vg, 12.0f);
    nvgFontFace(vg, "Inter");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, fg);
    nvgText(vg, px + pw * 0.5f, py + ph * 0.5f, label, NULL);
    nvgResetScissor(vg);
    nvgRestore(vg);
}

// -----------------------------------------------------------------------------
// Row Rendering
// -----------------------------------------------------------------------------

static void render_rows(UIHost *ui, Engine *engine __attribute__((unused)),
                        AslanTable *table,
                        Block *block, float width, float height,
                        float header_h, float row_h) {
    NVGcontext *vg = ui->vg;
    NvgWidgetCtx *wc = &ui->nvgw;
    int dark = ui->dark_mode;

    float mx = ui->mouse_x - ui->abs_offset_x;
    float my = ui->mouse_y - ui->abs_offset_y;

    float rows_area_h = height - header_h;
    size_t count = aslan_table_view_count(table);
    float content_h = (float)(count + 1) * row_h; // +1 for ghost row

    // Scroll area
    nvgw_begin_scroll_area(wc, 0, header_h, width, rows_area_h,
                            content_h, &ui->table_state.row_scroll_y);
    float scroll = ui->table_state.row_scroll_y;

    // Determine which popups are open (to suppress cell clicks underneath)
    bool any_popup = (ui->table_state.show_date_picker
                   || ui->table_state.show_select_popup
                   || ui->table_state.show_type_menu
                   || ui->table_state.show_formula_popup)
                   && ui->table_state.active_block_id == block->id;

    bool saved_clicked = wc->mouse_clicked;
    if (any_popup) {
        // Check if click is inside any open popup (absolute coords)
        bool in_popup = false;
        float am_x = wc->mouse_x; // absolute
        float am_y = wc->mouse_y;

        if (ui->table_state.show_date_picker && ui->table_state.active_block_id == block->id) {
            float px = ui->table_state.date_picker_x, py = ui->table_state.date_picker_y;
            if (am_x >= px && am_x < px + 220.0f && am_y >= py && am_y < py + 260.0f)
                in_popup = true;
        }
        if (ui->table_state.show_select_popup && ui->table_state.active_block_id == block->id) {
            AslanColumn *sc = &table->columns[ui->table_state.select_popup_col];
            float ph = (sc->option_count > 0 ? sc->option_count : 1) * 30.0f + 20.0f + 44.0f;
            float px = ui->table_state.select_popup_x, py = ui->table_state.select_popup_y;
            if (am_x >= px && am_x < px + 180.0f && am_y >= py && am_y < py + ph)
                in_popup = true;
        }
        if (ui->table_state.show_type_menu && ui->table_state.active_block_id == block->id) {
            float px = ui->table_state.type_menu_x, py = ui->table_state.type_menu_y;
            if (am_x >= px && am_x < px + 180.0f && am_y >= py && am_y < py + 320.0f)
                in_popup = true;
        }
        if (ui->table_state.show_formula_popup) {
            float fw = 420.0f, fh = 130.0f;
            float fpx = (ui->win_width  - fw) * 0.5f;
            float fpy = (ui->win_height - fh) * 0.5f;
            if (am_x >= fpx && am_x < fpx + fw && am_y >= fpy && am_y < fpy + fh)
                in_popup = true;
        }
        if (in_popup) wc->mouse_clicked = false;
    }

    for (size_t i = 0; i <= count; i++) {
        float ry = header_h + (float)i * row_h - scroll;
        // Only render if in visible range
        if (ry + row_h < header_h) continue;
        if (ry > height) break;

        bool is_ghost = (i == count);
        uint64_t row_id = is_ghost ? 0 : aslan_table_view_row_id(table, (int)i);

        // Row background (alternating stripe)
        if (i % 2 == 1 && !is_ghost) {
            nvgBeginPath(vg);
            nvgRect(vg, 0, ry, width, row_h);
            nvgFillColor(vg, tbl_stripe(dark));
            nvgFill(vg);
        }

        // Hover highlight
        if (!is_ghost && mx >= 0 && mx < width && my >= ry && my < ry + row_h) {
            nvgBeginPath(vg);
            nvgRect(vg, 0, ry, width, row_h);
            nvgFillColor(vg, tbl_hover(dark));
            nvgFill(vg);
        }

        // Ghost row hint
        if (is_ghost) {
            nvgFontSize(vg, 13.0f);
            nvgFontFace(vg, "Inter");
            nvgFillColor(vg, tbl_ghost(dark));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, 12, ry + row_h * 0.5f, "+ New Row...", NULL);

            // Click ghost row → add row
            if (wc->mouse_clicked && mx >= 0 && mx < width && my >= ry && my < ry + row_h) {
                // Commit any pending edit first
                if (ui->table_state.is_editing_cell) {
                    aslan_table_set_cell(table, ui->table_state.active_col_idx,
                                        ui->table_state.select_popup_row,
                                        ui->table_state.rename_buffer);
                    aslan_table_eval_formulas(table);
                    ui->table_state.is_editing_cell = 0;
                }
                uint64_t new_id = aslan_table_add_row(table);
                aslan_table_eval_formulas(table);
                // Start editing first text column of new row
                for (size_t cc = 0; cc < table->column_count; cc++) {
                    AslanColumnType ct = table->columns[cc].type;
                    if (ct == ASLAN_COL_TEXT || ct == ASLAN_COL_NUMBER
                        || ct == ASLAN_COL_LINK || ct == ASLAN_COL_FILE) {
                        ui->table_state.active_block_id = block->id;
                        ui->table_state.active_col_idx = (int)cc;
                        ui->table_state.select_popup_row = new_id;
                        ui->table_state.is_editing_cell = 1;
                        ui->table_state.rename_buffer[0] = '\0';
                        uint32_t fid = nvgw_id(&new_id, sizeof(new_id))
                            ^ nvgw_id(&cc, sizeof(cc))
                            ^ nvgw_id(&block->id, sizeof(block->id));
                        wc->focus_id = fid;
                        break;
                    }
                }
                ui_request_redraw(ui);
            }
            // Row separator
            nvgBeginPath(vg);
            nvgMoveTo(vg, 0, ry + row_h);
            nvgLineTo(vg, width, ry + row_h);
            nvgStrokeColor(vg, tbl_border(dark));
            nvgStrokeWidth(vg, 0.5f);
            nvgStroke(vg);
            continue;
        }

        // Right-click row context menu
        if ((ui->mouse_buttons & 4) && mx >= 0 && mx < width
            && my >= ry && my < ry + row_h) {
            ui->show_context_menu = 1;
            ui->context_menu_x = ui->mouse_x;
            ui->context_menu_y = ui->mouse_y;
            ui->table_state.context_menu_mode = 2;
            ui->table_state.context_menu_row = row_id;
            ui->table_state.active_block_id = block->id;
            ui_request_redraw(ui);
        }

        // Cells
        for (size_t c = 0; c < table->column_count; c++) {
            float cx = col_x(ui, table, (int)c);
            float cw_v = col_w(ui, (int)c);
            AslanColumn *col_ptr = &table->columns[c];
            uint32_t cell_id = nvgw_id(&row_id, sizeof(row_id))
                ^ nvgw_id(&c, sizeof(c))
                ^ nvgw_id(&block->id, sizeof(block->id));

            bool cell_active = ui->table_state.is_editing_cell
                && ui->table_state.active_col_idx == (int)c
                && ui->table_state.select_popup_row == row_id
                && ui->table_state.active_block_id == block->id;

            // ─── CHECKBOX: direct toggle ──────────────────────────────────
            if (col_ptr->type == ASLAN_COL_CHECKBOX) {
                bool checked = aslan_table_get_checkbox(table, (int)c, row_id);
                float bsz = 14.0f;
                uint32_t cid = cell_id ^ nvgw_id_str("chk");
                float bx = cx + cw_v * 0.5f - bsz * 0.5f;
                float by = ry + row_h * 0.5f - bsz * 0.5f;
                // Hit test
                float hmx = wc->mouse_x - wc->offset_x;
                float hmy = wc->mouse_y - wc->offset_y;
                bool chk_hov = hmx >= bx && hmx < bx + bsz && hmy >= by && hmy < by + bsz;
                if (chk_hov) wc->hot_id = cid;
                if (wc->active_id == cid && wc->mouse_released && chk_hov) {
                    checked = !checked;
                    aslan_table_set_checkbox(table, (int)c, row_id, checked);
                    aslan_table_eval_formulas(table);
                    ui_request_redraw(ui);
                }
                if (wc->hot_id == cid && wc->mouse_clicked) wc->active_id = cid;

                // Draw checkbox
                nvgBeginPath(vg);
                nvgRoundedRect(vg, bx, by, bsz, bsz, 3.0f);
                nvgFillColor(vg, checked ? tbl_accent() : (dark ? nvgRGBA(50,52,60,255) : nvgRGBA(235,236,240,255)));
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgRoundedRect(vg, bx, by, bsz, bsz, 3.0f);
                nvgStrokeColor(vg, checked ? nvgRGBA(40,100,220,255) : tbl_border(dark));
                nvgStrokeWidth(vg, 1.0f);
                nvgStroke(vg);
                if (checked) {
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, bx + 3.0f,  by + 7.5f);
                    nvgLineTo(vg, bx + 5.5f,  by + 10.0f);
                    nvgLineTo(vg, bx + 11.0f, by + 4.0f);
                    nvgStrokeColor(vg, nvgRGBA(255,255,255,255));
                    nvgStrokeWidth(vg, 2.0f);
                    nvgStroke(vg);
                }
                goto next_column;
            }

            // ─── Click detection (non-checkbox) ──────────────────────────
            if (wc->mouse_clicked && mx >= cx && mx < cx + cw_v
                && my >= ry && my < ry + row_h && !cell_active) {
                // Commit previous edit
                if (ui->table_state.is_editing_cell) {
                    aslan_table_set_cell(table, ui->table_state.active_col_idx,
                                        ui->table_state.select_popup_row,
                                        ui->table_state.rename_buffer);
                    aslan_table_eval_formulas(table);
                    ui->table_state.is_editing_cell = 0;
                }
                ui->table_state.active_block_id = block->id;

                if (col_ptr->type == ASLAN_COL_DATE) {
                    // Open date picker popup immediately
                    ui->table_state.active_col_idx = (int)c;
                    ui->table_state.select_popup_row = row_id;
                    ui->table_state.show_date_picker = 1;
                    ui->table_state.date_picker_col = (int)c;
                    ui->table_state.date_picker_row = row_id;
                    // Place popup near cell, clamped to window
                    float ppx = ui->mouse_x;
                    float ppy = ui->mouse_y + 4;
                    if (ppx + 220 > ui->win_width)  ppx = ui->win_width - 224;
                    if (ppy + 260 > ui->win_height) ppy = ui->mouse_y - 264;
                    ui->table_state.date_picker_x = ppx;
                    ui->table_state.date_picker_y = ppy;
                    wc->mouse_clicked = false; saved_clicked = false; // consume
                    ui_request_redraw(ui);
                } else if (col_ptr->type == ASLAN_COL_SELECT) {
                    // Open select popup immediately
                    ui->table_state.active_col_idx = (int)c;
                    ui->table_state.select_popup_row = row_id;
                    ui->table_state.show_select_popup = 1;
                    ui->table_state.select_popup_col = (int)c;
                    ui->table_state.editing_select_option = -1;
                    ui->table_state.option_buffer[0] = '\0';
                    float ppx = ui->mouse_x;
                    float ppy = ui->mouse_y + 4;
                    if (ppx + 180 > ui->win_width) ppx = ui->win_width - 184;
                    ui->table_state.select_popup_x = ppx;
                    ui->table_state.select_popup_y = ppy;
                    wc->mouse_clicked = false; saved_clicked = false; // consume
                    ui_request_redraw(ui);
                } else if (col_ptr->type == ASLAN_COL_FORMULA) {
                    // Open formula popup immediately
                    ui->table_state.show_formula_popup = 1;
                    ui->table_state.formula_popup_col = (int)c;
                    const char *f = aslan_table_get_formula(table, (int)c);
                    strncpy(ui->table_state.formula_buffer, f ? f : "",
                            sizeof(ui->table_state.formula_buffer) - 1);
                    wc->mouse_clicked = false; saved_clicked = false; // consume
                    ui_request_redraw(ui);
                } else {
                    // Text/Number/Link/File/Duration → inline edit
                    ui->table_state.active_col_idx = (int)c;
                    ui->table_state.select_popup_row = row_id;
                    ui->table_state.is_editing_cell = 1;
                    char cur[1024] = "";
                    aslan_table_get_cell_display(table, (int)c, row_id, cur, sizeof(cur));
                    strncpy(ui->table_state.rename_buffer, cur,
                            sizeof(ui->table_state.rename_buffer) - 1);
                    wc->focus_id = cell_id;
                    ui_request_redraw(ui);
                }
            }

            // ─── Inline editor ───────────────────────────────────────────
            if (cell_active
                && col_ptr->type != ASLAN_COL_DATE
                && col_ptr->type != ASLAN_COL_SELECT
                && col_ptr->type != ASLAN_COL_FORMULA
                && col_ptr->type != ASLAN_COL_CHECKBOX) {

                // Highlight active cell
                nvgBeginPath(vg);
                nvgRect(vg, cx + 1, ry + 1, cw_v - 2, row_h - 2);
                nvgFillColor(vg, tbl_sel(dark));
                nvgFill(vg);

                bool done = tbl_text_input(vg, wc, cell_id,
                                           cx, ry, cw_v, row_h,
                                           ui->table_state.rename_buffer,
                                           sizeof(ui->table_state.rename_buffer),
                                           13.0f, dark);
                if (done) {
                    aslan_table_set_cell(table, (int)c, row_id,
                                        ui->table_state.rename_buffer);
                    aslan_table_eval_formulas(table);
                    ui->table_state.is_editing_cell = 0;
                    // Advance to next row, same column (Enter)
                    size_t cnt2 = aslan_table_view_count(table);
                    int next_i = (int)i + 1;
                    if (next_i < (int)cnt2) {
                        uint64_t next_rid = aslan_table_view_row_id(table, next_i);
                        ui->table_state.active_col_idx = (int)c;
                        ui->table_state.select_popup_row = next_rid;
                        ui->table_state.is_editing_cell = 1;
                        aslan_table_get_cell_display(table, (int)c, next_rid,
                                                     ui->table_state.rename_buffer,
                                                     sizeof(ui->table_state.rename_buffer));
                        uint32_t nfid = nvgw_id(&next_rid, sizeof(next_rid))
                            ^ nvgw_id(&c, sizeof(c))
                            ^ nvgw_id(&block->id, sizeof(block->id));
                        wc->focus_id = nfid;
                    }
                    ui_request_redraw(ui);
                }
            } else if (!cell_active) {
                // ─── Read-only cell display ───────────────────────────────
                nvgSave(vg);
                nvgScissor(vg, cx + 4, ry, cw_v - 8, row_h);

                if (col_ptr->type == ASLAN_COL_SELECT) {
                    int sel = aslan_table_get_select(table, (int)c, row_id);
                    if (sel >= 0 && sel < col_ptr->option_count) {
                        AslanSelectOption *opt = &col_ptr->options[sel];
                        draw_select_badge(vg, cx, ry, cw_v, row_h,
                                          opt->label, opt->color, dark);
                    }
                } else {
                    char buf[1024];
                    aslan_table_get_cell_display(table, (int)c, row_id, buf, sizeof(buf));
                    nvgFontSize(vg, 13.0f);
                    nvgFontFace(vg, "Inter");
                    nvgFillColor(vg, tbl_text(dark));
                    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                    nvgText(vg, cx + 10, ry + row_h * 0.5f, buf, NULL);
                }

                nvgResetScissor(vg);
                nvgRestore(vg);
            }

            next_column:;
            // Column vertical separator in row
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx + cw_v, ry);
            nvgLineTo(vg, cx + cw_v, ry + row_h);
            nvgStrokeColor(vg, tbl_border(dark));
            nvgStrokeWidth(vg, 0.5f);
            nvgStroke(vg);
        }

        // Horizontal row separator
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, ry + row_h);
        nvgLineTo(vg, width, ry + row_h);
        nvgStrokeColor(vg, tbl_border(dark));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);

        // ── Row delete button (red X, shown when hovering left edge) ────────
        bool row_hovered_left = (mx >= -2 && mx < 22 && my >= ry && my < ry + row_h);
        bool row_hovered_any  = (mx >= 0  && mx < width && my >= ry && my < ry + row_h);
        if (row_hovered_any || row_hovered_left) {
            float bsz = 16.0f;
            float bx2 = 3.0f;
            float by2 = ry + (row_h - bsz) * 0.5f;

            // Fade in: full opacity when near left, dim when just in row
            float alpha = row_hovered_left ? 1.0f : (mx < 40 ? 0.5f : 0.18f);

            // Circle background
            nvgBeginPath(vg);
            nvgCircle(vg, bx2 + bsz * 0.5f, by2 + bsz * 0.5f, bsz * 0.5f);
            nvgFillColor(vg, nvgRGBAf(0.86f, 0.20f, 0.18f, alpha * 0.18f));
            nvgFill(vg);

            // ×  symbol
            NVGcolor x_col = nvgRGBAf(0.86f, 0.20f, 0.18f, alpha);
            float cx2 = bx2 + bsz * 0.5f;
            float cy2 = by2 + bsz * 0.5f;
            float arm = 3.6f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx2 - arm, cy2 - arm); nvgLineTo(vg, cx2 + arm, cy2 + arm);
            nvgMoveTo(vg, cx2 + arm, cy2 - arm); nvgLineTo(vg, cx2 - arm, cy2 + arm);
            nvgStrokeColor(vg, x_col);
            nvgStrokeWidth(vg, 1.8f);
            nvgStroke(vg);

            // Click to delete
            uint32_t del_id = nvgw_id(&row_id, sizeof(row_id)) ^ nvgw_id_str("row_del");
            float hmx = wc->mouse_x - wc->offset_x;
            float hmy = wc->mouse_y - wc->offset_y;
            bool del_hov = (hmx >= bx2 && hmx < bx2 + bsz
                         && hmy >= by2 && hmy < by2 + bsz);
            if (del_hov) {
                wc->hot_id = del_id;
                ui_set_cursor(ui, UI_CURSOR_HAND);
            }
            if (wc->active_id == del_id && wc->mouse_released && del_hov) {
                aslan_table_delete_row(table, row_id);
                aslan_table_eval_formulas(table);
                // Clear any editing state that pointed to this row
                if (ui->table_state.select_popup_row == row_id) {
                    ui->table_state.is_editing_cell = 0;
                    ui->table_state.show_select_popup = 0;
                    ui->table_state.show_date_picker  = 0;
                }
                ui_request_redraw(ui);
            }
            if (wc->hot_id == del_id && wc->mouse_clicked) wc->active_id = del_id;
        }
    }

    wc->mouse_clicked = saved_clicked;
    nvgw_end_scroll_area(wc);

    // Scrollbar indicator (when content taller than visible area)
    if (content_h > rows_area_h) {
        float sb_x = width - TBL_SCROLLBAR_W - 2;
        float sb_ratio = rows_area_h / content_h;
        float sb_h = rows_area_h * sb_ratio;
        float sb_y = header_h + ui->table_state.row_scroll_y * (rows_area_h / content_h);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, sb_x, sb_y, TBL_SCROLLBAR_W - 2, sb_h, 4.0f);
        nvgFillColor(vg, dark ? nvgRGBA(180,182,195,80) : nvgRGBA(0,0,0,50));
        nvgFill(vg);
    }
}

// -----------------------------------------------------------------------------
// Popups
// -----------------------------------------------------------------------------

static void render_context_menu(UIHost *ui, AslanTable *table, Block *block) {
    if (!ui->show_context_menu) return;
    if (ui->context_menu_node) return; // node context menu takes priority
    if (ui->table_state.active_block_id != block->id) return;

    NVGcontext *vg = ui->vg;
    NvgWidgetCtx *wc = &ui->nvgw;
    int dark = ui->dark_mode;

    int is_col = (ui->table_state.context_menu_mode == 1);
    float menu_w = 160.0f, item_h = 30.0f;

    int item_cnt = is_col ? 2 : 2;
    if (is_col) {
        AslanColumn *cobj = &table->columns[ui->table_state.context_menu_col];
        if (cobj->type == ASLAN_COL_FORMULA) item_cnt = 3;
    }
    float menu_h = item_cnt * item_h + 12.0f;

    // Position clamp
    float mx = ui->context_menu_x - ui->abs_offset_x;
    float my = ui->context_menu_y - ui->abs_offset_y;
    if (mx + menu_w > (float)ui->win_width - ui->abs_offset_x)
        mx = (float)ui->win_width - ui->abs_offset_x - menu_w - 4;

    // Panel
    NVGcolor bg = dark ? nvgRGBA(38,40,46,255) : nvgRGBA(255,255,255,255);
    nvgw_panel(wc, mx, my, menu_w, menu_h, bg, 8.0f);
    // Border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, mx, my, menu_w, menu_h, 8.0f);
    nvgStrokeColor(vg, dark ? nvgRGBA(58,60,70,255) : nvgRGBA(218,220,228,255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    const char *col_items[] = {"Rename Column", "Delete Column", "Edit Formula"};
    const char *row_items[] = {"Duplicate Row",  "Delete Row"};
    const char **items = is_col ? col_items : row_items;

    for (int m = 0; m < item_cnt; m++) {
        float iy = my + 6 + m * item_h;
        uint32_t mid = nvgw_id(&m, sizeof(m)) ^ nvgw_id_str("tbl_ctx");
        if (tbl_item_button(vg, wc, mid, mx + 4, iy, menu_w - 8, item_h, items[m], dark)) {
            if (is_col) {
                if (m == 0) {
                    // Rename column
                    ui->table_state.renaming_col_idx = ui->table_state.context_menu_col;
                    strncpy(ui->table_state.rename_buffer,
                            table->columns[ui->table_state.context_menu_col].name,
                            sizeof(ui->table_state.rename_buffer) - 1);
                } else if (m == 1) {
                    aslan_table_remove_column(table, ui->table_state.context_menu_col);
                } else if (m == 2) {
                    ui->table_state.show_formula_popup = 1;
                    ui->table_state.formula_popup_col = ui->table_state.context_menu_col;
                    const char *f = aslan_table_get_formula(table, ui->table_state.formula_popup_col);
                    strncpy(ui->table_state.formula_buffer, f ? f : "",
                            sizeof(ui->table_state.formula_buffer) - 1);
                }
            } else {
                if (m == 0) aslan_table_duplicate_row(table, ui->table_state.context_menu_row);
                else if (m == 1) aslan_table_delete_row(table, ui->table_state.context_menu_row);
            }
            ui->show_context_menu = 0;
            ui_request_redraw(ui);
        }
    }

    // Close on click outside
    if (wc->mouse_clicked) {
        float hmx = wc->mouse_x - wc->offset_x;
        float hmy = wc->mouse_y - wc->offset_y;
        bool in_menu = (hmx >= mx && hmx < mx + menu_w && hmy >= my && hmy < my + menu_h);
        if (!in_menu) ui->show_context_menu = 0;
    }
}

static void render_type_menu(UIHost *ui, AslanTable *table, Block *block) {
    if (!ui->table_state.show_type_menu) return;
    if (ui->table_state.active_block_id != block->id) return;

    NVGcontext *vg = ui->vg;
    NvgWidgetCtx *wc = &ui->nvgw;
    int dark = ui->dark_mode;

    const char *type_names[] = {
        "Text", "Number", "Checkbox", "Select", "Date",
        "Formula", "File", "Link", "Image", "Duration"
    };
    AslanColumnType types[] = {
        ASLAN_COL_TEXT, ASLAN_COL_NUMBER, ASLAN_COL_CHECKBOX,
        ASLAN_COL_SELECT, ASLAN_COL_DATE, ASLAN_COL_FORMULA,
        ASLAN_COL_FILE, ASLAN_COL_LINK, ASLAN_COL_IMAGE, ASLAN_COL_DURATION
    };
    int item_cnt = 10;
    float menu_w = 170.0f, item_h = 30.0f;
    float menu_h = item_cnt * item_h + 12.0f;

    float mx = ui->table_state.type_menu_x - ui->abs_offset_x;
    float my = ui->table_state.type_menu_y - ui->abs_offset_y;
    if (my + menu_h > (float)ui->win_height - ui->abs_offset_y)
        my = (float)ui->win_height - ui->abs_offset_y - menu_h - 4;

    NVGcolor bg = dark ? nvgRGBA(38,40,46,255) : nvgRGBA(255,255,255,255);
    nvgw_panel(wc, mx, my, menu_w, menu_h, bg, 8.0f);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, mx, my, menu_w, menu_h, 8.0f);
    nvgStrokeColor(vg, dark ? nvgRGBA(58,60,70,255) : nvgRGBA(218,220,228,255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    for (int m = 0; m < item_cnt; m++) {
        float iy = my + 6 + m * item_h;
        // Icon
        nvgFontSize(vg, 14.0f);
        nvgFontFace(vg, "Icons");
        nvgFillColor(vg, tbl_dim(dark));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, mx + 10, iy + item_h * 0.5f,
                table_col_type_icon(types[m]), NULL);

        uint32_t mid = nvgw_id(&m, sizeof(m)) ^ nvgw_id_str("type_menu");
        // Use tbl_item_button but shift text right for icon space
        float tx = mx + 28, tw = menu_w - 32;
        float hmx = wc->mouse_x - wc->offset_x;
        float hmy = wc->mouse_y - wc->offset_y;
        bool hov = hmx >= mx + 4 && hmx < mx + menu_w - 4 && hmy >= iy && hmy < iy + item_h;
        if (hov) wc->hot_id = mid;
        bool clicked = (wc->active_id == mid && wc->mouse_released && hov);
        if (wc->hot_id == mid && wc->mouse_clicked) wc->active_id = mid;
        if (hov) {
            nvgBeginPath(vg);
            nvgRoundedRect(vg, mx + 4, iy, menu_w - 8, item_h, 4.0f);
            nvgFillColor(vg, dark ? nvgRGBA(255,255,255,10) : nvgRGBA(0,0,0,6));
            nvgFill(vg);
        }
        nvgFontSize(vg, 13.0f);
        nvgFontFace(vg, "Inter");
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, tbl_text(dark));
        nvgText(vg, tx, iy + item_h * 0.5f, type_names[m], NULL);
        (void)tw;

        if (clicked) {
            // Generate unique column name (e.g. "Number 2", "Number 3")
            char col_name[64];
            strncpy(col_name, type_names[m], sizeof(col_name) - 1);
            col_name[sizeof(col_name) - 1] = '\0';
            if (aslan_table_find_column(table, col_name) >= 0) {
                for (int n = 2; n < 100; n++) {
                    snprintf(col_name, sizeof(col_name), "%s %d", type_names[m], n);
                    if (aslan_table_find_column(table, col_name) < 0) break;
                }
            }
            aslan_table_add_column(table, col_name, types[m]);
            if (types[m] == ASLAN_COL_SELECT) {
                int cidx = (int)table->column_count - 1;
                aslan_table_add_option(table, cidx, "Option 1", 0x3498dbFF);
                aslan_table_add_option(table, cidx, "Option 2", 0x2ecc71FF);
                aslan_table_add_option(table, cidx, "Option 3", 0xe67e22FF);
            }
            ui->table_state.show_type_menu = 0;
            ui_request_redraw(ui);
        }
    }

    // Close on outside click
    if (wc->mouse_clicked) {
        float hmx = wc->mouse_x - wc->offset_x;
        float hmy = wc->mouse_y - wc->offset_y;
        bool in_menu = (hmx >= mx && hmx < mx + menu_w && hmy >= my && hmy < my + menu_h);
        if (!in_menu) ui->table_state.show_type_menu = 0;
    }
}

static void render_select_popup(UIHost *ui, AslanTable *table, Block *block) {
    if (!ui->table_state.show_select_popup) return;
    if (ui->table_state.active_block_id != block->id) return;

    NVGcontext *vg = ui->vg;
    NvgWidgetCtx *wc = &ui->nvgw;
    int dark = ui->dark_mode;

    int cidx = ui->table_state.select_popup_col;
    uint64_t rrid = ui->table_state.select_popup_row;
    AslanColumn *col_ptr = &table->columns[cidx];

    float menu_w = 180.0f, item_h = 30.0f;
    int item_cnt = col_ptr->option_count;
    float menu_h = (item_cnt > 0 ? item_cnt : 1) * item_h + 12.0f + 44.0f;

    // Use absolute (screen) coords — popup floats above everything
    nvgw_set_offset(wc, 0, 0);
    float mx_s = ui->table_state.select_popup_x;
    float my_s = ui->table_state.select_popup_y;
    if (my_s + menu_h > (float)ui->win_height) my_s = (float)ui->win_height - menu_h - 4;

    NVGcolor bg = dark ? nvgRGBA(38,40,46,255) : nvgRGBA(255,255,255,255);
    nvgw_panel(wc, mx_s, my_s, menu_w, menu_h, bg, 8.0f);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, mx_s, my_s, menu_w, menu_h, 8.0f);
    nvgStrokeColor(vg, dark ? nvgRGBA(58,60,70,255) : nvgRGBA(218,220,228,255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Shadow
    NVGpaint shadow = nvgBoxGradient(vg, mx_s, my_s + 2, menu_w, menu_h,
                                     8.0f, 10.0f, nvgRGBA(0,0,0,40), nvgRGBA(0,0,0,0));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, mx_s - 10, my_s - 10, menu_w + 20, menu_h + 20, 8.0f);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);

    float cur_y = my_s + 6;
    int cur_sel = aslan_table_get_select(table, cidx, rrid);

    if (item_cnt == 0) {
        nvgFontSize(vg, 13.0f); nvgFontFace(vg, "Inter");
        nvgFillColor(vg, tbl_dim(dark));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, mx_s + menu_w * 0.5f, cur_y + item_h * 0.5f, "No options yet", NULL);
        cur_y += item_h;
    } else {
        for (int m = 0; m < item_cnt; m++) {
            AslanSelectOption *opt = &col_ptr->options[m];
            float iy = cur_y;
            bool is_cur = (m == cur_sel);

            if (ui->table_state.editing_select_option == m) {
                // Inline rename editor
                uint32_t inp_id = nvgw_id_str("edit_opt") ^ nvgw_id(&m, sizeof(m));
                tbl_text_input(vg, wc, inp_id, mx_s + 4, iy + 2, menu_w - 44, item_h - 4,
                               ui->table_state.option_buffer,
                               sizeof(ui->table_state.option_buffer), 13.0f, dark);
                // OK button
                uint32_t ok_id = nvgw_id_str("ok_opt") ^ nvgw_id(&m, sizeof(m));
                if (tbl_item_button(vg, wc, ok_id, mx_s + menu_w - 38, iy + 2, 34, item_h - 4, "OK", dark)) {
                    if (strlen(ui->table_state.option_buffer) == 0)
                        aslan_table_remove_option(table, cidx, m);
                    else
                        aslan_table_rename_option(table, cidx, m, ui->table_state.option_buffer);
                    ui->table_state.editing_select_option = -1;
                    ui->table_state.option_buffer[0] = '\0';
                    ui_request_redraw(ui);
                }
            } else {
                uint32_t mid = nvgw_id(&m, sizeof(m)) ^ nvgw_id_str("sel_opt");
                bool hov = (wc->mouse_x >= mx_s + 4 && wc->mouse_x < mx_s + menu_w - 4
                         && wc->mouse_y >= iy && wc->mouse_y < iy + item_h);
                if (hov) wc->hot_id = mid;
                bool clicked = (wc->active_id == mid && wc->mouse_released && hov);
                if (wc->hot_id == mid && wc->mouse_clicked) wc->active_id = mid;

                // Row bg
                if (is_cur || hov) {
                    nvgBeginPath(vg);
                    nvgRoundedRect(vg, mx_s + 4, iy, menu_w - 8, item_h, 4.0f);
                    nvgFillColor(vg, is_cur
                        ? nvgRGBA(60,120,255,30)
                        : (dark ? nvgRGBA(255,255,255,8) : nvgRGBA(0,0,0,5)));
                    nvgFill(vg);
                }

                // Colored dot
                float r = (opt->color >> 24) & 0xFF;
                float g = (opt->color >> 16) & 0xFF;
                float b = (opt->color >>  8) & 0xFF;
                nvgBeginPath(vg);
                nvgCircle(vg, mx_s + 18, iy + item_h * 0.5f, 5.0f);
                nvgFillColor(vg, nvgRGBA((int)r,(int)g,(int)b,220));
                nvgFill(vg);

                // Label
                nvgFontSize(vg, 13.0f); nvgFontFace(vg, "Inter");
                nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, tbl_text(dark));
                nvgText(vg, mx_s + 30, iy + item_h * 0.5f, opt->label, NULL);

                // Checkmark if selected
                if (is_cur) {
                    nvgFontSize(vg, 14.0f); nvgFontFace(vg, "Icons");
                    nvgFillColor(vg, tbl_accent());
                    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
                    nvgText(vg, mx_s + menu_w - 8, iy + item_h * 0.5f, "\xEE\xAD\xB5", NULL);
                }

                if (clicked) {
                    uint32_t now = SDL_GetTicks();
                    if (ui->table_state.last_option_click_idx == m
                        && (now - ui->table_state.last_option_click_time) < 400) {
                        // Double-click → edit
                        ui->table_state.editing_select_option = m;
                        strncpy(ui->table_state.option_buffer, opt->label,
                                sizeof(ui->table_state.option_buffer) - 1);
                        ui->table_state.last_option_click_idx = -1;
                    } else {
                        // Single-click → select
                        aslan_table_set_select(table, cidx, rrid, m);
                        aslan_table_eval_formulas(table);
                        ui->table_state.last_option_click_idx = m;
                        ui->table_state.last_option_click_time = now;
                    }
                    ui_request_redraw(ui);
                }
            }
            cur_y += item_h;
        }
    }

    // Divider
    cur_y += 2;
    nvgBeginPath(vg);
    nvgMoveTo(vg, mx_s + 8, cur_y);
    nvgLineTo(vg, mx_s + menu_w - 8, cur_y);
    nvgStrokeColor(vg, tbl_border(dark));
    nvgStrokeWidth(vg, 0.5f);
    nvgStroke(vg);
    cur_y += 4;

    // Add new option row
    uint32_t opt_in_id = nvgw_id_str("opt_in") ^ nvgw_id(&cidx, sizeof(cidx));
    tbl_text_input(vg, wc, opt_in_id, mx_s + 4, cur_y, menu_w - 44, 32,
                   ui->table_state.option_buffer,
                   sizeof(ui->table_state.option_buffer), 13.0f, dark);
    uint32_t opt_add_id = nvgw_id_str("opt_add") ^ nvgw_id(&cidx, sizeof(cidx));
    if (tbl_item_button(vg, wc, opt_add_id, mx_s + menu_w - 38, cur_y, 34, 32, "+", dark)) {
        if (strlen(ui->table_state.option_buffer) > 0) {
            // Cycle through pleasant colors
            static const uint32_t pal[] = {
                0x3498dbFF, 0x2ecc71FF, 0xe67e22FF, 0x9b59b6FF, 0xe74c3cFF,
                0x1abc9cFF, 0xf39c12FF, 0x2980b9FF
            };
            uint32_t color = pal[col_ptr->option_count % 8];
            aslan_table_add_option(table, cidx, ui->table_state.option_buffer, color);
            ui->table_state.option_buffer[0] = '\0';
            ui_request_redraw(ui);
        }
    }

    // Close on outside click
    if (wc->mouse_clicked) {
        bool in_menu = (wc->mouse_x >= mx_s && wc->mouse_x < mx_s + menu_w
                     && wc->mouse_y >= my_s && wc->mouse_y < my_s + menu_h);
        if (!in_menu) {
            ui->table_state.show_select_popup = 0;
            ui->table_state.is_editing_cell = 0;
        }
    }

    nvgw_set_offset(wc, ui->abs_offset_x, ui->abs_offset_y); // restore
}

static void render_date_popup(UIHost *ui, AslanTable *table, Block *block) {
    if (!ui->table_state.show_date_picker) return;
    if (ui->table_state.active_block_id != block->id) return;

    NVGcontext *vg = ui->vg;
    NvgWidgetCtx *wc = &ui->nvgw;
    int dark = ui->dark_mode;

    int cidx = ui->table_state.date_picker_col;
    uint64_t rrid = ui->table_state.date_picker_row;

    float menu_w = 220.0f, menu_h = 260.0f;
    nvgw_set_offset(wc, 0, 0);
    float px = ui->table_state.date_picker_x;
    float py = ui->table_state.date_picker_y;
    if (px + menu_w > (float)ui->win_width)  px = (float)ui->win_width  - menu_w - 4;
    if (py + menu_h > (float)ui->win_height) py = (float)ui->win_height - menu_h - 4;

    NVGcolor bg = dark ? nvgRGBA(38,40,46,255) : nvgRGBA(255,255,255,255);
    nvgw_panel(wc, px, py, menu_w, menu_h, bg, 10.0f);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, px, py, menu_w, menu_h, 10.0f);
    nvgStrokeColor(vg, dark ? nvgRGBA(58,60,70,255) : nvgRGBA(218,220,228,255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Fetch current date
    int64_t ts = aslan_table_get_date(table, cidx, rrid);
    int year = 2024, month = 0, day = 1;
    if (ts > 0) {
        time_t epoch = (time_t)(ts / 1000);
        struct tm *t = localtime(&epoch);
        if (t) { year = t->tm_year + 1900; month = t->tm_mon; day = t->tm_mday; }
    }
    // Store in persistent picker state
    if (ui->table_state.date_picker_year > 0) {
        year  = ui->table_state.date_picker_year;
        month = ui->table_state.date_picker_month;
    }

    // Title
    static const char *months[] = {"January","February","March","April","May","June",
                                    "July","August","September","October","November","December"};
    char title[64];
    snprintf(title, sizeof(title), "%s %d", months[month], year);
    nvgFontSize(vg, 14.0f); nvgFontFace(vg, "Inter");
    nvgFillColor(vg, tbl_text(dark));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, px + menu_w * 0.5f, py + 20, title, NULL);

    // Month navigation
    uint32_t b_prev = nvgw_id_str("dp_prev"), b_next = nvgw_id_str("dp_next");
    bool changed = false;
    if (tbl_item_button(vg, wc, b_prev, px + 6,  py + 8, 30, 24, "<", dark)) { month--; changed = true; }
    if (tbl_item_button(vg, wc, b_next, px + menu_w - 36, py + 8, 30, 24, ">", dark)) { month++; changed = true; }
    if (changed) {
        if (month < 0) { month = 11; year--; }
        if (month > 11) { month = 0; year++; }
        ui->table_state.date_picker_year  = year;
        ui->table_state.date_picker_month = month;
        ui_request_redraw(ui);
    }

    // Day-of-week header
    static const char *dow[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
    float cell_w = (menu_w - 12) / 7.0f;
    float grid_x = px + 6;
    float grid_y = py + 38;
    nvgFontSize(vg, 11.0f); nvgFontFace(vg, "Inter");
    nvgFillColor(vg, tbl_dim(dark));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    for (int d = 0; d < 7; d++) {
        nvgText(vg, grid_x + d * cell_w + cell_w * 0.5f, grid_y + 10, dow[d], NULL);
    }
    grid_y += 22;

    // First weekday of month
    struct tm tm0 = {0};
    tm0.tm_year = year - 1900; tm0.tm_mon = month; tm0.tm_mday = 1;
    tm0.tm_hour = 12; tm0.tm_isdst = -1;
    mktime(&tm0);
    int first_wday = tm0.tm_wday;

    // Days in month
    static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int days_in_month = mdays[month];
    if (month == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
        days_in_month = 29;

    float cell_h = 26.0f;
    bool day_changed = false;
    for (int d = 1; d <= days_in_month; d++) {
        int slot = (d - 1) + first_wday;
        int row2 = slot / 7, col2 = slot % 7;
        float dx = grid_x + col2 * cell_w;
        float dy = grid_y + row2 * cell_h;
        bool is_sel = (d == day);
        bool is_today = false;
        {
            time_t now2 = time(NULL);
            struct tm *tn = localtime(&now2);
            is_today = tn && (tn->tm_year + 1900 == year)
                          && (tn->tm_mon == month) && (tn->tm_mday == d);
        }
        bool hov = (wc->mouse_x >= dx && wc->mouse_x < dx + cell_w
                 && wc->mouse_y >= dy && wc->mouse_y < dy + cell_h);
        uint32_t did = nvgw_id_str("dp_day") ^ nvgw_id(&d, sizeof(d));
        if (hov) wc->hot_id = did;
        bool dcl = (wc->active_id == did && wc->mouse_released && hov);
        if (wc->hot_id == did && wc->mouse_clicked) wc->active_id = did;

        if (is_sel) {
            nvgBeginPath(vg);
            nvgCircle(vg, dx + cell_w * 0.5f, dy + cell_h * 0.5f, cell_h * 0.45f);
            nvgFillColor(vg, tbl_accent());
            nvgFill(vg);
        } else if (hov) {
            nvgBeginPath(vg);
            nvgCircle(vg, dx + cell_w * 0.5f, dy + cell_h * 0.5f, cell_h * 0.42f);
            nvgFillColor(vg, dark ? nvgRGBA(255,255,255,15) : nvgRGBA(0,0,0,8));
            nvgFill(vg);
        } else if (is_today) {
            nvgBeginPath(vg);
            nvgCircle(vg, dx + cell_w * 0.5f, dy + cell_h * 0.5f, cell_h * 0.42f);
            nvgStrokeColor(vg, tbl_accent());
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
        }

        nvgFontSize(vg, 12.0f); nvgFontFace(vg, "Inter");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, is_sel ? nvgRGBA(255,255,255,255) : tbl_text(dark));
        char ds[4]; snprintf(ds, sizeof(ds), "%d", d);
        nvgText(vg, dx + cell_w * 0.5f, dy + cell_h * 0.5f, ds, NULL);

        if (dcl) {
            day = d;
            day_changed = true;
        }
    }

    if (day_changed) {
        struct tm t = {0};
        t.tm_year = year - 1900; t.tm_mon = month; t.tm_mday = day;
        t.tm_hour = 12; t.tm_isdst = -1;
        int64_t new_ts = (int64_t)mktime(&t) * 1000LL;
        aslan_table_set_date(table, cidx, rrid, new_ts);
        aslan_table_eval_formulas(table);
        ui->table_state.date_picker_year  = year;
        ui->table_state.date_picker_month = month;
        ui_request_redraw(ui);
    }

    // Confirm + Clear buttons at bottom
    float btn_y = py + menu_h - 36;
    uint32_t b_ok  = nvgw_id_str("dp_ok");
    uint32_t b_clr = nvgw_id_str("dp_clear");
    float half_w = (menu_w - 18) * 0.5f;
    if (tbl_item_button(vg, wc, b_clr, px + 6,          btn_y, half_w, 28, "Clear", dark)) {
        aslan_table_set_date(table, cidx, rrid, 0);
        aslan_table_eval_formulas(table);
        ui->table_state.show_date_picker = 0;
        ui->table_state.date_picker_year = 0;
        ui_request_redraw(ui);
    }
    if (tbl_item_button(vg, wc, b_ok,  px + 9 + half_w, btn_y, half_w, 28, "Confirm", dark)) {
        ui->table_state.show_date_picker = 0;
        ui->table_state.date_picker_year = 0;
        ui_request_redraw(ui);
    }

    // Close on outside click
    if (wc->mouse_clicked) {
        bool in_menu = (wc->mouse_x >= px && wc->mouse_x < px + menu_w
                     && wc->mouse_y >= py && wc->mouse_y < py + menu_h);
        if (!in_menu) {
            ui->table_state.show_date_picker = 0;
            ui->table_state.date_picker_year = 0;
        }
    }

    nvgw_set_offset(wc, ui->abs_offset_x, ui->abs_offset_y);
}

static void render_formula_popup(UIHost *ui, Engine *engine,
                                 AslanTable *table, Block *block) {
    if (!ui->table_state.show_formula_popup) return;
    if (ui->table_state.active_block_id != block->id) return;

    NVGcontext *vg = ui->vg;
    NvgWidgetCtx *wc = &ui->nvgw;
    int dark = ui->dark_mode;

    nvgw_set_offset(wc, 0, 0);

    float fw = 440.0f, fh = 140.0f;
    float fpx = (ui->win_width  - fw) * 0.5f;
    float fpy = (ui->win_height - fh) * 0.5f;

    // Backdrop (click-blocker)
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, (float)ui->win_width, (float)ui->win_height);
    nvgFillColor(vg, nvgRGBA(0,0,0,50));
    nvgFill(vg);

    NVGcolor bg = dark ? nvgRGBA(38,40,46,255) : nvgRGBA(255,255,255,255);
    nvgw_panel(wc, fpx, fpy, fw, fh, bg, 10.0f);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, fpx, fpy, fw, fh, 10.0f);
    nvgStrokeColor(vg, dark ? nvgRGBA(70,72,85,255) : nvgRGBA(210,212,220,255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Title
    nvgFontSize(vg, 15.0f); nvgFontFace(vg, "Inter");
    nvgFillColor(vg, tbl_text(dark));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(vg, fpx + 16, fpy + 14, "Edit Formula", NULL);

    // Hint
    nvgFontSize(vg, 11.5f);
    nvgFillColor(vg, tbl_dim(dark));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(vg, fpx + 16, fpy + 34, "Use [ColumnName] to reference columns. Click a column header to insert.", NULL);

    // Formula input
    uint32_t inp_id = nvgw_id_str("formula_input");
    bool done = tbl_text_input(vg, wc, inp_id, fpx + 16, fpy + 54, fw - 32, 30,
                               ui->table_state.formula_buffer,
                               sizeof(ui->table_state.formula_buffer), 13.5f, dark);

    // Force focus on first frame
    if (wc->focus_id != inp_id) wc->focus_id = inp_id;

    // Cancel / Apply buttons
    uint32_t b_cancel = nvgw_id_str("formula_cancel");
    uint32_t b_apply  = nvgw_id_str("formula_apply");
    float btn_y = fpy + fh - 38;
    if (tbl_item_button(vg, wc, b_cancel, fpx + fw - 180, btn_y, 80, 28, "Cancel", dark)) {
        ui->table_state.show_formula_popup = 0;
        wc->focus_id = 0;
        wc->active_id = 0;
        ui_request_redraw(ui);
    }
    if (tbl_item_button(vg, wc, b_apply, fpx + fw - 94, btn_y, 78, 28, "Apply", dark)) {
        done = true;
    }

    if (done) {
        aslan_table_set_formula(table, ui->table_state.formula_popup_col,
                                ui->table_state.formula_buffer);
        aslan_table_eval_formulas(table);
        char cmd[1200];
        Page *pg = doc_page_get_active(engine->doc);
        snprintf(cmd, sizeof(cmd), "block_table_col_formula %u %u %d \"%s\"",
                 pg ? pg->id : 1, block->id,
                 ui->table_state.formula_popup_col, ui->table_state.formula_buffer);
        engine_execute_command(engine, cmd);
        ui->table_state.show_formula_popup = 0;
        wc->focus_id = 0;
        wc->active_id = 0;
        ui_request_redraw(ui);
    }

    nvgw_set_offset(wc, ui->abs_offset_x, ui->abs_offset_y);
}

// -----------------------------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------------------------

void ui_render_block_table(UIHost *ui, Engine *engine, Block *block,
                           float width, float height) {
    if (!ui || !engine || !block || block->type != BLOCK_TABLE) return;
    AslanTable *table = block->data.table.columnar;
    if (!table) return;

    NVGcontext *vg = ui->vg;
    int dark = ui->dark_mode;
    NvgWidgetCtx *wc = &ui->nvgw;
    nvgw_set_offset(wc, ui->abs_offset_x, ui->abs_offset_y);

    // Reset column resize state if mouse released globally
    if (ui->table_state.resizing_col >= 0 && !(ui->mouse_buttons & 1)) {
        ui->table_state.resizing_col = -1;
    }

    // Escape: cancel inline rename
    // (Escape is consumed in the event handler; here we check focus change)
    if (ui->table_state.renaming_col_idx >= 0) {
        uint32_t ren_id = nvgw_id_str("hdr_rename")
            ^ nvgw_id(&ui->table_state.renaming_col_idx,
                      sizeof(ui->table_state.renaming_col_idx));
        if (wc->mouse_clicked) {
            float hmx = wc->mouse_x - wc->offset_x;
            float hmy = wc->mouse_y - wc->offset_y;
            float cx = col_x(ui, table, ui->table_state.renaming_col_idx);
            float cw_v = col_w(ui, ui->table_state.renaming_col_idx);
            bool in_hdr = hmx >= cx && hmx < cx + cw_v && hmy >= 0 && hmy < TBL_HEADER_H;
            if (!in_hdr) {
                // Commit rename on outside click
                if (strlen(ui->table_state.rename_buffer) > 0) {
                    aslan_table_rename_column(table, ui->table_state.renaming_col_idx,
                                             ui->table_state.rename_buffer);
                }
                ui->table_state.renaming_col_idx = -1;
            }
        }
        (void)ren_id;
    }

    // Commit cell edit on click outside table
    if (ui->table_state.is_editing_cell && ui->table_state.active_block_id == block->id) {
        float hmx = wc->mouse_x - wc->offset_x;
        float hmy = wc->mouse_y - wc->offset_y;
        if (wc->mouse_clicked && (hmx < 0 || hmx > width || hmy < 0 || hmy > height)) {
            aslan_table_set_cell(table, ui->table_state.active_col_idx,
                                 ui->table_state.select_popup_row,
                                 ui->table_state.rename_buffer);
            aslan_table_eval_formulas(table);
            ui->table_state.is_editing_cell = 0;
            ui_request_redraw(ui);
        }
    }

    // ── Overall background ────────────────────────────────────────────────────
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0, 0, width, height, 6.0f);
    nvgFillColor(vg, tbl_bg(dark));
    nvgFill(vg);

    // ── Outer border ──────────────────────────────────────────────────────────
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.5f, 0.5f, width - 1, height - 1, 6.0f);
    nvgStrokeColor(vg, tbl_border(dark));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // ── Clip content ─────────────────────────────────────────────────────────
    nvgSave(vg);
    nvgScissor(vg, 0, 0, width, height);

    // ── Header ────────────────────────────────────────────────────────────────
    render_header(ui, engine, table, block, width, TBL_HEADER_H);

    // ── Rows ──────────────────────────────────────────────────────────────────
    render_rows(ui, engine, table, block, width, height, TBL_HEADER_H, TBL_ROW_H);

    nvgRestore(vg);

    // ── Popups (rendered last, float above clipped content) ─────────────────
    // NOTE: popups use absolute screen coords so nvgSave/Restore is needed
    nvgSave(vg);
    render_context_menu(ui, table, block);
    render_type_menu(ui, table, block);
    render_select_popup(ui, table, block);
    render_date_popup(ui, table, block);
    render_formula_popup(ui, engine, table, block);
    nvgRestore(vg);
}

// -----------------------------------------------------------------------------
// Column type helpers
// -----------------------------------------------------------------------------

const char *table_col_type_icon(AslanColumnType type) {
    switch (type) {
    case ASLAN_COL_TEXT:     return ICON_COL_TEXT;
    case ASLAN_COL_NUMBER:   return ICON_COL_NUMBER;
    case ASLAN_COL_CHECKBOX: return ICON_COL_CHECKBOX;
    case ASLAN_COL_SELECT:   return ICON_COL_SELECT;
    case ASLAN_COL_DATE:     return ICON_COL_DATE;
    case ASLAN_COL_FORMULA:  return ICON_COL_FORMULA;
    case ASLAN_COL_FILE:     return ICON_COL_FILE;
    case ASLAN_COL_IMAGE:    return ICON_COL_IMAGE;
    case ASLAN_COL_LINK:     return ICON_COL_LINK;
    case ASLAN_COL_DURATION: return "\xEF\x8C\xB8";
    default:                 return ICON_COL_TEXT;
    }
}

const char *table_col_type_name(AslanColumnType type) {
    switch (type) {
    case ASLAN_COL_TEXT:     return "Text";
    case ASLAN_COL_NUMBER:   return "Number";
    case ASLAN_COL_CHECKBOX: return "Checkbox";
    case ASLAN_COL_SELECT:   return "Select";
    case ASLAN_COL_DATE:     return "Date";
    case ASLAN_COL_FORMULA:  return "Formula";
    case ASLAN_COL_FILE:     return "File";
    case ASLAN_COL_IMAGE:    return "Image";
    case ASLAN_COL_LINK:     return "Link";
    case ASLAN_COL_DURATION: return "Duration";
    default:                 return "Text";
    }
}
