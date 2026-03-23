#ifndef UI_PROPERTIES_H
#define UI_PROPERTIES_H

#include "ui_host.h"
#include "../document/document.h"

void ui_render_sidebar_layers(UIHost *ui, Block *block, float x, float y,
                              float w, float h);

void design_insert_tableau_layout(DesignData *dd);

void design_render_sidebar(UIHost *ui, Engine *engine,
                           Block *block, float width, float height, float abs_x);

#endif // UI_PROPERTIES_H
