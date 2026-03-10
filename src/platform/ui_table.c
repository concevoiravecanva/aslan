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

static inline int tbl_mouse_in(UIHost *ui, float x, float y, float w, float h) {
    float mx = ui->nvgw.mouse_x, my = ui->nvgw.mouse_y;
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

void ui_render_block_table(UIHost *ui, Engine *engine, Block *block,
                           float width, float height) {
    if (!ui || !engine || !block || block->type != BLOCK_TABLE) return;

    AslanTable *table = block->data.table.columnar;
    if (!table) return;

    NVGcontext *vg = ui->vg;
    float row_h = 32.0f;
    float header_h = 36.0f;
    float col_w = width / (float)table->column_count;
    if (col_w < 100.0f) col_w = 120.0f;

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
    }

    // Horizontal line after header
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, header_h);
    nvgLineTo(vg, width, header_h);
    nvgStrokeColor(vg, ui->dark_mode ? nvgRGBA(80, 80, 85, 255) : nvgRGBA(200, 200, 205, 255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Rows
    size_t count = aslan_table_view_count(table);
    for (size_t i = 0; i < count; i++) {
        float ry = header_h + (float)i * row_h;
        if (ry > height) break;

        uint64_t row_id = aslan_table_view_row_id(table, (int)i);

        // Striping
        if (i % 2 == 1) {
            nvgBeginPath(vg);
            nvgRect(vg, 0, ry, width, row_h);
            nvgFillColor(vg, ui->dark_mode ? nvgRGBA(255, 255, 255, 5) : nvgRGBA(0, 0, 0, 5));
            nvgFill(vg);
        }

        for (size_t c = 0; c < table->column_count; c++) {
            float cx = (float)c * col_w;
            char buf[1024];
            aslan_table_get_cell_display(table, (int)c, row_id, buf, sizeof(buf));

            nvgFontSize(vg, 14.0f);
            nvgFontFace(vg, "Inter");
            nvgFillColor(vg, ui->dark_mode ? nvgRGBA(200, 200, 210, 255) : nvgRGBA(60, 60, 65, 255));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, cx + 10, ry + row_h / 2, buf, NULL);
        }

        // Horizontal line
        nvgBeginPath(vg);
        nvgMoveTo(vg, 0, ry + row_h);
        nvgLineTo(vg, width, ry + row_h);
        nvgStrokeColor(vg, ui->dark_mode ? nvgRGBA(60, 60, 65, 255) : nvgRGBA(230, 230, 235, 255));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);
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
