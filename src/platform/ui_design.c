#include "ui_design.h"
#include "../document/document.h"
#include "ui_canvas.h"
#include "ui_shared.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ui_render_block_design(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
  (void)engine;
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;

  // Initialize pages array if needed (for backwards compatibility)
  if (!dd->pages || dd->page_count == 0) {
    dd->page_capacity = 4;
    dd->pages = (DesignPage *)calloc(dd->page_capacity, sizeof(DesignPage));
    dd->page_count = 1;
    dd->current_page = 0;
    dd->pages[0].root = (Node *)calloc(1, sizeof(Node));
    if (dd->pages[0].root) {
      dd->pages[0].root->type = NODE_TYPE_ROOT;
      dd->pages[0].root->visible = 1;
    }
    dd->pages[0].bg_color = dd->bg_color;
    dd->root = dd->pages[0].root;
  }

  // Ensure root points to current page
  if (dd->current_page >= 0 && dd->current_page < dd->page_count) {
    dd->root = dd->pages[dd->current_page].root;
  }

  ui_render_expand_toggle(ui, ctx, block);

  int is_fullscreen = (ui->expanded_block_id == block->id);

  // --- Layout height calculation ---
  float toolbar_h = 52.0f;
  float display_h = 0;

  if (!is_fullscreen) {
    // Inline mode: scale to fit width, but cap height
    float scale_w = width / (float)dd->width;
    display_h = (float)dd->height * scale_w;
    if (display_h > 400.0f)
      display_h = 400.0f;
  } else {
    // Fullscreen: use window height
    display_h = nk_window_get_height(ctx) - 100.0f;
  }

  // Define layout
  if (is_fullscreen) {
    nk_layout_row_template_begin(ctx, display_h + toolbar_h);
    nk_layout_row_template_push_variable(ctx, 75);   // Workspace
    nk_layout_row_template_push_static(ctx, 250.0f); // Sidebar
    nk_layout_row_template_end(ctx);
  } else {
    nk_layout_row_dynamic(ctx, display_h + toolbar_h, 1);
  }

  // --- Rendering Workspace ---
  // DEBUG: Visualize the intended area
  /*
  nk_stroke_rect(nk_window_get_canvas(ctx),
      nk_layout_widget_bounds(ctx),
      2.0f, 2.0f, nk_rgb(255, 0, 0));
  */
  // Print dimensions to debug visibility
  ASLAN_DEBUG("Design Block %u: width=%.1f height=%.1f display_h=%.1f",
              block->id, width, (float)dd->height, display_h);

  if (nk_group_begin(ctx, "design_content_group",
                     NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
    struct nk_rect ws_bounds = nk_window_get_content_region(ctx);
    struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);
    DesignInteraction *ds = &ui->design_state;

    // Pre-calculate toolbar flags for interaction logic
    float toolbar_w = 6 * 36.0f + 5 * 8.0f + 24.0f;
    float toolbar_x = ws_bounds.x + (ws_bounds.w - toolbar_w) / 2.0f;
    float toolbar_y = ws_bounds.y + ws_bounds.h - 52.0f - 20.0f; // toolbar_h=52

    int in_toolbar =
        (ui->mouse_x >= toolbar_x && ui->mouse_x < toolbar_x + toolbar_w &&
         ui->mouse_y >= toolbar_y && ui->mouse_y < toolbar_y + 52.0f);
    int in_forms_menu = ds->show_forms_menu;

    // Early check for active state
    int is_active_design = (ds->active_design_id == block->id);

    // Hardened Input Check: Sync Nuklear input state (only if inside workspace)
    if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
        ui->mouse_x >= ws_bounds.x && ui->mouse_x < ws_bounds.x + ws_bounds.w &&
        ui->mouse_y >= ws_bounds.y && ui->mouse_y < ws_bounds.y + ws_bounds.h) {
      ds->mouse_just_pressed = 1;
    }

    // 1. Draw Workspace background (Notion gray)
    nk_fill_rect(canvas_buf, ws_bounds, 0, nk_rgb(241, 241, 239));

    // 2. Calculate Artboard Scale & Centering (Stable dimensions)
    float margin = 20.0f;
    float avail_w = ws_bounds.w - margin * 2;
    float avail_h = display_h - margin * 2; // Fixed height, not visible bounds

    float scale_w = avail_w / (float)dd->width;
    float scale_h = avail_h / (float)dd->height;
    float scale = (scale_w < scale_h) ? scale_w : scale_h;
    if (scale > 1.0f)
      scale = 1.0f; // Don't upscale past 100%

    float art_w = (float)dd->width * scale;
    float art_h = (float)dd->height * scale;

    // Centering: Vertical centering should use display_h, but absolute screen
    // position
    float sx = ws_bounds.x + (ws_bounds.w - art_w) / 2.0f;
    float sy = ws_bounds.y + (display_h - art_h) / 2.0f;

    // 3. Draw Shadow
    struct nk_rect shadow_rect = nk_rect(sx + 2, sy + 2, art_w, art_h);
    nk_fill_rect(canvas_buf, shadow_rect, 2, nk_rgba(0, 0, 0, 40));

    // 4. Draw Artboard (Page background)
    struct nk_rect art_rect = nk_rect(sx, sy, art_w, art_h);
    nk_fill_rect(canvas_buf, art_rect, 0,
                 nk_rgba((dd->pages[dd->current_page].bg_color >> 24) & 0xFF,
                         (dd->pages[dd->current_page].bg_color >> 16) & 0xFF,
                         (dd->pages[dd->current_page].bg_color >> 8) & 0xFF,
                         dd->pages[dd->current_page].bg_color & 0xFF));
    nk_stroke_rect(canvas_buf, art_rect, 0, 1.0f, nk_rgb(200, 200, 200));

    // 5. Setup Coordinate System and Clipping (Inside space)
    nk_layout_space_begin(ctx, NK_STATIC, ws_bounds.h, 1);
    nk_layout_space_push(
        ctx, nk_rect(sx - ws_bounds.x, sy - ws_bounds.y, art_w, art_h));

    if (nk_group_begin(ctx, "design_content", NK_WINDOW_NO_SCROLLBAR)) {
      struct nk_rect bounds = nk_window_get_content_region(ctx);
      // Coordinate note: bounds.x/y are now absolute artboard screen pos

      // Sync bounds and scale for interactions
      ds->bounds_x = bounds.x;
      ds->bounds_y = bounds.y;
      ds->bounds_w = bounds.w;
      ds->bounds_h = bounds.h;
      ds->scale = scale;

      // Track if this design is active
      // Track if this design is active
      is_active_design = (ds->active_design_id == block->id);

      // Auto-activate if clicked inside WORKSPACE (including toolbar)
      if (!is_active_design &&
          nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
          ui->mouse_x >= ws_bounds.x &&
          ui->mouse_x < ws_bounds.x + ws_bounds.w &&
          ui->mouse_y >= ws_bounds.y &&
          ui->mouse_y < ws_bounds.y + ws_bounds.h) {
        ds->active_design_id = block->id;
        ds->active_block = block;
        ui->editing_block_id = block->id;
        is_active_design = 1;
      }

      // --- Handle pending element creation ---
      if (is_active_design && ds->pending_create > 0) {
        float nx = (float)dd->width / 2.0f - 50.0f;
        float ny = (float)dd->height / 2.0f - 40.0f;
        Node *new_node = NULL;
        float def_w = 100.0f, def_h = 80.0f;

        if (ds->pending_create == 1) { // Body
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 200.0f, 40.0f, ds->selected_color);
          if (new_node)
            strncpy(new_node->text, "Body text", sizeof(new_node->text) - 1);
        } else if (ds->pending_create == 6) { // H1
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 300.0f, 60.0f, ds->selected_color);
          if (new_node)
            strncpy(new_node->text, "Heading", sizeof(new_node->text) - 1);
        } else if (ds->pending_create == 2) { // Sticky
          new_node = design_create_styled_node(
              dd, NODE_STYLE_RECT, nx, ny, 120.0f, 120.0f, ds->selected_color);
          if (new_node) {
            new_node->fill_color = 0xFFF9C4FF;
            new_node->stroke_color = 0xFBC02DFF;
            new_node->type = NODE_TYPE_TEXT;
            strncpy(new_node->text, "Note...", sizeof(new_node->text) - 1);
          }
        } else if (ds->pending_create == 3) { // Image
          char path[512] = {0};
          if (open_image_file_dialog(path, sizeof(path))) {
            char rel[256] = {0};
            if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
              new_node = node_create(dd->next_node_id++, NODE_TYPE_IMAGE, NULL);
              if (new_node && dd->root) {
                new_node->x = nx;
                new_node->y = ny;

                // Maintain aspect ratio
                int img_w = 0, img_h = 0;
                int size_ok = mac_get_image_size(path, &img_w, &img_h);
                ASLAN_DEBUG("design image size for '%s': %d x %d (ok=%d)", path,
                            img_w, img_h, size_ok);

                if (size_ok && img_w > 0 && img_h > 0) {
                  float aspect = (float)img_w / (float)img_h;
                  if (aspect > 1.0f) {
                    new_node->width = 300.0f;
                    new_node->height = 300.0f / aspect;
                  } else {
                    new_node->height = 300.0f;
                    new_node->width = 300.0f * aspect;
                  }
                } else {
                  new_node->width = 200.0f;
                  new_node->height = 150.0f;
                }

                new_node->visible = 1;
                new_node->parent = dd->root;
                strncpy(new_node->image_path, rel,
                        sizeof(new_node->image_path) - 1);

                // Add to tree
                if (!dd->root->first_child)
                  dd->root->first_child = new_node;
                else {
                  Node *l = dd->root->first_child;
                  while (l->next_sibling)
                    l = l->next_sibling;
                  l->next_sibling = new_node;
                }
              }
            }
          }
        } else if (ds->pending_create == 11) {
          // Chart
          new_node = node_create(dd->next_node_id++, NODE_TYPE_CHART, "Chart");
          if (new_node) {
            new_node->x = nx;
            new_node->y = ny;
            new_node->width = 400;
            new_node->height = 300;
            new_node->parent = dd->root;
            node_add_child(dd->root, new_node);

            // Auto-detect source table
            Page *page = doc_page_get_active(engine->doc);
            if (page && new_node->chart_data) {
              for (Block *b = page->first_block; b; b = b->next) {
                if (b->type == BLOCK_TABLE && b->data.table.columnar) {
                  new_node->chart_data->source_table_id = b->id;
                  break;
                }
              }
            }
          }
        } else if (ds->pending_create >= 10 && ds->pending_create <= 13) {
          NodeStyleType styles[] = {NODE_STYLE_RECT, NODE_STYLE_ELLIPSE,
                                    NODE_STYLE_TRIANGLE, NODE_STYLE_LINE};
          new_node =
              design_create_styled_node(dd, styles[ds->pending_create - 10], nx,
                                        ny, def_w, def_h, ds->selected_color);
        }

        if (new_node)
          ui->selected_node = new_node;
        ds->pending_create = 0;
      }

      // Handle Bezier point dragging
      if (ds->bezier_mode && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_PATH) {
        Node *node = ui->selected_node;
        if (ds->dragging_point_idx != -1 && (ui->mouse_buttons & 1)) {
          float dx = ctx->input.mouse.delta.x / scale;
          float dy = ctx->input.mouse.delta.y / scale;
          NodePathPoint *p = &node->path_points[ds->dragging_point_idx];
          if (ds->dragging_cp_idx == 0) {
            p->x += dx;
            p->y += dy;
          } else if (ds->dragging_cp_idx == 1) {
            p->cp1x += dx;
            p->cp1y += dy;
          } else if (ds->dragging_cp_idx == 2) {
            p->cp2x += dx;
            p->cp2y += dy;
          }
          node_update_path_bounds(node);
        } else {
          ds->dragging_point_idx = -1;
        }
      }

      // --- Render nodes ---
      if (dd->root) {
        for (Node *node = dd->root->first_child; node;
             node = node->next_sibling) {
          if (!node->visible)
            continue;

          float sx = bounds.x + node->x * scale;
          float sy = bounds.y + node->y * scale;
          float sw = node->width * scale;
          float sh = node->height * scale;
          struct nk_rect nr = nk_rect(sx, sy, sw, sh);

          struct nk_color fill = {
              (node->fill_color >> 24) & 0xFF, (node->fill_color >> 16) & 0xFF,
              (node->fill_color >> 8) & 0xFF, node->fill_color & 0xFF};
          struct nk_color stroke = {(node->stroke_color >> 24) & 0xFF,
                                    (node->stroke_color >> 16) & 0xFF,
                                    (node->stroke_color >> 8) & 0xFF,
                                    node->stroke_color & 0xFF};

          int drawn_image = 0;
          int has_fill = (node->fill_color & 0xFF) > 10;

          if (node->type == NODE_TYPE_CHART && node->chart_data) {
            Page *curr_page = doc_page_get_active(engine->doc);
            ui_render_chart_node(ui, ctx, node, nr, curr_page);

            // Rendering configuration bar if selected
            if (ui->selected_node == node) {
              ui_render_chart_node_config_bar(ui, engine, ctx, node, nr,
                                              curr_page);
            }
          } else if (node->type == NODE_TYPE_STROKE && node->stroke_data) {
            Stroke *s = node->stroke_data;
            if (s->count >= 2) {
              struct nk_color st_col =
                  nk_rgba((s->color >> 24) & 0xFF, (s->color >> 16) & 0xFF,
                          (s->color >> 8) & 0xFF, s->color & 0xFF);

              struct nk_vec2 *pts =
                  (struct nk_vec2 *)malloc(s->count * sizeof(struct nk_vec2));
              if (pts) {
                for (int k = 0; k < s->count; k++) {
                  pts[k].x = bounds.x + s->points[k].x * scale;
                  pts[k].y = bounds.y + s->points[k].y * scale;
                }
                nk_stroke_polyline(canvas_buf, (float *)pts, s->count,
                                   s->thickness * scale, st_col);

                // Render Endings for Strokes (Consistency with Canvas)
                if (node->line_start_style || node->line_end_style) {
                  float size =
                      (s->thickness * scale > 2.0f ? s->thickness * scale
                                                   : 2.0f) *
                      4.0f;

                  // Start Style
                  if (node->line_start_style > 0) {
                    float start_x = pts[0].x;
                    float start_y = pts[0].y;
                    float dx = pts[1].x - start_x;
                    float dy = pts[1].y - start_y;
                    float angle = atan2f(dy, dx);

                    if (node->line_start_style == 1) { // Circle
                      nk_fill_circle(canvas_buf,
                                     nk_rect(start_x - size / 2,
                                             start_y - size / 2, size, size),
                                     st_col);
                    } else if (node->line_start_style == 2) { // Arrow
                      float arrow_angle = angle + 3.14159f;
                      float tip_x = start_x;
                      float tip_y = start_y;
                      float base_angle1 = arrow_angle + 0.5f;
                      float base_angle2 = arrow_angle - 0.5f;
                      float p1x = tip_x + size * cosf(base_angle1);
                      float p1y = tip_y + size * sinf(base_angle1);
                      float p2x = tip_x + size * cosf(base_angle2);
                      float p2y = tip_y + size * sinf(base_angle2);
                      nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x,
                                       p2y, st_col);
                    }
                  }

                  // End Style
                  if (node->line_end_style > 0) {
                    float end_x = pts[s->count - 1].x;
                    float end_y = pts[s->count - 1].y;
                    float dx = end_x - pts[s->count - 2].x;
                    float dy = end_y - pts[s->count - 2].y;
                    float angle = atan2f(dy, dx);

                    if (node->line_end_style == 1) { // Circle
                      nk_fill_circle(canvas_buf,
                                     nk_rect(end_x - size / 2, end_y - size / 2,
                                             size, size),
                                     st_col);
                    } else if (node->line_end_style == 2) { // Arrow
                      float tip_x = end_x;
                      float tip_y = end_y;
                      // Arrow tip at end, base points away
                      // standard arrow at end points OUTWARD usually.
                      // ui_canvas.c logic: base_angle = angle + PI +/- 0.5.
                      // Wait, ui_canvas.c had angle (dy,dx).
                      // If line goes right (0 rad), arrow should point right.
                      // Tip at end. Base to left.
                      // So angle + PI is correct for base calculation relative
                      // to tip.
                      float effective_angle = angle;
                      float base_angle_a = effective_angle + 3.14159f + 0.5f;
                      float base_angle_b = effective_angle + 3.14159f - 0.5f;

                      float p1x = tip_x + size * cosf(base_angle_a);
                      float p1y = tip_y + size * sinf(base_angle_a);
                      float p2x = tip_x + size * cosf(base_angle_b);
                      float p2y = tip_y + size * sinf(base_angle_b);
                      nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x,
                                       p2y, st_col);
                    }
                  }
                }

                free(pts);
              }
            }
            continue;
          }

          if (node->type == NODE_TYPE_IMAGE) {
            int drawn = 0;
            if (node->image_path[0]) {
              SDL_Texture *tex = get_image_texture(ui, node->image_path);
              if (tex) {
                struct nk_image img = nk_image_ptr(tex);
                nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
                drawn = 1;
              }
            }
            if (!drawn) {
              nk_fill_rect(canvas_buf, nr, 0, nk_rgb(200, 200, 200));
            }
            if (node->stroke_width > 0)
              nk_stroke_rect(canvas_buf, nr, node->corner_radius * scale,
                             node->stroke_width * scale, stroke);
          } else if (node->type == NODE_TYPE_RECT) {
            if (node->is_container && !node->image_path[0]) {
              struct nk_color cbg = nk_rgba(66, 133, 244, 40);
              struct nk_color cst = nk_rgba(66, 133, 244, 255);
              nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, cbg);
              nk_stroke_rect(canvas_buf, nr, node->corner_radius * scale, 1.0f,
                             cst);
              if (ui->font_icons) {
                struct nk_rect ir =
                    nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
                nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
              }
              drawn_image = 1;
            }
            if (!drawn_image && has_fill) {
              nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, fill);
            }
            if (node->stroke_width > 0)
              nk_stroke_rect(canvas_buf, nr, node->corner_radius * scale,
                             node->stroke_width, stroke);

            // Line Endings (Start/End Styles)
            // Only apply if it looks like a line (thin)
            if (node->height < 8.0f || node->width < 8.0f) {
              float cx = sx + sw / 2.0f;
              float cy = sy + sh / 2.0f;
              float rad = node->rotation * 3.14159f / 180.0f;
              float c = cosf(rad);
              float s = sinf(rad);

              // Calculate geometric start/end relative to center, rotated
              // Assume standard line is horizontal (width > height)
              float half_w = sw / 2.0f;

              // Start point (left)
              float start_x = cx - half_w * c;
              float start_y = cy - half_w * s;

              // End point (right)
              float end_x = cx + half_w * c;
              float end_y = cy + half_w * s;

              float size =
                  (node->stroke_width > 2.0f ? node->stroke_width : 2.0f) *
                  4.0f * scale;

              // Render Start Style
              if (node->line_start_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(start_x - size / 2, start_y - size / 2, size, size),
                    stroke);
              } else if (node->line_start_style ==
                         2) { // Arrow (pointing left/outward)
                // Rotate arrow points
                // Tip at start_x, start_y. Base towards center.
                float angle =
                    rad + 3.14159f;    // Pointing away from center (left)
                float tip_x = start_x; // Tip exactly at geometric start
                float tip_y = start_y;

                // Base points
                float base_angle1 = angle + 2.5f; // ~145 deg
                float base_angle2 = angle - 2.5f;

                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);

                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 stroke);
              }

              // Render End Style
              if (node->line_end_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(end_x - size / 2, end_y - size / 2, size, size),
                    stroke);
              } else if (node->line_end_style ==
                         2) {      // Arrow (pointing right/outward)
                                   // Tip at end_x, end_y. Base towards center.
                float angle = rad; // Pointing away from center (right)
                float tip_x = end_x; // Tip exactly at geometric end
                float tip_y = end_y;

                // Base points
                float base_angle1 = angle + 2.5f;
                float base_angle2 = angle - 2.5f;

                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);

                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 stroke);
              }
            }
          } else if (node->type == NODE_TYPE_LINE) {
            // Render explicit LINE type as a horizontal stroke through the
            // center of its bounds
            float line_y = nr.y + nr.h / 2.0f;
            float line_w =
                (node->stroke_width > 0 ? node->stroke_width : 2.0f) * scale;
            struct nk_color line_col = stroke;

            // Draw the main line
            nk_stroke_line(canvas_buf, nr.x, line_y, nr.x + nr.w, line_y,
                           line_w, line_col);

            // Line Endings
            float size = (line_w > 2.0f ? line_w : 2.0f) * 4.0f;

            // Start Style (Left)
            if (node->line_start_style > 0) {
              if (node->line_start_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(nr.x - size / 2, line_y - size / 2, size, size),
                    line_col);
              } else if (node->line_start_style == 2) { // Arrow (Left <--)
                nk_fill_triangle(canvas_buf, nr.x, line_y, nr.x + size,
                                 line_y - size / 2, nr.x + size,
                                 line_y + size / 2, line_col);
              }
            }

            // End Style (Right)
            if (node->line_end_style > 0) {
              if (node->line_end_style == 1) { // Circle
                nk_fill_circle(canvas_buf,
                               nk_rect(nr.x + nr.w - size / 2,
                                       line_y - size / 2, size, size),
                               line_col);
              } else if (node->line_end_style == 2) { // Arrow (Right -->)
                nk_fill_triangle(canvas_buf, nr.x + nr.w, line_y,
                                 nr.x + nr.w - size, line_y - size / 2,
                                 nr.x + nr.w - size, line_y + size / 2,
                                 line_col);
              }
            }

          } else if (node->type == NODE_TYPE_PATH && node->path_points &&
                     node->path_point_count > 1) {
            // Fill path
            if (fill.a > 0) {
              float poly_pts[512 * 2];
              int poly_count =
                  node_get_path_polygon(node, poly_pts, 512, sx, sy, scale);
              if (poly_count > 2) {
                nk_fill_polygon(canvas_buf, poly_pts, poly_count, fill);
              }
            }

            // Stroke path
            for (int k = 0; k < node->path_point_count; k++) {
              int nxt = (k + 1) % node->path_point_count;
              NodePathPoint p1 = node->path_points[k],
                            p2 = node->path_points[nxt];
              if (p1.type == 1) // Cubic Bezier
                nk_stroke_curve(canvas_buf, sx + p1.x * scale,
                                sy + p1.y * scale, sx + p1.cp2x * scale,
                                sy + p1.cp2y * scale, sx + p2.cp1x * scale,
                                sy + p2.cp1y * scale, sx + p2.x * scale,
                                sy + p2.y * scale, node->stroke_width * scale,
                                stroke);
              else
                nk_stroke_line(canvas_buf, sx + p1.x * scale, sy + p1.y * scale,
                               sx + p2.x * scale, sy + p2.y * scale,
                               node->stroke_width * scale, stroke);
            }

            // Line Endings for Paths (only if 2 points)
            if (node->path_point_count == 2) {
              NodePathPoint p1 = node->path_points[0];
              NodePathPoint p2 = node->path_points[1];
              float start_x = sx + p1.x * scale;
              float start_y = sy + p1.y * scale;
              float end_x = sx + p2.x * scale;
              float end_y = sy + p2.y * scale;

              float dx = end_x - start_x;
              float dy = end_y - start_y;
              float angle = atan2f(dy, dx);
              float size =
                  (node->stroke_width > 2.0f ? node->stroke_width : 2.0f) *
                  4.0f * scale;

              // Start Style
              if (node->line_start_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(start_x - size / 2, start_y - size / 2, size, size),
                    stroke);
              } else if (node->line_start_style ==
                         2) { // Arrow (pointing away from line)
                // Tip at start_x, start_y. Pointing away means angle + PI.
                float arrow_angle = angle + 3.14159f;
                float tip_x = start_x; // Tip is exactly at start point
                float tip_y = start_y;

                // Move tip slightly out so it doesn't overlap line end exactly?
                // No, exact is fine. Actually, if we want the arrow tip to be
                // at start_x, we compute base from there.

                float base_angle1 = arrow_angle + 0.5f; // ~28 deg
                float base_angle2 = arrow_angle - 0.5f;

                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);

                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 stroke);
              }

              // End Style
              if (node->line_end_style == 1) { // Circle
                nk_fill_circle(
                    canvas_buf,
                    nk_rect(end_x - size / 2, end_y - size / 2, size, size),
                    stroke);
              } else if (node->line_end_style == 2) { // Arrow
                float tip_x = end_x;
                float tip_y = end_y;

                float base_angle1 = angle + 3.14159f + 0.5f;
                float base_angle2 = angle + 3.14159f - 0.5f;

                float p1x = tip_x + size * cosf(base_angle1);
                float p1y = tip_y + size * sinf(base_angle1);
                float p2x = tip_x + size * cosf(base_angle2);
                float p2y = tip_y + size * sinf(base_angle2);

                nk_fill_triangle(canvas_buf, tip_x, tip_y, p1x, p1y, p2x, p2y,
                                 stroke);
              }
            }
          } else if (node->type == NODE_TYPE_ELLIPSE) {
            if (node->is_container && !node->image_path[0]) {
              struct nk_color cbg = nk_rgba(66, 133, 244, 40);
              struct nk_color cst = nk_rgba(66, 133, 244, 255);
              nk_fill_circle(canvas_buf, nr, cbg);
              nk_stroke_circle(canvas_buf, nr, 1.0f, cst);
              if (ui->font_icons) {
                struct nk_rect ir =
                    nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
                nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
              }
              drawn_image = 1;
            }
            if (!drawn_image && has_fill)
              nk_fill_circle(canvas_buf, nr, fill);
            if (node->stroke_width > 0)
              nk_stroke_circle(canvas_buf, nr, node->stroke_width, stroke);
          } else if (node->type == NODE_TYPE_TRIANGLE) {
            if (node->is_container && !node->image_path[0]) {
              struct nk_color cbg = nk_rgba(66, 133, 244, 40);
              struct nk_color cst = nk_rgba(66, 133, 244, 255);
              float vpts[6] = {sx + sw / 2, sy, sx, sy + sh, sx + sw, sy + sh};
              nk_fill_polygon(canvas_buf, vpts, 3, cbg);
              nk_stroke_polygon(canvas_buf, vpts, 3, 1.0f, cst);
              if (ui->font_icons) {
                struct nk_rect ir =
                    nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 5, 20, 20);
                nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
              }
              drawn_image = 1;
            }
            float pts[6] = {sx + sw / 2, sy,       // Top
                            sx,          sy + sh,  // Bottom-left
                            sx + sw,     sy + sh}; // Bottom-right
            if (!drawn_image && has_fill)
              nk_fill_polygon(canvas_buf, pts, 3, fill);
            if (node->stroke_width > 0)
              nk_stroke_polygon(canvas_buf, pts, 3, node->stroke_width, stroke);
          } else if (node->type == NODE_TYPE_PENTAGON) {
            if (node->is_container && !node->image_path[0]) {
              struct nk_color cbg = nk_rgba(66, 133, 244, 40);
              struct nk_color cst = nk_rgba(66, 133, 244, 255);
              struct nk_vec2 vpts[5];
              for (int i = 0; i < 5; i++) {
                float angle =
                    (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
                vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
                vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
              }
              nk_fill_polygon(canvas_buf, (const float *)vpts, 5, cbg);
              nk_stroke_polygon(canvas_buf, (const float *)vpts, 5, 1.0f, cst);
              if (ui->font_icons) {
                struct nk_rect ir =
                    nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
                nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
              }
              drawn_image = 1;
            }
            struct nk_vec2 pts[5];
            for (int i = 0; i < 5; i++) {
              float angle = (float)i * 2.0f * 3.14159f / 5.0f - 3.14159f / 2.0f;
              pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
              pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
            }
            if (!drawn_image && has_fill)
              nk_fill_polygon(canvas_buf, (const float *)pts, 5, fill);
            if (node->stroke_width > 0)
              nk_stroke_polygon(canvas_buf, (const float *)pts, 5,
                                node->stroke_width, stroke);
          } else if (node->type == NODE_TYPE_HEXAGON) {
            if (node->is_container && !node->image_path[0]) {
              struct nk_color cbg = nk_rgba(66, 133, 244, 40);
              struct nk_color cst = nk_rgba(66, 133, 244, 255);
              struct nk_vec2 vpts[6];
              for (int i = 0; i < 6; i++) {
                float angle = (float)i * 2.0f * 3.14159f / 6.0f;
                vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
                vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
              }
              nk_fill_polygon(canvas_buf, (const float *)vpts, 6, cbg);
              nk_stroke_polygon(canvas_buf, (const float *)vpts, 6, 1.0f, cst);
              if (ui->font_icons) {
                struct nk_rect ir =
                    nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
                nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
              }
              drawn_image = 1;
            }
            struct nk_vec2 pts[6];
            for (int i = 0; i < 6; i++) {
              float angle = (float)i * 2.0f * 3.14159f / 6.0f;
              pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
              pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
            }
            if (!drawn_image && has_fill)
              nk_fill_polygon(canvas_buf, (const float *)pts, 6, fill);
            if (node->stroke_width > 0)
              nk_stroke_polygon(canvas_buf, (const float *)pts, 6,
                                node->stroke_width, stroke);
          } else if (node->type == NODE_TYPE_OCTAGON) {
            if (node->is_container && !node->image_path[0]) {
              struct nk_color cbg = nk_rgba(66, 133, 244, 40);
              struct nk_color cst = nk_rgba(66, 133, 244, 255);
              struct nk_vec2 vpts[8];
              for (int i = 0; i < 8; i++) {
                float angle =
                    (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
                vpts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
                vpts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
              }
              nk_fill_polygon(canvas_buf, (const float *)vpts, 8, cbg);
              nk_stroke_polygon(canvas_buf, (const float *)vpts, 8, 1.0f, cst);
              if (ui->font_icons) {
                struct nk_rect ir =
                    nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
                nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
              }
              drawn_image = 1;
            }
            struct nk_vec2 pts[8];
            for (int i = 0; i < 8; i++) {
              float angle = (float)i * 2.0f * 3.14159f / 8.0f + 3.14159f / 8.0f;
              pts[i].x = nr.x + nr.w / 2 + cosf(angle) * nr.w / 2;
              pts[i].y = nr.y + nr.h / 2 + sinf(angle) * nr.h / 2;
            }
            if (!drawn_image && has_fill)
              nk_fill_polygon(canvas_buf, (const float *)pts, 8, fill);
            if (node->stroke_width > 0)
              nk_stroke_polygon(canvas_buf, (const float *)pts, 8,
                                node->stroke_width, stroke);
          } else if (node->type == NODE_TYPE_HEART) {
            if (node->is_container && !node->image_path[0]) {
              struct nk_color cbg = nk_rgba(66, 133, 244, 40);
              struct nk_color cst = nk_rgba(66, 133, 244, 255);
              const int vhcount = 48;
              struct nk_vec2 vhpts[48];
              for (int i = 0; i < vhcount; i++) {
                float t = (float)i * 2.0f * 3.14159f / (float)vhcount;
                float hx = 16.0f * powf(sinf(t), 3.0f);
                float hy = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                             2.0f * cosf(3.0f * t) - cosf(4.0f * t));
                vhpts[i].x = nr.x + nr.w / 2.0f + (hx / 32.0f) * nr.w;
                vhpts[i].y = nr.y + nr.h * 0.45f + (hy / 32.0f) * nr.h;
              }
              nk_fill_polygon(canvas_buf, (const float *)vhpts, vhcount, cbg);
              nk_stroke_polygon(canvas_buf, (const float *)vhpts, vhcount, 1.0f,
                                cst);
              if (ui->font_icons) {
                struct nk_rect ir =
                    nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
                nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
              }
              drawn_image = 1;
            }
            const int hcount = 48;
            struct nk_vec2 hpts[48];
            for (int i = 0; i < hcount; i++) {
              float t = (float)i * 2.0f * 3.14159f / (float)hcount;
              float hx = 16.0f * powf(sinf(t), 3.0f);
              float hy = -(13.0f * cosf(t) - 5.0f * cosf(2.0f * t) -
                           2.0f * cosf(3.0f * t) - cosf(4.0f * t));
              hpts[i].x = nr.x + nr.w / 2.0f + (hx / 32.0f) * nr.w;
              hpts[i].y = nr.y + nr.h * 0.45f + (hy / 32.0f) * nr.h;
            }
            if (!drawn_image && has_fill)
              nk_fill_polygon(canvas_buf, (const float *)hpts, hcount, fill);
            if (node->stroke_width > 0)
              nk_stroke_polygon(canvas_buf, (const float *)hpts, hcount,
                                node->stroke_width, stroke);
          } else if (node->type == NODE_TYPE_TEXT) {
            // Text node

            // Render background for sticky notes / text with bg
            if (fill.a > 0)
              nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, fill);

            const char *display_text = node->text;
            int is_editing = (ui->editing_node == node);
            if (is_editing) {
              display_text = ui->edit_buffer;
              nk_fill_rect(canvas_buf,
                           nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2),
                           2.0f, nk_rgba(66, 133, 244, 20));
              nk_stroke_rect(canvas_buf,
                             nk_rect(nr.x - 2, nr.y - 1, nr.w + 4, nr.h + 2),
                             2.0f, 1.0f, nk_rgb(66, 133, 244));
            }
            if (display_text && strlen(display_text) > 0) {
              const struct nk_user_font *font =
                  ui->font_inter ? &ui->font_inter->handle : ctx->style.font;

              // Select font based on type and size
              if (node->font_type == 4 && ui->font_mono) { // Mono
                font = &ui->font_mono->handle;
              } else { // Inter variants (Body, H1, H2, H3)
                // Find closest size in font_inter_sizes
                float target_size = node->font_size;
                float best_diff = 1000.0f;
                int best_idx = -1;
                float available_sizes[] = {12.0f, 14.0f, 16.0f, 20.0f, 24.0f,
                                           32.0f, 48.0f, 64.0f, 96.0f};

                for (int i = 0; i < 9; i++) {
                  float diff = fabsf(available_sizes[i] - target_size);
                  if (diff < best_diff) {
                    best_diff = diff;
                    best_idx = i;
                  }
                }

                if (best_idx >= 0 && ui->font_inter_sizes[best_idx]) {
                  font = &ui->font_inter_sizes[best_idx]->handle;
                } else if (ui->font_inter) {
                  font = &ui->font_inter->handle;
                }
              }

              // Auto-resize text node to fit content
              if (display_text && strlen(display_text) > 0) {
                float text_w = font->width(font->userdata, font->height,
                                           display_text, strlen(display_text));
                float text_h = font->height;

                // Only update if dimensions differ significantly (prevent
                // jitter)
                if (fabsf(node->width - (text_w + 10.0f)) > 1.0f ||
                    fabsf(node->height - (text_h + 10.0f)) > 1.0f) {
                  node->width = text_w + 10.0f; // Add padding
                  node->height = text_h + 10.0f;
                }
              }

              struct nk_color txt_color = {(node->stroke_color >> 24) & 0xFF,
                                           (node->stroke_color >> 16) & 0xFF,
                                           (node->stroke_color >> 8) & 0xFF,
                                           node->stroke_color & 0xFF};

              nk_draw_text(canvas_buf, nr, display_text, strlen(display_text),
                           font, nk_rgba(0, 0, 0, 0), txt_color);
              if (is_editing) {
                float tw = font->width(font->userdata, font->height,
                                       display_text, strlen(display_text));
                int show_cursor = ((int)(SDL_GetTicks() / 500) % 2 == 0);
                if (show_cursor) {
                  nk_stroke_line(canvas_buf, nr.x + tw + 1, nr.y + 2,
                                 nr.x + tw + 1, nr.y + nr.h - 2, 1.5f,
                                 nk_rgb(66, 133, 244));
                }
              }
            }
          }

          // Selection highlight
          if (node == ui->selected_node && is_active_design &&
              !ds->bezier_mode) {
            float sel_off = 2.0f + (node->stroke_width * scale) / 2.0f;
            nk_stroke_rect(canvas_buf,
                           nk_rect(sx - sel_off, sy - sel_off, sw + sel_off * 2,
                                   sh + sel_off * 2),
                           2.0f, 2.0f, nk_rgb(66, 133, 244));

            // Resize handles
            float hsz = 8.0f;
            struct nk_color hc = nk_rgb(66, 133, 244);

            // Helper for corners accounting for offset
            float h_left = sx - sel_off;
            float h_top = sy - sel_off;
            float h_right = sx + sw + sel_off;
            float h_bottom = sy + sh + sel_off;

            // SE
            nk_fill_rect(
                canvas_buf,
                nk_rect(h_right - hsz / 2, h_bottom - hsz / 2, hsz, hsz), 2.0f,
                hc);
            // SW
            nk_fill_rect(
                canvas_buf,
                nk_rect(h_left - hsz / 2, h_bottom - hsz / 2, hsz, hsz), 2.0f,
                hc);
            // NW
            nk_fill_rect(canvas_buf,
                         nk_rect(h_left - hsz / 2, h_top - hsz / 2, hsz, hsz),
                         2.0f, hc);
            // NE
            nk_fill_rect(canvas_buf,
                         nk_rect(h_right - hsz / 2, h_top - hsz / 2, hsz, hsz),
                         2.0f, hc);
          }
        }
      }

      // Check mouse in design area
      int mouse_in_design =
          (ui->mouse_x >= bounds.x && ui->mouse_x < bounds.x + bounds.w &&
           ui->mouse_y >= bounds.y && ui->mouse_y < bounds.y + bounds.h);

      // Explicitly exclude sidebar area in fullscreen
      if (is_fullscreen && ui->mouse_x > nk_window_get_width(ctx) - 250.0f) {
        mouse_in_design = 0;
      }

      // --- Handle resize ---
      int on_resize_handle = 0;
      int hit_handle_index = 0;

      if (ui->selected_node && ui->selected_node->visible && is_active_design) {
        Node *sel = ui->selected_node;
        float sx = bounds.x + sel->x * scale;
        float sy = bounds.y + sel->y * scale;
        float sw = sel->width * scale;
        float sh = sel->height * scale;
        float hsize = 30.0f;

        // SE
        if (ui->mouse_x >= sx + sw - hsize / 2 &&
            ui->mouse_x < sx + sw + hsize / 2 &&
            ui->mouse_y >= sy + sh - hsize / 2 &&
            ui->mouse_y < sy + sh + hsize / 2) {
          on_resize_handle = 1;
          hit_handle_index = 0;
        }
        // SW
        else if (ui->mouse_x >= sx - hsize / 2 &&
                 ui->mouse_x < sx + hsize / 2 &&
                 ui->mouse_y >= sy + sh - hsize / 2 &&
                 ui->mouse_y < sy + sh + hsize / 2) {
          on_resize_handle = 1;
          hit_handle_index = 1;
        }
        // NW
        else if (ui->mouse_x >= sx - hsize / 2 &&
                 ui->mouse_x < sx + hsize / 2 &&
                 ui->mouse_y >= sy - hsize / 2 &&
                 ui->mouse_y < sy + hsize / 2) {
          on_resize_handle = 1;
          hit_handle_index = 2;
        }
        // NE
        else if (ui->mouse_x >= sx + sw - hsize / 2 &&
                 ui->mouse_x < sx + sw + hsize / 2 &&
                 ui->mouse_y >= sy - hsize / 2 &&
                 ui->mouse_y < sy + hsize / 2) {
          on_resize_handle = 1;
          hit_handle_index = 3;
        }
      }

      // Start resize
      if (mouse_in_design && ds->mouse_just_pressed && on_resize_handle &&
          ui->selected_node && is_active_design) {
        ds->is_resizing = 1;
        ds->resize_handle = hit_handle_index;
        ds->resize_start_w = ui->selected_node->width;
        ds->resize_start_h = ui->selected_node->height;
        ds->resize_start_x = ui->mouse_x;
        ds->resize_start_y = ui->mouse_y;
        ds->resize_start_node_x = ui->selected_node->x;
        ds->resize_start_node_y = ui->selected_node->y;
      }
      // Handle resize dragging
      else if (ds->is_resizing && ui->selected_node && is_active_design) {
        float dx = (ui->mouse_x - ds->resize_start_x) / scale;
        float dy = (ui->mouse_y - ds->resize_start_y) / scale;
        Node *sel = ui->selected_node;

        switch (ds->resize_handle) {
        case 0: // SE
          sel->width = fmaxf(20.0f, ds->resize_start_w + dx);
          sel->height = fmaxf(20.0f, ds->resize_start_h + dy);
          break;
        case 1: // SW
          sel->width = fmaxf(20.0f, ds->resize_start_w - dx);
          sel->height = fmaxf(20.0f, ds->resize_start_h + dy);
          sel->x = ds->resize_start_node_x + (ds->resize_start_w - sel->width);
          break;
        case 2: // NW
          sel->width = fmaxf(20.0f, ds->resize_start_w - dx);
          sel->height = fmaxf(20.0f, ds->resize_start_h - dy);
          sel->x = ds->resize_start_node_x + (ds->resize_start_w - sel->width);
          sel->y = ds->resize_start_node_y + (ds->resize_start_h - sel->height);
          break;
        case 3: // NE
          sel->width = fmaxf(20.0f, ds->resize_start_w + dx);
          sel->height = fmaxf(20.0f, ds->resize_start_h - dy);
          sel->y = ds->resize_start_node_y + (ds->resize_start_h - sel->height);
          break;
        }

        if (ds->mouse_just_released) {
          ds->is_resizing = 0;
        }
      }
      // 4. Handle Bezier interaction priority
      int handle_hit = 0;
      if (ds->bezier_mode && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_PATH && is_active_design) {
        Node *sel = ui->selected_node;
        float nsx = bounds.x + sel->x * scale;
        float nsy = bounds.y + sel->y * scale;
        for (int i = 0; i < sel->path_point_count; i++) {
          NodePathPoint *p = &sel->path_points[i];
          float px = nsx + p->x * scale, py = nsy + p->y * scale;
          struct nk_rect r_anchor =
              nk_rect(px - 10 * scale, py - 10 * scale, 20 * scale, 20 * scale);
          if (nk_input_is_mouse_hovering_rect(&ctx->input, r_anchor)) {
            if (ds->mouse_just_pressed) {
              ds->dragging_point_idx = i;
              ds->dragging_cp_idx = 0;
            }
            handle_hit = 1;
            break;
          }
          if (p->type == 1) {
            float cp1x = nsx + p->cp1x * scale, cp1y = nsy + p->cp1y * scale;
            float cp2x = nsx + p->cp2x * scale, cp2y = nsy + p->cp2y * scale;
            struct nk_rect r1 = nk_rect(cp1x - 8 * scale, cp1y - 8 * scale,
                                        16 * scale, 16 * scale);
            struct nk_rect r2 = nk_rect(cp2x - 8 * scale, cp2y - 8 * scale,
                                        16 * scale, 16 * scale);
            if (nk_input_is_mouse_hovering_rect(&ctx->input, r1)) {
              if (ds->mouse_just_pressed) {
                ds->dragging_point_idx = i;
                ds->dragging_cp_idx = 1;
              }
              handle_hit = 1;
              break;
            }
            if (nk_input_is_mouse_hovering_rect(&ctx->input, r2)) {
              if (ds->mouse_just_pressed) {
                ds->dragging_point_idx = i;
                ds->dragging_cp_idx = 2;
              }
              handle_hit = 1;
              break;
            }
          }
        }
      }

      // Handle click in design area
      if (handle_hit) {
        // Handle hit, skip node selection
      } else if (mouse_in_design && !in_toolbar && !in_forms_menu &&
                 ds->mouse_just_pressed && !on_resize_handle &&
                 is_active_design) {
        // Convert to design coordinates
        float design_x = (ui->mouse_x - bounds.x) / scale;
        float design_y = (ui->mouse_y - bounds.y) / scale;

        Node *hit = design_find_node_at(dd, design_x, design_y);
        if (hit) {
          if (ds->tool == CANVAS_TOOL_PEN) {
            // Skip node logic in pen mode to allow drawing over them
            hit = NULL;
          } else {
            // Check for double-click on text node
            if (hit == ui->selected_node && hit->type == NODE_TYPE_TEXT) {
              // Start inline editing
              ui->editing_node = hit;
              strncpy(ui->edit_buffer, hit->text, sizeof(ui->edit_buffer) - 1);
              SDL_StartTextInput();
            }
            ui->selected_node = hit;
            ds->is_dragging_node = 1;
            ds->drag_offset_x = design_x - hit->x;
            ds->drag_offset_y = design_y - hit->y;
          }
        } else {
          ui->selected_node = NULL;
          if (ui->editing_node) {
            strncpy(ui->editing_node->text, ui->edit_buffer,
                    sizeof(ui->editing_node->text) - 1);
            ui->editing_node = NULL;
            SDL_StopTextInput();
          }
        }
      }

      // Handle node dragging
      if (ds->is_dragging_node && ui->selected_node && is_active_design) {
        float design_x = (ui->mouse_x - bounds.x) / scale;
        float design_y = (ui->mouse_y - bounds.y) / scale;
        ui->selected_node->x = design_x - ds->drag_offset_x;
        ui->selected_node->y = design_y - ds->drag_offset_y;

        // Clamp to design bounds
        ui->selected_node->x =
            fmaxf(0, fminf(ui->selected_node->x,
                           dd->width - ui->selected_node->width));
        ui->selected_node->y =
            fmaxf(0, fminf(ui->selected_node->y,
                           dd->height - ui->selected_node->height));

        if (ds->mouse_just_released) {
          ds->is_dragging_node = 0;
        }
      }

      // Close forms menu if clicking elsewhere
      if (ds->mouse_just_pressed && !in_toolbar && !in_forms_menu &&
          is_active_design) {
        ds->show_forms_menu = 0;
      }

      // --- PEN TOOL logic (Adapted for Design) ---
      if (ds->tool == CANVAS_TOOL_PEN && is_active_design && !in_toolbar &&
          !in_forms_menu && ((ui->mouse_buttons & 1))) {
        float wx = (ui->mouse_x - bounds.x) / scale;
        float wy = (ui->mouse_y - bounds.y) / scale;

        if (ds->mouse_just_pressed) {
          ds->current_stroke = (Stroke *)calloc(1, sizeof(Stroke));
          ds->current_stroke->capacity = 128;
          ds->current_stroke->points = (Point *)calloc(128, sizeof(Point));
          if (ds->pen_is_marker) {
            ds->current_stroke->thickness = ds->pen_thickness * 10.0f;
            uint32_t c = ds->selected_color;
            uint8_t r = (c >> 24) & 0xFF;
            uint8_t g = (c >> 16) & 0xFF;
            uint8_t b = (c >> 8) & 0xFF;
            uint8_t a = c & 0xFF;
            // Reduce alpha to 30% for marker effect
            ds->current_stroke->color =
                (r << 24) | (g << 16) | (b << 8) | (uint8_t)(a * 0.3f);
          } else {
            ds->current_stroke->thickness = ds->pen_thickness;
            ds->current_stroke->color = ds->selected_color;
          }
          ds->current_stroke->points[0].x = wx;
          ds->current_stroke->points[0].y = wy;
          ds->current_stroke->count = 1;

          // Wrap it in a Node so it appears in Layers
          Node *node = (Node *)calloc(1, sizeof(Node));
          if (node) {
            node->id = dd->next_node_id++;
            node->type = NODE_TYPE_STROKE;
            node->stroke_data = ds->current_stroke;
            node->visible = 1;
            node->parent = dd->root;

            // Add to document tree
            if (!dd->root->first_child) {
              dd->root->first_child = node;
            } else {
              Node *last = dd->root->first_child;
              while (last->next_sibling)
                last = last->next_sibling;
              last->next_sibling = node;
            }
            ds->current_stroke_node = node;
            ds->stroke_block_id = block->id;

            // Initialize bounds
            node->x = wx;
            node->y = wy;
            node->width = 1.0f;
            node->height = 1.0f;
          }
        } else if (ds->current_stroke && ds->stroke_block_id == block->id) {
          if (ds->current_stroke->count >= ds->current_stroke->capacity) {
            ds->current_stroke->capacity *= 2;
            ds->current_stroke->points =
                (Point *)realloc(ds->current_stroke->points,
                                 ds->current_stroke->capacity * sizeof(Point));
          }
          Point last =
              ds->current_stroke->points[ds->current_stroke->count - 1];
          if ((wx - last.x) * (wx - last.x) + (wy - last.y) * (wy - last.y) >
              4.0f) {
            ds->current_stroke->points[ds->current_stroke->count].x = wx;
            ds->current_stroke->points[ds->current_stroke->count].y = wy;
            ds->current_stroke->count++;

            // Update node bounds for culling
            if (ds->current_stroke_node) {
              float min_x = ds->current_stroke_node->x;
              float min_y = ds->current_stroke_node->y;
              float max_x = min_x + ds->current_stroke_node->width;
              float max_y = min_y + ds->current_stroke_node->height;

              if (wx < min_x)
                min_x = wx;
              if (wy < min_y)
                min_y = wy;
              if (wx > max_x)
                max_x = wx;
              if (wy > max_y)
                max_y = wy;

              ds->current_stroke_node->x = min_x;
              ds->current_stroke_node->y = min_y;
              ds->current_stroke_node->width = max_x - min_x;
              ds->current_stroke_node->height = max_y - min_y;
            }
          }
        }
      } else if (!(ui->mouse_buttons & 1)) {
        ds->current_stroke = NULL;
        ds->stroke_block_id = 0;
      }

      // --- Active Stroke Rendering ---
      if (ds->current_stroke && ds->current_stroke->count > 1 &&
          ds->stroke_block_id == block->id) {
        if (ds->pen_is_marker) {
          for (int i = 0; i < ds->current_stroke->count - 1; i++) {
            Point p1 = ds->current_stroke->points[i];
            Point p2 = ds->current_stroke->points[i + 1];
            struct nk_color col =
                nk_rgba((ds->current_stroke->color >> 24) & 0xFF,
                        (ds->current_stroke->color >> 16) & 0xFF,
                        (ds->current_stroke->color >> 8) & 0xFF,
                        ds->current_stroke->color & 0xFF);
            nk_stroke_line(canvas_buf, bounds.x + p1.x * scale,
                           bounds.y + p1.y * scale, bounds.x + p2.x * scale,
                           bounds.y + p2.y * scale,
                           ds->current_stroke->thickness * scale, col);
          }
        } else {
          for (int i = 0; i < ds->current_stroke->count - 1; i++) {
            Point p1 = ds->current_stroke->points[i];
            Point p2 = ds->current_stroke->points[i + 1];
            struct nk_color col =
                nk_rgba((ds->current_stroke->color >> 24) & 0xFF,
                        (ds->current_stroke->color >> 16) & 0xFF,
                        (ds->current_stroke->color >> 8) & 0xFF,
                        ds->current_stroke->color & 0xFF);
            nk_stroke_line(canvas_buf, bounds.x + p1.x * scale,
                           bounds.y + p1.y * scale, bounds.x + p2.x * scale,
                           bounds.y + p2.y * scale,
                           ds->current_stroke->thickness * scale, col);
          }
        }
      }

      // --- Bezier Handles Rendering Overlay ---
      if (ds->bezier_mode && is_active_design && ui->selected_node &&
          ui->selected_node->type == NODE_TYPE_PATH) {
        Node *node = ui->selected_node;
        // Ensure node belongs to this design block
        // (Basic check: if it's selected and we are active, assume it is)
        {
          float sx = bounds.x + node->x * scale;
          float sy = bounds.y + node->y * scale;

          for (int i = 0; i < node->path_point_count; i++) {
            NodePathPoint *p = &node->path_points[i];
            float px = sx + p->x * scale;
            float py = sy + p->y * scale;

            // Anchor
            struct nk_rect anchor_rect =
                nk_rect(px - 5 * scale, py - 5 * scale, 10 * scale, 10 * scale);
            nk_fill_rect(canvas_buf, anchor_rect, 0, nk_rgb(255, 255, 255));
            nk_stroke_rect(canvas_buf, anchor_rect, 0, 1.5f * scale,
                           nk_rgb(66, 133, 244));

            if (p->type == 1) { // Cubic
              float cp1x = sx + p->cp1x * scale;
              float cp1y = sy + p->cp1y * scale;
              float cp2x = sx + p->cp2x * scale;
              float cp2y = sy + p->cp2y * scale;

              nk_stroke_line(canvas_buf, px, py, cp1x, cp1y, 1,
                             nk_rgb(200, 200, 200));
              nk_stroke_line(canvas_buf, px, py, cp2x, cp2y, 1,
                             nk_rgb(200, 200, 200));

              struct nk_rect r1 = nk_rect(cp1x - 4 * scale, cp1y - 4 * scale,
                                          8 * scale, 8 * scale);
              struct nk_rect r2 = nk_rect(cp2x - 4 * scale, cp2y - 4 * scale,
                                          8 * scale, 8 * scale);
              nk_fill_circle(canvas_buf, r1, nk_rgb(66, 133, 244));
              nk_fill_circle(canvas_buf, r2, nk_rgb(66, 133, 244));
            }
          }
        }
      }

      // Close Design content group (Artboard)
      nk_group_end(ctx);
    }
    nk_layout_space_end(ctx);

    // --- Toolbar (same as Canvas) ---
    // Rendered in workspace coordinate space, not artboard space

    // Render Toolbar using Canvas logic (with state swapping)
    {
      CanvasInteraction temp_cs = ui->canvas_state;

      // Map Design state to Canvas state
      ui->canvas_state.tool = ds->tool;
      ui->canvas_state.active_cat = ds->active_cat;
      ui->canvas_state.show_sub_menu = ds->show_sub_menu;
      ui->canvas_state.pending_create = ds->pending_create;
      ui->canvas_state.mouse_just_pressed = ds->mouse_just_pressed;
      ui->canvas_state.space_held = ds->space_held;
      ui->canvas_state.pan_from_menu = ds->pan_from_menu;
      ui->canvas_state.bezier_mode = ds->bezier_mode;
      ui->canvas_state.dragging_point_idx = ds->dragging_point_idx;
      ui->canvas_state.dragging_cp_idx = ds->dragging_cp_idx;
      ui->canvas_state.selected_color = ds->selected_color;
      ui->canvas_state.pen_thickness = ds->pen_thickness;
      ui->canvas_state.pen_is_marker = ds->pen_is_marker;
      ui->canvas_state.show_color_picker_popup = ds->show_color_picker_popup;

      // Position toolbar at bottom of workspace
      canvas_render_toolbar(
          ui, ctx, nk_rect(ws_bounds.x, ws_bounds.y, ws_bounds.w, ws_bounds.h),
          block);

      // Copy back relevant state
      ds->tool = ui->canvas_state.tool;
      ds->active_cat = ui->canvas_state.active_cat;
      ds->show_sub_menu = ui->canvas_state.show_sub_menu;
      ds->pending_create = ui->canvas_state.pending_create;
      ds->space_held = ui->canvas_state.space_held;
      ds->pan_from_menu = ui->canvas_state.pan_from_menu;
      ds->bezier_mode = ui->canvas_state.bezier_mode;
      ds->dragging_point_idx = ui->canvas_state.dragging_point_idx;
      ds->dragging_cp_idx = ui->canvas_state.dragging_cp_idx;
      ds->selected_color = ui->canvas_state.selected_color;
      ds->pen_thickness = ui->canvas_state.pen_thickness;
      ds->pen_is_marker = ui->canvas_state.pen_is_marker;
      ds->show_color_picker_popup = ui->canvas_state.show_color_picker_popup;

      // Restore Canvas state
      ui->canvas_state = temp_cs;
    }

    // --- Page Navigation (next to toolbar) ---
    {
      float toolbar_w =
          6 * 36.0f + 5 * 8.0f + 24.0f; // Use standard 6-btn width
      float toolbar_x = ws_bounds.x + (ws_bounds.w - toolbar_w) / 2.0f;
      float toolbar_y = ws_bounds.y + ws_bounds.h - toolbar_h - 20.0f;

      float nav_x = toolbar_x + toolbar_w + 20.0f;
      float nav_btn_size = 24.0f;
      float nav_y = toolbar_y + (toolbar_h - nav_btn_size) / 2.0f;

      struct nk_command_buffer *canvas_buf = nk_window_get_canvas(ctx);

      // Previous page button
      struct nk_rect prev_btn =
          nk_rect(nav_x, nav_y, nav_btn_size, nav_btn_size);
      int prev_hover =
          (ui->mouse_x >= prev_btn.x && ui->mouse_x < prev_btn.x + prev_btn.w &&
           ui->mouse_y >= prev_btn.y && ui->mouse_y < prev_btn.y + prev_btn.h);
      int prev_enabled = (dd->current_page > 0);

      if (prev_hover && prev_enabled)
        nk_fill_rect(canvas_buf, prev_btn, 4.0f, nk_rgba(0, 0, 0, 20));
      nk_draw_text(canvas_buf, nk_rect(prev_btn.x + 6, prev_btn.y + 2, 16, 20),
                   "<", 1, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   prev_enabled ? nk_rgb(60, 60, 60) : nk_rgb(180, 180, 180));

      if (prev_hover && ds->mouse_just_pressed && prev_enabled &&
          (ds->active_design_id == block->id)) {
        block_design_goto_page(block, dd->current_page - 1);
        ui->selected_node = NULL;
      }

      // Page counter (e.g., "1 / 3")
      char page_str[32];
      snprintf(page_str, sizeof(page_str), "%d / %d", dd->current_page + 1,
               dd->page_count);
      float counter_x = nav_x + nav_btn_size + 4.0f;
      float counter_w = 50.0f;
      nk_draw_text(canvas_buf, nk_rect(counter_x, nav_y + 2, counter_w, 20),
                   page_str, strlen(page_str), ctx->style.font,
                   nk_rgba(0, 0, 0, 0), nk_rgb(80, 80, 80));

      // Next page button
      struct nk_rect next_btn =
          nk_rect(counter_x + counter_w, nav_y, nav_btn_size, nav_btn_size);
      int next_hover =
          (ui->mouse_x >= next_btn.x && ui->mouse_x < next_btn.x + next_btn.w &&
           ui->mouse_y >= next_btn.y && ui->mouse_y < next_btn.y + next_btn.h);
      int next_enabled = (dd->current_page < dd->page_count - 1);

      if (next_hover && next_enabled)
        nk_fill_rect(canvas_buf, next_btn, 4.0f, nk_rgba(0, 0, 0, 20));
      nk_draw_text(canvas_buf, nk_rect(next_btn.x + 6, next_btn.y + 2, 16, 20),
                   ">", 1, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   next_enabled ? nk_rgb(60, 60, 60) : nk_rgb(180, 180, 180));

      if (next_hover && ds->mouse_just_pressed && next_enabled &&
          (ds->active_design_id == block->id)) {
        block_design_goto_page(block, dd->current_page + 1);
        ui->selected_node = NULL;
      }

      // Add page button (+)
      struct nk_rect add_btn = nk_rect(next_btn.x + nav_btn_size + 8.0f, nav_y,
                                       nav_btn_size, nav_btn_size);
      int add_hover =
          (ui->mouse_x >= add_btn.x && ui->mouse_x < add_btn.x + add_btn.w &&
           ui->mouse_y >= add_btn.y && ui->mouse_y < add_btn.y + add_btn.h);

      if (add_hover)
        nk_fill_rect(canvas_buf, add_btn, 4.0f, nk_rgba(66, 133, 244, 30));
      nk_draw_text(canvas_buf, nk_rect(add_btn.x + 6, add_btn.y + 2, 16, 20),
                   "+", 1, ctx->style.font, nk_rgba(0, 0, 0, 0),
                   nk_rgb(66, 133, 244));

      if (add_hover && ds->mouse_just_pressed &&
          (ds->active_design_id == block->id)) {
        int new_idx = block_design_add_page(block);
        if (new_idx >= 0) {
          block_design_goto_page(block, new_idx);
          ui->selected_node = NULL;
        }
      }
    }

    if (ds->mouse_just_pressed &&
        !nk_input_is_mouse_hovering_rect(
            &ctx->input,
            nk_rect(ws_bounds.x, ws_bounds.y + ws_bounds.h - toolbar_h,
                    ws_bounds.w, toolbar_h))) {
      ds->show_forms_menu = 0;
    }

    // Close Workspace group (Column 1)
    nk_group_end(ctx);
  }

  // --- Column 2: Sidebar (only in fullscreen mode) ---
  if (is_fullscreen) {
    if (nk_group_begin(ctx, "design_sidebar",
                       NK_WINDOW_BORDER | NK_WINDOW_SCROLL_AUTO_HIDE)) {
      if (ui->sidebar_mode == 1) {
        ui_render_sidebar_shape_selection(ui, ctx);
      } else {
        // Properties section
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Properties", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);
        ui_render_sidebar_properties(ui, engine, ctx, block);

        // Spacing between sections
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_spacing(ctx, 1);

        // Layers section
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "Layers", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 2, 1);
        nk_spacing(ctx, 1);
        ui_render_sidebar_layers(ui, ctx, block);
      }
      nk_group_end(ctx);
    }
  }
}
