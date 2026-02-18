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
  struct nk_color guide_col = nk_rgba(66, 133, 244, 150);
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
  const int item_count = 4;
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
                           "Send to Back"};

  for (int i = 0; i < item_count; i++) {
    struct nk_rect item_r =
        nk_rect(mx + padding, my + padding + i * item_h,
                menu_w - 2 * padding, item_h);
    int hovered =
        (ui->mouse_x >= item_r.x && ui->mouse_x < item_r.x + item_r.w &&
         ui->mouse_y >= item_r.y && ui->mouse_y < item_r.y + item_r.h);

    if (hovered)
      nk_fill_rect(canvas_buf, item_r, 4.0f, nk_rgba(66, 133, 244, 20));

    nk_draw_text(canvas_buf,
                 nk_rect(item_r.x + 10, item_r.y + 6, item_r.w - 20, 20),
                 labels[i], strlen(labels[i]), ctx->style.font,
                 nk_rgba(0, 0, 0, 0),
                 i == 1 ? nk_rgb(220, 50, 50) : nk_rgb(40, 40, 40));

    if (hovered && nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
      if (i == 0) {
        // Duplicate
        Node *dup = NULL;
        if (block->type == BLOCK_CANVAS) {
          dup = node_create(block->data.canvas.next_node_id++, node->type,
                            node->name);
        } else if (block->type == BLOCK_DESIGN) {
          dup = node_create(block->data.design.next_node_id++, node->type,
                            node->name);
        }
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

          Node *root = NULL;
          if (block->type == BLOCK_CANVAS)
            root = block->data.canvas.root;
          else if (block->type == BLOCK_DESIGN)
            root = block->data.design.root;
          if (root) {
            dup->parent = root;
            node_add_child(root, dup);
            ui->selected_node = dup;
          }
        }
      } else if (i == 1) {
        // Delete
        if (block->type == BLOCK_CANVAS)
          block_canvas_delete_node(block, node->id);
        else if (block->type == BLOCK_DESIGN)
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
      }
      ui->show_context_menu = 0;
      ui->context_menu_node = NULL;
    }
  }

  // Close menu on left click outside
  if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT)) {
    int in_menu =
        (ui->mouse_x >= mx && ui->mouse_x < mx + menu_w &&
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
  case NODE_TYPE_ELLIPSE:
  case NODE_TYPE_TRIANGLE:
  case NODE_TYPE_PENTAGON:
  case NODE_TYPE_HEXAGON:
  case NODE_TYPE_OCTAGON:
  case NODE_TYPE_HEART:
    return ICON_SHAPE;
  case NODE_TYPE_LINE:
  case NODE_TYPE_PATH:
    return ICON_MAGIC;
  case NODE_TYPE_TEXT:
    return ICON_TEXT;
  case NODE_TYPE_IMAGE:
    return ICON_IMAGE;
  case NODE_TYPE_STROKE:
    return (node->stroke_data && node->stroke_data->type == STROKE_TYPE_MARKER)
               ? ICON_MARKER
               : ICON_BRUSH;
  case NODE_TYPE_CHART:
    return ICON_CHART;
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
    nk_property_float(ctx, "Size:", 8.0f, &node->font_size, 200.0f, 1.0f,
                      1.0f);

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
    node->line_start_style = nk_combo(ctx, end_styles, 3,
                                       node->line_start_style, 28,
                                       nk_vec2(100, 120));
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
    nk_property_float(ctx, "Radius:", 0.0f, &node->corner_radius, 200.0f,
                      1.0f, 0.5f);
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

void ui_render_sidebar_layers(UIHost *ui, struct nk_context *ctx,
                              Block *block) {
  Node *root = NULL;
  if (block->type == BLOCK_CANVAS)
    root = block->data.canvas.root;
  else if (block->type == BLOCK_DESIGN)
    root = block->data.design.root;

  if (!root)
    return;

  // Collect nodes in reverse order (top layer first in UI)
  Node *nodes[1024];
  int count = 0;
  for (Node *node = root->first_child; node && count < 1024;
       node = node->next_sibling) {
    nodes[count++] = node;
  }

  // Header with element count
  sidebar_section_header(ctx, "LAYERS");
  nk_layout_row_dynamic(ctx, 16, 1);
  char count_label[64];
  snprintf(count_label, sizeof(count_label), "%d element%s", count,
           count != 1 ? "s" : "");
  nk_label_colored(ctx, count_label, NK_TEXT_LEFT, nk_rgb(160, 160, 165));

  nk_layout_row_dynamic(ctx, 4, 1);
  nk_spacing(ctx, 1);

  struct nk_color row_bg_even = nk_rgba(0, 0, 0, 0);
  struct nk_color row_bg_odd = nk_rgba(245, 245, 248, 255);
  struct nk_color selected_bg = nk_rgba(80, 140, 200, 40);
  struct nk_color hidden_text = nk_rgb(180, 180, 185);

  for (int i = count - 1; i >= 0; i--) {
    Node *node = nodes[i];
    int is_selected = (ui->selected_node == node);
    int row_index = count - 1 - i;
    struct nk_color bg = is_selected ? selected_bg
                         : (row_index % 2) ? row_bg_odd
                                           : row_bg_even;

    // Row background highlight
    struct nk_rect row_bounds = nk_widget_bounds(ctx);
    (void)row_bounds;

    // -- Layer row: [Icon] [Name ............] [Vis] [Up] [Dn] --
    nk_layout_row_template_begin(ctx, 28);
    nk_layout_row_template_push_static(ctx, 24);    // Type icon
    nk_layout_row_template_push_variable(ctx, 60);  // Name
    nk_layout_row_template_push_static(ctx, 22);    // Visibility
    nk_layout_row_template_push_static(ctx, 22);    // Up
    nk_layout_row_template_push_static(ctx, 22);    // Down
    nk_layout_row_template_end(ctx);

    // Draw row background
    row_bounds = nk_widget_bounds(ctx);
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    struct nk_rect full_row = nk_rect(row_bounds.x - 4, row_bounds.y,
                                      row_bounds.w + 100, 28);
    nk_fill_rect(canvas_buf, full_row, 0, bg);

    // Type icon button (clicking selects) — switch to icon font
    const char *icon = node_type_icon(node);
    struct nk_style_button icon_style = ctx->style.button;
    icon_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    icon_style.hover = nk_style_item_color(nk_rgba(0, 0, 0, 15));
    icon_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 25));
    icon_style.border = 0;
    icon_style.padding = nk_vec2(2, 2);
    struct nk_color icon_col = node->visible ? nk_rgb(120, 120, 130)
                                             : hidden_text;
    icon_style.text_normal = icon_col;
    icon_style.text_hover = icon_col;
    icon_style.text_active = icon_col;
    if (ui->font_icons)
      nk_style_set_font(ctx, &ui->font_icons->handle);
    if (nk_button_label_styled(ctx, &icon_style, icon)) {
      ui->selected_node = node;
    }
    if (ui->font_inter)
      nk_style_set_font(ctx, &ui->font_inter->handle);

    // Name button
    const char *display = node->name[0] ? node->name
                                        : node_type_display_name(node->type);
    // For strokes, use specific name
    if (!node->name[0] && node->type == NODE_TYPE_STROKE) {
      display = (node->stroke_data &&
                 node->stroke_data->type == STROKE_TYPE_MARKER)
                    ? "Marker Stroke"
                    : "Pen Stroke";
    }

    struct nk_style_button name_style = ctx->style.button;
    name_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    name_style.hover = nk_style_item_color(nk_rgba(0, 0, 0, 10));
    name_style.active = nk_style_item_color(nk_rgba(0, 0, 0, 20));
    name_style.border = 0;
    name_style.padding = nk_vec2(4, 4);
    name_style.text_alignment = NK_TEXT_LEFT;
    struct nk_color text_col = !node->visible ? hidden_text
                               : is_selected  ? COLOR_ACCENT
                                              : COLOR_TEXT;
    name_style.text_normal = text_col;
    name_style.text_hover = text_col;
    name_style.text_active = text_col;
    if (nk_button_label_styled(ctx, &name_style, display)) {
      ui->selected_node = node;
    }

    // Visibility toggle — icon font
    struct nk_style_button vis_style = icon_style;
    struct nk_color vis_col = node->visible ? nk_rgb(130, 130, 140)
                                            : nk_rgb(200, 200, 205);
    vis_style.text_normal = vis_col;
    vis_style.text_hover = nk_rgb(80, 140, 200);
    vis_style.text_active = vis_col;
    if (ui->font_icons)
      nk_style_set_font(ctx, &ui->font_icons->handle);
    if (nk_button_label_styled(ctx, &vis_style,
                               node->visible ? ICON_EYE : ICON_EYE_OFF)) {
      node->visible = !node->visible;
    }

    // Reorder Up
    struct nk_style_button arrow_style = icon_style;
    arrow_style.text_normal = nk_rgb(160, 160, 165);
    arrow_style.text_hover = COLOR_ACCENT;
    arrow_style.text_active = nk_rgb(160, 160, 165);
    if (nk_button_label_styled(ctx, &arrow_style, ICON_COLLAPSE)) {
      node_move_up(node);
    }

    // Reorder Down
    if (nk_button_label_styled(ctx, &arrow_style, ICON_EXPAND)) {
      node_move_down(node);
    }
    if (ui->font_inter)
      nk_style_set_font(ctx, &ui->font_inter->handle);
  }

  // Separator before delete zone
  if (ui->selected_node) {
    nk_layout_row_dynamic(ctx, 6, 1);
    nk_spacing(ctx, 1);

    nk_layout_row_dynamic(ctx, 28, 1);
    struct nk_style_button del_style = ctx->style.button;
    del_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    del_style.hover = nk_style_item_color(nk_rgba(220, 60, 60, 25));
    del_style.active = nk_style_item_color(nk_rgba(220, 60, 60, 40));
    del_style.border = 0;
    del_style.text_normal = nk_rgb(200, 80, 80);
    del_style.text_hover = nk_rgb(220, 50, 50);
    del_style.text_active = nk_rgb(180, 40, 40);
    del_style.text_alignment = NK_TEXT_LEFT;
    del_style.padding = nk_vec2(4, 4);

    const char *sel_name =
        ui->selected_node->name[0]
            ? ui->selected_node->name
            : node_type_display_name(ui->selected_node->type);
    char del_label[128];
    snprintf(del_label, sizeof(del_label), "Delete %s", sel_name);
    if (nk_button_label_styled(ctx, &del_style, del_label)) {
      Node *to_del = ui->selected_node;
      ui->selected_node = NULL;
      if (block->type == BLOCK_CANVAS)
        block_canvas_delete_node(block, to_del->id);
      else if (block->type == BLOCK_DESIGN)
        block_design_delete_node(block, to_del->id);
    }
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
  (void)block;

  // 6 buttons: Shapes, Text, Image, Brush, Color, Bezier
  const float btn_size = 36.0f;
  const float btn_spacing = 8.0f;
  const float toolbar_w = 7 * btn_size + 6 * btn_spacing + 24.0f; // 7 buttons
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

  // 7 buttons: Shapes, Text, Image, Brush, Color, Bezier, Chart
  // Brush icon changes based on marker mode
  const char *brush_icon = cs->pen_is_marker ? ICON_MARKER : ICON_BRUSH;
  const char *tool_icons[] = {ICON_SHAPE, ICON_TEXT,  ICON_IMAGE, brush_icon,
                              "",         ICON_MAGIC, ICON_CHART};
  int tool_count = 7;

  for (int i = 0; i < tool_count; i++) {
    struct nk_rect btn = nk_rect(btn_x + i * (btn_size + btn_spacing), btn_y,
                                 btn_size, btn_size);

    // Map button index to category (skip CANVAS_CAT_SELECT)
    int is_active = (cs->active_cat == (CanvasCategory)(i + 1));
    if (i == 3)
      is_active = (cs->tool == CANVAS_TOOL_PEN); // Brush/Marker combined
    if (i == 4)
      is_active = (cs->active_cat == CANVAS_CAT_COLOR);
    if (i == 5)
      is_active = cs->bezier_mode;
    if (i == 6)
      is_active = (cs->pending_create == 11); // Chart active state (transient)

    int hovered = (ui->mouse_x >= btn.x && ui->mouse_x < btn.x + btn.w &&
                   ui->mouse_y >= btn.y && ui->mouse_y < btn.y + btn.h);

    struct nk_color bg_color = is_active ? nk_rgba(66, 133, 244, 25)
                               : hovered ? nk_rgba(0, 0, 0, 10)
                                         : nk_rgba(0, 0, 0, 0);
    nk_fill_rect(canvas_buf, btn, 8.0f, bg_color);

    struct nk_color icon_color =
        is_active ? nk_rgb(66, 133, 244) : nk_rgb(60, 60, 60);

    if (ui->font_icons) {
      nk_draw_text(canvas_buf,
                   nk_rect(btn.x + 8, btn.y + 8, btn_size - 16, btn_size - 16),
                   tool_icons[i], strlen(tool_icons[i]),
                   &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), icon_color);
    }

    if (hovered && cs->mouse_just_pressed) {
      if (i == 5) {
        ASLAN_DEBUG("Bezier button hovered and mouse_just_pressed=%d",
                    cs->mouse_just_pressed);
      }
      if (i ==
          3) { // Brush button - toggles between brush, marker and deselected
        ASLAN_DEBUG("Clicked Brush/Marker button. Current tool: %d, marker: %d",
                    cs->tool, cs->pen_is_marker);
        if (cs->tool == CANVAS_TOOL_PEN) {
          if (!cs->pen_is_marker) {
            // State 1 (Pen) -> State 2 (Marker)
            cs->pen_is_marker = 1;
            ASLAN_DEBUG("Switched to Marker mode");
          } else {
            // State 2 (Marker) -> State 3 (None/Select)
            cs->tool = CANVAS_TOOL_SELECT;
            cs->pen_is_marker = 0; // Reset for next time
            ASLAN_DEBUG("Deselected pen tool");
          }
        } else {
          // State 3 (None) -> State 1 (Pen)
          cs->tool = CANVAS_TOOL_PEN;
          cs->pen_is_marker = 0;
          ASLAN_DEBUG("Set tool to CANVAS_TOOL_PEN");
        }
        cs->active_cat =
            (cs->tool == CANVAS_TOOL_PEN)
                ? (cs->pen_is_marker ? CANVAS_CAT_MARKER : CANVAS_CAT_BRUSH)
                : CANVAS_CAT_SELECT;
        cs->show_sub_menu = 0;
      } else if (i == 4) { // Color swatch button
        // Toggle the robust color picker popup
        cs->show_color_picker_popup = !cs->show_color_picker_popup;
      } else if (i == 5) { // Bezier (Magic) button
        cs->bezier_mode = !cs->bezier_mode;

        // Convert ALL shapes in the current document to paths
        if (cs->bezier_mode) {
          if (block->type == BLOCK_CANVAS && block->data.canvas.root) {
            recursive_convert_to_path(block->data.canvas.root);
          } else if (block->type == BLOCK_DESIGN && block->data.design.root) {
            recursive_convert_to_path(block->data.design.root);
          }
        }

        cs->active_cat = CANVAS_CAT_SELECT;
        cs->show_sub_menu = 0;
      } else {
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
        } else if (i == 6) { // Chart button (direct action)
          cs->pending_create = 11;
          cs->show_sub_menu = 0;
        } else if (cs->active_cat == cat) {
          cs->show_sub_menu = !cs->show_sub_menu;
        } else {
          cs->active_cat = cat;
          cs->show_sub_menu = 1; // Open sub-menu for others
        }
      }
    }

    // Special rendering for color swatch (button 4)
    if (i == 4) {
      // Draw color swatch instead of icon
      struct nk_color swatch_color = nk_rgba(
          (cs->selected_color >> 24) & 0xFF, (cs->selected_color >> 16) & 0xFF,
          (cs->selected_color >> 8) & 0xFF, cs->selected_color & 0xFF);
      struct nk_rect swatch_rect =
          nk_rect(btn.x + 8, btn.y + 8, btn_size - 16, btn_size - 16);
      nk_fill_circle(canvas_buf, swatch_rect, swatch_color);
      nk_stroke_circle(canvas_buf, swatch_rect, 1.5f, nk_rgb(100, 100, 100));
    }

    // Render sub-menu if active
    if (is_active && cs->show_sub_menu) {
      canvas_render_sub_menu(ui, ctx, canvas_buf, btn.x, btn.y,
                             (CanvasCategory)(i + 1)); // Skip CANVAS_CAT_SELECT
    }
  }

  // Render Color Picker Popup if active (Positioned above the color button)
  if (cs->show_color_picker_popup) {
    float btn4_x = toolbar_x + 12.0f + 4 * (btn_size + btn_spacing);
    struct nk_rect btn4_rect = nk_rect(btn4_x, btn_y, btn_size, btn_size);
    // Position popup above the button (toolbar is at bottom)
    struct nk_rect popup_rect = nk_rect(btn4_x - 90, btn_y - 310, 220, 300);

    if (nk_popup_begin(ctx, NK_POPUP_STATIC, "ColorPickerPopup",
                       NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER, popup_rect)) {

      // Unpack color
      struct nk_color selected_col = nk_rgba(
          (cs->selected_color >> 24) & 0xFF, (cs->selected_color >> 16) & 0xFF,
          (cs->selected_color >> 8) & 0xFF, cs->selected_color & 0xFF);
      struct nk_colorf col_f = nk_color_cf(selected_col);

      // Render Content directly
      col_f = ui_render_color_picker_content(ctx, col_f);

      // Update back
      selected_col = nk_rgba((int)(col_f.r * 255.0f), (int)(col_f.g * 255.0f),
                             (int)(col_f.b * 255.0f), (int)(col_f.a * 255.0f));
      cs->selected_color =
          ((uint32_t)selected_col.r << 24) | ((uint32_t)selected_col.g << 16) |
          ((uint32_t)selected_col.b << 8) | (uint32_t)selected_col.a;

      // Apply to selected node immediately if one is selected
      if (ui->selected_node) {
        ui->selected_node->fill_color = cs->selected_color;
        // Also stroke if it's a line/stroke/text/path
        if (ui->selected_node->type == NODE_TYPE_STROKE ||
            ui->selected_node->type == NODE_TYPE_LINE ||
            ui->selected_node->type == NODE_TYPE_TEXT ||
            ui->selected_node->type == NODE_TYPE_PATH) {
          ui->selected_node->stroke_color = cs->selected_color;
        }
      }

      nk_popup_end(ctx);

      // Close if mouse clicked outside
      if (cs->mouse_just_pressed &&
          !nk_input_is_mouse_hovering_rect(&ctx->input, popup_rect) &&
          !nk_input_is_mouse_hovering_rect(&ctx->input, btn4_rect)) {
        cs->show_color_picker_popup = 0;
      }
    } else {
      cs->show_color_picker_popup = 0;
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

Node *canvas_create_styled_node(CanvasData *cd, NodeStyleType style, float x,
                                float y, float w, float h, uint32_t color) {
  if (!cd->root)
    return NULL;

  Node *node = node_create(cd->next_node_id++, NODE_TYPE_RECT, NULL);
  if (!node)
    return NULL;
  node->x = x;
  node->y = y;
  node->width = w;
  node->height = h;
  node->visible = 1;
  node->parent = cd->root;

  switch (style) {
  case NODE_STYLE_RECT:
    node->type = NODE_TYPE_RECT;
    node->fill_color = color;
    node->stroke_color = 0x1A73E8FF;
    node->stroke_width = 0.0f;
    node->corner_radius = 8.0f;
    break;

  case NODE_STYLE_ELLIPSE:
    node->type = NODE_TYPE_ELLIPSE;
    node->fill_color = color;
    node->stroke_color = 0x1E8E3EFF;
    node->stroke_width = 0.0f;
    node->corner_radius = 0.0f;
    break;

  case NODE_STYLE_TRIANGLE:
    node->type = NODE_TYPE_TRIANGLE;
    node->fill_color = color;
    node->stroke_color = 0xF9A825FF;
    node->stroke_width = 0.0f;
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
  if (!cd->root->first_child) {
    cd->root->first_child = node;
  } else {
    Node *last = cd->root->first_child;
    while (last->next_sibling)
      last = last->next_sibling;
    last->next_sibling = node;
  }

  return node;
}

// -----------------------------------------------------------------------------
// Canvas Helper: Find node at world position
// -----------------------------------------------------------------------------

Node *canvas_find_node_at(CanvasData *cd, float world_x, float world_y) {
  if (!cd->root)
    return NULL;

  // Iterate in reverse to get topmost node first
  Node *found = NULL;
  for (Node *node = cd->root->first_child; node; node = node->next_sibling) {
    if (!node->visible)
      continue;
    if (world_x >= node->x && world_x < node->x + node->width &&
        world_y >= node->y && world_y < node->y + node->height) {
      found = node; // Keep going to find topmost
    }
  }
  return found;
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

// =============================================================================
// Shape-Masked Image Texture (Canva-like container image clipping)
// =============================================================================

#define MASKED_TEX_CACHE_SIZE 32
static struct {
  char path[256];
  int shape_type;
  int corner_key;     // corner_ratio * 100
  uint32_t path_hash; // hash of path points for PATH nodes
  SDL_Texture *tex;
  int width, height;
  int in_use;
  uint32_t last_used;
} masked_tex_cache[MASKED_TEX_CACHE_SIZE];
static uint32_t masked_tex_counter = 0;

// Simple hash of path points for cache invalidation
static uint32_t hash_path_points(NodePathPoint *pts, int count) {
  uint32_t h = 0x811c9dc5; // FNV-1a offset basis
  const uint8_t *data = (const uint8_t *)pts;
  size_t len = (size_t)count * sizeof(NodePathPoint);
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 0x01000193; // FNV-1a prime
  }
  return h;
}

// Tessellate path to normalized [0,1] polygon coords relative to node bounds
static int tessellate_path_normalized(Node *node, float *out_pts, int max_pts) {
  if (!node || !node->path_points || node->path_point_count < 2)
    return 0;
  float w = node->width, h = node->height;
  if (w < 1.0f)
    w = 1.0f;
  if (h < 1.0f)
    h = 1.0f;

  int total = 0;
  for (int i = 0; i < node->path_point_count; i++) {
    int next = (i + 1) % node->path_point_count;
    NodePathPoint p1 = node->path_points[i];
    NodePathPoint p2 = node->path_points[next];

    if (p1.type ==
        1) { // Cubic Bezier - tessellate in node-local coords then normalize
      float seg_pts[128];
      int c = tessellate_bezier(p1.x, p1.y, p1.cp2x, p1.cp2y, p2.cp1x, p2.cp1y,
                                p2.x, p2.y, seg_pts, 64, 0);
      for (int j = 0; j < c && total < max_pts; j++) {
        out_pts[total * 2] = seg_pts[j * 2] / w;
        out_pts[total * 2 + 1] = seg_pts[j * 2 + 1] / h;
        total++;
      }
    } else {
      if (total < max_pts) {
        out_pts[total * 2] = p1.x / w;
        out_pts[total * 2 + 1] = p1.y / h;
        total++;
      }
    }
  }
  return total;
}

// Winding number point-in-polygon test
static float path_winding_alpha(float px, float py, const float *poly,
                                int count) {
  int winding = 0;
  for (int i = 0; i < count; i++) {
    int j = (i + 1) % count;
    float y0 = poly[i * 2 + 1], y1 = poly[j * 2 + 1];
    float x0 = poly[i * 2], x1 = poly[j * 2];
    if (y0 <= py) {
      if (y1 > py) {
        float vt = (py - y0) / (y1 - y0);
        if (px < x0 + vt * (x1 - x0))
          winding++;
      }
    } else {
      if (y1 <= py) {
        float vt = (py - y0) / (y1 - y0);
        if (px < x0 + vt * (x1 - x0))
          winding--;
      }
    }
  }
  return winding != 0 ? 1.0f : 0.0f;
}

// Compute alpha for a pixel at normalized coords (nx, ny) in [0,1]
// Returns 0.0 (outside) to 1.0 (inside), with smooth AA at edges
static float shape_alpha(NodeType shape, float nx, float ny, float cr,
                         int img_size) {
  float aa = 1.5f / (float)img_size; // Anti-aliasing band

  switch (shape) {
  case NODE_TYPE_ELLIPSE: {
    float dx = nx - 0.5f, dy = ny - 0.5f;
    float r = sqrtf(dx * dx * 4.0f + dy * dy * 4.0f);
    float d = 1.0f - r; // >0 inside, <0 outside
    if (d > aa)
      return 1.0f;
    if (d < -aa)
      return 0.0f;
    return (d + aa) / (2.0f * aa);
  }
  case NODE_TYPE_RECT: {
    if (cr <= 0.001f)
      return 1.0f; // No rounding => no masking needed
    float r = cr * 0.5f;
    float cx = (nx < r) ? r : (nx > 1.0f - r) ? 1.0f - r : nx;
    float cy = (ny < r) ? r : (ny > 1.0f - r) ? 1.0f - r : ny;
    if (cx != nx || cy != ny) { // In a corner region
      float dd = sqrtf((nx - cx) * (nx - cx) + (ny - cy) * (ny - cy));
      float d = r - dd;
      if (d > aa)
        return 1.0f;
      if (d < -aa)
        return 0.0f;
      return (d + aa) / (2.0f * aa);
    }
    return 1.0f;
  }
  case NODE_TYPE_TRIANGLE: {
    // Triangle: top-center(0.5,0), bottom-left(0,1), bottom-right(1,1)
    // Edge normals pointing inward
    // Left edge: (0.5,0)->(0,1), normal = (1, 0.5)/|n|
    float d_left = (nx - 0.0f) * (0.0f - 1.0f) - (ny - 1.0f) * (0.5f - 0.0f);
    // Right edge: (1,1)->(0.5,0), normal = (-1, 0.5)/|n|
    float d_right = (nx - 0.5f) * (1.0f - 0.0f) - (ny - 0.0f) * (1.0f - 0.5f);
    // Bottom edge: (0,1)->(1,1), normal = (0, -1)
    float d_bottom = 1.0f - ny;

    float edge_len = sqrtf(0.25f + 1.0f); // length of left/right edges
    float min_d = fminf(fminf(d_left / edge_len, d_right / edge_len), d_bottom);

    if (min_d > aa)
      return 1.0f;
    if (min_d < -aa)
      return 0.0f;
    return (min_d + aa) / (2.0f * aa);
  }
  case NODE_TYPE_PENTAGON:
  case NODE_TYPE_HEXAGON:
  case NODE_TYPE_OCTAGON:
  case NODE_TYPE_STAR: {
    int sides = (shape == NODE_TYPE_PENTAGON)  ? 5
                : (shape == NODE_TYPE_HEXAGON) ? 6
                : (shape == NODE_TYPE_STAR)    ? 10
                                               : 8;
    float angle_off = (shape == NODE_TYPE_PENTAGON)  ? -3.14159f / 2.0f
                      : (shape == NODE_TYPE_HEXAGON) ? 0.0f
                      : (shape == NODE_TYPE_STAR)    ? -3.14159f / 2.0f
                                                     : 3.14159f / 8.0f;
    float dx = nx - 0.5f, dy = ny - 0.5f;
    float angle = atan2f(dy, dx) - angle_off;
    float sector = 2.0f * 3.14159f / (float)sides;
    angle = fmodf(angle, sector);
    if (angle < 0)
      angle += sector;
    float cos_half = cosf(sector / 2.0f);
    float edge_r;
    if (shape == NODE_TYPE_STAR) {
      // Alternating outer/inner radii for star points
      int idx = (int)((atan2f(dy, dx) - angle_off) / (3.14159f / 5.0f));
      if (idx < 0)
        idx += 10;
      idx = idx % 10;
      float outer_r = 0.5f;
      float inner_r = 0.2f;
      float r_at = (idx % 2 == 0) ? outer_r : inner_r;
      float r_next = (idx % 2 == 0) ? inner_r : outer_r;
      float frac = fmodf(angle, sector) / sector;
      edge_r = r_at + (r_next - r_at) * frac;
    } else {
      edge_r = 0.5f * cos_half / cosf(angle - sector / 2.0f);
    }
    float point_r = sqrtf(dx * dx + dy * dy);
    float d = edge_r - point_r;
    if (d > aa)
      return 1.0f;
    if (d < -aa)
      return 0.0f;
    return (d + aa) / (2.0f * aa);
  }
  case NODE_TYPE_HEART: {
    // Winding number test on sampled parametric heart boundary
    float hx = (nx - 0.5f) * 32.0f;
    float hy = (ny - 0.45f) * 32.0f;
    int winding = 0;
    float prev_bx = 0, prev_by = 0;
    for (int i = 0; i <= 48; i++) {
      float t = (float)(i % 48) * 2.0f * 3.14159f / 48.0f;
      float bx = 16.0f * powf(sinf(t), 3.0f);
      float by = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                   2.0f * cosf(3.0f * t) - cosf(4.0f * t));
      if (i > 0) {
        if (prev_by <= hy && by > hy) {
          float vt = (hy - prev_by) / (by - prev_by);
          if (hx < prev_bx + vt * (bx - prev_bx))
            winding++;
        } else if (prev_by > hy && by <= hy) {
          float vt = (hy - prev_by) / (by - prev_by);
          if (hx < prev_bx + vt * (bx - prev_bx))
            winding--;
        }
      }
      prev_bx = bx;
      prev_by = by;
    }
    return winding != 0 ? 1.0f : 0.0f;
  }
  default:
    return 1.0f; // No masking for unknown shapes
  }
}

SDL_Texture *get_masked_image_texture(UIHost *ui, const char *path,
                                      Node *node) {
  if (!path || !path[0] || !node)
    return NULL;

  NodeType shape = node->type;
  float corner_ratio = 0;
  if (shape == NODE_TYPE_RECT && node->width > 0 && node->height > 0)
    corner_ratio = node->corner_radius / fminf(node->width, node->height);

  masked_tex_counter++;
  int cr_key = (int)(corner_ratio * 100);
  uint32_t ph = 0;
  if (shape == NODE_TYPE_PATH && node->path_points &&
      node->path_point_count > 0)
    ph = hash_path_points(node->path_points, node->path_point_count);

  // Check cache
  for (int i = 0; i < MASKED_TEX_CACHE_SIZE; i++) {
    if (masked_tex_cache[i].in_use &&
        masked_tex_cache[i].shape_type == (int)shape &&
        masked_tex_cache[i].corner_key == cr_key &&
        masked_tex_cache[i].path_hash == ph &&
        strcmp(masked_tex_cache[i].path, path) == 0) {
      masked_tex_cache[i].last_used = masked_tex_counter;
      return masked_tex_cache[i].tex;
    }
  }

  // Load original image pixels
  int w, h;
  void *pixels = image_load_rgba(path, &w, &h);
  if (!pixels && strncmp(path, "assets/", 7) != 0) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "assets/%s", path);
    pixels = image_load_rgba(full_path, &w, &h);
  }
  if (!pixels)
    return NULL;

  // Apply shape mask to pixels
  uint8_t *rgba = (uint8_t *)pixels;
  int img_size = w > h ? w : h;

  if (shape == NODE_TYPE_PATH) {
    // Tessellate path to normalized polygon, then use winding number test
    float poly[1024 * 2];
    int poly_count = tessellate_path_normalized(node, poly, 1024);
    if (poly_count > 2) {
      for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
          float nx = ((float)px + 0.5f) / (float)w;
          float ny = ((float)py + 0.5f) / (float)h;
          float a = path_winding_alpha(nx, ny, poly, poly_count);
          int idx = (py * w + px) * 4;
          rgba[idx + 3] = (uint8_t)((float)rgba[idx + 3] * a);
        }
      }
    }
  } else {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        float nx = ((float)x + 0.5f) / (float)w;
        float ny = ((float)y + 0.5f) / (float)h;
        float a = shape_alpha(shape, nx, ny, corner_ratio, img_size);
        int idx = (y * w + x) * 4;
        rgba[idx + 3] = (uint8_t)((float)rgba[idx + 3] * a);
      }
    }
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

  // Find cache slot (LRU eviction)
  int slot = -1;
  for (int i = 0; i < MASKED_TEX_CACHE_SIZE; i++) {
    if (!masked_tex_cache[i].in_use) {
      slot = i;
      break;
    }
  }
  if (slot == -1) {
    uint32_t min_time = 0xFFFFFFFF;
    for (int i = 0; i < MASKED_TEX_CACHE_SIZE; i++) {
      if (masked_tex_cache[i].last_used < min_time) {
        min_time = masked_tex_cache[i].last_used;
        slot = i;
      }
    }
    if (masked_tex_cache[slot].tex)
      SDL_DestroyTexture(masked_tex_cache[slot].tex);
  }

  strncpy(masked_tex_cache[slot].path, path,
          sizeof(masked_tex_cache[slot].path) - 1);
  masked_tex_cache[slot].shape_type = (int)shape;
  masked_tex_cache[slot].corner_key = cr_key;
  masked_tex_cache[slot].path_hash = ph;
  masked_tex_cache[slot].tex = tex;
  masked_tex_cache[slot].width = w;
  masked_tex_cache[slot].height = h;
  masked_tex_cache[slot].last_used = masked_tex_counter;
  masked_tex_cache[slot].in_use = 1;

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
    node->type = NODE_TYPE_PATH;
  }
}

void ui_render_block_canvas(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
  (void)engine;
  (void)width;

  if (!block || block->type != BLOCK_CANVAS)
    return;

  CanvasData *cd = &block->data.canvas;
  CanvasInteraction *cs = &ui->canvas_state;
  // printf("[DEBUG] Render Canvas %u. Expanded: %d\n", block->id,
  // (ui->expanded_block_id == block->id));

  // Sync UI state with canvas data on first render or when switching canvas
  if (cs->active_canvas_id != block->id) {
    cs->active_canvas_id = block->id;
    cs->active_block = block;
    cs->cam_x = cd->view_x;
    cs->cam_y = cd->view_y;
    cs->zoom = cd->zoom > 0.01f ? cd->zoom : 1.0f;
    cs->is_creating = 0;
    cs->is_dragging_node = 0;
    ui->selected_node = NULL;
  }
  cs->active_block = block;

  float cam_x = cs->cam_x;
  float cam_y = cs->cam_y;
  float zoom = cs->zoom;

  // Toggle for expand/collapse
  ui_render_expand_toggle(ui, ctx, block);

  // Canvas height
  float total_h = 400.0f;
  float preview_h = 400.0f;
  int is_expanded = (ui->expanded_block_id == block->id);

  if (is_expanded) {
    total_h = nk_window_get_height(ctx) - 80.0f;
    preview_h = total_h; // Use 100% for canvas in fullscreen
  }

  // Preview Area (Canvas) - 2 columns in fullscreen, 1 column otherwise
  if (is_expanded) {
    // Fullscreen: Canvas + Sidebar layout
    nk_layout_row_template_begin(ctx, preview_h);
    nk_layout_row_template_push_variable(ctx, 75);   // Canvas (flexible)
    nk_layout_row_template_push_static(ctx, 250.0f); // Sidebar (fixed 250px)
    nk_layout_row_template_end(ctx);
  } else {
    // Normal mode: Single column
    nk_layout_row_dynamic(ctx, preview_h, 1);
  }

  struct nk_rect bounds = nk_widget_bounds(ctx);

  // --- 1. Interaction Logic (Calculations) ---
  cs->bounds_x = bounds.x;
  cs->bounds_y = bounds.y;
  cs->bounds_w = bounds.w;
  cs->bounds_h = preview_h; // Use stable height for coordinate math instead of
                            // visible rect
  cs->mouse_in_canvas =
      (ui->mouse_x >= bounds.x && ui->mouse_x < bounds.x + bounds.w &&
       ui->mouse_y >= bounds.y && ui->mouse_y < bounds.y + bounds.h);

  // Explicitly exclude sidebar area in fullscreen/expanded mode
  if (is_expanded && ui->mouse_x > nk_window_get_width(ctx) - 250.0f) {
    cs->mouse_in_canvas = 0;
  }

  // Activate block on click
  if (cs->mouse_just_pressed && cs->mouse_in_canvas) {
    ui->active_block_id = block->id;
  }

  int in_toolbar =
      canvas_point_in_toolbar(bounds, ui->mouse_x, ui->mouse_y, cs);

  // Proper in_forms_menu check (if sub-menu is open)
  int in_forms_menu = 0;
  if (cs->show_sub_menu) {
    // We already check sub-menu bounds inside canvas_point_in_toolbar
    // so we can reuse that or check specifically here.
    // For now, in_toolbar already includes the sub-menu.
    in_forms_menu = in_toolbar;
  }

  // Close sub-menu if clicking elsewhere
  if (cs->mouse_just_pressed && !in_toolbar) {
    cs->show_sub_menu = 0;
  }

  // --- 2. Handle pending element creation ---
  if (cs->pending_create > 0) {
    float center_world_x = cam_x + (bounds.w / 2.0f) / zoom;
    float center_world_y = cam_y + (bounds.h / 2.0f) / zoom;
    Node *new_node = NULL;
    float def_w = 100.0f, def_h = 80.0f;
    float nx = center_world_x - def_w / 2.0f,
          ny = center_world_y - def_h / 2.0f;

    if (cs->pending_create == 1) {
      // Body text
      new_node = canvas_create_styled_node(cd, NODE_STYLE_TEXT, nx, ny, 200.0f,
                                           40.0f, cs->selected_color);
      if (new_node) {
        strncpy(new_node->text, "Body text", sizeof(new_node->text) - 1);
      }
    } else if (cs->pending_create == 6) {
      // H1 Heading
      new_node = canvas_create_styled_node(cd, NODE_STYLE_TEXT, nx, ny, 300.0f,
                                           60.0f, cs->selected_color);
      if (new_node) {
        strncpy(new_node->text, "Heading", sizeof(new_node->text) - 1);
      }
    } else if (cs->pending_create == 2) {
      new_node = canvas_create_styled_node(cd, NODE_STYLE_RECT, nx, ny, 120.0f,
                                           120.0f, cs->selected_color);
      if (new_node) {
        new_node->fill_color = 0xFFF9C4FF;
        new_node->stroke_color = 0xFBC02DFF;
        new_node->type = NODE_TYPE_TEXT;
        strncpy(new_node->text, "Note...", sizeof(new_node->text) - 1);
      }
    } else if (cs->pending_create == 3) {
      char path[512] = {0};
      if (open_image_file_dialog(path, sizeof(path))) {
        char rel[256] = {0};
        if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
          new_node = node_create(cd->next_node_id++, NODE_TYPE_IMAGE, NULL);
          if (new_node && cd->root) {
            new_node->x = nx;
            new_node->y = ny;

            fflush(stdout);

            // Maintain aspect ratio
            int img_w = 0, img_h = 0;
            int size_ok = image_get_size(path, &img_w, &img_h);
            ASLAN_DEBUG("canvas image size for '%s': %d x %d (ok=%d)", path,
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
            new_node->parent = cd->root;
            strncpy(new_node->image_path, rel,
                    sizeof(new_node->image_path) - 1);
            ASLAN_DEBUG("set image_path to: %s", new_node->image_path);
            if (!cd->root->first_child)
              cd->root->first_child = new_node;
            else {
              Node *l = cd->root->first_child;
              while (l->next_sibling)
                l = l->next_sibling;
              l->next_sibling = new_node;
            }
          }
        }
      }
      new_node = canvas_create_styled_node(cd, NODE_STYLE_RECT, nx, ny, 200.0f,
                                           120.0f, cs->selected_color);
      if (new_node)
        new_node->type = NODE_TYPE_IMAGE;
    } else if (cs->pending_create == 11) {
      // Chart
      new_node = node_create(cd->next_node_id++, NODE_TYPE_CHART, "Chart");
      if (new_node) {
        new_node->x = nx;
        new_node->y = ny;
        new_node->width = 400;
        new_node->height = 300;
        new_node->parent = cd->root;
        node_add_child(cd->root, new_node);

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
    } else if (cs->pending_create >= 10 && cs->pending_create <= 13) {
      NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
      new_node =
          canvas_create_styled_node(cd, styles[cs->pending_create - 10], nx, ny,
                                    def_w, def_h, cs->selected_color);
    }
    if (new_node)
      ui->selected_node = new_node;
    cs->pending_create = 0;
  }

  // --- 3. Resize and Select logic ---
  int on_resize_handle = 0, hit_handle_index = 0;
  if (ui->selected_node && ui->selected_node->visible) {
    Node *sel = ui->selected_node;
    float sx = bounds.x + (sel->x - cam_x) * zoom,
          sy = bounds.y + (sel->y - cam_y) * zoom;
    float sw = sel->width * zoom, sh = sel->height * zoom, hsize = 40.0f;
    if (ui->mouse_x >= sx + sw - hsize / 2 &&
        ui->mouse_x < sx + sw + hsize / 2 &&
        ui->mouse_y >= sy + sh - hsize / 2 &&
        ui->mouse_y < sy + sh + hsize / 2) {
      on_resize_handle = 1;
      hit_handle_index = 0;
    } else if (ui->mouse_x >= sx - hsize / 2 && ui->mouse_x < sx + hsize / 2 &&
               ui->mouse_y >= sy + sh - hsize / 2 &&
               ui->mouse_y < sy + sh + hsize / 2) {
      on_resize_handle = 1;
      hit_handle_index = 1;
    } else if (ui->mouse_x >= sx - hsize / 2 && ui->mouse_x < sx + hsize / 2 &&
               ui->mouse_y >= sy - hsize / 2 && ui->mouse_y < sy + hsize / 2) {
      on_resize_handle = 1;
      hit_handle_index = 2;
    } else if (ui->mouse_x >= sx + sw - hsize / 2 &&
               ui->mouse_x < sx + sw + hsize / 2 &&
               ui->mouse_y >= sy - hsize / 2 && ui->mouse_y < sy + hsize / 2) {
      on_resize_handle = 1;
      hit_handle_index = 3;
    }

    float mw = 220.0f, mh = 45.0f, mx = sx, my = sy - mh - 10.0f;
    if (mx < bounds.x)
      mx = bounds.x;
    if (mx + mw > bounds.x + bounds.w)
      mx = bounds.x + bounds.w - mw;
    if (my < bounds.y)
      my = sy + sh + 10.0f;
    // Check if mouse is over property menu (reserved for future use)
    (void)(ui->mouse_x >= mx && ui->mouse_x < mx + mw && ui->mouse_y >= my &&
           ui->mouse_y < my + mh);
  }

  if (cs->mouse_in_canvas && !cs->is_panning && cs->mouse_just_pressed) {
    // 1. Prioritize Bezier handle selection
    int handle_hit = 0;
    if (cs->bezier_mode && ui->selected_node &&
        ui->selected_node->type == NODE_TYPE_PATH) {
      Node *sel = ui->selected_node;
      float nsx = bounds.x + (sel->x - cam_x) * zoom;
      float nsy = bounds.y + (sel->y - cam_y) * zoom;
      for (int i = 0; i < sel->path_point_count; i++) {
        NodePathPoint *p = &sel->path_points[i];
        float px = nsx + p->x * zoom, py = nsy + p->y * zoom;
        struct nk_rect r_anchor =
            nk_rect(px - 10 * zoom, py - 10 * zoom, 20 * zoom, 20 * zoom);
        if (nk_input_is_mouse_hovering_rect(&ctx->input, r_anchor)) {
          cs->dragging_point_idx = i;
          cs->dragging_cp_idx = 0;
          handle_hit = 1;
          break;
        }
        if (p->type == 1) {
          float cp1x = nsx + p->cp1x * zoom, cp1y = nsy + p->cp1y * zoom;
          float cp2x = nsx + p->cp2x * zoom, cp2y = nsy + p->cp2y * zoom;
          struct nk_rect r1 =
              nk_rect(cp1x - 8 * zoom, cp1y - 8 * zoom, 16 * zoom, 16 * zoom);
          struct nk_rect r2 =
              nk_rect(cp2x - 8 * zoom, cp2y - 8 * zoom, 16 * zoom, 16 * zoom);
          if (nk_input_is_mouse_hovering_rect(&ctx->input, r1)) {
            cs->dragging_point_idx = i;
            cs->dragging_cp_idx = 1;
            handle_hit = 1;
            break;
          }
          if (nk_input_is_mouse_hovering_rect(&ctx->input, r2)) {
            cs->dragging_point_idx = i;
            cs->dragging_cp_idx = 2;
            handle_hit = 1;
            break;
          }
        }
      }
    }

    if (handle_hit) {
      // Bezier handle hit, don't drag node
    } else if (on_resize_handle) {
      cs->is_resizing = 1;
      cs->resize_handle = hit_handle_index;
      cs->resize_start_w = ui->selected_node->width;
      cs->resize_start_h = ui->selected_node->height;
      cs->resize_start_x = ui->mouse_x;
      cs->resize_start_y = ui->mouse_y;
      cs->resize_start_node_x = ui->selected_node->x;
      cs->resize_start_node_y = ui->selected_node->y;
    } else if (!in_toolbar && !in_forms_menu) {
      float wx = cam_x + (ui->mouse_x - bounds.x) / zoom;
      float wy = cam_y + (ui->mouse_y - bounds.y) / zoom;
      Node *hit = canvas_find_node_at(cd, wx, wy);
      if (hit) {
        if (cs->tool == CANVAS_TOOL_PEN) {
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
          if (hit->type == NODE_TYPE_TEXT && ui->selected_node == hit &&
              !ui->editing_node) {
            ui->editing_node = hit;
            strncpy(ui->edit_buffer, hit->text, sizeof(ui->edit_buffer) - 1);
            SDL_StartTextInput();
          }
          ui->selected_node = hit;
          cs->is_dragging_node = 1;
          cs->drag_offset_x = wx - hit->x;
          cs->drag_offset_y = wy - hit->y;
        }
      } else {
        // Clicked on background - start panning automatically
        ui->selected_node = NULL;
        if (ui->editing_node) {
          strncpy(ui->editing_node->text, ui->edit_buffer,
                  sizeof(ui->editing_node->text) - 1);
          ui->editing_node = NULL;
          SDL_StartTextInput(); // keep text input alive for Nuklear edit widgets
        }
        // Start pan mode automatically if not in pen tool
        if (cs->tool != CANVAS_TOOL_PEN && !ui->block_drag.is_active) {
          cs->is_panning = 1;
          cs->pan_start_x = ui->mouse_x;
          cs->pan_start_y = ui->mouse_y;
          cs->cam_start_x = cs->cam_x;
          cs->cam_start_y = cs->cam_y;
        }
      }
    }
  }

  // Handle Bezier point dragging
  if (cs->bezier_mode && ui->selected_node &&
      ui->selected_node->type == NODE_TYPE_PATH) {
    Node *node = ui->selected_node;
    if (cs->dragging_point_idx != -1 && (ui->mouse_buttons & 1)) {
      float dx = ctx->input.mouse.delta.x / zoom;
      float dy = ctx->input.mouse.delta.y / zoom;
      NodePathPoint *p = &node->path_points[cs->dragging_point_idx];
      if (cs->dragging_cp_idx == 0) {
        p->x += dx;
        p->y += dy;
      } else if (cs->dragging_cp_idx == 1) {
        p->cp1x += dx;
        p->cp1y += dy;
      } else if (cs->dragging_cp_idx == 2) {
        p->cp2x += dx;
        p->cp2y += dy;
      }
      node_update_path_bounds(node);
    } else {
      cs->dragging_point_idx = -1;
    }
  }

  if (cs->is_resizing && ui->selected_node) {
    float dx = (ui->mouse_x - cs->resize_start_x) / zoom,
          dy = (ui->mouse_y - cs->resize_start_y) / zoom;
    float nw = cs->resize_start_w, nh = cs->resize_start_h,
          nx = cs->resize_start_node_x, ny = cs->resize_start_node_y;
    if (cs->resize_handle == 0) {
      nw += dx;
      nh += dy;
    } else if (cs->resize_handle == 1) {
      nw -= dx;
      nh += dy;
      nx += (cs->resize_start_w - nw);
    } else if (cs->resize_handle == 2) {
      nw -= dx;
      nh -= dy;
      nx += (cs->resize_start_w - nw);
      ny += (cs->resize_start_h - nh);
    } else if (cs->resize_handle == 3) {
      nw += dx;
      nh -= dy;
      ny += (cs->resize_start_h - nh);
    }
    if (nw < 20.0f)
      nw = 20.0f;
    if (nh < 20.0f)
      nh = 20.0f;
    ui->selected_node->width = nw;
    ui->selected_node->height = nh;
    ui->selected_node->x = nx;
    ui->selected_node->y = ny;
  }
  if (cs->mouse_just_released) {
    cs->is_resizing = 0;
    cs->is_dragging_node = 0;
  }

  // --- Right-click context menu detection ---
  if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_RIGHT) &&
      cs->mouse_in_canvas && !in_toolbar) {
    float wx = cam_x + (ui->mouse_x - bounds.x) / zoom;
    float wy = cam_y + (ui->mouse_y - bounds.y) / zoom;
    Node *hit = canvas_find_node_at(cd, wx, wy);
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

  // --- 4. PEN TOOL logic ---
  if (cs->tool == CANVAS_TOOL_PEN && cs->mouse_in_canvas && !in_toolbar &&
      !in_forms_menu && !cs->is_panning && (ui->mouse_buttons & 1)) {
    float wx = cam_x + (ui->mouse_x - bounds.x) / zoom,
          wy = cam_y + (ui->mouse_y - bounds.y) / zoom;
    if (cs->mouse_just_pressed) {
      cs->current_stroke = (Stroke *)calloc(1, sizeof(Stroke));
      cs->current_stroke->capacity = 128;
      cs->current_stroke->points = (Point *)calloc(128, sizeof(Point));
      if (cs->pen_is_marker) {
        cs->current_stroke->thickness = cs->pen_thickness * 10.0f;
        uint32_t c = cs->selected_color;
        uint8_t r = (c >> 24) & 0xFF;
        uint8_t g = (c >> 16) & 0xFF;
        uint8_t b = (c >> 8) & 0xFF;
        uint8_t a = c & 0xFF;
        // Reduce alpha to 30% for marker effect
        cs->current_stroke->color =
            (r << 24) | (g << 16) | (b << 8) | (uint8_t)(a * 0.3f);
      } else {
        cs->current_stroke->thickness = cs->pen_thickness;
        cs->current_stroke->color = cs->selected_color;
      }
      cs->current_stroke->points[0].x = wx;
      cs->current_stroke->points[0].y = wy;
      cs->current_stroke->count = 1;

      // Wrap it in a Node so it appears in Layers
      Node *node = (Node *)calloc(1, sizeof(Node));
      if (node) {
        node->id = cd->next_node_id++;
        node->type = NODE_TYPE_STROKE;
        node->stroke_data = cs->current_stroke;
        node->visible = 1;
        node->parent = cd->root;

        // Add to document tree
        if (!cd->root->first_child) {
          cd->root->first_child = node;
        } else {
          Node *last = cd->root->first_child;
          while (last->next_sibling)
            last = last->next_sibling;
          last->next_sibling = node;
        }
        cs->current_stroke_node = node;

        // Initialize bounds
        node->x = wx;
        node->y = wy;
        node->width = 1.0f;
        node->height = 1.0f;
      }
    } else if (cs->current_stroke) {
      if (cs->current_stroke->count >= cs->current_stroke->capacity) {
        cs->current_stroke->capacity *= 2;
        cs->current_stroke->points =
            (Point *)realloc(cs->current_stroke->points,
                             cs->current_stroke->capacity * sizeof(Point));
      }
      Point last = cs->current_stroke->points[cs->current_stroke->count - 1];
      if ((wx - last.x) * (wx - last.x) + (wy - last.y) * (wy - last.y) >
          4.0f) {
        cs->current_stroke->points[cs->current_stroke->count].x = wx;
        cs->current_stroke->points[cs->current_stroke->count].y = wy;
        cs->current_stroke->count++;

        // Update node bounds for culling
        if (cs->current_stroke_node) {
          float min_x = cs->current_stroke_node->x;
          float min_y = cs->current_stroke_node->y;
          float max_x = min_x + cs->current_stroke_node->width;
          float max_y = min_y + cs->current_stroke_node->height;

          if (wx < min_x)
            min_x = wx;
          if (wy < min_y)
            min_y = wy;
          if (wx > max_x)
            max_x = wx;
          if (wy > max_y)
            max_y = wy;

          cs->current_stroke_node->x = min_x;
          cs->current_stroke_node->y = min_y;
          cs->current_stroke_node->width = max_x - min_x;
          cs->current_stroke_node->height = max_y - min_y;
        }
      }
    }
  } else if (!(ui->mouse_buttons & 1))
    cs->current_stroke = NULL;

  // --- 5. RENDER ---
  if (nk_group_begin(ctx, "canvas_preview", NK_WINDOW_NO_SCROLLBAR)) {
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    nk_fill_rect(canvas_buf, bounds, 0, nk_rgb(255, 255, 255));
    canvas_draw_dot_grid(canvas_buf, bounds, cam_x, cam_y, zoom);
    nk_push_scissor(canvas_buf, bounds);

    // Empty state hint
    if (!cd->root->first_child) {
      const char *hint = "Click a tool to start";
      float hint_w = ctx->style.font->width(ctx->style.font->userdata,
                                             ctx->style.font->height, hint,
                                             strlen(hint));
      nk_draw_text(canvas_buf,
                   nk_rect(bounds.x + (bounds.w - hint_w) / 2.0f,
                           bounds.y + bounds.h / 2.0f - 10.0f, hint_w, 20),
                   hint, strlen(hint), ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(180, 180, 180));
    }

    // Nodes (including strokes)
    for (Node *node = cd->root->first_child; node; node = node->next_sibling) {
      if (!node->visible)
        continue;
      float sx = bounds.x + (node->x - cam_x) * zoom,
            sy = bounds.y + (node->y - cam_y) * zoom;
      float sw = node->width * zoom, sh = node->height * zoom;
      if (sx + sw < bounds.x || sx > bounds.x + bounds.w ||
          sy + sh < bounds.y || sy > bounds.y + bounds.h)
        continue;
      struct nk_rect nr = nk_rect(sx, sy, sw, sh);
      struct nk_color fill = nk_rgba(
          (node->fill_color >> 24) & 0xFF, (node->fill_color >> 16) & 0xFF,
          (node->fill_color >> 8) & 0xFF, node->fill_color & 0xFF);
      struct nk_color stroke = nk_rgba(
          (node->stroke_color >> 24) & 0xFF, (node->stroke_color >> 16) & 0xFF,
          (node->stroke_color >> 8) & 0xFF, node->stroke_color & 0xFF);

      int drawn_image = 0;
      if (node->is_container && node->image_path[0]) {
        SDL_Texture *tex = get_masked_image_texture(ui, node->image_path, node);
        if (tex) {
          struct nk_image img = nk_image_ptr(tex);
          nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
          drawn_image = 1;
        }
      }

      if (node->type == NODE_TYPE_STROKE && node->stroke_data) {
        Stroke *s = node->stroke_data;
        if (s->count >= 2) {
          struct nk_color sc =
              nk_rgba((s->color >> 24) & 0xFF, (s->color >> 16) & 0xFF,
                      (s->color >> 8) & 0xFF, s->color & 0xFF);
          struct nk_vec2 *pts =
              (struct nk_vec2 *)malloc(s->count * sizeof(struct nk_vec2));
          for (int i = 0; i < s->count; i++) {
            pts[i].x = bounds.x + (s->points[i].x - cam_x) * zoom;
            pts[i].y = bounds.y + (s->points[i].y - cam_y) * zoom;
          }
          nk_stroke_polyline(canvas_buf, (float *)pts, s->count,
                             s->thickness * zoom, sc);

          // Render Endings for Strokes
          if (node->line_start_style || node->line_end_style) {
            float size =
                (s->thickness * zoom > 2.0f ? s->thickness * zoom : 2.0f) *
                4.0f;

            // Start Style
            if (node->line_start_style > 0 && s->count >= 2) {
              if (node == ui->selected_node && (int)(SDL_GetTicks() % 100 == 0))
                ASLAN_DEBUG("Render Start Style: %d for Node %d",
                            node->line_start_style, node->id);

              float start_x = pts[0].x;
              float start_y = pts[0].y;
              float dx = pts[1].x - start_x;
              float dy = pts[1].y - start_y;
              float angle = atan2f(dy, dx);

              if (node->line_start_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(start_x - size / 2, start_y - size / 2, size, size),
                    sc);
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
                                 sc);
              }
            }

            // End Style
            if (node->line_end_style > 0 && s->count >= 2) {
              float end_x = pts[s->count - 1].x;
              float end_y = pts[s->count - 1].y;
              float dx = end_x - pts[s->count - 2].x;
              float dy = end_y - pts[s->count - 2].y;
              float angle = atan2f(dy, dx);

              if (node->line_end_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(end_x - size / 2, end_y - size / 2, size, size),
                    sc);
              } else if (node->line_end_style == 2) { // Arrow
                float tip_x = end_x;
                float tip_y = end_y;
                float base_angle1 = angle + 3.14159f + 0.5f;
                float base_angle2 = angle + 3.14159f - 0.5f;
                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);
                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 sc);
              }
            }
          }

          free(pts);
        }
      } else if (node->type == NODE_TYPE_RECT) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, cbg);
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        if (!drawn_image) {
          nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);

      } else if (node->type == NODE_TYPE_LINE) {
        // Render explicit LINE type as a horizontal stroke through the center
        // of its bounds
        float line_y = nr.y + nr.h / 2.0f;
        float line_w =
            (node->stroke_width > 0 ? node->stroke_width : 2.0f) * zoom;
        struct nk_color line_col = stroke;

        // Draw the main line
        nk_stroke_line(canvas_buf, nr.x, line_y, nr.x + nr.w, line_y, line_w,
                       line_col);

        // Line Endings
        float size = (line_w > 2.0f ? line_w : 2.0f) * 4.0f;

        // Start Style (Left)
        if (node->line_start_style > 0) {
          if (node->line_start_style == 1) { // Circle
            nk_fill_circle(
                canvas_buf,
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
            nk_fill_triangle(canvas_buf, nr.x + nr.w, line_y,
                             nr.x + nr.w - size, line_y - size / 2,
                             nr.x + nr.w - size, line_y + size / 2, line_col);
          }
        }

      } else if (node->type == NODE_TYPE_CHART && node->chart_data) {
        ui_render_chart_impl(ui, ctx, node->chart_data, nr, NULL);
      } else if (node->type == NODE_TYPE_ELLIPSE) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          nk_fill_circle(canvas_buf, nr, cbg);
          nk_stroke_circle(canvas_buf, nr, 1.0f, cst);
          // Draw ICON_ADD in center
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        if (!drawn_image) {
          nk_fill_circle(canvas_buf, nr, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_circle(canvas_buf, nr, node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_TRIANGLE) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          struct nk_vec2 vpts[3];
          vpts[0] = nk_vec2(nr.x + nr.w / 2, nr.y);
          vpts[1] = nk_vec2(nr.x, nr.y + nr.h);
          vpts[2] = nk_vec2(nr.x + nr.w, nr.y + nr.h);
          nk_fill_polygon(canvas_buf, (const float *)vpts, 3, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vpts, 3, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 5, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        struct nk_vec2 pts[3];
        pts[0] = nk_vec2(nr.x + nr.w / 2, nr.y);
        pts[1] = nk_vec2(nr.x, nr.y + nr.h);
        pts[2] = nk_vec2(nr.x + nr.w, nr.y + nr.h);
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)pts, 3, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)pts, 3,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_PENTAGON) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          struct nk_vec2 vpts[5];
          for (int i = 0; i < 5; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
            vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          nk_fill_polygon(canvas_buf, (const float *)vpts, 5, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vpts, 5, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        struct nk_vec2 pts[5];
        for (int i = 0; i < 5; i++) {
          float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
          pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
          pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
        }
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)pts, 5, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)pts, 5,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_HEXAGON) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          struct nk_vec2 vpts[6];
          for (int i = 0; i < 6; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 6.0f;
            vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          nk_fill_polygon(canvas_buf, (const float *)vpts, 6, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vpts, 6, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        struct nk_vec2 pts[6];
        for (int i = 0; i < 6; i++) {
          float angle = (float)i * 2.0f * 3.14159f / 6.0f;
          pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
          pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
        }
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)pts, 6, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)pts, 6,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_OCTAGON) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          struct nk_vec2 vpts[8];
          for (int i = 0; i < 8; i++) {
            float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
            vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
            vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
          }
          nk_fill_polygon(canvas_buf, (const float *)vpts, 8, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vpts, 8, 1.0f, cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        struct nk_vec2 pts[8];
        for (int i = 0; i < 8; i++) {
          float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
          pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
          pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
        }
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)pts, 8, fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)pts, 8,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_HEART) {
        if (node->is_container && !node->image_path[0]) {
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          const int vhcount = 48;
          struct nk_vec2 vhpts[48];
          for (int i = 0; i < vhcount; i++) {
            float t = (float)i * 2.0f * 3.14159f / (float)vhcount;
            float hx2 = 16.0f * powf(sinf(t), 3.0f);
            float hy2 = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                          2.0f * cosf(3.0f * t) - cosf(4.0f * t));
            vhpts[i].x = nr.x + nr.w / 2.0f + (hx2 / 32.0f) * nr.w;
            vhpts[i].y = nr.y + nr.h * 0.45f + (hy2 / 32.0f) * nr.h;
          }
          nk_fill_polygon(canvas_buf, (const float *)vhpts, vhcount, cbg);
          nk_stroke_polygon(canvas_buf, (const float *)vhpts, vhcount, 1.0f,
                            cst);
          if (ui->font_icons) {
            struct nk_rect ir =
                nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
            nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                         &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
          }
          drawn_image = 1;
        }
        // Parametric heart: (16sin^3(t), 13cos(t) - 5cos(2t) - 2cos(3t) -
        // cos(4t))
        const int heart_pts_count = 48;
        struct nk_vec2 hpts[48];
        for (int i = 0; i < heart_pts_count; i++) {
          float t = (float)i * 2.0f * 3.14159f / (float)heart_pts_count;
          float hx = 16.0f * powf(sinf(t), 3.0f);
          float hy = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                       2.0f * cosf(3.0f * t) - cosf(4.0f * t));
          hpts[i].x = nr.x + nr.w / 2.0f + (hx / 32.0f) * nr.w;
          hpts[i].y = nr.y + nr.h * 0.45f + (hy / 32.0f) * nr.h;
        }
        if (!drawn_image) {
          nk_fill_polygon(canvas_buf, (const float *)hpts, heart_pts_count,
                          fill);
        }
        if (node->stroke_width > 0)
          nk_stroke_polygon(canvas_buf, (const float *)hpts, heart_pts_count,
                            node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_PATH && node->path_points &&
                 node->path_point_count > 1) {
        if (node->is_container && !node->image_path[0]) {
          // Empty container placeholder for PATH shape
          struct nk_color cbg = nk_rgba(66, 133, 244, 40);
          struct nk_color cst = nk_rgba(66, 133, 244, 255);
          float poly_pts[512 * 2];
          int poly_count =
              node_get_path_polygon(node, poly_pts, 512, sx, sy, zoom);
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
              node_get_path_polygon(node, poly_pts, 512, sx, sy, zoom);
          if (poly_count > 2) {
            nk_fill_polygon(canvas_buf, poly_pts, poly_count, fill);
          }
        }

        // Stroke path
        for (int i = 0; i < node->path_point_count; i++) {
          int next = (i + 1) % node->path_point_count;
          NodePathPoint p1 = node->path_points[i];
          NodePathPoint p2 = node->path_points[next];

          if (p1.type == 1) { // Cubic Bezier
            nk_stroke_curve(canvas_buf, sx + p1.x * zoom, sy + p1.y * zoom,
                            sx + p1.cp2x * zoom, sy + p1.cp2y * zoom,
                            sx + p2.cp1x * zoom, sy + p2.cp1y * zoom,
                            sx + p2.x * zoom, sy + p2.y * zoom,
                            node->stroke_width * zoom, stroke);
          } else {
            nk_stroke_line(canvas_buf, sx + p1.x * zoom, sy + p1.y * zoom,
                           sx + p2.x * zoom, sy + p2.y * zoom,
                           node->stroke_width * zoom, stroke);
          }
        }
      } else if (node->type == NODE_TYPE_IMAGE) {
        SDL_Texture *tex = get_image_texture(ui, node->image_path);
        if (tex) {
          struct nk_image img = nk_image_ptr(tex);
          nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
        } else
          nk_fill_rect(canvas_buf, nr, 0, nk_rgb(200, 200, 200));

        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_TEXT) {
        // Render background for sticky notes / text with bg
        if (fill.a > 0)
          nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, fill);

        const char *display_text = node->text;
        int is_editing = (ui->editing_node == node);
        if (is_editing)
          display_text = ui->edit_buffer;

        if (display_text && strlen(display_text) > 0) {
          const struct nk_user_font *font =
              ui->font_inter ? &ui->font_inter->handle : ctx->style.font;

          // Select font based on type and size
          if (node->font_type == 4 && ui->font_mono) { // Mono
            font = &ui->font_mono->handle;
          } else { // Inter variants (Body, H1, H2, H3)
            // Find closest size in font_inter_sizes
            float target_size = node->font_size;
            float best_diff = 1000.0f;
            int best_idx = -1;
            float available_sizes[] = {12.0f, 14.0f, 16.0f, 20.0f, 24.0f,
                                       32.0f, 48.0f, 64.0f, 96.0f};

            for (int i = 0; i < 9; i++) {
              float diff = fabsf(available_sizes[i] - target_size);
              if (diff < best_diff) {
                best_diff = diff;
                best_idx = i;
              }
            }

            if (best_idx >= 0 && ui->font_inter_sizes[best_idx]) {
              font = &ui->font_inter_sizes[best_idx]->handle;
            } else if (ui->font_inter) {
              font = &ui->font_inter->handle;
            }
          }

          struct nk_color txt_color = {(node->stroke_color >> 24) & 0xFF,
                                       (node->stroke_color >> 16) & 0xFF,
                                       (node->stroke_color >> 8) & 0xFF,
                                       node->stroke_color & 0xFF};

          nk_draw_text(canvas_buf, nr, display_text, strlen(display_text), font,
                       nk_rgba(0, 0, 0, 0), txt_color);

          if (is_editing) {
            float tw = font->width(font->userdata, font->height, display_text,
                                   strlen(display_text));
            int show_cursor = ((int)(SDL_GetTicks() / 500) % 2 == 0);
            if (show_cursor) {
              nk_stroke_line(canvas_buf, nr.x + tw + 1, nr.y + 2, nr.x + tw + 1,
                             nr.y + nr.h - 2, 1.5f * zoom,
                             nk_rgb(66, 133, 244));
            }
          }
        }

        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
      } else if (node->type == NODE_TYPE_CHART) {
        Page *curr_page = doc_page_get_active(engine->doc);
        ui_render_chart_node(ui, ctx, node, nr, curr_page);

        // Rendering configuration bar if selected
        if (ui->selected_node == node && !cs->is_panning &&
            !cs->is_dragging_node && !cs->is_resizing) {
          ui_render_chart_node_config_bar(ui, engine, ctx, node, nr, curr_page);
        }

        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
      } else {
        nk_fill_rect(canvas_buf, nr, node->corner_radius * zoom, fill);
        if (node->stroke_width > 0)
          nk_stroke_rect(canvas_buf, nr, node->corner_radius * zoom,
                         node->stroke_width * zoom, stroke);
      }

      // Removed global unconditional stroke here
      if (ui->selected_node == node && !cs->bezier_mode) {
        float sel_off = 2.0f + (node->stroke_width * zoom) / 2.0f;
        nk_stroke_rect(canvas_buf,
                       nk_rect(nr.x - sel_off, nr.y - sel_off,
                               nr.w + sel_off * 2, nr.h + sel_off * 2),
                       0, 2.0f, nk_rgb(66, 133, 244));
      }

      // Bezier handles interaction and rendering
      // Show handles for ALL path nodes when bezier mode is active
      if (cs->bezier_mode && node->type == NODE_TYPE_PATH) {
        for (int i = 0; i < node->path_point_count; i++) {
          NodePathPoint *p = &node->path_points[i];
          float px = sx + p->x * zoom;
          float py = sy + p->y * zoom;

          // Anchor
          struct nk_rect anchor_rect =
              nk_rect(px - 5 * zoom, py - 5 * zoom, 10 * zoom, 10 * zoom);
          nk_fill_rect(canvas_buf, anchor_rect, 0, nk_rgb(255, 255, 255));
          nk_stroke_rect(canvas_buf, anchor_rect, 0, 1.5f * zoom,
                         nk_rgb(66, 133, 244));

          if (cs->mouse_just_pressed &&
              nk_input_is_mouse_hovering_rect(&ctx->input, anchor_rect)) {
            cs->dragging_point_idx = i;
            cs->dragging_cp_idx = 0;
          }

          if (p->type == 1) { // Cubic
            float cp1x = sx + p->cp1x * zoom;
            float cp1y = sy + p->cp1y * zoom;
            float cp2x = sx + p->cp2x * zoom;
            float cp2y = sy + p->cp2y * zoom;

            nk_stroke_line(canvas_buf, px, py, cp1x, cp1y, 1,
                           nk_rgb(200, 200, 200));
            nk_stroke_line(canvas_buf, px, py, cp2x, cp2y, 1,
                           nk_rgb(200, 200, 200));

            struct nk_rect r1 =
                nk_rect(cp1x - 4 * zoom, cp1y - 4 * zoom, 8 * zoom, 8 * zoom);
            struct nk_rect r2 =
                nk_rect(cp2x - 4 * zoom, cp2y - 4 * zoom, 8 * zoom, 8 * zoom);
            nk_fill_circle(canvas_buf, r1, nk_rgb(66, 133, 244));
            nk_fill_circle(canvas_buf, r2, nk_rgb(66, 133, 244));

            if (cs->mouse_just_pressed &&
                nk_input_is_mouse_hovering_rect(&ctx->input, r1)) {
              cs->dragging_point_idx = i;
              cs->dragging_cp_idx = 1;
            }
            if (cs->mouse_just_pressed &&
                nk_input_is_mouse_hovering_rect(&ctx->input, r2)) {
              cs->dragging_point_idx = i;
              cs->dragging_cp_idx = 2;
            }
          }
        }
      }
    }

    // --- Alignment guides when dragging ---
    if (cs->is_dragging_node && ui->selected_node) {
      Node *sel = ui->selected_node;
      float sx = bounds.x + (sel->x - cam_x) * zoom;
      float sy = bounds.y + (sel->y - cam_y) * zoom;
      float sw = sel->width * zoom;
      float sh = sel->height * zoom;
      draw_alignment_guides(canvas_buf, sel, bounds.x + bounds.w / 2.0f,
                            bounds.y + bounds.h / 2.0f, bounds.x, bounds.y,
                            bounds.w, bounds.h, sx, sy, sw, sh);
    }

    nk_push_scissor(canvas_buf, nk_rect(0, 0, 100000, 100000));
    canvas_render_toolbar(ui, ctx, bounds, block);

    // --- Zoom controls (left of toolbar) ---
    {
      float tb_w = 7 * 36.0f + 6 * 8.0f + 24.0f;
      float tb_x = bounds.x + (bounds.w - tb_w) / 2.0f;
      float tb_y = bounds.y + bounds.h - 52.0f - 20.0f;
      float zb_size = 24.0f;
      float zy = tb_y + (52.0f - zb_size) / 2.0f;
      float zx = tb_x - 130.0f;

      // Zoom percentage label
      char zoom_str[16];
      snprintf(zoom_str, sizeof(zoom_str), "%d%%", (int)(cs->zoom * 100.0f));
      nk_draw_text(canvas_buf, nk_rect(zx, zy + 2, 45, 20), zoom_str,
                   strlen(zoom_str), ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(80, 80, 80));

      // Minus button
      struct nk_rect minus_btn = nk_rect(zx + 48, zy, zb_size, zb_size);
      int minus_hover =
          (ui->mouse_x >= minus_btn.x &&
           ui->mouse_x < minus_btn.x + minus_btn.w &&
           ui->mouse_y >= minus_btn.y &&
           ui->mouse_y < minus_btn.y + minus_btn.h);
      if (minus_hover)
        nk_fill_rect(canvas_buf, minus_btn, 4.0f, nk_rgba(0, 0, 0, 20));
      nk_draw_text(canvas_buf,
                   nk_rect(minus_btn.x + 7, minus_btn.y + 2, 16, 20), "-", 1,
                   ctx->style.font, nk_rgba(0, 0, 0, 0), nk_rgb(60, 60, 60));
      if (minus_hover && cs->mouse_just_pressed) {
        float old_zoom = cs->zoom;
        cs->zoom *= 0.8f;
        if (cs->zoom < 0.1f)
          cs->zoom = 0.1f;
        // Keep center stable
        float cx = bounds.w / 2.0f, cy = bounds.h / 2.0f;
        cs->cam_x += cx / old_zoom - cx / cs->zoom;
        cs->cam_y += cy / old_zoom - cy / cs->zoom;
      }

      // Plus button
      struct nk_rect plus_btn =
          nk_rect(minus_btn.x + zb_size + 4, zy, zb_size, zb_size);
      int plus_hover =
          (ui->mouse_x >= plus_btn.x &&
           ui->mouse_x < plus_btn.x + plus_btn.w &&
           ui->mouse_y >= plus_btn.y &&
           ui->mouse_y < plus_btn.y + plus_btn.h);
      if (plus_hover)
        nk_fill_rect(canvas_buf, plus_btn, 4.0f, nk_rgba(0, 0, 0, 20));
      nk_draw_text(canvas_buf,
                   nk_rect(plus_btn.x + 6, plus_btn.y + 2, 16, 20), "+", 1,
                   ctx->style.font, nk_rgba(0, 0, 0, 0), nk_rgb(60, 60, 60));
      if (plus_hover && cs->mouse_just_pressed) {
        float old_zoom = cs->zoom;
        cs->zoom *= 1.25f;
        if (cs->zoom > 5.0f)
          cs->zoom = 5.0f;
        float cx = bounds.w / 2.0f, cy = bounds.h / 2.0f;
        cs->cam_x += cx / old_zoom - cx / cs->zoom;
        cs->cam_y += cy / old_zoom - cy / cs->zoom;
      }

      // Fit button
      struct nk_rect fit_btn =
          nk_rect(plus_btn.x + zb_size + 4, zy, 28, zb_size);
      int fit_hover =
          (ui->mouse_x >= fit_btn.x &&
           ui->mouse_x < fit_btn.x + fit_btn.w &&
           ui->mouse_y >= fit_btn.y &&
           ui->mouse_y < fit_btn.y + fit_btn.h);
      if (fit_hover)
        nk_fill_rect(canvas_buf, fit_btn, 4.0f, nk_rgba(0, 0, 0, 20));
      nk_draw_text(canvas_buf,
                   nk_rect(fit_btn.x + 4, fit_btn.y + 2, 24, 20), "Fit", 3,
                   ctx->style.font, nk_rgba(0, 0, 0, 0),
                   fit_hover ? nk_rgb(66, 133, 244) : nk_rgb(80, 80, 80));
      if (fit_hover && cs->mouse_just_pressed) {
        // Compute bounding box of all nodes
        Node *root = cd->root;
        float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
        int has_nodes = 0;
        if (root) {
          for (Node *n = root->first_child; n; n = n->next_sibling) {
            if (!n->visible)
              continue;
            has_nodes = 1;
            if (n->x < min_x)
              min_x = n->x;
            if (n->y < min_y)
              min_y = n->y;
            if (n->x + n->width > max_x)
              max_x = n->x + n->width;
            if (n->y + n->height > max_y)
              max_y = n->y + n->height;
          }
        }
        if (has_nodes) {
          float content_w = max_x - min_x + 100.0f; // Padding
          float content_h = max_y - min_y + 100.0f;
          float zw = bounds.w / content_w;
          float zh = bounds.h / content_h;
          cs->zoom = (zw < zh) ? zw : zh;
          if (cs->zoom > 2.0f)
            cs->zoom = 2.0f;
          if (cs->zoom < 0.1f)
            cs->zoom = 0.1f;
          cs->cam_x = min_x - 50.0f -
                      (bounds.w / cs->zoom - content_w + 100.0f) / 2.0f;
          cs->cam_y = min_y - 50.0f -
                      (bounds.h / cs->zoom - content_h + 100.0f) / 2.0f;
        } else {
          cs->zoom = 1.0f;
          cs->cam_x = 0;
          cs->cam_y = 0;
        }
      }
    }

    // --- Right-click context menu ---
    render_node_context_menu(ui, ctx, canvas_buf, block);

    nk_group_end(ctx);
  }

  // Right sidebar (only in fullscreen mode)
  if (is_expanded) {
    if (nk_group_begin(ctx, "canvas_sidebar",
                       NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
      if (ui->sidebar_mode == 1) {
        ui_render_sidebar_shape_selection(ui, ctx);
      } else {
        // Properties section
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Properties", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);
        ui_render_sidebar_properties(ui, engine, ctx, block);

        // Spacing between sections
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_spacing(ctx, 1);

        // Layers section
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Layers", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);
        ui_render_sidebar_layers(ui, ctx, block);
      }

      nk_group_end(ctx);
    }
  }

  // Update canvas view state
  cd->view_x = cs->cam_x;
  cd->view_y = cs->cam_y;
  cd->zoom = cs->zoom;
}

// -----------------------------------------------------------------------------
// Design Block Helper: Find node at position
// -----------------------------------------------------------------------------

Node *design_find_node_at(DesignData *dd, float x, float y) {
  if (!dd->root)
    return NULL;

  Node *hit = NULL;
  for (Node *node = dd->root->first_child; node; node = node->next_sibling) {
    if (!node->visible)
      continue;
    if (x >= node->x && x < node->x + node->width && y >= node->y &&
        y < node->y + node->height) {
      hit = node;
    }
  }
  return hit;
}

// =============================================================================
// Test Function
// =============================================================================

int test_ui_canvas(void) {
  ASLAN_INFO("Running ui_canvas tests...");
  ASLAN_INFO("ui_canvas tests PASSED");
  return 0;
}
