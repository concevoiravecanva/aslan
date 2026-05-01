#include "ui_design.h"
#include "../document/aslan_table.h"
#include "../document/document.h"
#include "ui_canvas.h"
#include "ui_shared.h"
#include "ui_properties.h"
#include "node_render.h"
#include "ui_toolbar.h"
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
void draw_alignment_guides(UIHost *ui, Node *node, float area_cx, float area_cy,
                           float area_x, float area_y, float area_w,
                           float area_h, float node_sx, float node_sy,
                           float node_sw, float node_sh);
void render_node_context_menu(UIHost *ui, Block *block);

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
  int hide_workspace = 0;
  float ws_width = width - sidebar_w;
  if (ws_width < 10.0f) {
      hide_workspace = 1;
      ws_width = 0.0f;
  } else if (ws_width < 100.0f) {
      ws_width = 100.0f; // Minimum workspace width before overlapping
  }

  if (!hide_workspace) {
    AslanRect ws_bounds = {0, 0, ws_width, row_h};
    float mx = ui->mouse_x - ui->abs_offset_x;
    float my = ui->mouse_y - ui->abs_offset_y;

    // Check if mouse is interacting with the global topbar dropdowns
    int in_toolbar = topbar_point_in_tools(ui, ui->abs_offset_x + width - 20.0f, 0.0f, 44.0f, ui->mouse_x, ui->mouse_y);
    int in_forms_menu = 0;

    // Early check for active state
    int is_active_design = (ui->input.active_block_id == block->id);

    float scale = 1.0f;
    AslanRect art_rect = {0, 0, 0, 0}; // populated in fixed-artboard branch

    if (dd->no_limits) {
      // --- Infinite Canvas Mode ---
      float zoom = dd->zoom > 0.01f ? dd->zoom : 1.0f;
      float cam_x = dd->view_x, cam_y = dd->view_y;

      // Sync design_state camera
      if (ui->input.active_block_id != block->id) {
        ui->editor.cam_x = cam_x;
        ui->editor.cam_y = cam_y;
        ui->editor.zoom = zoom;
      } else {
        cam_x = ui->editor.cam_x;
        cam_y = ui->editor.cam_y;
        zoom = ui->editor.zoom > 0.01f ? ui->editor.zoom : 1.0f;
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
        float cam_x = ui->editor.cam_x, cam_y = ui->editor.cam_y;
        float zoom = ui->editor.zoom > 0.01f ? ui->editor.zoom : 1.0f;
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
      ui->input.bounds_x = dd->no_limits ? ws_bounds.x : (ui->abs_offset_x + art_rect.x);
      ui->input.bounds_y = dd->no_limits ? ws_bounds.y : (ui->abs_offset_y + art_rect.y);
      ui->input.bounds_w = dd->no_limits ? ws_bounds.w : art_rect.w;
      ui->input.bounds_h = dd->no_limits ? ws_bounds.h : art_rect.h;
      ui->input.scale = scale;

      // Track if this design is active
      // Track if this design is active
      is_active_design = (ui->input.active_block_id == block->id);

      // Auto-activate if clicked inside WORKSPACE (including toolbar)
      if (!is_active_design && !ui->block_drag.is_active &&
          (ui->mouse_buttons & 1) &&
          mx >= ws_bounds.x &&
          mx < ws_bounds.x + ws_bounds.w &&
          my >= ws_bounds.y &&
          my < ws_bounds.y + ws_bounds.h) {
        ui->input.active_block_id = block->id;
        ui->input.active_block = block;
        ui->editing_block_id = block->id;
        is_active_design = 1;
      }

      // Auto-activate when a toolbar tool set pending_create (toolbar is outside
      // ws_bounds so the click-inside check above never fires for it)
      if (!is_active_design && ui->input.pending_create > 0) {
        ui->input.active_block_id = block->id;
        ui->input.active_block    = block;
        ui->editing_block_id      = block->id;
        is_active_design = 1;
      }

      // --- Handle pending element creation ---
      if (is_active_design && ui->input.pending_create > 0) {
        float nx, ny;
        if (dd->no_limits) {
          // Center in current infinite viewport (scale = zoom here)
          nx = ui->editor.cam_x + (ws_bounds.w / scale) / 2.0f - 50.0f;
          ny = ui->editor.cam_y + (ws_bounds.h / scale) / 2.0f - 40.0f;
        } else {
          // Center in fixed artboard
          nx = (float)dd->width / 2.0f - 50.0f;
          ny = (float)dd->height / 2.0f - 40.0f;
        }
        Node *new_node = NULL;
        float def_w = 100.0f, def_h = 80.0f;

        if (ui->input.pending_create == 1) { // Body
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 200.0f, 40.0f, ui->editor.selected_color);
          if (new_node)
            strncpy(new_node->text, "Body text", sizeof(new_node->text) - 1);
        } else if (ui->input.pending_create == 6) { // H1
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 300.0f, 60.0f, ui->editor.selected_color);
          if (new_node)
            strncpy(new_node->text, "Heading", sizeof(new_node->text) - 1);
        } else if (ui->input.pending_create == 2) { // Sticky
          new_node = design_create_styled_node(
              dd, NODE_STYLE_RECT, nx, ny, 120.0f, 120.0f, ui->editor.selected_color);
          if (new_node) {
            new_node->fill_color = 0xFFF9C4FF;
            new_node->stroke_color = 0xFBC02DFF;
            new_node->type = NODE_TYPE_TEXT;
            strncpy(new_node->text, "Note...", sizeof(new_node->text) - 1);
          }
        } else if (ui->input.pending_create == 3) { // Image
#ifdef __EMSCRIPTEN__
          // Defer: opening the dialog mid-render clears the canvas to white.
          // Flag it; main.c will open the dialog after renderer_end.
          ui->deferred_image_dialog   = 1;
          ui->deferred_image_nx       = nx;
          ui->deferred_image_ny       = ny;
          ui->deferred_image_block_id = block->id;
          ui->deferred_image_node_id  = 0;
          ui->input.pending_create    = 0;
          // Skip node creation for now — handled post-frame
          goto skip_node_append;
#endif
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
        } else if (ui->input.pending_create == 11) {
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
        } else if (ui->input.pending_create == 30) { // Table node (from toolbar)
          Page *page = doc_page_get_active(engine->doc);
          if (page && dd->root) {
            uint32_t tbl_id = page->next_block_id;
            char cmd_buf[128];
            snprintf(cmd_buf, sizeof(cmd_buf), "block_create %u %u table",
                     page->id, tbl_id);
            engine_execute_command(engine, cmd_buf);
            new_node = node_create(dd->next_node_id++, NODE_TYPE_TABLE_VIEW, NULL);
            if (new_node) {
              new_node->x = nx;
              new_node->y = ny;
              new_node->width  = 400.0f;
              new_node->height = 260.0f;
              new_node->visible = 1;
              new_node->linked_block_id = tbl_id;
              new_node->parent = dd->root;
              node_add_child(dd->root, new_node);
            }
          }
          ui->input.pending_create = 0;
        } else if (ui->input.pending_create == 12) { // Icon
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 60.0f, 60.0f, ui->editor.selected_color);
          if (new_node) {
            new_node->font_type = 4; // Assuming 4 is ui->font_icons
            new_node->font_size = 48.0f;
            strncpy(new_node->text, "\xF3\x87\xAF",
                    sizeof(new_node->text) - 1); // ri-remixicon-line
          }
        } else if (ui->input.pending_create >= 10 && ui->input.pending_create <= 13) {
          NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                    NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
          new_node =
              design_create_styled_node(dd, styles[ui->input.pending_create - 10], nx,
                                        ny, def_w, def_h, ui->editor.selected_color);
        }

        if (new_node)
          ui->selected_node = new_node;
        ui->input.pending_create = 0;
#ifdef __EMSCRIPTEN__
        skip_node_append:;
#endif
      }

      // Handle Bezier point dragging
      if (ui->input.bezier_mode && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_PATH) {
        Node *node = ui->selected_node;
        if (ui->input.dragging_point_idx != -1 && (ui->mouse_buttons & 1)) {
          float dx = 0;
          float dy = 0;
          NodePathPoint *p = &node->path_points[ui->input.dragging_point_idx];
          if (ui->input.dragging_cp_idx == 0) {
            p->x += dx;
            p->y += dy;
          } else if (ui->input.dragging_cp_idx == 1) {
            p->cp1x += dx;
            p->cp1y += dy;
          } else if (ui->input.dragging_cp_idx == 2) {
            p->cp2x += dx;
            p->cp2y += dy;
          }
          node_update_path_bounds(node);
        } else {
          ui->input.dragging_point_idx = -1;
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
        if (ui->selected_node && is_active_design && !ui->input.bezier_mode) {
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
        ui->input.mode = INTERACTION_NONE;
        ui->input.mode = INTERACTION_NONE;
        ui->input.mode = INTERACTION_NONE;
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
      if (mouse_in_design && ui->input.mouse_just_pressed && on_resize_handle &&
          !on_rotate_handle && ui->selected_node && is_active_design) {
        ui->input.mode = INTERACTION_RESIZE_NODE;
        ui->input.active_handle = hit_handle_index;
        ui->input.start_w = ui->selected_node->width;
        ui->input.start_h = ui->selected_node->height;
        ui->input.start_x = mx;
        ui->input.start_y = my;
        ui->input.start_node_x = ui->selected_node->x;
        ui->input.start_node_y = ui->selected_node->y;
        ui->input.start_font_size = ui->selected_node->font_size;
      }
      // Handle resize dragging
      else if ((ui->input.mode == INTERACTION_RESIZE_NODE) && ui->selected_node && is_active_design) {
        float dx = (mx - ui->input.start_x) / scale;
        float dy = (my - ui->input.start_y) / scale;
        Node *sel = ui->selected_node;

        int lr = sel->lock_ratio && ui->input.start_h > 0.1f;
        float ratio = lr ? ui->input.start_w / ui->input.start_h : 1.0f;
        float nw, nh;

        switch (ui->input.active_handle) {
        case 0: // SE
          nw = ui->input.start_w + dx;
          nh = ui->input.start_h + dy;
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
          nw = ui->input.start_w - dx;
          nh = ui->input.start_h + dy;
          if (lr) {
            if (fabsf(dx) >= fabsf(dy))
              nh = nw / ratio;
            else
              nw = nh * ratio;
          }
          sel->width = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          sel->x = ui->input.start_node_x + (ui->input.start_w - sel->width);
          break;
        case 2: // NW
          nw = ui->input.start_w - dx;
          nh = ui->input.start_h - dy;
          if (lr) {
            if (fabsf(dx) >= fabsf(dy))
              nh = nw / ratio;
            else
              nw = nh * ratio;
          }
          sel->width = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          sel->x = ui->input.start_node_x + (ui->input.start_w - sel->width);
          sel->y = ui->input.start_node_y + (ui->input.start_h - sel->height);
          break;
        case 3: // NE
          nw = ui->input.start_w + dx;
          nh = ui->input.start_h - dy;
          if (lr) {
            if (fabsf(dx) >= fabsf(dy))
              nh = nw / ratio;
            else
              nw = nh * ratio;
          }
          sel->width = fmaxf(20.0f, nw);
          sel->height = fmaxf(20.0f, nh);
          sel->y = ui->input.start_node_y + (ui->input.start_h - sel->height);
          break;
        }

        // Scale font size proportionally for text nodes
        if (sel->type == NODE_TYPE_TEXT && ui->input.start_w > 0.1f) {
          float scale_factor = sel->width / ui->input.start_w;
          sel->font_size = ui->input.start_font_size * scale_factor;
          if (sel->font_size < 8.0f) {
            sel->font_size = 8.0f;
          }
        }

        if (ui->input.mouse_just_released) {
          ui->input.mode = INTERACTION_NONE;
        }
      }

      // Start rotation
      if (mouse_in_design && ui->input.mouse_just_pressed && on_rotate_handle &&
          ui->selected_node && is_active_design) {
        Node *sel = ui->selected_node;
        float sx2 = bounds.x + sel->x * scale;
        float sy2 = bounds.y + sel->y * scale;
        float sw2 = sel->width * scale;
        float sh2 = sel->height * scale;
        float cx2 = sx2 + sw2 / 2.0f;
        float cy2 = sy2 + sh2 / 2.0f;
        ui->input.mode = INTERACTION_ROTATE_NODE;
        ui->input.start_angle = atan2f(my - cy2, mx - cx2);
        ui->input.start_node_rotation = sel->rotation;
      }
      // Handle rotation dragging
      else if ((ui->input.mode == INTERACTION_ROTATE_NODE) && ui->selected_node && is_active_design) {
        Node *sel = ui->selected_node;
        float sx2 = bounds.x + sel->x * scale;
        float sy2 = bounds.y + sel->y * scale;
        float sw2 = sel->width * scale;
        float sh2 = sel->height * scale;
        float cx2 = sx2 + sw2 / 2.0f;
        float cy2 = sy2 + sh2 / 2.0f;
        float cur_angle = atan2f(my - cy2, mx - cx2);
        float delta_deg =
            (cur_angle - ui->input.start_angle) * 180.0f / 3.14159f;
        sel->rotation = ui->input.start_node_rotation + delta_deg;
        if (ui->input.mouse_just_released)
          ui->input.mode = INTERACTION_NONE;
      }

      // 4. Handle Bezier interaction priority
      int handle_hit = 0;
      if (ui->input.bezier_mode && ui->selected_node &&
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
            if (ui->input.mouse_just_pressed) {
              ui->input.dragging_point_idx = i;
              ui->input.dragging_cp_idx = 0;
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
              if (ui->input.mouse_just_pressed) {
                ui->input.dragging_point_idx = i;
                ui->input.dragging_cp_idx = 1;
              }
              handle_hit = 1;
              break;
            }
            if (mx >= r2.x && mx < r2.x + r2.w &&
                my >= r2.y && my < r2.y + r2.h) {
              if (ui->input.mouse_just_pressed) {
                ui->input.dragging_point_idx = i;
                ui->input.dragging_cp_idx = 2;
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

        if (ui->input.mouse_just_pressed &&
            (hit_h_guide != -1 || hit_v_guide != -1)) {
          if (hit_h_guide != -1) {
            ui->input.dragging_guide_type = 1;
            ui->input.dragging_guide_idx = hit_h_guide;
          } else {
            ui->input.dragging_guide_type = 2;
            ui->input.dragging_guide_idx = hit_v_guide;
          }
          handle_hit = 1; // consume click
        }

        // Also update cursor
        if (hit_h_guide != -1 || ui->input.dragging_guide_type == 1) {
          ui_set_cursor(ui, UI_CURSOR_SIZENS);
        } else if (hit_v_guide != -1 || ui->input.dragging_guide_type == 2) {
          ui_set_cursor(ui, UI_CURSOR_SIZEWE);
        }
      }

      if (ui->input.dragging_guide_type != 0 && is_active_design) {
        DesignPage *dp = &dd->pages[dd->current_page];
        if (ui->input.dragging_guide_type == 1) { // H guide
          dp->h_guides[ui->input.dragging_guide_idx] =
              (my - bounds.y) / scale;
        } else if (ui->input.dragging_guide_type == 2) { // V guide
          dp->v_guides[ui->input.dragging_guide_idx] =
              (mx - bounds.x) / scale;
        }
        if (ui->input.mouse_just_released) {
          ui->input.dragging_guide_type = 0;
        }
        handle_hit = 1; // prevent other interactions while dragging guide
      }

      // Handle click in design area
      if (handle_hit) {
        // Handle hit, skip node selection
      } else if (mouse_in_design && !in_toolbar && !in_forms_menu &&
                 ui->input.mouse_just_pressed && !on_resize_handle &&
                 !on_rotate_handle && is_active_design) {
        // Convert to design coordinates
        float design_x = (mx - bounds.x) / scale;
        float design_y = (my - bounds.y) / scale;

        // Shift+click = multi-selection toggle
        int shift_click = 0;

        Node *hit = design_find_node_at(dd, design_x, design_y);
        if (hit && shift_click && ui->editor.active_tool != CANVAS_TOOL_PEN) {
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
          if (ui->editor.active_tool == CANVAS_TOOL_PEN) {
            // Skip node logic in pen mode to allow drawing over them
            hit = NULL;
          } else if (hit->is_container && !hit->image_path[0]) {
            // Click on empty container "+" -> open image file dialog
            ui->selected_node = hit;
#ifdef __EMSCRIPTEN__
            ui->deferred_image_dialog   = 1;
            ui->deferred_image_block_id = block->id;
            ui->deferred_image_node_id  = hit->id;
#else
            char path[512] = {0};
            if (open_image_file_dialog(path, sizeof(path))) {
              char rel[256] = {0};
              if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
                strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
                hit->image_path[sizeof(hit->image_path) - 1] = '\0';
              }
            }
#endif
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
                !(ui->input.mode == INTERACTION_CROP_IMAGE && hit->type == NODE_TYPE_IMAGE)) {
              ui->selected_node = hit;
              // In bezier mode, only select — don't drag so handles take
              // priority
              if (!ui->input.bezier_mode) {
                ui->input.mode = INTERACTION_DRAG_NODE;
                ui->input.drag_offset_x = design_x - hit->x;
                ui->input.drag_offset_y = design_y - hit->y;
                ui->input.start_x = mx;
                ui->input.start_y = my;
              }
            }
          }
        } else {
          ui->selected_node = NULL;
          ui->selected_count = 0;
          ui->input.mode = INTERACTION_NONE; // stop cropping
          if (ui->editing_node) {
            strncpy(ui->editing_node->text, ui->edit_buffer,
                    sizeof(ui->editing_node->text) - 1);
            ui->editing_node = NULL;
          }
          // Start panning in no_limits mode when clicking background
          if (dd->no_limits && ui->editor.active_tool != CANVAS_TOOL_PEN &&
              !ui->block_drag.is_active) {
            ui->input.mode = INTERACTION_PAN;
            ui->input.start_x = mx;
            ui->input.start_y = my;
            ui->input.start_cam_x = ui->editor.cam_x;
            ui->input.start_cam_y = ui->editor.cam_y;
          }
        }
      }

      // Handle image cropping dragging (active until user clicks crop btn
      // again)
      if (ui->input.mode == INTERACTION_CROP_IMAGE && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_IMAGE && is_active_design) {
        Node *sel = ui->selected_node;
        // Init start pos on each new press
        if (ui->input.mouse_just_pressed && mouse_in_design) {
          ui->input.start_x = mx;
          ui->input.start_y = my;
          ui->input.crop_start_node_x = sel->image_crop_x;
          ui->input.crop_start_node_y = sel->image_crop_y;
        }
        if ((ui->mouse_buttons & 1) &&
            mouse_in_design &&
            (ui->input.start_x != 0 || ui->input.start_y != 0)) {
          float dx = (mx - ui->input.start_x) / scale;
          float dy = (my - ui->input.start_y) / scale;
          sel->image_crop_x = ui->input.crop_start_node_x + dx;
          sel->image_crop_y = ui->input.crop_start_node_y + dy;
        }
        // Crop mode stays active — toggled off only via the button in
        // Properties
      }
      // Handle node dragging
      else if ((ui->input.mode == INTERACTION_DRAG_NODE) && ui->selected_node && is_active_design) {
        // 5px threshold to prevent accidental drag on simple click
        float pdx = mx - ui->input.start_x;
        float pdy = my - ui->input.start_y;
        if (fabsf(pdx) < 5.0f && fabsf(pdy) < 5.0f) {
          if (ui->input.mouse_just_released)
            ui->input.mode = INTERACTION_NONE;
          goto after_drag;
        }

        float design_x = (mx - bounds.x) / scale;
        float design_y = (my - bounds.y) / scale;
        Node *sel = ui->selected_node;

        float new_x = design_x - ui->input.drag_offset_x;
        float new_y = design_y - ui->input.drag_offset_y;

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
          ui->input.snap_guide_x = snap_sx;
        } else {
          ui->input.snap_guide_x = -1;
        }
        if (fabsf(best_dy) <= snap_tol) {
          new_y += best_dy;
          ui->input.snap_guide_y = snap_sy;
        } else {
          ui->input.snap_guide_y = -1;
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

        if (ui->input.mouse_just_released) {
          ui->input.mode = INTERACTION_NONE;
          ui->input.snap_guide_x = -1;
          ui->input.snap_guide_y = -1;
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
      if (ui->input.mouse_just_pressed && !in_toolbar && !in_forms_menu &&
          is_active_design) {
      }

      // --- PEN TOOL logic (Adapted for Design) ---
      if (ui->editor.active_tool == CANVAS_TOOL_PEN && is_active_design && !in_toolbar &&
          !in_forms_menu && ((ui->mouse_buttons & 1))) {
        float wx = (mx - bounds.x) / scale;
        float wy = (my - bounds.y) / scale;

        if (ui->input.mouse_just_pressed) {
          ui->input.current_stroke = (Stroke *)calloc(1, sizeof(Stroke));
          ui->input.current_stroke->capacity = 128;
          ui->input.current_stroke->points = (Point *)calloc(128, sizeof(Point));
          if (ui->editor.pen_is_marker) {
            ui->input.current_stroke->thickness = ui->editor.pen_thickness * 10.0f;
            uint32_t c = ui->editor.selected_color;
            uint8_t r = (c >> 24) & 0xFF;
            uint8_t g = (c >> 16) & 0xFF;
            uint8_t b = (c >> 8) & 0xFF;
            uint8_t a = c & 0xFF;
            // Reduce alpha to 30% for marker effect
            ui->input.current_stroke->color =
                (r << 24) | (g << 16) | (b << 8) | (uint8_t)(a * 0.3f);
          } else {
            ui->input.current_stroke->thickness = ui->editor.pen_thickness;
            ui->input.current_stroke->color = ui->editor.selected_color;
          }
          ui->input.current_stroke->points[0].x = wx;
          ui->input.current_stroke->points[0].y = wy;
          ui->input.current_stroke->count = 1;

          // Wrap it in a Node so it appears in Layers
          Node *node = (Node *)calloc(1, sizeof(Node));
          if (node) {
            node->id = dd->next_node_id++;
            node->type = NODE_TYPE_STROKE;
            node->stroke_data = ui->input.current_stroke;
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
            ui->input.current_stroke_node = node;
            ui->input.stroke_block_id = block->id;

            // Initialize bounds
            node->x = wx;
            node->y = wy;
            node->width = 1.0f;
            node->height = 1.0f;
          }
        } else if (ui->input.current_stroke && ui->input.stroke_block_id == block->id) {
          if (ui->input.current_stroke->count >= ui->input.current_stroke->capacity) {
            ui->input.current_stroke->capacity *= 2;
            ui->input.current_stroke->points =
                (Point *)realloc(ui->input.current_stroke->points,
                                 ui->input.current_stroke->capacity * sizeof(Point));
          }
          Point last =
              ui->input.current_stroke->points[ui->input.current_stroke->count - 1];
          if ((wx - last.x) * (wx - last.x) + (wy - last.y) * (wy - last.y) >
              4.0f) {
            ui->input.current_stroke->points[ui->input.current_stroke->count].x = wx;
            ui->input.current_stroke->points[ui->input.current_stroke->count].y = wy;
            ui->input.current_stroke->count++;

            // Update node bounds for culling
            if (ui->input.current_stroke_node) {
              float min_x = ui->input.current_stroke_node->x;
              float min_y = ui->input.current_stroke_node->y;
              float max_x = min_x + ui->input.current_stroke_node->width;
              float max_y = min_y + ui->input.current_stroke_node->height;

              if (wx < min_x)
                min_x = wx;
              if (wy < min_y)
                min_y = wy;
              if (wx > max_x)
                max_x = wx;
              if (wy > max_y)
                max_y = wy;

              ui->input.current_stroke_node->x = min_x;
              ui->input.current_stroke_node->y = min_y;
              ui->input.current_stroke_node->width = max_x - min_x;
              ui->input.current_stroke_node->height = max_y - min_y;
            }
          }
        }
      } else if (!(ui->mouse_buttons & 1)) {
        ui->input.current_stroke = NULL;
        ui->input.stroke_block_id = 0;
      }

      // --- Active Stroke Rendering ---
      if (ui->input.current_stroke && ui->input.current_stroke->count > 1 &&
          ui->input.stroke_block_id == block->id) {
        if (ui->editor.pen_is_marker) {
          // Simplification, just draw the lines for now. Arrow heads will be
          // parsed on commit.
          {
            NVGcolor col =
                nvgRGBA((ui->input.current_stroke->color >> 24) & 0xFF,
                        (ui->input.current_stroke->color >> 16) & 0xFF,
                        (ui->input.current_stroke->color >> 8) & 0xFF,
                        ui->input.current_stroke->color & 0xFF);
            for (int i = 0; i < ui->input.current_stroke->count - 1; i++) {
              Point p1 = ui->input.current_stroke->points[i];
              Point p2 = ui->input.current_stroke->points[i + 1];
              nvgBeginPath(ui->vg);
              nvgMoveTo(ui->vg, bounds.x + p1.x * scale,
                        bounds.y + p1.y * scale);
              nvgLineTo(ui->vg, bounds.x + p2.x * scale,
                        bounds.y + p2.y * scale);
              nvgStrokeColor(ui->vg, col);
              nvgStrokeWidth(ui->vg, ui->input.current_stroke->thickness * scale);
              nvgStroke(ui->vg);
            }
          }
        } else {
          NVGcolor col =
              nvgRGBA((ui->input.current_stroke->color >> 24) & 0xFF,
                      (ui->input.current_stroke->color >> 16) & 0xFF,
                      (ui->input.current_stroke->color >> 8) & 0xFF,
                      ui->input.current_stroke->color & 0xFF);
          for (int i = 0; i < ui->input.current_stroke->count - 1; i++) {
            Point p1 = ui->input.current_stroke->points[i];
            Point p2 = ui->input.current_stroke->points[i + 1];
            nvgBeginPath(ui->vg);
            nvgMoveTo(ui->vg, bounds.x + p1.x * scale,
                      bounds.y + p1.y * scale);
            nvgLineTo(ui->vg, bounds.x + p2.x * scale,
                      bounds.y + p2.y * scale);
            nvgStrokeColor(ui->vg, col);
            nvgStrokeWidth(ui->vg, ui->input.current_stroke->thickness * scale);
            nvgStroke(ui->vg);
          }
        }
      }

      // --- Bezier Handles Rendering Overlay ---
      if (ui->input.bezier_mode && is_active_design && ui->selected_node &&
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
      if ((ui->input.mode == INTERACTION_DRAG_NODE) && ui->selected_node) {
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
      if (ui->input.snap_guide_x >= 0 || ui->input.snap_guide_y >= 0) {
        NVGcolor snap_col = nvgRGBA(0, 195, 215, 200);
        if (ui->input.snap_guide_x >= 0) {
          nvgBeginPath(ui->vg);
          nvgMoveTo(ui->vg, ui->input.snap_guide_x, bounds.y);
          nvgLineTo(ui->vg, ui->input.snap_guide_x, bounds.y + bounds.h);
          nvgStrokeColor(ui->vg, snap_col);
          nvgStrokeWidth(ui->vg, 1.0f);
          nvgStroke(ui->vg);
        }
        if (ui->input.snap_guide_y >= 0) {
          nvgBeginPath(ui->vg);
          nvgMoveTo(ui->vg, bounds.x, ui->input.snap_guide_y);
          nvgLineTo(ui->vg, bounds.x + bounds.w, ui->input.snap_guide_y);
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

    // Topbar tools are now rendered globally by ui_host.c

    // --- Zoom / Pan controls for no_limits mode ---
    if (dd->no_limits) {

      int in_ws = (mx >= ws_bounds.x && mx < ws_bounds.x + ws_bounds.w &&
                   my >= ws_bounds.y && my < ws_bounds.y + ws_bounds.h);
      // Middle-mouse pan
      if (in_ws && is_active_design) {
        if ((ui->mouse_buttons & 4) && ui->input.mode != INTERACTION_PAN) {
          // Start middle-mouse pan
          ui->input.mode = INTERACTION_PAN;
          ui->input.start_x = mx;
          ui->input.start_y = my;
          ui->input.start_cam_x = ui->editor.cam_x;
          ui->input.start_cam_y = ui->editor.cam_y;
        }
      }

      // Scroll wheel: zoom centered on cursor, pan with shift
      if (in_ws) {
        float scroll_y = ui->nvgw.scroll_y;
        float scroll_x = ui->nvgw.scroll_x;
        if (scroll_y != 0.0f || scroll_x != 0.0f) {
          SDL_Keymod mods = SDL_GetModState();
          int shift_held = (mods & KMOD_SHIFT) != 0;

          if (shift_held) {
            // Shift+scroll = pan horizontal
            float zm = ui->editor.zoom > 0.01f ? ui->editor.zoom : 1.0f;
            ui->editor.cam_x -= scroll_y * 30.0f / zm;
          } else if (scroll_x != 0.0f && scroll_y == 0.0f) {
            // Pure horizontal scroll = pan horizontal
            float zm = ui->editor.zoom > 0.01f ? ui->editor.zoom : 1.0f;
            ui->editor.cam_x -= scroll_x * 30.0f / zm;
          } else {
            // Vertical scroll = zoom around cursor
            float old_zoom = ui->editor.zoom;
            float factor = (scroll_y > 0) ? 1.12f : (1.0f / 1.12f);
            // Clamp accumulated scroll for large deltas
            float steps = fabsf(scroll_y);
            if (steps > 3.0f) steps = 3.0f;
            for (int _s = 0; _s < (int)steps; _s++)
              ui->editor.zoom *= factor;
            if (ui->editor.zoom < 0.05f) ui->editor.zoom = 0.05f;
            if (ui->editor.zoom > 10.0f) ui->editor.zoom = 10.0f;
            // Zoom around cursor
            float cur_x = mx - ws_bounds.x;
            float cur_y = my - ws_bounds.y;
            ui->editor.cam_x += cur_x / old_zoom - cur_x / ui->editor.zoom;
            ui->editor.cam_y += cur_y / old_zoom - cur_y / ui->editor.zoom;
          }
          // Consume scroll so table/other panels don't also react
          ui->nvgw.scroll_y = 0;
          ui->nvgw.scroll_x = 0;
          ui_request_redraw(ui);
        }
      }

      // --- Zoom control widget (bottom-right corner) ---
      {
        NVGcontext *vg = ui->vg;
        NvgWidgetCtx *wc = &ui->nvgw;
        float btn_w = 28.0f, btn_h = 28.0f;
        float lbl_w = 52.0f;
        float bar_w = btn_w * 2 + lbl_w + 2.0f;
        float bar_h = btn_h;
        float bx = ws_bounds.x + ws_bounds.w - bar_w - 12.0f;
        float by = ws_bounds.y + ws_bounds.h - bar_h - 12.0f;

        // Background pill
        nvgBeginPath(vg);
        nvgRoundedRect(vg, bx, by, bar_w, bar_h, 6.0f);
        nvgFillColor(vg, nvgRGBA(36, 36, 39, 220));
        nvgFill(vg);

        // Separator lines between sections
        nvgBeginPath(vg);
        nvgMoveTo(vg, bx + btn_w, by + 6);
        nvgLineTo(vg, bx + btn_w, by + bar_h - 6);
        nvgMoveTo(vg, bx + btn_w + lbl_w, by + 6);
        nvgLineTo(vg, bx + btn_w + lbl_w, by + bar_h - 6);
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 25));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        // Hit test helpers (panel-local coords = mx, my)
        float lmx = mx, lmy = my;
        int clicked = wc->mouse_clicked;

        // Minus button hover/click
        int hov_minus = (lmx >= bx && lmx < bx + btn_w &&
                         lmy >= by && lmy < by + bar_h);
        if (hov_minus) {
          nvgBeginPath(vg);
          nvgRoundedRect(vg, bx, by, btn_w, bar_h, 6.0f);
          nvgFillColor(vg, nvgRGBA(255, 255, 255, clicked ? 40 : 18));
          nvgFill(vg);
          if (clicked) {
            ui->editor.zoom /= 1.2f;
            if (ui->editor.zoom < 0.05f) ui->editor.zoom = 0.05f;
            ui_request_redraw(ui);
          }
        }

        // Plus button hover/click
        float px_x = bx + btn_w + lbl_w;
        int hov_plus = (lmx >= px_x && lmx < px_x + btn_w &&
                        lmy >= by && lmy < by + bar_h);
        if (hov_plus) {
          nvgBeginPath(vg);
          nvgRoundedRect(vg, px_x, by, btn_w, bar_h, 6.0f);
          nvgFillColor(vg, nvgRGBA(255, 255, 255, clicked ? 40 : 18));
          nvgFill(vg);
          if (clicked) {
            ui->editor.zoom *= 1.2f;
            if (ui->editor.zoom > 10.0f) ui->editor.zoom = 10.0f;
            ui_request_redraw(ui);
          }
        }

        // Label center: click to reset to 100%
        float lx = bx + btn_w;
        int hov_lbl = (lmx >= lx && lmx < lx + lbl_w &&
                       lmy >= by && lmy < by + bar_h);
        if (hov_lbl) {
          nvgBeginPath(vg);
          nvgRect(vg, lx, by, lbl_w, bar_h);
          nvgFillColor(vg, nvgRGBA(255, 255, 255, clicked ? 40 : 14));
          nvgFill(vg);
          if (clicked) {
            ui->editor.zoom = 1.0f;
            ui_request_redraw(ui);
          }
        }

        // Draw icons and label
        nvgFontFace(vg, "Inter");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 200));

        // Minus symbol
        nvgFontSize(vg, 16.0f);
        nvgText(vg, bx + btn_w * 0.5f, by + bar_h * 0.5f, "\xe2\x88\x92", NULL); // − (minus sign)

        // Plus symbol
        nvgFontSize(vg, 16.0f);
        nvgText(vg, px_x + btn_w * 0.5f, by + bar_h * 0.5f, "+", NULL);

        // Zoom percentage
        char zoom_str[16];
        snprintf(zoom_str, sizeof(zoom_str), "%d%%", (int)(ui->editor.zoom * 100));
        nvgFontSize(vg, 11.0f);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, hov_lbl ? 230 : 180));
        nvgText(vg, lx + lbl_w * 0.5f, by + bar_h * 0.5f, zoom_str, NULL);
      }

      // Save camera state back to DesignData
      dd->view_x = ui->editor.cam_x;
      dd->view_y = ui->editor.cam_y;
      dd->zoom = ui->editor.zoom;
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
