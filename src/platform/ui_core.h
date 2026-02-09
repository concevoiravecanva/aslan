#ifndef UI_CORE_H
#define UI_CORE_H

#include "ui_host.h"

// Renders the Document toolbar (Floating Bottom Bar)
void ui_render_document_toolbar(UIHost *ui, Engine *engine,
                                struct nk_context *ctx, int win_w, int win_h);

// Renders the bottom bar (Page Tabs)
void ui_render_bottom_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                          int win_w, int win_h);

// Renders the Home Screen (Dashboard)
void ui_render_home(UIHost *ui, Engine *engine, struct nk_context *ctx,
                    int win_w, int win_h);

// Applies the current theme (Dark/Light) to Nuklear style
void ui_apply_theme(UIHost *ui);

// ICON_CHART defined in ui_host.h

#endif // UI_CORE_H
