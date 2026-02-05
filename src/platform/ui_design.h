#ifndef UI_DESIGN_H
#define UI_DESIGN_H

#include "ui_host.h"

// Renders a Design Block (Freeform Canvas)
void ui_render_block_design(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width);

#endif // UI_DESIGN_H
