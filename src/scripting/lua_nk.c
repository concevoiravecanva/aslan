#include "lua_nk.h"
#include <string.h>

static struct nk_context *g_ctx = NULL;

void scripting_nk_set_context(struct nk_context *ctx) { g_ctx = ctx; }

// --- Window ---

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
  if (g_ctx)
    nk_end(g_ctx);
  return 0;
}

// --- Layout ---

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

// --- Widgets ---

static int l_nk_label(lua_State *L) {
  const char *text = luaL_checkstring(L, 1);
  nk_flags align = (nk_flags)luaL_optinteger(L, 2, NK_TEXT_LEFT);
  if (g_ctx)
    nk_label(g_ctx, text, align);
  return 0;
}

static int l_nk_button_label(lua_State *L) {
  const char *label = luaL_checkstring(L, 1);
  if (!g_ctx)
    return 0;
  lua_pushboolean(L, nk_button_label(g_ctx, label));
  return 1;
}

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

static int l_nk_edit_string(lua_State *L) {
  const char *text = luaL_checkstring(L, 1);
  if (!g_ctx)
    return 0;

  char buffer[1024];
  strncpy(buffer, text, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  // Minimalistic edit. We might need better state handling later.
  nk_flags ret = nk_edit_string_zero_terminated(
      g_ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, buffer, sizeof(buffer) - 1,
      nk_filter_default);

  lua_pushstring(L, buffer);
  lua_pushinteger(L, (lua_Integer)ret);
  return 2;
}

// --- Registration ---

static const struct luaL_Reg nk_lib[] = {
    {"begin", l_nk_begin},
    {"nk_end", l_nk_end}, // Renamed to avoid reserved keyword 'end' in Lua
    {"layout_row_dynamic", l_nk_layout_row_dynamic},
    {"layout_row_static", l_nk_layout_row_static},
    {"label", l_nk_label},
    {"button", l_nk_button_label},
    {"check", l_nk_check_label},
    {"option", l_nk_option_label},
    {"property_int", l_nk_property_int},
    {"property_float", l_nk_property_float},
    {"edit_string", l_nk_edit_string},
    {NULL, NULL}};

int luaopen_nk(lua_State *L) {
  luaL_newlib(L, nk_lib);

  // Constants: Window Flags
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

  // Constants: Text Alignment
  lua_pushinteger(L, NK_TEXT_LEFT);
  lua_setfield(L, -2, "TEXT_LEFT");
  lua_pushinteger(L, NK_TEXT_CENTERED);
  lua_setfield(L, -2, "TEXT_CENTERED");
  lua_pushinteger(L, NK_TEXT_RIGHT);
  lua_setfield(L, -2, "TEXT_RIGHT");

  // Constants: Edit Flags
  lua_pushinteger(L, NK_EDIT_COMMITED);
  lua_setfield(L, -2, "EDIT_COMMITTED");

  return 1;
}
