import re

with open('src/platform/ui_design.c', 'r') as f:
    lines = f.readlines()

# Find start of loop
start_idx = -1
for i, l in enumerate(lines):
    if "for (Node *node = dd->root->first_child; node;" in l:
        start_idx = i
        break

# Find end of loop by counting braces
brace_count = 0
end_idx = -1
for i in range(start_idx, len(lines)):
    brace_count += lines[i].count('{')
    brace_count -= lines[i].count('}')
    if brace_count == 0 and lines[i].strip() == '}':
        end_idx = i
        break

print(f"Loop is from {start_idx} to {end_idx}")

# The loop body is from start_idx + 1 to end_idx - 1
loop_body = "".join(lines[start_idx+1:end_idx])

# We need to transform the loop body inside a new function
new_func_sig = """
static void ui_render_node_tree(UIHost *ui, struct nk_context *ctx, Engine *engine, Block *block, DesignData *dd, struct nk_command_buffer *canvas_buf, Node *node, struct nk_rect bounds, float scale, float parent_x, float parent_y) {
  if (!node || !node->visible) return;

  // Auto-layout
  if (node->is_container && node->layout_type > 0 && node->first_child) {
    float current_offset = 0;
    for (Node *child = node->first_child; child; child = child->next_sibling) {
      if (!child->visible) continue;
      if (node->layout_type == 1) { // V-Stack
        child->x = 0;
        child->y = current_offset;
        current_offset += child->height + node->layout_padding;
      } else if (node->layout_type == 2) { // H-Stack
        child->x = current_offset;
        child->y = 0;
        current_offset += child->width + node->layout_padding;
      }
    }
  }

  float sx = parent_x + node->x * scale;
  float sy = parent_y + node->y * scale;
  float sw = node->width * scale;
  float sh = node->height * scale;
  struct nk_rect nr = nk_rect(sx, sy, sw, sh);

  if (node->is_container && node->clip_content) {
    nk_push_scissor(canvas_buf, nr);
  }

  // --- Inject Shadow ---
  uint32_t f32 = node->fill_color;
  uint32_t s32 = node->stroke_color;
  float opacity = node->opacity;
  if (node->has_shadow && node->type != NODE_TYPE_LINE && node->type != NODE_TYPE_STROKE && node->type != NODE_TYPE_CHART) {
    struct nk_color sh_col = nk_rgba((node->shadow_color >> 24) & 0xFF, (node->shadow_color >> 16) & 0xFF, (node->shadow_color >> 8) & 0xFF, (int)((node->shadow_color & 0xFF) * opacity));
    float sh_x = nr.x + node->shadow_offset_x * scale;
    float sh_y = nr.y + node->shadow_offset_y * scale;
    struct nk_rect sr = nk_rect(sh_x, sh_y, nr.w, nr.h);
    float b = node->shadow_blur * scale;
    if (b > 0) {
      struct nk_rect srb = nk_rect(sh_x - b/2, sh_y - b/2, nr.w + b, nr.h + b);
      nk_fill_rect(canvas_buf, srb, (node->corner_radius + b/2) * scale, nk_rgba(sh_col.r, sh_col.g, sh_col.b, sh_col.a / 2));
    }
    nk_fill_rect(canvas_buf, sr, node->corner_radius * scale, sh_col);
  }
"""

original_lines = lines[start_idx+1:end_idx]
mod_body = []
for line in original_lines:
    if "float sx = bounds.x + node->x * scale;" in line: continue
    if "float sy = bounds.y + node->y * scale;" in line: continue
    if "float sw = node->width * scale;" in line: continue
    if "float sh = node->height * scale;" in line: continue
    if "struct nk_rect nr = nk_rect(sx, sy, sw, sh);" in line: continue
    if "nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, fill);" in line:
        mod_body.append("""
            if (node->fill_type == 1) { // Gradient
              struct nk_color fill2 = nk_rgba((node->fill_color_2 >> 24) & 0xFF, (node->fill_color_2 >> 16) & 0xFF, (node->fill_color_2 >> 8) & 0xFF, (int)((node->fill_color_2 & 0xFF) * opacity));
              nk_fill_rect_multi_color(canvas_buf, nr, fill, fill, fill2, fill2); // simplistic projection
            } else {
              nk_fill_rect(canvas_buf, nr, node->corner_radius * scale, fill);
            }
""")
        continue
    # Move selection highlight to outside the recursive function so it draws on top of everything!
    if "// Selection highlight" in line:
        break # We stop adding to mod_body here
    mod_body.append(line)

new_func_end = """
  for (Node *child = node->first_child; child; child = child->next_sibling) {
    ui_render_node_tree(ui, ctx, engine, block, dd, canvas_buf, child, bounds, scale, sx, sy);
  }

  if (node->is_container && node->clip_content) {
    nk_push_scissor(canvas_buf, nk_rect(-8192, -8192, 16384, 16384));
  }
}
"""

new_func = new_func_sig + "".join(mod_body) + new_func_end

# Find where to insert new function: above ui_render_block_design
insert_idx = -1
for i, l in enumerate(lines):
    if "void ui_render_block_design" in l:
        insert_idx = i - 1
        break

# The replacement for the loop:
replacement_loop = """
        for (Node *node = dd->root->first_child; node; node = node->next_sibling) {
          ui_render_node_tree(ui, ctx, engine, block, dd, canvas_buf, node, bounds, scale, bounds.x, bounds.y);
        }

        // Selection highlight over everything
        if (ui->selected_node && is_active_design && !ds->bezier_mode) {
          Node *node = ui->selected_node;
          float sx = bounds.x + node->x * scale;
          float sy = bounds.y + node->y * scale;
          Node *p = node->parent;
          while(p && p != dd->root) {
            sx += p->x * scale;
            sy += p->y * scale;
            p = p->parent;
          }
          float sw = node->width * scale;
          float sh = node->height * scale;
          struct nk_rect nr = nk_rect(sx, sy, sw, sh);
          
          float sel_off = 2.0f + (node->stroke_width * scale) / 2.0f;
          float rad = node->rotation * 3.14159f / 180.0f;
          float cx = sx + sw / 2.0f;
          float cy = sy + sh / 2.0f;

          fill_rotated_rounded_rect(canvas_buf, cx, cy, sw + sel_off * 2, sh + sel_off * 2, rad, 0, nk_rgb(66, 133, 244), 1, 1.0f);

          float hsz = 8.0f;
          struct nk_color hc = nk_rgb(66, 133, 244);
          float c = cosf(rad);
          float s = sinf(rad);
          float h_hw = (sw + sel_off * 2) / 2.0f;
          float h_hh = (sh + sel_off * 2) / 2.0f;
          float handle_pts[4][2] = {{h_hw, h_hh}, {-h_hw, h_hh}, {-h_hw, -h_hh}, {h_hw, -h_hh}};
          for (int i = 0; i < 4; i++) {
            float hx = cx + handle_pts[i][0] * c - handle_pts[i][1] * s;
            float hy = cy + handle_pts[i][0] * s + handle_pts[i][1] * c;
            nk_fill_rect(canvas_buf, nk_rect(hx - hsz / 2, hy - hsz / 2, hsz, hsz), 2.0f, hc);
          }
        }
"""

new_lines = lines[:insert_idx] + [new_func] + lines[insert_idx:start_idx+1] + [replacement_loop] + lines[end_idx:]

with open('src/platform/ui_design.c', 'w') as f:
    f.writelines(new_lines)

print("SUCCESS")
