#include "ui_properties.h"
#include "ui_canvas.h"
#include "ui_shared.h"
#include "ui_host.h"
#include "../document/aslan_table.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
static const char *node_type_display_name(NodeType type) {
  switch (type) {
  case NODE_TYPE_RECT:
    return "Rectangle";
  case NODE_TYPE_ELLIPSE:
    return "Ellipse";
  case NODE_TYPE_TRIANGLE:
    return "Triangle";
  case NODE_TYPE_PENTAGON:
    return "Pentagon";
  case NODE_TYPE_HEXAGON:
    return "Hexagon";
  case NODE_TYPE_OCTAGON:
    return "Octagon";
  case NODE_TYPE_HEART:
    return "Heart";
  case NODE_TYPE_LINE:
    return "Line";
  case NODE_TYPE_PATH:
    return "Path";
  case NODE_TYPE_TEXT:
    return "Text";
  case NODE_TYPE_IMAGE:
    return "Image";
  case NODE_TYPE_STROKE:
    return "Stroke";
  case NODE_TYPE_CHART:
    return "Chart";
  case NODE_TYPE_FRAME:
    return "Frame";
  case NODE_TYPE_GROUP:
    return "Group";
  default:
    return "Element";
  }
}


static void ui_render_layer_item(UIHost *ui, Block *block, Node *node,
                                 int depth, float x, float *cy, float w) {
  if (!node)
    return;
  NvgWidgetCtx *nvgw = &ui->nvgw;
  float row_h = 30.0f;
  float current_y = *cy;
  *cy += row_h;

  int is_selected = (ui->selected_node == node);
  int is_renaming = (ui->renaming_node_id == node->id);

  float indent_w = 4.0f + (float)depth * 16.0f;

  float lmx = nvgw->mouse_x - nvgw->offset_x;
  float lmy = nvgw->mouse_y - nvgw->offset_y;
  int is_hovered =
      (lmx >= x && lmx < x + w &&
       lmy >= current_y && lmy < current_y + row_h);

  // Background
  if (is_selected) {
    nvgBeginPath(ui->vg);
    nvgRect(ui->vg, x, current_y, w, row_h);
    nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 15));
    nvgFill(ui->vg);

    // Left accent bar
    nvgBeginPath(ui->vg);
    nvgRect(ui->vg, x, current_y + 4, 3, row_h - 8);
    nvgFillColor(ui->vg, nvgRGBA(60, 110, 180, 255)); // COLOR_ACCENT approx
    nvgFill(ui->vg);
  } else if (is_hovered) {
    nvgBeginPath(ui->vg);
    nvgRect(ui->vg, x, current_y, w, row_h);
    nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 8));
    nvgFill(ui->vg);
  }

  if (is_hovered && nvgw->mouse_clicked) {
    ui->selected_node = node;
    // Start drag
    ui->sidebar_drag_node_id = node->id;
    ui->sidebar_drag_started = 0;
  }

  // Track drag movement
  if (ui->sidebar_drag_node_id == node->id && nvgw->mouse_down && !nvgw->mouse_clicked) {
    ui->sidebar_drag_started = 1;
  }

  // During drag: check if mouse is over this row to mark as drop target
  int is_drag_active = (ui->sidebar_drag_started &&
                        ui->sidebar_drag_node_id != 0 &&
                        ui->sidebar_drag_node_id != node->id);
  if (is_drag_active && is_hovered) {
    ui->sidebar_drop_target_id = node->id;
    ui->sidebar_drag_drop_above = (lmy < current_y + row_h * 0.5f) ? 1 : 0;
  }

  // Draw drop indicator line
  int is_drop_target = (ui->sidebar_drop_target_id == node->id && is_drag_active);
  if (is_drop_target) {
    float line_y = ui->sidebar_drag_drop_above
                       ? current_y
                       : current_y + row_h;
    nvgBeginPath(ui->vg);
    nvgMoveTo(ui->vg, x + 4, line_y);
    nvgLineTo(ui->vg, x + w - 4, line_y);
    nvgStrokeColor(ui->vg, nvgRGB(60, 110, 180));
    nvgStrokeWidth(ui->vg, 2.0f);
    nvgStroke(ui->vg);
  }


  float cur_x = x + indent_w;

  // Fold toggle
  if (node->is_container && node->first_child) {
    if (nvgw_button_icon(nvgw, node->id ^ 0xF01D, cur_x, current_y + 7, 16, 16,
                         node->folded
                             ? "\xEF\x84\x85"
                             : "\xEF\x84\x87")) { // ICON_EXPAND/COLLAPSE
      node->folded = !node->folded;
    }
  }
  cur_x += 16.0f;

  // Name
  float name_w =
      w - (cur_x - x) - 24 * 3; // remaining space minus right buttons
  if (is_renaming) {
    if (nvgw_text_input(nvgw, node->id, cur_x, current_y + 4, name_w, 22,
                        ui->node_rename_buffer, 63, 14, false)) {
      strncpy(node->name, ui->node_rename_buffer, 63);
      ui->renaming_node_id = 0;
    }
  } else {
    const char *display =
        node->name[0] ? node->name : node_type_display_name(node->type);
    nvgw_label(nvgw, cur_x, current_y, name_w, row_h, display,
               NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE,
               is_selected ? nvgRGB(60, 60, 75) : nvgRGB(80, 80, 95), 14);
  }
  cur_x += name_w;

  // Visibility
  if (nvgw_button_icon(nvgw, node->id ^ 0x1111, cur_x, current_y + 3, 24, 24,
                       node->visible ? ICON_EYE : ICON_EYE_OFF)) {
    node->visible = !node->visible;
  }
  cur_x += 24.0f;

  // Lock
  if (nvgw_button_icon(nvgw, node->id ^ 0x2222, cur_x, current_y + 3, 24, 24,
                       node->locked ? ICON_LOCK : ICON_LOCK_UNLOCK)) {
    node->locked = !node->locked;
  }
  cur_x += 24.0f;

  // Trash
  if (nvgw_button_icon(nvgw, node->id ^ 0x3333, cur_x, current_y + 3, 24, 24,
                       "\xEE\xB0\xAA")) {
    if (ui->selected_node == node)
      ui->selected_node = NULL;
    block_design_delete_node(block, node->id);
    return;
  }

  // Recursive
  if (node->is_container && !node->folded) {
    Node *list[128];
    int c = 0;
    for (Node *ch = node->first_child; ch && c < 128; ch = ch->next_sibling)
      list[c++] = ch;
    for (int i = c - 1; i >= 0; i--) {
      ui_render_layer_item(ui, block, list[i], depth + 1, x, cy, w);
    }
  }
}

void ui_render_sidebar_layers(UIHost *ui, Block *block, float x, float y,
                              float w, float h) {
  (void)h;
  Node *root = NULL;
  if (block->type == BLOCK_DESIGN)
    root = block->data.design.root;

  if (!root)
    return;

  ui->hovered_node_id = 0;

  // Reset drop target each frame so it tracks current mouse position
  if (ui->sidebar_drag_started)
    ui->sidebar_drop_target_id = 0;

  Node *nodes[256];
  int count = 0;
  for (Node *node = root->first_child; node && count < 256;
       node = node->next_sibling) {
    nodes[count++] = node;
  }

  float cy = y;
  for (int i = count - 1; i >= 0; i--) {
    ui_render_layer_item(ui, block, nodes[i], 0, x, &cy, w);
  }

  // Perform reorder after full traversal so drop_target_id is set correctly
  if (ui->nvgw.mouse_released && ui->sidebar_drag_started &&
      ui->sidebar_drag_node_id != 0 && ui->sidebar_drop_target_id != 0 &&
      ui->sidebar_drag_node_id != ui->sidebar_drop_target_id) {
    Node *dragged = NULL, *target = NULL;
    for (Node *n = root->first_child; n; n = n->next_sibling) {
      if (n->id == ui->sidebar_drag_node_id)   dragged = n;
      if (n->id == ui->sidebar_drop_target_id) target  = n;
    }
    if (dragged && target) {
      // Panel is reversed: top of panel = end of list (drawn last = on top).
      // "above in panel" means later in list → node_insert_after
      // "below in panel" means earlier in list → node_insert_before
      if (ui->sidebar_drag_drop_above)
        node_insert_after(dragged, target);
      else
        node_insert_before(dragged, target);
    }
  }

  if (ui->nvgw.mouse_released) {
    ui->sidebar_drag_node_id  = 0;
    ui->sidebar_drag_started  = 0;
    ui->sidebar_drop_target_id = 0;
  }
}


void design_insert_tableau_layout(DesignData *dd) {
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


// Font IDs (must match clay_host constants, now used standalone)
#define ASLAN_FONT_INTER 0
#define ASLAN_FONT_BOLD 1
#define ASLAN_FONT_ICONS 2

void design_render_sidebar(UIHost *ui, Engine *engine,
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

// =============================================================================
