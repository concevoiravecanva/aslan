// =============================================================================
// ASLAN UI CANVAS - Infinite Canvas Block Rendering and Tools
// =============================================================================

#ifndef UI_CANVAS_H
#define UI_CANVAS_H

#include "ui_host.h"
#include "ui_shared.h"
#include <SDL.h>

// -----------------------------------------------------------------------------
// Canvas Rendering - Main
// -----------------------------------------------------------------------------

void ui_render_block_canvas(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width);

// -----------------------------------------------------------------------------
// Canvas Drawing Helpers
// -----------------------------------------------------------------------------

void canvas_draw_dot_grid(struct nk_command_buffer *canvas,
                          struct nk_rect bounds, float cam_x, float cam_y,
                          float zoom);

int canvas_point_in_toolbar(struct nk_rect bounds, float mx, float my,
                            CanvasInteraction *cs);

void canvas_render_sub_menu(UIHost *ui, struct nk_context *ctx,
                            struct nk_command_buffer *canvas_buf, float x,
                            float y, CanvasCategory cat);

void canvas_render_toolbar(UIHost *ui, struct nk_context *ctx,
                           struct nk_rect bounds, Block *block);

// -----------------------------------------------------------------------------
// Canvas Sidebar Rendering
// -----------------------------------------------------------------------------

void ui_render_sidebar_properties(UIHost *ui, Engine *engine,
                                  struct nk_context *ctx, Block *block);

void ui_render_sidebar_layers(UIHost *ui, struct nk_context *ctx, Block *block);

void ui_render_sidebar_shape_selection(UIHost *ui, struct nk_context *ctx);

// -----------------------------------------------------------------------------
// Color Picker Helpers
// -----------------------------------------------------------------------------

struct nk_colorf ui_render_color_picker_content(struct nk_context *ctx,
                                                struct nk_colorf colorf);

void ui_render_color_property(struct nk_context *ctx, const char *label,
                              uint32_t *color_ptr);

// -----------------------------------------------------------------------------
// Shared Creation Helpers
// -----------------------------------------------------------------------------

Node *design_create_styled_node(DesignData *dd, NodeStyleType style, float x,
                                float y, float w, float h, uint32_t color);

Node *canvas_create_styled_node(CanvasData *cd, NodeStyleType style, float x,
                                float y, float w, float h, uint32_t color);

// -----------------------------------------------------------------------------
// Node Utilities
// -----------------------------------------------------------------------------

void node_update_path_bounds(Node *node);

int node_get_path_polygon(Node *node, float *out_pts, int max_pts, float sx,
                          float sy, float scale);

SDL_Texture *get_image_texture(UIHost *ui, const char *path);

Node *design_find_node_at(DesignData *dd, float world_x, float world_y);

Node *canvas_find_node_at(CanvasData *cd, float world_x, float world_y);

void recursive_convert_to_path(Node *node);

int is_form_tool(CanvasToolMode tool);

// -----------------------------------------------------------------------------
// Test Function
// -----------------------------------------------------------------------------

int test_ui_canvas(void);

#endif // UI_CANVAS_H
