#include "../core/engine.h"
#include "../document/aslan_table.h"
#include "../document/document.h"
#include "ui_core.h"
#include "ui_host.h"
#include "ui_shared.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

#define CHART_MAX_POINTS 128

static const uint32_t chart_palette[] = {
    0x3778FAFF, // Blue
    0xF5734AFF, // Orange
    0x4BCF82FF, // Green
    0xC44BDBFF, // Purple
    0xF5C542FF, // Yellow
    0xFA4B6AFF, // Red
    0x42C5C8FF, // Teal
    0x8C6BCFFF, // Indigo
};
#define CHART_PALETTE_COUNT 8

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static struct nk_color rgba_from_u32(uint32_t c) {
  return nk_rgba((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

static AslanTable *chart_find_source_table(Page *page,
                                           uint32_t source_table_id) {
  if (!page)
    return NULL;
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == BLOCK_TABLE && b->data.table.columnar) {
      if (source_table_id == 0 || b->id == source_table_id)
        return b->data.table.columnar;
    }
  }
  return NULL;
}

static int chart_find_first_number_col(AslanTable *tbl) {
  for (size_t c = 0; c < tbl->column_count; c++) {
    if (tbl->columns[c].type == ASLAN_COL_NUMBER)
      return (int)c;
  }
  return -1;
}

static int chart_collect_data(AslanTable *tbl, int label_col, int value_col,
                              double *values, char labels[][64],
                              int max_points) {
  int count = 0;
  size_t view_count = aslan_table_view_count(tbl);

  for (size_t v = 0; v < view_count && count < max_points; v++) {
    uint64_t row_id = aslan_table_view_row_id(tbl, (int)v);
    if (row_id == 0)
      continue;

    double val = aslan_table_get_number(tbl, value_col, row_id);

    // Get label
    char buf[64] = {0};
    const char *lbl = "";
    if (label_col >= 0 && label_col < (int)tbl->column_count) {
      const char *cell_text = aslan_table_get_cell_display(
          tbl, label_col, row_id, buf, sizeof(buf));
      if (cell_text)
        lbl = cell_text;
    }

    // Skip entirely empty/uninitialized default rows to prevent phantom data
    if (val == 0.0 && lbl[0] == '\0') {
      continue;
    }

    values[count] = val;

    if (lbl[0] != '\0') {
      strncpy(labels[count], lbl, 63);
      labels[count][63] = '\0';
    } else {
      snprintf(labels[count], 64, "%d", count + 1);
    }
    count++;
  }
  return count;
}

// -----------------------------------------------------------------------------
// Bar Chart
// -----------------------------------------------------------------------------

static void chart_draw_bars(struct nk_command_buffer *canvas,
                            struct nk_rect area, double *values,
                            char labels[][64], int count, struct nk_color color,
                            bool show_values, const struct nk_user_font *font) {
  if (count <= 0)
    return;

  double max_val = 0;
  for (int i = 0; i < count; i++) {
    if (values[i] > max_val)
      max_val = values[i];
  }
  if (max_val <= 0)
    max_val = 1.0;

  float label_h = 18.0f;
  float top_pad = 10.0f;
  float chart_h = area.h - label_h - top_pad;
  float chart_y = area.y + top_pad;
  float bar_gap = 4.0f;
  float bar_total_w = area.w / (float)count;
  float bar_w = bar_total_w - bar_gap;
  if (bar_w < 4.0f)
    bar_w = 4.0f;

  // Axes
  nk_stroke_line(canvas, area.x, chart_y, area.x, chart_y + chart_h, 1.0f,
                 nk_rgba(180, 180, 180, 255));
  nk_stroke_line(canvas, area.x, chart_y + chart_h, area.x + area.w,
                 chart_y + chart_h, 1.0f, nk_rgba(180, 180, 180, 255));

  for (int i = 0; i < count; i++) {
    float ratio = (float)(values[i] / max_val);
    float h = ratio * chart_h;
    float x = area.x + (float)i * bar_total_w + bar_gap / 2.0f;
    float y = chart_y + chart_h - h;

    nk_fill_rect(canvas, nk_rect(x, y, bar_w, h), 2.0f, color);

    if (show_values && font && h > 2.0f) {
      char val_str[32];
      snprintf(val_str, sizeof(val_str), "%.0f", values[i]);
      int vlen = (int)strlen(val_str);
      float tw = font->width(font->userdata, font->height, val_str, vlen);
      nk_draw_text(canvas, nk_rect(x + (bar_w - tw) / 2, y - 15, tw, 14),
                   val_str, vlen, font, nk_rgba(0, 0, 0, 0),
                   nk_rgba(100, 100, 100, 255));
    }

    if (font) {
      int lbl_len = (int)strlen(labels[i]);
      if (lbl_len > 8)
        lbl_len = 8;
      float tw = font->width(font->userdata, font->height, labels[i], lbl_len);
      nk_draw_text(canvas,
                   nk_rect(x + (bar_w - tw) / 2, chart_y + chart_h + 3, tw, 14),
                   labels[i], lbl_len, font, nk_rgba(0, 0, 0, 0),
                   nk_rgba(120, 120, 120, 255));
    }
  }
}

// -----------------------------------------------------------------------------
// Line Chart
// -----------------------------------------------------------------------------

static void chart_draw_lines(struct nk_command_buffer *canvas,
                             struct nk_rect area, double *values,
                             char labels[][64], int count,
                             struct nk_color color, bool show_values,
                             const struct nk_user_font *font) {
  if (count <= 0)
    return;

  double max_val = 0, min_val = DBL_MAX;
  for (int i = 0; i < count; i++) {
    if (values[i] > max_val)
      max_val = values[i];
    if (values[i] < min_val)
      min_val = values[i];
  }
  if (max_val <= min_val)
    max_val = min_val + 1.0;

  float label_h = 18.0f;
  float top_pad = 10.0f;
  float chart_h = area.h - label_h - top_pad;
  float chart_y = area.y + top_pad;

  // Axes
  nk_stroke_line(canvas, area.x, chart_y, area.x, chart_y + chart_h, 1.0f,
                 nk_rgba(180, 180, 180, 255));
  nk_stroke_line(canvas, area.x, chart_y + chart_h, area.x + area.w,
                 chart_y + chart_h, 1.0f, nk_rgba(180, 180, 180, 255));

  float step_x = count > 1 ? area.w / (float)(count - 1) : area.w;
  float prev_x = 0, prev_y = 0;

  for (int i = 0; i < count; i++) {
    float ratio = (float)((values[i] - min_val) / (max_val - min_val));
    float px = area.x + (float)i * step_x;
    float py = chart_y + chart_h - ratio * chart_h;

    if (i > 0) {
      nk_stroke_line(canvas, prev_x, prev_y, px, py, 2.0f, color);
    }

    nk_fill_circle(canvas, nk_rect(px - 3, py - 3, 6, 6), color);

    if (show_values && font) {
      char val_str[32];
      snprintf(val_str, sizeof(val_str), "%.0f", values[i]);
      int vlen = (int)strlen(val_str);
      float tw = font->width(font->userdata, font->height, val_str, vlen);
      nk_draw_text(canvas, nk_rect(px - tw / 2, py - 16, tw, 14), val_str, vlen,
                   font, nk_rgba(0, 0, 0, 0), nk_rgba(100, 100, 100, 255));
    }

    if (font) {
      int lbl_len = (int)strlen(labels[i]);
      if (lbl_len > 8)
        lbl_len = 8;
      float tw = font->width(font->userdata, font->height, labels[i], lbl_len);
      // Clamp so first/last labels stay within chart bounds
      float lx = px - tw / 2;
      if (lx < area.x)
        lx = area.x;
      if (lx + tw > area.x + area.w)
        lx = area.x + area.w - tw;
      nk_draw_text(canvas, nk_rect(lx, chart_y + chart_h + 3, tw, 14),
                   labels[i], lbl_len, font, nk_rgba(0, 0, 0, 0),
                   nk_rgba(120, 120, 120, 255));
    }

    prev_x = px;
    prev_y = py;
  }
}

// -----------------------------------------------------------------------------
// Pie Chart
// -----------------------------------------------------------------------------

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void chart_draw_pie(struct nk_command_buffer *canvas,
                           struct nk_rect area, double *values,
                           char labels[][64], int count, bool show_values,
                           const struct nk_user_font *font) {
  if (count <= 0)
    return;

  double total = 0;
  for (int i = 0; i < count; i++) {
    if (values[i] > 0)
      total += values[i];
  }
  if (total <= 0)
    return;

  float legend_w = 110.0f;
  float chart_w = area.w - legend_w;
  float cx = area.x + chart_w / 2.0f;
  float cy = area.y + area.h / 2.0f;
  float radius = (chart_w < area.h ? chart_w : area.h) / 2.0f - 10.0f;
  if (radius < 20.0f)
    radius = 20.0f;

  float angle = (float)(-M_PI / 2.0); // Start at 12 o'clock

  for (int i = 0; i < count; i++) {
    if (values[i] <= 0)
      continue;

    float sweep = (float)(values[i] / total) * 2.0f * (float)M_PI;
    struct nk_color slice_color =
        rgba_from_u32(chart_palette[i % CHART_PALETTE_COUNT]);

    int segs = (int)(sweep / (float)(M_PI / 24.0));
    if (segs < 2)
      segs = 2;
    if (segs > 48)
      segs = 48;
    float seg_angle = sweep / (float)segs;

    for (int s = 0; s < segs; s++) {
      float a1 = angle + (float)s * seg_angle;
      float a2 = angle + (float)(s + 1) * seg_angle;
      float x1 = cx + cosf(a1) * radius;
      float y1 = cy + sinf(a1) * radius;
      float x2 = cx + cosf(a2) * radius;
      float y2 = cy + sinf(a2) * radius;
      nk_fill_triangle(canvas, cx, cy, x1, y1, x2, y2, slice_color);
    }

    if (show_values && font) {
      float mid_angle = angle + sweep / 2.0f;
      float label_r = radius * 0.6f;
      float lx = cx + cosf(mid_angle) * label_r;
      float ly = cy + sinf(mid_angle) * label_r;

      char val_str[32];
      snprintf(val_str, sizeof(val_str), "%.0f%%", (values[i] / total) * 100.0);
      int vlen = (int)strlen(val_str);
      float tw = font->width(font->userdata, font->height, val_str, vlen);
      nk_draw_text(canvas, nk_rect(lx - tw / 2, ly - 7, tw, 14), val_str, vlen,
                   font, nk_rgba(0, 0, 0, 0), nk_rgba(255, 255, 255, 255));
    }

    angle += sweep;
  }

  // Legend
  float legend_x = area.x + chart_w + 8.0f;
  float legend_y = area.y + 12.0f;
  for (int i = 0; i < count && i < 8; i++) {
    if (values[i] <= 0)
      continue;
    struct nk_color c = rgba_from_u32(chart_palette[i % CHART_PALETTE_COUNT]);
    nk_fill_rect(canvas, nk_rect(legend_x, legend_y, 10, 10), 2.0f, c);
    if (font) {
      int lbl_len = (int)strlen(labels[i]);
      if (lbl_len > 14)
        lbl_len = 14;
      nk_draw_text(canvas, nk_rect(legend_x + 14, legend_y - 1, 90, 14),
                   labels[i], lbl_len, font, nk_rgba(0, 0, 0, 0),
                   nk_rgba(100, 100, 100, 255));
    }
    legend_y += 18.0f;
  }
}

// -----------------------------------------------------------------------------
// Shared rendering (used by both Block and Canvas Node)
// -----------------------------------------------------------------------------

void ui_render_chart_impl(UIHost *ui, struct nk_context *ctx, ChartData *cd,
                          struct nk_rect bounds, Page *page) {
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Background
  struct nk_color bg =
      ui->dark_mode ? nk_rgb(35, 35, 40) : nk_rgb(255, 255, 255);
  struct nk_color border =
      ui->dark_mode ? nk_rgb(55, 55, 65) : nk_rgb(220, 220, 225);
  nk_fill_rect(canvas, bounds, 5.0f, bg);
  nk_stroke_rect(canvas, bounds, 5.0f, 1.0f, border);

  // Find source table
  AslanTable *tbl = chart_find_source_table(page, cd->source_table_id);

  if (!tbl) {
    struct nk_color muted = nk_rgba(150, 150, 150, 255);
    nk_draw_text(
        canvas,
        nk_rect(bounds.x + 10, bounds.y + bounds.h / 2 - 10, bounds.w - 20, 20),
        "No source table linked", 22, &ui->font_inter->handle,
        nk_rgba(0, 0, 0, 0), muted);
    return;
  }

  // ── GANTT chart: handled separately (no NUMBER col needed) ───────────────
  if (cd->type == CHART_GANTT) {
    // Find DATE and DURATION columns in source table
    int date_col = -1, dur_col = -1, name_col = cd->label_col_idx;
    for (int c = 0; c < (int)tbl->column_count; c++) {
      if (tbl->columns[c].type == ASLAN_COL_DATE && date_col < 0)
        date_col = c;
      if (tbl->columns[c].type == ASLAN_COL_DURATION && dur_col < 0)
        dur_col = c;
      if (name_col < 0 && tbl->columns[c].type == ASLAN_COL_TEXT)
        name_col = c;
    }
    if (date_col < 0 || dur_col < 0) {
      nk_draw_text(canvas,
                   nk_rect(bounds.x + 10, bounds.y + bounds.h / 2 - 10,
                           bounds.w - 20, 20),
                   "Add DATE + DURATION columns", 27, &ui->font_inter->handle,
                   nk_rgba(0, 0, 0, 0), nk_rgba(150, 150, 150, 255));
      return;
    }

    // Init navigation state
    if (ui->gantt_state.day_w <= 0.0f)
      ui->gantt_state.day_w = 40.0f;
    if (ui->gantt_state.view_start_day == 0) {
      time_t now = time(NULL);
      ui->gantt_state.view_start_day = (int64_t)(now / 86400) - 3;
    }
    float day_w = ui->gantt_state.day_w;
    int64_t vsd = ui->gantt_state.view_start_day;

    const float label_w = 140.0f;
    const float hdr_h = 28.0f;
    const float row_h = 28.0f;
    const struct nk_user_font *font = &ui->font_inter->handle;

    size_t view_count = aslan_table_view_count(tbl);
    int draw_rows = (int)view_count < 50 ? (int)view_count : 50;
    float total_h = bounds.h;
    int visible_days = (int)((bounds.w - label_w) / day_w) + 2;

    static const char *day_names[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    static const char *mon_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    // Zoom buttons (top-right corner)
    {
      float bx = bounds.x + bounds.w - 54.0f, by = bounds.y + 4.0f;
      struct nk_rect bm = nk_rect(bx, by, 22.0f, 20.0f);
      struct nk_rect bp = nk_rect(bx + 24.f, by, 22.0f, 20.0f);
      float mx = ctx->input.mouse.pos.x, my2 = ctx->input.mouse.pos.y;
      int hm =
          (mx >= bm.x && mx < bm.x + bm.w && my2 >= bm.y && my2 < bm.y + bm.h);
      int hp =
          (mx >= bp.x && mx < bp.x + bp.w && my2 >= bp.y && my2 < bp.y + bp.h);
      nk_fill_rect(canvas, bm, 4,
                   hm ? nk_rgba(0, 0, 0, 18) : nk_rgba(0, 0, 0, 8));
      nk_fill_rect(canvas, bp, 4,
                   hp ? nk_rgba(0, 0, 0, 18) : nk_rgba(0, 0, 0, 8));
      nk_draw_text(canvas, bm, "-", 1, font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(60, 60, 80));
      nk_draw_text(canvas, bp, "+", 1, font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(60, 60, 80));
      if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, bm))
        ui->gantt_state.day_w = day_w > 15.0f ? day_w - 5.0f : day_w;
      if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, bp))
        ui->gantt_state.day_w = day_w < 120.0f ? day_w + 5.0f : day_w;
    }

    // Navigation buttons (< >)
    {
      float by = bounds.y + 4.0f;
      struct nk_rect bl = nk_rect(bounds.x + label_w + 2.f, by, 22.f, 20.f);
      struct nk_rect br = nk_rect(bounds.x + bounds.w - 82.f, by, 22.f, 20.f);
      float mx = ctx->input.mouse.pos.x, my2 = ctx->input.mouse.pos.y;
      int hl =
          (mx >= bl.x && mx < bl.x + bl.w && my2 >= bl.y && my2 < bl.y + bl.h);
      int hr =
          (mx >= br.x && mx < br.x + br.w && my2 >= br.y && my2 < br.y + br.h);
      nk_fill_rect(canvas, bl, 4,
                   hl ? nk_rgba(0, 0, 0, 18) : nk_rgba(0, 0, 0, 8));
      nk_fill_rect(canvas, br, 4,
                   hr ? nk_rgba(0, 0, 0, 18) : nk_rgba(0, 0, 0, 8));
      nk_draw_text(canvas, bl, "<", 1, font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(60, 60, 80));
      nk_draw_text(canvas, br, ">", 1, font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(60, 60, 80));
      if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, bl))
        ui->gantt_state.view_start_day -= 7;
      if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, br))
        ui->gantt_state.view_start_day += 7;
    }

    // Day column headers
    float hx = bounds.x + label_w;
    for (int d = 0; d < visible_days; d++) {
      int64_t abs_day = vsd + d;
      float cx = hx + d * day_w;
      if (cx + day_w < bounds.x + label_w || cx > bounds.x + bounds.w)
        continue;

      time_t t = (time_t)(abs_day * 86400LL);
      struct tm *tm_p = localtime(&t);
      int wday = tm_p->tm_wday, mday = tm_p->tm_mday;

      // Weekend tint (full column height) - softer
      if (wday == 0 || wday == 6)
        nk_fill_rect(canvas, nk_rect(cx, bounds.y, day_w, total_h), 0,
                     nk_rgba(0, 0, 0, 4));

      // Month label on 1st - bolder/more professional
      struct nk_color dcol =
          ui->dark_mode ? nk_rgb(140, 142, 150) : nk_rgb(110, 110, 120);
      if (mday == 1) {
        char mon_lbl[16];
        snprintf(mon_lbl, sizeof(mon_lbl), "%s %d", mon_names[tm_p->tm_mon],
                 1900 + tm_p->tm_year);
        nk_draw_text(canvas, nk_rect(cx + 4, bounds.y, day_w * 4, hdr_h / 2),
                     mon_lbl, (int)strlen(mon_lbl), font, nk_rgba(0, 0, 0, 0),
                     nk_rgba(26, 115, 232, 255));
      }
      // Day name + number
      char dlbl[8];
      snprintf(dlbl, sizeof(dlbl), "%s %d", day_names[wday], mday);
      nk_draw_text(canvas,
                   nk_rect(cx + 2, bounds.y + hdr_h / 2, day_w - 2, hdr_h / 2),
                   dlbl, (int)strlen(dlbl), font, nk_rgba(0, 0, 0, 0), dcol);
      // Vertical grid line - very subtle
      nk_stroke_line(canvas, cx, bounds.y + hdr_h, cx, bounds.y + total_h, 1.0f,
                     nk_rgba(0, 0, 0, 6));
    }
    // Header separator - clean
    nk_stroke_line(canvas, bounds.x, bounds.y + hdr_h, bounds.x + bounds.w,
                   bounds.y + hdr_h, 1.0f, nk_rgba(0, 0, 0, 20));
    // Label column separator
    nk_stroke_line(canvas, bounds.x + label_w, bounds.y, bounds.x + label_w,
                   bounds.y + total_h, 1.0f, nk_rgba(0, 0, 0, 20));

    // Task rows
    for (int r = 0; r < draw_rows; r++) {
      float ry = bounds.y + hdr_h + r * row_h;
      if (ry + row_h > bounds.y + total_h)
        break;

      uint64_t rid = aslan_table_view_row_id(tbl, r);
      // Soft Zebra Striping
      if (r % 2 == 0)
        nk_fill_rect(canvas, nk_rect(bounds.x, ry, bounds.w, row_h), 0,
                     ui->dark_mode ? nk_rgba(255, 255, 255, 2)
                                   : nk_rgba(0, 0, 0, 2));

      // Task name - Vertically centered
      char nbuf[128] = {0};
      const char *nm = name_col >= 0
                           ? aslan_table_get_cell_display(tbl, name_col, rid,
                                                          nbuf, sizeof(nbuf))
                           : "";
      if (!nm || !nm[0])
        nm = "—";
      nk_draw_text(canvas,
                   nk_rect(bounds.x + 10, ry + (row_h - font->height) / 2.0f,
                           label_w - 15, font->height),
                   nm, (int)strlen(nm), font, nk_rgba(0, 0, 0, 0),
                   ui->dark_mode ? nk_rgb(210, 212, 220) : nk_rgb(50, 50, 60));

      // Horizontal separator - very subtle
      nk_stroke_line(canvas, bounds.x, ry + row_h - 1, bounds.x + bounds.w,
                     ry + row_h - 1, 1.0f, nk_rgba(0, 0, 0, 6));

      // Bar
      int64_t ts_ms = aslan_table_get_date(tbl, date_col, rid);
      int64_t md = aslan_table_get_duration(tbl, dur_col, rid);
      if (ts_ms > 0 && md > 0) {
        double start_day = (double)(ts_ms / 1000LL) / 86400.0;
        double dur_days = md / 1000.0;
        float bx =
            bounds.x + label_w + (float)(start_day - (double)vsd) * day_w;
        float bw2 = (float)(dur_days * day_w);
        if (bw2 < 4.0f)
          bw2 = 4.0f;
        float clip_l = bounds.x + label_w, clip_r = bounds.x + bounds.w;

        // Visual "Pill" config
        struct nk_color b_col =
            rgba_from_u32(chart_palette[r % CHART_PALETTE_COUNT]);
        b_col.a = 230; // High opacity but not solid

        if (bx < clip_l) {
          bw2 -= (clip_l - bx);
          bx = clip_l;
        }
        if (bx + bw2 > clip_r)
          bw2 = clip_r - bx;
        if (bw2 > 0) {
          // Main Task Bar (Rounded Pill)
          struct nk_rect bar_r = nk_rect(bx, ry + 6, bw2, row_h - 12);
          nk_fill_rect(canvas, bar_r, bar_r.h / 2.0f, b_col);

          // Subtle internal shadow/gradient
          nk_stroke_rect(canvas, bar_r, bar_r.h / 2.0f, 1.0f,
                         nk_rgba(0, 0, 0, 20));

          if (bw2 > 40) {
            char dbuf[16];
            if (dur_days >= 1.0)
              snprintf(dbuf, sizeof(dbuf), "%.1f d", dur_days);
            else
              snprintf(dbuf, sizeof(dbuf), "%.0f h", dur_days * 24.0);

            int dlen = (int)strlen(dbuf);
            float tw = font->width(font->userdata, font->height, dbuf, dlen);
            nk_draw_text(
                canvas,
                nk_rect(bx + (bw2 - tw) / 2.0f,
                        ry + (row_h - font->height) / 2.0f, tw, font->height),
                dbuf, dlen, font, nk_rgba(0, 0, 0, 0), nk_rgb(255, 255, 255));
          }
        }
      }
    }

    // Today marker (dashed style with top circle)
    {
      int64_t today = (int64_t)(time(NULL) / 86400);
      float tx = bounds.x + label_w + (float)(today - vsd) * day_w;
      if (tx >= bounds.x + label_w && tx <= bounds.x + bounds.w) {
        struct nk_color today_col = nk_rgba(234, 67, 53, 255);
        // Vertical line
        for (float y = bounds.y + hdr_h; y < bounds.y + total_h; y += 8) {
          float dy =
              (y + 4 > bounds.y + total_h) ? (bounds.y + total_h - y) : 4;
          nk_stroke_line(canvas, tx, y, tx, y + dy, 2.0f, today_col);
        }
        // Top circle indicator
        nk_fill_circle(canvas, nk_rect(tx - 4, bounds.y + hdr_h - 4, 8, 8),
                       today_col);
      }
    }
    return;
    return;
  }

  // Auto-detect value column (Bar / Line / Pie)
  int value_col = cd->value_col_idx;
  if (value_col < 0 || value_col >= (int)tbl->column_count ||
      tbl->columns[value_col].type != ASLAN_COL_NUMBER) {
    value_col = chart_find_first_number_col(tbl);
    if (value_col < 0) {
      nk_draw_text(canvas,
                   nk_rect(bounds.x + 10, bounds.y + bounds.h / 2 - 10,
                           bounds.w - 20, 20),
                   "No NUMBER column found", 22, &ui->font_inter->handle,
                   nk_rgba(0, 0, 0, 0), nk_rgba(150, 150, 150, 255));
      return;
    }
  }

  // Collect data
  double values[CHART_MAX_POINTS];
  char lbls[CHART_MAX_POINTS][64];
  memset(lbls, 0, sizeof(lbls));
  int count = chart_collect_data(tbl, cd->label_col_idx, value_col, values,
                                 lbls, CHART_MAX_POINTS);

  if (count == 0) {
    nk_draw_text(
        canvas,
        nk_rect(bounds.x + 10, bounds.y + bounds.h / 2 - 10, bounds.w - 20, 20),
        "No data rows", 12, &ui->font_inter->handle, nk_rgba(0, 0, 0, 0),
        nk_rgba(150, 150, 150, 255));
    return;
  }

  struct nk_color color = rgba_from_u32(cd->color);
  const struct nk_user_font *font = &ui->font_inter->handle;

  switch (cd->type) {
  case CHART_BAR:
    chart_draw_bars(canvas, bounds, values, lbls, count, color, cd->show_values,
                    font);
    break;
  case CHART_LINE:
    chart_draw_lines(canvas, bounds, values, lbls, count, color,
                     cd->show_values, font);
    break;
  case CHART_PIE:
    chart_draw_pie(canvas, bounds, values, lbls, count, cd->show_values, font);
    break;
  case CHART_GANTT:
    break; // handled above
  }
}

void ui_render_chart_node_config_bar(UIHost *ui, Engine *engine,
                                     struct nk_context *ctx, Node *node,
                                     struct nk_rect bounds, Page *page) {
  (void)ui;
  (void)engine;
  if (!node || node->type != NODE_TYPE_CHART || !node->chart_data || !page)
    return;

  ChartData *cd = node->chart_data;

  // Render a small floating bar above the chart
  struct nk_rect bar_rect = nk_rect(bounds.x, bounds.y - 35, bounds.w, 30);
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Background for the bar
  nk_fill_rect(canvas, bar_rect, 5.0f, nk_rgba(255, 255, 255, 240));
  nk_stroke_rect(canvas, bar_rect, 5.0f, 1.0f, nk_rgba(200, 200, 200, 255));

  // We use a group for the buttons to handle clipping and layout
  if (nk_group_begin(ctx, "chart_node_config", NK_WINDOW_NO_SCROLLBAR)) {
    nk_layout_row_template_begin(ctx, 24.0f);
    nk_layout_row_template_push_static(ctx, 60.0f);  // Type
    nk_layout_row_template_push_static(ctx, 100.0f); // Table
    nk_layout_row_template_push_static(ctx, 80.0f);  // Label Col
    nk_layout_row_template_push_static(ctx, 80.0f);  // Value Col
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);

    // --- Chart Type ---
    const char *type_labels[] = {"Bar", "Line", "Pie"};
    if (nk_button_label(ctx, type_labels[cd->type])) {
      cd->type = (cd->type + 1) % 3;
    }

    // --- Source Table ---
    char src_label[64] = "Table...";
    if (cd->source_table_id > 0) {
      Block *src = page_find_block_by_id(page, cd->source_table_id);
      if (src && src->type == BLOCK_TABLE)
        snprintf(src_label, sizeof(src_label), "T#%u", src->id);
    }

    if (nk_button_label(ctx, src_label)) {
      Block *first_table = NULL;
      Block *next_table = NULL;
      bool found_current = false;
      for (Block *b = page->first_block; b; b = b->next) {
        if (b->type == BLOCK_TABLE && b->data.table.columnar) {
          if (!first_table)
            first_table = b;
          if (found_current && !next_table) {
            next_table = b;
            break;
          }
          if (b->id == cd->source_table_id)
            found_current = true;
        }
      }
      if (!next_table)
        next_table = first_table;
      if (next_table) {
        cd->source_table_id = next_table->id;
      }
    }

    // --- Label Column ---
    AslanTable *tbl = chart_find_source_table(page, cd->source_table_id);
    char col_label[64] = "L: -";
    if (tbl && cd->label_col_idx >= 0 &&
        cd->label_col_idx < (int)tbl->column_count) {
      snprintf(col_label, sizeof(col_label), "L:%.8s",
               tbl->columns[cd->label_col_idx].name);
    }
    if (nk_button_label(ctx, col_label)) {
      if (tbl && tbl->column_count > 0) {
        cd->label_col_idx = (cd->label_col_idx + 1) % (int)tbl->column_count;
      }
    }

    // --- Value Column ---
    char val_label[64] = "V: -";
    if (tbl && cd->value_col_idx >= 0 &&
        cd->value_col_idx < (int)tbl->column_count) {
      snprintf(val_label, sizeof(val_label), "V:%.8s",
               tbl->columns[cd->value_col_idx].name);
    }
    if (nk_button_label(ctx, val_label)) {
      if (tbl && tbl->column_count > 0) {
        int start = cd->value_col_idx;
        for (int attempt = 0; attempt < (int)tbl->column_count; attempt++) {
          int candidate = (start + 1 + attempt) % (int)tbl->column_count;
          if (tbl->columns[candidate].type == ASLAN_COL_NUMBER) {
            cd->value_col_idx = candidate;
            break;
          }
        }
      }
    }

    nk_group_end(ctx);
  }
}

// -----------------------------------------------------------------------------
// Node-level chart rendering (for Canvas/Design blocks)
// -----------------------------------------------------------------------------

void ui_render_chart_node(UIHost *ui, struct nk_context *ctx, Node *node,
                          struct nk_rect bounds, Page *page) {
  if (!ui || !node || !ctx || node->type != NODE_TYPE_CHART ||
      !node->chart_data)
    return;

  ui_render_chart_impl(ui, ctx, node->chart_data, bounds, page);
}
