#ifndef NODE_RENDER_H
#define NODE_RENDER_H

#include "../document/document.h"
#include "ui_host.h"
#include "ui_design.h"

void ui_render_node_tree(UIHost *ui, Engine *engine, Block *block,
                         DesignData *dd, Node *node, AslanRect bounds,
                         float scale, float parent_x, float parent_y);

#endif // NODE_RENDER_H
