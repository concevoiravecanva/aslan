#include "ui_design.h"
#include "clay_host.h"
#include "../document/aslan_table.h"
#include "../document/document.h"
#include "../vendor/clay.h"
#include "ui_canvas.h"
#include "ui_shared.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Intersection of two rects — used to clip node scissors to artboard bounds
static AslanRect rect_intersect(AslanRect a, AslanRect b) {
  float x1 = a.x > b.x ? a.x : b.x;
  float y1 = a.y > b.y ? a.y : b.y;
  float x2 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
  float y2 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
  if (x2 < x1)
    x2 = x1;
  if (y2 < y1)
    y2 = y1;
  return (AslanRect){x1, y1, x2 - x1, y2 - y1};
}

int node_get_path_polygon(Node *node, float *out_pts, int max_pts, float sx,
                          float sy, float zoom);
int open_image_file_dialog(char *out_path, size_t path_size);
int copy_image_to_assets(const char *src_path, const char *doc_dir,
                         char *out_rel_path, size_t out_size);
void draw_alignment_guides(UIHost *ui, Node *node, float area_cx, float area_cy,
                           float area_x, float area_y, float area_w,
                           float area_h, float node_sx, float node_sy,
                           float node_sw, float node_sh);
void render_node_context_menu(UIHost *ui, Block *block);

// Helper for rotated rounded rect
static void fill_rotated_rounded_rect(UIHost *ui, float cx, float cy, float w,
                                      float h, float rad, float radius,
                                      NVGcolor col, int is_stroke,
                                      float thickness) {
  nvgSave(ui->vg);
  nvgTranslate(ui->vg, cx, cy);
  if (rad != 0.0f)
    nvgRotate(ui->vg, rad);

  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, -w / 2.0f, -h / 2.0f, w, h, radius);

  if (is_stroke) {
    nvgStrokeColor(ui->vg, col);
    nvgStrokeWidth(ui->vg, thickness);
    nvgStroke(ui->vg);
  } else {
    nvgFillColor(ui->vg, col);
    nvgFill(ui->vg);
  }
  nvgRestore(ui->vg);
}

typedef struct { float x, y; } AslanVec2;

static void fill_rounded_polygon(UIHost *ui, const AslanVec2 *points,
                                 int count, float radius, NVGcolor col,
                                 int is_stroke, float thickness,
                                 NVGpaint *grad, NVGpaint *img_paint) {
  if (count < 3)
    return;
  float out_pts[1024]; // max 512 points for rounded version
  int out_count = 0;

  if (radius < 0.1f) {
    int pc = (count > 64) ? 64 : count;
    for (int i = 0; i < pc; i++) {
      out_pts[i * 2] = points[i].x;
      out_pts[i * 2 + 1] = points[i].y;
    }
    out_count = pc;
  } else {
    for (int i = 0; i < count; i++) {
      AslanVec2 p1 = points[(i + count - 1) % count];
      AslanVec2 p2 = points[i];
      AslanVec2 p3 = points[(i + 1) % count];

      float dx1 = p1.x - p2.x;
      float dy1 = p1.y - p2.y;
      float dx2 = p3.x - p2.x;
      float dy2 = p3.y - p2.y;

      float len1 = sqrtf(dx1 * dx1 + dy1 * dy1);
      float len2 = sqrtf(dx2 * dx2 + dy2 * dy2);
      if (len1 < 0.001f || len2 < 0.001f)
        continue;

      dx1 /= len1;
      dy1 /= len1;
      dx2 /= len2;
      dy2 /= len2;

      float dot = dx1 * dx2 + dy1 * dy2;
      if (dot > 0.999f) {
        if (out_count < 511) {
          out_pts[out_count * 2] = p2.x;
          out_pts[out_count * 2 + 1] = p2.y;
          out_count++;
        }
        continue;
      }

      float angle = acosf(dot);
      float d = radius / tanf(angle / 2.0f);
      if (d > len1 / 2.0f)
        d = len1 / 2.0f;
      if (d > len2 / 2.0f)
        d = len2 / 2.0f;
      float actual_radius = d * tanf(angle / 2.0f);

      float dcx = dx1 + dx2;
      float dcy = dy1 + dy2;
      float dclen = sqrtf(dcx * dcx + dcy * dcy);
      dcx /= dclen;
      dcy /= dclen;

      float dist_to_center = actual_radius / sinf(angle / 2.0f);
      float cx = p2.x + dcx * dist_to_center;
      float cy = p2.y + dcy * dist_to_center;

      float start_angle = atan2f(p2.y + dy1 * d - cy, p2.x + dx1 * d - cx);
      float end_angle = atan2f(p2.y + dy2 * d - cy, p2.x + dx2 * d - cx);

      float diff = end_angle - start_angle;
      while (diff > 3.14159f)
        diff -= 2 * 3.14159f;
      while (diff < -3.14159f)
        diff += 2 * 3.14159f;

      int steps = 6;
      for (int j = 0; j <= steps; j++) {
        if (out_count >= 511)
          break;
        float a = start_angle + diff * (float)j / (float)steps;
        out_pts[out_count * 2] = cx + cosf(a) * actual_radius;
        out_pts[out_count * 2 + 1] = cy + sinf(a) * actual_radius;
        out_count++;
      }
    }
  }

  nvgBeginPath(ui->vg);
  nvgMoveTo(ui->vg, out_pts[0], out_pts[1]);
  for (int i = 1; i < out_count; i++) {
    nvgLineTo(ui->vg, out_pts[i * 2], out_pts[i * 2 + 1]);
  }
  nvgClosePath(ui->vg);

  if (is_stroke) {
    nvgStrokeColor(ui->vg, col);
    nvgStrokeWidth(ui->vg, thickness);
    nvgStroke(ui->vg);
  } else if (img_paint) {
    nvgFillPaint(ui->vg, *img_paint);
    nvgFill(ui->vg);
  } else if (grad) {
    nvgFillPaint(ui->vg, *grad);
    nvgFill(ui->vg);
  } else {
    nvgFillColor(ui->vg, col);
    nvgFill(ui->vg);
  }
}

// Smooth rounded rect using NanoVG natively
static void fill_smooth_rounded_rect(UIHost *ui, float rx, float ry, float rw,
                                     float rh, float cr, NVGcolor col) {
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, rx, ry, rw, rh, cr);
  nvgFillColor(ui->vg, col);
  nvgFill(ui->vg);
}

// Gradient-filled rounded rect at any angle (0=left→right, 90=top→bottom).
static void fill_gradient_rounded_rect(UIHost *ui, float rx, float ry,
                                       float rw, float rh, float cr,
                                       NVGcolor c1, NVGcolor c2,
                                       float angle_deg) {
  float rad = angle_deg * 3.14159265f / 180.0f;
  float cx = rx + rw * 0.5f, cy = ry + rh * 0.5f;
  float dx = cosf(rad) * rw * 0.5f;
  float dy = sinf(rad) * rh * 0.5f;
  NVGpaint paint = nvgLinearGradient(ui->vg, cx - dx, cy - dy,
                                     cx + dx, cy + dy, c1, c2);
  nvgBeginPath(ui->vg);
  if (cr > 0.0f)
    nvgRoundedRect(ui->vg, rx, ry, rw, rh, cr);
  else
    nvgRect(ui->vg, rx, ry, rw, rh);
  nvgFillPaint(ui->vg, paint);
  nvgFill(ui->vg);
}

// =============================================================================
// DESIGN SIDEBAR — Properties | Layers | Page
// =============================================================================

// =============================================================================
// DESIGN SMART LAYOUT — TABLEAU (live table top-left + chart below)
// =============================================================================
static void design_insert_tableau_layout(DesignData *dd) {
  if (!dd || !dd->root)
    return;

  float w = dd->width > 0 ? dd->width : 800.0f;
  float h = dd->height > 0 ? dd->height : 600.0f;

  float pad = 20.0f;
  float col_w = w * 0.48f; // left half
  float tbl_x = pad;
  float tbl_y = pad;
  float tbl_h = h * 0.50f; // table takes top half

  // ── Live table view node ───────────────────────────────────────────────────
  Node *tv = node_create(dd->next_node_id++, NODE_TYPE_TABLE_VIEW, "Table");
  if (tv) {
    tv->x = tbl_x;
    tv->y = tbl_y;
    tv->width = col_w;
    tv->height = tbl_h;
    tv->visible = 1;
    tv->opacity = 1.0f;
    tv->linked_block_id =
        dd->linked_table_id; // 0 = fallback to design's linked table
    tv->parent = dd->root;

    Node *last = dd->root->first_child;
    if (!last) {
      dd->root->first_child = tv;
    } else {
      while (last->next_sibling)
        last = last->next_sibling;
      last->next_sibling = tv;
    }
  }

  // Table only — no chart
  (void)h;
}

// ── Main sidebar ─────────────────────────────────────────────────────────────


// Font IDs for Clay (must match clay_host.c)
#define ASLAN_FONT_INTER 0
#define ASLAN_FONT_BOLD 1
#define ASLAN_FONT_ICONS 2

static void design_render_sidebar(UIHost *ui, Engine *engine,
                                  Block *block, float width, float height, float abs_x) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;
  NVGcontext *vg = ui->vg;
  bool dark = ui->dark_mode != 0;

  nvgw_set_offset(&ui->nvgw, abs_x, ui->abs_offset_y);

  // ── Background ────────────────────────────────────────────────────
  nvgBeginPath(vg);
  nvgRect(vg, 0, 0, width, height);
  nvgFillColor(vg, dark ? nvgRGBA(32, 34, 40, 255) : nvgRGBA(248, 248, 250, 255));
  nvgFill(vg);

  NVGcolor sep_col = dark ? nvgRGBA(50, 52, 60, 255) : nvgRGBA(220, 220, 225, 255);

  // Left border
  nvgBeginPath(vg);
  nvgMoveTo(vg, 0, 0);  nvgLineTo(vg, 0, height);
  nvgStrokeColor(vg, sep_col);  nvgStrokeWidth(vg, 1.0f);  nvgStroke(vg);

  // ── Tab bar ───────────────────────────────────────────────────────
  const float tab_bar_h = 40.0f;
  float tab_w = width / 3.0f;

  // Tab bar background (slightly darker than sidebar content)
  nvgBeginPath(vg);
  nvgRect(vg, 0, 0, width, tab_bar_h);
  nvgFillColor(vg, dark ? nvgRGBA(26, 28, 36, 255) : nvgRGBA(240, 241, 245, 255));
  nvgFill(vg);

  const char *tab_lbls[] = {"Properties", "Layers", "Page"};
  float mlx = ui->nvgw.mouse_x - abs_x;
  float mly = ui->nvgw.mouse_y - ui->abs_offset_y;

  for (int i = 0; i < 3; i++) {
    float tx    = (float)i * tab_w;
    bool active = (ui->design_bottom_tab == i);
    bool hov    = (mlx >= tx && mlx < tx + tab_w && mly >= 0 && mly < tab_bar_h);

    // Hover tint
    if (hov && !active) {
      nvgBeginPath(vg);
      nvgRect(vg, tx, 0, tab_w, tab_bar_h);
      nvgFillColor(vg, dark ? nvgRGBA(255,255,255,10) : nvgRGBA(0,0,0,6));
      nvgFill(vg);
    }

    // Label
    nvgFontSize(vg, 12.5f);
    nvgFontFaceId(vg, ui->nvgw.font_main);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, active
      ? (dark ? nvgRGBA(235, 237, 250, 255) : nvgRGBA(22,  24,  40,  255))
      : (hov
        ? (dark ? nvgRGBA(175, 177, 195, 255) : nvgRGBA(90,  92, 110, 255))
        : (dark ? nvgRGBA(110, 112, 130, 255) : nvgRGBA(145, 147, 165, 255))));
    nvgText(vg, tx + tab_w * 0.5f, tab_bar_h * 0.5f, tab_lbls[i], NULL);

    // Active accent — short underline bar centred under label
    if (active) {
      float iw = fminf(tab_w * 0.55f, 52.0f);
      float ix  = tx + (tab_w - iw) * 0.5f;
      nvgBeginPath(vg);
      nvgRoundedRect(vg, ix, tab_bar_h - 2.5f, iw, 2.5f, 1.5f);
      nvgFillColor(vg, nvgRGBA(60, 120, 220, 255));
      nvgFill(vg);
    }

    // Subtle vertical divider between tabs
    if (i < 2) {
      nvgBeginPath(vg);
      nvgMoveTo(vg, tx + tab_w, tab_bar_h * 0.25f);
      nvgLineTo(vg, tx + tab_w, tab_bar_h * 0.75f);
      nvgStrokeColor(vg, dark ? nvgRGBA(48,50,62,200) : nvgRGBA(210,212,222,200));
      nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
    }

    // Click handling
    uint32_t tid = nvgw_id_str("SbTab") ^ (uint32_t)i;
    if (hov) ui->nvgw.hot_id = tid;
    if (hov && ui->nvgw.mouse_clicked)  ui->nvgw.active_id = tid;
    if (ui->nvgw.active_id == tid && ui->nvgw.mouse_released && hov) {
      ui->nvgw.active_id = 0;
      ui->design_bottom_tab = i;
      ui_request_redraw(ui);
    }
  }

  // Separator below tab bar
  nvgBeginPath(vg);
  nvgMoveTo(vg, 0, tab_bar_h); nvgLineTo(vg, width, tab_bar_h);
  nvgStrokeColor(vg, sep_col); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);

  // ── Content area coords (local, 0-based) ──────────────────────────
  float cx    = 12.0f;
  float cy    = tab_bar_h + 1.0f;
  float cw    = width - 24.0f;
  float ch_h  = height - cy;

  if (ui->design_bottom_tab == 0) {
    // ── Properties ────────────────────────────────────────────────
    if (!ui->selected_node) {
      nvgFontSize(vg, 14.0f);
      nvgFontFace(vg, "Inter");
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, dark ? nvgRGBA(100, 102, 115, 255) : nvgRGBA(160, 162, 175, 255));
      nvgText(vg, width * 0.5f, cy + 60.0f, "No element selected", NULL);
    } else {
      Node *node = ui->selected_node;
      // Reset scroll when selected node changes
      if (ui->prop_input_node != (void *)node)
        ui->prop_scroll_y = 0.0f;
      // Handle scroll wheel inside Properties panel
      {
        float _mx = ui->nvgw.mouse_x - ui->nvgw.offset_x;
        float _my = ui->nvgw.mouse_y - ui->nvgw.offset_y;
        bool in_panel = _mx >= 0 && _mx < width && _my >= cy && _my < height;
        if (in_panel && ui->nvgw.scroll_y != 0.0f) {
          ui->prop_scroll_y -= ui->nvgw.scroll_y * 22.0f;
          if (ui->prop_scroll_y < 0.0f) ui->prop_scroll_y = 0.0f;
          ui_request_redraw(ui);
        }
      }
      // Apply scissor to clip panel content to panel area
      nvgScissor(vg, 0, cy, width, height - cy);
      nvgSave(vg);
      nvgTranslate(vg, 0, -ui->prop_scroll_y);
      nvgw_set_offset(&ui->nvgw, ui->nvgw.offset_x, ui->abs_offset_y - ui->prop_scroll_y);
      {
      float y_off = cy + 4.0f;
      float x_ptr = cx;
      float w_ptr = cw;
      float half_w = (w_ptr - 6) / 2.0f;
      const float lbl_w = 52.0f; // consistent label column width
      const float row_h = 28.0f;
      const float row_gap = 6.0f;
      // Reset persistent input buffers when the selected node changes
      if (ui->prop_input_node != (void *)node) {
        memset(ui->prop_input_bufs, 0, sizeof(ui->prop_input_bufs));
        ui->prop_input_node = (void *)node;
      }
      // Macro: only refresh buffer from node value when field is not focused
#define PROP_SYNC(N, FMT, VAL) \
      if (ui->nvgw.focus_id != nvgw_id(node, (N))) \
        snprintf(ui->prop_input_bufs[(N)], 64, (FMT), (VAL))
      bool dark = ui->dark_mode != 0;
      NVGcolor col_section = dark ? nvgRGBA(110, 112, 128, 255) : nvgRGBA(140, 142, 158, 255);
      NVGcolor col_label   = dark ? nvgRGBA(160, 162, 178, 255) : nvgRGBA(110, 112, 130, 255);
      NVGcolor col_sep     = dark ? nvgRGBA(55, 57, 68, 255)    : nvgRGBA(220, 220, 228, 255);

      // Helper macro: draw separator line
#define PROP_SEPARATOR() do { \
        nvgBeginPath(vg); \
        nvgMoveTo(vg, x_ptr, y_off + 4); \
        nvgLineTo(vg, x_ptr + w_ptr, y_off + 4); \
        nvgStrokeColor(vg, col_sep); \
        nvgStrokeWidth(vg, 1.0f); \
        nvgStroke(vg); \
        y_off += 14; \
      } while(0)

      // ── Node name (editable) ──────────────────────────────────────
      if (!node->name[0]) {
        // auto-fill with type name so field is never blank
        switch (node->type) {
          case NODE_TYPE_RECT:    strncpy(node->name, "Rectangle", 63); break;
          case NODE_TYPE_ELLIPSE: strncpy(node->name, "Ellipse", 63);   break;
          case NODE_TYPE_TEXT:    strncpy(node->name, "Text", 63);       break;
          case NODE_TYPE_IMAGE:   strncpy(node->name, "Image", 63);      break;
          case NODE_TYPE_LINE:    strncpy(node->name, "Line", 63);       break;
          case NODE_TYPE_FRAME:   strncpy(node->name, "Frame", 63);      break;
          case NODE_TYPE_GROUP:   strncpy(node->name, "Group", 63);      break;
          default:                strncpy(node->name, "Element", 63);    break;
        }
      }
      nvgw_text_input(&ui->nvgw, nvgw_id(node, 0), x_ptr, y_off, w_ptr, row_h, node->name, 64, 14, false);
      y_off += row_h + row_gap + 4;

      PROP_SEPARATOR();

      // ── TRANSFORM ────────────────────────────────────────────────
      nvgw_label(&ui->nvgw, x_ptr, y_off, w_ptr, 16, "TRANSFORM", NVG_ALIGN_LEFT, col_section, 10);
      y_off += 20;

      // Position: X | Y
      nvgw_label(&ui->nvgw, x_ptr,              y_off, lbl_w, row_h, "X", NVG_ALIGN_LEFT, col_label, 12);
      PROP_SYNC(1, "%.1f", node->x);
      if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 1), x_ptr + lbl_w, y_off, half_w - lbl_w, row_h, ui->prop_input_bufs[1], 64, 13, true))
        node->x = (float)atof(ui->prop_input_bufs[1]);

      nvgw_label(&ui->nvgw, x_ptr + half_w + 6, y_off, lbl_w, row_h, "Y", NVG_ALIGN_LEFT, col_label, 12);
      PROP_SYNC(2, "%.1f", node->y);
      if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 2), x_ptr + half_w + 6 + lbl_w, y_off, half_w - lbl_w, row_h, ui->prop_input_bufs[2], 64, 13, true))
        node->y = (float)atof(ui->prop_input_bufs[2]);
      y_off += row_h + row_gap;

      // Size: W | H
      nvgw_label(&ui->nvgw, x_ptr,              y_off, lbl_w, row_h, "W", NVG_ALIGN_LEFT, col_label, 12);
      PROP_SYNC(3, "%.1f", node->width);
      if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 3), x_ptr + lbl_w, y_off, half_w - lbl_w, row_h, ui->prop_input_bufs[3], 64, 13, true)) {
        float old = node->width; node->width = (float)atof(ui->prop_input_bufs[3]);
        if (node->lock_ratio && old > 0.0f) node->height *= (node->width / old);
      }
      nvgw_label(&ui->nvgw, x_ptr + half_w + 6, y_off, lbl_w, row_h, "H", NVG_ALIGN_LEFT, col_label, 12);
      PROP_SYNC(4, "%.1f", node->height);
      if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 4), x_ptr + half_w + 6 + lbl_w, y_off, half_w - lbl_w, row_h, ui->prop_input_bufs[4], 64, 13, true)) {
        float old = node->height; node->height = (float)atof(ui->prop_input_bufs[4]);
        if (node->lock_ratio && old > 0.0f) node->width *= (node->height / old);
      }
      y_off += row_h + row_gap;

      // Rotation | Opacity (0-100)
      nvgw_label(&ui->nvgw, x_ptr,              y_off, lbl_w, row_h, "Rotation", NVG_ALIGN_LEFT, col_label, 12);
      PROP_SYNC(12, "%.1f", node->rotation);
      if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 12), x_ptr + lbl_w, y_off, half_w - lbl_w, row_h, ui->prop_input_bufs[12], 64, 13, true))
        node->rotation = (float)atof(ui->prop_input_bufs[12]);

      nvgw_label(&ui->nvgw, x_ptr + half_w + 6, y_off, lbl_w, row_h, "Opacity", NVG_ALIGN_LEFT, col_label, 12);
      PROP_SYNC(13, "%.0f", node->opacity * 100.0f);
      if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 13), x_ptr + half_w + 6 + lbl_w, y_off, half_w - lbl_w, row_h, ui->prop_input_bufs[13], 64, 13, true)) {
        float v = (float)atof(ui->prop_input_bufs[13]) / 100.0f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        node->opacity = v;
      }
      y_off += row_h + row_gap + 4;

      PROP_SEPARATOR();

      if (node->type != NODE_TYPE_CHART) {

      // ── STYLE ────────────────────────────────────────────────────
      nvgw_label(&ui->nvgw, x_ptr, y_off, w_ptr, 16, "STYLE", NVG_ALIGN_LEFT, col_section, 10);
      y_off += 20;

      // Helper: open the floating HSV picker initialised from a uint32 RGBA field
#define OPEN_PICKER(field_ptr, swatch_x, swatch_y) do { \
        uint32_t _c = *(field_ptr); \
        float _r = ((_c >> 24) & 0xFF) / 255.0f; \
        float _g = ((_c >> 16) & 0xFF) / 255.0f; \
        float _b = ((_c >>  8) & 0xFF) / 255.0f; \
        float _a = ( _c        & 0xFF) / 255.0f; \
        float _mx = _r>_g?(_r>_b?_r:_b):(_g>_b?_g:_b); \
        float _mn = _r<_g?(_r<_b?_r:_b):(_g<_b?_g:_b); \
        float _d  = _mx - _mn; \
        ui->color_picker_v = _mx; \
        ui->color_picker_s = (_mx > 0.001f) ? _d / _mx : 0.0f; \
        if      (_d < 0.001f)  ui->color_picker_h = 0.0f; \
        else if (_mx == _r)    ui->color_picker_h = fmodf((_g - _b) / _d / 6.0f + 1.0f, 1.0f); \
        else if (_mx == _g)    ui->color_picker_h = ((_b - _r) / _d + 2.0f) / 6.0f; \
        else                   ui->color_picker_h = ((_r - _g) / _d + 4.0f) / 6.0f; \
        ui->color_picker_a    = _a; \
        ui->color_picker_ptr  = (field_ptr); \
        ui->color_picker_open = 1; \
        ui->color_picker_drag = 0; \
        ui->color_picker_x    = abs_x + (swatch_x); \
        ui->color_picker_y    = ui->abs_offset_y + (swatch_y) + row_h + 2.0f; \
      } while(0)

      // Shape type switcher (only for geometric shape nodes)
      {
        int is_shape = (node->type == NODE_TYPE_RECT     ||
                        node->type == NODE_TYPE_ELLIPSE  ||
                        node->type == NODE_TYPE_TRIANGLE ||
                        node->type == NODE_TYPE_PENTAGON ||
                        node->type == NODE_TYPE_HEXAGON  ||
                        node->type == NODE_TYPE_OCTAGON  ||
                        node->type == NODE_TYPE_STAR);
        if (is_shape) {
          nvgw_label(&ui->nvgw, x_ptr, y_off, w_ptr, 14, "Shape", NVG_ALIGN_LEFT, col_label, 11);
          y_off += 18;

          static const struct { NodeType type; const char *label; } shapes[] = {
            { NODE_TYPE_RECT,     "Rect"     },
            { NODE_TYPE_ELLIPSE,  "Circle"   },
            { NODE_TYPE_TRIANGLE, "Triangle" },
            { NODE_TYPE_PENTAGON, "Pentagon" },
            { NODE_TYPE_HEXAGON,  "Hexagon"  },
            { NODE_TYPE_OCTAGON,  "Octagon"  },
            { NODE_TYPE_STAR,     "Star"     },
          };
          const int n_shapes = 7;
          const int sh_cols = 4;
          const float sh_gap = 4.0f;
          float sh_btn_w = (w_ptr - (sh_cols - 1) * sh_gap) / sh_cols;
          float sh_btn_h = 24.0f;

          for (int si = 0; si < n_shapes; si++) {
            int r = si / sh_cols, c = si % sh_cols;
            float bx = x_ptr + c * (sh_btn_w + sh_gap);
            float by = y_off + r * (sh_btn_h + sh_gap);
            bool active = (node->type == shapes[si].type);
            float mx = ui->nvgw.mouse_x - ui->nvgw.offset_x;
            float my = ui->nvgw.mouse_y - ui->nvgw.offset_y;
            bool hov = mx >= bx && mx < bx+sh_btn_w && my >= by && my < by+sh_btn_h;
            NVGcolor bg = active
              ? nvgRGBA(60,120,220,255)
              : (hov ? (dark ? nvgRGBA(60,62,74,255) : nvgRGBA(210,212,222,255))
                     : (dark ? nvgRGBA(45,47,57,255) : nvgRGBA(225,226,234,255)));
            NVGcolor fg = active
              ? nvgRGBA(255,255,255,255)
              : (dark ? nvgRGBA(180,182,196,255) : nvgRGBA(55,57,75,255));
            nvgBeginPath(vg); nvgRoundedRect(vg, bx, by, sh_btn_w, sh_btn_h, 5.0f);
            nvgFillColor(vg, bg); nvgFill(vg);
            nvgFontSize(vg, 11.0f); nvgFontFaceId(vg, ui->nvgw.font_main);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, fg);
            nvgText(vg, bx + sh_btn_w * 0.5f, by + sh_btn_h * 0.5f, shapes[si].label, NULL);
            if (hov && ui->nvgw.mouse_clicked) {
              node->type = shapes[si].type;
              ui_request_redraw(ui);
            }
          }
          int sh_rows = (n_shapes + sh_cols - 1) / sh_cols;
          y_off += sh_rows * (sh_btn_h + sh_gap) + 4.0f;

          // Image container toggle + info
          {
            const float tog_w = 52.0f;
            bool img_on = (node->is_container == 1);
            float mx = ui->nvgw.mouse_x - ui->nvgw.offset_x;
            float my = ui->nvgw.mouse_y - ui->nvgw.offset_y;
            // Label
            nvgw_label(&ui->nvgw, x_ptr, y_off, lbl_w, row_h, "Image", NVG_ALIGN_LEFT, col_label, 13);
            // Toggle button
            float tog_x = x_ptr + lbl_w;
            NVGcolor tog_bg = img_on ? nvgRGBA(60,120,220,255)
                                     : (dark ? nvgRGBA(50,52,62,255) : nvgRGBA(222,224,232,255));
            NVGcolor tog_fg = img_on ? nvgRGBA(255,255,255,255)
                                     : (dark ? nvgRGBA(150,152,168,255) : nvgRGBA(90,92,110,255));
            nvgBeginPath(vg); nvgRoundedRect(vg, tog_x, y_off, tog_w, row_h, 4.0f);
            nvgFillColor(vg, tog_bg); nvgFill(vg);
            nvgFontSize(vg, 11.0f); nvgFontFaceId(vg, ui->nvgw.font_main);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, tog_fg);
            nvgText(vg, tog_x + tog_w * 0.5f, y_off + row_h * 0.5f, img_on ? "On" : "Off", NULL);
            bool hov_tog = mx >= tog_x && mx < tog_x+tog_w && my >= y_off && my < y_off+row_h;
            if (hov_tog && ui->nvgw.mouse_clicked) {
              node->is_container = img_on ? 0 : 1;
              if (!node->is_container) node->image_path[0] = '\0';
              ui_request_redraw(ui);
            }
            // When active: show filename + clear button
            if (img_on && node->image_path[0]) {
              float clr_w = 22.0f, clr_x = x_ptr + w_ptr - clr_w;
              // Filename (truncated)
              const char *fname = node->image_path;
              const char *slash = strrchr(fname, '/');
              if (slash) fname = slash + 1;
              nvgFontSize(vg, 11.0f); nvgFontFaceId(vg, ui->nvgw.font_main);
              nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
              nvgFillColor(vg, dark ? nvgRGBA(160,162,180,255) : nvgRGBA(80,82,100,255));
              nvgScissor(vg, tog_x + tog_w + 4, y_off, clr_x - tog_x - tog_w - 8, row_h);
              nvgText(vg, tog_x + tog_w + 6, y_off + row_h * 0.5f, fname, NULL);
              nvgResetScissor(vg);
              // Clear (×) button
              bool hov_clr = mx >= clr_x && mx < clr_x+clr_w && my >= y_off && my < y_off+row_h;
              NVGcolor clr_bg = hov_clr ? nvgRGBA(200,60,60,255) : (dark ? nvgRGBA(70,40,40,255) : nvgRGBA(240,220,220,255));
              nvgBeginPath(vg); nvgRoundedRect(vg, clr_x, y_off, clr_w, row_h, 4.0f);
              nvgFillColor(vg, clr_bg); nvgFill(vg);
              nvgFontSize(vg, 13.0f); nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
              nvgFillColor(vg, nvgRGBA(220,80,80,255));
              nvgText(vg, clr_x + clr_w * 0.5f, y_off + row_h * 0.5f, "×", NULL);
              if (hov_clr && ui->nvgw.mouse_clicked) {
                node->image_path[0] = '\0';
                ui_request_redraw(ui);
              }
            }
            y_off += row_h + row_gap;
          }

          // Lock ratio toggle
          {
            nvgw_label(&ui->nvgw, x_ptr, y_off, lbl_w, row_h, "Ratio", NVG_ALIGN_LEFT, col_label, 13);
            bool locked = (node->lock_ratio == 1);
            const float tog_w = 52.0f;
            float tog_x = x_ptr + lbl_w;
            float mx = ui->nvgw.mouse_x - ui->nvgw.offset_x;
            float my = ui->nvgw.mouse_y - ui->nvgw.offset_y;
            NVGcolor tog_bg = locked ? nvgRGBA(60,120,220,255)
                                     : (dark ? nvgRGBA(50,52,62,255) : nvgRGBA(222,224,232,255));
            NVGcolor tog_fg = locked ? nvgRGBA(255,255,255,255)
                                     : (dark ? nvgRGBA(150,152,168,255) : nvgRGBA(90,92,110,255));
            nvgBeginPath(vg); nvgRoundedRect(vg, tog_x, y_off, tog_w, row_h, 4.0f);
            nvgFillColor(vg, tog_bg); nvgFill(vg);
            nvgFontSize(vg, 11.0f); nvgFontFaceId(vg, ui->nvgw.font_main);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, tog_fg);
            nvgText(vg, tog_x + tog_w * 0.5f, y_off + row_h * 0.5f, locked ? "Locked" : "Free", NULL);
            bool hov_tog = mx >= tog_x && mx < tog_x+tog_w && my >= y_off && my < y_off+row_h;
            if (hov_tog && ui->nvgw.mouse_clicked) {
              node->lock_ratio = locked ? 0 : 1;
              ui_request_redraw(ui);
            }
            y_off += row_h + row_gap;
          }
        }
      }

      // Fill [swatch][Grad]  |  Radius [input]  — single row
      {
        const float fl = 36.0f;  // "Fill" label width
        const float gr = 38.0f;  // "Grad" button width
        const float rl = 36.0f;  // "Rad" label width
        const float gap4 = 4.0f;
        float fill_sw = half_w - fl - gap4 - gr;   // swatch width
        float rad_inp = half_w - rl - 6.0f;        // radius input width

        // Fill label + swatch
        nvgw_label(&ui->nvgw, x_ptr, y_off, fl, row_h, "Fill", NVG_ALIGN_LEFT, col_label, 13);
        NVGcolor fill_col = nvgRGBA((node->fill_color >> 24) & 0xFF, (node->fill_color >> 16) & 0xFF,
                                    (node->fill_color >> 8) & 0xFF, node->fill_color & 0xFF);
        if (nvgw_color_picker(&ui->nvgw, nvgw_id(node, 5), x_ptr + fl, y_off, fill_sw, row_h, &fill_col))
          OPEN_PICKER(&node->fill_color, x_ptr + fl, y_off);

        // Grad toggle
        bool grad_on = (node->fill_type == 1);
        float gx = x_ptr + fl + fill_sw + gap4;
        NVGcolor gbg = grad_on ? nvgRGBA(60,120,220,255) : (dark ? nvgRGBA(50,52,62,255) : nvgRGBA(222,224,232,255));
        NVGcolor gfg = grad_on ? nvgRGBA(255,255,255,255) : (dark ? nvgRGBA(150,152,168,255) : nvgRGBA(90,92,110,255));
        nvgBeginPath(vg); nvgRoundedRect(vg, gx, y_off, gr, row_h, 4.0f);
        nvgFillColor(vg, gbg); nvgFill(vg);
        nvgFontSize(vg, 11.0f); nvgFontFaceId(vg, ui->nvgw.font_main);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, gfg);
        nvgText(vg, gx + gr * 0.5f, y_off + row_h * 0.5f, "Grad", NULL);
        {
          float _mx = ui->nvgw.mouse_x - ui->nvgw.offset_x;
          float _my = ui->nvgw.mouse_y - ui->nvgw.offset_y;
          if (_mx >= gx && _mx < gx+gr && _my >= y_off && _my < y_off+row_h && ui->nvgw.mouse_clicked) {
            node->fill_type = grad_on ? 0 : 1;
            if (node->fill_type == 1 && node->fill_color_2 == 0) node->fill_color_2 = 0xFFFFFFFF;
            ui_request_redraw(ui);
          }
        }

        // Radius (right half) — only for non-line nodes
        if (node->type != NODE_TYPE_LINE && node->type != NODE_TYPE_STROKE) {
          float rx = x_ptr + half_w + 6;
          nvgw_label(&ui->nvgw, rx, y_off, rl, row_h, "Radius", NVG_ALIGN_LEFT, col_label, 12);
          PROP_SYNC(7, "%.1f", node->corner_radius);
          if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 7), rx + rl, y_off, rad_inp, row_h,
                              ui->prop_input_bufs[7], 64, 12, true))
            node->corner_radius = (float)atof(ui->prop_input_bufs[7]);
        }
      }
      y_off += row_h + row_gap;

      // Gradient row: Color 2 | Angle (only when gradient active)
      if (node->fill_type == 1) {
        const float c2l = 42.0f;
        float sw2 = half_w - c2l - 3;
        nvgw_label(&ui->nvgw, x_ptr, y_off, c2l, row_h, "Color 2", NVG_ALIGN_LEFT, col_label, 12);
        NVGcolor c2 = nvgRGBA((node->fill_color_2 >> 24) & 0xFF, (node->fill_color_2 >> 16) & 0xFF,
                              (node->fill_color_2 >> 8) & 0xFF, node->fill_color_2 & 0xFF);
        if (nvgw_color_picker(&ui->nvgw, nvgw_id(node, 19), x_ptr + c2l, y_off, sw2, row_h, &c2))
          OPEN_PICKER(&node->fill_color_2, x_ptr + c2l, y_off);
        float ax = x_ptr + half_w + 6;
        const float albl = 38.0f;
        nvgw_label(&ui->nvgw, ax, y_off, albl, row_h, "Angle", NVG_ALIGN_LEFT, col_label, 12);
        PROP_SYNC(15, "%.0f", node->gradient_angle);
        if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 15), ax + albl, y_off,
                            half_w - albl - 6, row_h, ui->prop_input_bufs[15], 64, 12, true))
          node->gradient_angle = (float)atof(ui->prop_input_bufs[15]);
        y_off += row_h + row_gap;
      }
      y_off += 2;

      } // end if not chart

      // ── TYPE-SPECIFIC ─────────────────────────────────────────────
      if (node->type == NODE_TYPE_TEXT) {
        PROP_SEPARATOR();
        nvgw_label(&ui->nvgw, x_ptr, y_off, w_ptr, 16, "TEXT", NVG_ALIGN_LEFT, col_section, 10);
        y_off += 20;

        // ── Font selector ─────────────────────────────────────────
        nvgw_label(&ui->nvgw, x_ptr, y_off, lbl_w, row_h, "Font", NVG_ALIGN_LEFT, col_label, 13);
        {
          const char *builtin_names[]  = { "Inter", "Bold" };
          const char *builtin_faces[]  = { "Inter", "Bold" };
          int n_total = 2 + ui->custom_font_count;
          if (n_total > 6) n_total = 6;
          int n_row1 = n_total < 3 ? n_total : 3; // up to 3 per row
          float bw = (w_ptr - lbl_w - (n_row1 - 1) * 3.0f) / (float)n_row1;
          for (int fi = 0; fi < n_total; fi++) {
            const char *fname = (fi < 2) ? builtin_names[fi] : ui->custom_fonts[fi - 2].name;
            const char *fface = (fi < 2) ? builtin_faces[fi] : ui->custom_fonts[fi - 2].name;
            int row_i = fi / n_row1, col_i = fi % n_row1;
            float bx = x_ptr + lbl_w + col_i * (bw + 3);
            float by = y_off + row_i * (row_h + row_gap);
            if (nvgw_button(&ui->nvgw, nvgw_id(node, 20 + fi), bx, by, bw, row_h, fname)) {
              strncpy(node->font_face, fface, 63);
              node->font_face[63] = '\0';
            }
            // active indicator: blue bottom border
            bool active = (fi == 0 && node->font_face[0] == '\0') ||
                          strcmp(node->font_face, fface) == 0;
            if (active) {
              nvgBeginPath(vg);
              nvgRect(vg, bx + 2, by + row_h - 2, bw - 4, 2);
              nvgFillColor(vg, nvgRGBA(80, 120, 220, 255));
              nvgFill(vg);
            }
          }
          // Import button at end of row 1
          int import_col = n_total % n_row1;
          float imp_x = x_ptr + lbl_w + import_col * (bw + 3);
          float imp_y = y_off + (n_total / n_row1) * (row_h + row_gap);
          if (nvgw_button(&ui->nvgw, nvgw_id_str("FontImport"), imp_x, imp_y, bw, row_h, "Import...")) {
            ui_host_open_and_import_font(ui);
          }
          int total_rows = (n_total + 1 + n_row1 - 1) / n_row1; // include import slot
          y_off += total_rows * (row_h + row_gap);
        }

        // ── Size + Align row ──────────────────────────────────────
        {
          float size_lbl = 30.0f;
          float size_inp = 48.0f;
          float align_x = x_ptr + size_lbl + size_inp + 8;
          float align_bw = (w_ptr - size_lbl - size_inp - 8 - 2 * 3.0f) / 3.0f;
          nvgw_label(&ui->nvgw, x_ptr, y_off, size_lbl, row_h, "Size", NVG_ALIGN_LEFT, col_label, 13);
          PROP_SYNC(8, "%.1f", node->font_size);
          if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 8), x_ptr + size_lbl, y_off, size_inp, row_h, ui->prop_input_bufs[8], 64, 13, true))
            node->font_size = (float)atof(ui->prop_input_bufs[8]);
          const char *aligns[] = { "L", "C", "R" };
          for (int ai = 0; ai < 3; ai++) {
            float ax2 = align_x + ai * (align_bw + 3);
            if (nvgw_button(&ui->nvgw, nvgw_id(node, 30 + ai), ax2, y_off, align_bw, row_h, aligns[ai])) {
              node->text_align = ai;
            }
            if (node->text_align == ai) {
              nvgBeginPath(vg);
              nvgRect(vg, ax2 + 2, y_off + row_h - 2, align_bw - 4, 2);
              nvgFillColor(vg, nvgRGBA(80, 120, 220, 255));
              nvgFill(vg);
            }
          }
        }
        y_off += row_h + row_gap;

        // ── Line height + Letter spacing ──────────────────────────
        {
          float half = (w_ptr - 6) / 2.0f;
          float lh_lbl = 38.0f, ls_lbl = 46.0f;
          nvgw_label(&ui->nvgw, x_ptr, y_off, lh_lbl, row_h, "Lead", NVG_ALIGN_LEFT, col_label, 12);
          PROP_SYNC(16, "%.2f", node->line_height > 0.0f ? node->line_height : 1.2f);
          if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 16), x_ptr + lh_lbl, y_off, half - lh_lbl, row_h, ui->prop_input_bufs[16], 64, 13, true)) {
            float v = (float)atof(ui->prop_input_bufs[16]);
            node->line_height = (v > 0.3f && v < 5.0f) ? v : 1.2f;
          }
          nvgw_label(&ui->nvgw, x_ptr + half + 6, y_off, ls_lbl, row_h, "Spacing", NVG_ALIGN_LEFT, col_label, 12);
          PROP_SYNC(17, "%.1f", node->letter_spacing);
          if (nvgw_text_input(&ui->nvgw, nvgw_id(node, 17), x_ptr + half + 6 + ls_lbl, y_off, half - ls_lbl, row_h, ui->prop_input_bufs[17], 64, 13, true))
            node->letter_spacing = (float)atof(ui->prop_input_bufs[17]);
        }
        y_off += row_h + row_gap;

        // ── Content ───────────────────────────────────────────────
        nvgw_label(&ui->nvgw, x_ptr, y_off, lbl_w, row_h, "Content", NVG_ALIGN_LEFT, col_label, 13);
        nvgw_text_input(&ui->nvgw, nvgw_id(node, 9), x_ptr + lbl_w, y_off, w_ptr - lbl_w, row_h, node->text, sizeof(node->text), 13, false);
        y_off += row_h + row_gap;

        // ── Linked column ─────────────────────────────────────────
        if (dd->linked_table_id && engine && engine->doc) {
          AslanTable *ltbl = NULL;
          for (int pi = 0; pi < 32 && !ltbl; pi++) {
            Page *pp = &engine->doc->pages[pi];
            if (!pp->in_use) continue;
            Block *tb = page_find_block_by_id(pp, dd->linked_table_id);
            if (tb && tb->type == BLOCK_TABLE && tb->data.table.columnar)
              ltbl = tb->data.table.columnar;
          }
          if (ltbl && ltbl->column_count > 0) {
            PROP_SEPARATOR();
            nvgw_label(&ui->nvgw, x_ptr, y_off, w_ptr, 16, "LINK TO COLUMN", NVG_ALIGN_LEFT, col_section, 10);
            y_off += 20;
            int ncols = (int)ltbl->column_count;
            if (ncols > 8) ncols = 8;
            int cols_per_row = 3;
            float cbw = (w_ptr - (cols_per_row - 1) * 3.0f) / (float)cols_per_row;
            for (int ci = 0; ci < ncols; ci++) {
              int row_i = ci / cols_per_row, col_i = ci % cols_per_row;
              float cbx = x_ptr + col_i * (cbw + 3);
              float cby = y_off + row_i * (row_h + row_gap);
              bool active = (node->linked_col_name[0] != '\0') &&
                            strcmp(node->linked_col_name, ltbl->columns[ci].name) == 0;
              if (nvgw_button(&ui->nvgw, nvgw_id(node, 40 + ci), cbx, cby, cbw, row_h, ltbl->columns[ci].name)) {
                if (active) {
                  node->linked_col_name[0] = '\0';
                } else {
                  strncpy(node->linked_col_name, ltbl->columns[ci].name, 63);
                  node->linked_col_name[63] = '\0';
                }
              }
              if (active) {
                nvgBeginPath(vg);
                nvgRect(vg, cbx + 2, cby + row_h - 2, cbw - 4, 2);
                nvgFillColor(vg, nvgRGBA(80, 120, 220, 255));
                nvgFill(vg);
              }
            }
            int col_rows = (ncols + cols_per_row - 1) / cols_per_row;
            y_off += col_rows * (row_h + row_gap);
          }
        }
      } else if (node->type == NODE_TYPE_IMAGE) {
        PROP_SEPARATOR();
        nvgw_label(&ui->nvgw, x_ptr, y_off, w_ptr, 16, "IMAGE", NVG_ALIGN_LEFT, col_section, 10);
        y_off += 20;
        if (nvgw_button(&ui->nvgw, nvgw_id(node, 10), x_ptr, y_off, w_ptr, row_h, "Select Image...")) {
          char new_path[512];
          if (open_image_file_dialog(new_path, sizeof(new_path))) {
            char doc_dir[512] = ".";
            if (engine->doc->filepath[0]) {
              strncpy(doc_dir, engine->doc->filepath, 511);
              char *ls = strrchr(doc_dir, '/'); if (ls) *ls = '\0';
            }
            copy_image_to_assets(new_path, doc_dir, node->image_path, sizeof(node->image_path));
          }
        }
        y_off += row_h + row_gap;
      } else if (node->type == NODE_TYPE_CHART && node->chart_data) {
        ChartData *cd = node->chart_data;
        PROP_SEPARATOR();
        nvgw_label(&ui->nvgw, x_ptr, y_off, w_ptr, 16, "CHART", NVG_ALIGN_LEFT, col_section, 10);
        y_off += 20;
        // 4×4 grid of chart / view type buttons
        {
          static const char *ct_labels[] = {
            "Bar","Line","Pie","Gantt",
            "Donut","Area","H-Bar","Scatter",
            "Stacked","Waterfall","Radar","Funnel",
            "Kanban","To Do"
          };
          const int n_types = 14;
          const int n_cols  = 4;
          const float btn_h = 26.0f;
          const float gap   = 4.0f;
          float btn_w = (w_ptr - gap * (n_cols - 1)) / n_cols;
          NVGcontext *vg2 = ui->vg;
          for (int ti = 0; ti < n_types; ti++) {
            int col = ti % n_cols;
            int row2 = ti / n_cols;
            float bx = x_ptr + col * (btn_w + gap);
            float by = y_off + row2 * (btn_h + gap);
            bool sel = (int)cd->type == ti;
            // Draw background manually to show selection state
            NVGcolor btn_bg = sel
              ? (dark ? nvgRGBA(60,120,220,255) : nvgRGBA(60,120,220,255))
              : (dark ? nvgRGBA(50,52,60,255)   : nvgRGBA(230,232,238,255));
            NVGcolor btn_fg = sel ? nvgRGBA(255,255,255,255)
              : (dark ? nvgRGBA(180,182,196,255) : nvgRGBA(60,62,76,255));
            nvgBeginPath(vg2);
            nvgRoundedRect(vg2, bx, by, btn_w, btn_h, 4.0f);
            nvgFillColor(vg2, btn_bg);
            nvgFill(vg2);
            nvgSave(vg2);
            nvgFontSize(vg2, 12.0f);
            nvgFontFaceId(vg2, ui->nvgw.font_main);
            nvgTextAlign(vg2, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg2, btn_fg);
            nvgText(vg2, bx + btn_w * 0.5f, by + btn_h * 0.5f, ct_labels[ti], NULL);
            nvgRestore(vg2);
            // Hit test (inline — point_in_rect is static to nvg_widgets.c)
            float _lx = ui->nvgw.mouse_x - ui->nvgw.offset_x;
            float _ly = ui->nvgw.mouse_y - ui->nvgw.offset_y;
            bool hov = _lx >= bx && _lx < bx+btn_w && _ly >= by && _ly < by+btn_h;
            if (hov && ui->nvgw.mouse_clicked)
              cd->type = (ChartType)ti;
          }
          y_off += 4 * btn_h + 3*gap + row_gap;
        }
      }

      (void)y_off; // suppress unused warning
#undef PROP_SEPARATOR
#undef OPEN_PICKER
#undef PROP_SYNC
      } // end properties block
      nvgRestore(vg);
      nvgResetScissor(vg);
      // Restore nvgw offset
      nvgw_set_offset(&ui->nvgw, ui->nvgw.offset_x, ui->abs_offset_y);
    } // end selected_node
  } else if (ui->design_bottom_tab == 1) {
    // ── Layers ────────────────────────────────────────────────────
    ui_render_sidebar_layers(ui, block, cx, cy, cw, ch_h);
  } else if (ui->design_bottom_tab == 2) {
    // ── Page settings ─────────────────────────────────────────────
    float y_p = cy + 12.0f;
    float x_p = cx;
    float w_p = cw;
    NVGcolor col_s = dark ? nvgRGBA(110, 112, 128, 255) : nvgRGBA(140, 142, 158, 255);
    NVGcolor col_l = dark ? nvgRGBA(160, 162, 178, 255) : nvgRGBA(110, 112, 130, 255);
    nvgw_label(&ui->nvgw, x_p, y_p, w_p, 16, "CANVAS", NVG_ALIGN_LEFT, col_s, 10);
    y_p += 20;
    bool nl = dd->no_limits;
    if (nvgw_checkbox(&ui->nvgw, nvgw_id_str("InfCanv"), x_p, y_p, w_p, 28, "Infinite Canvas", &nl)) { dd->no_limits = nl; }
    y_p += 34;
    if (!dd->no_limits) {
      // Unit selector — 3 buttons: px / in / cm
      {
        const char *unit_lbls[] = {"px", "in", "cm"};
        const float ubtn_h = 24.0f, ubtn_gap = 4.0f;
        float ubtn_w = (w_p - ubtn_gap * 2) / 3.0f;
        for (int ui2 = 0; ui2 < 3; ui2++) {
          float ux = x_p + ui2 * (ubtn_w + ubtn_gap);
          bool usel = dd->units == ui2;
          NVGcolor ubg = usel
            ? nvgRGBA(60, 120, 220, 255)
            : (dark ? nvgRGBA(50,52,60,255) : nvgRGBA(230,232,238,255));
          NVGcolor ufg = usel ? nvgRGBA(255,255,255,255)
            : (dark ? nvgRGBA(180,182,196,255) : nvgRGBA(60,62,76,255));
          nvgBeginPath(vg); nvgRoundedRect(vg, ux, y_p, ubtn_w, ubtn_h, 4.0f);
          nvgFillColor(vg, ubg); nvgFill(vg);
          nvgSave(vg);
          nvgFontSize(vg, 12.0f); nvgFontFaceId(vg, ui->nvgw.font_main);
          nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
          nvgFillColor(vg, ufg);
          nvgText(vg, ux + ubtn_w * 0.5f, y_p + ubtn_h * 0.5f, unit_lbls[ui2], NULL);
          nvgRestore(vg);
          float _lx2 = ui->nvgw.mouse_x - ui->nvgw.offset_x;
          float _ly2 = ui->nvgw.mouse_y - ui->nvgw.offset_y;
          if (_lx2 >= ux && _lx2 < ux+ubtn_w && _ly2 >= y_p && _ly2 < y_p+ubtn_h && ui->nvgw.mouse_clicked)
            dd->units = ui2;
        }
        y_p += ubtn_h + 6.0f;
      }
      // Pixel-per-unit conversion factor
      // 1 inch = 96 px (screen), 1 cm = 96/2.54 px
      float to_unit, from_unit;
      const char *unit_fmt;
      if (dd->units == 1) { to_unit = 1.0f/96.0f;    from_unit = 96.0f;           unit_fmt = "%.2f"; }
      else if (dd->units == 2) { to_unit = 2.54f/96.0f; from_unit = 96.0f/2.54f;  unit_fmt = "%.2f"; }
      else                  { to_unit = 1.0f;          from_unit = 1.0f;           unit_fmt = "%.0f"; }
      char bw[32], bh[32];
      snprintf(bw, 32, unit_fmt, dd->width  * to_unit);
      snprintf(bh, 32, unit_fmt, dd->height * to_unit);
      float hw = (w_p - 8) / 2.0f;
      nvgw_label(&ui->nvgw, x_p,        y_p, 24, 28, "W", NVG_ALIGN_LEFT, col_l, 12);
      if (nvgw_text_input(&ui->nvgw, nvgw_id_str("CanvW"), x_p + 24, y_p, hw - 24, 28, bw, 32, 13, true))
        dd->width  = (float)atof(bw) * from_unit;
      nvgw_label(&ui->nvgw, x_p + hw + 8, y_p, 24, 28, "H", NVG_ALIGN_LEFT, col_l, 12);
      if (nvgw_text_input(&ui->nvgw, nvgw_id_str("CanvH"), x_p + hw + 32, y_p, hw - 24, 28, bh, 32, 13, true))
        dd->height = (float)atof(bh) * from_unit;
      y_p += 34;
    }
    // Separator
    nvgBeginPath(vg); nvgMoveTo(vg, x_p, y_p + 4); nvgLineTo(vg, x_p + w_p, y_p + 4);
    nvgStrokeColor(vg, sep_col); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
    y_p += 14;
    nvgw_label(&ui->nvgw, x_p, y_p, w_p, 16, "BACKGROUND", NVG_ALIGN_LEFT, col_s, 10);
    y_p += 20;
    uint32_t _pg_bg = dd->pages[dd->current_page].bg_color;
    NVGcolor bgc = nvgRGBA((_pg_bg >> 24) & 0xFF, (_pg_bg >> 16) & 0xFF, (_pg_bg >> 8) & 0xFF, _pg_bg & 0xFF);
    {
      const float _rh = 28.0f;
#define OPEN_PICKER_PAGE(field_ptr, swatch_x, swatch_y) do { \
        uint32_t _c = *(field_ptr); \
        float _r2 = ((_c >> 24) & 0xFF) / 255.0f, _g2 = ((_c >> 16) & 0xFF) / 255.0f; \
        float _b2 = ((_c >>  8) & 0xFF) / 255.0f, _a2 = ( _c        & 0xFF) / 255.0f; \
        float _mx2=_r2>_g2?(_r2>_b2?_r2:_b2):(_g2>_b2?_g2:_b2); \
        float _mn2=_r2<_g2?(_r2<_b2?_r2:_b2):(_g2<_b2?_g2:_b2); \
        float _d2=_mx2-_mn2; \
        ui->color_picker_v=_mx2; \
        ui->color_picker_s=(_mx2>0.001f)?_d2/_mx2:0.0f; \
        if      (_d2<0.001f)   ui->color_picker_h=0.0f; \
        else if (_mx2==_r2)    ui->color_picker_h=fmodf((_g2-_b2)/_d2/6.0f+1.0f,1.0f); \
        else if (_mx2==_g2)    ui->color_picker_h=((_b2-_r2)/_d2+2.0f)/6.0f; \
        else                   ui->color_picker_h=((_r2-_g2)/_d2+4.0f)/6.0f; \
        ui->color_picker_a=_a2; ui->color_picker_ptr=(field_ptr); \
        ui->color_picker_open=1; ui->color_picker_drag=0; \
        ui->color_picker_x=abs_x+(swatch_x); \
        ui->color_picker_y=ui->abs_offset_y+(swatch_y)+_rh+2.0f; \
      } while(0)
      if (nvgw_color_picker(&ui->nvgw, nvgw_id_str("PgBG"), x_p, y_p, w_p, _rh, &bgc))
        OPEN_PICKER_PAGE(&dd->pages[dd->current_page].bg_color, x_p, y_p);
#undef OPEN_PICKER_PAGE
      y_p += _rh + 8;
    }
    // Separator
    nvgBeginPath(vg); nvgMoveTo(vg, x_p, y_p + 4); nvgLineTo(vg, x_p + w_p, y_p + 4);
    nvgStrokeColor(vg, sep_col); nvgStrokeWidth(vg, 1.0f); nvgStroke(vg);
    y_p += 14;
    nvgw_label(&ui->nvgw, x_p, y_p, w_p, 16, "EXPORT", NVG_ALIGN_LEFT, col_s, 10);
    y_p += 20;
    {
      const float exp_h = 28.0f;
      const float exp_gap = 4.0f;
      float hw2 = (w_p - exp_gap) / 2.0f;
      if (nvgw_button(&ui->nvgw, nvgw_id_str("ExpPNG"), x_p,           y_p, hw2, exp_h, "Export PNG"))
        ui_host_export_png_jpeg(ui, engine, 0);
      if (nvgw_button(&ui->nvgw, nvgw_id_str("ExpJPG"), x_p + hw2 + exp_gap, y_p, hw2, exp_h, "Export JPEG"))
        ui_host_export_png_jpeg(ui, engine, 1);
      y_p += exp_h + exp_gap;
      if (nvgw_button(&ui->nvgw, nvgw_id_str("ExpSVG"), x_p,           y_p, hw2, exp_h, "Export SVG"))
        ui_host_export_svg(ui, engine);
      if (nvgw_button(&ui->nvgw, nvgw_id_str("ExpPDF"), x_p + hw2 + exp_gap, y_p, hw2, exp_h, "Export PDF"))
        ui_host_export_pdf(ui, engine);
      y_p += exp_h + exp_gap;
    }
    (void)y_p;
  }

  (void)ch_h;
}

void ui_render_node_tree(UIHost *ui, Engine *engine, Block *block,
                         DesignData *dd, Node *node, AslanRect bounds,
                         float scale, float parent_x, float parent_y) {
  (void)bounds;
  if (!node || !node->visible)
    return;

  // Auto-layout
  if (node->is_container && node->layout_type > 0 && node->first_child) {
    float current_offset = 0;
    for (Node *child = node->first_child; child; child = child->next_sibling) {
      if (!child->visible)
        continue;
      if (node->layout_type == 1) { // V-Stack
        child->x = 0;
        child->y = current_offset;
        current_offset += child->height + node->layout_padding;
      } else if (node->layout_type == 2) { // H-Stack
        child->x = current_offset;
        child->y = 0;
        current_offset += child->width + node->layout_padding;
      }
    }
  }

  float sx = parent_x + node->x * scale;
  float sy = parent_y + node->y * scale;
  float sw = node->width * scale;
  float sh = node->height * scale;
  AslanRect nr = {sx, sy, sw, sh};
  // Highlight if hovered (Sidebar -> Canvas or Canvas -> Sidebar)
  if (ui->hovered_node_id == node->id) {
    NVGcolor highlight_col = nvgRGBA(110, 110, 130, 255);
    float rad_h = node->rotation * 3.14159f / 180.0f;
    float cx_h = sx + sw / 2.0f;
    float cy_h = sy + sh / 2.0f;
    if (node->type == NODE_TYPE_ELLIPSE) {
      nvgBeginPath(ui->vg);
      nvgEllipse(ui->vg, sx - 2 + (sw + 4) / 2.0f, sy - 2 + (sh + 4) / 2.0f,
                 (sw + 4) / 2.0f, (sh + 4) / 2.0f);
      nvgStrokeColor(ui->vg, highlight_col);
      nvgStrokeWidth(ui->vg, 2.0f);
      nvgStroke(ui->vg);
    } else if (node->type == NODE_TYPE_RECT || node->type == NODE_TYPE_TEXT ||
               node->type == NODE_TYPE_IMAGE || node->type == NODE_TYPE_LINE) {
      fill_rotated_rounded_rect(ui, cx_h, cy_h, sw + 2, sh + 2, rad_h,
                                node->corner_radius * scale, highlight_col, 1,
                                2.0f);
    } else {
      // Polygon shapes (triangle, pentagon, hexagon, octagon): rect outline
      // is acceptable as bounding box indicator
      fill_rotated_rounded_rect(ui, cx_h, cy_h, sw + 2, sh + 2, rad_h, 0,
                                highlight_col, 1, 2.0f);
    }
  }

  if (node->is_container && node->clip_content) {
    AslanRect sc = rect_intersect(nr, ui->artboard_clip);
    nvgScissor(ui->vg, sc.x, sc.y, sc.w, sc.h);
  }

  // --- Inject Shadow ---
  uint32_t f32 = node->fill_color;
  uint32_t s32 = node->stroke_color;
  float opacity = node->opacity;
  if (node->has_shadow && node->type != NODE_TYPE_LINE &&
      node->type != NODE_TYPE_STROKE && node->type != NODE_TYPE_CHART) {
    NVGcolor sh_col = nvgRGBA(
        (node->shadow_color >> 24) & 0xFF, (node->shadow_color >> 16) & 0xFF,
        (node->shadow_color >> 8) & 0xFF,
        (int)((node->shadow_color & 0xFF) * opacity));
    float sh_x = nr.x + node->shadow_offset_x * scale;
    float sh_y = nr.y + node->shadow_offset_y * scale;
    float b = node->shadow_blur * scale;
    if (b > 0) {
      NVGcolor sh_half = nvgRGBAf(sh_col.r, sh_col.g, sh_col.b, sh_col.a * 0.5f);
      fill_smooth_rounded_rect(
          ui, sh_x - b / 2, sh_y - b / 2, nr.w + b, nr.h + b,
          (node->corner_radius + b / 2) * scale, sh_half);
    }
    fill_smooth_rounded_rect(ui, sh_x, sh_y, nr.w, nr.h,
                             node->corner_radius * scale, sh_col);
  }
  NVGcolor fill = nvgRGBA(
      (f32 >> 24) & 0xFF, (f32 >> 16) & 0xFF, (f32 >> 8) & 0xFF,
      (int)((f32 & 0xFF) * opacity));
  NVGcolor stroke = nvgRGBA(
      (s32 >> 24) & 0xFF, (s32 >> 16) & 0xFF, (s32 >> 8) & 0xFF,
      (int)((s32 & 0xFF) * opacity));

  int drawn_image = 0;
  int has_fill = (node->fill_color & 0xFF) > 10;

  // Precompute gradient paint (used by all shape types)
  NVGpaint node_grad_paint;
  NVGpaint *gp = NULL;
  if (node->fill_type == 1 && has_fill) {
    NVGcolor c2 = nvgRGBA((node->fill_color_2 >> 24) & 0xFF,
                          (node->fill_color_2 >> 16) & 0xFF,
                          (node->fill_color_2 >>  8) & 0xFF,
                          (int)((node->fill_color_2 & 0xFF) * opacity));
    float grad_rad = node->gradient_angle * NVG_PI / 180.0f;
    float gdx = cosf(grad_rad) * sw * 0.5f;
    float gdy = sinf(grad_rad) * sh * 0.5f;
    float gcx = sx + sw * 0.5f, gcy = sy + sh * 0.5f;
    node_grad_paint = nvgLinearGradient(ui->vg,
                                        gcx - gdx, gcy - gdy,
                                        gcx + gdx, gcy + gdy,
                                        fill, c2);
    gp = &node_grad_paint;
  }

  // Precompute image paint for container shapes (used per shape type below)
  NVGpaint node_img_paint;
  NVGpaint *ip = NULL;
  if (node->is_container && node->image_path[0]) {
    int _cimg = get_image_texture(ui, node->image_path);
    if (_cimg) {
      float _iw = nr.w * (node->image_scale > 0.01f ? node->image_scale : 1.0f);
      float _ih = nr.h * (node->image_scale > 0.01f ? node->image_scale : 1.0f);
      node_img_paint = nvgImagePattern(ui->vg,
                                       nr.x + node->image_crop_x * scale,
                                       nr.y + node->image_crop_y * scale,
                                       _iw, _ih, 0.0f, _cimg, 1.0f);
      ip = &node_img_paint;
    }
  }

  if (node->type == NODE_TYPE_GROUP) {
    // Group: no background fill — just a faint dashed border so the
    // designer can see its bounds.  Children are rendered below in the
    // normal loop.
    nvgBeginPath(ui->vg);
    nvgRect(ui->vg, nr.x, nr.y, nr.w, nr.h);
    nvgStrokeColor(ui->vg, nvgRGBA(120, 120, 180, 60));
    nvgStrokeWidth(ui->vg, 1.0f);
    nvgStroke(ui->vg);
  } else if (node->type == NODE_TYPE_CHART && node->chart_data) {
    Page *curr_page = doc_page_get_active(engine->doc);
    ui_render_chart_node(ui, node, nr, curr_page);

    // Rendering configuration bar if selected
    if (ui->selected_node == node) {
      ui_render_chart_node_config_bar(ui, engine, node, nr, curr_page);
    }
  } else if (node->type == NODE_TYPE_TABLE_VIEW) {
    // ── Embedded live-table — mirrors ui_render_chart_impl pattern ──────
    NVGcontext *vg = ui->vg;
    // Find first TABLE block on active page
    AslanTable *tv_tbl = NULL;
    Page *_active = doc_page_get_active(engine->doc);
    if (_active) {
      for (Block *_b = _active->first_block; _b; _b = _b->next) {
        if (_b->type == BLOCK_TABLE && _b->data.table.columnar) {
          tv_tbl = _b->data.table.columnar;
          break;
        }
      }
    }

    // Card background (same as charts, drawn before nvgSave)
    nvgBeginPath(vg);
    nvgRoundedRect(vg, nr.x, nr.y, nr.w, nr.h, 6);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, nr.x, nr.y, nr.w, nr.h, 6);
    nvgStrokeColor(vg, nvgRGBA(220, 220, 228, 255));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // All content inside nvgSave/nvgRestore (same as charts)
    nvgSave(vg);
    nvgScissor(vg, nr.x + 1, nr.y + 1, nr.w - 2, nr.h - 2);

    if (tv_tbl && tv_tbl->column_count > 0) {
      int ncols = (int)tv_tbl->column_count;
      if (ncols > 32) ncols = 32;
      int nrows = (int)aslan_table_view_count(tv_tbl);

      const float HDR_H  = 28.0f;
      const float ROW_H  = 24.0f;
      const float PAD_X  = 8.0f;
      const float FONT   = 11.5f;

      // Equal column widths
      float cw = nr.w / (float)ncols;

      // ── Header ────────────────────────────────────────────────────────
      nvgBeginPath(vg);
      nvgRect(vg, nr.x, nr.y, nr.w, HDR_H);
      nvgFillColor(vg, nvgRGBA(40, 42, 58, 255));
      nvgFill(vg);

      nvgFontSize(vg, FONT);
      nvgFontFace(vg, "Inter");
      nvgFillColor(vg, nvgRGBA(200, 205, 220, 255));
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      for (int c = 0; c < ncols; c++) {
        float cx = nr.x + c * cw;
        if (c > 0) {
          nvgBeginPath(vg);
          nvgMoveTo(vg, cx, nr.y + 4);
          nvgLineTo(vg, cx, nr.y + HDR_H - 4);
          nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 40));
          nvgStrokeWidth(vg, 1.0f);
          nvgStroke(vg);
        }
        nvgText(vg, cx + PAD_X, nr.y + HDR_H * 0.5f,
                tv_tbl->columns[c].name, NULL);
      }

      // ── Rows ──────────────────────────────────────────────────────────
      float data_h = nr.h - HDR_H;
      int max_vis = (int)(data_h / ROW_H);
      int draw_rows = nrows < max_vis ? nrows : max_vis;

      for (int r = 0; r < draw_rows; r++) {
        float ry = nr.y + HDR_H + r * ROW_H;

        // Alternating stripe
        nvgBeginPath(vg);
        nvgRect(vg, nr.x, ry, nr.w, ROW_H);
        nvgFillColor(vg, (r & 1) ? nvgRGBA(245, 246, 250, 255)
                                  : nvgRGBA(252, 252, 255, 255));
        nvgFill(vg);

        // Row separator
        nvgBeginPath(vg);
        nvgMoveTo(vg, nr.x, ry + ROW_H - 0.5f);
        nvgLineTo(vg, nr.x + nr.w, ry + ROW_H - 0.5f);
        nvgStrokeColor(vg, nvgRGBA(220, 221, 230, 255));
        nvgStrokeWidth(vg, 0.5f);
        nvgStroke(vg);

        uint64_t row_id = aslan_table_view_row_id(tv_tbl, r);
        nvgFontSize(vg, FONT);
        nvgFontFace(vg, "Inter");
        nvgFillColor(vg, nvgRGBA(40, 42, 58, 255));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

        for (int c = 0; c < ncols; c++) {
          float cx = nr.x + c * cw;
          if (c > 0) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, cx, ry);
            nvgLineTo(vg, cx, ry + ROW_H);
            nvgStrokeColor(vg, nvgRGBA(220, 221, 230, 255));
            nvgStrokeWidth(vg, 0.5f);
            nvgStroke(vg);
          }
          char _buf[128];
          const char *val = aslan_table_get_cell_display(
              tv_tbl, c, row_id, _buf, sizeof(_buf));
          if (val && val[0])
            nvgText(vg, cx + PAD_X, ry + ROW_H * 0.5f, val, NULL);
        }
      }
    } else {
      // Placeholder
      nvgFontSize(vg, 12.0f);
      nvgFontFace(vg, "Inter");
      nvgFillColor(vg, nvgRGBA(160, 160, 175, 255));
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(vg, nr.x + nr.w * 0.5f, nr.y + nr.h * 0.5f,
              "No table data", NULL);
    }

    nvgRestore(vg);
  } else if (node->type == NODE_TYPE_STROKE && node->stroke_data) {
    Stroke *s = node->stroke_data;
    if (s->count >= 2) {
      NVGcolor st_col =
          nvgRGBA((s->color >> 24) & 0xFF, (s->color >> 16) & 0xFF,
                  (s->color >> 8) & 0xFF, s->color & 0xFF);

      AslanVec2 *pts =
          (AslanVec2 *)malloc(s->count * sizeof(AslanVec2));
      if (pts) {
        for (int k = 0; k < s->count; k++) {
          pts[k].x = bounds.x + s->points[k].x * scale;
          pts[k].y = bounds.y + s->points[k].y * scale;
        }
        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, pts[0].x, pts[0].y);
        for (int k = 1; k < s->count; k++) {
          nvgLineTo(ui->vg, pts[k].x, pts[k].y);
        }
        nvgLineCap(ui->vg, NVG_ROUND);
        nvgLineJoin(ui->vg, NVG_ROUND);
        nvgStrokeColor(ui->vg, st_col);
        nvgStrokeWidth(ui->vg, s->thickness * scale);
        nvgStroke(ui->vg);

        // Render Endings for Strokes (Consistency with Canvas)
        if (node->line_start_style || node->line_end_style) {
          float size =
              (s->thickness * scale > 2.0f ? s->thickness * scale : 2.0f) *
              4.0f;

          // Start Style
          if (node->line_start_style > 0) {
            float start_x = pts[0].x;
            float start_y = pts[0].y;
            float dx = pts[1].x - start_x;
            float dy = pts[1].y - start_y;
            float angle = atan2f(dy, dx);

            if (node->line_start_style == 1) { // Circle
              nvgBeginPath(ui->vg);
              nvgCircle(ui->vg, start_x, start_y, size / 2.0f);
              nvgFillColor(ui->vg,
                           st_col);
              nvgFill(ui->vg);
            } else if (node->line_start_style == 2) { // Arrow
              float arrow_angle = angle + 3.14159f;
              float tip_x = start_x;
              float tip_y = start_y;
              float base_angle1 = arrow_angle + 0.5f;
              float base_angle2 = arrow_angle - 0.5f;
              float p1x = tip_x + size * cosf(base_angle1);
              float p1y = tip_y + size * sinf(base_angle1);
              float p2x = tip_x + size * cosf(base_angle2);
              float p2y = tip_y + size * sinf(base_angle2);
              nvgBeginPath(ui->vg);
              nvgMoveTo(ui->vg, tip_x, tip_y);
              nvgLineTo(ui->vg, p1x, p1y);
              nvgLineTo(ui->vg, p2x, p2y);
              nvgClosePath(ui->vg);
              nvgFillColor(ui->vg,
                           st_col);
              nvgFill(ui->vg);
            }
          }

          // End Style
          if (node->line_end_style > 0) {
            float end_x = pts[s->count - 1].x;
            float end_y = pts[s->count - 1].y;
            float dx = end_x - pts[s->count - 2].x;
            float dy = end_y - pts[s->count - 2].y;
            float angle = atan2f(dy, dx);

            if (node->line_end_style == 1) { // Circle
              nvgBeginPath(ui->vg);
              nvgCircle(ui->vg, end_x, end_y, size / 2.0f);
              nvgFillColor(ui->vg,
                           st_col);
              nvgFill(ui->vg);
            } else if (node->line_end_style == 2) { // Arrow
              float tip_x = end_x;
              float tip_y = end_y;
              // Arrow tip at end, base points away
              // standard arrow at end points OUTWARD usually.
              // ui_canvas.c logic: base_angle = angle + PI +/- 0.5.
              // Wait, ui_canvas.c had angle (dy,dx).
              // If line goes right (0 rad), arrow should point right.
              // Tip at end. Base to left.
              // So angle + PI is correct for base calculation relative
              // to tip.
              float effective_angle = angle;
              float base_angle_a = effective_angle + 3.14159f + 0.5f;
              float base_angle_b = effective_angle + 3.14159f - 0.5f;

              float p1x = tip_x + size * cosf(base_angle_a);
              float p1y = tip_y + size * sinf(base_angle_a);
              float p2x = tip_x + size * cosf(base_angle_b);
              float p2y = tip_y + size * sinf(base_angle_b);
              nvgBeginPath(ui->vg);
              nvgMoveTo(ui->vg, tip_x, tip_y);
              nvgLineTo(ui->vg, p1x, p1y);
              nvgLineTo(ui->vg, p2x, p2y);
              nvgClosePath(ui->vg);
              nvgFillColor(ui->vg,
                           st_col);
              nvgFill(ui->vg);
            }
          }
        }

        free(pts);
      }
    }
  }

  if (node->type == NODE_TYPE_IMAGE) {
    int drawn = 0;
    const char *img_path_to_use = node->image_path;

    // Check if we have a linked image column
    if (node->linked_image_col_name[0] && block->data.design.linked_table_id) {
      Block *tbl_block = NULL;
      if (engine && engine->doc) {
        for (int i = 0; i < 32; i++) {
          Page *p = &engine->doc->pages[i];
          if (!p->in_use)
            continue;
          Block *tb =
              page_find_block_by_id(p, block->data.design.linked_table_id);
          if (tb && tb->type == BLOCK_TABLE && tb->data.table.columnar) {
            tbl_block = tb;
            break;
          }
        }
      }
      if (tbl_block) {
        AslanTable *tbl = tbl_block->data.table.columnar;
        int col_idx = -1;
        for (size_t k = 0; k < tbl->column_count; k++) {
          if (strcmp(tbl->columns[k].name, node->linked_image_col_name) == 0) {
            col_idx = (int)k;
            break;
          }
        }
        if (col_idx >= 0) {
          int row_count_view = (int)aslan_table_view_count(tbl);
          if (row_count_view > 0) {
            int r_idx = ui->design_linked_row_idx;
            if (r_idx >= row_count_view)
              r_idx = row_count_view - 1;
            uint64_t active_row_id = tbl->view_index[r_idx];
            const char *val = aslan_table_get_text(tbl, col_idx, active_row_id);
            if (val && val[0]) {
              img_path_to_use = val;
            }
          }
        }
      }
    }

    if (img_path_to_use && img_path_to_use[0]) {
      int img = get_image_texture(ui, img_path_to_use);
      if (img != 0) {
        // Image Cropping implementation

        // Clip image to node bounds intersected with artboard
        AslanRect sc = rect_intersect(nr, ui->artboard_clip);
        nvgScissor(ui->vg, sc.x, sc.y, sc.w, sc.h);

        // Calculate transformed image rect
        float img_w = nr.w * node->image_scale;
        float img_h = nr.h * node->image_scale;
        float img_x = nr.x + node->image_crop_x * scale;
        float img_y = nr.y + node->image_crop_y * scale;

        NVGpaint imgPaint = nvgImagePattern(ui->vg, img_x, img_y, img_w, img_h,
                                            0, img, 1.0f);
        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, img_x, img_y, img_w, img_h);
        nvgFillPaint(ui->vg, imgPaint);
        nvgFill(ui->vg);

        // Restore scissor back to artboard clip
        nvgScissor(ui->vg, ui->artboard_clip.x, ui->artboard_clip.y,
                   ui->artboard_clip.w, ui->artboard_clip.h);

        drawn = 1;
      }
    }
    if (!drawn) {
      nvgBeginPath(ui->vg);
      nvgRect(ui->vg, nr.x, nr.y, nr.w, nr.h);
      nvgFillColor(ui->vg, nvgRGB(200, 200, 200));
      nvgFill(ui->vg);
    }
  } else if (node->type == NODE_TYPE_RECT) {
    if (node->is_container && !node->image_path[0]) {
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, nr.x, nr.y, nr.w, nr.h,
                     node->corner_radius * scale);
      nvgStrokeColor(ui->vg, nvgRGBA(0, 0, 0, 25));
      nvgStrokeWidth(ui->vg, 1.0f);
      nvgStroke(ui->vg);

      if (ui->font_icons) {
        nvgFontSize(ui->vg, 20.0f);
        nvgFontFace(ui->vg, "Icons");
        nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 25));
        nvgTextAlign(ui->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        char icon_buf[16];
        snprintf(icon_buf, sizeof(icon_buf), "%s", ICON_ADD);
        nvgText(ui->vg, nr.x + nr.w / 2, nr.y + nr.h / 2, icon_buf, NULL);
      }
      drawn_image = 1;
    }
    if (!drawn_image && ip) {
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, nr.x, nr.y, nr.w, nr.h, node->corner_radius * scale);
      nvgFillPaint(ui->vg, *ip);
      nvgFill(ui->vg);
      drawn_image = 1;
    }
    if (!drawn_image && has_fill) {
      nvgSave(ui->vg);
      nvgTranslate(ui->vg, nr.x + nr.w / 2, nr.y + nr.h / 2);
      if (node->rotation != 0.0f) {
        nvgRotate(ui->vg, node->rotation * NVG_PI / 180.0f);
      }
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, -nr.w / 2, -nr.h / 2, nr.w, nr.h,
                     node->corner_radius * scale);

      if (node->fill_type == 1) { // Gradient
        NVGcolor c1 = fill;
        NVGcolor c2 = nvgRGBA((node->fill_color_2 >> 24) & 0xFF,
                              (node->fill_color_2 >> 16) & 0xFF,
                              (node->fill_color_2 >> 8) & 0xFF,
                              (int)((node->fill_color_2 & 0xFF) * opacity));
        float rad = node->gradient_angle * NVG_PI / 180.0f;
        float dx = cosf(rad) * nr.w / 2;
        float dy = sinf(rad) * nr.h / 2;
        NVGpaint paint = nvgLinearGradient(ui->vg, -dx, -dy, dx, dy, c1, c2);
        nvgFillPaint(ui->vg, paint);
      } else {
        nvgFillColor(ui->vg, fill);
      }
      nvgFill(ui->vg);
      nvgRestore(ui->vg);
    }

    // Line Endings (Start/End Styles)
    // Only apply if it looks like a line (thin)
    if (node->height < 8.0f || node->width < 8.0f) {
      float cx = sx + sw / 2.0f;
      float cy = sy + sh / 2.0f;
      float rad = node->rotation * 3.14159f / 180.0f;
      float c = cosf(rad);
      float s = sinf(rad);

      // Calculate geometric start/end relative to center, rotated
      // Assume standard line is horizontal (width > height)
      float half_w = sw / 2.0f;

      // Start point (left)
      float start_x = cx - half_w * c;
      float start_y = cy - half_w * s;

      // End point (right)
      float end_x = cx + half_w * c;
      float end_y = cy + half_w * s;

      float size = (node->stroke_width > 2.0f ? node->stroke_width : 2.0f) *
                   4.0f * scale;

      // Render Start Style
      if (node->line_start_style == 1) { // Circle
        nvgBeginPath(ui->vg);
        nvgCircle(ui->vg, start_x, start_y, size / 2.0f);
        nvgFillColor(ui->vg, stroke);
        nvgFill(ui->vg);
      } else if (node->line_start_style == 2) { // Arrow (pointing left/outward)
        // Rotate arrow points
        // Tip at start_x, start_y. Base towards center.
        float angle = rad + 3.14159f; // Pointing away from center (left)
        float tip_x = start_x;        // Tip exactly at geometric start
        float tip_y = start_y;

        // Base points
        float base_angle1 = angle + 2.5f; // ~145 deg
        float base_angle2 = angle - 2.5f;

        float p1x = tip_x + size * cosf(base_angle1);
        float p1y = tip_y + size * sinf(base_angle1);
        float p2x = tip_x + size * cosf(base_angle2);
        float p2y = tip_y + size * sinf(base_angle2);

        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, tip_x, tip_y);
        nvgLineTo(ui->vg, p1x, p1y);
        nvgLineTo(ui->vg, p2x, p2y);
        nvgClosePath(ui->vg);
        nvgFillColor(ui->vg, stroke);
        nvgFill(ui->vg);
      }

      // Render End Style
      if (node->line_end_style == 1) { // Circle
        nvgBeginPath(ui->vg);
        nvgCircle(ui->vg, end_x, end_y, size / 2.0f);
        nvgFillColor(ui->vg, stroke);
        nvgFill(ui->vg);
      } else if (node->line_end_style ==
                 2) {        // Arrow (pointing right/outward)
                             // Tip at end_x, end_y. Base towards center.
        float angle = rad;   // Pointing away from center (right)
        float tip_x = end_x; // Tip exactly at geometric end
        float tip_y = end_y;

        // Base points
        float base_angle1 = angle + 2.5f;
        float base_angle2 = angle - 2.5f;

        float p1x = tip_x + size * cosf(base_angle1);
        float p1y = tip_y + size * sinf(base_angle1);
        float p2x = tip_x + size * cosf(base_angle2);
        float p2y = tip_y + size * sinf(base_angle2);

        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, tip_x, tip_y);
        nvgLineTo(ui->vg, p1x, p1y);
        nvgLineTo(ui->vg, p2x, p2y);
        nvgClosePath(ui->vg);
        nvgFillColor(ui->vg, stroke);
        nvgFill(ui->vg);
      }
    }
  } else if (node->type == NODE_TYPE_LINE) {
    float line_y = nr.y + nr.h / 2.0f;
    float line_w = (node->stroke_width > 0 ? node->stroke_width : 2.0f) * scale;

    nvgBeginPath(ui->vg);
    nvgMoveTo(ui->vg, nr.x, line_y);
    nvgLineTo(ui->vg, nr.x + nr.w, line_y);
    NVGcolor line_col = stroke;
    nvgStrokeColor(ui->vg, line_col);
    nvgStrokeWidth(ui->vg, line_w);
    nvgStroke(ui->vg);

    float size = (line_w > 2.0f ? line_w : 2.0f) * 4.0f;

    // Start Style (Left)
    if (node->line_start_style > 0) {
      if (node->line_start_style == 1) { // Circle
        nvgBeginPath(ui->vg);
        nvgCircle(ui->vg, nr.x, line_y, size / 2.0f);
        nvgFillColor(ui->vg, line_col);
        nvgFill(ui->vg);
      } else if (node->line_start_style == 2) { // Arrow (Left <--)
        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, nr.x, line_y);
        nvgLineTo(ui->vg, nr.x + size, line_y - size / 2);
        nvgLineTo(ui->vg, nr.x + size, line_y + size / 2);
        nvgClosePath(ui->vg);
        nvgFillColor(ui->vg, line_col);
        nvgFill(ui->vg);
      }
    }

    // End Style (Right)
    if (node->line_end_style > 0) {
      if (node->line_end_style == 1) { // Circle
        nvgBeginPath(ui->vg);
        nvgCircle(ui->vg, nr.x + nr.w, line_y, size / 2.0f);
        nvgFillColor(ui->vg, line_col);
        nvgFill(ui->vg);
      } else if (node->line_end_style == 2) { // Arrow (Right -->)
        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, nr.x + nr.w, line_y);
        nvgLineTo(ui->vg, nr.x + nr.w - size, line_y - size / 2);
        nvgLineTo(ui->vg, nr.x + nr.w - size, line_y + size / 2);
        nvgClosePath(ui->vg);
        nvgFillColor(ui->vg, line_col);
        nvgFill(ui->vg);
      }
    }

  } else if (node->type == NODE_TYPE_PATH && node->path_points &&
             node->path_point_count > 1) {
    if (node->is_container && !node->image_path[0]) {
      // Empty container placeholder for PATH shape
      NVGcolor cbg = nvgRGBA(110, 110, 130, 40);
      NVGcolor cst = nvgRGBA(110, 110, 130, 255);
      float poly_pts[512 * 2];
      int poly_count =
          node_get_path_polygon(node, poly_pts, 512, sx, sy, scale);
      if (poly_count > 2) {
        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, poly_pts[0], poly_pts[1]);
        for (int _pi = 1; _pi < poly_count; _pi++)
          nvgLineTo(ui->vg, poly_pts[_pi * 2], poly_pts[_pi * 2 + 1]);
        nvgClosePath(ui->vg);
        nvgFillColor(ui->vg, cbg);
        nvgFill(ui->vg);
        nvgStrokeColor(ui->vg, cst);
        nvgStrokeWidth(ui->vg, 1.0f);
        nvgStroke(ui->vg);
      }
      if (ui->font_icons) {
        float ir_cx = nr.x + nr.w / 2;
        float ir_cy = nr.y + nr.h / 2;
        nvgFontFaceId(ui->vg, ui->nvgw.font_icons);
        nvgFontSize(ui->vg, 20.0f);
        nvgFillColor(ui->vg, cst);
        nvgTextAlign(ui->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ui->vg, ir_cx, ir_cy, ICON_ADD, NULL);
      }
      drawn_image = 1;
    }

    // Fill path
    if (!drawn_image && fill.a > 0) {
      float poly_pts[512 * 2];
      int poly_count =
          node_get_path_polygon(node, poly_pts, 512, sx, sy, scale);
      if (poly_count > 2) {
        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, poly_pts[0], poly_pts[1]);
        for (int _pi = 1; _pi < poly_count; _pi++)
          nvgLineTo(ui->vg, poly_pts[_pi * 2], poly_pts[_pi * 2 + 1]);
        nvgClosePath(ui->vg);
        nvgFillColor(ui->vg, fill);
        nvgFill(ui->vg);
      }
    }

    // Stroke path
    for (int k = 0; k < node->path_point_count; k++) {
      int nxt = (k + 1) % node->path_point_count;
      NodePathPoint p1 = node->path_points[k], p2 = node->path_points[nxt];
      if (p1.type == 1) { // Cubic Bezier
        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, sx + p1.x * scale, sy + p1.y * scale);
        nvgBezierTo(ui->vg, sx + p1.cp2x * scale, sy + p1.cp2y * scale,
                    sx + p2.cp1x * scale, sy + p2.cp1y * scale,
                    sx + p2.x * scale, sy + p2.y * scale);
        nvgStrokeColor(ui->vg, stroke);
        nvgStrokeWidth(ui->vg, node->stroke_width * scale);
        nvgStroke(ui->vg);
      } else {
        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, sx + p1.x * scale, sy + p1.y * scale);
        nvgLineTo(ui->vg, sx + p2.x * scale, sy + p2.y * scale);
        nvgStrokeColor(ui->vg, stroke);
        nvgStrokeWidth(ui->vg, node->stroke_width * scale);
        nvgStroke(ui->vg);
      }
    }

    // Line Endings for Paths (only if 2 points)
    if (node->path_point_count == 2) {
      NodePathPoint p1 = node->path_points[0];
      NodePathPoint p2 = node->path_points[1];
      float start_x = sx + p1.x * scale;
      float start_y = sy + p1.y * scale;
      float end_x = sx + p2.x * scale;
      float end_y = sy + p2.y * scale;

      float dx = end_x - start_x;
      float dy = end_y - start_y;
      float angle = atan2f(dy, dx);
      float size = (node->stroke_width > 2.0f ? node->stroke_width : 2.0f) *
                   4.0f * scale;

      // Start Style
      if (node->line_start_style == 1) { // Circle
        nvgBeginPath(ui->vg);
        nvgCircle(ui->vg, start_x, start_y, size / 2.0f);
        nvgFillColor(ui->vg, stroke);
        nvgFill(ui->vg);
      } else if (node->line_start_style ==
                 2) { // Arrow (pointing away from line)
        float arrow_angle = angle + 3.14159f;
        float tip_x = start_x;
        float tip_y = start_y;

        float base_angle1 = arrow_angle + 0.5f;
        float base_angle2 = arrow_angle - 0.5f;

        float p1x = tip_x + size * cosf(base_angle1);
        float p1y = tip_y + size * sinf(base_angle1);
        float p2x = tip_x + size * cosf(base_angle2);
        float p2y = tip_y + size * sinf(base_angle2);

        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, tip_x, tip_y);
        nvgLineTo(ui->vg, p1x, p1y);
        nvgLineTo(ui->vg, p2x, p2y);
        nvgClosePath(ui->vg);
        nvgFillColor(ui->vg, stroke);
        nvgFill(ui->vg);
      }

      // End Style
      if (node->line_end_style == 1) { // Circle
        nvgBeginPath(ui->vg);
        nvgCircle(ui->vg, end_x, end_y, size / 2.0f);
        nvgFillColor(ui->vg, stroke);
        nvgFill(ui->vg);
      } else if (node->line_end_style == 2) { // Arrow
        float tip_x = end_x;
        float tip_y = end_y;

        float base_angle1 = angle + 3.14159f + 0.5f;
        float base_angle2 = angle + 3.14159f - 0.5f;

        float p1x = tip_x + size * cosf(base_angle1);
        float p1y = tip_y + size * sinf(base_angle1);
        float p2x = tip_x + size * cosf(base_angle2);
        float p2y = tip_y + size * sinf(base_angle2);

        nvgBeginPath(ui->vg);
        nvgMoveTo(ui->vg, tip_x, tip_y);
        nvgLineTo(ui->vg, p1x, p1y);
        nvgLineTo(ui->vg, p2x, p2y);
        nvgClosePath(ui->vg);
        nvgFillColor(ui->vg, stroke);
        nvgFill(ui->vg);
      }
    }
  } else if (node->type == NODE_TYPE_ELLIPSE) {
    if (node->is_container && !node->image_path[0]) {
      nvgBeginPath(ui->vg);
      nvgEllipse(ui->vg, nr.x + nr.w / 2, nr.y + nr.h / 2, nr.w / 2, nr.h / 2);
      nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 40));
      nvgFill(ui->vg);
      nvgStrokeColor(ui->vg, nvgRGBA(110, 110, 130, 255));
      nvgStrokeWidth(ui->vg, 1.0f);
      nvgStroke(ui->vg);

      if (ui->font_icons) {
        nvgFontSize(ui->vg, 20.0f);
        nvgFontFace(ui->vg, "Icons");
        nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 255));
        nvgTextAlign(ui->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        char icon_buf[16];
        snprintf(icon_buf, sizeof(icon_buf), "%s", ICON_ADD);
        nvgText(ui->vg, nr.x + nr.w / 2, nr.y + nr.h / 2, icon_buf, NULL);
      }
      drawn_image = 1;
    }
    if (!drawn_image && ip) {
      nvgBeginPath(ui->vg);
      nvgEllipse(ui->vg, nr.x + nr.w / 2, nr.y + nr.h / 2, nr.w / 2, nr.h / 2);
      nvgFillPaint(ui->vg, *ip);
      nvgFill(ui->vg);
      drawn_image = 1;
    }
    if (!drawn_image && has_fill) {
      nvgSave(ui->vg);
      nvgTranslate(ui->vg, nr.x + nr.w / 2, nr.y + nr.h / 2);
      if (node->rotation != 0.0f) {
        nvgRotate(ui->vg, node->rotation * NVG_PI / 180.0f);
      }
      nvgBeginPath(ui->vg);
      nvgEllipse(ui->vg, 0, 0, nr.w / 2, nr.h / 2);
      if (gp) { nvgFillPaint(ui->vg, *gp); } else { nvgFillColor(ui->vg, fill); }
      nvgFill(ui->vg);
      nvgRestore(ui->vg);
    }
  } else if (node->type == NODE_TYPE_TRIANGLE) {
    if (node->is_container && !node->image_path[0]) {
      nvgBeginPath(ui->vg);
      nvgMoveTo(ui->vg, sx + sw / 2, sy);
      nvgLineTo(ui->vg, sx + sw, sy + sh);
      nvgLineTo(ui->vg, sx, sy + sh);
      nvgClosePath(ui->vg);

      nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 40));
      nvgFill(ui->vg);
      nvgStrokeColor(ui->vg, nvgRGBA(110, 110, 130, 255));
      nvgStrokeWidth(ui->vg, 1.0f);
      nvgStroke(ui->vg);

      if (ui->font_icons) {
        nvgFontSize(ui->vg, 20.0f);
        nvgFontFace(ui->vg, "Icons");
        nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 255));
        nvgTextAlign(ui->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        char icon_buf[16];
        snprintf(icon_buf, sizeof(icon_buf), "%s", ICON_ADD);
        nvgText(ui->vg, nr.x + nr.w / 2, nr.y + nr.h / 2 + 5, icon_buf, NULL);
      }
      drawn_image = 1;
    }
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c = cosf(rad), s = sinf(rad);
    float p0x = 0, p0y = -sh / 2.0f;
    float p1x = -sw / 2.0f, p1y = sh / 2.0f;
    float p2x = sw / 2.0f, p2y = sh / 2.0f;
    AslanVec2 pts[3] = {{cx + p0x * c - p0y * s, cy + p0x * s + p0y * c},
                             {cx + p1x * c - p1y * s, cy + p1x * s + p1y * c},
                             {cx + p2x * c - p2y * s, cy + p2x * s + p2y * c}};
    if (ip && !drawn_image)
      fill_rounded_polygon(ui, pts, 3, node->corner_radius * scale, fill, 0, 0, NULL, ip);
    else if (!drawn_image && has_fill)
      fill_rounded_polygon(ui, pts, 3, node->corner_radius * scale, fill, 0, 0, gp, NULL);
  } else if (node->type == NODE_TYPE_PENTAGON) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    AslanVec2 pts[5];
    for (int i = 0; i < 5; i++) {
      float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
      float px = cosf(angle) * sw / 2.0f;
      float py = sinf(angle) * sh / 2.0f;
      pts[i].x = cx + px * c_rot - py * s_rot;
      pts[i].y = cy + px * s_rot + py * c_rot;
    }
    if (ip && !drawn_image)
      fill_rounded_polygon(ui, pts, 5, node->corner_radius * scale, fill, 0, 0, NULL, ip);
    else if (!drawn_image && has_fill)
      fill_rounded_polygon(ui, pts, 5, node->corner_radius * scale, fill, 0, 0, gp, NULL);
  } else if (node->type == NODE_TYPE_HEXAGON) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    AslanVec2 pts[6];
    for (int i = 0; i < 6; i++) {
      float angle = (float)i * 2.0f * 3.14159f / 6.0f;
      float px = cosf(angle) * sw / 2.0f;
      float py = sinf(angle) * sh / 2.0f;
      pts[i].x = cx + px * c_rot - py * s_rot;
      pts[i].y = cy + px * s_rot + py * c_rot;
    }
    if (ip && !drawn_image)
      fill_rounded_polygon(ui, pts, 6, node->corner_radius * scale, fill, 0, 0, NULL, ip);
    else if (!drawn_image && has_fill)
      fill_rounded_polygon(ui, pts, 6, node->corner_radius * scale, fill, 0, 0, gp, NULL);
  } else if (node->type == NODE_TYPE_OCTAGON) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    AslanVec2 pts[8];
    for (int i = 0; i < 8; i++) {
      float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
      float px = cosf(angle) * sw / 2.0f;
      float py = sinf(angle) * sh / 2.0f;
      pts[i].x = cx + px * c_rot - py * s_rot;
      pts[i].y = cy + px * s_rot + py * c_rot;
    }
    if (ip && !drawn_image)
      fill_rounded_polygon(ui, pts, 8, node->corner_radius * scale, fill, 0, 0, NULL, ip);
    else if (!drawn_image && has_fill)
      fill_rounded_polygon(ui, pts, 8, node->corner_radius * scale, fill, 0, 0, gp, NULL);
  } else if (node->type == NODE_TYPE_STAR) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    AslanVec2 pts[10];
    for (int i = 0; i < 10; i++) {
      float angle = (float)i * 2.0f * 3.14159f / 10.0f - 3.14159f / 2.0f;
      float r = (i % 2 == 0) ? (sw / 2.0f) : (sw / 4.0f);
      float px = cosf(angle) * r;
      float py = sinf(angle) * r;
      pts[i].x = cx + px * c_rot - py * s_rot;
      pts[i].y = cy + px * s_rot + py * c_rot;
    }
    if (ip && !drawn_image)
      fill_rounded_polygon(ui, pts, 10, node->corner_radius * scale, fill, 0, 0, NULL, ip);
    else if (!drawn_image && has_fill)
      fill_rounded_polygon(ui, pts, 10, node->corner_radius * scale, fill, 0,
                           0, gp, NULL);
  } else if (node->type == NODE_TYPE_HEART) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    const int hcount = 32;
    AslanVec2 hpts[32];
    for (int i = 0; i < hcount; i++) {
      float t = (float)i * 2.0f * 3.14159f / (float)hcount;
      float hx = 16.0f * powf(sinf(t), 3.0f);
      float hy = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                   2.0f * cosf(3.0f * t) - cosf(4.0f * t));
      float lx = (hx / 32.0f) * sw;
      float ly = (hy / 32.0f) * sh + sh * 0.05f;
      hpts[i].x = cx + lx * c_rot - ly * s_rot;
      hpts[i].y = cy + lx * s_rot + ly * c_rot;
    }
    if (!drawn_image && has_fill) {
      nvgBeginPath(ui->vg);
      nvgMoveTo(ui->vg, hpts[0].x, hpts[0].y);
      for (int i = 1; i < hcount; i++) {
        nvgLineTo(ui->vg, hpts[i].x, hpts[i].y);
      }
      nvgClosePath(ui->vg);
      nvgFillColor(ui->vg, fill);
      nvgFill(ui->vg);
    }
  } else if (node->type == NODE_TYPE_TEXT) {
    // Text node

    // Render background for sticky notes / text with bg
    if (fill.a > 0) {
      if (node->fill_type == 1) { // Gradient
        NVGcolor fill2 = nvgRGBA(
            (node->fill_color_2 >> 24) & 0xFF,
            (node->fill_color_2 >> 16) & 0xFF, (node->fill_color_2 >> 8) & 0xFF,
            (int)((node->fill_color_2 & 0xFF) * opacity));
        fill_gradient_rounded_rect(ui, nr.x, nr.y, nr.w, nr.h, 0, fill, fill2,
                                   node->gradient_angle);
      } else {
        fill_smooth_rounded_rect(ui, nr.x, nr.y, nr.w, nr.h,
                                 node->corner_radius * scale, fill);
      }
    }

    const char *display_text = node->text;
    static char linked_val_buf[256];
    int is_editing = (ui->editing_node == node);
    // Substitute linked column value
    if (!is_editing && node->linked_col_name[0]) {
      if (dd->linked_table_id && engine && engine->doc) {
        for (int i = 0; i < 32; i++) {
          Page *p = &engine->doc->pages[i];
          if (!p->in_use)
            continue;
          Block *tb = page_find_block_by_id(p, dd->linked_table_id);
          if (tb && tb->type == BLOCK_TABLE && tb->data.table.columnar) {
            AslanTable *tbl = tb->data.table.columnar;
            for (int lk = 0; lk < (int)tbl->column_count; lk++) {
              if (strcmp(tbl->columns[lk].name, node->linked_col_name) == 0) {
                int ridx = ui->design_linked_row_idx;
                if (ridx < (int)tbl->active_count) {
                  uint64_t rid = aslan_table_view_row_id(tbl, ridx);
                  const char *val = aslan_table_get_cell_display(
                      tbl, lk, rid, linked_val_buf, sizeof(linked_val_buf));
                  if (val && val[0]) {
                    display_text = val;
                  }
                }
                break;
              }
            }
            break;
          }
        }
      }
    }
    if (is_editing) {
      display_text = ui->edit_buffer;
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2, 2.0f);
      nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 20));
      nvgFill(ui->vg);
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2, 2.0f);
      nvgStrokeColor(ui->vg, nvgRGBA(COLOR_ACCENT.r, COLOR_ACCENT.g,
                                     COLOR_ACCENT.b, COLOR_ACCENT.a));
      nvgStrokeWidth(ui->vg, 1.0f);
      nvgStroke(ui->vg);
    }

    // Auto-resize text node to fit content
    if (display_text && strlen(display_text) > 0) {
      // Measure with the same font size used for rendering so that
      // node->width/height match the actual drawn glyphs.
      float measure_font_size = node->font_size * scale;
      if (measure_font_size < 1.0f) measure_font_size = 1.0f;
      nvgFontSize(ui->vg, measure_font_size);
      const char *_face = (node->font_face[0]) ? node->font_face : "Inter";
      if (node->font_type == 4) _face = "Icons";
      nvgFontFace(ui->vg, _face);
      // NanoVG multi-line measurement and Auto-resize
      char *text_copy = strdup(display_text);
      float max_w = 0;
      int lines = 0;
      if (text_copy) {
        char *p = text_copy;
        char *start = p;
        while (*p) {
          if (*p == '\n') {
            *p = '\0';
            float bounds[4];
            nvgTextBounds(ui->vg, 0, 0, start, NULL, bounds);
            float lw = bounds[2] - bounds[0] +
                       (strlen(start) * node->letter_spacing * scale);
            if (lw > max_w)
              max_w = lw;
            lines++;
            start = p + 1;
          }
          p++;
        }
        if (start < p) {
          float bounds[4];
          nvgTextBounds(ui->vg, 0, 0, start, NULL, bounds);
          float lw = bounds[2] - bounds[0] +
                     (strlen(start) * node->letter_spacing * scale);
          if (lw > max_w)
            max_w = lw;
          lines++;
        }
        free(text_copy);
      }
      // Convert screen-pixel measurement back to design units
      float text_w = (scale > 0.001f) ? max_w / scale : max_w;
      float text_h = node->font_size * lines * node->line_height;
      if (lines == 0) {
        text_w = 20.0f;
        text_h = node->font_size > 0 ? node->font_size * 1.4f : 24.0f;
      }
      // Minimum grab area: 2× font width and 1.4× font height
      float min_w = node->font_size * 2.0f;
      float min_h = node->font_size * 1.4f;
      if (min_w < 40.0f) min_w = 40.0f;
      if (min_h < 24.0f) min_h = 24.0f;
      if (text_w < min_w) text_w = min_w;
      if (text_h < min_h) text_h = min_h;

      // Auto-resize
      if (fabsf(node->width - (text_w + 10.0f)) > 1.0f ||
          fabsf(node->height - (text_h + 10.0f)) > 1.0f) {
        node->width = text_w + 10.0f; // Add padding
        node->height = text_h + 10.0f;
      }
    }

    NVGcolor txt_color = nvgRGBA(
        (node->stroke_color >> 24) & 0xFF, (node->stroke_color >> 16) & 0xFF,
        (node->stroke_color >> 8) & 0xFF, node->stroke_color & 0xFF);

    // --- Advanced Text Rendering with NanoVG ---
    char *text_copy = strdup(display_text);
    if (text_copy) {
      char *lines_arr[32];
      int line_count = 0;
      char *p = text_copy;
      lines_arr[line_count++] = p;
      while (*p && line_count < 32) {
        if (*p == '\n') {
          *p = '\0';
          lines_arr[line_count++] = p + 1;
        }
        p++;
      }

      float cursor_y = nr.y + node->font_size * scale; // baseline estimate
      float actual_line_height = node->font_size * scale * node->line_height;

      nvgFillColor(ui->vg, txt_color);
      nvgFontSize(ui->vg, node->font_size * scale);
      const char *_rface = (node->font_face[0]) ? node->font_face : "Inter";
      if (node->font_type == 4) _rface = "Icons";
      nvgFontFace(ui->vg, _rface);

      for (int i = 0; i < line_count; i++) {
        char *line = lines_arr[i];
        int len = strlen(line);
        if (len == 0) {
          cursor_y += actual_line_height;
          continue;
        }

        float bounds[4];
        nvgTextBounds(ui->vg, 0, 0, line, NULL, bounds);
        float line_w =
            bounds[2] - bounds[0] + (len * node->letter_spacing * scale);

        // Alignment
        float cursor_x = nr.x;
        if (node->text_align == 1) { // Center
          cursor_x = nr.x + (nr.w - line_w) / 2.0f;
        } else if (node->text_align == 2) { // Right
          cursor_x = nr.x + nr.w - line_w;
        }

        nvgTextLetterSpacing(ui->vg, node->letter_spacing * scale);
        nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        nvgText(ui->vg, cursor_x, cursor_y, line, NULL);

        cursor_y += actual_line_height;
      }

      free(text_copy);
    }

    if (is_editing) {
      // Text input overlay is rendered after the group traverse.
    }
  }

  // Container indicator (Add icon in the center if empty container)
  if (node->is_container && !node->image_path[0] &&
      node->type != NODE_TYPE_TEXT && node->type != NODE_TYPE_ROOT) {
    if (ui->font_icons) {
      nvgFontSize(ui->vg, 20.0f);
      nvgFontFace(ui->vg, "Icons");
      nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 255));
      nvgTextAlign(ui->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      char icon_buf[16];
      snprintf(icon_buf, sizeof(icon_buf), "%s", ICON_ADD);
      nvgText(ui->vg, nr.x + nr.w / 2, nr.y + nr.h / 2, icon_buf, NULL);
    }
  }

  // Render children
  for (Node *child = node->first_child; child; child = child->next_sibling) {
    ui_render_node_tree(ui, engine, block, dd, child, bounds, scale, sx, sy);
  }
  if (node->is_container && node->clip_content) {
    nvgScissor(ui->vg, ui->artboard_clip.x, ui->artboard_clip.y,
               ui->artboard_clip.w, ui->artboard_clip.h);
  }
}

// =============================================================================
// GRID, GUIDES & RULERS RENDERING
// =============================================================================

static void design_draw_grid(UIHost *ui, AslanRect art, float scale,
                             const DesignPage *dp, float pan_x, float pan_y,
                             int no_limits) {
  if (!dp)
    return;

  NVGcolor grid_col = nvgRGBA(120, 120, 120, 50);
  NVGcolor dot_col = nvgRGBA(80, 82, 86, 255);
  float spacing =
      (dp->grid_type == 5 && dp->grid_spacing > 1) ? dp->grid_spacing : 40.0f;
  float step = spacing * scale;

  int max_cols = (int)(art.w / step) + 1;
  int max_rows = (int)(art.h / step) + 1;
  if (max_cols > 80)
    max_cols = 80;
  if (max_rows > 80)
    max_rows = 80;

  if (step < 8.0f)
    return; // Too dense — skip to avoid vertex overflow

  if (no_limits) {
    // Dotted grid for infinite canvas (always on in infinite mode)
    float ox = fmodf(-pan_x * scale, step);
    float oy = fmodf(-pan_y * scale, step);
    if (ox < 0)
      ox += step;
    if (oy < 0)
      oy += step;

    for (float y = art.y + oy; y < art.y + art.h; y += step) {
      for (float x = art.x + ox; x < art.x + art.w; x += step) {
        // Draw slightly larger/clearer dots
        nvgBeginPath(ui->vg);
        nvgCircle(ui->vg, x, y, 1.0f);
        nvgFillColor(ui->vg, dot_col);
        nvgFill(ui->vg);
      }
    }
    return;
  }

  if (dp->grid_type == 0)
    return;

  nvgBeginPath(ui->vg);
  switch (dp->grid_type) {
  case 1: { // Dots — cap at 50x50 = 2500 circles
    int dc = max_cols > 50 ? 50 : max_cols;
    int dr = max_rows > 50 ? 50 : max_rows;
    for (int iy = 0; iy < dr; iy++) {
      float sy = art.y + iy * step;
      for (int ix = 0; ix < dc; ix++) {
        float sx = art.x + ix * step;
        nvgCircle(ui->vg, sx, sy, 1.0f);
      }
    }
    nvgFillColor(ui->vg, dot_col);
    nvgFill(ui->vg);
    return;
    break;
  }
  case 2: { // Lines
    for (int ix = 0; ix < max_cols; ix++) {
      float sx = art.x + ix * step;
      nvgMoveTo(ui->vg, sx, art.y);
      nvgLineTo(ui->vg, sx, art.y + art.h);
    }
    for (int iy = 0; iy < max_rows; iy++) {
      float sy = art.y + iy * step;
      nvgMoveTo(ui->vg, art.x, sy);
      nvgLineTo(ui->vg, art.x + art.w, sy);
    }
    break;
  }
  case 3: { // 12 Columns
    float col_w = art.w / 12.0f;
    NVGcolor col_fill = nvgRGBA(100, 140, 220, 20);
    NVGcolor col_stroke = nvgRGBA(100, 140, 220, 40);
    for (int i = 0; i < 12; i++) {
      float cx = art.x + i * col_w;
      nvgBeginPath(ui->vg);
      nvgRect(ui->vg, cx, art.y, col_w, art.h);
      nvgFillColor(ui->vg, col_fill);
      nvgFill(ui->vg);
      nvgStrokeColor(ui->vg, col_stroke);
      nvgStrokeWidth(ui->vg, 0.5f);
      nvgStroke(ui->vg);
    }
    return; // Early return to bypass unified stroke logic
  }
  case 4: { // Isometric — cap total lines
    int iso_max = max_cols + max_rows;
    if (iso_max > 100)
      iso_max = 100;
    for (int i = 0; i < iso_max; i++) {
      float sx = art.x + i * step;
      nvgMoveTo(ui->vg, sx, art.y);
      nvgLineTo(ui->vg, sx - art.h, art.y + art.h);

      nvgMoveTo(ui->vg, sx - art.w, art.y);
      nvgLineTo(ui->vg, sx, art.y + art.h);
    }
    break;
  }
  case 5: { // Custom — same as Lines
    for (int ix = 0; ix < max_cols; ix++) {
      float sx = art.x + ix * step;
      nvgMoveTo(ui->vg, sx, art.y);
      nvgLineTo(ui->vg, sx, art.y + art.h);
    }
    for (int iy = 0; iy < max_rows; iy++) {
      float sy = art.y + iy * step;
      nvgMoveTo(ui->vg, art.x, sy);
      nvgLineTo(ui->vg, art.x + art.w, sy);
    }
    break;
  }
  default:
    break;
  }

  // Unified stroke for grid lines
  nvgStrokeColor(ui->vg, grid_col);
  nvgStrokeWidth(ui->vg, 0.5f);
  nvgStroke(ui->vg);
}

static void design_draw_guides(UIHost *ui, AslanRect art, float scale,
                               const DesignPage *dp) {
  if (!dp)
    return;
  NVGcolor guide_col = nvgRGBA(110, 110, 130, 180);

  // Horizontal guides
  for (int i = 0; i < dp->h_guide_count; i++) {
    float sy = art.y + dp->h_guides[i] * scale;
    if (sy >= art.y && sy <= art.y + art.h) {
      nvgBeginPath(ui->vg);
      nvgMoveTo(ui->vg, art.x, sy);
      nvgLineTo(ui->vg, art.x + art.w, sy);
      nvgStrokeColor(ui->vg, guide_col);
      nvgStrokeWidth(ui->vg, 1.0f);
      nvgStroke(ui->vg);

      // Label
      char lbl[16];
      snprintf(lbl, sizeof(lbl), "%.0f", dp->h_guides[i]);
      if (ui->font_inter) {
        nvgFontSize(ui->vg, ui->font_size_px);
        nvgFontFace(ui->vg, "Inter");
        float bounds[4];
        nvgTextBounds(ui->vg, 0, 0, lbl, NULL, bounds);
        float tw = bounds[2] - bounds[0];

        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, art.x + 2, sy - 12, tw + 4, 12);
        nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 200));
        nvgFill(ui->vg);

        nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
        nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(ui->vg, art.x + 4, sy - 12 + 1, lbl, NULL);
      }
    }
  }

  // Vertical guides
  for (int i = 0; i < dp->v_guide_count; i++) {
    float sx = art.x + dp->v_guides[i] * scale;
    if (sx >= art.x && sx <= art.x + art.w) {
      nvgBeginPath(ui->vg);
      nvgMoveTo(ui->vg, sx, art.y);
      nvgLineTo(ui->vg, sx, art.y + art.h);
      nvgStrokeColor(ui->vg, guide_col);
      nvgStrokeWidth(ui->vg, 1.0f);
      nvgStroke(ui->vg);

      // Label
      char lbl[16];
      snprintf(lbl, sizeof(lbl), "%.0f", dp->v_guides[i]);
      if (ui->font_inter) {
        nvgFontSize(ui->vg, ui->font_size_px);
        nvgFontFace(ui->vg, "Inter");
        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, sx + 2, art.y + 2, 30, 12);
        nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 200));
        nvgFill(ui->vg);

        nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
        nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(ui->vg, sx + 4, art.y + 2 + 1, lbl, NULL);
      }
    }
  }
}

static void design_draw_rulers(UIHost *ui, AslanRect art, float scale,
                               const DesignPage *dp, int page_w, int page_h) {
  if (!dp || !dp->show_rulers)
    return;

  const float ruler_w = 20.0f;
  NVGcolor ruler_bg = nvgRGBA(50, 50, 55, 220);
  NVGcolor tick_col = nvgRGBA(180, 180, 180, 255);
  NVGcolor text_col = nvgRGBA(200, 200, 200, 255);

  nvgBeginPath(ui->vg);
  // Top ruler
  nvgRect(ui->vg, art.x, art.y - ruler_w, art.w, ruler_w);
  // Left ruler
  nvgRect(ui->vg, art.x - ruler_w, art.y, ruler_w, art.h);
  // Corner square
  nvgRect(ui->vg, art.x - ruler_w, art.y - ruler_w, ruler_w, ruler_w);
  nvgFillColor(ui->vg, ruler_bg);
  nvgFill(ui->vg);

  // Top ruler ticks
  nvgBeginPath(ui->vg);
  for (int px = 0; px <= page_w; px += 10) {
    float sx = art.x + (float)px * scale;
    if (sx < art.x || sx > art.x + art.w)
      continue;
    int is_major = (px % 100 == 0);
    float tick_h = is_major ? ruler_w * 0.6f : ruler_w * 0.3f;
    nvgMoveTo(ui->vg, sx, art.y - tick_h);
    nvgLineTo(ui->vg, sx, art.y);
  }
  // Left ruler ticks
  for (int py = 0; py <= page_h; py += 10) {
    float sy = art.y + (float)py * scale;
    if (sy < art.y || sy > art.y + art.h)
      continue;
    int is_major = (py % 100 == 0);
    float tick_w = is_major ? ruler_w * 0.6f : ruler_w * 0.3f;
    nvgMoveTo(ui->vg, art.x - tick_w, sy);
    nvgLineTo(ui->vg, art.x, sy);
  }
  nvgStrokeColor(ui->vg, tick_col);
  nvgStrokeWidth(ui->vg, 0.5f);
  nvgStroke(ui->vg);

  if (ui->font_inter) {
    nvgFontSize(ui->vg, ui->font_size_px);
    nvgFontFace(ui->vg, "Inter");
    nvgFillColor(ui->vg, text_col);
    nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    for (int px = 0; px <= page_w; px += 10) {
      float sx = art.x + (float)px * scale;
      if (sx < art.x || sx > art.x + art.w)
        continue;
      if (px % 100 == 0) {
        char lbl[8];
        snprintf(lbl, sizeof(lbl), "%d", px);
        nvgText(ui->vg, sx + 2, art.y - ruler_w + 1, lbl, NULL);
      }
    }

    for (int py = 0; py <= page_h; py += 10) {
      float sy = art.y + (float)py * scale;
      if (sy < art.y || sy > art.y + art.h)
        continue;
      if (py % 100 == 0) {
        char lbl[8];
        snprintf(lbl, sizeof(lbl), "%d", py);
        nvgText(ui->vg, art.x - ruler_w + 2, sy + 1, lbl, NULL);
      }
    }
  }
}

void ui_render_block_design(UIHost *ui, Engine *engine, Block *block,
                            float width, float height) {

  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;

  // Initialize pages array if needed (for backwards compatibility)
  if (!dd->pages || dd->page_count == 0) {
    dd->page_capacity = 4;
    dd->pages = (DesignPage *)calloc(dd->page_capacity, sizeof(DesignPage));
    dd->page_count = 1;
    dd->current_page = 0;
    dd->pages[0].root = (Node *)calloc(1, sizeof(Node));
    if (dd->pages[0].root) {
      dd->pages[0].root->type = NODE_TYPE_ROOT;
      dd->pages[0].root->visible = 1;
    }
    dd->pages[0].bg_color = dd->bg_color;
    dd->root = dd->pages[0].root;
  }

  // Ensure background is not transparent (0) - default to White
  if (dd->bg_color == 0)
    dd->bg_color = 0xFFFFFFFF;
  if (dd->pages[dd->current_page].bg_color == 0)
    dd->pages[dd->current_page].bg_color = 0xFFFFFFFF;

  // Ensure root points to current page
  if (dd->current_page >= 0 && dd->current_page < dd->page_count) {
    dd->root = dd->pages[dd->current_page].root;
  }

  int is_fullscreen = (ui->expanded_block_id == block->id);

  // Initialize zoom for no_limits mode
  if (dd->no_limits && dd->zoom < 0.01f)
    dd->zoom = 1.0f;

  // --- Layout height calculation ---
  float toolbar_h = 52.0f;
  // float sidebar_w = 260.0f;

  float row_h = height;
                      // sidebar visibility
  ASLAN_DEBUG("Design block layout row_h = %f", row_h);

  float display_h = row_h - toolbar_h;
  if (display_h < 100.0f)
    display_h = 100.0f;

  float sidebar_w = 260.0f;
  float ws_width = width - sidebar_w;
  if (ws_width < 100.0f) ws_width = width; // fail-safe if panel too narrow

  if (1) {
    AslanRect ws_bounds = {0, 0, ws_width, row_h};
    DesignInteraction *ds = &ui->design_state;
    float mx = ui->mouse_x - ui->abs_offset_x;
    float my = ui->mouse_y - ui->abs_offset_y;

    // Pre-calculate toolbar flags for interaction logic (8 buttons in
    // design)
    float toolbar_w = 8 * 36.0f + 7 * 8.0f + 24.0f;
    float toolbar_x = ws_bounds.x + (ws_bounds.w - toolbar_w) / 2.0f;
    float toolbar_y = ws_bounds.y + ws_bounds.h - 52.0f - 20.0f; // toolbar_h=52

    int in_toolbar =
        (mx >= toolbar_x && mx < toolbar_x + toolbar_w &&
         my >= toolbar_y && my < toolbar_y + 52.0f);
    int in_forms_menu = 0;

    // Early check for active state
    int is_active_design = (ds->active_design_id == block->id);

    float scale = 1.0f;
    AslanRect art_rect = {0, 0, 0, 0}; // populated in fixed-artboard branch

    if (dd->no_limits) {
      // --- Infinite Canvas Mode ---
      float zoom = dd->zoom > 0.01f ? dd->zoom : 1.0f;
      float cam_x = dd->view_x, cam_y = dd->view_y;

      // Sync design_state camera
      if (ds->active_design_id != block->id) {
        ds->cam_x = cam_x;
        ds->cam_y = cam_y;
        ds->zoom = zoom;
      } else {
        cam_x = ds->cam_x;
        cam_y = ds->cam_y;
        zoom = ds->zoom > 0.01f ? ds->zoom : 1.0f;
      }
      scale = zoom;

      // Use screen-space bounds for background filling
      AslanRect sc_bounds = {ws_bounds.x, ws_bounds.y, ws_bounds.w, ws_bounds.h};

      // Background: support solid, linear, radial
      NVGcolor c1 =
          nvgRGBA((dd->pages[dd->current_page].bg_color >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color & 0xFF);
      NVGcolor c2 =
          nvgRGBA((dd->pages[dd->current_page].bg_color_2 >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color_2 >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color_2 >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color_2 & 0xFF);

      if (dd->pages[dd->current_page].bg_type == 0) { // Solid
        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, sc_bounds.x, sc_bounds.y, sc_bounds.w, sc_bounds.h);
        nvgFillColor(ui->vg, c1);
        nvgFill(ui->vg);
      } else if (dd->pages[dd->current_page].bg_type == 1) { // Linear (V)
        NVGpaint paint = nvgLinearGradient(
            ui->vg, sc_bounds.x, sc_bounds.y, sc_bounds.x,
            sc_bounds.y + sc_bounds.h, c1, c2);
        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, sc_bounds.x, sc_bounds.y, sc_bounds.w, sc_bounds.h);
        nvgFillPaint(ui->vg, paint);
        nvgFill(ui->vg);
      } else { // Radial (Approximation)
        NVGpaint paint = nvgRadialGradient(
            ui->vg, sc_bounds.x + sc_bounds.w / 2,
            sc_bounds.y + sc_bounds.h / 2, 0, sc_bounds.w / 2, c2, c1);
        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, sc_bounds.x, sc_bounds.y, sc_bounds.w, sc_bounds.h);
        nvgFillPaint(ui->vg, paint);
        nvgFill(ui->vg);
      }
      DesignPage *cur_dp = &dd->pages[dd->current_page];
      design_draw_grid(ui, sc_bounds, zoom, cur_dp, cam_x, cam_y, 1);
      design_draw_guides(ui, sc_bounds, zoom, cur_dp);
      design_draw_rulers(ui, sc_bounds, zoom, cur_dp, dd->width, dd->height);
      nvgScissor(ui->vg, sc_bounds.x, sc_bounds.y, sc_bounds.w, sc_bounds.h);

    } else {
      // --- Fixed Artboard Mode ---
      nvgBeginPath(ui->vg);
      nvgRect(ui->vg, ws_bounds.x, ws_bounds.y, ws_bounds.w, ws_bounds.h);
      nvgFillColor(ui->vg, nvgRGBA(241, 241, 239, 255));
      nvgFill(ui->vg);

      float margin = 20.0f;
      float avail_w = ws_bounds.w - margin * 2;
      float avail_h = display_h - margin * 2;

      float scale_w = avail_w / (float)dd->width;
      float scale_h = avail_h / (float)dd->height;
      scale = (scale_w < scale_h) ? scale_w : scale_h;
      if (scale > 1.0f)
        scale = 1.0f;

      float art_w = (float)dd->width * scale;
      float art_h = (float)dd->height * scale;

      float sx = ws_bounds.x + (ws_bounds.w - art_w) / 2.0f;
      float sy = ws_bounds.y + (display_h - art_h) / 2.0f;

      art_rect = (AslanRect){sx, sy, art_w, art_h};
      NVGcolor c1 =
          nvgRGBA((dd->pages[dd->current_page].bg_color >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color & 0xFF);
      NVGcolor c2 =
          nvgRGBA((dd->pages[dd->current_page].bg_color_2 >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color_2 >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color_2 >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color_2 & 0xFF);

      if (dd->pages[dd->current_page].bg_type == 0) {
        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, art_rect.x, art_rect.y, art_rect.w, art_rect.h);
        nvgFillColor(ui->vg, c1);
        nvgFill(ui->vg);
      } else if (dd->pages[dd->current_page].bg_type == 1) {
        NVGpaint paint = nvgLinearGradient(
            ui->vg, art_rect.x, art_rect.y, art_rect.x, art_rect.y + art_rect.h,
            c1, c2);
        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, art_rect.x, art_rect.y, art_rect.w, art_rect.h);
        nvgFillPaint(ui->vg, paint);
        nvgFill(ui->vg);
      } else {
        NVGpaint paint = nvgRadialGradient(
            ui->vg, art_rect.x + art_rect.w / 2, art_rect.y + art_rect.h / 2, 0,
            art_rect.w / 2, c2, c1);
        nvgBeginPath(ui->vg);
        nvgRect(ui->vg, art_rect.x, art_rect.y, art_rect.w, art_rect.h);
        nvgFillPaint(ui->vg, paint);
        nvgFill(ui->vg);
      }
      // Store art_rect so border + scissor are identical after the group ends
      ui->artboard_clip = art_rect;
      ui->artboard_rect = art_rect;

      {
        DesignPage *cur_dp = &dd->pages[dd->current_page];
        design_draw_grid(ui, art_rect, scale, cur_dp, 0, 0, 0);
        design_draw_guides(ui, art_rect, scale, cur_dp);
        design_draw_rulers(ui, art_rect, scale, cur_dp, dd->width, dd->height);
      }

    }

    // --- Shared content rendering (both modes) ---
    ASLAN_DEBUG("BEGIN design_content");
    int group_res = 1;
    if (group_res) {
      AslanRect bounds;

      if (dd->no_limits) {
        // Offset bounds so existing code (bounds.x + node.x * scale) works
        float cam_x = ds->cam_x, cam_y = ds->cam_y;
        float zoom = ds->zoom > 0.01f ? ds->zoom : 1.0f;
        bounds = (AslanRect){ws_bounds.x - cam_x * zoom, ws_bounds.y - cam_y * zoom,
                    ws_bounds.w, ws_bounds.h};
      } else {
        // Use art_rect as the rendering origin so nodes are positioned
        // relative to the (centered) artboard, not the panel top-left.
        // This prevents nodes from drifting when the split-view divider
        // is moved and the panel width/scale changes.
        bounds = art_rect;
      }

      // Expose bounds in screen-space for hit-tests in ui_host.c.
      // In fixed artboard mode the interactive area is the artboard itself.
      ds->bounds_x = dd->no_limits ? ws_bounds.x : (ui->abs_offset_x + art_rect.x);
      ds->bounds_y = dd->no_limits ? ws_bounds.y : (ui->abs_offset_y + art_rect.y);
      ds->bounds_w = dd->no_limits ? ws_bounds.w : art_rect.w;
      ds->bounds_h = dd->no_limits ? ws_bounds.h : art_rect.h;
      ds->scale = scale;

      // Track if this design is active
      // Track if this design is active
      is_active_design = (ds->active_design_id == block->id);

      // Auto-activate if clicked inside WORKSPACE (including toolbar)
      if (!is_active_design && !ui->block_drag.is_active &&
          (ui->mouse_buttons & 1) &&
          mx >= ws_bounds.x &&
          mx < ws_bounds.x + ws_bounds.w &&
          my >= ws_bounds.y &&
          my < ws_bounds.y + ws_bounds.h) {
        ds->active_design_id = block->id;
        ds->active_block = block;
        ui->editing_block_id = block->id;
        is_active_design = 1;
      }

      // --- Handle pending element creation ---
      if (is_active_design && ds->pending_create > 0) {
        float nx, ny;
        if (dd->no_limits) {
          // Center in current infinite viewport (scale = zoom here)
          nx = ds->cam_x + (ws_bounds.w / scale) / 2.0f - 50.0f;
          ny = ds->cam_y + (ws_bounds.h / scale) / 2.0f - 40.0f;
        } else {
          // Center in fixed artboard
          nx = (float)dd->width / 2.0f - 50.0f;
          ny = (float)dd->height / 2.0f - 40.0f;
        }
        Node *new_node = NULL;
        float def_w = 100.0f, def_h = 80.0f;

        if (ds->pending_create == 1) { // Body
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 200.0f, 40.0f, ds->selected_color);
          if (new_node)
            strncpy(new_node->text, "Body text", sizeof(new_node->text) - 1);
        } else if (ds->pending_create == 6) { // H1
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 300.0f, 60.0f, ds->selected_color);
          if (new_node)
            strncpy(new_node->text, "Heading", sizeof(new_node->text) - 1);
        } else if (ds->pending_create == 2) { // Sticky
          new_node = design_create_styled_node(
              dd, NODE_STYLE_RECT, nx, ny, 120.0f, 120.0f, ds->selected_color);
          if (new_node) {
            new_node->fill_color = 0xFFF9C4FF;
            new_node->stroke_color = 0xFBC02DFF;
            new_node->type = NODE_TYPE_TEXT;
            strncpy(new_node->text, "Note...", sizeof(new_node->text) - 1);
          }
        } else if (ds->pending_create == 3) { // Image
          char path[512] = {0};
          if (open_image_file_dialog(path, sizeof(path))) {
            char rel[256] = {0};
            if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
              new_node = node_create(dd->next_node_id++, NODE_TYPE_IMAGE, NULL);
              if (new_node && dd->root) {
                new_node->x = nx;
                new_node->y = ny;

                // Maintain aspect ratio
                int img_w = 0, img_h = 0;
                int size_ok = mac_get_image_size(path, &img_w, &img_h);
                ASLAN_DEBUG("design image size for '%s': %d x %d (ok=%d)", path,
                            img_w, img_h, size_ok);

                if (size_ok && img_w > 0 && img_h > 0) {
                  float aspect = (float)img_w / (float)img_h;
                  if (aspect > 1.0f) {
                    new_node->width = 300.0f;
                    new_node->height = 300.0f / aspect;
                  } else {
                    new_node->height = 300.0f;
                    new_node->width = 300.0f * aspect;
                  }
                } else {
                  new_node->width = 200.0f;
                  new_node->height = 150.0f;
                }

                new_node->visible = 1;
                new_node->lock_ratio = 1;
                new_node->parent = dd->root;
                strncpy(new_node->image_path, rel,
                        sizeof(new_node->image_path) - 1);

                // Add to tree
                if (!dd->root->first_child)
                  dd->root->first_child = new_node;
                else {
                  Node *l = dd->root->first_child;
                  while (l->next_sibling)
                    l = l->next_sibling;
                  l->next_sibling = new_node;
                }
              }
            }
          }
        } else if (ds->pending_create == 11) {
          // Chart
          new_node = node_create(dd->next_node_id++, NODE_TYPE_CHART, "Chart");
          if (new_node) {
            new_node->x = nx;
            new_node->y = ny;
            new_node->width = 400;
            new_node->height = 300;
            new_node->parent = dd->root;
            node_add_child(dd->root, new_node);

            // Auto-detect source table
            Page *page = doc_page_get_active(engine->doc);
            if (page && new_node->chart_data) {
              for (Block *b = page->first_block; b; b = b->next) {
                if (b->type == BLOCK_TABLE && b->data.table.columnar) {
                  new_node->chart_data->source_table_id = b->id;
                  break;
                }
              }
            }
          }
        } else if (ds->pending_create == 20) { // Table layout (from toolbar)
          design_insert_tableau_layout(dd);
          ds->pending_create = 0;
        } else if (ds->pending_create == 12) { // Icon
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 60.0f, 60.0f, ds->selected_color);
          if (new_node) {
            new_node->font_type = 4; // Assuming 4 is ui->font_icons
            new_node->font_size = 48.0f;
            strncpy(new_node->text, "\xF3\x87\xAF",
                    sizeof(new_node->text) - 1); // ri-remixicon-line
          }
        } else if (ds->pending_create >= 10 && ds->pending_create <= 13) {
          NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                    NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
          new_node =
              design_create_styled_node(dd, styles[ds->pending_create - 10], nx,
                                        ny, def_w, def_h, ds->selected_color);
        }

        if (new_node)
          ui->selected_node = new_node;
        ds->pending_create = 0;
      }

      // Handle Bezier point dragging
      if (ds->bezier_mode && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_PATH) {
        Node *node = ui->selected_node;
        if (ds->dragging_point_idx != -1 && (ui->mouse_buttons & 1)) {
          float dx = 0;
          float dy = 0;
          NodePathPoint *p = &node->path_points[ds->dragging_point_idx];
          if (ds->dragging_cp_idx == 0) {
            p->x += dx;
            p->y += dy;
          } else if (ds->dragging_cp_idx == 1) {
            p->cp1x += dx;
            p->cp1y += dy;
          } else if (ds->dragging_cp_idx == 2) {
            p->cp2x += dx;
            p->cp2y += dy;
          }
          node_update_path_bounds(node);
        } else {
          ds->dragging_point_idx = -1;
        }
      }

      // Empty state hint
      if (dd->root && !dd->root->first_child) {
        const char *hint = "Click a tool to start";
        if (ui->font_inter) {
          nvgFontSize(ui->vg, ui->font_size_px);
          nvgFontFace(ui->vg, "Inter");
          nvgFillColor(ui->vg, nvgRGBA(200, 200, 200, 255));
          nvgTextAlign(ui->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
          nvgText(ui->vg, bounds.x + bounds.w / 2.0f,
                  bounds.y + bounds.h / 2.0f - 10.0f +
                      ui->font_size_px / 2.0f,
                  hint, NULL);
        }
      }

      // --- Apply artboard scissor ---
      // artboard_clip was set to art_rect (fixed) or infinite (free) above.
      if (dd->no_limits) {
      } else {
        // artboard_clip already set to art_rect — apply it now
        nvgScissor(ui->vg, ui->artboard_clip.x, ui->artboard_clip.y,
                   ui->artboard_clip.w, ui->artboard_clip.h);
      }

      // --- Render nodes ---
      if (dd->root) {
        ui->hovered_node_id = 0; // Reset each frame so hover doesn't persist
        for (Node *node = dd->root->first_child; node;
             node = node->next_sibling) {
          ui_render_node_tree(ui, engine, block, dd, node, bounds, scale,
                              bounds.x, bounds.y);
        }
        // Selection highlight over everything
        if (ui->selected_node && is_active_design && !ds->bezier_mode) {
          Node *node = ui->selected_node;
          float sx = bounds.x + node->x * scale;
          float sy = bounds.y + node->y * scale;
          Node *p = node->parent;
          while (p && p != dd->root) {
            sx += p->x * scale;
            sy += p->y * scale;
            p = p->parent;
          }
          float sw = node->width * scale;
          float sh = node->height * scale;

          float sel_off = 2.0f + (node->stroke_width * scale) / 2.0f;
          float rad = node->rotation * 3.14159f / 180.0f;
          float cx = sx + sw / 2.0f;
          float cy = sy + sh / 2.0f;

          nvgSave(ui->vg);
          nvgTranslate(ui->vg, cx, cy);
          nvgRotate(ui->vg, rad);

          // Bounding box outline
          nvgBeginPath(ui->vg);
          nvgRoundedRect(ui->vg, -(sw + sel_off * 2) / 2.0f,
                         -(sh + sel_off * 2) / 2.0f, sw + sel_off * 2,
                         sh + sel_off * 2,
                         0); // sharp corners for selection box
          NVGcolor accent_nvg = nvgRGBA(COLOR_ACCENT.r, COLOR_ACCENT.g,
                                        COLOR_ACCENT.b, COLOR_ACCENT.a);
          nvgStrokeColor(ui->vg, accent_nvg);
          nvgStrokeWidth(ui->vg, 1.0f);
          nvgStroke(ui->vg);

          // Handles points
          float hsz = 8.0f;
          float h_hw = (sw + sel_off * 2) / 2.0f;
          float h_hh = (sh + sel_off * 2) / 2.0f;
          float handle_pts[4][2] = {
              {h_hw, h_hh}, {-h_hw, h_hh}, {-h_hw, -h_hh}, {h_hw, -h_hh}};

          for (int i = 0; i < 4; i++) {
            nvgBeginPath(ui->vg);
            nvgRect(ui->vg, handle_pts[i][0] - hsz / 2.0f,
                    handle_pts[i][1] - hsz / 2.0f, hsz, hsz);
            nvgFillColor(ui->vg, accent_nvg);
            nvgFill(ui->vg);
          }

          // Rotation handle: small circle above top-center of the selection box
          float rot_dist = 28.0f;
          nvgBeginPath(ui->vg);
          nvgMoveTo(ui->vg, 0, -h_hh);
          nvgLineTo(ui->vg, 0, -(h_hh + rot_dist));
          nvgStrokeWidth(ui->vg, 1.2f);
          nvgStroke(ui->vg);

          float rh_r = 6.0f;
          nvgBeginPath(ui->vg);
          nvgCircle(ui->vg, 0, -(h_hh + rot_dist), rh_r);
          nvgFillColor(ui->vg, accent_nvg);
          nvgFill(ui->vg);
          nvgStrokeColor(ui->vg, nvgRGBA(255, 255, 255, 255));
          nvgStrokeWidth(ui->vg, 1.5f);
          nvgStroke(ui->vg);

          nvgRestore(ui->vg);
        }

        // Multi-selection: thin outline for each secondary selected node
        for (int _mi = 0; _mi < ui->selected_count; _mi++) {
          Node *mn = ui->selected_nodes[_mi];
          if (!mn || mn == ui->selected_node || !mn->visible)
            continue;
          float msx = bounds.x + mn->x * scale;
          float msy = bounds.y + mn->y * scale;
          Node *mp = mn->parent;
          while (mp && mp != dd->root) {
            msx += mp->x * scale;
            msy += mp->y * scale;
            mp = mp->parent;
          }
          float msw = mn->width * scale;
          float msh = mn->height * scale;
          float mrad = mn->rotation * 3.14159f / 180.0f;
          float mcx = msx + msw / 2.0f;
          float mcy = msy + msh / 2.0f;

          nvgSave(ui->vg);
          nvgTranslate(ui->vg, mcx, mcy);
          nvgRotate(ui->vg, mrad);

          nvgBeginPath(ui->vg);
          nvgRoundedRect(ui->vg, -(msw + 4) / 2.0f, -(msh + 4) / 2.0f, msw + 4,
                         msh + 4, 0);
          NVGcolor accent_nvg = nvgRGBA(COLOR_ACCENT.r, COLOR_ACCENT.g,
                                        COLOR_ACCENT.b, COLOR_ACCENT.a);
          nvgStrokeColor(ui->vg, accent_nvg);
          nvgStrokeWidth(ui->vg, 1.5f);
          nvgStroke(ui->vg);

          nvgRestore(ui->vg);
        }
      }

      // Check mouse in design area
      int mouse_in_design = (mx >= ws_bounds.x &&
                             mx < ws_bounds.x + ws_bounds.w &&
                             my >= ws_bounds.y &&
                             my < ws_bounds.y + ws_bounds.h);

      // Explicitly exclude sidebar area in fullscreen
      if (is_fullscreen && mx > width - 250.0f) {
        mouse_in_design = 0;
      }

      // While the split-view divider is being dragged the canvas bounds and
      // scale change every frame. Block all design interactions to prevent
      // nodes from jumping or being accidentally moved.
      if (ui->split_view.is_dragging_splitter) {
        mouse_in_design = 0;
        ds->is_dragging_node = 0;
        ds->is_panning = 0;
        ds->is_resizing = 0;
      }

      // --- Handle resize ---
      int on_resize_handle = 0;
      int hit_handle_index = 0;

      if (ui->selected_node && ui->selected_node->visible && is_active_design) {
        Node *sel = ui->selected_node;
        float sx = bounds.x + sel->x * scale;
        float sy = bounds.y + sel->y * scale;
        float sw = sel->width * scale;
        float sh = sel->height * scale;
        float rad = sel->rotation * 3.14159f / 180.0f;
        float cx = sx + sw / 2.0f;
        float cy = sy + sh / 2.0f;
        float sel_off = 2.0f + (sel->stroke_width * scale) / 2.0f;
        float h_hw = (sw + sel_off * 2) / 2.0f;
        float h_hh = (sh + sel_off * 2) / 2.0f;
        float c = cosf(rad);
        float s = sinf(rad);

        float handle_pts[4][2] = {
            {h_hw, h_hh},   // SE (0)
            {-h_hw, h_hh},  // SW (1)
            {-h_hw, -h_hh}, // NW (2)
            {h_hw, -h_hh}   // NE (3)
        };

        float hit_dist_sq = 15.0f * 15.0f; // roughly 30x30 effective hit area
        for (int i = 0; i < 4; i++) {
          float hx = cx + handle_pts[i][0] * c - handle_pts[i][1] * s;
          float hy = cy + handle_pts[i][0] * s + handle_pts[i][1] * c;
          float dx = mx - hx;
          float dy = my - hy;
          if (dx * dx + dy * dy < hit_dist_sq) {
            on_resize_handle = 1;
            hit_handle_index = i;
            break;
          }
        }
      }

      // --- Check rotation handle hit ---
      int on_rotate_handle = 0;
      if (!on_resize_handle && ui->selected_node &&
          ui->selected_node->visible && is_active_design) {
        Node *sel = ui->selected_node;
        float sx2 = bounds.x + sel->x * scale;
        float sy2 = bounds.y + sel->y * scale;
        float sw2 = sel->width * scale;
        float sh2 = sel->height * scale;
        float rad2 = sel->rotation * 3.14159f / 180.0f;
        float cx2 = sx2 + sw2 / 2.0f;
        float cy2 = sy2 + sh2 / 2.0f;
        float sel_off2 = 2.0f + (sel->stroke_width * scale) / 2.0f;
        float h_hh2 = (sh2 + sel_off2 * 2) / 2.0f;
        float c2 = cosf(rad2), s2 = sinf(rad2);
        float rot_dist = 28.0f;
        float rh_lx2 = 0.0f, rh_ly2 = -(h_hh2 + rot_dist);
        float rh_x2 = cx2 + rh_lx2 * c2 - rh_ly2 * s2;
        float rh_y2 = cy2 + rh_lx2 * s2 + rh_ly2 * c2;
        float rdx = mx - rh_x2;
        float rdy = my - rh_y2;
        if (rdx * rdx + rdy * rdy < 16.0f * 16.0f)
          on_rotate_handle = 1;
      }

      // Start resize
      if (mouse_in_design && ds->mouse_just_pressed && on_resize_handle &&
          !on_rotate_handle && ui->selected_node && is_active_design) {
        ds->is_resizing = 1;
        ds->resize_handle = hit_handle_index;
        ds->resize_start_w = ui->selected_node->width;
        ds->resize_start_h = ui->selected_node->height;
        ds->resize_start_x = mx;
        ds->resize_start_y = my;
        ds->resize_start_node_x = ui->selected_node->x;
        ds->resize_start_node_y = ui->selected_node->y;
        ds->resize_start_font_size = ui->selected_node->font_size;
      }
      // Handle resize dragging
      else if (ds->is_resizing && ui->selected_node && is_active_design) {
        float dx = (mx - ds->resize_start_x) / scale;
        float dy = (my - ds->resize_start_y) / scale;
        Node *sel = ui->selected_node;

        int lr = sel->lock_ratio && ds->resize_start_h > 0.1f;
        float ratio = lr ? ds->resize_start_w / ds->resize_start_h : 1.0f;
        float nw, nh;

        switch (ds->resize_handle) {
        case 0: // SE
          nw = ds->resize_start_w + dx;
          nh = ds->resize_start_h + dy;
          if (lr) {
            if (fabsf(dx) >= fabsf(dy))
              nh = nw / ratio;
            else
              nw = nh * ratio;
          }
          sel->width = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          break;
        case 1: // SW
          nw = ds->resize_start_w - dx;
          nh = ds->resize_start_h + dy;
          if (lr) {
            if (fabsf(dx) >= fabsf(dy))
              nh = nw / ratio;
            else
              nw = nh * ratio;
          }
          sel->width = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          sel->x = ds->resize_start_node_x + (ds->resize_start_w - sel->width);
          break;
        case 2: // NW
          nw = ds->resize_start_w - dx;
          nh = ds->resize_start_h - dy;
          if (lr) {
            if (fabsf(dx) >= fabsf(dy))
              nh = nw / ratio;
            else
              nw = nh * ratio;
          }
          sel->width = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          sel->x = ds->resize_start_node_x + (ds->resize_start_w - sel->width);
          sel->y = ds->resize_start_node_y + (ds->resize_start_h - sel->height);
          break;
        case 3: // NE
          nw = ds->resize_start_w + dx;
          nh = ds->resize_start_h - dy;
          if (lr) {
            if (fabsf(dx) >= fabsf(dy))
              nh = nw / ratio;
            else
              nw = nh * ratio;
          }
          sel->width = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          sel->y = ds->resize_start_node_y + (ds->resize_start_h - sel->height);
          break;
        }

        // Scale font size proportionally for text nodes
        if (sel->type == NODE_TYPE_TEXT && ds->resize_start_w > 0.1f) {
          float scale_factor = sel->width / ds->resize_start_w;
          sel->font_size = ds->resize_start_font_size * scale_factor;
          if (sel->font_size < 8.0f) {
            sel->font_size = 8.0f;
          }
        }

        if (ds->mouse_just_released) {
          ds->is_resizing = 0;
        }
      }

      // Start rotation
      if (mouse_in_design && ds->mouse_just_pressed && on_rotate_handle &&
          ui->selected_node && is_active_design) {
        Node *sel = ui->selected_node;
        float sx2 = bounds.x + sel->x * scale;
        float sy2 = bounds.y + sel->y * scale;
        float sw2 = sel->width * scale;
        float sh2 = sel->height * scale;
        float cx2 = sx2 + sw2 / 2.0f;
        float cy2 = sy2 + sh2 / 2.0f;
        ds->is_rotating = 1;
        ds->rotate_start_angle = atan2f(my - cy2, mx - cx2);
        ds->rotate_start_node_rotation = sel->rotation;
      }
      // Handle rotation dragging
      else if (ds->is_rotating && ui->selected_node && is_active_design) {
        Node *sel = ui->selected_node;
        float sx2 = bounds.x + sel->x * scale;
        float sy2 = bounds.y + sel->y * scale;
        float sw2 = sel->width * scale;
        float sh2 = sel->height * scale;
        float cx2 = sx2 + sw2 / 2.0f;
        float cy2 = sy2 + sh2 / 2.0f;
        float cur_angle = atan2f(my - cy2, mx - cx2);
        float delta_deg =
            (cur_angle - ds->rotate_start_angle) * 180.0f / 3.14159f;
        sel->rotation = ds->rotate_start_node_rotation + delta_deg;
        if (ds->mouse_just_released)
          ds->is_rotating = 0;
      }

      // 4. Handle Bezier interaction priority
      int handle_hit = 0;
      if (ds->bezier_mode && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_PATH && is_active_design) {
        Node *sel = ui->selected_node;
        float nsx = bounds.x + sel->x * scale;
        float nsy = bounds.y + sel->y * scale;
        for (int i = 0; i < sel->path_point_count; i++) {
          NodePathPoint *p = &sel->path_points[i];
          float px = nsx + p->x * scale, py = nsy + p->y * scale;
          AslanRect r_anchor = {px - 10 * scale, py - 10 * scale, 20 * scale, 20 * scale};
          if (mx >= r_anchor.x && mx < r_anchor.x + r_anchor.w &&
              my >= r_anchor.y && my < r_anchor.y + r_anchor.h) {
            if (ds->mouse_just_pressed) {
              ds->dragging_point_idx = i;
              ds->dragging_cp_idx = 0;
            }
            handle_hit = 1;
            break;
          }
          if (p->type == 1) {
            float cp1x = nsx + p->cp1x * scale, cp1y = nsy + p->cp1y * scale;
            float cp2x = nsx + p->cp2x * scale, cp2y = nsy + p->cp2y * scale;
            AslanRect r1 = {cp1x - 8 * scale, cp1y - 8 * scale, 16 * scale, 16 * scale};
            AslanRect r2 = {cp2x - 8 * scale, cp2y - 8 * scale, 16 * scale, 16 * scale};
            if (mx >= r1.x && mx < r1.x + r1.w &&
                my >= r1.y && my < r1.y + r1.h) {
              if (ds->mouse_just_pressed) {
                ds->dragging_point_idx = i;
                ds->dragging_cp_idx = 1;
              }
              handle_hit = 1;
              break;
            }
            if (mx >= r2.x && mx < r2.x + r2.w &&
                my >= r2.y && my < r2.y + r2.h) {
              if (ds->mouse_just_pressed) {
                ds->dragging_point_idx = i;
                ds->dragging_cp_idx = 2;
              }
              handle_hit = 1;
              break;
            }
          }
        }
      }

      // --- Check guide dragging ---
      if (mouse_in_design && is_active_design && !handle_hit &&
          !on_resize_handle && !on_rotate_handle) {
        int hit_h_guide = -1;
        int hit_v_guide = -1;
        DesignPage *dp = &dd->pages[dd->current_page];
        float hit_tol = 6.0f; // relatively small hit area in screen space
        for (int i = 0; i < dp->h_guide_count; i++) {
          float sy = bounds.y + dp->h_guides[i] * scale;
          if (fabsf(my - sy) < hit_tol) {
            hit_h_guide = i;
            break;
          }
        }
        if (hit_h_guide == -1) {
          for (int i = 0; i < dp->v_guide_count; i++) {
            float sx = bounds.x + dp->v_guides[i] * scale;
            if (fabsf(mx - sx) < hit_tol) {
              hit_v_guide = i;
              break;
            }
          }
        }

        if (ds->mouse_just_pressed &&
            (hit_h_guide != -1 || hit_v_guide != -1)) {
          if (hit_h_guide != -1) {
            ds->dragging_guide_type = 1;
            ds->dragging_guide_idx = hit_h_guide;
          } else {
            ds->dragging_guide_type = 2;
            ds->dragging_guide_idx = hit_v_guide;
          }
          handle_hit = 1; // consume click
        }

        // Also update cursor
        if (hit_h_guide != -1 || ds->dragging_guide_type == 1) {
          ui_set_cursor(ui, UI_CURSOR_SIZENS);
        } else if (hit_v_guide != -1 || ds->dragging_guide_type == 2) {
          ui_set_cursor(ui, UI_CURSOR_SIZEWE);
        }
      }

      if (ds->dragging_guide_type != 0 && is_active_design) {
        DesignPage *dp = &dd->pages[dd->current_page];
        if (ds->dragging_guide_type == 1) { // H guide
          dp->h_guides[ds->dragging_guide_idx] =
              (my - bounds.y) / scale;
        } else if (ds->dragging_guide_type == 2) { // V guide
          dp->v_guides[ds->dragging_guide_idx] =
              (mx - bounds.x) / scale;
        }
        if (ds->mouse_just_released) {
          ds->dragging_guide_type = 0;
        }
        handle_hit = 1; // prevent other interactions while dragging guide
      }

      // Handle click in design area
      if (handle_hit) {
        // Handle hit, skip node selection
      } else if (mouse_in_design && !in_toolbar && !in_forms_menu &&
                 ds->mouse_just_pressed && !on_resize_handle &&
                 !on_rotate_handle && is_active_design) {
        // Convert to design coordinates
        float design_x = (mx - bounds.x) / scale;
        float design_y = (my - bounds.y) / scale;

        // Shift+click = multi-selection toggle
        int shift_click = 0;

        Node *hit = design_find_node_at(dd, design_x, design_y);
        if (hit && shift_click && ds->tool != CANVAS_TOOL_PEN) {
          // Toggle this node in/out of the multi-selection set
          int found = 0;
          for (int _si = 0; _si < ui->selected_count; _si++) {
            if (ui->selected_nodes[_si] == hit) {
              // Remove: swap with last entry
              ui->selected_nodes[_si] =
                  ui->selected_nodes[--ui->selected_count];
              found = 1;
              break;
            }
          }
          if (!found && ui->selected_count < 64) {
            ui->selected_nodes[ui->selected_count++] = hit;
            ui->selected_node = hit; // update primary too
          }
          // Do NOT start drag on shift-click
        } else if (hit) {
          if (!shift_click) {
            // Normal click: clear multi-selection
            ui->selected_count = 0;
          }
          if (ds->tool == CANVAS_TOOL_PEN) {
            // Skip node logic in pen mode to allow drawing over them
            hit = NULL;
          } else if (hit->is_container && !hit->image_path[0]) {
            // Click on empty container "+" -> open image file dialog
            char path[512] = {0};
            if (open_image_file_dialog(path, sizeof(path))) {
              char rel[256] = {0};
              if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
                strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
                hit->image_path[sizeof(hit->image_path) - 1] = '\0';
              }
            }
            ui->selected_node = hit;
          } else {
            // Double-click: edit text / crop image
            int is_double = 0;
            if (is_double && hit == ui->selected_node) {
              if (hit->type == NODE_TYPE_TEXT) {
                // Edit existing text node inline natively
                ui->editing_node = hit;
                strncpy(ui->edit_buffer, hit->text,
                        sizeof(ui->edit_buffer) - 1);
                ui->rename_needs_focus = 1;
                ui->edit_buffer[sizeof(ui->edit_buffer) - 1] = '\0';
              }
            }
            if (!ui->editing_node &&
                !(ds->is_cropping && hit->type == NODE_TYPE_IMAGE)) {
              ui->selected_node = hit;
              // In bezier mode, only select — don't drag so handles take
              // priority
              if (!ds->bezier_mode) {
                ds->is_dragging_node = 1;
                ds->drag_offset_x = design_x - hit->x;
                ds->drag_offset_y = design_y - hit->y;
                ds->drag_press_x = mx;
                ds->drag_press_y = my;
              }
            }
          }
        } else {
          ui->selected_node = NULL;
          ui->selected_count = 0;
          ds->is_cropping = 0; // stop cropping
          if (ui->editing_node) {
            strncpy(ui->editing_node->text, ui->edit_buffer,
                    sizeof(ui->editing_node->text) - 1);
            ui->editing_node = NULL;
          }
          // Start panning in no_limits mode when clicking background
          if (dd->no_limits && ds->tool != CANVAS_TOOL_PEN &&
              !ui->block_drag.is_active) {
            ds->is_panning = 1;
            ds->pan_start_x = mx;
            ds->pan_start_y = my;
            ds->cam_start_x = ds->cam_x;
            ds->cam_start_y = ds->cam_y;
          }
        }
      }

      // Handle image cropping dragging (active until user clicks crop btn
      // again)
      if (ds->is_cropping && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_IMAGE && is_active_design) {
        Node *sel = ui->selected_node;
        // Init start pos on each new press
        if (ds->mouse_just_pressed && mouse_in_design) {
          ds->crop_start_x = mx;
          ds->crop_start_y = my;
          ds->crop_start_node_x = sel->image_crop_x;
          ds->crop_start_node_y = sel->image_crop_y;
        }
        if ((ui->mouse_buttons & 1) &&
            mouse_in_design &&
            (ds->crop_start_x != 0 || ds->crop_start_y != 0)) {
          float dx = (mx - ds->crop_start_x) / scale;
          float dy = (my - ds->crop_start_y) / scale;
          sel->image_crop_x = ds->crop_start_node_x + dx;
          sel->image_crop_y = ds->crop_start_node_y + dy;
        }
        // Crop mode stays active — toggled off only via the button in
        // Properties
      }
      // Handle node dragging
      else if (ds->is_dragging_node && ui->selected_node && is_active_design) {
        // 5px threshold to prevent accidental drag on simple click
        float pdx = mx - ds->drag_press_x;
        float pdy = my - ds->drag_press_y;
        if (fabsf(pdx) < 5.0f && fabsf(pdy) < 5.0f) {
          if (ds->mouse_just_released)
            ds->is_dragging_node = 0;
          goto after_drag;
        }

        float design_x = (mx - bounds.x) / scale;
        float design_y = (my - bounds.y) / scale;
        Node *sel = ui->selected_node;

        float new_x = design_x - ds->drag_offset_x;
        float new_y = design_y - ds->drag_offset_y;

        // --- Snap to grid ---
        DesignPage *dp = &dd->pages[dd->current_page];
        if (dp->grid_type != 0 && dp->grid_spacing > 4.0f) {
          float gs = dp->grid_spacing;
          new_x = roundf(new_x / gs) * gs;
          new_y = roundf(new_y / gs) * gs;
        }

        // --- Snap to element edges ---
        float snap_tol = 6.0f; // design units
        float best_dx = snap_tol + 1.0f, best_dy = snap_tol + 1.0f;
        float snap_sx = -1, snap_sy = -1; // screen-space guide positions
        // 3 X snap points on dragged node
        float s_left = new_x, s_cx = new_x + sel->width / 2.0f,
              s_right = new_x + sel->width;
        float s_top = new_y, s_cy = new_y + sel->height / 2.0f,
              s_bot = new_y + sel->height;
        for (Node *other = dd->root->first_child; other;
             other = other->next_sibling) {
          if (other == sel || !other->visible)
            continue;
          float o_left = other->x, o_cx = other->x + other->width / 2.0f,
                o_right = other->x + other->width;
          float o_top = other->y, o_cy = other->y + other->height / 2.0f,
                o_bot = other->y + other->height;
          float x_cands[9] = {
              o_left - s_left,  o_cx - s_left,  o_right - s_left, o_cx - s_cx,
              o_left - s_right, o_cx - s_right, o_right - s_right};
          float y_cands[9] = {o_top - s_top, o_cy - s_top,  o_bot - s_top,
                              o_cy - s_cy,   o_top - s_bot, o_cy - s_bot,
                              o_bot - s_bot};
          for (int _xi = 0; _xi < 7; _xi++) {
            if (fabsf(x_cands[_xi]) < fabsf(best_dx)) {
              best_dx = x_cands[_xi];
              // Guide at the snapped node edge in screen space
              snap_sx = bounds.x + (new_x + best_dx) * scale;
            }
          }
          for (int _yi = 0; _yi < 7; _yi++) {
            if (fabsf(y_cands[_yi]) < fabsf(best_dy)) {
              best_dy = y_cands[_yi];
              snap_sy = bounds.y + (new_y + best_dy) * scale;
            }
          }
        }
        if (fabsf(best_dx) <= snap_tol) {
          new_x += best_dx;
          ds->snap_guide_x = snap_sx;
        } else {
          ds->snap_guide_x = -1;
        }
        if (fabsf(best_dy) <= snap_tol) {
          new_y += best_dy;
          ds->snap_guide_y = snap_sy;
        } else {
          ds->snap_guide_y = -1;
        }

        // Apply delta to secondary selected nodes
        float ddx = new_x - sel->x;
        float ddy = new_y - sel->y;
        sel->x = new_x;
        sel->y = new_y;
        // Stroke nodes: also translate all stroke points
        if (sel->type == NODE_TYPE_STROKE && sel->stroke_data) {
          Stroke *s = sel->stroke_data;
          for (int _k = 0; _k < s->count; _k++) {
            s->points[_k].x += ddx;
            s->points[_k].y += ddy;
          }
        }
        for (int _mi = 0; _mi < ui->selected_count; _mi++) {
          Node *mn = ui->selected_nodes[_mi];
          if (mn && mn != sel) {
            mn->x += ddx;
            mn->y += ddy;
            if (mn->type == NODE_TYPE_STROKE && mn->stroke_data) {
              Stroke *s = mn->stroke_data;
              for (int _k = 0; _k < s->count; _k++) {
                s->points[_k].x += ddx;
                s->points[_k].y += ddy;
              }
            }
          }
        }

        if (ds->mouse_just_released) {
          ds->is_dragging_node = 0;
          ds->snap_guide_x = -1;
          ds->snap_guide_y = -1;
        }
      }
    after_drag: (void)0;

      // --- Right-click context menu detection ---
      if (ui->mouse_buttons & 2 &&
          mouse_in_design && !in_toolbar && is_active_design) {
        float dx = (mx - bounds.x) / scale;
        float dy = (my - bounds.y) / scale;
        Node *hit = design_find_node_at(dd, dx, dy);
        if (hit) {
          ui->show_context_menu = 1;
          ui->context_menu_x = mx;
          ui->context_menu_y = my;
          ui->context_menu_node = hit;
          ui->selected_node = hit;
        } else {
          ui->show_context_menu = 0;
        }
      }

      // Close forms menu if clicking elsewhere
      if (ds->mouse_just_pressed && !in_toolbar && !in_forms_menu &&
          is_active_design) {
      }

      // --- PEN TOOL logic (Adapted for Design) ---
      if (ds->tool == CANVAS_TOOL_PEN && is_active_design && !in_toolbar &&
          !in_forms_menu && ((ui->mouse_buttons & 1))) {
        float wx = (mx - bounds.x) / scale;
        float wy = (my - bounds.y) / scale;

        if (ds->mouse_just_pressed) {
          ds->current_stroke = (Stroke *)calloc(1, sizeof(Stroke));
          ds->current_stroke->capacity = 128;
          ds->current_stroke->points = (Point *)calloc(128, sizeof(Point));
          if (ds->pen_is_marker) {
            ds->current_stroke->thickness = ds->pen_thickness * 10.0f;
            uint32_t c = ds->selected_color;
            uint8_t r = (c >> 24) & 0xFF;
            uint8_t g = (c >> 16) & 0xFF;
            uint8_t b = (c >> 8) & 0xFF;
            uint8_t a = c & 0xFF;
            // Reduce alpha to 30% for marker effect
            ds->current_stroke->color =
                (r << 24) | (g << 16) | (b << 8) | (uint8_t)(a * 0.3f);
          } else {
            ds->current_stroke->thickness = ds->pen_thickness;
            ds->current_stroke->color = ds->selected_color;
          }
          ds->current_stroke->points[0].x = wx;
          ds->current_stroke->points[0].y = wy;
          ds->current_stroke->count = 1;

          // Wrap it in a Node so it appears in Layers
          Node *node = (Node *)calloc(1, sizeof(Node));
          if (node) {
            node->id = dd->next_node_id++;
            node->type = NODE_TYPE_STROKE;
            node->stroke_data = ds->current_stroke;
            node->visible = 1;
            node->parent = dd->root;

            // Add to document tree
            if (!dd->root->first_child) {
              dd->root->first_child = node;
            } else {
              Node *last = dd->root->first_child;
              while (last->next_sibling)
                last = last->next_sibling;
              last->next_sibling = node;
            }
            ds->current_stroke_node = node;
            ds->stroke_block_id = block->id;

            // Initialize bounds
            node->x = wx;
            node->y = wy;
            node->width = 1.0f;
            node->height = 1.0f;
          }
        } else if (ds->current_stroke && ds->stroke_block_id == block->id) {
          if (ds->current_stroke->count >= ds->current_stroke->capacity) {
            ds->current_stroke->capacity *= 2;
            ds->current_stroke->points =
                (Point *)realloc(ds->current_stroke->points,
                                 ds->current_stroke->capacity * sizeof(Point));
          }
          Point last =
              ds->current_stroke->points[ds->current_stroke->count - 1];
          if ((wx - last.x) * (wx - last.x) + (wy - last.y) * (wy - last.y) >
              4.0f) {
            ds->current_stroke->points[ds->current_stroke->count].x = wx;
            ds->current_stroke->points[ds->current_stroke->count].y = wy;
            ds->current_stroke->count++;

            // Update node bounds for culling
            if (ds->current_stroke_node) {
              float min_x = ds->current_stroke_node->x;
              float min_y = ds->current_stroke_node->y;
              float max_x = min_x + ds->current_stroke_node->width;
              float max_y = min_y + ds->current_stroke_node->height;

              if (wx < min_x)
                min_x = wx;
              if (wy < min_y)
                min_y = wy;
              if (wx > max_x)
                max_x = wx;
              if (wy > max_y)
                max_y = wy;

              ds->current_stroke_node->x = min_x;
              ds->current_stroke_node->y = min_y;
              ds->current_stroke_node->width = max_x - min_x;
              ds->current_stroke_node->height = max_y - min_y;
            }
          }
        }
      } else if (!(ui->mouse_buttons & 1)) {
        ds->current_stroke = NULL;
        ds->stroke_block_id = 0;
      }

      // --- Active Stroke Rendering ---
      if (ds->current_stroke && ds->current_stroke->count > 1 &&
          ds->stroke_block_id == block->id) {
        if (ds->pen_is_marker) {
          // Simplification, just draw the lines for now. Arrow heads will be
          // parsed on commit.
          {
            NVGcolor col =
                nvgRGBA((ds->current_stroke->color >> 24) & 0xFF,
                        (ds->current_stroke->color >> 16) & 0xFF,
                        (ds->current_stroke->color >> 8) & 0xFF,
                        ds->current_stroke->color & 0xFF);
            for (int i = 0; i < ds->current_stroke->count - 1; i++) {
              Point p1 = ds->current_stroke->points[i];
              Point p2 = ds->current_stroke->points[i + 1];
              nvgBeginPath(ui->vg);
              nvgMoveTo(ui->vg, bounds.x + p1.x * scale,
                        bounds.y + p1.y * scale);
              nvgLineTo(ui->vg, bounds.x + p2.x * scale,
                        bounds.y + p2.y * scale);
              nvgStrokeColor(ui->vg, col);
              nvgStrokeWidth(ui->vg, ds->current_stroke->thickness * scale);
              nvgStroke(ui->vg);
            }
          }
        } else {
          NVGcolor col =
              nvgRGBA((ds->current_stroke->color >> 24) & 0xFF,
                      (ds->current_stroke->color >> 16) & 0xFF,
                      (ds->current_stroke->color >> 8) & 0xFF,
                      ds->current_stroke->color & 0xFF);
          for (int i = 0; i < ds->current_stroke->count - 1; i++) {
            Point p1 = ds->current_stroke->points[i];
            Point p2 = ds->current_stroke->points[i + 1];
            nvgBeginPath(ui->vg);
            nvgMoveTo(ui->vg, bounds.x + p1.x * scale,
                      bounds.y + p1.y * scale);
            nvgLineTo(ui->vg, bounds.x + p2.x * scale,
                      bounds.y + p2.y * scale);
            nvgStrokeColor(ui->vg, col);
            nvgStrokeWidth(ui->vg, ds->current_stroke->thickness * scale);
            nvgStroke(ui->vg);
          }
        }
      }

      // --- Bezier Handles Rendering Overlay ---
      if (ds->bezier_mode && is_active_design && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_PATH) {
        Node *node = ui->selected_node;
        // Ensure node belongs to this design block
        // (Basic check: if it's selected and we are active, assume it is)
        {
          float sx = bounds.x + node->x * scale;
          float sy = bounds.y + node->y * scale;

          for (int i = 0; i < node->path_point_count; i++) {
            NodePathPoint *p = &node->path_points[i];
            float px = sx + p->x * scale;
            float py = sy + p->y * scale;

            // Anchor
            nvgBeginPath(ui->vg);
            nvgRect(ui->vg, px - 5 * scale, py - 5 * scale, 10 * scale,
                    10 * scale);
            nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
            nvgFill(ui->vg);
            nvgStrokeColor(ui->vg, nvgRGBA(COLOR_ACCENT.r, COLOR_ACCENT.g,
                                           COLOR_ACCENT.b, COLOR_ACCENT.a));
            nvgStrokeWidth(ui->vg, 1.5f * scale);
            nvgStroke(ui->vg);

            if (p->type == 1) { // Cubic
              float cp1x = sx + p->cp1x * scale;
              float cp1y = sy + p->cp1y * scale;
              float cp2x = sx + p->cp2x * scale;
              float cp2y = sy + p->cp2y * scale;

              nvgBeginPath(ui->vg);
              nvgMoveTo(ui->vg, px, py);
              nvgLineTo(ui->vg, cp1x, cp1y);
              nvgStrokeColor(ui->vg, nvgRGBA(200, 200, 200, 255));
              nvgStrokeWidth(ui->vg, 1.0f);
              nvgStroke(ui->vg);

              nvgBeginPath(ui->vg);
              nvgMoveTo(ui->vg, px, py);
              nvgLineTo(ui->vg, cp2x, cp2y);
              nvgStrokeColor(ui->vg, nvgRGBA(200, 200, 200, 255));
              nvgStrokeWidth(ui->vg, 1.0f);
              nvgStroke(ui->vg);

              nvgBeginPath(ui->vg);
              nvgCircle(ui->vg, cp1x, cp1y, 4 * scale);
              nvgFillColor(ui->vg, nvgRGBA(COLOR_ACCENT.r, COLOR_ACCENT.g,
                                           COLOR_ACCENT.b, COLOR_ACCENT.a));
              nvgFill(ui->vg);

              nvgBeginPath(ui->vg);
              nvgCircle(ui->vg, cp2x, cp2y, 4 * scale);
              nvgFillColor(ui->vg, nvgRGBA(COLOR_ACCENT.r, COLOR_ACCENT.g,
                                           COLOR_ACCENT.b, COLOR_ACCENT.a));
              nvgFill(ui->vg);
            }
          }
        }
      }

      // --- Alignment guides when dragging ---
      if (ds->is_dragging_node && ui->selected_node) {
        Node *sel = ui->selected_node;
        float nsx = bounds.x + sel->x * scale;
        float nsy = bounds.y + sel->y * scale;
        float nsw = sel->width * scale;
        float nsh = sel->height * scale;
        draw_alignment_guides(ui, sel, bounds.x + bounds.w / 2.0f,
                              bounds.y + bounds.h / 2.0f, bounds.x, bounds.y,
                              bounds.w, bounds.h, nsx, nsy, nsw, nsh);
      }

      // --- Snap guide lines (element-to-element snap feedback) ---
      if (ds->snap_guide_x >= 0 || ds->snap_guide_y >= 0) {
        NVGcolor snap_col = nvgRGBA(0, 195, 215, 200);
        if (ds->snap_guide_x >= 0) {
          nvgBeginPath(ui->vg);
          nvgMoveTo(ui->vg, ds->snap_guide_x, bounds.y);
          nvgLineTo(ui->vg, ds->snap_guide_x, bounds.y + bounds.h);
          nvgStrokeColor(ui->vg, snap_col);
          nvgStrokeWidth(ui->vg, 1.0f);
          nvgStroke(ui->vg);
        }
        if (ds->snap_guide_y >= 0) {
          nvgBeginPath(ui->vg);
          nvgMoveTo(ui->vg, bounds.x, ds->snap_guide_y);
          nvgLineTo(ui->vg, bounds.x + bounds.w, ds->snap_guide_y);
          nvgStrokeColor(ui->vg, snap_col);
          nvgStrokeWidth(ui->vg, 1.0f);
          nvgStroke(ui->vg);
        }
      }

      // Close Design content group (Artboard)
      ASLAN_DEBUG("END design_content");
    }

    // --- Fixed artboard: draw border on top of clipped content ---
    if (!dd->no_limits) {
      nvgBeginPath(ui->vg);
      nvgRect(ui->vg, ui->artboard_rect.x, ui->artboard_rect.y,
              ui->artboard_rect.w, ui->artboard_rect.h);
      nvgStrokeColor(ui->vg, nvgRGBA(0, 0, 0, 60));
      nvgStrokeWidth(ui->vg, 1.0f);
      nvgStroke(ui->vg);
    }

    // --- Toolbar (same as Canvas) ---
    // Rendered in workspace coordinate space, not artboard space

    // Render Toolbar using Canvas logic (with state swapping)
    {
      CanvasInteraction temp_cs = ui->canvas_state;

      // Map Design state to Canvas state
      ui->canvas_state.tool = ds->tool;
      ui->canvas_state.active_cat = ds->active_cat;
      ui->canvas_state.show_sub_menu = ds->show_sub_menu;
      ui->canvas_state.pending_create = ds->pending_create;
      ui->canvas_state.mouse_just_pressed = ds->mouse_just_pressed;
      ui->canvas_state.space_held = ds->space_held;
      ui->canvas_state.pan_from_menu = ds->pan_from_menu;
      ui->canvas_state.bezier_mode = ds->bezier_mode;
      ui->canvas_state.dragging_point_idx = ds->dragging_point_idx;
      ui->canvas_state.dragging_cp_idx = ds->dragging_cp_idx;
      ui->canvas_state.selected_color = ds->selected_color;
      ui->canvas_state.pen_thickness = ds->pen_thickness;
      ui->canvas_state.pen_is_marker = ds->pen_is_marker;
      ui->canvas_state.show_color_picker_popup = ds->show_color_picker_popup;

      // Position toolbar at bottom of workspace
      nvgResetScissor(ui->vg);
      canvas_render_toolbar(ui, ws_bounds.x, ws_bounds.y, ws_bounds.w,
                            ws_bounds.h, block);

      // Copy back relevant state
      ds->tool = ui->canvas_state.tool;
      ds->active_cat = ui->canvas_state.active_cat;
      ds->show_sub_menu = ui->canvas_state.show_sub_menu;
      ds->pending_create = ui->canvas_state.pending_create;
      ds->space_held = ui->canvas_state.space_held;
      ds->pan_from_menu = ui->canvas_state.pan_from_menu;
      ds->bezier_mode = ui->canvas_state.bezier_mode;
      ds->dragging_point_idx = ui->canvas_state.dragging_point_idx;
      ds->dragging_cp_idx = ui->canvas_state.dragging_cp_idx;
      ds->selected_color = ui->canvas_state.selected_color;
      ds->pen_thickness = ui->canvas_state.pen_thickness;
      ds->pen_is_marker = ui->canvas_state.pen_is_marker;
      ds->show_color_picker_popup = ui->canvas_state.show_color_picker_popup;

      // Restore Canvas state
      ui->canvas_state = temp_cs;
    }

    if (ds->mouse_just_pressed &&
        !(mx >= ws_bounds.x && mx < ws_bounds.x + ws_bounds.w &&
          my >= ws_bounds.y + ws_bounds.h - toolbar_h && my < ws_bounds.y + ws_bounds.h)) {
    }

    // --- Zoom controls for no_limits mode ---
    if (dd->no_limits) {
      // Zoom with scroll wheel
      if (mx >= ws_bounds.x &&
          mx < ws_bounds.x + ws_bounds.w &&
          my >= ws_bounds.y &&
          my < ws_bounds.y + ws_bounds.h) {
        float scroll = 0;
        if (scroll != 0.0f) {
          float old_zoom = ds->zoom;
          if (scroll > 0)
            ds->zoom *= 1.15f;
          else
            ds->zoom *= 0.87f;
          if (ds->zoom < 0.1f)
            ds->zoom = 0.1f;
          if (ds->zoom > 5.0f)
            ds->zoom = 5.0f;
          float cx = ws_bounds.w / 2.0f, cy = ws_bounds.h / 2.0f;
          ds->cam_x += cx / old_zoom - cx / ds->zoom;
          ds->cam_y += cy / old_zoom - cy / ds->zoom;
        }
      }

      // Zoom percentage display
      char zoom_str[16];
      snprintf(zoom_str, sizeof(zoom_str), "%d%%", (int)(ds->zoom * 100));
      float zx = ws_bounds.x + ws_bounds.w - 60.0f;
      float zy = ws_bounds.y + ws_bounds.h - 30.0f;
      if (ui->font_inter) {
        nvgFontSize(ui->vg, ui->font_size_px);
        nvgFontFace(ui->vg, "Inter");
        nvgFillColor(ui->vg, nvgRGBA(120, 120, 120, 255));
        nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgText(ui->vg, zx, zy, zoom_str, NULL);
      }

      // Save camera state back to DesignData
      dd->view_x = ds->cam_x;
      dd->view_y = ds->cam_y;
      dd->zoom = ds->zoom;
    }

    // --- Right-click context menu ---
    render_node_context_menu(ui, block);

    // --- Native Inline Text Editor Overlay ---
    if (ui->editing_node && ui->editing_node->type == NODE_TYPE_TEXT &&
        is_active_design) {
      if (ui->rename_needs_focus)
        ui->rename_needs_focus = 0;
      ui->editing_node = NULL;
    }

  }

  // --- Rendering Sidebar ---
  if (ws_width < width) {
    nvgResetScissor(ui->vg);
    nvgSave(ui->vg);
    nvgTranslate(ui->vg, ws_width, 0);
    design_render_sidebar(ui, engine, block, sidebar_w, height, ui->abs_offset_x + ws_width);
    nvgRestore(ui->vg);
  }
}
