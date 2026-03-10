#include "../core/engine.h"
#include "../document/aslan_table.h"
#include "../document/document.h"
#include "ui_core.h"
#include "ui_host.h"
#include "ui_shared.h"

void ui_render_chart_impl(UIHost *ui, ChartData *cd, AslanRect bounds, Page *page) {
  (void)ui; (void)cd; (void)bounds; (void)page;
  // TODO: port chart rendering to NanoVG
}

void ui_render_chart_node(UIHost *ui, Node *node, AslanRect bounds, Page *page) {
  (void)ui; (void)node; (void)bounds; (void)page;
}

void ui_render_chart_node_config_bar(UIHost *ui, Engine *engine, Node *node, AslanRect bounds, Page *page) {
  (void)ui; (void)engine; (void)node; (void)bounds; (void)page;
}
