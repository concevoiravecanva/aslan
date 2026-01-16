#include "lua_nk.h"
#include <string.h>

// =============================================================================
// PHASE 2: EXTENDED NUKLEAR-LUA BRIDGE
// =============================================================================
// This module provides a comprehensive Lua binding for Nuklear UI, enabling
// users to build custom UIs and node behaviors entirely in Lua scripts.

static struct nk_context *g_ctx = NULL;
static SDL_Renderer *g_renderer = NULL;

void scripting_nk_set_context(struct nk_context *ctx) { g_ctx = ctx; }

void scripting_nk_set_renderer(SDL_Renderer *renderer) {
  g_renderer = renderer;
}

// =============================================================================
// WINDOW MANAGEMENT
// =============================================================================

static int l_nk_begin(lua_State *L) {
  const char *title = luaL_checkstring(L, 1);
  float x = (float)luaL_checknumber(L, 2);
  float y = (float)luaL_checknumber(L, 3);
  float w = (float)luaL_checknumber(L, 4);
  float h = (float)luaL_checknumber(L, 5);
  nk_flags flags = (nk_flags)luaL_optinteger(L, 6, 0);

  if (!g_ctx)
    return 0;
  bool open = nk_begin(g_ctx, title, nk_rect(x, y, w, h), flags);
  lua_pushboolean(L, open);
  return 1;
}

static int l_nk_end(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_end(g_ctx);
  return 0;
}

// nk.window_get_bounds() -> x, y, w, h
static int l_nk_window_get_bounds(lua_State *L) {
  if (!g_ctx)
    return 0;
  struct nk_rect r = nk_window_get_bounds(g_ctx);
  lua_pushnumber(L, r.x);
  lua_pushnumber(L, r.y);
  lua_pushnumber(L, r.w);
  lua_pushnumber(L, r.h);
  return 4;
}

// nk.window_get_content_region() -> x, y, w, h
static int l_nk_window_get_content_region(lua_State *L) {
  if (!g_ctx)
    return 0;
  struct nk_rect r = nk_window_get_content_region(g_ctx);
  lua_pushnumber(L, r.x);
  lua_pushnumber(L, r.y);
  lua_pushnumber(L, r.w);
  lua_pushnumber(L, r.h);
  return 4;
}

// nk.window_is_hovered() -> bool
static int l_nk_window_is_hovered(lua_State *L) {
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_window_is_hovered(g_ctx));
  return 1;
}

// nk.window_is_any_hovered() -> bool
static int l_nk_window_is_any_hovered(lua_State *L) {
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_window_is_any_hovered(g_ctx));
  return 1;
}

// nk.item_is_any_active() -> bool
static int l_nk_item_is_any_active(lua_State *L) {
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_item_is_any_active(g_ctx));
  return 1;
}

// =============================================================================
// LAYOUT
// =============================================================================

static int l_nk_layout_row_dynamic(lua_State *L) {
  float height = (float)luaL_checknumber(L, 1);
  int cols = (int)luaL_checkinteger(L, 2);
  if (g_ctx)
    nk_layout_row_dynamic(g_ctx, height, cols);
  return 0;
}

static int l_nk_layout_row_static(lua_State *L) {
  float height = (float)luaL_checknumber(L, 1);
  int item_width = (int)luaL_checkinteger(L, 2);
  int cols = (int)luaL_checkinteger(L, 3);
  if (g_ctx)
    nk_layout_row_static(g_ctx, height, item_width, cols);
  return 0;
}

// nk.layout_row_begin(format, height, cols)
static int l_nk_layout_row_begin(lua_State *L) {
  int format = (int)luaL_optinteger(L, 1, NK_DYNAMIC);
  float height = (float)luaL_checknumber(L, 2);
  int cols = (int)luaL_checkinteger(L, 3);
  if (g_ctx)
    nk_layout_row_begin(g_ctx, (enum nk_layout_format)format, height, cols);
  return 0;
}

// nk.layout_row_push(width)
static int l_nk_layout_row_push(lua_State *L) {
  float width = (float)luaL_checknumber(L, 1);
  if (g_ctx)
    nk_layout_row_push(g_ctx, width);
  return 0;
}

// nk.layout_row_end()
static int l_nk_layout_row_end(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_layout_row_end(g_ctx);
  return 0;
}

// nk.layout_space_begin(format, height, widget_count)
static int l_nk_layout_space_begin(lua_State *L) {
  int format = (int)luaL_optinteger(L, 1, NK_STATIC);
  float height = (float)luaL_checknumber(L, 2);
  int widget_count = (int)luaL_checkinteger(L, 3);
  if (g_ctx)
    nk_layout_space_begin(g_ctx, (enum nk_layout_format)format, height,
                          widget_count);
  return 0;
}

// nk.layout_space_push(x, y, w, h)
static int l_nk_layout_space_push(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);
  if (g_ctx)
    nk_layout_space_push(g_ctx, nk_rect(x, y, w, h));
  return 0;
}

// nk.layout_space_end()
static int l_nk_layout_space_end(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_layout_space_end(g_ctx);
  return 0;
}

// nk.spacing(cols)
static int l_nk_spacing(lua_State *L) {
  int cols = (int)luaL_checkinteger(L, 1);
  if (g_ctx)
    nk_spacing(g_ctx, cols);
  return 0;
}

// =============================================================================
// GROUPS & TREES
// =============================================================================

// nk.group_begin(title, flags) -> bool
static int l_nk_group_begin(lua_State *L) {
  const char *title = luaL_checkstring(L, 1);
  nk_flags flags = (nk_flags)luaL_optinteger(L, 2, 0);
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_group_begin(g_ctx, title, flags));
  return 1;
}

// nk.group_end()
static int l_nk_group_end(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_group_end(g_ctx);
  return 0;
}

// nk.tree_push(type, title, state) -> bool
static int l_nk_tree_push(lua_State *L) {
  int type = (int)luaL_optinteger(L, 1, NK_TREE_TAB);
  const char *title = luaL_checkstring(L, 2);
  int state = (int)luaL_optinteger(L, 3, NK_MINIMIZED);
  if (!g_ctx)
    return 0;
  lua_pushboolean(
      L, nk_tree_push(g_ctx, (enum nk_tree_type)type, title,
                      (enum nk_collapse_states)state));
  return 1;
}

// nk.tree_pop()
static int l_nk_tree_pop(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_tree_pop(g_ctx);
  return 0;
}

// =============================================================================
// WIDGETS: TEXT & LABELS
// =============================================================================

static int l_nk_label(lua_State *L) {
  const char *text = luaL_checkstring(L, 1);
  nk_flags align = (nk_flags)luaL_optinteger(L, 2, NK_TEXT_LEFT);
  if (g_ctx)
    nk_label(g_ctx, text, align);
  return 0;
}

// nk.label_colored(text, align, r, g, b, a)
static int l_nk_label_colored(lua_State *L) {
  const char *text = luaL_checkstring(L, 1);
  nk_flags align = (nk_flags)luaL_optinteger(L, 2, NK_TEXT_LEFT);
  int r = (int)luaL_optinteger(L, 3, 255);
  int g = (int)luaL_optinteger(L, 4, 255);
  int b = (int)luaL_optinteger(L, 5, 255);
  int a = (int)luaL_optinteger(L, 6, 255);
  if (g_ctx)
    nk_label_colored(g_ctx, text, align, nk_rgba(r, g, b, a));
  return 0;
}

// nk.label_wrap(text)
static int l_nk_label_wrap(lua_State *L) {
  const char *text = luaL_checkstring(L, 1);
  if (g_ctx)
    nk_label_wrap(g_ctx, text);
  return 0;
}

// =============================================================================
// WIDGETS: BUTTONS
// =============================================================================

static int l_nk_button_label(lua_State *L) {
  const char *label = luaL_checkstring(L, 1);
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_button_label(g_ctx, label));
  return 1;
}

// nk.button_color(r, g, b, a) -> bool
static int l_nk_button_color(lua_State *L) {
  int r = (int)luaL_checkinteger(L, 1);
  int g = (int)luaL_checkinteger(L, 2);
  int b = (int)luaL_checkinteger(L, 3);
  int a = (int)luaL_optinteger(L, 4, 255);
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_button_color(g_ctx, nk_rgba(r, g, b, a)));
  return 1;
}

// nk.button_symbol(symbol) -> bool
static int l_nk_button_symbol(lua_State *L) {
  int symbol = (int)luaL_checkinteger(L, 1);
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_button_symbol(g_ctx, (enum nk_symbol_type)symbol));
  return 1;
}

// =============================================================================
// WIDGETS: CHECKBOXES & RADIO
// =============================================================================

static int l_nk_check_label(lua_State *L) {
  const char *label = luaL_checkstring(L, 1);
  bool active = lua_toboolean(L, 2);
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_check_label(g_ctx, label, active));
  return 1;
}

static int l_nk_option_label(lua_State *L) {
  const char *label = luaL_checkstring(L, 1);
  bool active = lua_toboolean(L, 2);
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_option_label(g_ctx, label, active));
  return 1;
}

// =============================================================================
// WIDGETS: SLIDERS & PROGRESS
// =============================================================================

// nk.slider_float(min, val, max, step) -> new_val
static int l_nk_slider_float(lua_State *L) {
  float min = (float)luaL_checknumber(L, 1);
  float val = (float)luaL_checknumber(L, 2);
  float max = (float)luaL_checknumber(L, 3);
  float step = (float)luaL_optnumber(L, 4, 0.1f);
  if (!g_ctx)
    return 0;
  nk_slider_float(g_ctx, min, &val, max, step);
  lua_pushnumber(L, val);
  return 1;
}

// nk.slider_int(min, val, max, step) -> new_val
static int l_nk_slider_int(lua_State *L) {
  int min = (int)luaL_checkinteger(L, 1);
  int val = (int)luaL_checkinteger(L, 2);
  int max = (int)luaL_checkinteger(L, 3);
  int step = (int)luaL_optinteger(L, 4, 1);
  if (!g_ctx)
    return 0;
  nk_slider_int(g_ctx, min, &val, max, step);
  lua_pushinteger(L, val);
  return 1;
}

// nk.progress(cur, max, modifiable) -> new_cur
static int l_nk_progress(lua_State *L) {
  nk_size cur = (nk_size)luaL_checkinteger(L, 1);
  nk_size max = (nk_size)luaL_checkinteger(L, 2);
  bool modifiable = lua_toboolean(L, 3);
  if (!g_ctx)
    return 0;
  nk_progress(g_ctx, &cur, max, modifiable);
  lua_pushinteger(L, (lua_Integer)cur);
  return 1;
}

// =============================================================================
// WIDGETS: PROPERTIES
// =============================================================================

static int l_nk_property_int(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  int min = (int)luaL_checkinteger(L, 2);
  int val = (int)luaL_checkinteger(L, 3);
  int max = (int)luaL_checkinteger(L, 4);
  int step = (int)luaL_checkinteger(L, 5);
  float inc_per_pixel = (float)luaL_optnumber(L, 6, 1.0f);

  if (!g_ctx)
    return 0;
  int res = nk_propertyi(g_ctx, name, min, val, max, step, inc_per_pixel);
  lua_pushinteger(L, res);
  return 1;
}

static int l_nk_property_float(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  float min = (float)luaL_checknumber(L, 2);
  float val = (float)luaL_checknumber(L, 3);
  float max = (float)luaL_checknumber(L, 4);
  float step = (float)luaL_checknumber(L, 5);
  float inc_per_pixel = (float)luaL_optnumber(L, 6, 1.0f);

  if (!g_ctx)
    return 0;
  float res = nk_propertyf(g_ctx, name, min, val, max, step, inc_per_pixel);
  lua_pushnumber(L, res);
  return 1;
}

// =============================================================================
// WIDGETS: TEXT INPUT
// =============================================================================

static int l_nk_edit_string(lua_State *L) {
  const char *text = luaL_checkstring(L, 1);
  nk_flags flags = (nk_flags)luaL_optinteger(L, 2, NK_EDIT_FIELD);
  if (!g_ctx)
    return 0;

  char buffer[1024];
  strncpy(buffer, text, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  nk_flags ret = nk_edit_string_zero_terminated(g_ctx, flags, buffer,
                                                sizeof(buffer) - 1,
                                                nk_filter_default);

  lua_pushstring(L, buffer);
  lua_pushinteger(L, (lua_Integer)ret);
  return 2;
}

// nk.edit_multiline(text, flags) -> new_text, event_flags
static int l_nk_edit_multiline(lua_State *L) {
  const char *text = luaL_checkstring(L, 1);
  nk_flags flags =
      (nk_flags)luaL_optinteger(L, 2, NK_EDIT_BOX | NK_EDIT_MULTILINE);
  if (!g_ctx)
    return 0;

  static char buffer[4096];
  strncpy(buffer, text, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  nk_flags ret = nk_edit_string_zero_terminated(g_ctx, flags, buffer,
                                                sizeof(buffer) - 1,
      nk_filter_default);

  lua_pushstring(L, buffer);
  lua_pushinteger(L, (lua_Integer)ret);
  return 2;
}

// =============================================================================
// WIDGETS: COMBO & SELECTABLE
// =============================================================================

// nk.combo(items_table, selected_idx, item_height, width, height) -> new_idx
static int l_nk_combo(lua_State *L) {
  if (!lua_istable(L, 1)) {
    return luaL_error(L, "expected table as first argument");
  }
  int selected = (int)luaL_checkinteger(L, 2) - 1; // Lua 1-indexed
  int item_height = (int)luaL_optinteger(L, 3, 25);
  float width = (float)luaL_optnumber(L, 4, 200);
  float height = (float)luaL_optnumber(L, 5, 200);

  if (!g_ctx)
    return 0;

  int count = (int)lua_rawlen(L, 1);
  if (count <= 0) {
    lua_pushinteger(L, 1);
    return 1;
  }

  // Get selected item label for combo button
  lua_rawgeti(L, 1, selected + 1);
  const char *current = lua_tostring(L, -1);
  if (!current)
    current = "";
  lua_pop(L, 1);

  struct nk_vec2 size = nk_vec2(width, height);
  if (nk_combo_begin_label(g_ctx, current, size)) {
    nk_layout_row_dynamic(g_ctx, (float)item_height, 1);
    for (int i = 0; i < count; i++) {
      lua_rawgeti(L, 1, i + 1);
      const char *item = lua_tostring(L, -1);
      if (item && nk_combo_item_label(g_ctx, item, NK_TEXT_LEFT)) {
        selected = i;
      }
      lua_pop(L, 1);
    }
    nk_combo_end(g_ctx);
  }

  lua_pushinteger(L, selected + 1); // Return Lua 1-indexed
  return 1;
}

// nk.selectable(label, selected) -> new_selected
static int l_nk_selectable(lua_State *L) {
  const char *label = luaL_checkstring(L, 1);
  int selected = lua_toboolean(L, 2) ? 1 : 0;
  if (!g_ctx)
    return 0;
  nk_selectable_label(g_ctx, label, NK_TEXT_LEFT, &selected);
  lua_pushboolean(L, selected);
  return 1;
}

// =============================================================================
// WIDGETS: COLOR PICKER
// =============================================================================

// nk.color_picker(r, g, b, a, format) -> r, g, b, a
static int l_nk_color_picker(lua_State *L) {
  int r = (int)luaL_checkinteger(L, 1);
  int g = (int)luaL_checkinteger(L, 2);
  int b = (int)luaL_checkinteger(L, 3);
  int a = (int)luaL_optinteger(L, 4, 255);
  int format = (int)luaL_optinteger(L, 5, NK_RGBA);

  if (!g_ctx)
    return 0;

  struct nk_colorf colorf =
      nk_color_cf(nk_rgba(r, g, b, a));
  colorf = nk_color_picker(g_ctx, colorf, (enum nk_color_format)format);
  struct nk_color color = nk_rgba_cf(colorf);

  lua_pushinteger(L, color.r);
  lua_pushinteger(L, color.g);
  lua_pushinteger(L, color.b);
  lua_pushinteger(L, color.a);
  return 4;
}

// =============================================================================
// DRAWING PRIMITIVES (Custom Canvas)
// =============================================================================

// nk.stroke_line(x0, y0, x1, y1, thickness, r, g, b, a)
static int l_nk_stroke_line(lua_State *L) {
  float x0 = (float)luaL_checknumber(L, 1);
  float y0 = (float)luaL_checknumber(L, 2);
  float x1 = (float)luaL_checknumber(L, 3);
  float y1 = (float)luaL_checknumber(L, 4);
  float thickness = (float)luaL_optnumber(L, 5, 1.0f);
  int r = (int)luaL_optinteger(L, 6, 255);
  int g = (int)luaL_optinteger(L, 7, 255);
  int b = (int)luaL_optinteger(L, 8, 255);
  int a = (int)luaL_optinteger(L, 9, 255);

  if (!g_ctx)
    return 0;

  struct nk_command_buffer *canvas = nk_window_get_canvas(g_ctx);
  if (canvas) {
    nk_stroke_line(canvas, x0, y0, x1, y1, thickness, nk_rgba(r, g, b, a));
  }
  return 0;
}

// nk.stroke_rect(x, y, w, h, rounding, thickness, r, g, b, a)
static int l_nk_stroke_rect(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);
  float rounding = (float)luaL_optnumber(L, 5, 0.0f);
  float thickness = (float)luaL_optnumber(L, 6, 1.0f);
  int r = (int)luaL_optinteger(L, 7, 255);
  int g = (int)luaL_optinteger(L, 8, 255);
  int b = (int)luaL_optinteger(L, 9, 255);
  int a = (int)luaL_optinteger(L, 10, 255);

  if (!g_ctx)
    return 0;

  struct nk_command_buffer *canvas = nk_window_get_canvas(g_ctx);
  if (canvas) {
    nk_stroke_rect(canvas, nk_rect(x, y, w, h), rounding, thickness,
                   nk_rgba(r, g, b, a));
  }
  return 0;
}

// nk.fill_rect(x, y, w, h, rounding, r, g, b, a)
static int l_nk_fill_rect(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);
  float rounding = (float)luaL_optnumber(L, 5, 0.0f);
  int r = (int)luaL_optinteger(L, 6, 255);
  int g = (int)luaL_optinteger(L, 7, 255);
  int b = (int)luaL_optinteger(L, 8, 255);
  int a = (int)luaL_optinteger(L, 9, 255);

  if (!g_ctx)
    return 0;

  struct nk_command_buffer *canvas = nk_window_get_canvas(g_ctx);
  if (canvas) {
    nk_fill_rect(canvas, nk_rect(x, y, w, h), rounding, nk_rgba(r, g, b, a));
  }
  return 0;
}

// nk.stroke_circle(x, y, w, h, thickness, r, g, b, a)
static int l_nk_stroke_circle(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);
  float thickness = (float)luaL_optnumber(L, 5, 1.0f);
  int r = (int)luaL_optinteger(L, 6, 255);
  int g = (int)luaL_optinteger(L, 7, 255);
  int b = (int)luaL_optinteger(L, 8, 255);
  int a = (int)luaL_optinteger(L, 9, 255);

  if (!g_ctx)
    return 0;

  struct nk_command_buffer *canvas = nk_window_get_canvas(g_ctx);
  if (canvas) {
    nk_stroke_circle(canvas, nk_rect(x, y, w, h), thickness, nk_rgba(r, g, b, a));
  }
  return 0;
}

// nk.fill_circle(x, y, w, h, r, g, b, a)
static int l_nk_fill_circle(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);
  int r = (int)luaL_optinteger(L, 5, 255);
  int g = (int)luaL_optinteger(L, 6, 255);
  int b = (int)luaL_optinteger(L, 7, 255);
  int a = (int)luaL_optinteger(L, 8, 255);

  if (!g_ctx)
    return 0;

  struct nk_command_buffer *canvas = nk_window_get_canvas(g_ctx);
  if (canvas) {
    nk_fill_circle(canvas, nk_rect(x, y, w, h), nk_rgba(r, g, b, a));
  }
  return 0;
}

// nk.stroke_triangle(x0, y0, x1, y1, x2, y2, thickness, r, g, b, a)
static int l_nk_stroke_triangle(lua_State *L) {
  float x0 = (float)luaL_checknumber(L, 1);
  float y0 = (float)luaL_checknumber(L, 2);
  float x1 = (float)luaL_checknumber(L, 3);
  float y1 = (float)luaL_checknumber(L, 4);
  float x2 = (float)luaL_checknumber(L, 5);
  float y2 = (float)luaL_checknumber(L, 6);
  float thickness = (float)luaL_optnumber(L, 7, 1.0f);
  int r = (int)luaL_optinteger(L, 8, 255);
  int g = (int)luaL_optinteger(L, 9, 255);
  int b = (int)luaL_optinteger(L, 10, 255);
  int a = (int)luaL_optinteger(L, 11, 255);

  if (!g_ctx)
    return 0;

  struct nk_command_buffer *canvas = nk_window_get_canvas(g_ctx);
  if (canvas) {
    nk_stroke_triangle(canvas, x0, y0, x1, y1, x2, y2, thickness,
                       nk_rgba(r, g, b, a));
  }
  return 0;
}

// nk.fill_triangle(x0, y0, x1, y1, x2, y2, r, g, b, a)
static int l_nk_fill_triangle(lua_State *L) {
  float x0 = (float)luaL_checknumber(L, 1);
  float y0 = (float)luaL_checknumber(L, 2);
  float x1 = (float)luaL_checknumber(L, 3);
  float y1 = (float)luaL_checknumber(L, 4);
  float x2 = (float)luaL_checknumber(L, 5);
  float y2 = (float)luaL_checknumber(L, 6);
  int r = (int)luaL_optinteger(L, 7, 255);
  int g = (int)luaL_optinteger(L, 8, 255);
  int b = (int)luaL_optinteger(L, 9, 255);
  int a = (int)luaL_optinteger(L, 10, 255);

  if (!g_ctx)
    return 0;

  struct nk_command_buffer *canvas = nk_window_get_canvas(g_ctx);
  if (canvas) {
    nk_fill_triangle(canvas, x0, y0, x1, y1, x2, y2, nk_rgba(r, g, b, a));
  }
  return 0;
}

// =============================================================================
// INPUT STATE QUERIES
// =============================================================================

// nk.input_is_mouse_click_in_rect(button, x, y, w, h) -> bool
static int l_nk_input_mouse_click_in_rect(lua_State *L) {
  int button = (int)luaL_optinteger(L, 1, NK_BUTTON_LEFT);
  float x = (float)luaL_checknumber(L, 2);
  float y = (float)luaL_checknumber(L, 3);
  float w = (float)luaL_checknumber(L, 4);
  float h = (float)luaL_checknumber(L, 5);

  if (!g_ctx)
    return 0;

  lua_pushboolean(L, nk_input_is_mouse_click_in_rect(&g_ctx->input,
                                                     (enum nk_buttons)button,
                                                     nk_rect(x, y, w, h)));
  return 1;
}

// nk.input_is_mouse_hovering_rect(x, y, w, h) -> bool
static int l_nk_input_mouse_hovering_rect(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);

  if (!g_ctx)
    return 0;

  lua_pushboolean(
      L, nk_input_is_mouse_hovering_rect(&g_ctx->input, nk_rect(x, y, w, h)));
  return 1;
}

// nk.input_mouse_pos() -> x, y
static int l_nk_input_mouse_pos(lua_State *L) {
  if (!g_ctx)
    return 0;
  lua_pushnumber(L, g_ctx->input.mouse.pos.x);
  lua_pushnumber(L, g_ctx->input.mouse.pos.y);
  return 2;
}

// nk.input_mouse_delta() -> dx, dy
static int l_nk_input_mouse_delta(lua_State *L) {
  if (!g_ctx)
    return 0;
  lua_pushnumber(L, g_ctx->input.mouse.delta.x);
  lua_pushnumber(L, g_ctx->input.mouse.delta.y);
  return 2;
}

// =============================================================================
// POPUP & CONTEXTUAL MENUS
// =============================================================================

// nk.popup_begin(type, title, flags, x, y, w, h) -> bool
static int l_nk_popup_begin(lua_State *L) {
  int type = (int)luaL_optinteger(L, 1, NK_POPUP_STATIC);
  const char *title = luaL_checkstring(L, 2);
  nk_flags flags = (nk_flags)luaL_optinteger(L, 3, NK_WINDOW_BORDER);
  float x = (float)luaL_checknumber(L, 4);
  float y = (float)luaL_checknumber(L, 5);
  float w = (float)luaL_checknumber(L, 6);
  float h = (float)luaL_checknumber(L, 7);

  if (!g_ctx)
    return 0;

  lua_pushboolean(
      L, nk_popup_begin(g_ctx, (enum nk_popup_type)type, title, flags,
                        nk_rect(x, y, w, h)));
  return 1;
}

// nk.popup_close()
static int l_nk_popup_close(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_popup_close(g_ctx);
  return 0;
}

// nk.popup_end()
static int l_nk_popup_end(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_popup_end(g_ctx);
  return 0;
}

// nk.contextual_begin(flags, w, h, trigger_x, trigger_y, trigger_w, trigger_h) -> bool
static int l_nk_contextual_begin(lua_State *L) {
  nk_flags flags = (nk_flags)luaL_optinteger(L, 1, 0);
  float w = (float)luaL_checknumber(L, 2);
  float h = (float)luaL_checknumber(L, 3);
  float tx = (float)luaL_checknumber(L, 4);
  float ty = (float)luaL_checknumber(L, 5);
  float tw = (float)luaL_checknumber(L, 6);
  float th = (float)luaL_checknumber(L, 7);

  if (!g_ctx)
    return 0;

  lua_pushboolean(L, nk_contextual_begin(g_ctx, flags, nk_vec2(w, h),
                                         nk_rect(tx, ty, tw, th)));
  return 1;
}

// nk.contextual_item_label(label, align) -> bool
static int l_nk_contextual_item_label(lua_State *L) {
  const char *label = luaL_checkstring(L, 1);
  nk_flags align = (nk_flags)luaL_optinteger(L, 2, NK_TEXT_LEFT);

  if (!g_ctx)
    return 0;

  lua_pushboolean(L, nk_contextual_item_label(g_ctx, label, align));
  return 1;
}

// nk.contextual_close()
static int l_nk_contextual_close(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_contextual_close(g_ctx);
  return 0;
}

// nk.contextual_end()
static int l_nk_contextual_end(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_contextual_end(g_ctx);
  return 0;
}

// =============================================================================
// TOOLTIP
// =============================================================================

// nk.tooltip(text)
static int l_nk_tooltip(lua_State *L) {
  const char *text = luaL_checkstring(L, 1);
  if (g_ctx)
    nk_tooltip(g_ctx, text);
  return 0;
}

// nk.tooltip_begin(width) -> bool
static int l_nk_tooltip_begin(lua_State *L) {
  float width = (float)luaL_checknumber(L, 1);
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_tooltip_begin(g_ctx, width));
  return 1;
}

// nk.tooltip_end()
static int l_nk_tooltip_end(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_tooltip_end(g_ctx);
  return 0;
}

// =============================================================================
// STYLE
// =============================================================================

// nk.style_push_color(type, r, g, b, a)
// For simplicity, just expose button normal color
static int l_nk_style_push_button_color(lua_State *L) {
  int r = (int)luaL_checkinteger(L, 1);
  int g = (int)luaL_checkinteger(L, 2);
  int b = (int)luaL_checkinteger(L, 3);
  int a = (int)luaL_optinteger(L, 4, 255);

  if (!g_ctx)
    return 0;

  nk_style_push_color(g_ctx, &g_ctx->style.button.normal.data.color,
                      nk_rgba(r, g, b, a));
  return 0;
}

// nk.style_pop_color()
static int l_nk_style_pop_color(lua_State *L) {
  (void)L;
  if (g_ctx)
    nk_style_pop_color(g_ctx);
  return 0;
}

// =============================================================================
// REGISTRATION
// =============================================================================

static const struct luaL_Reg nk_lib[] = {
    // Window
    {"begin", l_nk_begin},
    {"nk_end", l_nk_end},
    {"window_get_bounds", l_nk_window_get_bounds},
    {"window_get_content_region", l_nk_window_get_content_region},
    {"window_is_hovered", l_nk_window_is_hovered},
    {"window_is_any_hovered", l_nk_window_is_any_hovered},
    {"item_is_any_active", l_nk_item_is_any_active},

    // Layout
    {"layout_row_dynamic", l_nk_layout_row_dynamic},
    {"layout_row_static", l_nk_layout_row_static},
    {"layout_row_begin", l_nk_layout_row_begin},
    {"layout_row_push", l_nk_layout_row_push},
    {"layout_row_end", l_nk_layout_row_end},
    {"layout_space_begin", l_nk_layout_space_begin},
    {"layout_space_push", l_nk_layout_space_push},
    {"layout_space_end", l_nk_layout_space_end},
    {"spacing", l_nk_spacing},

    // Groups & Trees
    {"group_begin", l_nk_group_begin},
    {"group_end", l_nk_group_end},
    {"tree_push", l_nk_tree_push},
    {"tree_pop", l_nk_tree_pop},

    // Labels
    {"label", l_nk_label},
    {"label_colored", l_nk_label_colored},
    {"label_wrap", l_nk_label_wrap},

    // Buttons
    {"button", l_nk_button_label},
    {"button_color", l_nk_button_color},
    {"button_symbol", l_nk_button_symbol},

    // Checkbox & Radio
    {"check", l_nk_check_label},
    {"option", l_nk_option_label},

    // Sliders & Progress
    {"slider_float", l_nk_slider_float},
    {"slider_int", l_nk_slider_int},
    {"progress", l_nk_progress},

    // Properties
    {"property_int", l_nk_property_int},
    {"property_float", l_nk_property_float},

    // Text Input
    {"edit_string", l_nk_edit_string},
    {"edit_multiline", l_nk_edit_multiline},

    // Combo & Selectable
    {"combo", l_nk_combo},
    {"selectable", l_nk_selectable},

    // Color Picker
    {"color_picker", l_nk_color_picker},

    // Drawing
    {"stroke_line", l_nk_stroke_line},
    {"stroke_rect", l_nk_stroke_rect},
    {"fill_rect", l_nk_fill_rect},
    {"stroke_circle", l_nk_stroke_circle},
    {"fill_circle", l_nk_fill_circle},
    {"stroke_triangle", l_nk_stroke_triangle},
    {"fill_triangle", l_nk_fill_triangle},

    // Input
    {"input_mouse_click_in_rect", l_nk_input_mouse_click_in_rect},
    {"input_mouse_hovering_rect", l_nk_input_mouse_hovering_rect},
    {"input_mouse_pos", l_nk_input_mouse_pos},
    {"input_mouse_delta", l_nk_input_mouse_delta},

    // Popup & Contextual
    {"popup_begin", l_nk_popup_begin},
    {"popup_close", l_nk_popup_close},
    {"popup_end", l_nk_popup_end},
    {"contextual_begin", l_nk_contextual_begin},
    {"contextual_item_label", l_nk_contextual_item_label},
    {"contextual_close", l_nk_contextual_close},
    {"contextual_end", l_nk_contextual_end},

    // Tooltip
    {"tooltip", l_nk_tooltip},
    {"tooltip_begin", l_nk_tooltip_begin},
    {"tooltip_end", l_nk_tooltip_end},

    // Style
    {"style_push_button_color", l_nk_style_push_button_color},
    {"style_pop_color", l_nk_style_pop_color},

    {NULL, NULL}};

int luaopen_nk(lua_State *L) {
  luaL_newlib(L, nk_lib);

  // ==========================================================================
  // CONSTANTS
  // ==========================================================================

  // Window Flags
  lua_pushinteger(L, NK_WINDOW_BORDER);
  lua_setfield(L, -2, "WINDOW_BORDER");
  lua_pushinteger(L, NK_WINDOW_MOVABLE);
  lua_setfield(L, -2, "WINDOW_MOVABLE");
  lua_pushinteger(L, NK_WINDOW_SCALABLE);
  lua_setfield(L, -2, "WINDOW_SCALABLE");
  lua_pushinteger(L, NK_WINDOW_CLOSABLE);
  lua_setfield(L, -2, "WINDOW_CLOSABLE");
  lua_pushinteger(L, NK_WINDOW_MINIMIZABLE);
  lua_setfield(L, -2, "WINDOW_MINIMIZABLE");
  lua_pushinteger(L, NK_WINDOW_NO_SCROLLBAR);
  lua_setfield(L, -2, "WINDOW_NO_SCROLLBAR");
  lua_pushinteger(L, NK_WINDOW_TITLE);
  lua_setfield(L, -2, "WINDOW_TITLE");
  lua_pushinteger(L, NK_WINDOW_SCROLL_AUTO_HIDE);
  lua_setfield(L, -2, "WINDOW_SCROLL_AUTO_HIDE");
  lua_pushinteger(L, NK_WINDOW_BACKGROUND);
  lua_setfield(L, -2, "WINDOW_BACKGROUND");

  // Text Alignment
  lua_pushinteger(L, NK_TEXT_LEFT);
  lua_setfield(L, -2, "TEXT_LEFT");
  lua_pushinteger(L, NK_TEXT_CENTERED);
  lua_setfield(L, -2, "TEXT_CENTERED");
  lua_pushinteger(L, NK_TEXT_RIGHT);
  lua_setfield(L, -2, "TEXT_RIGHT");

  // Edit Flags
  lua_pushinteger(L, NK_EDIT_DEFAULT);
  lua_setfield(L, -2, "EDIT_DEFAULT");
  lua_pushinteger(L, NK_EDIT_READ_ONLY);
  lua_setfield(L, -2, "EDIT_READ_ONLY");
  lua_pushinteger(L, NK_EDIT_AUTO_SELECT);
  lua_setfield(L, -2, "EDIT_AUTO_SELECT");
  lua_pushinteger(L, NK_EDIT_SIG_ENTER);
  lua_setfield(L, -2, "EDIT_SIG_ENTER");
  lua_pushinteger(L, NK_EDIT_FIELD);
  lua_setfield(L, -2, "EDIT_FIELD");
  lua_pushinteger(L, NK_EDIT_BOX);
  lua_setfield(L, -2, "EDIT_BOX");
  lua_pushinteger(L, NK_EDIT_MULTILINE);
  lua_setfield(L, -2, "EDIT_MULTILINE");
  lua_pushinteger(L, NK_EDIT_COMMITED);
  lua_setfield(L, -2, "EDIT_COMMITTED");

  // Layout Format
  lua_pushinteger(L, NK_DYNAMIC);
  lua_setfield(L, -2, "DYNAMIC");
  lua_pushinteger(L, NK_STATIC);
  lua_setfield(L, -2, "STATIC");

  // Tree Types
  lua_pushinteger(L, NK_TREE_NODE);
  lua_setfield(L, -2, "TREE_NODE");
  lua_pushinteger(L, NK_TREE_TAB);
  lua_setfield(L, -2, "TREE_TAB");

  // Collapse States
  lua_pushinteger(L, NK_MINIMIZED);
  lua_setfield(L, -2, "MINIMIZED");
  lua_pushinteger(L, NK_MAXIMIZED);
  lua_setfield(L, -2, "MAXIMIZED");

  // Symbol Types
  lua_pushinteger(L, NK_SYMBOL_NONE);
  lua_setfield(L, -2, "SYMBOL_NONE");
  lua_pushinteger(L, NK_SYMBOL_X);
  lua_setfield(L, -2, "SYMBOL_X");
  lua_pushinteger(L, NK_SYMBOL_UNDERSCORE);
  lua_setfield(L, -2, "SYMBOL_UNDERSCORE");
  lua_pushinteger(L, NK_SYMBOL_CIRCLE_SOLID);
  lua_setfield(L, -2, "SYMBOL_CIRCLE_SOLID");
  lua_pushinteger(L, NK_SYMBOL_CIRCLE_OUTLINE);
  lua_setfield(L, -2, "SYMBOL_CIRCLE_OUTLINE");
  lua_pushinteger(L, NK_SYMBOL_RECT_SOLID);
  lua_setfield(L, -2, "SYMBOL_RECT_SOLID");
  lua_pushinteger(L, NK_SYMBOL_RECT_OUTLINE);
  lua_setfield(L, -2, "SYMBOL_RECT_OUTLINE");
  lua_pushinteger(L, NK_SYMBOL_TRIANGLE_UP);
  lua_setfield(L, -2, "SYMBOL_TRIANGLE_UP");
  lua_pushinteger(L, NK_SYMBOL_TRIANGLE_DOWN);
  lua_setfield(L, -2, "SYMBOL_TRIANGLE_DOWN");
  lua_pushinteger(L, NK_SYMBOL_TRIANGLE_LEFT);
  lua_setfield(L, -2, "SYMBOL_TRIANGLE_LEFT");
  lua_pushinteger(L, NK_SYMBOL_TRIANGLE_RIGHT);
  lua_setfield(L, -2, "SYMBOL_TRIANGLE_RIGHT");
  lua_pushinteger(L, NK_SYMBOL_PLUS);
  lua_setfield(L, -2, "SYMBOL_PLUS");
  lua_pushinteger(L, NK_SYMBOL_MINUS);
  lua_setfield(L, -2, "SYMBOL_MINUS");

  // Color Formats
  lua_pushinteger(L, NK_RGB);
  lua_setfield(L, -2, "RGB");
  lua_pushinteger(L, NK_RGBA);
  lua_setfield(L, -2, "RGBA");

  // Mouse Buttons
  lua_pushinteger(L, NK_BUTTON_LEFT);
  lua_setfield(L, -2, "BUTTON_LEFT");
  lua_pushinteger(L, NK_BUTTON_MIDDLE);
  lua_setfield(L, -2, "BUTTON_MIDDLE");
  lua_pushinteger(L, NK_BUTTON_RIGHT);
  lua_setfield(L, -2, "BUTTON_RIGHT");

  // Popup Types
  lua_pushinteger(L, NK_POPUP_STATIC);
  lua_setfield(L, -2, "POPUP_STATIC");
  lua_pushinteger(L, NK_POPUP_DYNAMIC);
  lua_setfield(L, -2, "POPUP_DYNAMIC");

  return 1;
}
