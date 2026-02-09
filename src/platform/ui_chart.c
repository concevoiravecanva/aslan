#include "../document/aslan_table.h"
#include "../document/document.h"
#include "ui_core.h"
#include "ui_host.h"
#include <float.h>
#include <stdio.h>

// -----------------------------------------------------------------------------
// Chart Rendering Logic
// -----------------------------------------------------------------------------

// Shared rendering logic for both Blocks and Nodes
void ui_render_chart_impl(UIHost *ui, struct nk_context *ctx, ChartData *cd,
                          struct nk_rect bounds) {
  struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

  // Background and border
  nk_fill_rect(canvas, bounds, 5.0f, nk_rgba(255, 255, 255, 255));
  nk_stroke_rect(canvas, bounds, 5.0f, 1.0f, nk_rgba(200, 200, 200, 255));

  // Title / Placeholder if no source
  if (cd->source_table_id == 0) {
    // Center text in bounds
    struct nk_rect text_bounds = bounds;
    text_bounds.h = 20;
    text_bounds.y += (bounds.h - 20) / 2;
    nk_draw_text(canvas, text_bounds, "No source table linked", 22,
                 &ui->font_inter->handle, nk_rgba(0, 0, 0, 0),
                 nk_rgba(0, 0, 0, 255));
    return;
  }

  // Stub for now: Draw a dummy chart to verify plumbing
  float padding = 10.0f;
  float graph_w = bounds.w - padding * 2;
  float graph_h = bounds.h - padding * 2;
  float x = bounds.x + padding;
  float y = bounds.y + padding;

  nk_stroke_line(canvas, x, y + graph_h, x + graph_w, y + graph_h, 2.0f,
                 nk_rgba(100, 100, 100, 255)); // X Axis
  nk_stroke_line(canvas, x, y, x, y + graph_h, 2.0f,
                 nk_rgba(100, 100, 100, 255)); // Y Axis

  // Draw a simple example line
  nk_stroke_line(canvas, x, y + graph_h, x + graph_w, y, 2.0f,
                 nk_rgba(cd->color >> 24 & 0xFF, cd->color >> 16 & 0xFF,
                         cd->color >> 8 & 0xFF, cd->color & 0xFF));
}

void ui_render_chart_node(UIHost *ui, struct nk_context *ctx, Node *node,
                          struct nk_rect bounds) {
  if (!ui || !node || !ctx || node->type != NODE_TYPE_CHART ||
      !node->chart_data)
    return;

  ui_render_chart_impl(ui, ctx, node->chart_data, bounds);
}
