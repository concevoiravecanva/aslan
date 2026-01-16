#ifndef LUA_HOST_H
#define LUA_HOST_H

#include "../document/document.h"

// Forward declaration
struct Engine;

#ifdef NO_LUA
// Stub types for compilation without Lua
typedef struct lua_State lua_State;
#else
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#endif

// Initialise l'environnement Lua et charge les bindings
lua_State *scripting_init(struct Engine *engine);

// Définit un répertoire de base pour résoudre les scripts relatifs (ex:
// ".../Aslan")
void scripting_set_base_dir(const char *base_dir);

// Ferme l'environnement Lua
void scripting_shutdown(lua_State *L);

// Exécute un fichier de script
int scripting_run_file(lua_State *L, const char *filepath);

// Appelle une fonction Lua spécifique (ex: pour les événements)
void scripting_call_event(lua_State *L, const char *func_name, float x,
                          float y);
void scripting_call_key_event(lua_State *L, const char *func_name, int key,
                              int mods);

// Update raw mouse position in SCREEN coordinates (pixels) for Lua tools.
void scripting_set_mouse_screen(float sx, float sy);
void scripting_set_key_mods(int mods);

// Plugin System: load all scripts in scripts/
int scripting_load_plugins(lua_State *L);

// Sync internal script host pointers after Engine swaps/rebuilds its Document
// (undo/redo).
void scripting_set_document(Document *doc);

// Specialized UI render call (inside Nuklear context)
void scripting_call_ui_render(lua_State *L);

#endif
