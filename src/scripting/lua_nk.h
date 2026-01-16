#ifndef LUA_NK_H
#define LUA_NK_H

#include "../platform/ui_host.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

// Registers the 'nk' module in Lua
int luaopen_nk(lua_State *L);

// Sets the Nuklear context to be used for subsequent 'nk' calls from Lua
void scripting_nk_set_context(struct nk_context *ctx);

#endif
