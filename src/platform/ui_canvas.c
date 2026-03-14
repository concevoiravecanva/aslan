// =============================================================================
// ASLAN UI CANVAS - Infinite Canvas Block Rendering and Tools
// =============================================================================

#include "ui_canvas.h"
#include "../document/aslan_table.h"
#include "image_loader.h"
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif
#define NANOVG_GL3
#include "../vendor/nanovg/nanovg_gl.h"

#include "ui_shared.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations for external functions (will be in ui_host.h eventually)
int open_image_file_dialog(char *out_path, size_t path_size);
int copy_image_to_assets(const char *src_path, const char *dest_dir,
                         char *out_rel_path, size_t rel_path_size);
int mac_get_image_size(const char *path, int *w, int *h);

// Forward declarations for internal functions
void node_convert_to_path(Node *node);
int tessellate_bezier(float x1, float y1, float cp1x, float cp1y, float cp2x,
                      float cp2y, float x2, float y2, float *out_pts,
                      int max_pts, int include_last);

// -----------------------------------------------------------------------------
// Shared Helper: Draw alignment center guides when dragging a node
// -----------------------------------------------------------------------------

void draw_alignment_guides(UIHost *ui, Node *node, float area_cx, float area_cy,
                           float area_x, float area_y, float area_w,
                           float area_h, float node_sx, float node_sy,
                           float node_sw, float node_sh) {
  if (!node)
    return;
  NVGcolor guide_col = nvgRGBA(110, 110, 130, 150);
  float tolerance = 3.0f;

  float node_cx = node_sx + node_sw / 2.0f;
  float node_cy = node_sy + node_sh / 2.0f;

  nvgBeginPath(ui->vg);
  // Vertical center guide
  if (fabsf(node_cx - area_cx) < tolerance) {
    nvgMoveTo(ui->vg, area_cx, area_y);
    nvgLineTo(ui->vg, area_cx, area_y + area_h);
  }
  // Horizontal center guide
  if (fabsf(node_cy - area_cy) < tolerance) {
    nvgMoveTo(ui->vg, area_x, area_cy);
    nvgLineTo(ui->vg, area_x + area_w, area_cy);
  }
  nvgStrokeColor(ui->vg, guide_col);
  nvgStrokeWidth(ui->vg, 1.0f);
  nvgStroke(ui->vg);
}

// -----------------------------------------------------------------------------
// Shared Helper: Render right-click context menu for nodes
// -----------------------------------------------------------------------------

void render_node_context_menu(UIHost *ui, Block *block) {
  if (!ui->show_context_menu || !ui->context_menu_node)
    return;

  Node *node = ui->context_menu_node;
  const float menu_w = 150.0f;
  const float item_h = 30.0f;
  const float padding = 6.0f;
  int is_group = (node->type == NODE_TYPE_GROUP);
  int item_count = is_group ? 5 : 4;
  float menu_h = item_count * item_h + 2 * padding;
  float mx = ui->context_menu_x;
  float my = ui->context_menu_y;

  // Shadow
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, mx + 2, my + 3, menu_w, menu_h, 8.0f);
  nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 25));
  nvgFill(ui->vg);
  // Background
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, mx, my, menu_w, menu_h, 8.0f);
  nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
  nvgFill(ui->vg);
  nvgStrokeColor(ui->vg, nvgRGBA(220, 220, 220, 255));
  nvgStrokeWidth(ui->vg, 1.0f);
  nvgStroke(ui->vg);

  const char *labels[] = {"Duplicate", "Delete", "Bring to Front",
                          "Send to Back", "Ungroup"};

  if (ui->font_inter) {
    nvgFontSize(ui->vg, ui->font_size_px);
    nvgFontFace(ui->vg, "Inter");
  }

  for (int i = 0; i < item_count; i++) {
    float ir_x = mx + padding, ir_y = my + padding + i * item_h;
    float ir_w = menu_w - 2 * padding, ir_h = item_h;
    int hovered =
        (ui->mouse_x >= ir_x && ui->mouse_x < ir_x + ir_w &&
         ui->mouse_y >= ir_y && ui->mouse_y < ir_y + ir_h);

    if (hovered) {
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, ir_x, ir_y, ir_w, ir_h, 4.0f);
      nvgFillColor(ui->vg, nvgRGBA(110, 110, 130, 20));
      nvgFill(ui->vg);
    }

    nvgFillColor(ui->vg,
                 i == 1 ? nvgRGBA(220, 50, 50, 255) : nvgRGBA(40, 40, 40, 255));
    nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ui->vg, ir_x + 10, ir_y + item_h / 2.0f, labels[i], NULL);

    if (hovered && (ui->mouse_buttons & 1) &&
        ui->canvas_state.mouse_just_pressed) {
      if (i == 0) {
        // Duplicate
        Node *dup = NULL;
        if (block->type == BLOCK_DESIGN)
          dup = node_create(block->data.design.next_node_id++, node->type,
                            node->name);
        if (dup) {
          dup->x = node->x + 20.0f;
          dup->y = node->y + 20.0f;
          dup->width = node->width;
          dup->height = node->height;
          dup->rotation = node->rotation;
          dup->fill_color = node->fill_color;
          dup->stroke_color = node->stroke_color;
          dup->stroke_width = node->stroke_width;
          dup->corner_radius = node->corner_radius;
          dup->font_size = node->font_size;
          dup->font_type = node->font_type;
          dup->line_start_style = node->line_start_style;
          dup->line_end_style = node->line_end_style;
          dup->visible = 1;
          dup->is_container = node->is_container;
          strncpy(dup->text, node->text, sizeof(dup->text) - 1);
          strncpy(dup->image_path, node->image_path,
                  sizeof(dup->image_path) - 1);
          if (block->type == BLOCK_DESIGN) {
            Node *root = block->data.design.root;
            dup->parent = root;
            node_add_child(root, dup);
            ui->selected_node = dup;
          }
        }
      } else if (i == 1) {
        // Delete
        if (block->type == BLOCK_DESIGN)
          block_design_delete_node(block, node->id);
        ui->selected_node = NULL;
      } else if (i == 2) {
        // Bring to Front (move to end of sibling list)
        node_move_down(node);
        node_move_down(node);
        node_move_down(node);
        node_move_down(node); // Move several times to ensure front
      } else if (i == 3) {
        // Send to Back (move to start of sibling list)
        node_move_up(node);
        node_move_up(node);
        node_move_up(node);
        node_move_up(node);
      } else if (i == 4 && node->type == NODE_TYPE_GROUP &&
                 block->type == BLOCK_DESIGN) {
        // Ungroup: extract children into parent's sibling list
        Node *child = node->first_child;
        while (child) {
          Node *next = child->next_sibling;
          node_remove_from_parent(child);
          child->x += node->x;
          child->y += node->y;
          node_insert_before(child, node);
          child = next;
        }
        block_design_delete_node(block, node->id);
        ui->selected_node = NULL;
        ui->selected_count = 0;
      }
      ui->show_context_menu = 0;
      ui->context_menu_node = NULL;
    }
  }

  // Close menu on left click outside
  if ((ui->mouse_buttons & 1) && ui->canvas_state.mouse_just_pressed) {
    int in_menu = (ui->mouse_x >= mx && ui->mouse_x < mx + menu_w &&
                   ui->mouse_y >= my && ui->mouse_y < my + menu_h);
    if (!in_menu) {
      ui->show_context_menu = 0;
      ui->context_menu_node = NULL;
    }
  }
}

// -----------------------------------------------------------------------------
// Canvas Helper: Draw infinite dot grid
// -----------------------------------------------------------------------------

void canvas_draw_dot_grid(UIHost *ui, float bx, float by, float bw, float bh,
                          float cam_x, float cam_y, float zoom) {
  // Grid spacing in world units
  const float grid_spacing = 40.0f;
  // Dot appearance
  const float dot_radius = 1.5f;

  // Calculate visible world bounds
  float world_left = cam_x;
  float world_top = cam_y;
  float world_right = cam_x + bw / zoom;
  float world_bottom = cam_y + bh / zoom;

  // Snap to grid
  float start_x = floorf(world_left / grid_spacing) * grid_spacing;
  float start_y = floorf(world_top / grid_spacing) * grid_spacing;

  // Limit iterations for performance
  int max_dots_x = (int)((bw / (grid_spacing * zoom)) + 2);
  int max_dots_y = (int)((bh / (grid_spacing * zoom)) + 2);
  if (max_dots_x > 100)
    max_dots_x = 100;
  if (max_dots_y > 100)
    max_dots_y = 100;

  nvgBeginPath(ui->vg);
  nvgFillColor(ui->vg, nvgRGBA(80, 82, 86, 255));

  for (int iy = 0; iy < max_dots_y; iy++) {
    float world_y = start_y + iy * grid_spacing;
    if (world_y > world_bottom)
      break;

    float screen_y = by + (world_y - cam_y) * zoom;

    for (int ix = 0; ix < max_dots_x; ix++) {
      float world_x = start_x + ix * grid_spacing;
      if (world_x > world_right)
        break;

      float screen_x = bx + (world_x - cam_x) * zoom;

      // Draw dot as small filled circle
      nvgCircle(ui->vg, screen_x, screen_y, dot_radius);
    }
  }
  nvgFill(ui->vg);
}

// -----------------------------------------------------------------------------
// Canvas Helper: Check if point is in toolbar area
// -----------------------------------------------------------------------------

int canvas_point_in_toolbar(float bx, float by, float bw, float bh,
                            float mx, float my, CanvasInteraction *cs) {
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  const float toolbar_w = 7 * btn_size + 6 * btn_spacing + 24.0f; // 7 buttons
  const float toolbar_h = 52.0f;
  const float margin = 20.0f;

  float toolbar_x = bx + (bw - toolbar_w) / 2.0f;
  float toolbar_y = by + bh - toolbar_h - margin;

  // Basic toolbar hit
  if (mx >= toolbar_x && mx < toolbar_x + toolbar_w && my >= toolbar_y &&
      my < toolbar_y + toolbar_h)
    return 1;

  // Sub-menu hit
  if (cs->show_sub_menu) {
    const float menu_w = 140.0f;
    const float item_h = 32.0f;
    const float menu_padding = 8.0f;
    int items = (cs->active_cat == CANVAS_CAT_SHAPES) ? 4 : 3;
    float menu_h = items * item_h + 2 * menu_padding;

    // Calculate which button is active to find menu_x
    float btn_x_start = toolbar_x + 12.0f;
    float active_btn_x =
        btn_x_start + (int)cs->active_cat * (btn_size + btn_spacing);
    float menu_x = active_btn_x - menu_w / 2.0f + 16.0f;
    float menu_y = toolbar_y - menu_h - 12.0f;

    if (mx >= menu_x && mx < menu_x + menu_w && my >= menu_y &&
        my < menu_y + menu_h)
      return 1;
  }

  return 0;
}

// -----------------------------------------------------------------------------
// Canvas Helper: Render floating toolbar (Apple Freeform style)
// -----------------------------------------------------------------------------

// Check if tool is a form tool
int is_form_tool(CanvasToolMode tool) {
  return tool == CANVAS_TOOL_FORM_RECT || tool == CANVAS_TOOL_FORM_ELLIPSE ||
         tool == CANVAS_TOOL_FORM_TRIANGLE || tool == CANVAS_TOOL_FORM_LINE;
}

void canvas_render_sub_menu(UIHost *ui, float x, float y, CanvasCategory cat) {
  CanvasInteraction *cs = &ui->canvas_state;
  const float item_h = 32.0f;
  const float menu_padding = 8.0f;
  const float menu_w = 140.0f;
  int item_count = 0;

  // Define sub-options based on category
  const char *items[8];
  const char *icons[8];
  CanvasSubOption sub_opts[8];

  if (cat == CANVAS_CAT_SELECT) {
    // Pan is now automatic when dragging background, so no menu needed
    item_count = 0; // Hide the menu entirely
  } else if (cat == CANVAS_CAT_SHAPES) {
    items[0] = "Rectangle";
    icons[0] = ICON_SHAPE;
    sub_opts[0] = CANVAS_SUB_SHAPE_RECT;
    items[1] = "Ellipse";
    icons[1] = "\xEE\xAD\xA4";
    sub_opts[1] = CANVAS_SUB_SHAPE_ELLIPSE; // Re-use shape for now
    items[2] = "Triangle";
    icons[2] = ICON_SHAPE;
    sub_opts[2] = CANVAS_SUB_SHAPE_TRIANGLE;
    items[3] = "Line";
    icons[3] = "\xEF\x84\xA5";
    sub_opts[3] = CANVAS_SUB_SHAPE_LINE;
    item_count = 4;
  } else if (cat == CANVAS_CAT_TEXT) {
    items[0] = "Heading";
    icons[0] = ICON_H1;
    sub_opts[0] = CANVAS_SUB_TEXT_HEADING;
    items[1] = "Body";
    icons[1] = ICON_TEXT;
    sub_opts[1] = CANVAS_SUB_TEXT_BODY;
    items[2] = "Sticky";
    icons[2] = ICON_STICKY;
    sub_opts[2] = CANVAS_SUB_TEXT_STICKY;
    item_count = 3;
  } else if (cat == CANVAS_CAT_MEDIA) {
    items[0] = "Image";
    icons[0] = ICON_IMAGE;
    sub_opts[0] = CANVAS_SUB_MEDIA_IMAGE;
    item_count = 1;
  }

  if (item_count == 0)
    return;

  float menu_h = item_count * item_h + 2 * menu_padding;
  float menu_x = x - menu_w / 2.0f + 16.0f; // Center relative to button
  float menu_y = y - menu_h - 12.0f;

  // Shadow
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, menu_x + 2, menu_y + 4, menu_w, menu_h, 12.0f);
  nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 30));
  nvgFill(ui->vg);

  // Background
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, menu_x, menu_y, menu_w, menu_h, 12.0f);
  nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
  nvgFill(ui->vg);
  nvgStrokeColor(ui->vg, nvgRGBA(230, 230, 230, 255));
  nvgStrokeWidth(ui->vg, 1.0f);
  nvgStroke(ui->vg);

  for (int i = 0; i < item_count; i++) {
    float ir_x = menu_x + menu_padding, ir_y = menu_y + menu_padding + i * item_h;
    float ir_w = menu_w - 2 * menu_padding, ir_h = item_h;
    int hovered =
        (ui->mouse_x >= ir_x && ui->mouse_x < ir_x + ir_w &&
         ui->mouse_y >= ir_y && ui->mouse_y < ir_y + ir_h);

    if (hovered) {
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, ir_x, ir_y, ir_w, ir_h, 6.0f);
      nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 15));
      nvgFill(ui->vg);

      if (cs->mouse_just_pressed) {
        // Handle sub-option selection
        if (cat == CANVAS_CAT_SHAPES) {
          // Set pending_create to 10-13 for shapes
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          if (sub_opts[i] == CANVAS_SUB_SHAPE_RECT)
            cs->pending_create = 10;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_ELLIPSE)
            cs->pending_create = 11;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_TRIANGLE)
            cs->pending_create = 12;
          else if (sub_opts[i] == CANVAS_SUB_SHAPE_LINE)
            cs->pending_create = 13;
        } else if (cat == CANVAS_CAT_TEXT) {
          cs->tool = CANVAS_TOOL_SELECT;
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          // H1=6, Body=1, Sticky=2
          if (sub_opts[i] == CANVAS_SUB_TEXT_HEADING)
            cs->pending_create = 6;
          else if (sub_opts[i] == CANVAS_SUB_TEXT_BODY)
            cs->pending_create = 1;
          else if (sub_opts[i] == CANVAS_SUB_TEXT_STICKY)
            cs->pending_create = 2;
        } else if (cat == CANVAS_CAT_MEDIA) {
          cs->tool = CANVAS_TOOL_SELECT;
          cs->space_held = 0;    // Disable pan mode
          cs->pan_from_menu = 0; // Clear menu pan
          cs->is_panning = 0;    // Stop any active panning
          if (sub_opts[i] == CANVAS_SUB_MEDIA_IMAGE)
            cs->pending_create = 3;
        }
        cs->show_sub_menu = 0;
      }
    }

    // Icon
    if (ui->font_icons) {
      nvgFontSize(ui->vg, 20.0f);
      nvgFontFace(ui->vg, "Icons");
      nvgFillColor(ui->vg, nvgRGBA(80, 80, 80, 255));
      nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ui->vg, ir_x + 8, ir_y + 16, icons[i], NULL);
    }
    // Label
    if (ui->font_inter) {
      nvgFontSize(ui->vg, ui->font_size_px);
      nvgFontFace(ui->vg, "Inter");
      nvgFillColor(ui->vg, nvgRGBA(40, 40, 40, 255));
      nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ui->vg, ir_x + 36, ir_y + 16, items[i], NULL);
    }
  }
}

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

// Helper: get icon for node type
static const char *node_type_icon(Node *node) {
  switch (node->type) {
  case NODE_TYPE_RECT:
    return "\xEE\xBD\xB5"; // ri-rectangle-line (U+EF75)
  case NODE_TYPE_ELLIPSE:
    return "\xEE\xA6\xB1"; // ri-circle-line (U+E9B1)
  case NODE_TYPE_TRIANGLE:
    return "\xEF\x82\x98"; // ri-triangle-line (U+F098)
  case NODE_TYPE_TEXT:
    return ICON_TEXT;
  case NODE_TYPE_IMAGE:
    return ICON_IMAGE;
  case NODE_TYPE_LINE:
    return "\xEE\xBC\xAE"; // ri-separator (U+EF2E)
  case NODE_TYPE_PATH:
    return ICON_MAGIC;
  case NODE_TYPE_STROKE:
    return (node->stroke_data && node->stroke_data->type == STROKE_TYPE_MARKER)
               ? ICON_MARKER
               : ICON_BRUSH;
  case NODE_TYPE_CHART:
    return ICON_CHART;
  case NODE_TYPE_FRAME:
    return ICON_CANVAS;
  case NODE_TYPE_GROUP:
    return ICON_DESIGN;
  default:
    return ICON_SHAPE;
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

  // Type icon
  nvgw_label(nvgw, cur_x, current_y, 24, row_h, node_type_icon(node),
             NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE,
             is_selected ? nvgRGB(60, 110, 180) : nvgRGB(110, 110, 125), 16);
  cur_x += 24.0f;

  // Name
  float name_w =
      w - (cur_x - x) - 24 * 3; // remaining space minus right buttons
  if (is_renaming) {
    if (nvgw_text_input(nvgw, node->id, cur_x, current_y + 4, name_w, 22,
                        ui->node_rename_buffer, 63, 14)) {
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
}

void recursive_convert_to_path(Node *node) {
  if (!node)
    return;

  // Convert current node only if it's a basic
  // geometry shape Note: We avoid converting
  // TEXT or IMAGE or existing PATHs
  if (node->type == NODE_TYPE_RECT || node->type == NODE_TYPE_ELLIPSE ||
      node->type == NODE_TYPE_TRIANGLE || node->type == NODE_TYPE_OCTAGON ||
      node->type == NODE_TYPE_HEART || node->type == NODE_TYPE_PENTAGON ||
      node->type == NODE_TYPE_HEXAGON) {
    node_convert_to_path(node);
  }

  // Recurse to children
  if (node->first_child) {
    recursive_convert_to_path(node->first_child);
  }

  // Recurse to siblings
  if (node->next_sibling) {
    recursive_convert_to_path(node->next_sibling);
  }
}

void canvas_render_toolbar(UIHost *ui, float bx, float by, float bw, float bh,
                           Block *block) {
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  // Design gets an extra "Table layout" button,
  // canvas has one fewer
  int tool_count = (block->type == BLOCK_DESIGN) ? 5 : 4;
  const float toolbar_w =
      tool_count * btn_size + (tool_count - 1) * btn_spacing + 24.0f;
  const float toolbar_h = 52.0f;
  const float margin = 20.0f;

  float toolbar_x = bx + (bw - toolbar_w) / 2.0f;
  float toolbar_y = by + bh - toolbar_h - margin;

  CanvasInteraction *cs = &ui->canvas_state;

  // Shadow
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, toolbar_x + 2, toolbar_y + 4, toolbar_w, toolbar_h,
                 26.0f);
  nvgFillColor(ui->vg, nvgRGBA(0, 0, 0, 20));
  nvgFill(ui->vg);

  // Background
  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, toolbar_x, toolbar_y, toolbar_w, toolbar_h, 26.0f);
  nvgFillColor(ui->vg, nvgRGBA(255, 255, 255, 255));
  nvgFill(ui->vg);
  nvgStrokeColor(ui->vg, nvgRGBA(220, 220, 220, 255));
  nvgStrokeWidth(ui->vg, 1.0f);
  nvgStroke(ui->vg);

  float btn_x = toolbar_x + 12.0f;
  float btn_y = toolbar_y + (toolbar_h - btn_size) / 2.0f;

  // 7 buttons (canvas) or 8 buttons (design):
  // last button is Linked Text
  const char *tool_icons[] = {ICON_SHAPE, ICON_TEXT, ICON_IMAGE, ICON_CHART,
                              "\xEF\x87\x9E"}; // U+F1DE sliders — insert
                                               // table layout

  for (int i = 0; i < tool_count; i++) {
    float btn_ix = btn_x + i * (btn_size + btn_spacing);
    float btn_iy = btn_y;

    // Map button index to category (skip
    // CANVAS_CAT_SELECT)
    int is_active = (cs->active_cat == (CanvasCategory)(i + 1));
    if (i == 3)
      is_active = (cs->pending_create == 11); // Chart active state (transient)
    if (i == 4)
      is_active = 0; // Table layout — one-shot,
                     // never stays active

    float rel_mx = ui->mouse_x - (ui->abs_offset_x + btn_ix);
    float rel_my = ui->mouse_y - (ui->abs_offset_y + btn_iy);
    int hovered = (rel_mx >= 0 && rel_mx < btn_size &&
                   rel_my >= 0 && rel_my < btn_size);

    if (is_active || hovered) {
      NVGcolor bg_color = is_active ? nvgRGBA(110, 110, 130, 25)
                                    : nvgRGBA(0, 0, 0, 10);
      nvgBeginPath(ui->vg);
      nvgRoundedRect(ui->vg, btn_ix, btn_iy, btn_size, btn_size, 8.0f);
      nvgFillColor(ui->vg, bg_color);
      nvgFill(ui->vg);
    }

    NVGcolor icon_color = is_active ? nvgRGBA(110, 110, 130, 255)
                                    : nvgRGB(60, 60, 60);

    {
      nvgFontSize(ui->vg, btn_size - 16);
      nvgFontFaceId(ui->vg, ui->nvgw.font_icons);
      nvgFillColor(ui->vg, icon_color);
      nvgTextAlign(ui->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(ui->vg, btn_ix + btn_size / 2, btn_iy + btn_size / 2,
              tool_icons[i], NULL);
    }

    if (hovered && cs->mouse_just_pressed) {
      CanvasCategory cat = (CanvasCategory)(i + 1); // Skip
                                                    // CANVAS_CAT_SELECT
      if (cat == CANVAS_CAT_SHAPES) {
        // DEFAULT: Just create a rectangle
        cs->pending_create = 10;
        cs->show_sub_menu = 0;
      } else if (cat == CANVAS_CAT_TEXT) {
        // DIRECT ACTION: Create paragraph
        cs->pending_create = 1; // 1 = Body Text
        cs->show_sub_menu = 0;
      } else if (cat == CANVAS_CAT_MEDIA) {
        // DIRECT ACTION: Open image dialog
        cs->pending_create = 3; // 3 = Image
        cs->show_sub_menu = 0;
      } else if (i == 3) { // Chart button
                           // (direct action)
        cs->pending_create = 11;
        cs->show_sub_menu = 0;
      } else if (i == 4) { // Table layout button
                           // (direct action)
        cs->pending_create = 20;
        cs->show_sub_menu = 0;
      } else if (cs->active_cat == cat) {
        cs->show_sub_menu = !cs->show_sub_menu;
      } else {
        cs->active_cat = cat;
        cs->show_sub_menu = 1; // Open sub-menu for others
      }
    }

    // Render sub-menu if active
    if (is_active && cs->show_sub_menu) {
      canvas_render_sub_menu(ui, btn_ix, btn_iy,
                             (CanvasCategory)(i + 1)); // Skip
                                                       // CANVAS_CAT_SELECT
    }
  }
}

// -----------------------------------------------------------------------------
// Canvas Helper: Create new node in canvas
// -----------------------------------------------------------------------------

Node *design_create_styled_node(DesignData *dd, NodeStyleType style, float x,
                                float y, float w, float h, uint32_t color) {
  if (!dd || !dd->root)
    return NULL;

  Node *node = node_create(dd->next_node_id++, NODE_TYPE_RECT, NULL);
  if (!node)
    return NULL;
  node->x = x;
  node->y = y;
  node->width = w;
  node->height = h;
  node->visible = 1;
  node->parent = dd->root;

  // Use the same styles as canvas for
  // consistency
  switch (style) {
  case NODE_STYLE_RECT:
    node->type = NODE_TYPE_RECT;
    node->fill_color = color;
    node->stroke_color = 0x1A73E8FF;
    node->stroke_width = 2.0f;
    node->corner_radius = 8.0f;
    break;

  case NODE_STYLE_ELLIPSE:
    node->type = NODE_TYPE_ELLIPSE;
    node->fill_color = color;
    node->stroke_color = 0x1E8E3EFF;
    node->stroke_width = 2.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_TRIANGLE:
    node->type = NODE_TYPE_TRIANGLE;
    node->fill_color = color;
    node->stroke_color = 0xF9A825FF;
    node->stroke_width = 2.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_LINE:
    node->type = NODE_TYPE_LINE;
    node->fill_color = color;
    node->stroke_color = color;
    node->stroke_width = 2.0f;
    node->corner_radius = 0.0f;
    node->height = 4.0f; // Thin line
    break;

  case NODE_STYLE_TEXT:
    node->type = NODE_TYPE_TEXT;
    node->fill_color = 0x00000000;   // Transparent
    node->stroke_color = 0x000000FF; // Black text
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    node->font_size = 12.0f;
    node->font_type = 0;
    strncpy(node->text, "Text", sizeof(node->text) - 1);
    break;
  }

  // Add to tree
  if (!dd->root->first_child) {
    dd->root->first_child = node;
  } else {
    Node *last = dd->root->first_child;
    while (last->next_sibling)
      last = last->next_sibling;
    last->next_sibling = node;
  }

  return node;
}

// -----------------------------------------------------------------------------
// Texture Cache (Simple LRU)
// -----------------------------------------------------------------------------

typedef struct {
  char path[512];
  int nvg_image;
  int width;
  int height;
  uint32_t last_used;
  int in_use;
} TextureCacheEntry;

#define TEX_CACHE_SIZE 64
TextureCacheEntry tex_cache[TEX_CACHE_SIZE];
uint32_t tex_cache_counter = 0;

int get_image_texture(UIHost *ui, const char *path) {
  if (!path || !path[0])
    return 0;

  tex_cache_counter++;

  // Check cache
  for (int i = 0; i < TEX_CACHE_SIZE; i++) {
    if (tex_cache[i].in_use && strcmp(tex_cache[i].path, path) == 0) {
      tex_cache[i].last_used = tex_cache_counter;
      return tex_cache[i].nvg_image;
    }
  }

  // Not in cache, load it using macOS native
  // loader
  int w, h;
  void *pixels = image_load_rgba(path, &w, &h);

  ASLAN_DEBUG("get_image_texture: trying path "
              "'%s' -> %p",
              path, pixels);

  if (!pixels) {
    // Try relative to assets if not already
    // starting with assets/
    if (strncmp(path, "assets/", 7) != 0) {
      char full_path[512];
      snprintf(full_path, sizeof(full_path), "assets/%s", path);
      pixels = image_load_rgba(full_path, &w, &h);
      ASLAN_DEBUG("get_image_texture: trying "
                  "full_path '%s' -> %p",
                  full_path, pixels);
    }
  }

  if (!pixels) {
    ASLAN_ERROR("get_image_texture failed to load: %s", path);
    return 0;
  }

  // Create NanoVG image
  int nvg_img =
      nvgCreateImageRGBA(ui->vg, w, h, 0, (const unsigned char *)pixels);
  free(pixels);

  if (nvg_img == 0)
    return 0;

  // Find slot in cache
  int slot = -1;
  // First pass: empty slot
  for (int i = 0; i < TEX_CACHE_SIZE; i++) {
    if (!tex_cache[i].in_use) {
      slot = i;
      break;
    }
  }
  // Second pass: LRU
  if (slot == -1) {
    uint32_t min_time = 0xFFFFFFFF;
    for (int i = 0; i < TEX_CACHE_SIZE; i++) {
      if (tex_cache[i].last_used < min_time) {
        min_time = tex_cache[i].last_used;
        slot = i;
      }
    }
    // Free old texture
    if (tex_cache[slot].nvg_image != 0) {
      nvgDeleteImage(ui->vg, tex_cache[slot].nvg_image);
    }
  }

  // Store in cache
  strncpy(tex_cache[slot].path, path, sizeof(tex_cache[slot].path) - 1);
  tex_cache[slot].nvg_image = nvg_img;
  tex_cache[slot].width = w;
  tex_cache[slot].height = h;
  tex_cache[slot].last_used = tex_cache_counter;
  tex_cache[slot].in_use = 1;

  return nvg_img;
}

int tessellate_bezier(float x1, float y1, float cp1x, float cp1y, float cp2x,
                      float cp2y, float x2, float y2, float *out_pts,
                      int max_pts, int include_last) {
  int segments = 12;
  int count = 0;
  for (int i = 0; i < segments + (include_last ? 1 : 0); i++) {
    if (count >= max_pts)
      break;
    float t = (float)i / (float)segments;
    float mt = 1.0f - t;
    float mt2 = mt * mt;
    float mt3 = mt2 * mt;
    float t2 = t * t;
    float t3 = t2 * t;

    out_pts[count * 2] =
        mt3 * x1 + 3 * mt2 * t * cp1x + 3 * mt * t2 * cp2x + t3 * x2;
    out_pts[count * 2 + 1] =
        mt3 * y1 + 3 * mt2 * t * cp1y + 3 * mt * t2 * cp2y + t3 * y2;
    count++;
  }
  return count;
}

int node_get_path_polygon(Node *node, float *out_pts, int max_pts, float sx,
                          float sy, float zoom) {
  if (!node || node->type != NODE_TYPE_PATH || !node->path_points)
    return 0;
  int total_count = 0;
  for (int i = 0; i < node->path_point_count; i++) {
    int next = (i + 1) % node->path_point_count;
    NodePathPoint p1 = node->path_points[i];
    NodePathPoint p2 = node->path_points[next];

    if (p1.type == 1) { // Cubic Bezier
      int c = tessellate_bezier(
          sx + p1.x * zoom, sy + p1.y * zoom, sx + p1.cp2x * zoom,
          sy + p1.cp2y * zoom, sx + p2.cp1x * zoom, sy + p2.cp1y * zoom,
          sx + p2.x * zoom, sy + p2.y * zoom, &out_pts[total_count * 2],
          max_pts - total_count, 0);
      total_count += c;
    } else {
      if (total_count < max_pts) {
        out_pts[total_count * 2] = sx + p1.x * zoom;
        out_pts[total_count * 2 + 1] = sy + p1.y * zoom;
        total_count++;
      }
    }
  }
  return total_count;
}

void node_update_path_bounds(Node *node) {
  if (!node || node->type != NODE_TYPE_PATH || !node->path_points ||
      node->path_point_count == 0)
    return;

  float min_x = node->path_points[0].x;
  float min_y = node->path_points[0].y;
  float max_x = min_x;
  float max_y = min_y;

  for (int i = 0; i < node->path_point_count; i++) {
    NodePathPoint p = node->path_points[i];
    if (p.x < min_x)
      min_x = p.x;
    if (p.y < min_y)
      min_y = p.y;
    if (p.x > max_x)
      max_x = p.x;
    if (p.y > max_y)
      max_y = p.y;
    if (p.type == 1) {
      if (p.cp1x < min_x)
        min_x = p.cp1x;
      if (p.cp1y < min_y)
        min_y = p.cp1y;
      if (p.cp1x > max_x)
        max_x = p.cp1x;
      if (p.cp1y > max_y)
        max_y = p.cp1y;
      if (p.cp2x < min_x)
        min_x = p.cp2x;
      if (p.cp2y < min_y)
        min_y = p.cp2y;
      if (p.cp2x > max_x)
        max_x = p.cp2x;
      if (p.cp2y > max_y)
        max_y = p.cp2y;
    }
  }

  // Offset correction to keep points stable
  // while moving the node's origin
  float dx = min_x;
  float dy = min_y;

  if (fabsf(dx) > 0.0001f || fabsf(dy) > 0.0001f) {
    node->x += dx;
    node->y += dy;
    for (int i = 0; i < node->path_point_count; i++) {
      node->path_points[i].x -= dx;
      node->path_points[i].y -= dy;
      node->path_points[i].cp1x -= dx;
      node->path_points[i].cp1y -= dy;
      node->path_points[i].cp2x -= dx;
      node->path_points[i].cp2y -= dy;
    }
    max_x -= dx;
    max_y -= dy;
  }

  node->width = max_x;
  node->height = max_y;
}

void node_convert_to_path(Node *node) {
  if (!node || node->type == NODE_TYPE_PATH)
    return;

  // Define points based on shape type
  NodePathPoint *pts = NULL;
  int count = 0;

  if (node->type == NODE_TYPE_RECT) {
    count = 4;
    pts = (NodePathPoint *)calloc(count, sizeof(NodePathPoint));
    pts[0] = (NodePathPoint){0, 0, 0, 0, 0, 0, 0};
    pts[1] = (NodePathPoint){node->width, 0, node->width, 0, node->width, 0, 0};
    pts[2] = (NodePathPoint){node->width,
                             node->height,
                             node->width,
                             node->height,
                             node->width,
                             node->height,
                             0};
    pts[3] =
        (NodePathPoint){0, node->height, 0, node->height, 0, node->height, 0};
  } else if (node->type == NODE_TYPE_TRIANGLE) {
    count = 3;
    pts = (NodePathPoint *)calloc(count, sizeof(NodePathPoint));
    pts[0] = (NodePathPoint){
        node->width / 2, 0, node->width / 2, 0, node->width / 2, 0, 0};
    pts[1] =
        (NodePathPoint){0, node->height, 0, node->height, 0, node->height, 0};
    pts[2] = (NodePathPoint){node->width,
                             node->height,
                             node->width,
                             node->height,
                             node->width,
                             node->height,
                             0};
  } else if (node->type == NODE_TYPE_ELLIPSE) {
    count = 4;
    pts = (NodePathPoint *)calloc(count, sizeof(NodePathPoint));
    float kappa = 0.552284749831f;
    float ox = node->width / 2.0f;
    float oy = node->height / 2.0f;
    float rx = node->width / 2.0f;
    float ry = node->height / 2.0f;

    // Top
    pts[0] = (NodePathPoint){ox, 0, ox - rx * kappa, 0, ox + rx * kappa, 0, 1};
    // Right
    pts[1] = (NodePathPoint){
        node->width,     oy, node->width, oy - ry * kappa, node->width,
        oy + ry * kappa, 1};
    // Bottom
    pts[2] = (NodePathPoint){ox,
                             node->height,
                             ox + rx * kappa,
                             node->height,
                             ox - rx * kappa,
                             node->height,
                             1};
    // Left
    pts[3] = (NodePathPoint){0, oy, 0, oy + ry * kappa, 0, oy - ry * kappa, 1};
  }

  if (pts) {
    if (node->path_points)
      free(node->path_points);
    node->path_points = pts;
    node->path_point_count = count;
    node->type = NODE_TYPE_PATH; // must be set so bezier
                                 // handles activate
  }
}

Node *design_find_node_at(DesignData *dd, float x, float y) {
  if (!dd->root)
    return NULL;

  Node *hit = NULL;
  for (Node *node = dd->root->first_child; node; node = node->next_sibling) {
    if (!node->visible)
      continue;

    // Stroke/path nodes have thin bounding boxes
    // — add hit padding
    float pad = (node->type == NODE_TYPE_STROKE || node->type == NODE_TYPE_PATH)
                    ? 8.0f
                    : 0.0f;

    float rad = node->rotation * 3.14159f / 180.0f;
    if (rad == 0.0f) {
      if (x >= node->x - pad && x < node->x + node->width + pad &&
          y >= node->y - pad && y < node->y + node->height + pad) {
        hit = node;
      }
    } else {
      // Rotated hit test: transform (x,y) to
      // local space
      float cx = node->x + node->width / 2.0f;
      float cy = node->y + node->height / 2.0f;
      float dx = x - cx;
      float dy = y - cy;
      float cos_r = cosf(-rad);
      float sin_r = sinf(-rad);
      float lx = dx * cos_r - dy * sin_r;
      float ly = dx * sin_r + dy * cos_r;
      float hw = node->width / 2.0f + pad;
      float hh = node->height / 2.0f + pad;

      if (lx >= -hw && lx < hw && ly >= -hh && ly < hh) {
        hit = node;
      }
    }
  }
  return hit;
}
