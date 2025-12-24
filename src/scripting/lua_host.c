#include "lua_host.h"
#include "../core/engine.h"
#include "../platform/ui_host.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

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
static char g_script_base_dir[PATH_MAX] = {0};

// Raw mouse position in SCREEN coordinates (pixels). Updated from the main loop.
static float g_mouse_sx = 0.0f;
static float g_mouse_sy = 0.0f;

void scripting_set_document(Document* doc) {
    g_current_doc = doc;
}

void scripting_set_base_dir(const char* base_dir) {
    if (!base_dir) return;
    strncpy(g_script_base_dir, base_dir, sizeof(g_script_base_dir) - 1);
}

void scripting_set_mouse_screen(float sx, float sy) {
    g_mouse_sx = sx;
    g_mouse_sy = sy;
}

static int resolve_script_path(char out_path[PATH_MAX], const char* filepath) {
    if (!out_path || !filepath) return 0;

    // If absolute, use directly
    if (filepath[0] == '/') {
        strncpy(out_path, filepath, PATH_MAX - 1);
        return 1;
    }

    // Prefer explicit base dir if set
    if (g_script_base_dir[0] != '\0') {
        snprintf(out_path, PATH_MAX, "%s/%s", g_script_base_dir, filepath);
        return 1;
    }

    // Fallback: try relative as-is
    strncpy(out_path, filepath, PATH_MAX - 1);
    return 1;
}

// --- Bindings C -> Lua ---

// lua: execute(command_string) -> bool
static int l_execute(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    if (!g_current_engine) return 0;
    
    bool success = engine_execute_command(g_current_engine, cmd);
    lua_pushboolean(L, success);
    return 1;
}

// lua: execute_volatile(command_string) -> bool
// Exécute sans écrire dans history.log (utile pour les drags en continu)
static int l_execute_volatile(lua_State* L) {
    const char* cmd = luaL_checkstring(L, 1);
    if (!g_current_engine) return 0;

    FILE* temp = g_current_engine->history_file;
    g_current_engine->history_file = NULL;
    bool success = engine_execute_command(g_current_engine, cmd);
    g_current_engine->history_file = temp;

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
    if (!g_current_doc || !g_current_doc->root) return 0;
    
    Node* node = doc_find_node_at(g_current_doc, x, y);
    if (node) {
        lua_pushinteger(L, node->id);
        return 1;
    }
    return 0;
}

// lua: get_node_rect(id) -> x, y, w, h
static int l_get_node_rect(lua_State* L) {
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_current_doc) return 0;
    
    Node* node = doc_find_node_by_id(g_current_doc, id);
    if (node) {
        lua_pushnumber(L, node->x);
        lua_pushnumber(L, node->y);
        lua_pushnumber(L, node->width);
        lua_pushnumber(L, node->height);
        return 4;
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

// lua: set_tool_mode(mode_int)
// 0 = select/move/resize, 1 = create-rect
static int l_set_tool_mode(lua_State* L) {
    int mode = (int)luaL_checkinteger(L, 1);
    if (!g_current_engine || !g_current_engine->ui_host) return 0;
    g_current_engine->ui_host->tool_mode = mode;
    return 0;
}

// lua: get_tool_mode() -> mode_int
static int l_get_tool_mode(lua_State* L) {
    if (!g_current_engine || !g_current_engine->ui_host) return 0;
    lua_pushinteger(L, g_current_engine->ui_host->tool_mode);
    return 1;
}

// lua: get_active_page_id() -> page_id
static int l_get_active_page_id(lua_State* L) {
    if (!g_current_doc) return 0;
    lua_pushinteger(L, (lua_Integer)doc_page_active_id(g_current_doc));
    return 1;
}

// lua: get_page_type(page_id) -> int (PageType)
static int l_get_page_type(lua_State* L) {
    uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_current_doc) return 0;
    lua_pushinteger(L, (lua_Integer)doc_page_get_type(g_current_doc, page_id));
    return 1;
}

// lua: get_page_meta(page_id, key) -> number
static int l_get_page_meta(lua_State* L) {
    uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (!g_current_doc) return 0;
    lua_pushnumber(L, (lua_Number)doc_page_get_meta(g_current_doc, page_id, key));
    return 1;
}

// lua: set_page_meta(page_id, key, value) -> bool
// Writes via engine command so it is persisted in AOF (but not undoable per option B).
static int l_set_page_meta(lua_State* L) {
    uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
    const char* key = luaL_checkstring(L, 2);
    float value = (float)luaL_checknumber(L, 3);
    if (!g_current_engine) return 0;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "page_meta %u %s %f", (unsigned)page_id, key, value);
    bool ok = engine_execute_command(g_current_engine, cmd);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// lua: get_mouse_x() -> number
static int l_get_mouse_x(lua_State* L) {
    lua_pushnumber(L, (lua_Number)g_mouse_sx);
    return 1;
}

// lua: get_mouse_y() -> number
static int l_get_mouse_y(lua_State* L) {
    lua_pushnumber(L, (lua_Number)g_mouse_sy);
    return 1;
}

// lua: grid_get("A1") -> string|nil
// Reads from the active page if it is a GRID page.
static int l_aslan_grid_get(lua_State* L) {
    const char* addr = luaL_checkstring(L, 1);
    if (!g_current_doc) return 0;

    uint32_t pid = doc_page_active_id(g_current_doc);
    // Find page slot by page_id
    for (int i = 0; i < 32; i++) {
        if (!g_current_doc->pages[i].in_use) continue;
        if (g_current_doc->pages[i].page_id != pid) continue;
        if (g_current_doc->pages[i].type != PAGE_TYPE_GRID) {
            return 0;
        }
        AslanGrid* grid = g_current_doc->pages[i].grid;
        if (!grid) return 0;

        int r = 0, c = 0;
        if (!grid_address_to_coords(addr, &r, &c)) return 0;
        if (r < 0 || r >= grid->rows) return 0;
        if (c < 0 || c >= grid->cols) return 0;

        const char* v = (grid->data[r] && grid->data[r][c]) ? grid->data[r][c] : NULL;
        if (!v || !v[0]) return 0;
        lua_pushstring(L, v);
        return 1;
    }
    return 0;
}

// Enregistrement des fonctions
static const struct luaL_Reg aslan_lib[] = {
    {"execute", l_execute},
    {"execute_volatile", l_execute_volatile},
    {"create_rect", l_create_rect},
    {"get_node_at", l_get_node_at},
    {"get_node_rect", l_get_node_rect},
    {"get_selected_id", l_get_selected_id},
    {"set_tool_mode", l_set_tool_mode},
    {"get_tool_mode", l_get_tool_mode},
    {"get_active_page_id", l_get_active_page_id},
    {"get_page_type", l_get_page_type},
    {"get_page_meta", l_get_page_meta},
    {"set_page_meta", l_set_page_meta},
    {"get_mouse_x", l_get_mouse_x},
    {"get_mouse_y", l_get_mouse_y},
    {"grid_get", l_aslan_grid_get},
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
    char resolved[PATH_MAX];
    resolve_script_path(resolved, filepath);
    if (luaL_dofile(L, resolved) != LUA_OK) {
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
