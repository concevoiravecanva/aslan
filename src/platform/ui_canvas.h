// =============================================================================
// ASLAN UI CANVAS - Shared Drawing Helpers (Canvas block removed)
// =============================================================================

#ifndef UI_CANVAS_H
#define UI_CANVAS_H

#include "ui_host.h"
#include "ui_shared.h"
#include <SDL.h>

// -----------------------------------------------------------------------------
// Canvas Drawing Helpers (shared by Design block)
// -----------------------------------------------------------------------------

void canvas_draw_dot_grid(UIHost *ui, float bx, float by, float bw, float bh,
                          float cam_x, float cam_y, float zoom);

int canvas_point_in_toolbar(float bx, float by, float bw, float bh,
                            float mx, float my, CanvasInteraction *cs);

void canvas_render_sub_menu(UIHost *ui, float x, float y, CanvasCategory cat);

void canvas_render_toolbar(UIHost *ui, float bx, float by, float bw, float bh,
                           Block *block);

// -----------------------------------------------------------------------------
// Canvas Sidebar Rendering (shared by Design block)
// -----------------------------------------------------------------------------

void ui_render_sidebar_layers(UIHost *ui, Block *block, float x, float y,
                              float w, float h);

// -----------------------------------------------------------------------------
// Shared Creation Helpers
// -----------------------------------------------------------------------------

Node *design_create_styled_node(DesignData *dd, NodeStyleType style, float x,
                                float y, float w, float h, uint32_t color);

// -----------------------------------------------------------------------------
// Node Utilities
// -----------------------------------------------------------------------------

void node_update_path_bounds(Node *node);

int node_get_path_polygon(Node *node, float *out_pts, int max_pts, float sx,
                          float sy, float scale);

int get_image_texture(UIHost *ui, const char *path);

Node *design_find_node_at(DesignData *dd, float world_x, float world_y);

void recursive_convert_to_path(Node *node);

// -----------------------------------------------------------------------------
// Test Function
// -----------------------------------------------------------------------------

#endif // UI_CANVAS_H
