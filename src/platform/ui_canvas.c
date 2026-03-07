// =============================================================================
// ASLAN UI CANVAS - Infinite Canvas Block Rendering and Tools
// =============================================================================

#include "ui_canvas.h"
#include "../document/aslan_table.h"
#include "platform/image_loader.h"
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

void draw_alignment_guides(struct nk_command_buffer *canvas_buf, Node *node,
                           float area_cx, float area_cy, float area_x,
                           float area_y, float area_w, float area_h,
                           float node_sx, float node_sy, float node_sw,
                           float node_sh) {
  if (!node)
    return;
  struct nk_color guide_col = nk_rgba(110, 110, 130, 150);
  float tolerance = 3.0f;

  float node_cx = node_sx + node_sw / 2.0f;
  float node_cy = node_sy + node_sh / 2.0f;

  // Vertical center guide
  if (fabsf(node_cx - area_cx) < tolerance) {
    nk_stroke_line(canvas_buf, area_cx, area_y, area_cx, area_y + area_h, 1.0f,
                   guide_col);
  }
  // Horizontal center guide
  if (fabsf(node_cy - area_cy) < tolerance) {
    nk_stroke_line(canvas_buf, area_x, area_cy, area_x + area_w, area_cy, 1.0f,
                   guide_col);
  }
}

// -----------------------------------------------------------------------------
// Shared Helper: Render right-click context menu for nodes
// -----------------------------------------------------------------------------

void render_node_context_menu(UIHost *ui, struct nk_context *ctx,
                              struct nk_command_buffer *canvas_buf,
                              Block *block) {
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
  nk_fill_rect(canvas_buf, nk_rect(mx + 2, my + 3, menu_w, menu_h), 8.0f,
               nk_rgba(0, 0, 0, 25));
  // Background
  nk_fill_rect(canvas_buf, nk_rect(mx, my, menu_w, menu_h), 8.0f,
               nk_rgb(255, 255, 255));
  nk_stroke_rect(canvas_buf, nk_rect(mx, my, menu_w, menu_h), 8.0f, 1.0f,
                 nk_rgb(220, 220, 220));

  const char *labels[] = {"Duplicate", "Delete", "Bring to Front",
                          "Send to Back", "Ungroup"};

  for (int i = 0; i < item_count; i++) {
    struct nk_rect item_r = nk_rect(mx + padding, my + padding + i * item_h,
                                    menu_w - 2 * padding, item_h);
    int hovered =
        (ui->mouse_x >= item_r.x && ui->mouse_x < item_r.x + item_r.w &&
         ui->mouse_y >= item_r.y && ui->mouse_y < item_r.y + item_r.h);

    if (hovered)
      nk_fill_rect(canvas_buf, item_r, 4.0f, nk_rgba(110, 110, 130, 20));

    nk_draw_text(
        canvas_buf, nk_rect(item_r.x + 10, item_r.y + 6, item_r.w - 20, 20),
        labels[i], strlen(labels[i]), ctx->style.font, nk_rgba(0, 0, 0, 0),
        i == 1 ? nk_rgb(220, 50, 50) : nk_rgb(40, 40, 40));

    if (hovered && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
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
  if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
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

void canvas_draw_dot_grid(struct nk_command_buffer *canvas,
                          struct nk_rect bounds, float cam_x, float cam_y,
                          float zoom) {
  // Grid spacing in world units
  const float grid_spacing = 40.0f;
  // Dot appearance
  const float dot_radius = 1.5f;
  struct nk_color dot_color = {80, 82, 86, 255}; // Subtle gray

  // Calculate visible world bounds
  float world_left = cam_x;
  float world_top = cam_y;
  float world_right = cam_x + bounds.w / zoom;
  float world_bottom = cam_y + bounds.h / zoom;

  // Snap to grid
  float start_x = floorf(world_left / grid_spacing) * grid_spacing;
  float start_y = floorf(world_top / grid_spacing) * grid_spacing;

  // Limit iterations for performance
  int max_dots_x = (int)((bounds.w / (grid_spacing * zoom)) + 2);
  int max_dots_y = (int)((bounds.h / (grid_spacing * zoom)) + 2);
  if (max_dots_x > 100)
    max_dots_x = 100;
  if (max_dots_y > 100)
    max_dots_y = 100;

  for (int iy = 0; iy < max_dots_y; iy++) {
    float world_y = start_y + iy * grid_spacing;
    if (world_y > world_bottom)
      break;

    float screen_y = bounds.y + (world_y - cam_y) * zoom;

    for (int ix = 0; ix < max_dots_x; ix++) {
      float world_x = start_x + ix * grid_spacing;
      if (world_x > world_right)
        break;

      float screen_x = bounds.x + (world_x - cam_x) * zoom;

      // Draw dot as small filled circle
      nk_fill_circle(canvas,
                     nk_rect(screen_x - dot_radius, screen_y - dot_radius,
                             dot_radius * 2, dot_radius * 2),
                     dot_color);
    }
  }
}

// -----------------------------------------------------------------------------
// Canvas Helper: Check if point is in toolbar area
// -----------------------------------------------------------------------------

int canvas_point_in_toolbar(struct nk_rect bounds, float mx, float my,
                            CanvasInteraction *cs) {
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  const float toolbar_w = 7 * btn_size + 6 * btn_spacing + 24.0f; // 7 buttons
  const float toolbar_h = 52.0f;
  const float margin = 20.0f;

  float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
  float toolbar_y = bounds.y + bounds.h - toolbar_h - margin;

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

void canvas_render_sub_menu(UIHost *ui, struct nk_context *ctx,
                            struct nk_command_buffer *canvas_buf, float x,
                            float y, CanvasCategory cat) {
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
  nk_fill_rect(canvas_buf, nk_rect(menu_x + 2, menu_y + 4, menu_w, menu_h),
               12.0f, nk_rgba(0, 0, 0, 30));
  // Background
  nk_fill_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 12.0f,
               nk_rgb(255, 255, 255));
  nk_stroke_rect(canvas_buf, nk_rect(menu_x, menu_y, menu_w, menu_h), 12.0f,
                 1.0f, nk_rgb(230, 230, 230));

  for (int i = 0; i < item_count; i++) {
    struct nk_rect item_r =
        nk_rect(menu_x + menu_padding, menu_y + menu_padding + i * item_h,
                menu_w - 2 * menu_padding, item_h);
    int hovered =
        (ui->mouse_x >= item_r.x && ui->mouse_x < item_r.x + item_r.w &&
         ui->mouse_y >= item_r.y && ui->mouse_y < item_r.y + item_r.h);

    if (hovered) {
      nk_fill_rect(canvas_buf, item_r, 6.0f, nk_rgba(0, 0, 0, 15));
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
      nk_draw_text(canvas_buf, nk_rect(item_r.x + 8, item_r.y + 6, 20, 20),
                   icons[i], strlen(icons[i]), &ui->font_icons->handle,
                   nk_rgba(0, 0, 0, 0), nk_rgb(80, 80, 80));
    }
    // Label
    nk_draw_text(canvas_buf,
                 nk_rect(item_r.x + 36, item_r.y + 6, item_r.w - 40, 20),
                 items[i], strlen(items[i]), ctx->style.font,
                 nk_rgba(0, 0, 0, 0), nk_rgb(40, 40, 40));
  }
}

// Helper to render the inner content of the color picker (Wheel + Hex +
// Sliders) Returns updated colorf, or original if no change.
struct nk_colorf ui_render_color_picker_content(struct nk_context *ctx,
                                                struct nk_colorf colorf) {
  nk_layout_row_dynamic(ctx, 150, 1);
  colorf = nk_color_picker(ctx, colorf, NK_RGBA);

  nk_layout_row_dynamic(ctx, 25, 1);

  // Hex Code Display
  char hex_buf[10];
  snprintf(hex_buf, sizeof(hex_buf), "#%02X%02X%02X", (int)(colorf.r * 255.0f),
           (int)(colorf.g * 255.0f), (int)(colorf.b * 255.0f));

  nk_layout_row_dynamic(ctx, 20, 1);
  nk_label(ctx, hex_buf, NK_TEXT_CENTERED);

  // RGBA Sliders
  nk_layout_row_dynamic(ctx, 25, 1);
  colorf.r = nk_propertyf(ctx, "R", 0, colorf.r, 1, 0.01f, 0.005f);
  colorf.g = nk_propertyf(ctx, "G", 0, colorf.g, 1, 0.01f, 0.005f);
  colorf.b = nk_propertyf(ctx, "B", 0, colorf.b, 1, 0.01f, 0.005f);
  colorf.a = nk_propertyf(ctx, "A", 0, colorf.a, 1, 0.01f, 0.005f);

  return colorf;
}

// Helper to interact with color properties (Picker + Hex + RGBA) - Uses Combo
// (for Sidebar)
void ui_render_color_property(struct nk_context *ctx, const char *label,
                              uint32_t *color_ptr) {
  if (!color_ptr)
    return;

  // Current Color
  struct nk_color combo_color =
      nk_rgba((*color_ptr >> 24) & 0xFF, (*color_ptr >> 16) & 0xFF,
              (*color_ptr >> 8) & 0xFF, *color_ptr & 0xFF);

  nk_layout_row_dynamic(ctx, 25, 1);
  nk_label(ctx, label, NK_TEXT_LEFT);

  nk_layout_row_dynamic(ctx, 30, 1);
  if (nk_combo_begin_color(ctx, combo_color, nk_vec2(220, 450))) {
    struct nk_colorf combo_colorf = nk_color_cf(combo_color);

    // Render Content
    combo_colorf = ui_render_color_picker_content(ctx, combo_colorf);

    nk_combo_end(ctx);

    // Update back to uint32
    combo_color =
        nk_rgba((int)(combo_colorf.r * 255.0f), (int)(combo_colorf.g * 255.0f),
                (int)(combo_colorf.b * 255.0f), (int)(combo_colorf.a * 255.0f));
    *color_ptr = ((uint32_t)combo_color.r << 24) |
                 ((uint32_t)combo_color.g << 16) |
                 ((uint32_t)combo_color.b << 8) | (uint32_t)combo_color.a;
  }
}

// Helper: draw a subtle section divider line
static void sidebar_section_divider(struct nk_context *ctx) {
  nk_layout_row_dynamic(ctx, 8, 1);
  nk_spacing(ctx, 1);
}

// Helper: section header with subtle gray label
static void sidebar_section_header(struct nk_context *ctx, const char *title) {
  nk_layout_row_dynamic(ctx, 20, 1);
  struct nk_color header_col = nk_rgb(130, 130, 140);
  nk_label_colored(ctx, title, NK_TEXT_LEFT, header_col);
}

// Helper: get display name for node type
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

void ui_render_sidebar_properties(UIHost *ui, Engine *engine,
                                  struct nk_context *ctx, Block *block) {
  // =====================================================================
  // NO NODE SELECTED — Show block-level settings
  // =====================================================================
  if (!ui->selected_node) {
    if (block && block->type == BLOCK_DESIGN) {
      DesignData *dd = &block->data.design;

      sidebar_section_header(ctx, "ARTBOARD");

      int old_w = dd->width;
      int old_h = dd->height;
      int old_units = dd->units;

      // Unit selector
      nk_layout_row_dynamic(ctx, 28, 1);
      const char *unit_names[] = {"Pixels (px)", "Centimeters (cm)",
                                  "Inches (in)"};
      int cur_units =
          nk_combo(ctx, unit_names, 3, dd->units, 28, nk_vec2(200, 200));

      // Dimension inputs
      float val_w = (float)old_w, val_h = (float)old_h;
      float step = 1.0f, min_val = 100.0f, max_val = 4000.0f;
      if (cur_units == 1) {
        val_w /= 37.795f;
        val_h /= 37.795f;
        step = 0.1f;
        min_val = 2.0f;
        max_val = 100.0f;
      } else if (cur_units == 2) {
        val_w /= 96.0f;
        val_h /= 96.0f;
        step = 0.1f;
        min_val = 1.0f;
        max_val = 40.0f;
      }

      nk_layout_row_dynamic(ctx, 28, 2);
      nk_property_float(ctx, "W:", min_val, &val_w, max_val, step, step / 2.0f);
      nk_property_float(ctx, "H:", min_val, &val_h, max_val, step, step / 2.0f);

      int new_w = old_w, new_h = old_h;
      if (cur_units == 0) {
        new_w = (int)val_w;
        new_h = (int)val_h;
      } else if (cur_units == 1) {
        new_w = (int)(val_w * 37.795f);
        new_h = (int)(val_h * 37.795f);
      } else if (cur_units == 2) {
        new_w = (int)(val_w * 96.0f);
        new_h = (int)(val_h * 96.0f);
      }

      if (new_w != old_w || new_h != old_h || cur_units != old_units) {
        Page *page = doc_page_get_active(engine->doc);
        if (page) {
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_design_resize %u %u %d %d %d",
                   page->id, block->id, new_w, new_h, cur_units);
          engine_execute_command(engine, cmd);
        }
      }

      // Presets
      sidebar_section_divider(ctx);
      sidebar_section_header(ctx, "PRESETS");
      nk_layout_row_dynamic(ctx, 28, 4);
      Page *page = doc_page_get_active(engine->doc);
      if (page) {
        struct {
          const char *label;
          int w, h, unit;
        } presets[] = {{"A4", 794, 1123, 1},
                       {"A3", 1123, 1587, 1},
                       {"HD", 1920, 1080, 0},
                       {"4K", 3840, 2160, 0}};
        for (int i = 0; i < 4; i++) {
          if (nk_button_label(ctx, presets[i].label)) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "block_design_resize %u %u %d %d %d",
                     page->id, block->id, presets[i].w, presets[i].h,
                     presets[i].unit);
            engine_execute_command(engine, cmd);
          }
        }
        nk_layout_row_dynamic(ctx, 28, 4);
        struct {
          const char *label;
          int w, h;
        } social[] = {{"Square", 1080, 1080},
                      {"Story", 1080, 1920},
                      {"Post", 1080, 1350},
                      {"Banner", 1920, 480}};
        for (int i = 0; i < 4; i++) {
          if (nk_button_label(ctx, social[i].label)) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "block_design_resize %u %u %d %d %d",
                     page->id, block->id, social[i].w, social[i].h, 0);
            engine_execute_command(engine, cmd);
          }
        }
      }

      // Background color
      sidebar_section_divider(ctx);
      ui_render_color_property(ctx, "Background",
                               &dd->pages[dd->current_page].bg_color);
    } else {
      nk_layout_row_dynamic(ctx, 28, 1);
      nk_label_colored(ctx, "No element selected", NK_TEXT_LEFT,
                       nk_rgb(150, 150, 150));
    }
    return;
  }

  Node *node = ui->selected_node;

  // =====================================================================
  // NODE HEADER — Type icon + name
  // =====================================================================
  nk_layout_row_dynamic(ctx, 28, 1);
  nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                                 node->name, sizeof(node->name) - 1,
                                 nk_filter_default);

  // Type button (click to change shape)
  nk_layout_row_dynamic(ctx, 26, 1);
  const char *type_display = node_type_display_name(node->type);
  if (nk_button_label(ctx, type_display)) {
    ui->sidebar_mode = 1;
  }

  // =====================================================================
  // TEXT NODES — Typography section
  // =====================================================================
  if (node->type == NODE_TYPE_TEXT) {
    sidebar_section_divider(ctx);
    sidebar_section_header(ctx, "TEXT");

    nk_layout_row_dynamic(ctx, 50, 1);
    nk_edit_string_zero_terminated(ctx, NK_EDIT_BOX, node->text,
                                   sizeof(node->text) - 1, nk_filter_default);

    sidebar_section_header(ctx, "TYPOGRAPHY");

    nk_layout_row_dynamic(ctx, 28, 1);
    const char *fonts[] = {"Inter", "Heading 1", "Heading 2", "Heading 3",
                           "Monospace"};
    node->font_type =
        nk_combo(ctx, fonts, 5, node->font_type, 28, nk_vec2(200, 200));

    if (node->font_size < 8.0f)
      node->font_size = 12.0f;
    nk_layout_row_dynamic(ctx, 28, 1);
    nk_property_float(ctx, "Size:", 8.0f, &node->font_size, 200.0f, 1.0f, 1.0f);

    sidebar_section_divider(ctx);
    ui_render_color_property(ctx, "Color", &node->stroke_color);

    // Fill (background)
    ui_render_color_property(ctx, "Background", &node->fill_color);
    return;
  }

  // =====================================================================
  // CHART NODES — Data settings
  // =====================================================================
  if (node->type == NODE_TYPE_CHART && node->chart_data) {
    ChartData *cd = node->chart_data;
    sidebar_section_divider(ctx);
    sidebar_section_header(ctx, "CHART");

    nk_layout_row_dynamic(ctx, 28, 1);
    const char *chart_types[] = {"Bar", "Line", "Pie"};
    int ct = (int)cd->type;
    int new_ct = nk_combo(ctx, chart_types, 3, ct, 28, nk_vec2(200, 200));
    if (new_ct != ct)
      cd->type = (ChartType)new_ct;

    ui_render_color_property(ctx, "Color", &cd->color);

    sidebar_section_header(ctx, "DATA SOURCE");
    Page *page = doc_page_get_active(engine->doc);
    if (page) {
      char *table_names[32];
      uint32_t table_ids[32];
      int table_count = 0;
      int current_table_idx = -1;
      table_names[0] = "None";
      table_ids[0] = 0;
      table_count++;
      if (cd->source_table_id == 0)
        current_table_idx = 0;

      char name_storage[32][64];
      for (Block *b = page->first_block; b && table_count < 32; b = b->next) {
        if (b->type == BLOCK_TABLE) {
          snprintf(name_storage[table_count], 64, "Table %u", b->id);
          table_names[table_count] = name_storage[table_count];
          table_ids[table_count] = b->id;
          if (b->id == cd->source_table_id)
            current_table_idx = table_count;
          table_count++;
        }
      }
      if (current_table_idx == -1)
        current_table_idx = 0;

      nk_layout_row_dynamic(ctx, 28, 1);
      int new_ti = nk_combo(ctx, (const char **)table_names, table_count,
                            current_table_idx, 28, nk_vec2(200, 200));
      if (new_ti != current_table_idx) {
        cd->source_table_id = table_ids[new_ti];
        cd->label_col_idx = 0;
        cd->value_col_idx = 1;
      }

      if (cd->source_table_id != 0) {
        Block *src_block = NULL;
        for (Block *b = page->first_block; b; b = b->next) {
          if (b->id == cd->source_table_id) {
            src_block = b;
            break;
          }
        }
        if (src_block && src_block->data.table.columnar) {
          AslanTable *tbl = src_block->data.table.columnar;
          int col_count = tbl->column_count;
          if (col_count > 0) {
            char *col_names[32];
            for (int k = 0; k < col_count && k < 32; k++)
              col_names[k] = tbl->columns[k].name;

            sidebar_section_header(ctx, "COLUMNS");
            nk_layout_row_dynamic(ctx, 28, 1);
            int l_idx = cd->label_col_idx;
            if (l_idx >= col_count)
              l_idx = 0;
            int new_l = nk_combo(ctx, (const char **)col_names, col_count,
                                 l_idx, 28, nk_vec2(200, 200));
            if (new_l != cd->label_col_idx)
              cd->label_col_idx = new_l;

            nk_layout_row_dynamic(ctx, 28, 1);
            int v_idx = cd->value_col_idx;
            if (v_idx >= col_count)
              v_idx = 0;
            int new_v = nk_combo(ctx, (const char **)col_names, col_count,
                                 v_idx, 28, nk_vec2(200, 200));
            if (new_v != cd->value_col_idx)
              cd->value_col_idx = new_v;
          }
        }
      }
    }
    return;
  }

  // =====================================================================
  // POSITION & SIZE — Figma-style compact layout
  // =====================================================================
  sidebar_section_divider(ctx);
  sidebar_section_header(ctx, "POSITION");

  nk_layout_row_dynamic(ctx, 28, 2);
  nk_property_float(ctx, "X:", -10000.0f, &node->x, 10000.0f, 1.0f, 0.5f);
  nk_property_float(ctx, "Y:", -10000.0f, &node->y, 10000.0f, 1.0f, 0.5f);

  sidebar_section_header(ctx, "SIZE");
  nk_layout_row_dynamic(ctx, 28, 2);
  nk_property_float(ctx, "W:", 1.0f, &node->width, 10000.0f, 1.0f, 0.5f);
  nk_property_float(ctx, "H:", 1.0f, &node->height, 10000.0f, 1.0f, 0.5f);

  // Rotation
  nk_layout_row_dynamic(ctx, 28, 1);
  nk_property_float(ctx, "Rotation:", -360.0f, &node->rotation, 360.0f, 1.0f,
                    0.5f);

  // =====================================================================
  // LINE ENDPOINTS — for lines, strokes, thin rects
  // =====================================================================
  int is_line = 0;
  if ((node->type == NODE_TYPE_RECT &&
       (node->height < 8.0f || node->width < 8.0f)) ||
      (node->type == NODE_TYPE_PATH && node->path_point_count == 2) ||
      node->type == NODE_TYPE_LINE || node->type == NODE_TYPE_STROKE) {
    is_line = 1;
  }

  if (is_line) {
    sidebar_section_divider(ctx);
    sidebar_section_header(ctx, "ENDPOINTS");
    const char *end_styles[] = {"None", "Circle", "Arrow"};
    nk_layout_row_dynamic(ctx, 28, 2);
    node->line_start_style = nk_combo(
        ctx, end_styles, 3, node->line_start_style, 28, nk_vec2(100, 120));
    node->line_end_style = nk_combo(ctx, end_styles, 3, node->line_end_style,
                                    28, nk_vec2(100, 120));
  }

  // =====================================================================
  // APPEARANCE — Fill, Stroke, Corner Radius, Opacity
  // =====================================================================
  sidebar_section_divider(ctx);
  sidebar_section_header(ctx, "FILL");
  ui_render_color_property(ctx, "Color", &node->fill_color);

  // Corner radius (for shapes that support it)
  if (node->type == NODE_TYPE_RECT || node->type == NODE_TYPE_IMAGE ||
      node->type == NODE_TYPE_FRAME) {
    nk_layout_row_dynamic(ctx, 28, 1);
    nk_property_float(ctx, "Radius:", 0.0f, &node->corner_radius, 200.0f, 1.0f,
                      0.5f);
  }

  sidebar_section_divider(ctx);
  sidebar_section_header(ctx, "STROKE");
  nk_layout_row_dynamic(ctx, 28, 1);
  nk_property_float(ctx, "Width:", 0.0f, &node->stroke_width, 100.0f, 0.5f,
                    0.25f);
  ui_render_color_property(ctx, "Color", &node->stroke_color);

  // =====================================================================
  // ADVANCED — Container mode
  // =====================================================================
  if (node->type != NODE_TYPE_TEXT && node->type != NODE_TYPE_STROKE &&
      node->type != NODE_TYPE_CHART) {
    sidebar_section_divider(ctx);
    sidebar_section_header(ctx, "ADVANCED");
    nk_layout_row_dynamic(ctx, 28, 1);
    const char *modes[] = {"Standard", "Image Container"};
    int old_mode = node->is_container ? 1 : 0;
    int new_mode = nk_combo(ctx, modes, 2, old_mode, 28, nk_vec2(200, 100));
    if (new_mode != old_mode)
      node->is_container = new_mode;
  }
}

static void ui_render_layer_item(UIHost *ui, struct nk_context *ctx,
                                 Block *block, Node *node, int depth) {
  if (!node)
    return;

  int is_selected = (ui->selected_node == node);
  int is_renaming = (ui->renaming_node_id == node->id);
  struct nk_color hidden_text = nk_rgb(180, 180, 185);

  // -- Layer row: [indent] [Fold] [Icon] [Name flex] [Swatch] [Vis] [Lock] --
  float indent_w = 4.0f + (float)(depth) * 16.0f;
  nk_layout_row_template_begin(ctx, 30);
  nk_layout_row_template_push_static(ctx, indent_w); // indent spacer
  nk_layout_row_template_push_static(ctx, 16.0f);    // fold toggle
  nk_layout_row_template_push_static(ctx, 24.0f);    // type icon
  nk_layout_row_template_push_dynamic(ctx);          // name (flexible)
  nk_layout_row_template_push_static(ctx, 24.0f);    // visibility
  nk_layout_row_template_push_static(ctx, 24.0f);    // lock
  nk_layout_row_template_push_static(ctx, 24.0f);    // trash
  nk_layout_row_template_end(ctx);

  struct nk_rect row_bounds = nk_widget_bounds(ctx);
  // Full row area for interaction - spans entire content width
  struct nk_rect full_row = nk_window_get_content_region(ctx);
  full_row.y = row_bounds.y;
  full_row.h = 30;
  int is_hovered_row = nk_input_is_mouse_hovering_rect(&ctx->input, full_row);

  // 1. Hover Sync (Sidebar -> Canvas)
  if (is_hovered_row) {
    ui->hovered_node_id = node->id;
  }

  // 2. Drag & Drop Detection
  int mouse_down = nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT);
  int mouse_pressed = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT);
  int mouse_released = nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT);

  // Start dragging
  if (is_hovered_row && mouse_pressed && !is_renaming) {
    ui->sidebar_drag_node_id = node->id;
    ui->sidebar_drag_started = 0;
  }

  // Detect drag threshold (prevent accidental drag on simple click)
  if (ui->sidebar_drag_node_id == node->id && mouse_down &&
      !ui->sidebar_drag_started) {
    float dx = ctx->input.mouse.delta.x;
    float dy = ctx->input.mouse.delta.y;
    if (fabsf(dx) > 3 || fabsf(dy) > 3) {
      ui->sidebar_drag_started = 1;
    }
  }

  // Detect drop target
  if (ui->sidebar_drag_node_id != 0 && ui->sidebar_drag_node_id != node->id &&
      is_hovered_row) {
    ui->sidebar_drop_target_id = node->id;
  }

  // Draw row background for hover or dragging (selection uses only left bar)
  if (ui->sidebar_drag_node_id == node->id && ui->sidebar_drag_started) {
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    nk_fill_rect(canvas_buf, full_row, 4.0f, nk_rgba(110, 110, 130, 30));
  } else if (is_hovered_row && !ui->sidebar_drag_node_id) {
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    nk_fill_rect(canvas_buf, full_row, 4.0f, nk_rgba(0, 0, 0, 12));
  }

  // Left accent bar for selected row (professional selection indicator)
  if (is_selected) {
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    nk_fill_rect(canvas_buf,
                 nk_rect(full_row.x, full_row.y + 4, 3, full_row.h - 8), 1.5f,
                 COLOR_ACCENT);
  }

  // Draw drop indicator line
  if (ui->sidebar_drop_target_id == node->id && ui->sidebar_drag_started) {
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    float my = ctx->input.mouse.pos.y;
    float row_mid = full_row.y + full_row.h / 2.0f;
    if (my < row_mid) {
      // Drop before (above)
      nk_fill_rect(canvas_buf,
                   nk_rect(full_row.x, full_row.y - 1, full_row.w, 2), 0,
                   COLOR_ACCENT);
    } else {
      // Drop after (below)
      nk_fill_rect(
          canvas_buf,
          nk_rect(full_row.x, full_row.y + full_row.h - 1, full_row.w, 2), 0,
          COLOR_ACCENT);
    }
  }

  nk_spacing(ctx, 1); // Indentation spacer

  // Shared transparent icon button style (reused for fold, type icon, vis,
  // lock)
  struct nk_style_button icon_style = ctx->style.button;
  icon_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
  icon_style.hover = nk_style_item_color(nk_rgba(0, 0, 0, 15));
  icon_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 25));
  icon_style.border = 0;
  icon_style.padding = nk_vec2(2, 2);
  struct nk_color icon_col =
      node->visible ? nk_rgb(110, 110, 125) : hidden_text;
  icon_style.text_normal = icon_col;
  icon_style.text_hover = icon_col;
  icon_style.active =
      nk_style_item_color(nk_rgba(0, 0, 0, 0)); // No blue active
  icon_style.text_active = icon_col;

  // 1. Fold Toggle (uses icons font for proper arrow glyphs)
  if (node->is_container && node->first_child) {
    if (ui->font_icons)
      nk_style_set_font(ctx, &ui->font_icons->handle);
    struct nk_style_button fold_style = icon_style;
    fold_style.text_normal = nk_rgb(160, 160, 175);
    fold_style.text_hover = COLOR_ACCENT;
    fold_style.text_active = nk_rgb(60, 110, 180);
    if (nk_button_label_styled(ctx, &fold_style,
                               node->folded ? ICON_EXPAND : ICON_COLLAPSE)) {
      node->folded = !node->folded;
    }
    if (ui->font_inter)
      nk_style_set_font(ctx, &ui->font_inter->handle);
  } else {
    nk_spacing(ctx, 1);
  }

  // 2. Type Icon
  const char *icon = node_type_icon(node);
  if (ui->font_icons)
    nk_style_set_font(ctx, &ui->font_icons->handle);
  struct nk_style_button type_btn_style = icon_style;
  if (is_selected)
    type_btn_style.text_normal = COLOR_ACCENT; // Neutral grey instead of blue
  if (nk_button_label_styled(ctx, &type_btn_style, icon)) {
    ui->selected_node = node;
    ui->renaming_node_id = 0; // Cancel renaming if clicking icon
  }
  if (ui->font_inter)
    nk_style_set_font(ctx, &ui->font_inter->handle);

  // 3. Name or Edit
  if (is_renaming) {
    struct nk_style_edit edit_style = ctx->style.edit;
    edit_style.normal.data.color = nk_rgb(255, 255, 255);
    edit_style.active.data.color = nk_rgb(255, 255, 255);
    edit_style.hover.data.color = nk_rgb(255, 255, 255);
    edit_style.text_normal = nk_rgb(0, 0, 0);
    edit_style.text_active = nk_rgb(0, 0, 0);
    edit_style.cursor_normal = nk_rgb(0, 0, 0);
    edit_style.padding = nk_vec2(4, 2);

    struct nk_style_edit old_edit = ctx->style.edit;
    ctx->style.edit = edit_style;
    nk_flags state = nk_edit_string_zero_terminated(
        ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, ui->node_rename_buffer, 63,
        nk_filter_default);
    ctx->style.edit = old_edit;

    int just_focused = ui->rename_needs_focus;
    if (just_focused) {
      nk_edit_focus(ctx, NK_EDIT_ALWAYS_INSERT_MODE);
      ui->rename_needs_focus = 0;
    }

    if (state & NK_EDIT_COMMITED) {
      strncpy(node->name, ui->node_rename_buffer, 63);
      ui->renaming_node_id = 0;
    } else if (!(state & NK_EDIT_ACTIVE) && !just_focused) {
      strncpy(node->name, ui->node_rename_buffer, 63);
      ui->renaming_node_id = 0;
    }
  } else {
    // Build display name
    char display_buf[72];
    const char *base_name =
        node->name[0] ? node->name : node_type_display_name(node->type);
    snprintf(display_buf, sizeof(display_buf), "%s", base_name);
    const char *display = display_buf;

    struct nk_style_button name_style = ctx->style.button;
    name_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    name_style.hover = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    name_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    name_style.border = 0.0f;
    name_style.border_color = nk_rgba(0, 0, 0, 0);
    name_style.padding = nk_vec2(4, 4);
    name_style.text_alignment = NK_TEXT_LEFT;
    struct nk_color text_col = !node->visible ? hidden_text
                               : is_selected  ? nk_rgb(60, 60, 75)
                                              : nk_rgb(80, 80, 95);
    name_style.text_normal = text_col;
    name_style.text_hover = text_col;
    name_style.text_active = text_col;
    if (nk_button_label_styled(ctx, &name_style, display)) {
      if (ui->selected_node == node) {
        ui->renaming_node_id = node->id;
        const char *initial =
            node->name[0] ? node->name : node_type_display_name(node->type);
        strncpy(ui->node_rename_buffer, initial, 63);
        ui->rename_needs_focus = 1;
      } else {
        ui->selected_node = node;
        ui->renaming_node_id = 0;
      }
    }
  }

  // 5. Visibility Toggle
  if (ui->font_icons)
    nk_style_set_font(ctx, &ui->font_icons->handle);
  struct nk_style_button vis_style = icon_style;
  vis_style.text_normal =
      node->visible ? nk_rgb(150, 150, 160) : nk_rgb(210, 210, 215);
  vis_style.text_hover = COLOR_ACCENT;
  if (nk_button_label_styled(ctx, &vis_style,
                             node->visible ? ICON_EYE : ICON_EYE_OFF)) {
    node->visible = !node->visible;
  }

  // 6. Lock Toggle
  struct nk_style_button lock_style = vis_style;
  lock_style.text_normal =
      node->locked ? nk_rgb(220, 100, 100) : nk_rgb(210, 210, 215);
  if (nk_button_label_styled(ctx, &lock_style,
                             node->locked ? ICON_LOCK : ICON_LOCK_UNLOCK)) {
    node->locked = !node->locked;
  }

  // 7. Trash Button
  if (ui->font_icons)
    nk_style_set_font(ctx, &ui->font_icons->handle);
  struct nk_style_button trash_style = icon_style;
  trash_style.text_normal = nk_rgb(200, 100, 100);
  trash_style.text_hover = nk_rgb(255, 50, 50);
  // Add Trash Icon (which matches the &#xEC2A; specified by the user)
  if (nk_button_label_styled(ctx, &trash_style, "\xEE\xB0\xAA")) {
    if (ui->selected_node == node)
      ui->selected_node = NULL;
    block_design_delete_node(block, node->id);
    if (ui->font_inter)
      nk_style_set_font(ctx, &ui->font_inter->handle);
    return; // Node deleted, stop processing it
  }
  if (ui->font_inter)
    nk_style_set_font(ctx, &ui->font_inter->handle);

  // Recursive call for children.
  // Reversed order so the topmost layer in the panel matches the front in
  // canvas. Only container nodes show their children.
  {
    Node *list[128];
    int c = 0;
    for (Node *ch = node->first_child; ch && c < 128; ch = ch->next_sibling)
      list[c++] = ch;
    // Render children in reversed order.
    for (int i = c - 1; i >= 0; i--) {
      Node *ch = list[i];
      if (!node->is_container)
        continue; // skip children for non-containers
      if (node->folded)
        continue; // respect fold for containers
      ui_render_layer_item(ui, ctx, block, ch, depth + 1);
    }
  }
}

// =============================================================================
// LAYER INSPECTOR — fixed panel shown below the layer tree when a node is
// selected.  Properties here are quick-access; full editing is in Properties.
// =============================================================================
void ui_render_layer_inspector(UIHost *ui, struct nk_context *ctx,
                               Block *block) {
  Node *node = ui->selected_node;
  if (!node || !block)
    return;

  // ── Row 1: type icon + editable name ──────────────────────────────────────
  nk_layout_row_template_begin(ctx, 24);
  nk_layout_row_template_push_static(ctx, 22.0f); // type icon
  nk_layout_row_template_push_dynamic(ctx);       // name field
  nk_layout_row_template_end(ctx);

  if (ui->font_icons)
    nk_style_set_font(ctx, &ui->font_icons->handle);
  {
    struct nk_style_button is = ctx->style.button;
    is.normal = is.hover = is.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    is.border = 0;
    is.padding = nk_vec2(2, 2);
    is.text_normal = is.text_hover = is.text_active = COLOR_ACCENT;
    nk_button_label_styled(ctx, &is, node_type_icon(node));
  }
  if (ui->font_inter)
    nk_style_set_font(ctx, &ui->font_inter->handle);

  // Edit name directly in node->name
  {
    struct nk_style_edit ne = ctx->style.edit;
    ne.normal.data.color = nk_rgba(0, 0, 0, 0);
    ne.hover.data.color = nk_rgba(0, 0, 0, 10);
    ne.active.data.color = nk_rgba(255, 255, 255, 255);
    ne.text_normal = COLOR_TEXT;
    ne.text_active = nk_rgb(0, 0, 0);
    ne.border = 0.0f;
    struct nk_style_edit old_ne = ctx->style.edit;
    ctx->style.edit = ne;
    nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, node->name, 63,
                                   nk_filter_default);
    ctx->style.edit = old_ne;
    // If tree-inline rename was active for this node, cancel it
    if (ui->renaming_node_id == node->id)
      ui->renaming_node_id = 0;
  }

  // ── Row 2: opacity slider ──────────────────────────────────────────────────
  nk_layout_row_dynamic(ctx, 20, 1);
  nk_property_float(ctx, "Opacity", 0.0f, &node->opacity, 1.0f, 0.05f, 0.01f);

  // ── Row 3: fill swatch | label  + stroke swatch | width ───────────────────
  nk_layout_row_template_begin(ctx, 22);
  nk_layout_row_template_push_static(ctx, 13.0f); // fill swatch
  nk_layout_row_template_push_dynamic(ctx);       // "Fill" label
  nk_layout_row_template_push_static(ctx, 13.0f); // stroke swatch
  nk_layout_row_template_push_static(ctx, 58.0f); // stroke width
  nk_layout_row_template_end(ctx);

  struct nk_command_buffer *insp_cb = nk_window_get_canvas(ctx);

  // Fill swatch
  {
    struct nk_rect sr = nk_widget_bounds(ctx);
    nk_spacing(ctx, 1);
    uint32_t fc = node->fill_color;
    float s = 9.0f;
    float fx = sr.x + (sr.w - s) * 0.5f, fy = sr.y + (sr.h - s) * 0.5f;
    nk_fill_rect(
        insp_cb, nk_rect(fx, fy, s, s), 2.0f,
        nk_rgba((fc >> 24) & 0xFF, (fc >> 16) & 0xFF, (fc >> 8) & 0xFF, 230));
    nk_stroke_rect(insp_cb, nk_rect(fx, fy, s, s), 2.0f, 0.5f,
                   nk_rgba(0, 0, 0, 40));
  }
  nk_label_colored(ctx, "Fill", NK_TEXT_LEFT, nk_rgb(120, 120, 135));

  // Stroke swatch
  {
    struct nk_rect sr = nk_widget_bounds(ctx);
    nk_spacing(ctx, 1);
    uint32_t sc = node->stroke_color;
    float s = 9.0f;
    float fx = sr.x + (sr.w - s) * 0.5f, fy = sr.y + (sr.h - s) * 0.5f;
    nk_fill_rect(
        insp_cb, nk_rect(fx, fy, s, s), 2.0f,
        nk_rgba((sc >> 24) & 0xFF, (sc >> 16) & 0xFF, (sc >> 8) & 0xFF, 230));
    nk_stroke_rect(insp_cb, nk_rect(fx, fy, s, s), 2.0f, 0.5f,
                   nk_rgba(0, 0, 0, 40));
  }
  nk_property_float(ctx, "W:", 0.0f, &node->stroke_width, 100.0f, 0.5f, 0.25f);

  // ── Row 4: type-specific property ─────────────────────────────────────────
  nk_layout_row_dynamic(ctx, 24, 1);
  if (node->type == NODE_TYPE_TEXT) {
    nk_property_float(ctx, "Size", 6.0f, &node->font_size, 200.0f, 1.0f, 0.5f);
  } else if (node->is_container) {
    nk_checkbox_label(ctx, "Clip content", &node->clip_content);
  } else {
    nk_property_float(ctx, "Radius", 0.0f, &node->corner_radius, 200.0f, 1.0f,
                      0.5f);
  }

  // ── Row 5: action buttons ─────────────────────────────────────────────────
  nk_layout_row_dynamic(ctx, 25, 2);

  // Copy
  {
    struct nk_style_button ab = ctx->style.button;
    ab.rounding = 4.0f;
    ab.border = 1.0f;
    ab.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    ab.hover = nk_style_item_color(nk_rgba(0, 0, 0, 10));
    ab.text_normal = nk_rgb(110, 110, 125);
    ab.text_hover = nk_rgb(60, 60, 75);
    ab.border_color = nk_rgba(0, 0, 0, 30);
    if (nk_button_label_styled(ctx, &ab, "Duplicate"))
      block_design_duplicate_node(block, node->id);
  }

  // Delete
  {
    struct nk_style_button db = ctx->style.button;
    db.rounding = 4.0f;
    db.border = 1.0f;
    db.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    db.hover = nk_style_item_color(nk_rgba(220, 60, 60, 12));
    db.text_normal = nk_rgb(200, 80, 80);
    db.text_hover = nk_rgb(230, 50, 50);
    db.border_color = nk_rgba(200, 80, 80, 100);
    if (nk_button_label_styled(ctx, &db, "Delete")) {
      ui->selected_node = NULL;
      block_design_delete_node(block, node->id);
    }
  }
}

// =============================================================================
// LAYER TREE
// =============================================================================
void ui_render_sidebar_layers(UIHost *ui, struct nk_context *ctx,
                              Block *block) {
  Node *root = NULL;
  if (block->type == BLOCK_DESIGN)
    root = block->data.design.root;

  if (!root)
    return;

  ui->hovered_node_id = 0;

  // Root level children
  Node *nodes[256];
  int count = 0;
  for (Node *node = root->first_child; node && count < 256;
       node = node->next_sibling) {
    nodes[count++] = node;
  }

  for (int i = count - 1; i >= 0; i--) {
    ui_render_layer_item(ui, ctx, block, nodes[i], 0);
  }

  // --- Finalize Drag & Drop ---
  if (ui->sidebar_drag_node_id != 0 &&
      nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT)) {
    if (ui->sidebar_drag_started && ui->sidebar_drop_target_id != 0) {
      Node *drag_node = block_design_find_node(block, ui->sidebar_drag_node_id);
      Node *target_node =
          block_design_find_node(block, ui->sidebar_drop_target_id);
      if (drag_node && target_node && drag_node != target_node) {
        // Find target row mid-point to decide before/after
        // This is tricky because we don't have the target's rect here easily.
        // But we know the last hovered row was the target.
        // For simplicity, we'll use the mouse position vs target mid-point
        // if we were able to store it, or just a simple 'insert before'.
        // Since we are in the sidebar list (Last Child first),
        // dragging 'Up' means moving to a later position in the list.

        // We'll use a simplified 'insert before' for now as proof of concept
        node_insert_before(drag_node, target_node);
        ui_request_redraw(ui);
      }
    }
    ui->sidebar_drag_node_id = 0;
    ui->sidebar_drag_started = 0;
    ui->sidebar_drop_target_id = 0;
  }

  // Reset drop target if mouse not down
  if (!nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
    ui->sidebar_drop_target_id = 0;
  }
}

void ui_render_sidebar_shape_selection(UIHost *ui, struct nk_context *ctx) {
  if (!ui->selected_node) {
    ui->sidebar_mode = 0;
    return;
  }

  nk_layout_row_dynamic(ctx, 30, 1);
  if (nk_button_label(ctx, "< Back to Properties")) {
    ui->sidebar_mode = 0;
  }

  nk_layout_row_dynamic(ctx, 20, 1);
  nk_spacing(ctx, 1);

  nk_layout_row_dynamic(ctx, 30, 1);
  nk_label(ctx, "Choose Shape:", NK_TEXT_LEFT);

  // Grid: 3 items per row
  NodeType types[] = {NODE_TYPE_RECT,    NODE_TYPE_ELLIPSE,  NODE_TYPE_TRIANGLE,
                      NODE_TYPE_OCTAGON, NODE_TYPE_PENTAGON, NODE_TYPE_HEXAGON,
                      NODE_TYPE_HEART,   NODE_TYPE_LINE};
  const char *labels[] = {"Rect",     "Ellipse", "Triangle", "Octagon",
                          "Pentagon", "Hexagon", "Heart",    "Line"};

  nk_layout_row_dynamic(ctx, 45, 3);
  for (int i = 0; i < 8; i++) {
    if (nk_button_label(ctx, labels[i])) {
      ui->selected_node->type = types[i];
      ui->sidebar_mode = 0;
    }
  }
}

void recursive_convert_to_path(Node *node) {
  if (!node)
    return;

  // Convert current node only if it's a basic geometry shape
  // Note: We avoid converting TEXT or IMAGE or existing PATHs
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

void canvas_render_toolbar(UIHost *ui, struct nk_context *ctx,
                           struct nk_rect bounds, Block *block) {
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  // Design gets an extra "Table layout" button, canvas has one fewer
  int tool_count = (block->type == BLOCK_DESIGN) ? 5 : 4;
  const float toolbar_w =
      tool_count * btn_size + (tool_count - 1) * btn_spacing + 24.0f;
  const float toolbar_h = 52.0f;
  const float margin = 20.0f;

  float toolbar_x = bounds.x + (bounds.w - toolbar_w) / 2.0f;
  float toolbar_y = bounds.y + bounds.h - toolbar_h - margin;

  struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
  struct nk_color toolbar_bg = {255, 255, 255, 255};
  struct nk_color toolbar_border = {220, 220, 220, 255};

  CanvasInteraction *cs = &ui->canvas_state;

  // Shadow
  nk_fill_rect(canvas_buf,
               nk_rect(toolbar_x + 2, toolbar_y + 4, toolbar_w, toolbar_h),
               26.0f, nk_rgba(0, 0, 0, 20));

  // Background
  nk_fill_rect(canvas_buf, nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h),
               26.0f, toolbar_bg);
  nk_stroke_rect(canvas_buf,
                 nk_rect(toolbar_x, toolbar_y, toolbar_w, toolbar_h), 26.0f,
                 1.0f, toolbar_border);

  float btn_x = toolbar_x + 12.0f;
  float btn_y = toolbar_y + (toolbar_h - btn_size) / 2.0f;

  // 7 buttons (canvas) or 8 buttons (design): last button is Linked Text
  const char *tool_icons[] = {
      ICON_SHAPE, ICON_TEXT, ICON_IMAGE, ICON_CHART,
      "\xEF\x87\x9E"}; // U+F1DE sliders — insert table layout

  for (int i = 0; i < tool_count; i++) {
    struct nk_rect btn = nk_rect(btn_x + i * (btn_size + btn_spacing), btn_y,
                                 btn_size, btn_size);

    // Map button index to category (skip CANVAS_CAT_SELECT)
    int is_active = (cs->active_cat == (CanvasCategory)(i + 1));
    if (i == 3)
      is_active = (cs->pending_create == 11); // Chart active state (transient)
    if (i == 4)
      is_active = 0; // Table layout — one-shot, never stays active

    int hovered = (ui->mouse_x >= btn.x && ui->mouse_x < btn.x + btn.w &&
                   ui->mouse_y >= btn.y && ui->mouse_y < btn.y + btn.h);

    struct nk_color bg_color = is_active ? nk_rgba(110, 110, 130, 25)
                               : hovered ? nk_rgba(0, 0, 0, 10)
                                         : nk_rgba(0, 0, 0, 0);
    nk_fill_rect(canvas_buf, btn, 8.0f, bg_color);

    struct nk_color icon_color = is_active ? COLOR_ACCENT : nk_rgb(60, 60, 60);

    if (ui->font_icons) {
      nk_draw_text(canvas_buf,
                   nk_rect(btn.x + 8, btn.y + 8, btn_size - 16, btn_size - 16),
                   tool_icons[i], strlen(tool_icons[i]),
                   &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), icon_color);
    }

    if (hovered && cs->mouse_just_pressed) {
      CanvasCategory cat = (CanvasCategory)(i + 1); // Skip CANVAS_CAT_SELECT
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
      } else if (i == 3) { // Chart button (direct action)
        cs->pending_create = 11;
        cs->show_sub_menu = 0;
      } else if (i == 4) { // Table layout button (direct action)
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
      canvas_render_sub_menu(ui, ctx, canvas_buf, btn.x, btn.y,
                             (CanvasCategory)(i + 1)); // Skip CANVAS_CAT_SELECT
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

  // Use the same styles as canvas for consistency
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
  SDL_Texture *tex;
  int width;
  int height;
  uint32_t last_used;
  int in_use;
} TextureCacheEntry;

#define TEX_CACHE_SIZE 64
TextureCacheEntry tex_cache[TEX_CACHE_SIZE];
uint32_t tex_cache_counter = 0;

SDL_Texture *get_image_texture(UIHost *ui, const char *path) {
  if (!path || !path[0])
    return NULL;

  tex_cache_counter++;

  // Check cache
  for (int i = 0; i < TEX_CACHE_SIZE; i++) {
    if (tex_cache[i].in_use && strcmp(tex_cache[i].path, path) == 0) {
      tex_cache[i].last_used = tex_cache_counter;
      return tex_cache[i].tex;
    }
  }

  // Not in cache, load it using macOS native loader
  int w, h;
  void *pixels = image_load_rgba(path, &w, &h);

  ASLAN_DEBUG("get_image_texture: trying path '%s' -> %p", path, pixels);

  if (!pixels) {
    // Try relative to assets if not already starting with assets/
    if (strncmp(path, "assets/", 7) != 0) {
      char full_path[512];
      snprintf(full_path, sizeof(full_path), "assets/%s", path);
      pixels = image_load_rgba(full_path, &w, &h);
      ASLAN_DEBUG("get_image_texture: trying full_path '%s' -> %p", full_path,
                  pixels);
    }
  }

  if (!pixels) {
    ASLAN_ERROR("get_image_texture failed to load: %s", path);
    return NULL;
  }

  // Create texture
  SDL_Texture *tex = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, w, h);
  if (tex) {
    SDL_UpdateTexture(tex, NULL, pixels, w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  }

  free(pixels);

  if (!tex)
    return NULL;

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
    if (tex_cache[slot].tex) {
      SDL_DestroyTexture(tex_cache[slot].tex);
    }
  }

  // Store in cache
  strncpy(tex_cache[slot].path, path, sizeof(tex_cache[slot].path) - 1);
  tex_cache[slot].tex = tex;
  tex_cache[slot].width = w;
  tex_cache[slot].height = h;
  tex_cache[slot].last_used = tex_cache_counter;
  tex_cache[slot].in_use = 1;

  return tex;
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

  // Offset correction to keep points stable while moving the node's origin
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
    node->type = NODE_TYPE_PATH; // must be set so bezier handles activate
  }
}

Node *design_find_node_at(DesignData *dd, float x, float y) {
  if (!dd->root)
    return NULL;

  Node *hit = NULL;
  for (Node *node = dd->root->first_child; node; node = node->next_sibling) {
    if (!node->visible)
      continue;

    // Stroke/path nodes have thin bounding boxes — add hit padding
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
      // Rotated hit test: transform (x,y) to local space
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
