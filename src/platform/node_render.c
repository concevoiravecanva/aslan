#include "node_render.h"
#include "ui_canvas.h"
#include "../document/aslan_table.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

void ui_render_chart_node(UIHost *ui, Node *node, AslanRect bounds, Page *page);
void ui_render_chart_node_config_bar(UIHost *ui, Engine *engine, Node *node, AslanRect bounds, Page *page);

#include <stdlib.h>

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
        // Clip to artboard
        nvgScissor(ui->vg, ui->artboard_clip.x, ui->artboard_clip.y,
                   ui->artboard_clip.w, ui->artboard_clip.h);

        float img_w = nr.w * node->image_scale;
        float img_h = nr.h * node->image_scale;

        // Apply rotation around the node center
        float cx = nr.x + nr.w * 0.5f;
        float cy = nr.y + nr.h * 0.5f;
        float rad = node->rotation * NVG_PI / 180.0f;

        nvgSave(ui->vg);
        nvgTranslate(ui->vg, cx, cy);
        if (rad != 0.0f)
          nvgRotate(ui->vg, rad);

        // Coordinates relative to rotated center
        float img_x = node->image_crop_x * scale - nr.w * 0.5f;
        float img_y = node->image_crop_y * scale - nr.h * 0.5f;

        NVGpaint imgPaint = nvgImagePattern(ui->vg, img_x, img_y,
                                            img_w, img_h, 0, img, 1.0f);
        nvgBeginPath(ui->vg);
        // Draw clipped to node bounds (half-extents in local space)
        float corner_r = node->corner_radius * scale;
        if (corner_r > 0.0f)
          nvgRoundedRect(ui->vg, -nr.w * 0.5f, -nr.h * 0.5f, nr.w, nr.h, corner_r);
        else
          nvgRect(ui->vg, -nr.w * 0.5f, -nr.h * 0.5f, nr.w, nr.h);
        nvgFillPaint(ui->vg, imgPaint);
        nvgFill(ui->vg);

        nvgRestore(ui->vg);

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
