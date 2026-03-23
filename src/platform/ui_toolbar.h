#ifndef UI_TOOLBAR_H
#define UI_TOOLBAR_H

#include "ui_host.h"
#include "ui_shared.h"

int canvas_point_in_toolbar(float bx, float by, float bw, float bh,
                            float mx, float my, CanvasInteraction *cs);

void canvas_render_sub_menu(UIHost *ui, float x, float y, CanvasCategory cat);

void canvas_render_toolbar(UIHost *ui, float bx, float by, float bw, float bh,
                           Block *block);

#endif // UI_TOOLBAR_H
