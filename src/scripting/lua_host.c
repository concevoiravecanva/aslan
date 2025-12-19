#include "lua_host.h"
#include "../core/engine.h"
#include "../platform/ui_host.h"
#include <stdio.h>
#include <string.h>

#ifdef NO_LUA
// Stub implementation
lua_State* scripting_init(Document* doc) {
    (void)doc;
    printf("[WARN] Lua support disabled (NO_LUA defined).\n");
    return (lua_State*)1; // Return dummy non-NULL
}

void scripting_shutdown(lua_State* L) {
    (void)L;
}

int scripting_run_file(lua_State* L, const char* filepath) {
    (void)L;
    printf("[WARN] Cannot run script '%s': Lua disabled.\n", filepath);
    return 0;
}

#else

// Pointeur global vers le document courant pour les callbacks Lua
// Dans une architecture plus complexe, on passerait cela via un userdata ou upvalue
static Document* g_current_doc = NULL;
static Engine* g_current_engine = NULL;

// --- Bindings C -> Lua ---

// lua: execute(command_string) -> bool
static int l_execute(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    if (!g_current_engine) return 0;
    
    bool success = engine_execute_command(g_current_engine, cmd);
    lua_pushboolean(L, success);
    return 1;
}

// lua: create_rect(name, x, y, w, h) -> node_id
static int l_create_rect(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    
    if (!g_current_doc) return 0;
    
    Node* node = doc_create_node(g_current_doc, NODE_TYPE_RECT, name);
    node->x = x;
    node->y = y;
    node->width = w;
    node->height = h;
    
    // Ajout à la racine par défaut pour ce prototype
    doc_add_child(g_current_doc->root, node);
    
    lua_pushinteger(L, node->id);
    return 1; // Retourne 1 valeur (l'ID)
}

// lua: log_info(message)
static int l_log_info(lua_State* L) {
    const char* msg = luaL_checkstring(L, 1);
    printf("[LUA INFO] %s\n", msg);
    return 0;
}

// lua: get_node_at(x, y) -> node_id or nil
static int l_get_node_at(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    if (!g_current_doc) return 0;
    
    Node* node = doc_find_node_at(g_current_doc, x, y);
    if (node) {
        lua_pushinteger(L, node->id);
        return 1;
    }
    return 0;
}

// lua: set_node_pos(id, x, y)
static int l_set_node_pos(lua_State* L) {
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    
    if (!g_current_doc) return 0;
    Node* node = doc_find_node_by_id(g_current_doc, id);
    if (node) {
        node->x = x;
        node->y = y;
    }
    return 0;
}

// lua: get_selected_id() -> node_id or nil
static int l_get_selected_id(lua_State* L) {
    if (!g_current_engine || !g_current_engine->ui_host) return 0;
    if (g_current_engine->ui_host->selected_node) {
        lua_pushinteger(L, g_current_engine->ui_host->selected_node->id);
        return 1;
    }
    return 0;
}

// Enregistrement des fonctions
static const struct luaL_Reg aslan_lib[] = {
    {"execute", l_execute},
    {"create_rect", l_create_rect},
    {"get_node_at", l_get_node_at},
    {"get_selected_id", l_get_selected_id},
    {"log", l_log_info},
    {NULL, NULL}
};

int luaopen_aslan(lua_State* L) {
    luaL_newlib(L, aslan_lib);
    return 1;
}

// --- Gestion du cycle de vie ---

lua_State* scripting_init(Engine* engine) {
    g_current_engine = engine;
    g_current_doc = engine->doc;
    
    lua_State* L = luaL_newstate();
    luaL_openlibs(L); // Charge les libs standard (base, string, math...)
    
    // Enregistre notre module 'aslan' globalement pour simplifier les scripts
    luaL_requiref(L, "aslan", luaopen_aslan, 1);
    lua_pop(L, 1); // Retire la copie laissée par requiref
    
    return L;
}

void scripting_shutdown(lua_State* L) {
    if (L) lua_close(L);
    g_current_engine = NULL;
    g_current_doc = NULL;
}

int scripting_run_file(lua_State* L, const char* filepath) {
    if (luaL_dofile(L, filepath) != LUA_OK) {
        const char* error = lua_tostring(L, -1);
        fprintf(stderr, "[LUA ERROR] %s\n", error);
        lua_pop(L, 1);
        return 0;
    }
    return 1;
}

void scripting_call_event(lua_State* L, const char* func_name, float x, float y) {
    if (!L) return;
    
    lua_getglobal(L, func_name);
    if (lua_isfunction(L, -1)) {
        lua_pushnumber(L, x);
        lua_pushnumber(L, y);
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            fprintf(stderr, "[LUA EVENT ERROR] %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}
#endif
