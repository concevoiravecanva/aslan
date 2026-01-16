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

// =============================================================================
// PHASE 2: THE SCRIPTING EXPLOSION - Public API
// =============================================================================

// --- Lifecycle ---
lua_State *scripting_init(struct Engine *engine);
void scripting_shutdown(lua_State *L);

// --- Configuration ---
void scripting_set_base_dir(const char *base_dir);
void scripting_set_document(Document *doc);
void scripting_set_mouse_screen(float sx, float sy);
void scripting_set_key_mods(int mods);

// --- Script Execution ---
int scripting_run_file(lua_State *L, const char *filepath);
int scripting_load_plugins(lua_State *L);

// --- Event Callbacks ---
void scripting_call_event(lua_State *L, const char *func_name, float x, float y);
void scripting_call_key_event(lua_State *L, const char *func_name, int key, int mods);

// --- UI Rendering (inside Nuklear context) ---
void scripting_call_ui_render(lua_State *L);

// =============================================================================
// HOT-RELOAD SYSTEM
// =============================================================================
// Automatically reloads modified .lua scripts for rapid iteration.

void scripting_hot_reload_check(lua_State *L);

// =============================================================================
// NODE BEHAVIORS SYSTEM
// =============================================================================
// Per-node script attachment for interactive behaviors.
// Scripts can define: on_click_<id>(x, y), on_hover_<id>(), on_update_<id>(dt)

// Call behavior callbacks for a specific node
void scripting_node_on_click(lua_State *L, uint32_t node_id, float x, float y);
void scripting_node_on_hover(lua_State *L, uint32_t node_id);
void scripting_node_on_update(lua_State *L, uint32_t node_id, float dt);

// Update all registered node behaviors (call each frame)
void scripting_update_behaviors(lua_State *L, float dt);

// =============================================================================
// MINI-APP ARCHITECTURE
// =============================================================================
// Allows Lua to handle rendering for specialized node types (Grid, Clip, etc.)
// Register a renderer with: aslan.register_app(node_type, "render_func_name")

// Check if a node should be rendered by Lua
bool scripting_should_render_node(lua_State *L, Node *node);

// Render a node using its registered Lua app
void scripting_render_app(lua_State *L, Node *node, float screen_x,
                          float screen_y, float screen_w, float screen_h);

#endif
