#ifndef UI_TOOLBAR_H
#define UI_TOOLBAR_H

#include "ui_host.h"
#include "ui_shared.h"

// Render topbar design tools (table layout, chart, image, text, shape)
void ui_render_design_topbar_tools(UIHost *ui, float right_x, float top_y, float top_h);

// Sub menu tailored for top bar placement
void canvas_render_sub_menu_dropdown(UIHost *ui, float x, float y, CanvasCategory cat);

// Check if mouse is hovering over the top bar dropdown tools
int topbar_point_in_tools(UIHost *ui, float right_x, float top_y, float top_h, float mx, float my);

#endif // UI_TOOLBAR_H
