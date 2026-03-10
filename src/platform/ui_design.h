#ifndef UI_DESIGN_H
#define UI_DESIGN_H

#include "../document/document.h"
#include "ui_host.h"

// Renders a Design Block (Freeform Canvas)
void ui_render_block_design(UIHost *ui, Engine *engine, Block *block,
                            float width);

void design_render_properties(UIHost *ui, Engine *engine,
                              struct nk_rect bounds);
void design_render_layers(UIHost *ui, Engine *engine, struct nk_rect bounds);

void ui_render_node_tree(UIHost *ui, Engine *engine, Block *block,
                         DesignData *dd, Node *node, struct nk_rect bounds,
                         float scale, float parent_x, float parent_y);

#endif // UI_DESIGN_H
