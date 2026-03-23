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

#include "ui_toolbar.h"

void canvas_draw_dot_grid(UIHost *ui, float bx, float by, float bw, float bh,
                          float cam_x, float cam_y, float zoom);

// -----------------------------------------------------------------------------
// Canvas Sidebar Rendering (shared by Design block)
// -----------------------------------------------------------------------------

#include "ui_properties.h"

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
