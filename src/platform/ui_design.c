#include "ui_design.h"
#include "../document/aslan_table.h"
#include "../document/document.h"
#include "ui_canvas.h"
#include "ui_shared.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int node_get_path_polygon(Node *node, float *out_pts, int max_pts, float sx,
                          float sy, float zoom);
int open_image_file_dialog(char *out_path, size_t path_size);
int copy_image_to_assets(const char *src_path, const char *doc_dir,
                         char *out_rel_path, size_t out_size);
void draw_alignment_guides(struct nk_command_buffer *canvas_buf, Node *node,
                           float area_cx, float area_cy, float area_x,
                           float area_y, float area_w, float area_h,
                           float node_sx, float node_sy, float node_sw,
                           float node_sh);
void render_node_context_menu(UIHost *ui, struct nk_context *ctx,
                              struct nk_command_buffer *canvas_buf,
                              Block *block);

// Helper for rotated rounded rect
static void fill_rotated_rounded_rect(struct nk_command_buffer *canvas,
                                      float cx, float cy, float w, float h,
                                      float rad, float radius,
                                      struct nk_color col, int is_stroke,
                                      float thickness) {
  if (radius < 0.1f) {
    float c = cosf(rad);
    float s = sinf(rad);
    float hw = w / 2.0f;
    float hh = h / 2.0f;
    float pts[8] = {cx + (-hw) * c - (-hh) * s, cy + (-hw) * s + (-hh) * c,
                    cx + (hw)*c - (-hh) * s,    cy + (hw)*s + (-hh) * c,
                    cx + (hw)*c - (hh)*s,       cy + (hw)*s + (hh)*c,
                    cx + (-hw) * c - (hh)*s,    cy + (-hw) * s + (hh)*c};
    if (is_stroke)
      nk_stroke_polygon(canvas, pts, 4, thickness, col);
    else
      nk_fill_polygon(canvas, pts, 4, col);
    return;
  }

  // Generate rounded vertices
  float pts[64 * 2];
  int count = 0;
  float c = cosf(rad);
  float s = sinf(rad);
  float hw = w / 2.0f;
  float hh = h / 2.0f;

  // 4 corners
  float offsets[4][2] = {{hw - radius, hh - radius},
                         {-hw + radius, hh - radius},
                         {-hw + radius, -hh + radius},
                         {hw - radius, -hh + radius}};
  float angles[4] = {0, 3.14159f / 2.0f, 3.14159f, 3.14159f * 1.5f};

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j <= 8; j++) {
      float a = angles[i] + (j / 8.0f) * (3.14159f / 2.0f);
      float px = offsets[i][0] + cosf(a) * radius;
      float py = offsets[i][1] + sinf(a) * radius;
      pts[count * 2] = cx + px * c - py * s;
      pts[count * 2 + 1] = cy + px * s + py * c;
      count++;
    }
  }

  if (is_stroke)
    nk_stroke_polygon(canvas, pts, count, thickness, col);
  else
    nk_fill_polygon(canvas, pts, count, col);
}

static void fill_rounded_polygon(struct nk_command_buffer *canvas,
                                 const struct nk_vec2 *points, int count,
                                 float radius, struct nk_color col,
                                 int is_stroke, float thickness) {
  if (count < 3)
    return;
  if (radius < 0.1f) {
    float pts[128]; // max 64 vertices for raw polygon
    int pc = (count > 64) ? 64 : count;
    for (int i = 0; i < pc; i++) {
      pts[i * 2] = points[i].x;
      pts[i * 2 + 1] = points[i].y;
    }
    if (is_stroke)
      nk_stroke_polygon(canvas, pts, pc, thickness, col);
    else
      nk_fill_polygon(canvas, pts, pc, col);
    return;
  }

  float out_pts[1024]; // max 512 points for rounded version
  int out_count = 0;

  for (int i = 0; i < count; i++) {
    struct nk_vec2 p1 = points[(i + count - 1) % count];
    struct nk_vec2 p2 = points[i];
    struct nk_vec2 p3 = points[(i + 1) % count];

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

  if (is_stroke)
    nk_stroke_polygon(canvas, out_pts, out_count, thickness, col);
  else
    nk_fill_polygon(canvas, out_pts, out_count, col);
}

// Proper thick polygon stroke: each edge as a filled quad + round cap at every
// vertex. Eliminates the gap/overlap artifacts of nk_stroke_polygon at large
// thicknesses. pts = interleaved x0,y0,x1,y1,... , count = vertex count.
static void draw_thick_polygon_stroke(struct nk_command_buffer *canvas,
                                      const float *pts, int count,
                                      float thickness, struct nk_color col) {
  if (count < 2 || thickness < 0.5f) {
    nk_stroke_polygon(canvas, pts, count, thickness, col);
    return;
  }
  float r = thickness / 2.0f;
  for (int i = 0; i < count; i++) {
    float x1 = pts[i * 2];
    float y1 = pts[i * 2 + 1];
    float x2 = pts[((i + 1) % count) * 2];
    float y2 = pts[((i + 1) % count) * 2 + 1];
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) continue;
    float nx = -dy / len * r, ny = dx / len * r;
    float quad[8] = {x1 + nx, y1 + ny, x2 + nx, y2 + ny,
                     x2 - nx, y2 - ny, x1 - nx, y1 - ny};
    nk_fill_polygon(canvas, quad, 4, col);
    nk_fill_circle(canvas, nk_rect(x1 - r, y1 - r, r * 2, r * 2), col);
  }
}

// nk_vec2 variant
static void draw_thick_polygon_stroke_v(struct nk_command_buffer *canvas,
                                        const struct nk_vec2 *pts, int count,
                                        float thickness, struct nk_color col) {
  float flat[128];
  int n = (count < 64) ? count : 64;
  for (int i = 0; i < n; i++) {
    flat[i * 2] = pts[i].x;
    flat[i * 2 + 1] = pts[i].y;
  }
  draw_thick_polygon_stroke(canvas, flat, n, thickness, col);
}

// Rounded rect as single polygon — avoids alpha seam artifacts from overlapping
// primitives when opacity < 1.  12 segments per quarter arc = 48 total points.
static void gen_rounded_rect_polygon(struct nk_rect r, float cr,
                                     float *pts, int *out_count) {
  float max_cr = (r.w < r.h ? r.w : r.h) * 0.5f;
  if (cr > max_cr) cr = max_cr;
  const int segs = 12;
  const float step = 1.5707963f / segs; // PI/2 / segs
  int n = 0;
  float ax, ay, t;
  // TL: 180° → 270°
  ax = r.x + cr; ay = r.y + cr;
  for (int i = 0; i < segs; i++) { t = 3.14159265f + i * step; pts[n++] = ax + cosf(t)*cr; pts[n++] = ay + sinf(t)*cr; }
  // TR: 270° → 360°
  ax = r.x + r.w - cr; ay = r.y + cr;
  for (int i = 0; i < segs; i++) { t = 4.71238898f + i * step; pts[n++] = ax + cosf(t)*cr; pts[n++] = ay + sinf(t)*cr; }
  // BR: 0° → 90°
  ax = r.x + r.w - cr; ay = r.y + r.h - cr;
  for (int i = 0; i < segs; i++) { t = i * step; pts[n++] = ax + cosf(t)*cr; pts[n++] = ay + sinf(t)*cr; }
  // BL: 90° → 180°
  ax = r.x + cr; ay = r.y + r.h - cr;
  for (int i = 0; i < segs; i++) { t = 1.5707963f + i * step; pts[n++] = ax + cosf(t)*cr; pts[n++] = ay + sinf(t)*cr; }
  *out_count = segs * 4;
}

static void fill_smooth_rounded_rect(struct nk_command_buffer *canvas,
                                     struct nk_rect r, float cr,
                                     struct nk_color col) {
  if (cr < 0.5f) { nk_fill_rect(canvas, r, 0, col); return; }
  float pts[48 * 2]; int count;
  gen_rounded_rect_polygon(r, cr, pts, &count);
  nk_fill_polygon(canvas, pts, count, col);
}

static void stroke_smooth_rounded_rect(struct nk_command_buffer *canvas,
                                       struct nk_rect r, float cr,
                                       float thickness, struct nk_color col) {
  if (cr < 0.5f) { nk_stroke_rect(canvas, r, 0, thickness, col); return; }
  float pts[48 * 2]; int count;
  gen_rounded_rect_polygon(r, cr, pts, &count);
  if (thickness < 1.5f)
    nk_stroke_polygon(canvas, pts, count, thickness, col);
  else
    draw_thick_polygon_stroke(canvas, pts, count, thickness, col);
}

// Linear interpolation between two colors
static struct nk_color lerp_color(struct nk_color a, struct nk_color b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return nk_rgba((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t),
                 (int)(a.b + (b.b - a.b) * t), (int)(a.a + (b.a - a.a) * t));
}

// Gradient-filled rounded rect at any angle (0=left→right, 90=top→bottom).
// Uses horizontal strips to support corner_radius correctly.
// nk_fill_rect_multi_color corner mapping: left=TL, top=TR, right=BR, bottom=BL
static void fill_gradient_rounded_rect(struct nk_command_buffer *canvas,
                                       struct nk_rect r, float cr,
                                       struct nk_color c1, struct nk_color c2,
                                       float angle_deg) {
  float rad = angle_deg * 3.14159265f / 180.0f;
  float gx = cosf(rad), gy = sinf(rad);
  float hw = r.w * 0.5f, hh = r.h * 0.5f;
  float cx_r = r.x + hw, cy_r = r.y + hh;
  (void)cx_r; (void)cy_r;

  // Project all 4 corners onto gradient axis to find t range
  float p_tl = -hw * gx - hh * gy, p_tr = hw * gx - hh * gy;
  float p_br =  hw * gx + hh * gy, p_bl = -hw * gx + hh * gy;
  float lo = p_tl, hi = p_tl;
  if (p_tr < lo) lo = p_tr; if (p_tr > hi) hi = p_tr;
  if (p_br < lo) lo = p_br; if (p_br > hi) hi = p_br;
  if (p_bl < lo) lo = p_bl; if (p_bl > hi) hi = p_bl;
  float range = (hi - lo > 0.001f) ? (hi - lo) : 0.001f;

  nk_fill_rect_multi_color(canvas, r,
      lerp_color(c1, c2, (p_tl - lo) / range),  // left  = TL
      lerp_color(c1, c2, (p_tr - lo) / range),  // top   = TR
      lerp_color(c1, c2, (p_br - lo) / range),  // right = BR
      lerp_color(c1, c2, (p_bl - lo) / range)); // bottom= BL
  (void)cr;
}

// =============================================================================
// DESIGN SIDEBAR — Properties | Layers | Page
// =============================================================================

// ── Section header: gap + tinted label + hairline ───────────────────────────
static void ds_section(struct nk_context *ctx, const char *label) {
  nk_layout_row_dynamic(ctx, 8, 1);
  nk_spacing(ctx, 1);
  nk_layout_row_dynamic(ctx, 18, 1);
  struct nk_command_buffer *cb = nk_window_get_canvas(ctx);
  struct nk_rect lb = nk_widget_bounds(ctx);
  nk_fill_rect(cb, nk_rect(lb.x - 6, lb.y, lb.w + 12, lb.h), 0,
               nk_rgba(0, 0, 0, 10));
  nk_label_colored(ctx, label, NK_TEXT_LEFT, nk_rgb(95, 95, 115));
  nk_layout_row_dynamic(ctx, 1, 1);
  {
    struct nk_rect sr = nk_widget_bounds(ctx);
    nk_fill_rect(nk_window_get_canvas(ctx),
                 nk_rect(sr.x - 6, sr.y, sr.w + 12, 1), 0,
                 nk_rgba(0, 0, 0, 18));
    nk_spacing(ctx, 1);
  }
  nk_layout_row_dynamic(ctx, 4, 1);
  nk_spacing(ctx, 1);
}

// ── Compact color row: [label ........ ■ swatch_combo] ──────────────────────
static void ds_color(struct nk_context *ctx, const char *label, uint32_t *c32) {
  nk_layout_row_template_begin(ctx, 26);
  nk_layout_row_template_push_static(ctx, 60);
  nk_layout_row_template_push_dynamic(ctx);
  nk_layout_row_template_end(ctx);
  struct nk_color c = nk_rgba((*c32 >> 24) & 0xFF, (*c32 >> 16) & 0xFF,
                              (*c32 >> 8) & 0xFF, *c32 & 0xFF);
  if (nk_combo_begin_color(ctx, c, nk_vec2(220, 380))) {
    struct nk_colorf cf = nk_color_cf(c);
    cf = ui_render_color_picker_content(ctx, cf);
    nk_combo_end(ctx);
    c = nk_rgba((int)(cf.r * 255), (int)(cf.g * 255), (int)(cf.b * 255),
                (int)(cf.a * 255));
    *c32 = ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) |
           ((uint32_t)c.b << 8) | c.a;
  }
  nk_label(ctx, label, NK_TEXT_LEFT);
}

// =============================================================================
// DESIGN SMART LAYOUT — TABLEAU (live table top-left + chart below)
// =============================================================================
static void design_insert_tableau_layout(DesignData *dd) {
  if (!dd || !dd->root)
    return;

  float w = dd->width > 0 ? dd->width : 800.0f;
  float h = dd->height > 0 ? dd->height : 600.0f;

  float pad    = 20.0f;
  float col_w  = w * 0.48f;   // left half
  float tbl_x  = pad;
  float tbl_y  = pad;
  float tbl_h  = h * 0.50f;   // table takes top half

  // ── Live table view node ───────────────────────────────────────────────────
  Node *tv = node_create(dd->next_node_id++, NODE_TYPE_TABLE_VIEW, "Table");
  if (tv) {
    tv->x               = tbl_x;
    tv->y               = tbl_y;
    tv->width           = col_w;
    tv->height          = tbl_h;
    tv->visible         = 1;
    tv->opacity         = 1.0f;
    tv->linked_block_id = dd->linked_table_id;  // 0 = fallback to design's linked table
    tv->parent          = dd->root;

    Node *last = dd->root->first_child;
    if (!last) {
      dd->root->first_child = tv;
    } else {
      while (last->next_sibling) last = last->next_sibling;
      last->next_sibling = tv;
    }
  }

  // Table only — no chart
  (void)h;
}

#if 0
  // Old smart layouts removed — replaced by design_insert_tableau_layout above.
  if (0) {
    // Executive Scorecard
    Node *main_v =
        design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
    main_v->is_container = 1;
    main_v->layout_type = 1;
    main_v->layout_padding = 20;

    Node *top_h =
        design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, 150, color);
    top_h->is_container = 1;
    top_h->layout_type = 2;
    top_h->layout_padding = 20;
    top_h->parent = main_v;
    node_add_child(main_v, top_h);

    for (int i = 0; i < 4; i++) {
      Node *card = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0,
                                             (w - 60) / 4, 150, 0xE0E0E0FF);
      card->parent = top_h;
      node_add_child(top_h, card);
    }

    Node *bot_text = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, w,
                                               h - 170, 0xFFFFFFFF);
    bot_text->parent = main_v;
    node_add_child(main_v, bot_text);
    if (linked_tbl && linked_tbl->column_count > 0) {
      strncpy(bot_text->linked_col_name, linked_tbl->columns[0].name, 63);
    }

  } else if (layout_id == 1) {
    // CRM Card Grid (Approximated as V of H)
    Node *main_v =
        design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
    main_v->is_container = 1;
    main_v->layout_type = 1;
    main_v->layout_padding = 20;

    for (int r = 0; r < 3; r++) {
      Node *row_h = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w,
                                              (h - 40) / 3, color);
      row_h->is_container = 1;
      row_h->layout_type = 2;
      row_h->layout_padding = 20;
      row_h->parent = main_v;
      node_add_child(main_v, row_h);

      for (int c = 0; c < 3; c++) {
        Node *card = design_create_styled_node(
            dd, NODE_STYLE_RECT, 0, 0, (w - 40) / 3, (h - 40) / 3, 0xFFFFFFFF);
        card->has_shadow = 1;
        card->shadow_blur = 10;
        card->shadow_offset_y = 5;
        card->parent = row_h;
        node_add_child(row_h, card);
      }
    }
  } else if (layout_id == 2) {
    // Comparison Matrix
    Node *main_h =
        design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
    main_h->is_container = 1;
    main_h->layout_type = 2;
    main_h->layout_padding = 40;

    Node *left_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0,
                                             (w - 40) / 2, h, 0xFFFFFFFF);
    left_v->is_container = 1;
    left_v->layout_type = 1;
    left_v->layout_padding = 20;
    left_v->has_shadow = 1;
    left_v->parent = main_h;
    node_add_child(main_h, left_v);

    Node *right_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0,
                                              (w - 40) / 2, h, 0xFFFFFFFF);
    right_v->is_container = 1;
    right_v->layout_type = 1;
    right_v->layout_padding = 20;
    right_v->has_shadow = 1;
    right_v->parent = main_h;
    node_add_child(main_h, right_v);
  } else if (layout_id == 3) {
    // Project Timeline
    design_create_styled_node(dd, NODE_STYLE_LINE, 50, h / 2, w - 100, 4,
                              0x444444FF);

    for (int i = 0; i < 5; i++) {
      design_create_styled_node(dd, NODE_STYLE_ELLIPSE,
                                50 + i * ((w - 100) / 4) - 10, h / 2 - 10, 20,
                                20, 0x4285F4FF);
      Node *text = design_create_styled_node(
          dd, NODE_STYLE_TEXT, 50 + i * ((w - 100) / 4) - 50,
          (i % 2 == 0) ? h / 2 - 60 : h / 2 + 20, 100, 30, color);
      strncpy(text->text, "Milestone", 63);
    }
  } else if (layout_id == 4) {
    // Financial Split
    Node *main_h =
        design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
    main_h->is_container = 1;
    main_h->layout_type = 2;
    main_h->layout_padding = 40;

    Node *pie = design_create_styled_node(
        dd, NODE_STYLE_ELLIPSE, 0, 0, (w - 40) / 2, (w - 40) / 2, 0x4285F4FF);
    pie->parent = main_h;
    node_add_child(main_h, pie);

    Node *right_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0,
                                              (w - 40) / 2, h, 0xFFFFFFFF);
    right_v->is_container = 1;
    right_v->layout_type = 1;
    right_v->layout_padding = 20;
    right_v->has_shadow = 1;
    right_v->parent = main_h;
    node_add_child(main_h, right_v);

    for (int i = 0; i < 5; i++) {
      Node *row = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0,
                                            (w - 40) / 2, 40, 0xEEEEEEFF);
      row->parent = right_v;
      node_add_child(right_v, row);
    }
  } else if (layout_id == 5) {
    // Technical Spec Sheet
    Node *main_h =
        design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
    main_h->is_container = 1;
    main_h->layout_type = 2;
    main_h->layout_padding = 40;

    Node *img = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w * 0.4, h,
                                          0xD0D0D0FF);
    img->parent = main_h;
    node_add_child(main_h, img);

    Node *right_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0,
                                              w * 0.6 - 40, h, color);
    right_v->is_container = 1;
    right_v->layout_type = 1;
    right_v->layout_padding = 20;
    right_v->parent = main_h;
    node_add_child(main_h, right_v);

    Node *title = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0,
                                            w * 0.6 - 40, 60, color);
    strncpy(title->text, "Product Title", 63);
    title->parent = right_v;
    node_add_child(right_v, title);

    Node *desc = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0,
                                           w * 0.6 - 40, h - 80, color);
    strncpy(desc->text, "Product Specifications...", 63);
    desc->parent = right_v;
    node_add_child(right_v, desc);
  } else if (layout_id == 6) {
    // Kanban Light
    Node *main_h =
        design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
    main_h->is_container = 1;
    main_h->layout_type = 2;
    main_h->layout_padding = 20;

    for (int c = 0; c < 3; c++) {
      Node *col_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0,
                                              (w - 40) / 3, h, 0xF5F5F5FF);
      col_v->is_container = 1;
      col_v->layout_type = 1;
      col_v->layout_padding = 10;
      col_v->parent = main_h;
      node_add_child(main_h, col_v);

      Node *title = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0,
                                              (w - 40) / 3, 30, color);
      strncpy(title->text, c == 0 ? "To Do" : (c == 1 ? "Doing" : "Done"), 63);
      title->parent = col_v;
      node_add_child(col_v, title);

      for (int r = 0; r < 3; r++) {
        Node *card = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0,
                                               (w - 40) / 3, 80, 0xFFFFFFFF);
        card->has_shadow = 1;
        card->parent = col_v;
        node_add_child(col_v, card);
      }
    }
  } else if (layout_id == 7) {
    // Feedback Feed
    Node *main_v = design_create_styled_node(dd, NODE_STYLE_RECT, w * 0.2, 0,
                                             w * 0.6, h, color);
    main_v->is_container = 1;
    main_v->layout_type = 1;
    main_v->layout_padding = 20;

    for (int r = 0; r < 5; r++) {
      Node *card = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w * 0.6,
                                             100, 0xFFFFFFFF);
      card->has_shadow = 1;
      card->parent = main_v;
      node_add_child(main_v, card);
    }
  } else if (layout_id == 8) {
    // Sales Funnel
    Node *main_v =
        design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
    main_v->is_container = 1;
    main_v->layout_type = 1;
    main_v->layout_padding = 10;

    for (int r = 0; r < 4; r++) {
      float rw = w - (r * w * 0.2);
      Node *step = design_create_styled_node(dd, NODE_STYLE_RECT, (w - rw) / 2,
                                             0, rw, (h - 30) / 4, 0x4285F4FF);
      step->parent = main_v;
      node_add_child(main_v, step);
    }
  } else if (layout_id == 9) {
    // Clean Page (Export Ready)
    Node *main_v = design_create_styled_node(dd, NODE_STYLE_RECT, 40, 40,
                                             w - 80, h - 80, color);
    main_v->is_container = 1;
    main_v->layout_type = 1;
    main_v->layout_padding = 20;

    Node *header =
        design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, w - 80, 60, color);
    strncpy(header->text, "Official Report Header", 63);
    header->parent = main_v;
    node_add_child(main_v, header);

    Node *content = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w - 80,
                                              h - 200, 0xFFFFFFFF);
    content->parent = main_v;
    node_add_child(main_v, content);

    Node *footer =
        design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, w - 80, 40, color);
    strncpy(footer->text, "Confidential - Aslan Export", 63);
    footer->parent = main_v;
    node_add_child(main_v, footer);
  }
}
#endif // old layouts disabled
// ── Main sidebar ─────────────────────────────────────────────────────────────
static void design_render_sidebar(UIHost *ui, Engine *engine,
                                  struct nk_context *ctx, Block *block) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;
  Page *page = doc_page_get_active(engine->doc);

  // ── Tab bar: Properties | Layers | Page ─────────────────────────────────
  nk_layout_row_dynamic(ctx, 30, 3);
  static const char *tab_labels[] = {"Properties", "Layers", "Page"};
  for (int t = 0; t < 3; t++) {
    struct nk_style_button ts = ctx->style.button;
    if (ui->design_bottom_tab == t) {
      ts.normal =
          nk_style_item_color(nk_rgba(0, 0, 0, 15)); // Subtle grey active tab
      ts.hover = nk_style_item_color(nk_rgba(0, 0, 0, 20));
      ts.active = nk_style_item_color(nk_rgba(0, 0, 0, 25));
      ts.text_normal = ts.text_hover = ts.text_active = COLOR_ACCENT;
    } else {
      ts.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      ts.hover = nk_style_item_color(nk_rgba(0, 0, 0, 10));
      ts.active = nk_style_item_color(nk_rgba(0, 0, 0, 20));
      ts.text_normal = COLOR_ACCENT;
      ts.text_hover = nk_rgb(60, 60, 80);
      ts.text_active = nk_rgb(40, 40, 60);
      ts.border = 0;
    }
    if (nk_button_label_styled(ctx, &ts, tab_labels[t]))
      ui->design_bottom_tab = t;
  }

  // Thin hairline separator
  nk_layout_row_dynamic(ctx, 1, 1);
  {
    struct nk_rect sr = nk_widget_bounds(ctx);
    nk_fill_rect(nk_window_get_canvas(ctx),
                 nk_rect(sr.x - 6, sr.y, sr.w + 12, 1), 0,
                 nk_rgba(0, 0, 0, 25));
    nk_spacing(ctx, 1);
  }

  // Scrollable content: fill remaining height.
  // Use -50 (not -36) to budget for Nuklear's inter-row spacing so the
  // bottom inspector group never overflows and its inputs get clipped.
  float sb_avail = nk_window_get_content_region(ctx).h - 50.0f;
  if (sb_avail < 50.0f)
    sb_avail = 50.0f;

  // =========================================================================
  // TAB 0 — PROPERTIES
  // =========================================================================
  char gp_props[64], gp_layers[64], gp_page[64];
  snprintf(gp_props, sizeof(gp_props), "ds_props_%u", block->id);
  snprintf(gp_layers, sizeof(gp_layers), "ds_layers_%u", block->id);
  snprintf(gp_page, sizeof(gp_page), "ds_page_%u", block->id);

  if (ui->design_bottom_tab == 0) {
    nk_layout_row_dynamic(ctx, sb_avail, 1);
    if (nk_group_begin(ctx, gp_props, NK_WINDOW_BORDER)) {
      if (!ui->selected_node) {
        nk_layout_row_dynamic(ctx, 50, 1);
        nk_label_colored(ctx, "No element selected", NK_TEXT_CENTERED,
                         nk_rgb(160, 160, 175));
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label_colored(ctx, "Click an element to edit", NK_TEXT_CENTERED,
                         nk_rgb(195, 195, 210));
      } else {
        Node *node = ui->selected_node;

        // ── TABLE VIEW: size/position only, content is read-only ─────────
        if (node->type == NODE_TYPE_TABLE_VIEW) {
          ds_section(ctx, "TABLE VIEW");
          nk_layout_row_dynamic(ctx, 24, 2);
          nk_property_float(ctx, "X:", -9999.f, &node->x, 9999.f, 1.f, 0.5f);
          nk_property_float(ctx, "Y:", -9999.f, &node->y, 9999.f, 1.f, 0.5f);
          nk_property_float(ctx, "W:", 10.f, &node->width,  9999.f, 1.f, 0.5f);
          nk_property_float(ctx, "H:", 10.f, &node->height, 9999.f, 1.f, 0.5f);
          nk_layout_row_dynamic(ctx, 18, 1);
          nk_label_colored(ctx, "Content mirrors the linked table",
                           NK_TEXT_LEFT, nk_rgb(160, 160, 175));
          goto props_end;
        }

        // ── CHART: type + data binding ───────────────────────────────────
        if (node->type == NODE_TYPE_CHART && node->chart_data) {
          ChartData *cd = node->chart_data;
          Page *cp = doc_page_get_active(engine->doc);

          ds_section(ctx, "CHART TYPE");
          static const char *ct_labels[] = {"Bar", "Line", "Pie", "Gantt"};
          nk_layout_row_dynamic(ctx, 28, 4);
          for (int _ct = 0; _ct < 4; _ct++) {
            struct nk_style_button bs = ctx->style.button;
            if ((int)cd->type == _ct) {
              bs.normal = nk_style_item_color(COLOR_ACCENT);
              bs.hover  = nk_style_item_color(COLOR_ACCENT);
              bs.text_normal = nk_rgb(255, 255, 255);
            }
            if (nk_button_label_styled(ctx, &bs, ct_labels[_ct]))
              cd->type = (ChartType)_ct;
          }

          if (cp) {
            // Source table picker
            ds_section(ctx, "DATA SOURCE");
            char src_lbl[64] = "None";
            Block *cur_tbl_blk = NULL;
            for (Block *b = cp->first_block; b; b = b->next) {
              if (b->type == BLOCK_TABLE && b->id == cd->source_table_id) {
                cur_tbl_blk = b;
                snprintf(src_lbl, sizeof(src_lbl), "Table #%u", b->id);
                break;
              }
            }
            nk_layout_row_dynamic(ctx, 24, 1);
            if (nk_button_label(ctx, src_lbl)) {
              // Cycle to next table block
              Block *first_t = NULL, *next_t = NULL;
              int found = 0;
              for (Block *b = cp->first_block; b; b = b->next) {
                if (b->type != BLOCK_TABLE || !b->data.table.columnar) continue;
                if (!first_t) first_t = b;
                if (found && !next_t) { next_t = b; break; }
                if (b->id == cd->source_table_id) found = 1;
              }
              if (!next_t) next_t = first_t;
              if (next_t) cd->source_table_id = next_t->id;
            }

            // Column pickers (Label + Value)
            AslanTable *ct = cur_tbl_blk ? cur_tbl_blk->data.table.columnar : NULL;
            if (ct && ct->column_count > 0) {
              ds_section(ctx, "COLUMNS");
              nk_layout_row_dynamic(ctx, 24, 2);
              // Label column
              char lbl_c[48] = "Label: —";
              if (cd->label_col_idx >= 0 && cd->label_col_idx < (int)ct->column_count)
                snprintf(lbl_c, sizeof(lbl_c), "L: %.10s",
                         ct->columns[cd->label_col_idx].name);
              if (nk_button_label(ctx, lbl_c))
                cd->label_col_idx = (cd->label_col_idx + 1) % (int)ct->column_count;
              // Value column (cycle only NUMBER columns)
              char val_c[48] = "Value: —";
              if (cd->value_col_idx >= 0 && cd->value_col_idx < (int)ct->column_count)
                snprintf(val_c, sizeof(val_c), "V: %.10s",
                         ct->columns[cd->value_col_idx].name);
              if (nk_button_label(ctx, val_c)) {
                int start = cd->value_col_idx;
                for (int _a = 0; _a < (int)ct->column_count; _a++) {
                  int cand = (start + 1 + _a) % (int)ct->column_count;
                  if (ct->columns[cand].type == ASLAN_COL_NUMBER) {
                    cd->value_col_idx = cand; break;
                  }
                }
              }
            }
          }
          goto props_end;
        }

        // ── PEN / MARKER STROKES: only color ─────────────────────────────
        if (node->type == NODE_TYPE_STROKE || node->type == NODE_TYPE_PATH) {
          ds_section(ctx, "STROKE");
          ds_color(ctx, "Color", &node->stroke_color);
          goto props_end;
        }

        // ── APPEARANCE ───────────────────────────────────────────────────
        ds_section(ctx, "APPEARANCE");
        nk_layout_row_dynamic(ctx, 24, 1);
        nk_checkbox_label(ctx, "Lock Ratio", &node->lock_ratio);

        // Shape type selection
        const char *shape_labels[] = {"Rectangle", "Ellipse", "Triangle",
                                      "Pentagon",  "Hexagon", "Octagon",
                                      "Line"};
        NodeType shape_types[] = {
            NODE_TYPE_RECT,     NODE_TYPE_ELLIPSE, NODE_TYPE_TRIANGLE,
            NODE_TYPE_PENTAGON, NODE_TYPE_HEXAGON, NODE_TYPE_OCTAGON,
            NODE_TYPE_LINE};
        int cur_shape = -1;
        for (int i = 0; i < 7; i++) {
          if (node->type == shape_types[i]) {
            cur_shape = i;
            break;
          }
        }
        nk_layout_row_dynamic(ctx, 24, 1);
        if (cur_shape < 0 || cur_shape >= 7)
          cur_shape = 0;
        int new_shape =
            nk_combo(ctx, shape_labels, 7, cur_shape, 24, nk_vec2(220, 220));
        if (new_shape != cur_shape && new_shape >= 0) {
          node->type = shape_types[new_shape];
        }

        ds_color(ctx, "Fill", &node->fill_color);
        ds_color(ctx, "Stroke", &node->stroke_color);
        nk_layout_row_dynamic(ctx, 24, 1);
        nk_property_float(ctx, "Stroke width:", 0.f, &node->stroke_width, 100.f,
                          0.5f, 0.25f);

        if (node->type != NODE_TYPE_TEXT && node->type != NODE_TYPE_LINE) {
          nk_layout_row_dynamic(ctx, 24, 1);
          nk_property_float(ctx, "Corner radius:", 0.f, &node->corner_radius,
                            200.f, 1.f, 0.5f);
        }

        // Gradient Options
        nk_layout_row_dynamic(ctx, 24, 2);
        const char *fill_types[] = {"Solid", "Gradient"};
        if (node->fill_type < 0 || node->fill_type >= 2)
          node->fill_type = 0;
        node->fill_type = nk_combo(ctx, fill_types, 2, node->fill_type, 24,
                                   nk_vec2(220, 200));

        if (node->fill_type == 1) {
          ds_color(ctx, "Gradient To", &node->fill_color_2);
          nk_layout_row_dynamic(ctx, 24, 1);
          nk_property_float(ctx, "Angle:", 0.f, &node->gradient_angle, 360.f,
                            1.f, 0.5f);
        }

        nk_layout_row_dynamic(ctx, 24, 1);
        nk_property_float(ctx, "Opacity %:", 0.f, &node->opacity, 1.0f, 0.01f,
                          0.005f);

        // ── PATH EDITING ─────────────────────────────────────────────────
        {
          DesignInteraction *ds_p = &ui->design_state;
          int is_path = (node->type == NODE_TYPE_PATH);
          ds_section(ctx, "PATH EDITING");
          nk_layout_row_dynamic(ctx, 28, 1);
          struct nk_style_button bs_p = ctx->style.button;
          if (ds_p->bezier_mode) {
            bs_p.normal = nk_style_item_color(COLOR_ACCENT);
            bs_p.hover  = nk_style_item_color(COLOR_ACCENT);
            bs_p.text_normal = nk_rgb(255, 255, 255);
            bs_p.text_hover  = nk_rgb(255, 255, 255);
          }
          const char *btn_lbl = is_path ? "Edit Path" : "Convert to Path";
          if (nk_button_label_styled(ctx, &bs_p, btn_lbl)) {
            if (!is_path) {
              // Convert the selected node to path then enable bezier mode
              recursive_convert_to_path(node);
              ds_p->bezier_mode = 1;
            } else {
              ds_p->bezier_mode = !ds_p->bezier_mode;
            }
          }
        }

        // ── CONTAINER ────────────────────────────────────────────────────
        if (node->type != NODE_TYPE_TEXT || node->font_type != 4) {
          ds_section(ctx, "CONTAINER");
          nk_layout_row_dynamic(ctx, 24, 1);
          nk_checkbox_label(ctx, "Is Container", &node->is_container);
          if (node->is_container) {
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_checkbox_label(ctx, "Clip Content", &node->clip_content);

            const char *layout_modes[] = {"None", "V-Stack", "H-Stack"};
            nk_layout_row_dynamic(ctx, 24, 2);
            nk_label(ctx, "Auto Layout:", NK_TEXT_LEFT);
            if (node->layout_type < 0 || node->layout_type >= 3)
              node->layout_type = 0;
            node->layout_type = nk_combo(
                ctx, layout_modes, 3, node->layout_type, 24, nk_vec2(150, 200));

            if (node->layout_type > 0) {
              nk_layout_row_dynamic(ctx, 24, 1);
              nk_property_float(ctx, "Spacing:", 0.f, &node->layout_padding,
                                200.f, 1.f, 0.5f);
            }
          }
        }

        // ── IMAGE CROP ───────────────────────────────────────────────────
        if (node->type == NODE_TYPE_IMAGE && node->image_path[0]) {
          ds_section(ctx, "IMAGE");
          DesignInteraction *ds_img = &ui->design_state;
          int is_crop_active = ds_img->is_cropping &&
                               ui->selected_node == node;
          nk_layout_row_template_begin(ctx, 28);
          nk_layout_row_template_push_static(ctx, 32.0f); // crop icon btn
          nk_layout_row_template_push_dynamic(ctx);       // scale
          nk_layout_row_template_end(ctx);

          // Crop toggle button
          {
            struct nk_style_button cb = ctx->style.button;
            cb.rounding = 4.0f;
            if (is_crop_active) {
              cb.normal = cb.hover = cb.active =
                  nk_style_item_color(COLOR_ACCENT);
              cb.text_normal = cb.text_hover = cb.text_active =
                  nk_rgb(255, 255, 255);
            } else {
              cb.normal = nk_style_item_color(nk_rgba(0,0,0,0));
              cb.hover  = nk_style_item_color(nk_rgba(0,0,0,12));
              cb.text_normal = cb.text_hover = cb.text_active = COLOR_ACCENT;
              cb.border_color = COLOR_ACCENT;
            }
            if (ui->font_icons)
              nk_style_set_font(ctx, &ui->font_icons->handle);
            if (nk_button_label_styled(ctx, &cb, "\xEE\xB0\x82")) {
              if (is_crop_active) {
                ds_img->is_cropping = 0;
              } else {
                ds_img->is_cropping = 1;
                ds_img->crop_start_x = 0;
                ds_img->crop_start_y = 0;
                ds_img->crop_start_node_x = node->image_crop_x;
                ds_img->crop_start_node_y = node->image_crop_y;
              }
            }
            if (ui->font_inter)
              nk_style_set_font(ctx, &ui->font_inter->handle);
          }

          nk_property_float(ctx, "Scale:", 0.1f, &node->image_scale, 10.0f,
                            0.1f, 0.05f);
        }

        // ── LAYERS ───────────────────────────────────────────────────────
        if (node->type != NODE_TYPE_TEXT || node->font_type != 4) {
          ds_section(ctx, "ORDER");
          nk_layout_row_dynamic(ctx, 24, 4);
          if (nk_button_label(ctx, "Front"))
            node_bring_to_front(node);
          if (nk_button_label(ctx, "Back"))
            node_send_to_back(node);
          if (nk_button_label(ctx, "Fwd"))
            node_move_up(node);
          if (nk_button_label(ctx, "Bwd"))
            node_move_down(node);
        }
        // ── TEXT CONTENT ─────────────────────────────────────────────────
        if (node->type == NODE_TYPE_TEXT) {
          ds_section(ctx, "TEXT CONTENT");
          nk_layout_row_dynamic(ctx, 100, 1);
          nk_edit_string_zero_terminated(
              ctx, NK_EDIT_BOX | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD,
              node->text, sizeof(node->text), nk_filter_default);

          nk_layout_row_dynamic(ctx, 24, 1);
          nk_property_float(ctx, "Font size:", 6.f, &node->font_size, 200.f,
                            1.f, 0.5f);
          nk_layout_row_dynamic(ctx, 24, 2);
          const char *font_types[] = {"Inter", "Inter Bold", "Courier", "Serif",
                                      "Icons"};
          int cur_font = node->font_type;
          if (cur_font < 0 || cur_font >= 5)
            cur_font = 0;
          int new_font =
              nk_combo(ctx, font_types, 5, cur_font, 24, nk_vec2(150, 200));
          if (new_font != cur_font) {
            node->font_type = new_font;
          }
          // Alignment
          const char *aligns[] = {"Left", "Center", "Right"};
          int cur_align = node->text_align;
          if (cur_align < 0 || cur_align >= 3)
            cur_align = 0;
          int new_align =
              nk_combo(ctx, aligns, 3, cur_align, 24, nk_vec2(100, 100));
          if (new_align != cur_align) {
            node->text_align = new_align;
          }

          nk_layout_row_dynamic(ctx, 24, 2);
          nk_property_float(ctx, "Line height:", 0.5f, &node->line_height, 3.0f,
                            0.1f, 0.05f);
          nk_property_float(ctx, "Letter spacing:", -10.0f,
                            &node->letter_spacing, 50.0f, 0.5f, 0.1f);

          // Link to table and column
          if (node->type == NODE_TYPE_TEXT && node->font_type != 4) {
            ds_section(ctx, "LINKED TEXT");
          } else if (node->type == NODE_TYPE_IMAGE) {
            ds_section(ctx, "LINKED IMAGE");
          }

          static char text_tnames[32][64];
          uint32_t text_tids[32];
          const char *text_tnames_ptr[32];
          int text_tcount = 0;
          text_tnames_ptr[text_tcount] = "None";
          text_tids[text_tcount] = 0;
          text_tcount++;

          Block *tbl_block = NULL;
          if (engine && engine->doc) {
            for (int i = 0; i < 32; i++) {
              Page *p = &engine->doc->pages[i];
              if (!p->in_use)
                continue;
              for (Block *b = p->first_block; b; b = b->next) {
                if (b->type == BLOCK_TABLE && b->data.table.columnar) {
                  if (text_tcount < 32) {
                    text_tids[text_tcount] = b->id;
                    snprintf(text_tnames[text_tcount], 64, "Table %u", b->id);
                    text_tnames_ptr[text_tcount] = text_tnames[text_tcount];
                    text_tcount++;
                  }
                  if (b->id == dd->linked_table_id) {
                    tbl_block = b;
                  }
                }
              }
            }
          }

          nk_layout_row_dynamic(ctx, 20, 1);
          nk_label(ctx, "Source Table:", NK_TEXT_LEFT);
          nk_layout_row_dynamic(ctx, 24, 1);

          int cur_tidx = 0;
          for (int i = 0; i < text_tcount; i++) {
            if (text_tids[i] == dd->linked_table_id)
              cur_tidx = i;
          }
          if (cur_tidx < 0 || cur_tidx >= text_tcount)
            cur_tidx = 0;
          int new_tidx = nk_combo(ctx, text_tnames_ptr, text_tcount, cur_tidx,
                                  24, nk_vec2(220, 200));
          if (new_tidx != cur_tidx) {
            dd->linked_table_id = text_tids[new_tidx];
            ui->design_linked_row_idx = 0;
            node->linked_col_name[0] = '\0'; // reset column selection

            tbl_block = NULL;
            if (dd->linked_table_id && engine && engine->doc) {
              for (int i = 0; i < 32; i++) {
                Page *p = &engine->doc->pages[i];
                if (!p->in_use)
                  continue;
                Block *tb = page_find_block_by_id(p, dd->linked_table_id);
                if (tb && tb->type == BLOCK_TABLE && tb->data.table.columnar) {
                  tbl_block = tb;
                  break;
                }
              }
            }
          }

          if (tbl_block) {
            AslanTable *tbl = tbl_block->data.table.columnar;
            int col_count = (int)tbl->column_count;
            if (col_count > 32)
              col_count = 32;
            const char *col_names[33];
            col_names[0] = "None";
            for (int k = 0; k < col_count; k++)
              col_names[k + 1] = tbl->columns[k].name;
            int cur_sel = 0;
            char *active_col_name = (node->type == NODE_TYPE_IMAGE)
                                        ? node->linked_image_col_name
                                        : node->linked_col_name;
            for (int k = 0; k < col_count; k++) {
              if (strcmp(active_col_name, tbl->columns[k].name) == 0) {
                cur_sel = k + 1;
                break;
              }
            }
            nk_layout_row_dynamic(ctx, 4, 1);
            nk_spacing(ctx, 1);
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "Data Column:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 24, 1);
            if (cur_sel < 0 || cur_sel >= col_count + 1)
              cur_sel = 0;
            int new_sel = nk_combo(ctx, col_names, col_count + 1, cur_sel, 24,
                                   nk_vec2(220, 200));
            if (new_sel != cur_sel) {
              if (new_sel == 0) {
                active_col_name[0] = '\0';
              } else {
                strncpy(active_col_name, col_names[new_sel], 63);
                active_col_name[63] = '\0';
              }
            }
            if (active_col_name[0]) {
              int total_rows = (int)tbl->active_count;
              if (ui->design_linked_row_idx >= total_rows && total_rows > 0)
                ui->design_linked_row_idx = total_rows - 1;
              nk_layout_row_template_begin(ctx, 26);
              nk_layout_row_template_push_static(ctx, 26);
              nk_layout_row_template_push_dynamic(ctx);
              nk_layout_row_template_push_static(ctx, 26);
              nk_layout_row_template_end(ctx);
              if (nk_button_label(ctx, "<") && ui->design_linked_row_idx > 0)
                ui->design_linked_row_idx--;
              char rl[32];
              snprintf(rl, sizeof(rl), "Row %d / %d",
                       ui->design_linked_row_idx + 1, total_rows);
              nk_label(ctx, rl, NK_TEXT_CENTERED);
              if (nk_button_label(ctx, ">") &&
                  ui->design_linked_row_idx < total_rows - 1)
                ui->design_linked_row_idx++;
            }
          } else {
            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label_colored(ctx, "Link a table in the 'Page' tab",
                             NK_TEXT_CENTERED, nk_rgb(160, 160, 175));
            nk_label_colored(ctx, "to use data columns here.", NK_TEXT_CENTERED,
                             nk_rgb(160, 160, 175));
          }
        }
      }
      props_end:;
      nk_group_end(ctx);
    }
  }

  // =========================================================================
  // TAB 1 — LAYERS  (toolbar | scrollable tree | fixed inspector)
  // =========================================================================
  if (ui->design_bottom_tab == 1) {
    // Inspector height: base rows
    // (name+opacity+fill/stroke+type-specific+buttons): ~148px
    float inspector_h = ui->selected_node ? 148.0f : 0.0f;
    float tree_h = sb_avail - inspector_h -
                   (ui->selected_node ? 4.0f : 0.0f); // Nuklear row spacing
    if (tree_h < 40.0f)
      tree_h = 40.0f;

    // ── Layer tree (scrollable)
    // ───────────────────────────────────────────
    nk_layout_row_dynamic(ctx, tree_h, 1);
    if (nk_group_begin(ctx, gp_layers, 0)) {
      ui_render_sidebar_layers(ui, ctx, block);
      nk_group_end(ctx);
    }

    // ── Fixed bottom inspector (only when a node is selected) ────────────
    if (ui->selected_node) {
      char gp_insp[64];
      snprintf(gp_insp, sizeof(gp_insp), "ds_insp_%u", block->id);
      nk_layout_row_dynamic(ctx, inspector_h, 1);
      if (nk_group_begin(ctx, gp_insp, 0)) {
        ui_render_layer_inspector(ui, ctx, block);
        nk_group_end(ctx);
      }
    }
  }

  // =========================================================================
  // TAB 2 — PAGE
  // =========================================================================
  if (ui->design_bottom_tab == 2) {
    nk_layout_row_dynamic(ctx, sb_avail, 1);
    if (nk_group_begin(ctx, gp_page, NK_WINDOW_BORDER)) {
      if (page) {
        // ── CANVAS SIZE ──────────────────────────────────────────────────
        ds_section(ctx, "CANVAS SIZE");
        {
          int no_limits = dd->no_limits;
          nk_layout_row_dynamic(ctx, 24, 1);
          if (nk_checkbox_label(ctx, "Infinite canvas (No Limits)",
                                &no_limits) &&
              no_limits != dd->no_limits) {
            dd->no_limits = no_limits;
            if (dd->no_limits && dd->zoom < 0.01f)
              dd->zoom = 1.0f;
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "block_design_nolimits %u %u %d",
                     page->id, block->id, dd->no_limits);
            engine_execute_command(engine, cmd);
          }

          if (!dd->no_limits) {
            static const struct {
              const char *l;
              int w, h;
            } presets[] = {
                {"A4", 794, 1123},    {"A3", 1123, 1587},
                {"HD", 1920, 1080},   {"4K", 3840, 2160},
                {"Sq.", 1080, 1080},  {"Story", 1080, 1920},
                {"Post", 1080, 1350}, {"Banner", 1920, 480},
            };
            nk_layout_row_dynamic(ctx, 22, 4);
            for (int i = 0; i < 8; i++) {
              if (nk_button_label(ctx, presets[i].l)) {
                char cmd[128];
                snprintf(cmd, sizeof(cmd), "block_design_resize %u %u %d %d 0",
                         page->id, block->id, presets[i].w, presets[i].h);
                engine_execute_command(engine, cmd);
              }
            }
            nk_layout_row_dynamic(ctx, 4, 1); // Spacing after presets
            nk_spacing(ctx, 1);

            // Unit Selection
            static const char *unit_names[] = {"PX", "CM", "IN"};
            nk_layout_row_dynamic(ctx, 24, 2);
            nk_label(ctx, "Units:", NK_TEXT_LEFT);
            if (dd->units < 0 || dd->units >= 3)
              dd->units = 0;
            dd->units =
                nk_combo(ctx, unit_names, 3, dd->units, 24, nk_vec2(80, 100));

            float to_px = 1.0f;
            if (dd->units == 1)
              to_px = 37.795f;
            else if (dd->units == 2)
              to_px = 96.0f;

            float val_w = (float)dd->width / to_px;
            float val_h = (float)dd->height / to_px;

            nk_layout_row_dynamic(ctx, 24, 2);
            nk_property_float(ctx, "W:", 1.0f, &val_w, 5000.f, 1.f, 0.1f);
            nk_property_float(ctx, "H:", 1.0f, &val_h, 5000.f, 1.f, 0.1f);

            int new_w = (int)(val_w * to_px);
            int new_h = (int)(val_h * to_px);

            if (new_w != dd->width || new_h != dd->height) {
              char cmd[128];
              snprintf(cmd, sizeof(cmd), "block_design_resize %u %u %d %d 0",
                       page->id, block->id, new_w, new_h);
              engine_execute_command(engine, cmd);
            }
          }
        }

        // ── LAYOUT AIDS ──────────────────────────────────────────────────
        ds_section(ctx, "LAYOUT AIDS");
        {
          DesignPage *dp = &dd->pages[dd->current_page];

          nk_spacing(ctx, 1);
          nk_layout_row_dynamic(ctx, 22, 1);
          nk_checkbox_label(ctx, "Show Rulers", &dp->show_rulers);

          nk_spacing(ctx, 1);
          nk_layout_row_dynamic(ctx, 22, 1);
          nk_label(ctx, "Guides:", NK_TEXT_LEFT);
          nk_layout_row_dynamic(ctx, 22, 2);
          if (nk_button_label(ctx, "+ H Guide") && dp->h_guide_count < 16) {
            dp->h_guides[dp->h_guide_count++] = (float)dd->height / 2.0f;
          }
          if (nk_button_label(ctx, "+ V Guide") && dp->v_guide_count < 16) {
            dp->v_guides[dp->v_guide_count++] = (float)dd->width / 2.0f;
          }
          for (int gi = 0; gi < dp->h_guide_count; gi++) {
            nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 3);
            nk_layout_row_push(ctx, 0.25f);
            nk_labelf(ctx, NK_TEXT_LEFT, "H%d", gi + 1);
            nk_layout_row_push(ctx, 0.50f);
            nk_property_float(ctx, "#", 0, &dp->h_guides[gi], (float)dd->height,
                              1, 1);
            nk_layout_row_push(ctx, 0.25f);
            if (nk_button_label(ctx, "X")) {
              for (int k = gi; k < dp->h_guide_count - 1; k++)
                dp->h_guides[k] = dp->h_guides[k + 1];
              dp->h_guide_count--;
              gi--;
            }
            nk_layout_row_end(ctx);
          }
          for (int gi = 0; gi < dp->v_guide_count; gi++) {
            nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 3);
            nk_layout_row_push(ctx, 0.25f);
            nk_labelf(ctx, NK_TEXT_LEFT, "V%d", gi + 1);
            nk_layout_row_push(ctx, 0.50f);
            nk_property_float(ctx, "#", 0, &dp->v_guides[gi], (float)dd->width,
                              1, 1);
            nk_layout_row_push(ctx, 0.25f);
            if (nk_button_label(ctx, "X")) {
              for (int k = gi; k < dp->v_guide_count - 1; k++)
                dp->v_guides[k] = dp->v_guides[k + 1];
              dp->v_guide_count--;
              gi--;
            }
            nk_layout_row_end(ctx);
          }
        }

        // ── EXPORT ───────────────────────────────────────────────────────
        ds_section(ctx, "EXPORT");
        nk_layout_row_dynamic(ctx, 24, 4);
        if (nk_button_label(ctx, "SVG")) {
          // Check for linked table manually find first available
          AslanTable *linked_tbl = NULL;
          for (int i = 0; i < engine->doc->page_count; i++) {
            Page *p = &engine->doc->pages[i];
            if (!p->in_use)
              continue;
            for (Block *b = p->first_block; b; b = b->next) {
              if (b->type == BLOCK_TABLE && b->data.table.columnar) {
                linked_tbl = b->data.table.columnar;
                break;
              }
            }
            if (linked_tbl)
              break;
          }
          if (linked_tbl) {
            ui_host_export_svg_merge(ui, engine, linked_tbl);
          } else {
            ui_host_export_svg(ui, engine);
          }
        }
        if (nk_button_label(ctx, "PNG")) {
          ui_host_export_png_jpeg(ui, engine, 0);
        }
        if (nk_button_label(ctx, "JPEG")) {
          ui_host_export_png_jpeg(ui, engine, 1);
        }
        if (nk_button_label(ctx, "PDF")) {
          ui_host_export_pdf(ui, engine);
        }
      }
      nk_group_end(ctx);
    }
  }
}

void ui_render_node_tree(UIHost *ui, struct nk_context *ctx, Engine *engine,
                         Block *block, DesignData *dd,
                         struct nk_command_buffer *canvas_buf, Node *node,
                         struct nk_rect bounds, float scale, float parent_x,
                         float parent_y) {
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
  struct nk_rect nr = nk_rect(sx, sy, sw, sh);

  // Hover detection (Canvas -> Sidebar)
  if (nk_input_is_mouse_hovering_rect(&ctx->input, nr) &&
      !ui->design_state.is_dragging_node && !ui->sidebar_drag_node_id) {
    ui->hovered_node_id = node->id;
  }

  // Highlight if hovered (Sidebar -> Canvas or Canvas -> Sidebar)
  if (ui->hovered_node_id == node->id) {
    struct nk_color highlight_col = nk_rgba(110, 110, 130, 255);
    float rad_h = node->rotation * 3.14159f / 180.0f;
    float cx_h = sx + sw / 2.0f;
    float cy_h = sy + sh / 2.0f;
    if (node->type == NODE_TYPE_ELLIPSE) {
      nk_stroke_circle(canvas_buf,
                       nk_rect(sx - 2, sy - 2, sw + 4, sh + 4),
                       2.0f, highlight_col);
    } else if (node->type == NODE_TYPE_RECT || node->type == NODE_TYPE_TEXT ||
               node->type == NODE_TYPE_IMAGE || node->type == NODE_TYPE_LINE) {
      fill_rotated_rounded_rect(canvas_buf, cx_h, cy_h, sw + 2, sh + 2, rad_h,
                                node->corner_radius * scale, highlight_col, 1,
                                2.0f);
    } else {
      // Polygon shapes (triangle, pentagon, hexagon, octagon): rect outline
      // is acceptable as bounding box indicator
      fill_rotated_rounded_rect(canvas_buf, cx_h, cy_h, sw + 2, sh + 2, rad_h,
                                0, highlight_col, 1, 2.0f);
    }
  }

  if (node->is_container && node->clip_content) {
    nk_push_scissor(canvas_buf, nr);
  }

  // --- Inject Shadow ---
  uint32_t f32 = node->fill_color;
  uint32_t s32 = node->stroke_color;
  float opacity = node->opacity;
  if (node->has_shadow && node->type != NODE_TYPE_LINE &&
      node->type != NODE_TYPE_STROKE && node->type != NODE_TYPE_CHART) {
    struct nk_color sh_col = nk_rgba(
        (node->shadow_color >> 24) & 0xFF, (node->shadow_color >> 16) & 0xFF,
        (node->shadow_color >> 8) & 0xFF,
        (int)((node->shadow_color & 0xFF) * opacity));
    float sh_x = nr.x + node->shadow_offset_x * scale;
    float sh_y = nr.y + node->shadow_offset_y * scale;
    struct nk_rect sr = nk_rect(sh_x, sh_y, nr.w, nr.h);
    float b = node->shadow_blur * scale;
    if (b > 0) {
      struct nk_rect srb =
          nk_rect(sh_x - b / 2, sh_y - b / 2, nr.w + b, nr.h + b);
      fill_smooth_rounded_rect(canvas_buf, srb, (node->corner_radius + b / 2) * scale,
                               nk_rgba(sh_col.r, sh_col.g, sh_col.b, sh_col.a / 2));
    }
    fill_smooth_rounded_rect(canvas_buf, sr, node->corner_radius * scale, sh_col);
  }
  struct nk_color fill =
      nk_rgba((f32 >> 24) & 0xFF, (f32 >> 16) & 0xFF, (f32 >> 8) & 0xFF,
              (int)((f32 & 0xFF) * opacity));
  struct nk_color stroke =
      nk_rgba((s32 >> 24) & 0xFF, (s32 >> 16) & 0xFF, (s32 >> 8) & 0xFF,
              (int)((s32 & 0xFF) * opacity));

  int drawn_image = 0;
  int has_fill = (node->fill_color & 0xFF) > 10;

  // Container with image
  if (node->is_container && node->image_path[0]) {
    SDL_Texture *tex = get_image_texture(ui, node->image_path);
    if (tex) {
      struct nk_image img = nk_image_ptr(tex);
      nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
      drawn_image = 1;
    }
  }

  if (node->type == NODE_TYPE_GROUP) {
    // Group: no background fill — just a faint dashed border so the
    // designer can see its bounds.  Children are rendered below in the
    // normal loop.
    nk_stroke_rect(canvas_buf, nr, 0, 1.0f, nk_rgba(120, 120, 180, 60));
  } else if (node->type == NODE_TYPE_CHART && node->chart_data) {
    Page *curr_page = doc_page_get_active(engine->doc);
    ui_render_chart_node(ui, ctx, node, nr, curr_page);

    // Rendering configuration bar if selected
    if (ui->selected_node == node) {
      ui_render_chart_node_config_bar(ui, engine, ctx, node, nr, curr_page);
    }
  } else if (node->type == NODE_TYPE_TABLE_VIEW) {
    // ── Embedded live-table (read-only) ──────────────────────────────────
    // Find the linked table block
    AslanTable *tv_tbl = NULL;
    for (int _pi = 0; _pi < engine->doc->page_count && !tv_tbl; _pi++) {
      Page *_p = &engine->doc->pages[_pi];
      if (!_p->in_use) continue;
      for (Block *_b = _p->first_block; _b && !tv_tbl; _b = _b->next) {
        if (_b->type == BLOCK_TABLE && _b->data.table.columnar &&
            (_b->id == node->linked_block_id ||
             (node->linked_block_id == 0 && _b->id == dd->linked_table_id)))
          tv_tbl = _b->data.table.columnar;
      }
    }

    // White card background
    nk_fill_rect(canvas_buf, nr, 4.0f * scale, nk_rgba(255, 255, 255, 255));
    nk_stroke_rect(canvas_buf, nr, 4.0f * scale, 1.0f, nk_rgba(220, 220, 230, 255));

    if (tv_tbl && tv_tbl->column_count > 0) {
      int   ncols  = tv_tbl->column_count;
      int   nrows  = (int)aslan_table_view_count(tv_tbl);
      float hdr_h  = 26.0f * scale;
      float row_h  = 22.0f * scale;
      float col_w  = nr.w / (float)ncols;
      const struct nk_user_font *fnt = ctx->style.font;

      // ── Header ──────────────────────────────────────────────────────────
      for (int c = 0; c < ncols; c++) {
        struct nk_rect hcell = nk_rect(nr.x + c * col_w, nr.y, col_w, hdr_h);
        // Clip to node bounds
        if (hcell.x + hcell.w > nr.x + nr.w)
          hcell.w = nr.x + nr.w - hcell.x;
        if (hcell.w <= 0) continue;

        nk_fill_rect(canvas_buf, hcell, 0, nk_rgba(26, 115, 232, 255));
        // Separator
        if (c > 0)
          nk_stroke_line(canvas_buf, hcell.x, hcell.y, hcell.x,
                         hcell.y + hdr_h, 1.0f, nk_rgba(255, 255, 255, 60));
        // Column name
        struct nk_rect txt_r = nk_rect(hcell.x + 4, hcell.y + (hdr_h - fnt->height) / 2.0f,
                                       hcell.w - 8, fnt->height);
        const char *col_nm = tv_tbl->columns[c].name;
        nk_draw_text(canvas_buf, txt_r, col_nm, (int)strlen(col_nm),
                     fnt, nk_rgba(0, 0, 0, 0), nk_rgba(255, 255, 255, 255));
      }

      // ── Data rows ───────────────────────────────────────────────────────
      int max_vis = (int)((nr.h - hdr_h) / row_h) + 1;
      int draw_rows = nrows < max_vis ? nrows : max_vis;

      for (int r = 0; r < draw_rows; r++) {
        float ry = nr.y + hdr_h + r * row_h;
        if (ry >= nr.y + nr.h) break;

        uint64_t row_id = aslan_table_view_row_id(tv_tbl, r);
        struct nk_color row_bg = (r % 2 == 0) ? nk_rgba(248, 249, 250, 255)
                                               : nk_rgba(255, 255, 255, 255);

        for (int c = 0; c < ncols; c++) {
          struct nk_rect cell = nk_rect(nr.x + c * col_w, ry, col_w, row_h);
          if (cell.x + cell.w > nr.x + nr.w)
            cell.w = nr.x + nr.w - cell.x;
          if (cell.w <= 0 || cell.y + cell.h > nr.y + nr.h + 2) continue;

          nk_fill_rect(canvas_buf, cell, 0, row_bg);
          // Horizontal line
          nk_stroke_line(canvas_buf, cell.x, cell.y + cell.h - 1,
                         cell.x + cell.w, cell.y + cell.h - 1,
                         1.0f, nk_rgba(230, 230, 235, 255));
          // Vertical separator
          if (c > 0)
            nk_stroke_line(canvas_buf, cell.x, cell.y, cell.x,
                           cell.y + cell.h, 1.0f, nk_rgba(220, 220, 230, 255));

          // Cell value
          char _cbuf[128];
          const char *val = aslan_table_get_cell_display(tv_tbl, c, row_id,
                                                         _cbuf, sizeof(_cbuf));
          if (val && val[0]) {
            struct nk_rect txt_r = nk_rect(cell.x + 4,
                                           cell.y + (cell.h - fnt->height) / 2.0f,
                                           cell.w - 8, fnt->height);
            nk_draw_text(canvas_buf, txt_r, val, (int)strlen(val),
                         fnt, nk_rgba(0, 0, 0, 0), nk_rgba(40, 40, 50, 255));
          }
        }
      }
    } else {
      // No table linked — placeholder
      struct nk_rect ph = nk_rect(nr.x + 8, nr.y + nr.h / 2.0f - 8, nr.w - 16, 16);
      const char *msg = "Link a table in the Page tab";
      nk_draw_text(canvas_buf, ph, msg, (int)strlen(msg),
                   ctx->style.font, nk_rgba(0, 0, 0, 0), nk_rgba(160, 160, 175, 255));
    }
  } else if (node->type == NODE_TYPE_STROKE && node->stroke_data) {
    Stroke *s = node->stroke_data;
    if (s->count >= 2) {
      struct nk_color st_col =
          nk_rgba((s->color >> 24) & 0xFF, (s->color >> 16) & 0xFF,
                  (s->color >> 8) & 0xFF, s->color & 0xFF);

      struct nk_vec2 *pts =
          (struct nk_vec2 *)malloc(s->count * sizeof(struct nk_vec2));
      if (pts) {
        for (int k = 0; k < s->count; k++) {
          pts[k].x = bounds.x + s->points[k].x * scale;
          pts[k].y = bounds.y + s->points[k].y * scale;
        }
        nk_stroke_polyline(canvas_buf, (float *)pts, s->count,
                           s->thickness * scale, st_col);

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
              nk_fill_circle(
                  canvas_buf,
                  nk_rect(start_x - size / 2, start_y - size / 2, size, size),
                  st_col);
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
              nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                               st_col);
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
              nk_fill_circle(
                  canvas_buf,
                  nk_rect(end_x - size / 2, end_y - size / 2, size, size),
                  st_col);
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
              nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                               st_col);
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
      SDL_Texture *tex = get_image_texture(ui, img_path_to_use);
      if (tex) {
        // Image Cropping implementation
        struct nk_image img = nk_image_ptr(tex);

        // Use Scissor to clip image to node bounds
        nk_push_scissor(canvas_buf, nr);

        // Calculate transformed image rect
        float img_w = nr.w * node->image_scale;
        float img_h = nr.h * node->image_scale;
        float img_x = nr.x + node->image_crop_x * scale;
        float img_y = nr.y + node->image_crop_y * scale;
        struct nk_rect ir = nk_rect(img_x, img_y, img_w, img_h);

        nk_draw_image(canvas_buf, ir, &img, nk_rgb(255, 255, 255));

        // Pop scissor back to artboard clip (infinite in free mode)
        nk_push_scissor(canvas_buf, ui->artboard_clip);

        drawn = 1;
      }
    }
    if (!drawn) {
      nk_fill_rect(canvas_buf, nr, 0, nk_rgb(200, 200, 200));
    }
    if (node->stroke_width > 0)
      stroke_smooth_rounded_rect(canvas_buf, nr, node->corner_radius * scale,
                                 node->stroke_width * scale, stroke);
  } else if (node->type == NODE_TYPE_RECT) {
    if (node->is_container && !node->image_path[0]) {
      struct nk_color cst = nk_rgba(0, 0, 0, 25); // Subtle grey border
      stroke_smooth_rounded_rect(canvas_buf, nr, node->corner_radius * scale, 1.0f, cst);
      if (ui->font_icons) {
        struct nk_rect ir =
            nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
        nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                     &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
      }
      drawn_image = 1;
    }
    if (!drawn_image && has_fill) {
      if (node->rotation != 0.0f && node->type == NODE_TYPE_RECT) {
        fill_rotated_rounded_rect(canvas_buf, nr.x + nr.w / 2, nr.y + nr.h / 2,
                                  nr.w, nr.h,
                                  node->rotation * 3.14159f / 180.0f,
                                  node->corner_radius * scale, fill, 0, 0);
      } else {

        if (node->fill_type == 1) { // Gradient
          struct nk_color fill2 =
              nk_rgba((node->fill_color_2 >> 24) & 0xFF,
                      (node->fill_color_2 >> 16) & 0xFF,
                      (node->fill_color_2 >> 8) & 0xFF,
                      (int)((node->fill_color_2 & 0xFF) * opacity));
          fill_gradient_rounded_rect(canvas_buf, nr, 0,
                                     fill, fill2, node->gradient_angle);
        } else {
          fill_smooth_rounded_rect(canvas_buf, nr, node->corner_radius * scale, fill);
        }
      }
    }
    if (node->stroke_width > 0) {
      if (node->rotation != 0.0f && node->type == NODE_TYPE_RECT) {
        fill_rotated_rounded_rect(
            canvas_buf, nr.x + nr.w / 2, nr.y + nr.h / 2, nr.w, nr.h,
            node->rotation * 3.14159f / 180.0f, node->corner_radius * scale,
            stroke, 1, node->stroke_width * scale);
      } else {
        stroke_smooth_rounded_rect(canvas_buf, nr, node->corner_radius * scale,
                                   node->stroke_width, stroke);
      }
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
        nk_fill_circle(
            canvas_buf,
            nk_rect(start_x - size / 2, start_y - size / 2, size, size),
            stroke);
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

        nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y, stroke);
      }

      // Render End Style
      if (node->line_end_style == 1) { // Circle
        nk_fill_circle(canvas_buf,
                       nk_rect(end_x - size / 2, end_y - size / 2, size, size),
                       stroke);
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

        nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y, stroke);
      }
    }
  } else if (node->type == NODE_TYPE_LINE) {
    // Render explicit LINE type as a horizontal stroke through the
    // center of its bounds
    float line_y = nr.y + nr.h / 2.0f;
    float line_w = (node->stroke_width > 0 ? node->stroke_width : 2.0f) * scale;
    struct nk_color line_col = stroke;

    // Draw the main line
    nk_stroke_line(canvas_buf, nr.x, line_y, nr.x + nr.w, line_y, line_w,
                   line_col);

    // Line Endings
    float size = (line_w > 2.0f ? line_w : 2.0f) * 4.0f;

    // Start Style (Left)
    if (node->line_start_style > 0) {
      if (node->line_start_style == 1) { // Circle
        nk_fill_circle(canvas_buf,
                       nk_rect(nr.x - size / 2, line_y - size / 2, size, size),
                       line_col);
      } else if (node->line_start_style == 2) { // Arrow (Left <--)
        nk_fill_triangle(canvas_buf, nr.x, line_y, nr.x + size,
                         line_y - size / 2, nr.x + size, line_y + size / 2,
                         line_col);
      }
    }

    // End Style (Right)
    if (node->line_end_style > 0) {
      if (node->line_end_style == 1) { // Circle
        nk_fill_circle(
            canvas_buf,
            nk_rect(nr.x + nr.w - size / 2, line_y - size / 2, size, size),
            line_col);
      } else if (node->line_end_style == 2) { // Arrow (Right -->)
        nk_fill_triangle(canvas_buf, nr.x + nr.w, line_y, nr.x + nr.w - size,
                         line_y - size / 2, nr.x + nr.w - size,
                         line_y + size / 2, line_col);
      }
    }

  } else if (node->type == NODE_TYPE_PATH && node->path_points &&
             node->path_point_count > 1) {
    if (node->is_container && !node->image_path[0]) {
      // Empty container placeholder for PATH shape
      struct nk_color cbg = nk_rgba(110, 110, 130, 40);
      struct nk_color cst = nk_rgba(110, 110, 130, 255);
      float poly_pts[512 * 2];
      int poly_count =
          node_get_path_polygon(node, poly_pts, 512, sx, sy, scale);
      if (poly_count > 2) {
        nk_fill_polygon(canvas_buf, poly_pts, poly_count, cbg);
        nk_stroke_polygon(canvas_buf, poly_pts, poly_count, 1.0f, cst);
      }
      if (ui->font_icons) {
        struct nk_rect ir =
            nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
        nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                     &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
      }
      drawn_image = 1;
    }

    // Fill path
    if (!drawn_image && fill.a > 0) {
      float poly_pts[512 * 2];
      int poly_count =
          node_get_path_polygon(node, poly_pts, 512, sx, sy, scale);
      if (poly_count > 2) {
        nk_fill_polygon(canvas_buf, poly_pts, poly_count, fill);
      }
    }

    // Stroke path
    for (int k = 0; k < node->path_point_count; k++) {
      int nxt = (k + 1) % node->path_point_count;
      NodePathPoint p1 = node->path_points[k], p2 = node->path_points[nxt];
      if (p1.type == 1) // Cubic Bezier
        nk_stroke_curve(canvas_buf, sx + p1.x * scale, sy + p1.y * scale,
                        sx + p1.cp2x * scale, sy + p1.cp2y * scale,
                        sx + p2.cp1x * scale, sy + p2.cp1y * scale,
                        sx + p2.x * scale, sy + p2.y * scale,
                        node->stroke_width * scale, stroke);
      else
        nk_stroke_line(canvas_buf, sx + p1.x * scale, sy + p1.y * scale,
                       sx + p2.x * scale, sy + p2.y * scale,
                       node->stroke_width * scale, stroke);
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
        nk_fill_circle(
            canvas_buf,
            nk_rect(start_x - size / 2, start_y - size / 2, size, size),
            stroke);
      } else if (node->line_start_style ==
                 2) { // Arrow (pointing away from line)
        // Tip at start_x, start_y. Pointing away means angle + PI.
        float arrow_angle = angle + 3.14159f;
        float tip_x = start_x; // Tip is exactly at start point
        float tip_y = start_y;

        // Move tip slightly out so it doesn't overlap line end exactly?
        // No, exact is fine. Actually, if we want the arrow tip to be
        // at start_x, we compute base from there.

        float base_angle1 = arrow_angle + 0.5f; // ~28 deg
        float base_angle2 = arrow_angle - 0.5f;

        float p1x = tip_x + size * cosf(base_angle1);
        float p1y = tip_y + size * sinf(base_angle1);
        float p2x = tip_x + size * cosf(base_angle2);
        float p2y = tip_y + size * sinf(base_angle2);

        nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y, stroke);
      }

      // End Style
      if (node->line_end_style == 1) { // Circle
        nk_fill_circle(canvas_buf,
                       nk_rect(end_x - size / 2, end_y - size / 2, size, size),
                       stroke);
      } else if (node->line_end_style == 2) { // Arrow
        float tip_x = end_x;
        float tip_y = end_y;

        float base_angle1 = angle + 3.14159f + 0.5f;
        float base_angle2 = angle + 3.14159f - 0.5f;

        float p1x = tip_x + size * cosf(base_angle1);
        float p1y = tip_y + size * sinf(base_angle1);
        float p2x = tip_x + size * cosf(base_angle2);
        float p2y = tip_y + size * sinf(base_angle2);

        nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y, stroke);
      }
    }
  } else if (node->type == NODE_TYPE_ELLIPSE) {
    if (node->is_container && !node->image_path[0]) {
      struct nk_color cbg = nk_rgba(110, 110, 130, 40);
      struct nk_color cst = nk_rgba(110, 110, 130, 255);
      nk_fill_circle(canvas_buf, nr, cbg);
      nk_stroke_circle(canvas_buf, nr, 1.0f, cst);
      if (ui->font_icons) {
        struct nk_rect ir =
            nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
        nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                     &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
      }
      drawn_image = 1;
    }
    if (node->rotation != 0.0f) {
      float rad = node->rotation * 3.14159f / 180.0f;
      float c = cosf(rad);
      float s = sinf(rad);
      float cx = nr.x + nr.w / 2.0f;
      float cy = nr.y + nr.h / 2.0f;
      float hw = nr.w / 2.0f;
      float hh = nr.h / 2.0f;
      float pts[128];
      for (int i = 0; i < 64; i++) {
        float th = i * 2.0f * 3.14159f / 64.0f;
        float ex = cosf(th) * hw;
        float ey = sinf(th) * hh;
        pts[i * 2] = cx + ex * c - ey * s;
        pts[i * 2 + 1] = cy + ex * s + ey * c;
      }
      if (!drawn_image && has_fill)
        nk_fill_polygon(canvas_buf, pts, 64, fill);
      if (node->stroke_width > 0)
        draw_thick_polygon_stroke(canvas_buf, pts, 64, node->stroke_width,
                                  stroke);
    } else {
      if (node->stroke_width > 0 && has_fill) {
        // Ring technique: outer filled circle (stroke color), inner (fill)
        float hsw = node->stroke_width / 2.0f;
        struct nk_rect outer =
            nk_rect(nr.x - hsw, nr.y - hsw, nr.w + node->stroke_width,
                    nr.h + node->stroke_width);
        nk_fill_circle(canvas_buf, outer, stroke);
        if (!drawn_image)
          nk_fill_circle(canvas_buf, nr, fill);
      } else {
        if (!drawn_image && has_fill)
          nk_fill_circle(canvas_buf, nr, fill);
        if (node->stroke_width > 0) {
          // No fill: generate 64-pt polygon and use thick stroke
          float ecx = nr.x + nr.w / 2.0f, ecy = nr.y + nr.h / 2.0f;
          float ehw = nr.w / 2.0f, ehh = nr.h / 2.0f;
          float epts[128];
          for (int i = 0; i < 64; i++) {
            float th = i * 2.0f * 3.14159f / 64.0f;
            epts[i * 2] = ecx + cosf(th) * ehw;
            epts[i * 2 + 1] = ecy + sinf(th) * ehh;
          }
          draw_thick_polygon_stroke(canvas_buf, epts, 64, node->stroke_width,
                                    stroke);
        }
      }
    }
  } else if (node->type == NODE_TYPE_TRIANGLE) {
    if (node->is_container && !node->image_path[0]) {
      struct nk_color cbg = nk_rgba(110, 110, 130, 40);
      struct nk_color cst = nk_rgba(110, 110, 130, 255);
      float vpts[6] = {sx + sw / 2, sy, sx, sy + sh, sx + sw, sy + sh};
      nk_fill_polygon(canvas_buf, vpts, 3, cbg);
      nk_stroke_polygon(canvas_buf, vpts, 3, 1.0f, cst);
      if (ui->font_icons) {
        struct nk_rect ir =
            nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 5, 20, 20);
        nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                     &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
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
    struct nk_vec2 pts[3] = {{cx + p0x * c - p0y * s, cy + p0x * s + p0y * c},
                             {cx + p1x * c - p1y * s, cy + p1x * s + p1y * c},
                             {cx + p2x * c - p2y * s, cy + p2x * s + p2y * c}};
    if (!drawn_image && has_fill)
      fill_rounded_polygon(canvas_buf, pts, 3, node->corner_radius * scale,
                           fill, 0, 0);
    if (node->stroke_width > 0)
      draw_thick_polygon_stroke_v(canvas_buf, pts, 3, node->stroke_width,
                                  stroke);
  } else if (node->type == NODE_TYPE_PENTAGON) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    struct nk_vec2 pts[5];
    for (int i = 0; i < 5; i++) {
      float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
      float px = cosf(angle) * sw / 2.0f;
      float py = sinf(angle) * sh / 2.0f;
      pts[i].x = cx + px * c_rot - py * s_rot;
      pts[i].y = cy + px * s_rot + py * c_rot;
    }
    if (!drawn_image && has_fill)
      fill_rounded_polygon(canvas_buf, pts, 5, node->corner_radius * scale,
                           fill, 0, 0);
    if (node->stroke_width > 0)
      draw_thick_polygon_stroke_v(canvas_buf, pts, 5, node->stroke_width,
                                  stroke);
  } else if (node->type == NODE_TYPE_HEXAGON) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    struct nk_vec2 pts[6];
    for (int i = 0; i < 6; i++) {
      float angle = (float)i * 2.0f * 3.14159f / 6.0f;
      float px = cosf(angle) * sw / 2.0f;
      float py = sinf(angle) * sh / 2.0f;
      pts[i].x = cx + px * c_rot - py * s_rot;
      pts[i].y = cy + px * s_rot + py * c_rot;
    }
    if (!drawn_image && has_fill)
      fill_rounded_polygon(canvas_buf, pts, 6, node->corner_radius * scale,
                           fill, 0, 0);
    if (node->stroke_width > 0)
      draw_thick_polygon_stroke_v(canvas_buf, pts, 6, node->stroke_width,
                                  stroke);
  } else if (node->type == NODE_TYPE_OCTAGON) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    struct nk_vec2 pts[8];
    for (int i = 0; i < 8; i++) {
      float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
      float px = cosf(angle) * sw / 2.0f;
      float py = sinf(angle) * sh / 2.0f;
      pts[i].x = cx + px * c_rot - py * s_rot;
      pts[i].y = cy + px * s_rot + py * c_rot;
    }
    if (!drawn_image && has_fill)
      fill_rounded_polygon(canvas_buf, pts, 8, node->corner_radius * scale,
                           fill, 0, 0);
    if (node->stroke_width > 0)
      draw_thick_polygon_stroke_v(canvas_buf, pts, 8, node->stroke_width,
                                  stroke);
  } else if (node->type == NODE_TYPE_STAR) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    struct nk_vec2 pts[10];
    for (int i = 0; i < 10; i++) {
      float angle = (float)i * 2.0f * 3.14159f / 10.0f - 3.14159f / 2.0f;
      float r = (i % 2 == 0) ? (sw / 2.0f) : (sw / 4.0f);
      float px = cosf(angle) * r;
      float py = sinf(angle) * r;
      pts[i].x = cx + px * c_rot - py * s_rot;
      pts[i].y = cy + px * s_rot + py * c_rot;
    }
    if (!drawn_image && has_fill)
      fill_rounded_polygon(canvas_buf, pts, 10, node->corner_radius * scale,
                           fill, 0, 0);
    if (node->stroke_width > 0)
      fill_rounded_polygon(canvas_buf, pts, 10, node->corner_radius * scale,
                           stroke, 1, node->stroke_width);
  } else if (node->type == NODE_TYPE_HEART) {
    float cx = sx + sw / 2.0f;
    float cy = sy + sh / 2.0f;
    float rad = node->rotation * 3.14159f / 180.0f;
    float c_rot = cosf(rad), s_rot = sinf(rad);
    const int hcount = 32;
    struct nk_vec2 hpts[32];
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
    if (!drawn_image && has_fill)
      nk_fill_polygon(canvas_buf, (const float *)hpts, hcount, fill);
    if (node->stroke_width > 0)
      nk_stroke_polygon(canvas_buf, (const float *)hpts, hcount,
                        node->stroke_width, stroke);
  } else if (node->type == NODE_TYPE_TEXT) {
    // Text node

    // Render background for sticky notes / text with bg
    if (fill.a > 0) {
      if (node->fill_type == 1) { // Gradient
        struct nk_color fill2 = nk_rgba(
            (node->fill_color_2 >> 24) & 0xFF,
            (node->fill_color_2 >> 16) & 0xFF, (node->fill_color_2 >> 8) & 0xFF,
            (int)((node->fill_color_2 & 0xFF) * opacity));
        fill_gradient_rounded_rect(canvas_buf, nr, 0,
                                   fill, fill2, node->gradient_angle);
      } else {
        fill_smooth_rounded_rect(canvas_buf, nr, node->corner_radius * scale, fill);
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
      nk_fill_rect(canvas_buf, nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2),
                   2.0f, nk_rgba(110, 110, 130, 20));
      nk_stroke_rect(canvas_buf,
                     nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2), 2.0f,
                     1.0f, COLOR_ACCENT);
    }
    const struct nk_user_font *unscaled_font =
        ui->font_inter ? &ui->font_inter->handle : ctx->style.font;
    const struct nk_user_font *font = unscaled_font;

    // Select font based on type and size
    if (node->font_type == 4 && ui->font_icons) {
      unscaled_font = &ui->font_icons->handle;
      font = &ui->font_icons->handle;
    } else { // Inter variants (Body, H1, H2, H3)
      // Find closest size in font_inter_sizes
      float target_size = node->font_size;
      float scaled_size = node->font_size * scale;
      float available_sizes[] = {12.0f, 14.0f, 16.0f, 20.0f, 24.0f,
                                 32.0f, 48.0f, 64.0f, 96.0f};

      int best_idx = -1;
      float best_diff = 1000.0f;
      for (int i = 0; i < 9; i++) {
        if (fabsf(available_sizes[i] - target_size) < best_diff) {
          best_diff = fabsf(available_sizes[i] - target_size);
          best_idx = i;
        }
      }
      if (best_idx >= 0 && ui->font_inter_sizes[best_idx]) {
        unscaled_font = &ui->font_inter_sizes[best_idx]->handle;
      } else if (ui->font_inter) {
        unscaled_font = &ui->font_inter->handle;
      }

      int scaled_best_idx = -1;
      float scaled_best_diff = 1000.0f;
      for (int i = 0; i < 9; i++) {
        if (fabsf(available_sizes[i] - scaled_size) < scaled_best_diff) {
          scaled_best_diff = fabsf(available_sizes[i] - scaled_size);
          scaled_best_idx = i;
        }
      }
      if (scaled_best_idx >= 0 && ui->font_inter_sizes[scaled_best_idx]) {
        font = &ui->font_inter_sizes[scaled_best_idx]->handle;
      } else if (ui->font_inter) {
        font = &ui->font_inter->handle;
      }
    }

    // Auto-resize text node to fit content
    if (display_text && strlen(display_text) > 0) {
      // Calculate multi-line bounds using unscaled font!
      char *text_copy = strdup(display_text);
      float max_w = 0;
      int lines = 0;
      if (text_copy) {
        char *p = text_copy;
        char *start = p;
        while (*p) {
          if (*p == '\n') {
            *p = '\0';
            float lw = unscaled_font->width(unscaled_font->userdata,
                                            unscaled_font->height, start,
                                            strlen(start));
            if (lw > max_w)
              max_w = lw;
            lines++;
            start = p + 1;
          }
          p++;
        }
        if (start < p) {
          float lw =
              unscaled_font->width(unscaled_font->userdata,
                                   unscaled_font->height, start, strlen(start));
          if (lw > max_w)
            max_w = lw;
          lines++;
        }
        free(text_copy);
      }

      float text_w = max_w;
      float text_h = unscaled_font->height * lines * node->line_height;
      if (lines == 0) {
        text_w = 10;
        text_h = 24.0f;
      }

      if (fabsf(node->width - (text_w + 10.0f)) > 1.0f ||
          fabsf(node->height - (text_h + 10.0f)) > 1.0f) {
        node->width = text_w + 10.0f; // Add padding
        node->height = text_h + 10.0f;
      }
    }

    struct nk_color txt_color = {
        (node->stroke_color >> 24) & 0xFF, (node->stroke_color >> 16) & 0xFF,
        (node->stroke_color >> 8) & 0xFF, node->stroke_color & 0xFF};

    // --- Advanced Text Rendering ---
    // 1. Split text into lines
    char *text_copy = strdup(display_text);
    if (text_copy) {
      char *lines[32];
      int line_count = 0;
      char *p = text_copy;
      lines[line_count++] = p;
      while (*p && line_count < 32) {
        if (*p == '\n') {
          *p = '\0';
          lines[line_count++] = p + 1;
        }
        p++;
      }

      float cursor_y = nr.y;
      float actual_line_height = font->height * node->line_height;

      for (int i = 0; i < line_count; i++) {
        char *line = lines[i];
        int len = strlen(line);
        if (len == 0) {
          cursor_y += actual_line_height;
          continue;
        }

        // Calculate line width with letter spacing
        float line_w = 0;
        for (int c = 0; c < len; c++) {
          line_w += font->width(font->userdata, font->height, &line[c], 1);
          if (c < len - 1)
            line_w += node->letter_spacing * scale;
        }

        // Alignment
        float cursor_x = nr.x;
        if (node->text_align == 1) { // Center
          cursor_x = nr.x + (nr.w - line_w) / 2.0f;
        } else if (node->text_align == 2) { // Right
          cursor_x = nr.x + nr.w - line_w;
        }

        // Draw char by char to apply letter spacing
        for (int c = 0; c < len; c++) {
          float char_w = font->width(font->userdata, font->height, &line[c], 1);
          nk_draw_text(canvas_buf,
                       nk_rect(cursor_x, cursor_y, char_w + 5, font->height),
                       &line[c], 1, font, nk_rgba(0, 0, 0, 0), txt_color);
          cursor_x += char_w + (node->letter_spacing * scale);
        }

        cursor_y += actual_line_height;
      }

      // Auto-resize
      if (!is_editing) {
        float max_w = 0;
        for (int i = 0; i < line_count; i++) {
          float lw = 0;
          int l = strlen(lines[i]);
          for (int c = 0; c < l; c++)
            lw += font->width(font->userdata, font->height, &lines[i][c], 1) +
                  (c < l - 1 ? node->letter_spacing * scale : 0);
          if (lw > max_w)
            max_w = lw;
        }
        float total_h = line_count * actual_line_height;
        float target_w = (max_w / scale) + 10.0f;
        float target_h = (total_h / scale) + 10.0f;
        if (fabsf(node->width - target_w) > 2.0f ||
            fabsf(node->height - target_h) > 2.0f) {
          node->width = target_w;
          node->height = target_h;
        }
      }

      free(text_copy);
    }

    if (is_editing) {
      ui->editing_node_rect = nk_rect(nr.x, nr.y, nr.w, nr.h);
      // We do not render the nk_edit here because nk_layout_space is not active
      // in this group traverse. Instead, we pass the absolute screen bounds
      // out, and render the text input overlay after the group.
    }
  }

  // Container indicator (Add icon in the center if empty container)
  if (node->is_container && !node->image_path[0] &&
      node->type != NODE_TYPE_TEXT && node->type != NODE_TYPE_ROOT) {
    struct nk_color cst = nk_rgba(110, 110, 130, 255);
    if (ui->font_icons) {
      struct nk_rect ir =
          nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
      nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                   &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
    }
  }

  // Render children
  for (Node *child = node->first_child; child; child = child->next_sibling) {
    ui_render_node_tree(ui, ctx, engine, block, dd, canvas_buf, child, bounds,
                        scale, sx, sy);
  }

  if (node->is_container && node->clip_content) {
    nk_push_scissor(canvas_buf, ui->artboard_clip);
  }
}

// =============================================================================
// GRID, GUIDES & RULERS RENDERING
// =============================================================================

static void design_draw_grid(struct nk_command_buffer *buf, struct nk_rect art,
                             float scale, const DesignPage *dp, float pan_x,
                             float pan_y, int no_limits) {
  if (!dp)
    return;

  struct nk_color grid_col = nk_rgba(120, 120, 120, 50);
  struct nk_color dot_col = nk_rgba(80, 82, 86, 255);
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
        nk_fill_circle(buf, nk_rect(x - 1, y - 1, 2.0f, 2.0f), dot_col);
      }
    }
    return;
  }

  if (dp->grid_type == 0)
    return;

  switch (dp->grid_type) {
  case 1: { // Dots — cap at 50x50 = 2500 circles
    int dc = max_cols > 50 ? 50 : max_cols;
    int dr = max_rows > 50 ? 50 : max_rows;
    for (int iy = 0; iy < dr; iy++) {
      float sy = art.y + iy * step;
      for (int ix = 0; ix < dc; ix++) {
        float sx = art.x + ix * step;
        nk_fill_circle(buf, nk_rect(sx - 1, sy - 1, 2, 2), dot_col);
      }
    }
    break;
  }
  case 2: { // Lines
    for (int ix = 0; ix < max_cols; ix++) {
      float sx = art.x + ix * step;
      nk_stroke_line(buf, sx, art.y, sx, art.y + art.h, 0.5f, grid_col);
    }
    for (int iy = 0; iy < max_rows; iy++) {
      float sy = art.y + iy * step;
      nk_stroke_line(buf, art.x, sy, art.x + art.w, sy, 0.5f, grid_col);
    }
    break;
  }
  case 3: { // 12 Columns
    float col_w = art.w / 12.0f;
    struct nk_color col_fill = nk_rgba(100, 140, 220, 20);
    struct nk_color col_stroke = nk_rgba(100, 140, 220, 40);
    for (int i = 0; i < 12; i++) {
      float cx = art.x + i * col_w;
      struct nk_rect cr = nk_rect(cx, art.y, col_w, art.h);
      nk_fill_rect(buf, cr, 0, col_fill);
      nk_stroke_rect(buf, cr, 0, 0.5f, col_stroke);
    }
    break;
  }
  case 4: { // Isometric — cap total lines
    int iso_max = max_cols + max_rows;
    if (iso_max > 100)
      iso_max = 100;
    for (int i = 0; i < iso_max; i++) {
      float sx = art.x + i * step;
      nk_stroke_line(buf, sx, art.y, sx - art.h, art.y + art.h, 0.5f, grid_col);
      nk_stroke_line(buf, sx - art.w, art.y, sx, art.y + art.h, 0.5f, grid_col);
    }
    break;
  }
  case 5: { // Custom — same as Lines
    for (int ix = 0; ix < max_cols; ix++) {
      float sx = art.x + ix * step;
      nk_stroke_line(buf, sx, art.y, sx, art.y + art.h, 0.5f, grid_col);
    }
    for (int iy = 0; iy < max_rows; iy++) {
      float sy = art.y + iy * step;
      nk_stroke_line(buf, art.x, sy, art.x + art.w, sy, 0.5f, grid_col);
    }
    break;
  }
  default:
    break;
  }
}

static void design_draw_guides(struct nk_command_buffer *buf,
                               struct nk_rect art, float scale,
                               const DesignPage *dp) {
  if (!dp)
    return;
  struct nk_color guide_col = nk_rgba(110, 110, 130, 180);

  // Horizontal guides
  for (int i = 0; i < dp->h_guide_count; i++) {
    float sy = art.y + dp->h_guides[i] * scale;
    if (sy >= art.y && sy <= art.y + art.h) {
      nk_stroke_line(buf, art.x, sy, art.x + art.w, sy, 1.0f, guide_col);
      // Dashed effect: draw label
      char lbl[16];
      snprintf(lbl, sizeof(lbl), "%.0f", dp->h_guides[i]);
      struct nk_user_font *fnt = (struct nk_user_font *)buf->userdata.ptr;
      if (fnt) {
        float tw =
            fnt->width(fnt->userdata, fnt->height, lbl, (int)strlen(lbl));
        struct nk_rect lr = nk_rect(art.x + 2, sy - 12, tw + 4, 12);
        nk_fill_rect(buf, lr, 0, nk_rgba(110, 110, 130, 200));
        nk_draw_text(buf, lr, lbl, (int)strlen(lbl), fnt, nk_rgba(0, 0, 0, 0),
                     nk_rgb(255, 255, 255));
      }
    }
  }

  // Vertical guides
  for (int i = 0; i < dp->v_guide_count; i++) {
    float sx = art.x + dp->v_guides[i] * scale;
    if (sx >= art.x && sx <= art.x + art.w) {
      nk_stroke_line(buf, sx, art.y, sx, art.y + art.h, 1.0f, guide_col);
      char lbl[16];
      snprintf(lbl, sizeof(lbl), "%.0f", dp->v_guides[i]);
      struct nk_user_font *fnt = (struct nk_user_font *)buf->userdata.ptr;
      if (fnt) {
        struct nk_rect lr = nk_rect(sx + 2, art.y + 2, 30, 12);
        nk_fill_rect(buf, lr, 0, nk_rgba(110, 110, 130, 200));
        nk_draw_text(buf, lr, lbl, (int)strlen(lbl), fnt, nk_rgba(0, 0, 0, 0),
                     nk_rgb(255, 255, 255));
      }
    }
  }
}

static void design_draw_rulers(struct nk_command_buffer *buf,
                               struct nk_rect art, float scale,
                               const DesignPage *dp, int page_w, int page_h) {
  if (!dp || !dp->show_rulers)
    return;

  const float ruler_w = 20.0f;
  struct nk_color ruler_bg = nk_rgba(50, 50, 55, 220);
  struct nk_color tick_col = nk_rgba(180, 180, 180, 255);
  struct nk_color text_col = nk_rgb(200, 200, 200);

  // Top ruler
  struct nk_rect top_ruler = nk_rect(art.x, art.y - ruler_w, art.w, ruler_w);
  nk_fill_rect(buf, top_ruler, 0, ruler_bg);

  // Left ruler
  struct nk_rect left_ruler = nk_rect(art.x - ruler_w, art.y, ruler_w, art.h);
  nk_fill_rect(buf, left_ruler, 0, ruler_bg);

  // Corner square
  nk_fill_rect(buf, nk_rect(art.x - ruler_w, art.y - ruler_w, ruler_w, ruler_w),
               0, ruler_bg);

  struct nk_user_font *fnt = (struct nk_user_font *)buf->userdata.ptr;

  // Top ruler ticks
  for (int px = 0; px <= page_w; px += 10) {
    float sx = art.x + (float)px * scale;
    if (sx < art.x || sx > art.x + art.w)
      continue;
    int is_major = (px % 100 == 0);
    float tick_h = is_major ? ruler_w * 0.6f : ruler_w * 0.3f;
    nk_stroke_line(buf, sx, art.y - tick_h, sx, art.y, 0.5f, tick_col);
    if (is_major && fnt) {
      char lbl[8];
      snprintf(lbl, sizeof(lbl), "%d", px);
      struct nk_rect lr = nk_rect(sx + 1, art.y - ruler_w, 30, 10);
      nk_draw_text(buf, lr, lbl, (int)strlen(lbl), fnt, nk_rgba(0, 0, 0, 0),
                   text_col);
    }
  }

  // Left ruler ticks
  for (int py = 0; py <= page_h; py += 10) {
    float sy = art.y + (float)py * scale;
    if (sy < art.y || sy > art.y + art.h)
      continue;
    int is_major = (py % 100 == 0);
    float tick_w = is_major ? ruler_w * 0.6f : ruler_w * 0.3f;
    nk_stroke_line(buf, art.x - tick_w, sy, art.x, sy, 0.5f, tick_col);
    if (is_major && fnt) {
      char lbl[8];
      snprintf(lbl, sizeof(lbl), "%d", py);
      struct nk_rect lr = nk_rect(art.x - ruler_w + 1, sy + 1, ruler_w - 2, 10);
      nk_draw_text(buf, lr, lbl, (int)strlen(lbl), fnt, nk_rgba(0, 0, 0, 0),
                   text_col);
    }
  }
}

void ui_render_block_design(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
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
  float sidebar_w = 260.0f;

  float row_h = nk_window_get_content_region(ctx).h;
  if (row_h < 600.0f) // Changed minimum height from 200 to 600 for better
                      // sidebar visibility
    row_h = 600.0f;
  ASLAN_DEBUG("Design block layout row_h = %f", row_h);

  float display_h = row_h - toolbar_h;
  if (display_h < 100.0f)
    display_h = 100.0f;

  // Workspace column (dynamic width) + sidebar column (fixed width)
  nk_layout_row_template_begin(ctx, row_h);
  nk_layout_row_template_push_dynamic(ctx);
  nk_layout_row_template_push_static(ctx, sidebar_w);
  nk_layout_row_template_end(ctx);

  // --- Rendering Workspace ---
  // DEBUG: Visualize the intended area
  /*
  nk_stroke_rect(nk_window_get_canvas(ctx),
      nk_layout_widget_bounds(ctx),
      2.0f, 2.0f, nk_rgb(255, 0, 0));
  */
  // Print dimensions to debug visibility
  ASLAN_DEBUG("Design Block %u: width=%.1f height=%.1f display_h=%.1f",
              block->id, width, (float)dd->height, display_h);

  char ws_gp[64];
  snprintf(ws_gp, sizeof(ws_gp), "design_content_group_%u", block->id);
  if (nk_group_begin(ctx, ws_gp, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
    struct nk_rect ws_bounds = nk_window_get_content_region(ctx);
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    DesignInteraction *ds = &ui->design_state;

    if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                        ws_bounds)) {
      ui->active_block_id = block->id;
    }

    // Pre-calculate toolbar flags for interaction logic (8 buttons in
    // design)
    float toolbar_w = 8 * 36.0f + 7 * 8.0f + 24.0f;
    float toolbar_x = ws_bounds.x + (ws_bounds.w - toolbar_w) / 2.0f;
    float toolbar_y = ws_bounds.y + ws_bounds.h - 52.0f - 20.0f; // toolbar_h=52

    int in_toolbar =
        (ui->mouse_x >= toolbar_x && ui->mouse_x < toolbar_x + toolbar_w &&
         ui->mouse_y >= toolbar_y && ui->mouse_y < toolbar_y + 52.0f);
    int in_forms_menu = 0;

    // Early check for active state
    int is_active_design = (ds->active_design_id == block->id);

    // Hardened Input Check: Sync Nuklear input state (only if inside
    // workspace)
    if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
        !ui->block_drag.is_active && ui->mouse_x >= ws_bounds.x &&
        ui->mouse_x < ws_bounds.x + ws_bounds.w && ui->mouse_y >= ws_bounds.y &&
        ui->mouse_y < ws_bounds.y + ws_bounds.h) {
      ds->mouse_just_pressed = 1;
    }

    float scale = 1.0f;

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
      struct nk_rect sc_bounds = nk_window_get_bounds(ctx);

      // Background: support solid, linear, radial
      struct nk_color c1 =
          nk_rgba((dd->pages[dd->current_page].bg_color >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color & 0xFF);
      struct nk_color c2 =
          nk_rgba((dd->pages[dd->current_page].bg_color_2 >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color_2 >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color_2 >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color_2 & 0xFF);

      if (dd->pages[dd->current_page].bg_type == 0) { // Solid
        nk_fill_rect(canvas_buf, sc_bounds, 0, c1);
      } else if (dd->pages[dd->current_page].bg_type == 1) { // Linear (V)
        nk_fill_rect_multi_color(canvas_buf, sc_bounds, c1, c1, c2, c2);
      } else { // Radial (Approximation using multi-color with corners)
        // For a better radial we'd need a custom shader or many steps.
        // We'll use multi-color center-edge which is a 4-color linear mix.
        nk_fill_rect_multi_color(canvas_buf, sc_bounds, c2, c2, c1,
                                 c1); // Reverse for center-ish feel if possible
      }
      DesignPage *cur_dp = &dd->pages[dd->current_page];
      design_draw_grid(canvas_buf, sc_bounds, zoom, cur_dp, cam_x, cam_y, 1);
      design_draw_guides(canvas_buf, sc_bounds, zoom, cur_dp);
      design_draw_rulers(canvas_buf, sc_bounds, zoom, cur_dp, dd->width,
                         dd->height);
      nk_push_scissor(canvas_buf, sc_bounds);

      // Use full workspace as coordinate space
      nk_layout_space_begin(ctx, NK_STATIC, ws_bounds.h, 1);
      nk_layout_space_push(ctx, nk_rect(0, 0, ws_bounds.w, ws_bounds.h));
    } else {
      // --- Fixed Artboard Mode ---
      nk_fill_rect(canvas_buf, ws_bounds, 0, nk_rgb(241, 241, 239));

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

      struct nk_rect shadow_rect = nk_rect(sx + 2, sy + 2, art_w, art_h);
      nk_fill_rect(canvas_buf, shadow_rect, 2, nk_rgba(0, 0, 0, 40));

      struct nk_rect art_rect = nk_rect(sx, sy, art_w, art_h);
      struct nk_color c1 =
          nk_rgba((dd->pages[dd->current_page].bg_color >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color & 0xFF);
      struct nk_color c2 =
          nk_rgba((dd->pages[dd->current_page].bg_color_2 >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color_2 >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color_2 >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color_2 & 0xFF);

      if (dd->pages[dd->current_page].bg_type == 0) {
        nk_fill_rect(canvas_buf, art_rect, 0, c1);
      } else if (dd->pages[dd->current_page].bg_type == 1) {
        nk_fill_rect_multi_color(canvas_buf, art_rect, c1, c1, c2, c2);
      } else {
        // Simple Radial approximation for artboard
        nk_fill_rect_multi_color(canvas_buf, art_rect, c2, c2, c1, c1);
      }
      // Border drawn AFTER content (see below, post nk_layout_space_end)
      {
        DesignPage *cur_dp = &dd->pages[dd->current_page];
        design_draw_grid(canvas_buf, art_rect, scale, cur_dp, 0, 0, 0);
        design_draw_guides(canvas_buf, art_rect, scale, cur_dp);
        design_draw_rulers(canvas_buf, art_rect, scale, cur_dp, dd->width,
                           dd->height);
      }

      nk_layout_space_begin(ctx, NK_STATIC, ws_bounds.h, 1);
      nk_layout_space_push(
          ctx, nk_rect(sx - ws_bounds.x, sy - ws_bounds.y, art_w, art_h));
    }

    // --- Shared content rendering (both modes) ---
    nk_style_push_style_item(ctx, &ctx->style.window.fixed_background,
                             nk_style_item_color(nk_rgba(0, 0, 0, 0)));
    int group_res =
        nk_group_begin(ctx, "design_content", NK_WINDOW_NO_SCROLLBAR);
    nk_style_pop_style_item(ctx);
    if (group_res) {
      struct nk_rect bounds_raw = nk_window_get_content_region(ctx);
      struct nk_rect bounds;

      if (dd->no_limits) {
        // Offset bounds so existing code (bounds.x + node.x * scale) works
        float cam_x = ds->cam_x, cam_y = ds->cam_y;
        float zoom = ds->zoom > 0.01f ? ds->zoom : 1.0f;
        bounds =
            nk_rect(bounds_raw.x - cam_x * zoom, bounds_raw.y - cam_y * zoom,
                    bounds_raw.w, bounds_raw.h);
      } else {
        bounds = bounds_raw;
      }

      ds->bounds_x = bounds_raw.x;
      ds->bounds_y = bounds_raw.y;
      ds->bounds_w = bounds_raw.w;
      ds->bounds_h = bounds_raw.h;
      ds->scale = scale;

      // Track if this design is active
      // Track if this design is active
      is_active_design = (ds->active_design_id == block->id);

      // Auto-activate if clicked inside WORKSPACE (including toolbar)
      if (!is_active_design && !ui->block_drag.is_active &&
          nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
          ui->mouse_x >= ws_bounds.x &&
          ui->mouse_x < ws_bounds.x + ws_bounds.w &&
          ui->mouse_y >= ws_bounds.y &&
          ui->mouse_y < ws_bounds.y + ws_bounds.h) {
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
          float dx = ctx->input.mouse.delta.x / scale;
          float dy = ctx->input.mouse.delta.y / scale;
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
        float hint_w =
            ctx->style.font->width(ctx->style.font->userdata,
                                   ctx->style.font->height, hint, strlen(hint));
        nk_draw_text(canvas_buf,
                     nk_rect(bounds.x + (bounds.w - hint_w) / 2.0f,
                             bounds.y + bounds.h / 2.0f - 10.0f, hint_w, 20),
                     hint, strlen(hint), ctx->style.font, nk_rgba(0, 0, 0, 0),
                     nk_rgb(200, 200, 200));
      }

      // --- Set artboard clip for this frame ---
      // In fixed artboard mode, bounds_raw == artboard screen rect.
      // In free (no_limits) mode, use infinite so nothing clips.
      if (dd->no_limits) {
        ui->artboard_clip = nk_rect(-8192, -8192, 16384, 16384);
      } else {
        ui->artboard_clip = bounds_raw; // = artboard screen rect
        nk_push_scissor(canvas_buf, bounds_raw);
      }

      // --- Render nodes ---
      if (dd->root) {
        ui->hovered_node_id = 0; // Reset each frame so hover doesn't persist
        for (Node *node = dd->root->first_child; node;
             node = node->next_sibling) {
          ui_render_node_tree(ui, ctx, engine, block, dd, canvas_buf, node,
                              bounds, scale, bounds.x, bounds.y);
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
          // struct nk_rect nr = nk_rect(sx, sy, sw, sh);

          float sel_off = 2.0f + (node->stroke_width * scale) / 2.0f;
          float rad = node->rotation * 3.14159f / 180.0f;
          float cx = sx + sw / 2.0f;
          float cy = sy + sh / 2.0f;

          fill_rotated_rounded_rect(canvas_buf, cx, cy, sw + sel_off * 2,
                                    sh + sel_off * 2, rad, 0, COLOR_ACCENT, 1,
                                    1.0f);

          float hsz = 8.0f;
          struct nk_color hc = COLOR_ACCENT;
          float c = cosf(rad);
          float s = sinf(rad);
          float h_hw = (sw + sel_off * 2) / 2.0f;
          float h_hh = (sh + sel_off * 2) / 2.0f;
          float handle_pts[4][2] = {
              {h_hw, h_hh}, {-h_hw, h_hh}, {-h_hw, -h_hh}, {h_hw, -h_hh}};
          for (int i = 0; i < 4; i++) {
            float hx = cx + handle_pts[i][0] * c - handle_pts[i][1] * s;
            float hy = cy + handle_pts[i][0] * s + handle_pts[i][1] * c;
            nk_fill_rect(canvas_buf,
                         nk_rect(hx - hsz / 2, hy - hsz / 2, hsz, hsz), 2.0f,
                         hc);
          }

          // Rotation handle: small circle above top-center of the selection
          // box
          float rot_dist = 28.0f;
          float top_x = cx + 0.0f * c - (-h_hh) * s;
          float top_y = cy + 0.0f * s + (-h_hh) * c;
          float rh_lx = 0.0f, rh_ly = -(h_hh + rot_dist);
          float rh_x = cx + rh_lx * c - rh_ly * s;
          float rh_y = cy + rh_lx * s + rh_ly * c;
          float rh_r = 6.0f;
          nk_stroke_line(canvas_buf, top_x, top_y, rh_x, rh_y, 1.2f, hc);
          nk_fill_circle(canvas_buf,
                         nk_rect(rh_x - rh_r, rh_y - rh_r, rh_r * 2, rh_r * 2),
                         hc);
          nk_stroke_circle(canvas_buf,
                           nk_rect(rh_x - rh_r - 1, rh_y - rh_r - 1,
                                   rh_r * 2 + 2, rh_r * 2 + 2),
                           1.5f, nk_rgb(255, 255, 255));
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
          fill_rotated_rounded_rect(canvas_buf, mcx, mcy, msw + 4, msh + 4,
                                    mrad, 0, COLOR_ACCENT, 1, 1.5f);
        }
      }

      // Check mouse in design area
      int mouse_in_design = (ui->mouse_x >= bounds_raw.x &&
                             ui->mouse_x < bounds_raw.x + bounds_raw.w &&
                             ui->mouse_y >= bounds_raw.y &&
                             ui->mouse_y < bounds_raw.y + bounds_raw.h);

      // Explicitly exclude sidebar area in fullscreen
      if (is_fullscreen && ui->mouse_x > nk_window_get_width(ctx) - 250.0f) {
        mouse_in_design = 0;
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
          float dx = ui->mouse_x - hx;
          float dy = ui->mouse_y - hy;
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
        float rdx = ui->mouse_x - rh_x2;
        float rdy = ui->mouse_y - rh_y2;
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
        ds->resize_start_x = ui->mouse_x;
        ds->resize_start_y = ui->mouse_y;
        ds->resize_start_node_x = ui->selected_node->x;
        ds->resize_start_node_y = ui->selected_node->y;
        ds->resize_start_font_size = ui->selected_node->font_size;
      }
      // Handle resize dragging
      else if (ds->is_resizing && ui->selected_node && is_active_design) {
        float dx = (ui->mouse_x - ds->resize_start_x) / scale;
        float dy = (ui->mouse_y - ds->resize_start_y) / scale;
        Node *sel = ui->selected_node;

        int lr = sel->lock_ratio && ds->resize_start_h > 0.1f;
        float ratio = lr ? ds->resize_start_w / ds->resize_start_h : 1.0f;
        float nw, nh;

        switch (ds->resize_handle) {
        case 0: // SE
          nw = ds->resize_start_w + dx;
          nh = ds->resize_start_h + dy;
          if (lr) { if (fabsf(dx) >= fabsf(dy)) nh = nw / ratio; else nw = nh * ratio; }
          sel->width  = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          break;
        case 1: // SW
          nw = ds->resize_start_w - dx;
          nh = ds->resize_start_h + dy;
          if (lr) { if (fabsf(dx) >= fabsf(dy)) nh = nw / ratio; else nw = nh * ratio; }
          sel->width  = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          sel->x = ds->resize_start_node_x + (ds->resize_start_w - sel->width);
          break;
        case 2: // NW
          nw = ds->resize_start_w - dx;
          nh = ds->resize_start_h - dy;
          if (lr) { if (fabsf(dx) >= fabsf(dy)) nh = nw / ratio; else nw = nh * ratio; }
          sel->width  = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          sel->x = ds->resize_start_node_x + (ds->resize_start_w - sel->width);
          sel->y = ds->resize_start_node_y + (ds->resize_start_h - sel->height);
          break;
        case 3: // NE
          nw = ds->resize_start_w + dx;
          nh = ds->resize_start_h - dy;
          if (lr) { if (fabsf(dx) >= fabsf(dy)) nh = nw / ratio; else nw = nh * ratio; }
          sel->width  = fmaxf(20.0f, nw);
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
        ds->rotate_start_angle = atan2f(ui->mouse_y - cy2, ui->mouse_x - cx2);
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
        float cur_angle = atan2f(ui->mouse_y - cy2, ui->mouse_x - cx2);
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
          struct nk_rect r_anchor =
              nk_rect(px - 10 * scale, py - 10 * scale, 20 * scale, 20 * scale);
          if (nk_input_is_mouse_hovering_rect(&ctx->input, r_anchor)) {
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
            struct nk_rect r1 = nk_rect(cp1x - 8 * scale, cp1y - 8 * scale,
                                        16 * scale, 16 * scale);
            struct nk_rect r2 = nk_rect(cp2x - 8 * scale, cp2y - 8 * scale,
                                        16 * scale, 16 * scale);
            if (nk_input_is_mouse_hovering_rect(&ctx->input, r1)) {
              if (ds->mouse_just_pressed) {
                ds->dragging_point_idx = i;
                ds->dragging_cp_idx = 1;
              }
              handle_hit = 1;
              break;
            }
            if (nk_input_is_mouse_hovering_rect(&ctx->input, r2)) {
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
          if (fabsf(ui->mouse_y - sy) < hit_tol) {
            hit_h_guide = i;
            break;
          }
        }
        if (hit_h_guide == -1) {
          for (int i = 0; i < dp->v_guide_count; i++) {
            float sx = bounds.x + dp->v_guides[i] * scale;
            if (fabsf(ui->mouse_x - sx) < hit_tol) {
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
              (ui->mouse_y - bounds.y) / scale;
        } else if (ds->dragging_guide_type == 2) { // V guide
          dp->v_guides[ds->dragging_guide_idx] =
              (ui->mouse_x - bounds.x) / scale;
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
        float design_x = (ui->mouse_x - bounds.x) / scale;
        float design_y = (ui->mouse_y - bounds.y) / scale;

        // Shift+click = multi-selection toggle
        SDL_Keymod _click_mods = SDL_GetModState();
        int shift_click = (_click_mods & KMOD_SHIFT) != 0;

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
            if (hit == ui->selected_node) {
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
              // In bezier mode, only select — don't drag so handles take priority
              if (!ds->bezier_mode) {
                ds->is_dragging_node = 1;
                ds->drag_offset_x = design_x - hit->x;
                ds->drag_offset_y = design_y - hit->y;
                ds->drag_press_x = ui->mouse_x;
                ds->drag_press_y = ui->mouse_y;
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
            ds->pan_start_x = ui->mouse_x;
            ds->pan_start_y = ui->mouse_y;
            ds->cam_start_x = ds->cam_x;
            ds->cam_start_y = ds->cam_y;
          }
        }
      }

      // Handle image cropping dragging (active until user clicks crop btn again)
      if (ds->is_cropping && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_IMAGE && is_active_design) {
        Node *sel = ui->selected_node;
        // Init start pos on each new press
        if (ds->mouse_just_pressed && mouse_in_design) {
          ds->crop_start_x       = ui->mouse_x;
          ds->crop_start_y       = ui->mouse_y;
          ds->crop_start_node_x  = sel->image_crop_x;
          ds->crop_start_node_y  = sel->image_crop_y;
        }
        if (nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT) &&
            mouse_in_design && (ds->crop_start_x != 0 || ds->crop_start_y != 0)) {
          float dx = (ui->mouse_x - ds->crop_start_x) / scale;
          float dy = (ui->mouse_y - ds->crop_start_y) / scale;
          sel->image_crop_x = ds->crop_start_node_x + dx;
          sel->image_crop_y = ds->crop_start_node_y + dy;
        }
        // Crop mode stays active — toggled off only via the button in Properties
      }
      // Handle node dragging
      else if (ds->is_dragging_node && ui->selected_node && is_active_design) {
        // 5px threshold to prevent accidental drag on simple click
        float pdx = ui->mouse_x - ds->drag_press_x;
        float pdy = ui->mouse_y - ds->drag_press_y;
        if (fabsf(pdx) < 5.0f && fabsf(pdy) < 5.0f) {
          if (ds->mouse_just_released) ds->is_dragging_node = 0;
          goto after_drag;
        }

        float design_x = (ui->mouse_x - bounds.x) / scale;
        float design_y = (ui->mouse_y - bounds.y) / scale;
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
      after_drag:;

      // --- Right-click context menu detection ---
      if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_RIGHT) &&
          mouse_in_design && !in_toolbar && is_active_design) {
        float dx = (ui->mouse_x - bounds.x) / scale;
        float dy = (ui->mouse_y - bounds.y) / scale;
        Node *hit = design_find_node_at(dd, dx, dy);
        if (hit) {
          ui->show_context_menu = 1;
          ui->context_menu_x = ui->mouse_x;
          ui->context_menu_y = ui->mouse_y;
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
        float wx = (ui->mouse_x - bounds.x) / scale;
        float wy = (ui->mouse_y - bounds.y) / scale;

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
          for (int i = 0; i < ds->current_stroke->count - 1; i++) {
            Point p1 = ds->current_stroke->points[i];
            Point p2 = ds->current_stroke->points[i + 1];
            struct nk_color col =
                nk_rgba((ds->current_stroke->color >> 24) & 0xFF,
                        (ds->current_stroke->color >> 16) & 0xFF,
                        (ds->current_stroke->color >> 8) & 0xFF,
                        ds->current_stroke->color & 0xFF);
            nk_stroke_line(canvas_buf, bounds.x + p1.x * scale,
                           bounds.y + p1.y * scale, bounds.x + p2.x * scale,
                           bounds.y + p2.y * scale,
                           ds->current_stroke->thickness * scale, col);
          }
        } else {
          for (int i = 0; i < ds->current_stroke->count - 1; i++) {
            Point p1 = ds->current_stroke->points[i];
            Point p2 = ds->current_stroke->points[i + 1];
            struct nk_color col =
                nk_rgba((ds->current_stroke->color >> 24) & 0xFF,
                        (ds->current_stroke->color >> 16) & 0xFF,
                        (ds->current_stroke->color >> 8) & 0xFF,
                        ds->current_stroke->color & 0xFF);
            nk_stroke_line(canvas_buf, bounds.x + p1.x * scale,
                           bounds.y + p1.y * scale, bounds.x + p2.x * scale,
                           bounds.y + p2.y * scale,
                           ds->current_stroke->thickness * scale, col);
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
            struct nk_rect anchor_rect =
                nk_rect(px - 5 * scale, py - 5 * scale, 10 * scale, 10 * scale);
            nk_fill_rect(canvas_buf, anchor_rect, 0, nk_rgb(255, 255, 255));
            nk_stroke_rect(canvas_buf, anchor_rect, 0, 1.5f * scale,
                           COLOR_ACCENT);

            if (p->type == 1) { // Cubic
              float cp1x = sx + p->cp1x * scale;
              float cp1y = sy + p->cp1y * scale;
              float cp2x = sx + p->cp2x * scale;
              float cp2y = sy + p->cp2y * scale;

              nk_stroke_line(canvas_buf, px, py, cp1x, cp1y, 1,
                             nk_rgb(200, 200, 200));
              nk_stroke_line(canvas_buf, px, py, cp2x, cp2y, 1,
                             nk_rgb(200, 200, 200));

              struct nk_rect r1 = nk_rect(cp1x - 4 * scale, cp1y - 4 * scale,
                                          8 * scale, 8 * scale);
              struct nk_rect r2 = nk_rect(cp2x - 4 * scale, cp2y - 4 * scale,
                                          8 * scale, 8 * scale);
              nk_fill_circle(canvas_buf, r1, COLOR_ACCENT);
              nk_fill_circle(canvas_buf, r2, COLOR_ACCENT);
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
        draw_alignment_guides(canvas_buf, sel, bounds.x + bounds.w / 2.0f,
                              bounds.y + bounds.h / 2.0f, bounds.x, bounds.y,
                              bounds.w, bounds.h, nsx, nsy, nsw, nsh);
      }

      // --- Snap guide lines (element-to-element snap feedback) ---
      if (ds->snap_guide_x >= 0 || ds->snap_guide_y >= 0) {
        struct nk_color snap_col = nk_rgba(0, 195, 215, 200);
        if (ds->snap_guide_x >= 0)
          nk_stroke_line(canvas_buf, ds->snap_guide_x, bounds.y,
                         ds->snap_guide_x, bounds.y + bounds.h, 1.0f, snap_col);
        if (ds->snap_guide_y >= 0)
          nk_stroke_line(canvas_buf, bounds.x, ds->snap_guide_y,
                         bounds.x + bounds.w, ds->snap_guide_y, 1.0f, snap_col);
      }

      // Close Design content group (Artboard)
      nk_group_end(ctx);
    }
    nk_layout_space_end(ctx);

    // --- Fixed artboard: redraw border on top of content ---
    // This covers any pixel bleed at the edges, giving a clean professional look.
    if (!dd->no_limits) {
      struct nk_command_buffer *ws_canvas = nk_window_get_canvas(ctx);
      struct nk_rect ac = ui->artboard_clip;
      // Thin shadow (already drawn before, but redraw for depth)
      nk_stroke_rect(ws_canvas, nk_rect(ac.x+2, ac.y+2, ac.w, ac.h),
                     0, 6.0f, nk_rgba(0, 0, 0, 18));
      // Mask: draw 4 thin border-covering strips in the workspace background
      // color to cleanly hide any node pixels that escaped the scissor.
      struct nk_color ws_bg = nk_rgb(241, 241, 239);
      float bw = 1.5f; // cover width in px
      nk_fill_rect(ws_canvas, nk_rect(ac.x - bw, ac.y - bw, ac.w + bw*2, bw), 0, ws_bg);
      nk_fill_rect(ws_canvas, nk_rect(ac.x - bw, ac.y + ac.h, ac.w + bw*2, bw), 0, ws_bg);
      nk_fill_rect(ws_canvas, nk_rect(ac.x - bw, ac.y, bw, ac.h), 0, ws_bg);
      nk_fill_rect(ws_canvas, nk_rect(ac.x + ac.w, ac.y, bw, ac.h), 0, ws_bg);
      // Clean border on top
      nk_stroke_rect(ws_canvas, ac, 0, 1.0f, nk_rgba(180, 180, 185, 255));
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
      canvas_render_toolbar(
          ui, ctx, nk_rect(ws_bounds.x, ws_bounds.y, ws_bounds.w, ws_bounds.h),
          block);

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
        !nk_input_is_mouse_hovering_rect(
            &ctx->input,
            nk_rect(ws_bounds.x, ws_bounds.y + ws_bounds.h - toolbar_h,
                    ws_bounds.w, toolbar_h))) {
    }

    // --- Zoom controls for no_limits mode ---
    if (dd->no_limits) {
      struct nk_command_buffer *zcb = nk_window_get_canvas(ctx);

      // Zoom with scroll wheel
      if (ui->mouse_x >= ws_bounds.x &&
          ui->mouse_x < ws_bounds.x + ws_bounds.w &&
          ui->mouse_y >= ws_bounds.y &&
          ui->mouse_y < ws_bounds.y + ws_bounds.h) {
        float scroll = ctx->input.mouse.scroll_delta.y;
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
      nk_draw_text(zcb, nk_rect(zx, zy, 50, 20), zoom_str, strlen(zoom_str),
                   ctx->style.font, nk_rgba(0, 0, 0, 0), nk_rgb(120, 120, 120));

      // Save camera state back to DesignData
      dd->view_x = ds->cam_x;
      dd->view_y = ds->cam_y;
      dd->zoom = ds->zoom;
    }

    // --- Right-click context menu ---
    render_node_context_menu(ui, ctx, nk_window_get_canvas(ctx), block);

    // --- Native Inline Text Editor Overlay ---
    if (ui->editing_node && ui->editing_node->type == NODE_TYPE_TEXT &&
        is_active_design) {
      Node *enode = ui->editing_node;
      struct nk_rect ebounds = ui->editing_node_rect;

      struct nk_color txt_color = {(enode->stroke_color >> 24) & 0xFF,
                                   (enode->stroke_color >> 16) & 0xFF,
                                   (enode->stroke_color >> 8) & 0xFF,
                                   enode->stroke_color & 0xFF};

      char overlay_id[64];
      snprintf(overlay_id, sizeof(overlay_id), "edit_overlay_%u", enode->id);

      nk_style_push_style_item(ctx, &ctx->style.window.fixed_background,
                               nk_style_item_color(nk_rgba(0, 0, 0, 0)));
      if (nk_popup_begin(ctx, NK_POPUP_STATIC, overlay_id, 0,
                         nk_rect(ebounds.x - ws_bounds.x,
                                 ebounds.y - ws_bounds.y, ebounds.w + 40,
                                 ebounds.h + 20))) {

        nk_layout_row_dynamic(ctx, ebounds.h, 1);

        struct nk_style_edit style = ctx->style.edit;
        ctx->style.edit.normal = nk_style_item_color(
            nk_rgba(0, 0, 0, 10)); // Slight hint it's editable
        ctx->style.edit.hover = nk_style_item_color(nk_rgba(0, 0, 0, 15));
        ctx->style.edit.active =
            nk_style_item_color(nk_rgba(255, 255, 255, 200));
        ctx->style.edit.border = 0;
        ctx->style.edit.text_normal = txt_color;
        ctx->style.edit.text_hover = txt_color;
        ctx->style.edit.text_active = txt_color;

        // Get correct font
        const struct nk_user_font *font = ctx->style.font;
        if (enode->font_type == 4 && ui->font_icons) {
          font = &ui->font_icons->handle;
        } else {
          float scaled_size = enode->font_size * scale;
          float available_sizes[] = {12.0f, 14.0f, 16.0f, 20.0f, 24.0f,
                                     32.0f, 48.0f, 64.0f, 96.0f};
          int best_idx = -1;
          float best_diff = 1000.0f;
          for (int i = 0; i < 9; i++) {
            if (fabsf(available_sizes[i] - scaled_size) < best_diff) {
              best_diff = fabsf(available_sizes[i] - scaled_size);
              best_idx = i;
            }
          }
          if (best_idx >= 0 && ui->font_inter_sizes[best_idx])
            font = &ui->font_inter_sizes[best_idx]->handle;
          else if (ui->font_inter)
            font = &ui->font_inter->handle;
        }

        nk_style_push_font(ctx, font);

        if (ui->rename_needs_focus) {
          nk_edit_focus(ctx, NK_EDIT_DEFAULT);
          ui->rename_needs_focus = 0;
        }

        int edit_state = nk_edit_string_zero_terminated(
            ctx, NK_EDIT_BOX | NK_EDIT_MULTILINE, ui->edit_buffer,
            sizeof(ui->edit_buffer) - 1, nk_filter_default);

        nk_style_pop_font(ctx);
        ctx->style.edit = style;

        strncpy(enode->text, ui->edit_buffer, sizeof(enode->text) - 1);

        if (edit_state & NK_EDIT_COMMITED || edit_state & NK_EDIT_DEACTIVATED) {
          ui->editing_node = NULL;
        }

        nk_popup_end(ctx);
      } else {
        ui->editing_node = NULL; // Failsafe
      }
      nk_style_pop_style_item(ctx);
    }

    // Close Workspace group (Column 1)
    nk_group_end(ctx);
  } // design_conte  nk_group_end(ctx); // Ends the workspace group

  // --- Rendering Sidebar ---
  char sb_gp[64];
  snprintf(sb_gp, sizeof(sb_gp), "design_sidebar_group_%u", block->id);
  if (nk_group_begin(ctx, sb_gp, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
    design_render_sidebar(ui, engine, ctx, block);
    nk_group_end(ctx);
  }
}
