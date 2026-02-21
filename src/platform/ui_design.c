#include "ui_design.h"
#include "../document/aslan_table.h"
#include "../document/document.h"
#include "ui_canvas.h"
#include "ui_shared.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External: shape-masked image texture (defined in ui_canvas.c)
SDL_Texture *get_masked_image_texture(UIHost *ui, const char *path, Node *node);
int node_get_path_polygon(Node *node, float *out_pts, int max_pts, float sx,
                          float sy, float zoom);
int open_image_file_dialog(char *out_path, size_t path_size);
int copy_image_to_assets(const char *src_path, const char *doc_dir,
                         char *out_rel_path, size_t out_size);
void draw_alignment_guides(struct nk_command_buffer *canvas_buf, Node *node,
                           float area_cx, float area_cy, float area_x,
                           float area_y, float area_w, float area_h,
                           float node_sx, float node_sy, float node_sw,
                           float node_sh);
void render_node_context_menu(UIHost *ui, struct nk_context *ctx,
                              struct nk_command_buffer *canvas_buf,
                              Block *block);

// =============================================================================
// DESIGN SIDEBAR — Properties | Layers | Page
// =============================================================================

// Returns true if any text node in the tree has a linked column set
static bool design_has_linked_nodes(Node *node) {
  if (!node)
    return false;
  if (node->type == NODE_TYPE_TEXT && node->linked_col_name[0])
    return true;
  for (Node *c = node->first_child; c; c = c->next_sibling)
    if (design_has_linked_nodes(c))
      return true;
  return false;
}

// ── Section header: gap + tinted label + hairline ───────────────────────────
static void ds_section(struct nk_context *ctx, const char *label) {
  nk_layout_row_dynamic(ctx, 8, 1);
  nk_spacing(ctx, 1);
  nk_layout_row_dynamic(ctx, 18, 1);
  struct nk_command_buffer *cb = nk_window_get_canvas(ctx);
  struct nk_rect lb = nk_widget_bounds(ctx);
  nk_fill_rect(cb, nk_rect(lb.x - 6, lb.y, lb.w + 12, lb.h), 0,
               nk_rgba(0, 0, 0, 10));
  nk_label_colored(ctx, label, NK_TEXT_LEFT, nk_rgb(95, 95, 115));
  nk_layout_row_dynamic(ctx, 1, 1);
  {
    struct nk_rect sr = nk_widget_bounds(ctx);
    nk_fill_rect(nk_window_get_canvas(ctx),
                 nk_rect(sr.x - 6, sr.y, sr.w + 12, 1), 0,
                 nk_rgba(0, 0, 0, 18));
    nk_spacing(ctx, 1);
  }
  nk_layout_row_dynamic(ctx, 4, 1);
  nk_spacing(ctx, 1);
}

// ── Compact color row: [label ........ ■ swatch_combo] ──────────────────────
static void ds_color(struct nk_context *ctx, const char *label, uint32_t *c32) {
  nk_layout_row_template_begin(ctx, 26);
  nk_layout_row_template_push_dynamic(ctx);
  nk_layout_row_template_push_static(ctx, 60);
  nk_layout_row_template_end(ctx);
  nk_label(ctx, label, NK_TEXT_LEFT);
  struct nk_color c = nk_rgba((*c32 >> 24) & 0xFF, (*c32 >> 16) & 0xFF,
                              (*c32 >> 8) & 0xFF, *c32 & 0xFF);
  if (nk_combo_begin_color(ctx, c, nk_vec2(220, 380))) {
    struct nk_colorf cf = nk_color_cf(c);
    cf = ui_render_color_picker_content(ctx, cf);
    nk_combo_end(ctx);
    c = nk_rgba((int)(cf.r * 255), (int)(cf.g * 255), (int)(cf.b * 255),
                (int)(cf.a * 255));
    *c32 = ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) |
           ((uint32_t)c.b << 8) | c.a;
  }
}

// ── Main sidebar ─────────────────────────────────────────────────────────────
static void design_render_sidebar(UIHost *ui, Engine *engine,
                                  struct nk_context *ctx, Block *block) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;
  Page *page = doc_page_get_active(engine->doc);

  // ── Tab bar: Properties | Layers | Page ─────────────────────────────────
  nk_layout_row_dynamic(ctx, 30, 3);
  static const char *tab_labels[] = {"Properties", "Layers", "Page"};
  for (int t = 0; t < 3; t++) {
    struct nk_style_button ts = ctx->style.button;
    if (ui->design_bottom_tab == t) {
      ts.normal = nk_style_item_color(nk_rgb(55, 120, 235));
      ts.hover = nk_style_item_color(nk_rgb(45, 100, 215));
      ts.active = nk_style_item_color(nk_rgb(35, 80, 195));
      ts.text_normal = ts.text_hover = ts.text_active = nk_rgb(255, 255, 255);
    } else {
      ts.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      ts.hover = nk_style_item_color(nk_rgba(0, 0, 0, 10));
      ts.active = nk_style_item_color(nk_rgba(0, 0, 0, 20));
      ts.text_normal = nk_rgb(110, 110, 130);
      ts.text_hover = nk_rgb(60, 60, 80);
      ts.text_active = nk_rgb(40, 40, 60);
      ts.border = 0;
    }
    if (nk_button_label_styled(ctx, &ts, tab_labels[t]))
      ui->design_bottom_tab = t;
  }

  // Thin hairline separator
  nk_layout_row_dynamic(ctx, 1, 1);
  {
    struct nk_rect sr = nk_widget_bounds(ctx);
    nk_fill_rect(nk_window_get_canvas(ctx),
                 nk_rect(sr.x - 6, sr.y, sr.w + 12, 1), 0,
                 nk_rgba(0, 0, 0, 25));
    nk_spacing(ctx, 1);
  }

  // Scrollable content: fill remaining height in the sidebar group
  float sb_avail = nk_window_get_content_region(ctx).h - 36.0f;
  if (sb_avail < 50.0f)
    sb_avail = 50.0f;
  nk_layout_row_dynamic(ctx, sb_avail, 1);

  // =========================================================================
  // TAB 0 — PROPERTIES
  // =========================================================================
  if (ui->design_bottom_tab == 0) {
    if (nk_group_begin(ctx, "ds_props", NK_WINDOW_BORDER)) {
      if (!ui->selected_node) {
        nk_layout_row_dynamic(ctx, 50, 1);
        nk_label_colored(ctx, "No element selected", NK_TEXT_CENTERED,
                         nk_rgb(160, 160, 175));
        nk_layout_row_dynamic(ctx, 18, 1);
        nk_label_colored(ctx, "Click an element to edit", NK_TEXT_CENTERED,
                         nk_rgb(195, 195, 210));
      } else {
        Node *node = ui->selected_node;

        // ── TRANSFORM ────────────────────────────────────────────────────
        ds_section(ctx, "TRANSFORM");
        nk_layout_row_dynamic(ctx, 24, 2);
        nk_property_float(ctx, "X:", -10000.f, &node->x, 10000.f, 1.f, 0.5f);
        nk_property_float(ctx, "Y:", -10000.f, &node->y, 10000.f, 1.f, 0.5f);
        nk_layout_row_dynamic(ctx, 24, 2);
        nk_property_float(ctx, "W:", 1.f, &node->width, 10000.f, 1.f, 0.5f);
        nk_property_float(ctx, "H:", 1.f, &node->height, 10000.f, 1.f, 0.5f);
        nk_layout_row_dynamic(ctx, 24, 1);
        nk_property_float(ctx, "Rotation:", -360.f, &node->rotation, 360.f, 1.f,
                          0.5f);

        // ── APPEARANCE ───────────────────────────────────────────────────
        ds_section(ctx, "APPEARANCE");
        ds_color(ctx, "Fill", &node->fill_color);
        ds_color(ctx, "Stroke", &node->stroke_color);
        nk_layout_row_dynamic(ctx, 24, 1);
        nk_property_float(ctx, "Stroke width:", 0.f, &node->stroke_width, 100.f,
                          0.5f, 0.25f);
        if (node->type == NODE_TYPE_RECT || node->type == NODE_TYPE_IMAGE ||
            node->type == NODE_TYPE_FRAME) {
          nk_layout_row_dynamic(ctx, 24, 1);
          nk_property_float(ctx, "Corner radius:", 0.f, &node->corner_radius,
                            200.f, 1.f, 0.5f);
        }

        // ── TEXT ─────────────────────────────────────────────────────────
        if (node->type == NODE_TYPE_TEXT) {
          ds_section(ctx, "TEXT");
          if (node->font_size < 8.0f)
            node->font_size = 12.0f;
          nk_layout_row_dynamic(ctx, 24, 1);
          nk_property_float(ctx, "Font size:", 8.f, &node->font_size, 200.f,
                            1.f, 0.5f);

          // Link to column
          Block *tbl_block = NULL;
          for (Block *b = page ? page->first_block : NULL; b; b = b->next) {
            if (b->type == BLOCK_TABLE && b->data.table.columnar) {
              tbl_block = b;
              break;
            }
          }
          if (tbl_block) {
            ds_section(ctx, "LINKED TEXT");
            AslanTable *tbl = tbl_block->data.table.columnar;
            int col_count = (int)tbl->column_count;
            const char *col_names[33];
            col_names[0] = "None";
            for (int k = 0; k < col_count && k < 32; k++)
              col_names[k + 1] = tbl->columns[k].name;
            int cur_sel = 0;
            for (int k = 0; k < col_count; k++) {
              if (strcmp(node->linked_col_name, tbl->columns[k].name) == 0) {
                cur_sel = k + 1;
                break;
              }
            }
            nk_layout_row_dynamic(ctx, 24, 1);
            int new_sel = nk_combo(ctx, col_names, col_count + 1, cur_sel, 24,
                                   nk_vec2(220, 200));
            if (new_sel != cur_sel) {
              if (new_sel == 0) {
                node->linked_col_name[0] = '\0';
              } else {
                strncpy(node->linked_col_name, col_names[new_sel],
                        sizeof(node->linked_col_name) - 1);
                node->linked_col_name[sizeof(node->linked_col_name) - 1] = '\0';
                ui->design_linked_row_idx = 0;
              }
            }
            if (node->linked_col_name[0]) {
              int total_rows = (int)tbl->active_count;
              if (ui->design_linked_row_idx >= total_rows && total_rows > 0)
                ui->design_linked_row_idx = total_rows - 1;
              nk_layout_row_template_begin(ctx, 26);
              nk_layout_row_template_push_static(ctx, 26);
              nk_layout_row_template_push_dynamic(ctx);
              nk_layout_row_template_push_static(ctx, 26);
              nk_layout_row_template_end(ctx);
              if (nk_button_label(ctx, "<") && ui->design_linked_row_idx > 0)
                ui->design_linked_row_idx--;
              char rl[32];
              snprintf(rl, sizeof(rl), "Row %d / %d",
                       ui->design_linked_row_idx + 1, total_rows);
              nk_label(ctx, rl, NK_TEXT_CENTERED);
              if (nk_button_label(ctx, ">") &&
                  ui->design_linked_row_idx < total_rows - 1)
                ui->design_linked_row_idx++;
            }
          }
        }
      }
      nk_group_end(ctx);
    }
  }

  // =========================================================================
  // TAB 1 — LAYERS
  // =========================================================================
  if (ui->design_bottom_tab == 1) {
    if (nk_group_begin(ctx, "ds_layers", NK_WINDOW_BORDER)) {
      ui_render_sidebar_layers(ui, ctx, block);
      nk_group_end(ctx);
    }
  }

  // =========================================================================
  // TAB 2 — PAGE
  // =========================================================================
  if (ui->design_bottom_tab == 2) {
    if (nk_group_begin(ctx, "ds_page", NK_WINDOW_BORDER)) {
      if (page) {
        // ── CANVAS SIZE ──────────────────────────────────────────────────
        ds_section(ctx, "CANVAS SIZE");
        static const struct {
          const char *l;
          int w, h;
        } presets[] = {
            {"A4", 794, 1123},    {"A3", 1123, 1587},    {"HD", 1920, 1080},
            {"4K", 3840, 2160},   {"Sq.", 1080, 1080},   {"Story", 1080, 1920},
            {"Post", 1080, 1350}, {"Banner", 1920, 480},
        };
        nk_layout_row_dynamic(ctx, 26, 4);
        for (int i = 0; i < 8; i++) {
          if (nk_button_label(ctx, presets[i].l)) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "block_design_resize %u %u %d %d 0",
                     page->id, block->id, presets[i].w, presets[i].h);
            engine_execute_command(engine, cmd);
          }
        }
        nk_layout_row_dynamic(ctx, 24, 2);
        float val_w = (float)dd->width;
        float val_h = (float)dd->height;
        nk_property_float(ctx, "W:", 100.f, &val_w, 5000.f, 1.f, 1.f);
        nk_property_float(ctx, "H:", 100.f, &val_h, 5000.f, 1.f, 1.f);
        if ((int)val_w != dd->width || (int)val_h != dd->height) {
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_design_resize %u %u %d %d 0",
                   page->id, block->id, (int)val_w, (int)val_h);
          engine_execute_command(engine, cmd);
        }

        // ── BACKGROUND ───────────────────────────────────────────────────
        ds_section(ctx, "BACKGROUND");
        ds_color(ctx, "Color", &dd->pages[dd->current_page].bg_color);

        // ── MODE ─────────────────────────────────────────────────────────
        ds_section(ctx, "MODE");
        nk_layout_row_dynamic(ctx, 24, 1);
        int no_limits = dd->no_limits;
        if (nk_checkbox_label(ctx, "Infinite canvas", &no_limits) &&
            no_limits != dd->no_limits) {
          dd->no_limits = no_limits;
          if (dd->no_limits && dd->zoom < 0.01f)
            dd->zoom = 1.0f;
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "block_design_nolimits %u %u %d", page->id,
                   block->id, dd->no_limits);
          engine_execute_command(engine, cmd);
        }

        // ── PAGES ────────────────────────────────────────────────────────
        ds_section(ctx, "PAGES");
        nk_layout_row_template_begin(ctx, 28);
        nk_layout_row_template_push_static(ctx, 28); // <
        nk_layout_row_template_push_dynamic(ctx);    // counter
        nk_layout_row_template_push_static(ctx, 28); // >
        nk_layout_row_template_push_static(ctx, 28); // +
        nk_layout_row_template_end(ctx);

        int prev_ok = (dd->current_page > 0);
        int next_ok = (dd->current_page < dd->page_count - 1);
        struct nk_style_button arr = ctx->style.button;
        arr.padding = nk_vec2(2, 2);

        struct nk_style_button arr_prev = arr;
        if (!prev_ok)
          arr_prev.text_normal = nk_rgb(200, 200, 215);
        if (nk_button_label_styled(ctx, &arr_prev, "<") && prev_ok) {
          block_design_goto_page(block, dd->current_page - 1);
          ui->selected_node = NULL;
        }
        char pg_str[32];
        snprintf(pg_str, sizeof(pg_str), "%d / %d", dd->current_page + 1,
                 dd->page_count);
        nk_label(ctx, pg_str, NK_TEXT_CENTERED);
        struct nk_style_button arr_next = arr;
        if (!next_ok)
          arr_next.text_normal = nk_rgb(200, 200, 215);
        if (nk_button_label_styled(ctx, &arr_next, ">") && next_ok) {
          block_design_goto_page(block, dd->current_page + 1);
          ui->selected_node = NULL;
        }
        struct nk_style_button add_btn = arr;
        add_btn.text_normal = nk_rgb(55, 120, 235);
        if (nk_button_label_styled(ctx, &add_btn, "+")) {
          int new_pg = block_design_add_page(block);
          if (new_pg >= 0) {
            block_design_goto_page(block, new_pg);
            ui->selected_node = NULL;
          }
        }

        if (dd->page_count > 1) {
          nk_layout_row_dynamic(ctx, 24, 1);
          struct nk_style_button del_btn = ctx->style.button;
          del_btn.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
          del_btn.hover = nk_style_item_color(nk_rgba(220, 50, 50, 18));
          del_btn.active = nk_style_item_color(nk_rgba(220, 50, 50, 35));
          del_btn.border = 0;
          del_btn.text_normal = nk_rgb(200, 65, 65);
          del_btn.text_hover = nk_rgb(220, 35, 35);
          del_btn.text_active = nk_rgb(180, 25, 25);
          if (nk_button_label_styled(ctx, &del_btn, "Delete current page")) {
            int cur = dd->current_page;
            block_design_delete_page(block, cur);
            block_design_goto_page(block, cur > 0 ? cur - 1 : 0);
            ui->selected_node = NULL;
          }
        }

        // ── LINKED DATA ──────────────────────────────────────────────────
        if (dd->root && design_has_linked_nodes(dd->root)) {
          ds_section(ctx, "LINKED DATA");
          int total_rows = 0;
          for (Block *b = page->first_block; b; b = b->next) {
            if (b->type == BLOCK_TABLE && b->data.table.columnar) {
              total_rows = (int)b->data.table.columnar->active_count;
              break;
            }
          }
          if (total_rows > 0) {
            if (ui->design_linked_row_idx >= total_rows)
              ui->design_linked_row_idx = total_rows - 1;
            nk_layout_row_template_begin(ctx, 26);
            nk_layout_row_template_push_static(ctx, 26);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_push_static(ctx, 26);
            nk_layout_row_template_end(ctx);
            if (nk_button_label(ctx, "<") && ui->design_linked_row_idx > 0)
              ui->design_linked_row_idx--;
            char rl[32];
            snprintf(rl, sizeof(rl), "Row %d / %d",
                     ui->design_linked_row_idx + 1, total_rows);
            nk_label(ctx, rl, NK_TEXT_CENTERED);
            if (nk_button_label(ctx, ">") &&
                ui->design_linked_row_idx < total_rows - 1)
              ui->design_linked_row_idx++;
          }
        }
      }
      nk_group_end(ctx);
    }
  }
}

void ui_render_block_design(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width) {
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

  // Ensure background is not transparent (0) - default to White
  if (dd->bg_color == 0)
    dd->bg_color = 0xFFFFFFFF;
  if (dd->pages[dd->current_page].bg_color == 0)
    dd->pages[dd->current_page].bg_color = 0xFFFFFFFF;

  // Ensure root points to current page
  if (dd->current_page >= 0 && dd->current_page < dd->page_count) {
    dd->root = dd->pages[dd->current_page].root;
  }

  ui_render_expand_toggle(ui, ctx, block);

  int is_fullscreen = (ui->expanded_block_id == block->id);

  // Initialize zoom for no_limits mode
  if (dd->no_limits && dd->zoom < 0.01f)
    dd->zoom = 1.0f;

  // --- Layout height calculation ---
  float toolbar_h = 52.0f;
  float sidebar_w = 260.0f;

  float row_h = nk_window_get_content_region(ctx).h;
  if (row_h < 200.0f)
    row_h = 200.0f;

  float display_h = row_h - toolbar_h;
  if (display_h < 100.0f)
    display_h = 100.0f;

  // Workspace column (dynamic width) + sidebar column (fixed width)
  nk_layout_row_template_begin(ctx, row_h);
  nk_layout_row_template_push_dynamic(ctx);
  nk_layout_row_template_push_static(ctx, sidebar_w);
  nk_layout_row_template_end(ctx);

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

    if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                        ws_bounds)) {
      ui->active_block_id = block->id;
    }

    // Pre-calculate toolbar flags for interaction logic (8 buttons in design)
    float toolbar_w = 8 * 36.0f + 7 * 8.0f + 24.0f;
    float toolbar_x = ws_bounds.x + (ws_bounds.w - toolbar_w) / 2.0f;
    float toolbar_y = ws_bounds.y + ws_bounds.h - 52.0f - 20.0f; // toolbar_h=52

    int in_toolbar =
        (ui->mouse_x >= toolbar_x && ui->mouse_x < toolbar_x + toolbar_w &&
         ui->mouse_y >= toolbar_y && ui->mouse_y < toolbar_y + 52.0f);
    int in_forms_menu = 0;

    // Early check for active state
    int is_active_design = (ds->active_design_id == block->id);

    // Hardened Input Check: Sync Nuklear input state (only if inside workspace)
    if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
        !ui->block_drag.is_active && ui->mouse_x >= ws_bounds.x &&
        ui->mouse_x < ws_bounds.x + ws_bounds.w && ui->mouse_y >= ws_bounds.y &&
        ui->mouse_y < ws_bounds.y + ws_bounds.h) {
      ds->mouse_just_pressed = 1;
    }

    float scale = 1.0f;

    if (dd->no_limits) {
      // --- Infinite Canvas Mode ---
      float zoom = dd->zoom > 0.01f ? dd->zoom : 1.0f;
      float cam_x = dd->view_x, cam_y = dd->view_y;

      // Sync design_state camera
      if (ds->active_design_id != block->id) {
        ds->cam_x = cam_x;
        ds->cam_y = cam_y;
        ds->zoom = zoom;
      } else {
        cam_x = ds->cam_x;
        cam_y = ds->cam_y;
        zoom = ds->zoom > 0.01f ? ds->zoom : 1.0f;
      }
      scale = zoom;

      // Background: use page bg color + dot grid
      struct nk_color bg_col =
          nk_rgba((dd->pages[dd->current_page].bg_color >> 24) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 16) & 0xFF,
                  (dd->pages[dd->current_page].bg_color >> 8) & 0xFF,
                  dd->pages[dd->current_page].bg_color & 0xFF);
      nk_fill_rect(canvas_buf, ws_bounds, 0, bg_col);
      canvas_draw_dot_grid(canvas_buf, ws_bounds, cam_x, cam_y, zoom);
      nk_push_scissor(canvas_buf, ws_bounds);

      // Use full workspace as coordinate space
      nk_layout_space_begin(ctx, NK_STATIC, ws_bounds.h, 1);
      nk_layout_space_push(ctx, nk_rect(0, 0, ws_bounds.w, ws_bounds.h));
    } else {
      // --- Fixed Artboard Mode ---
      nk_fill_rect(canvas_buf, ws_bounds, 0, nk_rgb(241, 241, 239));

      float margin = 20.0f;
      float avail_w = ws_bounds.w - margin * 2;
      float avail_h = display_h - margin * 2;

      float scale_w = avail_w / (float)dd->width;
      float scale_h = avail_h / (float)dd->height;
      scale = (scale_w < scale_h) ? scale_w : scale_h;
      if (scale > 1.0f)
        scale = 1.0f;

      float art_w = (float)dd->width * scale;
      float art_h = (float)dd->height * scale;

      float sx = ws_bounds.x + (ws_bounds.w - art_w) / 2.0f;
      float sy = ws_bounds.y + (display_h - art_h) / 2.0f;

      struct nk_rect shadow_rect = nk_rect(sx + 2, sy + 2, art_w, art_h);
      nk_fill_rect(canvas_buf, shadow_rect, 2, nk_rgba(0, 0, 0, 40));

      struct nk_rect art_rect = nk_rect(sx, sy, art_w, art_h);
      nk_fill_rect(canvas_buf, art_rect, 0,
                   nk_rgba((dd->pages[dd->current_page].bg_color >> 24) & 0xFF,
                           (dd->pages[dd->current_page].bg_color >> 16) & 0xFF,
                           (dd->pages[dd->current_page].bg_color >> 8) & 0xFF,
                           dd->pages[dd->current_page].bg_color & 0xFF));
      nk_stroke_rect(canvas_buf, art_rect, 0, 1.0f, nk_rgb(200, 200, 200));

      nk_layout_space_begin(ctx, NK_STATIC, ws_bounds.h, 1);
      nk_layout_space_push(
          ctx, nk_rect(sx - ws_bounds.x, sy - ws_bounds.y, art_w, art_h));
    }

    // --- Shared content rendering (both modes) ---
    if (nk_group_begin(ctx, "design_content", NK_WINDOW_NO_SCROLLBAR)) {
      struct nk_rect bounds_raw = nk_window_get_content_region(ctx);
      struct nk_rect bounds;

      if (dd->no_limits) {
        // Offset bounds so existing code (bounds.x + node.x * scale) works
        float cam_x = ds->cam_x, cam_y = ds->cam_y;
        float zoom = ds->zoom > 0.01f ? ds->zoom : 1.0f;
        bounds =
            nk_rect(bounds_raw.x - cam_x * zoom, bounds_raw.y - cam_y * zoom,
                    bounds_raw.w, bounds_raw.h);
      } else {
        bounds = bounds_raw;
      }

      ds->bounds_x = bounds_raw.x;
      ds->bounds_y = bounds_raw.y;
      ds->bounds_w = bounds_raw.w;
      ds->bounds_h = bounds_raw.h;
      ds->scale = scale;

      // Track if this design is active
      // Track if this design is active
      is_active_design = (ds->active_design_id == block->id);

      // Auto-activate if clicked inside WORKSPACE (including toolbar)
      if (!is_active_design && !ui->block_drag.is_active &&
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
        } else if (ds->pending_create == 7) { // Linked text
          new_node = design_create_styled_node(
              dd, NODE_STYLE_TEXT, nx, ny, 200.0f, 40.0f, ds->selected_color);
          if (new_node)
            strncpy(new_node->text, "Linked text", sizeof(new_node->text) - 1);
          // linked_col_name starts empty; user links it via Properties sidebar
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

      // Empty state hint
      if (dd->root && !dd->root->first_child) {
        const char *hint = "Click a tool to start";
        float hint_w =
            ctx->style.font->width(ctx->style.font->userdata,
                                   ctx->style.font->height, hint, strlen(hint));
        nk_draw_text(canvas_buf,
                     nk_rect(bounds.x + (bounds.w - hint_w) / 2.0f,
                             bounds.y + bounds.h / 2.0f - 10.0f, hint_w, 20),
                     hint, strlen(hint), ctx->style.font, nk_rgba(0, 0, 0, 0),
                     nk_rgb(200, 200, 200));
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

          // Container with image: render shape-masked image
          if (node->is_container && node->image_path[0]) {
            SDL_Texture *tex =
                get_masked_image_texture(ui, node->image_path, node);
            if (tex) {
              struct nk_image img = nk_image_ptr(tex);
              nk_draw_image(canvas_buf, nr, &img, nk_rgb(255, 255, 255));
              drawn_image = 1;
            }
          }

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
            if (node->is_container && !node->image_path[0]) {
              // Empty container placeholder for PATH shape
              struct nk_color cbg = nk_rgba(66, 133, 244, 40);
              struct nk_color cst = nk_rgba(66, 133, 244, 255);
              float poly_pts[512 * 2];
              int poly_count =
                  node_get_path_polygon(node, poly_pts, 512, sx, sy, scale);
              if (poly_count > 2) {
                nk_fill_polygon(canvas_buf, poly_pts, poly_count, cbg);
                nk_stroke_polygon(canvas_buf, poly_pts, poly_count, 1.0f, cst);
              }
              if (ui->font_icons) {
                struct nk_rect ir =
                    nk_rect(nr.x + nr.w / 2 - 10, nr.y + nr.h / 2 - 10, 20, 20);
                nk_draw_text(canvas_buf, ir, ICON_ADD, strlen(ICON_ADD),
                             &ui->font_icons->handle, nk_rgba(0, 0, 0, 0), cst);
              }
              drawn_image = 1;
            }

            // Fill path
            if (!drawn_image && fill.a > 0) {
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
            static char linked_val_buf[256];
            int is_editing = (ui->editing_node == node);
            // Substitute linked column value when not editing
            if (!is_editing && node->linked_col_name[0]) {
              Page *lpage = doc_page_get_active(engine->doc);
              for (Block *b = lpage ? lpage->first_block : NULL; b;
                   b = b->next) {
                if (b->type == BLOCK_TABLE && b->data.table.columnar) {
                  AslanTable *tbl = b->data.table.columnar;
                  for (int lk = 0; lk < (int)tbl->column_count; lk++) {
                    if (strcmp(tbl->columns[lk].name, node->linked_col_name) ==
                        0) {
                      int ridx = ui->design_linked_row_idx;
                      if (ridx < (int)tbl->active_count) {
                        uint64_t rid = aslan_table_view_row_id(tbl, ridx);
                        const char *val = aslan_table_get_cell_display(
                            tbl, lk, rid, linked_val_buf,
                            sizeof(linked_val_buf));
                        if (val && val[0])
                          display_text = val;
                      }
                      break;
                    }
                  }
                  break;
                }
              }
            }
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
              if (node->font_type == 4 && ui->font_inter) { // Mono -> Inter
                font = &ui->font_inter->handle;
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
      int mouse_in_design = (ui->mouse_x >= bounds_raw.x &&
                             ui->mouse_x < bounds_raw.x + bounds_raw.w &&
                             ui->mouse_y >= bounds_raw.y &&
                             ui->mouse_y < bounds_raw.y + bounds_raw.h);

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
          } else if (hit->is_container && !hit->image_path[0]) {
            // Click on empty container "+" -> open image file dialog
            char path[512] = {0};
            if (open_image_file_dialog(path, sizeof(path))) {
              char rel[256] = {0};
              if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
                strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
                hit->image_path[sizeof(hit->image_path) - 1] = '\0';
              }
            }
            ui->selected_node = hit;
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
            SDL_StartTextInput(); // keep text input alive for Nuklear edit
                                  // widgets
          }
          // Start panning in no_limits mode when clicking background
          if (dd->no_limits && ds->tool != CANVAS_TOOL_PEN &&
              !ui->block_drag.is_active) {
            ds->is_panning = 1;
            ds->pan_start_x = ui->mouse_x;
            ds->pan_start_y = ui->mouse_y;
            ds->cam_start_x = ds->cam_x;
            ds->cam_start_y = ds->cam_y;
          }
        }
      }

      // Handle node dragging
      if (ds->is_dragging_node && ui->selected_node && is_active_design) {
        float design_x = (ui->mouse_x - bounds.x) / scale;
        float design_y = (ui->mouse_y - bounds.y) / scale;
        ui->selected_node->x = design_x - ds->drag_offset_x;
        ui->selected_node->y = design_y - ds->drag_offset_y;

        // Clamp to design bounds (only in artboard mode)
        if (!dd->no_limits) {
          ui->selected_node->x =
              fmaxf(0, fminf(ui->selected_node->x,
                             dd->width - ui->selected_node->width));
          ui->selected_node->y =
              fmaxf(0, fminf(ui->selected_node->y,
                             dd->height - ui->selected_node->height));
        }

        if (ds->mouse_just_released) {
          ds->is_dragging_node = 0;
        }
      }

      // --- Right-click context menu detection ---
      if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_RIGHT) &&
          mouse_in_design && !in_toolbar && is_active_design) {
        float dx = (ui->mouse_x - bounds.x) / scale;
        float dy = (ui->mouse_y - bounds.y) / scale;
        Node *hit = design_find_node_at(dd, dx, dy);
        if (hit) {
          ui->show_context_menu = 1;
          ui->context_menu_x = ui->mouse_x;
          ui->context_menu_y = ui->mouse_y;
          ui->context_menu_node = hit;
          ui->selected_node = hit;
        } else {
          ui->show_context_menu = 0;
        }
      }

      // Close forms menu if clicking elsewhere
      if (ds->mouse_just_pressed && !in_toolbar && !in_forms_menu &&
          is_active_design) {
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

      // --- Alignment guides when dragging ---
      if (ds->is_dragging_node && ui->selected_node) {
        Node *sel = ui->selected_node;
        float nsx = bounds.x + sel->x * scale;
        float nsy = bounds.y + sel->y * scale;
        float nsw = sel->width * scale;
        float nsh = sel->height * scale;
        draw_alignment_guides(canvas_buf, sel, bounds.x + bounds.w / 2.0f,
                              bounds.y + bounds.h / 2.0f, bounds.x, bounds.y,
                              bounds.w, bounds.h, nsx, nsy, nsw, nsh);
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

    if (ds->mouse_just_pressed &&
        !nk_input_is_mouse_hovering_rect(
            &ctx->input,
            nk_rect(ws_bounds.x, ws_bounds.y + ws_bounds.h - toolbar_h,
                    ws_bounds.w, toolbar_h))) {
    }

    // --- Zoom controls for no_limits mode ---
    if (dd->no_limits) {
      struct nk_command_buffer *zcb = nk_window_get_canvas(ctx);

      // Zoom with scroll wheel
      if (ui->mouse_x >= ws_bounds.x &&
          ui->mouse_x < ws_bounds.x + ws_bounds.w &&
          ui->mouse_y >= ws_bounds.y &&
          ui->mouse_y < ws_bounds.y + ws_bounds.h) {
        float scroll = ctx->input.mouse.scroll_delta.y;
        if (scroll != 0.0f) {
          float old_zoom = ds->zoom;
          if (scroll > 0)
            ds->zoom *= 1.15f;
          else
            ds->zoom *= 0.87f;
          if (ds->zoom < 0.1f)
            ds->zoom = 0.1f;
          if (ds->zoom > 5.0f)
            ds->zoom = 5.0f;
          float cx = ws_bounds.w / 2.0f, cy = ws_bounds.h / 2.0f;
          ds->cam_x += cx / old_zoom - cx / ds->zoom;
          ds->cam_y += cy / old_zoom - cy / ds->zoom;
        }
      }

      // Zoom percentage display
      char zoom_str[16];
      snprintf(zoom_str, sizeof(zoom_str), "%d%%", (int)(ds->zoom * 100));
      float zx = ws_bounds.x + ws_bounds.w - 60.0f;
      float zy = ws_bounds.y + ws_bounds.h - 30.0f;
      nk_draw_text(zcb, nk_rect(zx, zy, 50, 20), zoom_str, strlen(zoom_str),
                   ctx->style.font, nk_rgba(0, 0, 0, 0), nk_rgb(120, 120, 120));

      // Save camera state back to DesignData
      dd->view_x = ds->cam_x;
      dd->view_y = ds->cam_y;
      dd->zoom = ds->zoom;
    }

    // --- Right-click context menu ---
    render_node_context_menu(ui, ctx, nk_window_get_canvas(ctx), block);

    // Close Workspace group (Column 1)
    nk_group_end(ctx);
  } // design_content_group

  // --- Right sidebar: Properties | Layers | Page ---
  if (nk_group_begin(ctx, "design_sidebar",
                     NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
    design_render_sidebar(ui, engine, ctx, block);
    nk_group_end(ctx);
  }
}
