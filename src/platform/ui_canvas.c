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
        ui->input.mouse_just_pressed) {
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
  if ((ui->mouse_buttons & 1) && ui->input.mouse_just_pressed) {
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

    // Stroke/path nodes have thin bounding boxes — add hit padding.
    // Text nodes can also be tiny — add padding to make them easier to grab.
    float pad = (node->type == NODE_TYPE_STROKE || node->type == NODE_TYPE_PATH)
                    ? 8.0f
                    : (node->type == NODE_TYPE_TEXT ? 12.0f : 0.0f);

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
