#ifndef LUA_HOST_H
#define LUA_HOST_H

#include "../document/document.h"

#ifdef NO_LUA
    // Stub types for compilation without Lua
    typedef struct lua_State lua_State;
#else
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
#endif

// Initialise l'environnement Lua et charge les bindings
lua_State* scripting_init(Document* doc);

// Ferme l'environnement Lua
void scripting_shutdown(lua_State* L);

// Exécute un fichier de script
int scripting_run_file(lua_State* L, const char* filepath);

#endif
