#ifndef UI_CORE_H
#define UI_CORE_H

#include "ui_host.h"

// Renders the top bar (Block Insertion Menu, etc.)
void ui_render_top_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                       int win_w);

// Renders the bottom bar (Page Tabs)
void ui_render_bottom_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                          int win_w, int win_h);

// Renders the Home Screen (Dashboard)
void ui_render_home(UIHost *ui, Engine *engine, struct nk_context *ctx,
                    int win_w, int win_h);

// Applies the current theme (Dark/Light) to Nuklear style
void ui_apply_theme(UIHost *ui);

#endif // UI_CORE_H
