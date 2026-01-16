#include "lua_host.h"
#include "../core/engine.h"
#include "../platform/ui_host.h"
#include "lua_nk.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// =============================================================================
// PHASE 2: THE SCRIPTING EXPLOSION
// =============================================================================
// This module implements:
// 1. Extended Lua-C bindings for document manipulation
// 2. Hot-reload capability for Lua scripts
// 3. Node Behaviors system (per-node scripts with callbacks)
// 4. Mini-App Architecture (Lua-driven rendering for specialized nodes)

#ifdef NO_LUA
// Stub implementation
lua_State *scripting_init(Document *doc) {
  (void)doc;
  printf("[WARN] Lua support disabled (NO_LUA defined).\n");
  return (lua_State *)1; // Return dummy non-NULL
}

void scripting_shutdown(lua_State *L) { (void)L; }

int scripting_run_file(lua_State *L, const char *filepath) {
  (void)L;
  printf("[WARN] Cannot run script '%s': Lua disabled.\n", filepath);
  return 0;
}

void scripting_set_document(Document *doc) { (void)doc; }
void scripting_set_base_dir(const char *base_dir) { (void)base_dir; }
void scripting_set_mouse_screen(float sx, float sy) { (void)sx; (void)sy; }
void scripting_set_key_mods(int mods) { (void)mods; }
int scripting_load_plugins(lua_State *L) { (void)L; return 0; }
void scripting_call_event(lua_State *L, const char *func_name, float x, float y) {
  (void)L; (void)func_name; (void)x; (void)y;
}
void scripting_call_key_event(lua_State *L, const char *func_name, int key, int mods) {
  (void)L; (void)func_name; (void)key; (void)mods;
}
void scripting_call_ui_render(lua_State *L) { (void)L; }

#else

// =============================================================================
// GLOBAL STATE
// =============================================================================

static Document *g_current_doc = NULL;
static Engine *g_current_engine = NULL;
static char g_script_base_dir[PATH_MAX] = {0};

// Raw mouse position in SCREEN coordinates (pixels)
static float g_mouse_sx = 0.0f;
static float g_mouse_sy = 0.0f;
static int g_key_mods = 0;

// =============================================================================
// HOT-RELOAD SYSTEM
// =============================================================================
// Track loaded scripts and their modification times for automatic reloading.

#define MAX_TRACKED_SCRIPTS 64

typedef struct {
  char path[PATH_MAX];
  time_t last_modified;
  bool active;
} TrackedScript;

static TrackedScript g_tracked_scripts[MAX_TRACKED_SCRIPTS];
static int g_tracked_count = 0;
static double g_last_hot_reload_check = 0.0;
#define HOT_RELOAD_INTERVAL_MS 1000.0

static time_t get_file_mtime(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    return st.st_mtime;
  }
  return 0;
}

static int track_script(const char *path) {
  // Check if already tracked
  for (int i = 0; i < g_tracked_count; i++) {
    if (g_tracked_scripts[i].active &&
        strcmp(g_tracked_scripts[i].path, path) == 0) {
      g_tracked_scripts[i].last_modified = get_file_mtime(path);
      return i;
    }
  }

  // Add new entry
  if (g_tracked_count >= MAX_TRACKED_SCRIPTS) {
    fprintf(stderr, "[WARN] Maximum tracked scripts reached\n");
    return -1;
  }

  int idx = g_tracked_count++;
  strncpy(g_tracked_scripts[idx].path, path, PATH_MAX - 1);
  g_tracked_scripts[idx].last_modified = get_file_mtime(path);
  g_tracked_scripts[idx].active = true;
  return idx;
}

static void clear_tracked_scripts(void) {
  g_tracked_count = 0;
  memset(g_tracked_scripts, 0, sizeof(g_tracked_scripts));
}

// Check for modified scripts and reload them
void scripting_hot_reload_check(lua_State *L) {
  if (!L)
    return;

  // Simple time-based throttle using static counter
  // In production, use SDL_GetTicks() or similar
  static int check_counter = 0;
  check_counter++;
  if (check_counter < 60) // Check every ~60 frames
    return;
  check_counter = 0;

  bool any_reloaded = false;
  for (int i = 0; i < g_tracked_count; i++) {
    if (!g_tracked_scripts[i].active)
      continue;

    time_t current_mtime = get_file_mtime(g_tracked_scripts[i].path);
    if (current_mtime > g_tracked_scripts[i].last_modified) {
      printf("[HOT-RELOAD] Reloading: %s\n", g_tracked_scripts[i].path);

      if (luaL_dofile(L, g_tracked_scripts[i].path) != LUA_OK) {
        fprintf(stderr, "[LUA RELOAD ERROR] %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
      } else {
        any_reloaded = true;
      }

      g_tracked_scripts[i].last_modified = current_mtime;
    }
  }

  if (any_reloaded) {
    printf("[HOT-RELOAD] Scripts reloaded successfully\n");
  }
}

// =============================================================================
// NODE BEHAVIORS SYSTEM
// =============================================================================
// Allows attaching Lua scripts to individual nodes.
// Scripts define callbacks: on_click(node_id, x, y), on_hover(node_id),
// on_update(node_id, dt), on_render(node_id, x, y, w, h)

#define MAX_NODE_BEHAVIORS 256

typedef struct {
  uint32_t node_id;
  char script_path[PATH_MAX];
  char script_name[64]; // Unique identifier for Lua namespace
  bool active;
  time_t last_modified;
} NodeBehavior;

static NodeBehavior g_node_behaviors[MAX_NODE_BEHAVIORS];
static int g_behavior_count = 0;

// Find behavior by node ID
static NodeBehavior *find_behavior(uint32_t node_id) {
  for (int i = 0; i < g_behavior_count; i++) {
    if (g_node_behaviors[i].active && g_node_behaviors[i].node_id == node_id) {
      return &g_node_behaviors[i];
    }
  }
  return NULL;
}

// lua: aslan.set_behavior(node_id, script_path) -> bool
static int l_set_behavior(lua_State *L) {
  uint32_t node_id = (uint32_t)luaL_checkinteger(L, 1);
  const char *script_path = luaL_checkstring(L, 2);

  if (!g_current_doc) {
    lua_pushboolean(L, 0);
    return 1;
  }

  // Verify node exists
  Node *node = doc_find_node_by_id(g_current_doc, node_id);
  if (!node) {
    fprintf(stderr, "[WARN] set_behavior: node %u not found\n", node_id);
    lua_pushboolean(L, 0);
    return 1;
  }

  // Check if behavior already exists
  NodeBehavior *existing = find_behavior(node_id);
  if (existing) {
    // Update existing
    strncpy(existing->script_path, script_path, PATH_MAX - 1);
    existing->last_modified = get_file_mtime(script_path);
  } else {
    // Add new
    if (g_behavior_count >= MAX_NODE_BEHAVIORS) {
      fprintf(stderr, "[WARN] Maximum node behaviors reached\n");
      lua_pushboolean(L, 0);
      return 1;
    }

    int idx = g_behavior_count++;
    g_node_behaviors[idx].node_id = node_id;
    strncpy(g_node_behaviors[idx].script_path, script_path, PATH_MAX - 1);
    snprintf(g_node_behaviors[idx].script_name, 64, "behavior_%u", node_id);
    g_node_behaviors[idx].active = true;
    g_node_behaviors[idx].last_modified = get_file_mtime(script_path);
  }

  // Resolve and load the script
  char resolved[PATH_MAX];
  if (g_script_base_dir[0] != '\0') {
    snprintf(resolved, PATH_MAX, "%s/%s", g_script_base_dir, script_path);
  } else {
    strncpy(resolved, script_path, PATH_MAX - 1);
  }

  // Load script into its own table namespace
  // Create: _G.behaviors[node_id] = {}
  lua_getglobal(L, "behaviors");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_setglobal(L, "behaviors");
    lua_getglobal(L, "behaviors");
  }

  // Execute script
  if (luaL_dofile(L, resolved) != LUA_OK) {
    fprintf(stderr, "[BEHAVIOR ERROR] Failed to load %s: %s\n", resolved,
            lua_tostring(L, -1));
    lua_pop(L, 2); // error + behaviors table
    lua_pushboolean(L, 0);
    return 1;
  }

  lua_pop(L, 1); // behaviors table

  printf("[BEHAVIOR] Attached %s to node %u\n", script_path, node_id);
  lua_pushboolean(L, 1);
  return 1;
}

// lua: aslan.remove_behavior(node_id) -> bool
static int l_remove_behavior(lua_State *L) {
  uint32_t node_id = (uint32_t)luaL_checkinteger(L, 1);

  for (int i = 0; i < g_behavior_count; i++) {
    if (g_node_behaviors[i].active && g_node_behaviors[i].node_id == node_id) {
      g_node_behaviors[i].active = false;
      lua_pushboolean(L, 1);
      return 1;
    }
  }

  lua_pushboolean(L, 0);
  return 1;
}

// lua: aslan.get_behavior(node_id) -> script_path or nil
static int l_get_behavior(lua_State *L) {
  uint32_t node_id = (uint32_t)luaL_checkinteger(L, 1);

  NodeBehavior *b = find_behavior(node_id);
  if (b) {
    lua_pushstring(L, b->script_path);
    return 1;
  }
  return 0;
}

// Call a behavior callback for a specific node
void scripting_call_node_callback(lua_State *L, uint32_t node_id,
                                  const char *callback_name, int nargs) {
  if (!L)
    return;

  // Look for global function: <callback>_<node_id> e.g., on_click_42
  char func_name[128];
  snprintf(func_name, sizeof(func_name), "%s_%u", callback_name, node_id);

  lua_getglobal(L, func_name);
  if (lua_isfunction(L, -1)) {
    // Move function before the arguments (which are already on stack)
    lua_insert(L, -(nargs + 1));

    if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
      fprintf(stderr, "[BEHAVIOR ERROR] %s: %s\n", func_name,
              lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  } else {
    // Try generic callback with node_id as first arg
    lua_pop(L, 1);
    lua_getglobal(L, callback_name);
    if (lua_isfunction(L, -1)) {
      lua_insert(L, -(nargs + 1));
      lua_pushinteger(L, node_id);
      lua_insert(L, -(nargs + 1));
      if (lua_pcall(L, nargs + 1, 0, 0) != LUA_OK) {
        fprintf(stderr, "[BEHAVIOR ERROR] %s: %s\n", callback_name,
                lua_tostring(L, -1));
        lua_pop(L, 1);
      }
    } else {
      lua_pop(L, 1 + nargs); // Pop function + unused args
    }
  }
}

// Call on_click callback for a node
void scripting_node_on_click(lua_State *L, uint32_t node_id, float x, float y) {
  if (!L)
    return;
  lua_pushnumber(L, x);
  lua_pushnumber(L, y);
  scripting_call_node_callback(L, node_id, "on_click", 2);
}

// Call on_hover callback for a node
void scripting_node_on_hover(lua_State *L, uint32_t node_id) {
  if (!L)
    return;
  scripting_call_node_callback(L, node_id, "on_hover", 0);
}

// Call on_update callback for a node (with delta time)
void scripting_node_on_update(lua_State *L, uint32_t node_id, float dt) {
  if (!L)
    return;
  lua_pushnumber(L, dt);
  scripting_call_node_callback(L, node_id, "on_update", 1);
}

// =============================================================================
// MINI-APP ARCHITECTURE
// =============================================================================
// Allows Lua to handle rendering for specialized node types (Grid, Clip, etc.)

// Register a Lua function as the renderer for a node type
// lua: aslan.register_app(node_type, render_func_name) -> bool
static int l_register_app(lua_State *L) {
  int node_type = (int)luaL_checkinteger(L, 1);
  const char *func_name = luaL_checkstring(L, 2);

  // Store in a global registry table
  lua_getglobal(L, "aslan_app_registry");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_setglobal(L, "aslan_app_registry");
    lua_getglobal(L, "aslan_app_registry");
  }

  lua_pushstring(L, func_name);
  lua_rawseti(L, -2, node_type);
  lua_pop(L, 1);

  printf("[MINI-APP] Registered renderer '%s' for node type %d\n", func_name,
         node_type);
  lua_pushboolean(L, 1);
  return 1;
}

// Check if a node type has a Lua app registered
bool scripting_has_app_renderer(lua_State *L, int node_type) {
  if (!L)
    return false;

  lua_getglobal(L, "aslan_app_registry");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return false;
  }

  lua_rawgeti(L, -1, node_type);
  bool has_renderer = lua_isstring(L, -1);
  lua_pop(L, 2);
  return has_renderer;
}

// Call Lua app renderer for a node
void scripting_render_app(lua_State *L, Node *node, float screen_x,
                          float screen_y, float screen_w, float screen_h) {
  if (!L || !node)
    return;

  // Get registry
  lua_getglobal(L, "aslan_app_registry");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  // Get function name for this node type
  lua_rawgeti(L, -1, (int)node->type);
  if (!lua_isstring(L, -1)) {
    lua_pop(L, 2);
    return;
  }

  const char *func_name = lua_tostring(L, -1);
  lua_pop(L, 2); // func_name string + registry table

  // Call the render function
  lua_getglobal(L, func_name);
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  lua_pushinteger(L, node->id);
  lua_pushnumber(L, screen_x);
  lua_pushnumber(L, screen_y);
  lua_pushnumber(L, screen_w);
  lua_pushnumber(L, screen_h);

  if (lua_pcall(L, 5, 0, 0) != LUA_OK) {
    fprintf(stderr, "[APP RENDER ERROR] %s: %s\n", func_name,
            lua_tostring(L, -1));
    lua_pop(L, 1);
  }
}

// =============================================================================
// BASIC SETTERS
// =============================================================================

void scripting_set_document(Document *doc) { g_current_doc = doc; }

void scripting_set_base_dir(const char *base_dir) {
  if (!base_dir)
    return;
  strncpy(g_script_base_dir, base_dir, sizeof(g_script_base_dir) - 1);
}

void scripting_set_mouse_screen(float sx, float sy) {
  g_mouse_sx = sx;
  g_mouse_sy = sy;
}

void scripting_set_key_mods(int mods) { g_key_mods = mods; }

// =============================================================================
// PATH RESOLUTION
// =============================================================================

static int resolve_script_path(char out_path[PATH_MAX], const char *filepath) {
  if (!out_path || !filepath)
    return 0;

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

// =============================================================================
// LUA BINDINGS: DOCUMENT MANIPULATION
// =============================================================================

// lua: execute(command_string) -> bool
static int l_execute(lua_State *L) {
  const char *cmd = luaL_checkstring(L, 1);
  if (!g_current_engine)
    return 0;

  bool success = engine_execute_command(g_current_engine, cmd);
  lua_pushboolean(L, success);
  return 1;
}

// lua: execute_volatile(command_string) -> bool
static int l_execute_volatile(lua_State *L) {
  const char *cmd = luaL_checkstring(L, 1);
  if (!g_current_engine)
    return 0;

  FILE *temp = g_current_engine->history_file;
  g_current_engine->history_file = NULL;
  bool success = engine_execute_command(g_current_engine, cmd);
  g_current_engine->history_file = temp;

  lua_pushboolean(L, success);
  return 1;
}

static int l_create_rect(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  float x = (float)luaL_checknumber(L, 2);
  float y = (float)luaL_checknumber(L, 3);
  float w = (float)luaL_checknumber(L, 4);
  float h = (float)luaL_checknumber(L, 5);

  if (!g_current_engine || !g_current_doc)
    return 0;

  uint32_t id = g_current_doc->next_node_id ? g_current_doc->next_node_id : 1;

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "create_rect %u %s %f %f %f %f", (unsigned)id,
           name, x, y, w, h);

  bool ok = engine_execute_command(g_current_engine, cmd);
  if (!ok)
    return 0;

  lua_pushinteger(L, (lua_Integer)id);
  return 1;
}

static int l_create_frame(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);

  if (!g_current_engine)
    return 0;

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "frame %f %f %f %f", x, y, w, h);

  bool ok = engine_execute_command(g_current_engine, cmd);
  if (!ok)
    return 0;

  if (g_current_engine->ui_host && g_current_engine->ui_host->selected_node) {
    lua_pushinteger(L,
                    (lua_Integer)g_current_engine->ui_host->selected_node->id);
    return 1;
  }

  return 0;
}

static int l_create_text_block(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);
  const char *content = luaL_checkstring(L, 5);

  if (!g_current_engine)
    return 0;

  char escaped[256];
  int j = 0;
  for (int i = 0; content[i] && j < (int)sizeof(escaped) - 2; i++) {
    if (content[i] == '"') {
      escaped[j++] = '\\';
    }
    escaped[j++] = content[i];
  }
  escaped[j] = '\0';

  char cmd[512];
  snprintf(cmd, sizeof(cmd), "new_text %f %f %f %f \"%s\"", x, y, w, h,
           escaped);

  bool ok = engine_execute_command(g_current_engine, cmd);
  if (!ok)
    return 0;

  if (g_current_engine->ui_host && g_current_engine->ui_host->selected_node &&
      g_current_engine->ui_host->selected_node->type == NODE_TYPE_TEXT) {
    lua_pushinteger(L,
                    (lua_Integer)g_current_engine->ui_host->selected_node->id);
    return 1;
  }

  return 0;
}

static int l_create_group(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  float w = (float)luaL_checknumber(L, 3);
  float h = (float)luaL_checknumber(L, 4);

  if (!g_current_engine || !g_current_doc)
    return 0;

  uint32_t id = g_current_doc->next_node_id ? g_current_doc->next_node_id : 1;

  char name[64];
  snprintf(name, sizeof(name), "Group%u", (unsigned)id);

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "create_group %u %s %f %f %f %f", (unsigned)id,
           name, x, y, w, h);

  bool ok = engine_execute_command(g_current_engine, cmd);
  if (!ok)
    return 0;

  lua_pushinteger(L, (lua_Integer)id);
  return 1;
}

static int l_group_attach(lua_State *L) {
  uint32_t group_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t child_id = (uint32_t)luaL_checkinteger(L, 2);

  if (!g_current_engine)
    return 0;

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "group_attach %u %u", (unsigned)group_id,
           (unsigned)child_id);

  bool ok = engine_execute_command(g_current_engine, cmd);
  lua_pushboolean(L, ok ? 1 : 0);
  return 1;
}

// lua: log_info(message)
static int l_log_info(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  printf("[LUA INFO] %s\n", msg);
  return 0;
}

// lua: get_node_at(x, y) -> node_id or nil
static int l_get_node_at(lua_State *L) {
  float x = (float)luaL_checknumber(L, 1);
  float y = (float)luaL_checknumber(L, 2);
  if (!g_current_doc || !g_current_doc->root)
    return 0;

  Node *node = doc_find_node_at(g_current_doc, x, y);
  if (node) {
    lua_pushinteger(L, node->id);
    return 1;
  }
  return 0;
}

// lua: get_node_rect(id) -> x, y, w, h
static int l_get_node_rect(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_doc)
    return 0;

  Node *node = doc_find_node_by_id(g_current_doc, id);
  if (node) {
    lua_pushnumber(L, node->x);
    lua_pushnumber(L, node->y);
    lua_pushnumber(L, node->width);
    lua_pushnumber(L, node->height);
    return 4;
  }
  return 0;
}

// lua: get_node_type(id) -> int (NodeType)
static int l_get_node_type(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_doc)
    return 0;

  Node *node = doc_find_node_by_id(g_current_doc, id);
  if (node) {
    lua_pushinteger(L, (lua_Integer)node->type);
    return 1;
  }
  return 0;
}

// lua: get_node_name(id) -> string or nil
static int l_get_node_name(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_doc)
    return 0;

  Node *node = doc_find_node_by_id(g_current_doc, id);
  if (node) {
    lua_pushstring(L, node->name);
    return 1;
  }
  return 0;
}

// lua: get_node_text(id) -> string or nil
static int l_get_node_text(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_doc)
    return 0;

  Node *node = doc_find_node_by_id(g_current_doc, id);
  if (node && node->text[0]) {
    lua_pushstring(L, node->text);
    return 1;
  }
  return 0;
}

// lua: set_node_pos(id, x, y)
static int l_set_node_pos(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  float x = (float)luaL_checknumber(L, 2);
  float y = (float)luaL_checknumber(L, 3);

  if (!g_current_doc)
    return 0;
  Node *node = doc_find_node_by_id(g_current_doc, id);
  if (node) {
    node->x = x;
    node->y = y;
  }
  return 0;
}

// lua: set_node_size(id, w, h)
static int l_set_node_size(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  float w = (float)luaL_checknumber(L, 2);
  float h = (float)luaL_checknumber(L, 3);

  if (!g_current_doc)
    return 0;
  Node *node = doc_find_node_by_id(g_current_doc, id);
  if (node) {
    node->width = w;
    node->height = h;
  }
  return 0;
}

// lua: get_selected_id() -> node_id or nil
static int l_get_selected_id(lua_State *L) {
  if (!g_current_engine || !g_current_engine->ui_host)
    return 0;
  if (g_current_engine->ui_host->selected_node) {
    lua_pushinteger(L, g_current_engine->ui_host->selected_node->id);
    return 1;
  }
  return 0;
}

// lua: select_node(id)
static int l_select_node(lua_State *L) {
  uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_engine || !g_current_engine->ui_host || !g_current_doc)
    return 0;

  Node *node = doc_find_node_by_id(g_current_doc, id);
  g_current_engine->ui_host->selected_node = node;
  return 0;
}

// lua: set_tool_mode(mode_int)
static int l_set_tool_mode(lua_State *L) {
  int mode = (int)luaL_checkinteger(L, 1);
  if (!g_current_engine || !g_current_engine->ui_host)
    return 0;
  g_current_engine->ui_host->tool_mode = mode;
  return 0;
}

// lua: get_tool_mode() -> mode_int
static int l_get_tool_mode(lua_State *L) {
  if (!g_current_engine || !g_current_engine->ui_host)
    return 0;
  lua_pushinteger(L, g_current_engine->ui_host->tool_mode);
  return 1;
}

// lua: get_active_page_id() -> page_id
static int l_get_active_page_id(lua_State *L) {
  if (!g_current_doc)
    return 0;
  lua_pushinteger(L, (lua_Integer)doc_page_active_id(g_current_doc));
  return 1;
}

// lua: get_page_type(page_id) -> int (PageType)
static int l_get_page_type(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_doc)
    return 0;
  lua_pushinteger(L, (lua_Integer)doc_page_get_type(g_current_doc, page_id));
  return 1;
}

// lua: get_page_meta(page_id, key) -> number
static int l_get_page_meta(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  const char *key = luaL_checkstring(L, 2);
  if (!g_current_doc)
    return 0;
  lua_pushnumber(L, (lua_Number)doc_page_get_meta(g_current_doc, page_id, key));
  return 1;
}

// lua: set_page_meta(page_id, key, value) -> bool
static int l_set_page_meta(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  const char *key = luaL_checkstring(L, 2);
  float value = (float)luaL_checknumber(L, 3);
  if (!g_current_engine)
    return 0;

  char cmd[256];
  snprintf(cmd, sizeof(cmd), "page_meta %u %s %f", (unsigned)page_id, key,
           value);
  bool ok = engine_execute_command(g_current_engine, cmd);
  lua_pushboolean(L, ok ? 1 : 0);
  return 1;
}

// lua: get_mouse_x() -> number
static int l_get_mouse_x(lua_State *L) {
  lua_pushnumber(L, (lua_Number)g_mouse_sx);
  return 1;
}

// lua: get_mouse_y() -> number
static int l_get_mouse_y(lua_State *L) {
  lua_pushnumber(L, (lua_Number)g_mouse_sy);
  return 1;
}

// lua: get_key_mods() -> integer (bitmask)
static int l_get_key_mods(lua_State *L) {
  lua_pushinteger(L, (lua_Integer)g_key_mods);
  return 1;
}

// lua: get_time() -> number (seconds since start)
static int l_get_time(lua_State *L) {
  static clock_t start_time = 0;
  if (start_time == 0)
    start_time = clock();
  double elapsed = (double)(clock() - start_time) / CLOCKS_PER_SEC;
  lua_pushnumber(L, elapsed);
  return 1;
}

// =============================================================================
// LUA BINDINGS: GRID ACCESS
// =============================================================================

// lua: grid_get("A1") -> string|nil
static int l_aslan_grid_get(lua_State *L) {
  const char *addr = luaL_checkstring(L, 1);
  if (!g_current_doc)
    return 0;

  uint32_t pid = doc_page_active_id(g_current_doc);
  for (int i = 0; i < 32; i++) {
    if (!g_current_doc->pages[i].in_use)
      continue;
    if (g_current_doc->pages[i].page_id != pid)
      continue;
    if (g_current_doc->pages[i].type != PAGE_TYPE_GRID)
      return 0;

    AslanGrid *grid = g_current_doc->pages[i].grid;
    if (!grid)
      return 0;

    int r = 0, c = 0;
    if (!grid_address_to_coords(addr, &r, &c))
      return 0;
    if (r < 0 || r >= grid->rows)
      return 0;
    if (c < 0 || c >= grid->cols)
      return 0;

    const char *v =
        (grid->data[r] && grid->data[r][c]) ? grid->data[r][c] : NULL;
    if (!v || !v[0])
      return 0;
    lua_pushstring(L, v);
    return 1;
  }
  return 0;
}

// lua: grid_set("A1", "value") -> bool
static int l_aslan_grid_set(lua_State *L) {
  const char *addr = luaL_checkstring(L, 1);
  const char *value = luaL_checkstring(L, 2);
  if (!g_current_engine)
    return 0;

  int r = 0, c = 0;
  if (!grid_address_to_coords(addr, &r, &c)) {
    lua_pushboolean(L, 0);
    return 1;
  }

  char escaped[512];
  int j = 0;
  for (int i = 0; value[i] && j < (int)sizeof(escaped) - 2; i++) {
    if (value[i] == '"') {
      escaped[j++] = '\\';
    }
    escaped[j++] = value[i];
  }
  escaped[j] = '\0';

  char cmd[640];
  snprintf(cmd, sizeof(cmd), "grid_cell 0 %d %d \"%s\"", r, c, escaped);
  bool ok = engine_execute_command(g_current_engine, cmd);
  lua_pushboolean(L, ok);
  return 1;
}

// lua: grid_node_get(node_id, row, col) -> string|nil
static int l_grid_node_get(lua_State *L) {
  uint32_t node_id = (uint32_t)luaL_checkinteger(L, 1);
  int row = (int)luaL_checkinteger(L, 2);
  int col = (int)luaL_checkinteger(L, 3);

  if (!g_current_doc)
    return 0;

  Node *node = doc_find_node_by_id(g_current_doc, node_id);
  if (!node || node->type != NODE_TYPE_GRID)
    return 0;

  GridPayload *grid = node_get_grid_payload(node);
  if (!grid)
    return 0;

  const char *v = grid_get_cell(grid, row, col);
  if (v && v[0]) {
    lua_pushstring(L, v);
    return 1;
  }
  return 0;
}

// lua: grid_node_set(node_id, row, col, value) -> bool
static int l_grid_node_set(lua_State *L) {
  uint32_t node_id = (uint32_t)luaL_checkinteger(L, 1);
  int row = (int)luaL_checkinteger(L, 2);
  int col = (int)luaL_checkinteger(L, 3);
  const char *value = luaL_checkstring(L, 4);

  if (!g_current_engine)
    return 0;

  char escaped[512];
  int j = 0;
  for (int i = 0; value[i] && j < (int)sizeof(escaped) - 2; i++) {
    if (value[i] == '"') {
      escaped[j++] = '\\';
    }
    escaped[j++] = value[i];
  }
  escaped[j] = '\0';

  char cmd[640];
  snprintf(cmd, sizeof(cmd), "grid_cell %u %d %d \"%s\"", node_id, row, col,
           escaped);
  bool ok = engine_execute_command(g_current_engine, cmd);
  lua_pushboolean(L, ok);
  return 1;
}

// =============================================================================
// LUA BINDINGS: ITERATION
// =============================================================================

// lua: get_all_nodes() -> table of node_ids
static int l_get_all_nodes(lua_State *L) {
  if (!g_current_doc || !g_current_doc->root)
    return 0;

  lua_newtable(L);
  int idx = 1;

  // Simple recursive helper via stack
  Node *stack[1024];
  int top = 0;
  if (g_current_doc->root->first_child) {
    stack[top++] = g_current_doc->root->first_child;
  }

  while (top > 0) {
    Node *n = stack[--top];
    if (!n)
      continue;

    lua_pushinteger(L, n->id);
    lua_rawseti(L, -2, idx++);

    if (n->next_sibling)
      stack[top++] = n->next_sibling;
    if (n->first_child)
      stack[top++] = n->first_child;

    if (top >= 1024)
      break;
  }

  return 1;
}

// lua: get_children(parent_id) -> table of node_ids
static int l_get_children(lua_State *L) {
  uint32_t parent_id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_doc)
    return 0;

  Node *parent = doc_find_node_by_id(g_current_doc, parent_id);
  if (!parent) {
    lua_newtable(L);
    return 1;
  }

  lua_newtable(L);
  int idx = 1;
  for (Node *child = parent->first_child; child; child = child->next_sibling) {
    lua_pushinteger(L, child->id);
    lua_rawseti(L, -2, idx++);
  }
  return 1;
}

// =============================================================================
// EXPORT FUNCTIONS (Phase 3)
// =============================================================================

#include "../media/export.h"

// lua: export_svg(filename, title, scale) -> bool
static int l_export_svg(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  const char *title = luaL_optstring(L, 2, "Aslan Export");
  float scale = (float)luaL_optnumber(L, 3, 1.0);

  if (!g_current_doc) {
    lua_pushboolean(L, 0);
    return 1;
  }

  ExportOptions opts;
  export_options_init(&opts);
  strncpy(opts.title, title, sizeof(opts.title) - 1);
  opts.scale = scale;

  bool ok = export_to_svg(g_current_doc, filename, &opts);
  lua_pushboolean(L, ok);
  return 1;
}

// lua: export_pdf(filename, title, page_size, landscape) -> bool
static int l_export_pdf(lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  const char *title = luaL_optstring(L, 2, "Aslan Export");
  const char *page_size = luaL_optstring(L, 3, "A4");
  bool landscape = lua_toboolean(L, 4);

  if (!g_current_doc) {
    lua_pushboolean(L, 0);
    return 1;
  }

  ExportOptions opts;
  export_options_init(&opts);
  strncpy(opts.title, title, sizeof(opts.title) - 1);
  strncpy(opts.pdf_page_size, page_size, sizeof(opts.pdf_page_size) - 1);
  opts.pdf_landscape = landscape;

  bool ok = export_to_pdf(g_current_doc, filename, &opts);
  lua_pushboolean(L, ok);
  return 1;
}

// =============================================================================
// RELOAD FUNCTION
// =============================================================================

static int l_reload(lua_State *L) {
  clear_tracked_scripts();
  scripting_load_plugins(L);
  return 0;
}

// =============================================================================
// REGISTRATION
// =============================================================================

static const struct luaL_Reg aslan_lib[] = {
    // Document commands
    {"execute", l_execute},
    {"execute_volatile", l_execute_volatile},

    // Node creation
    {"create_rect", l_create_rect},
    {"create_frame", l_create_frame},
    {"create_group", l_create_group},
    {"group_attach", l_group_attach},
    {"create_text_block", l_create_text_block},

    // Node queries
    {"get_node_at", l_get_node_at},
    {"get_node_rect", l_get_node_rect},
    {"get_node_type", l_get_node_type},
    {"get_node_name", l_get_node_name},
    {"get_node_text", l_get_node_text},
    {"get_all_nodes", l_get_all_nodes},
    {"get_children", l_get_children},

    // Node manipulation
    {"set_node_pos", l_set_node_pos},
    {"set_node_size", l_set_node_size},
    {"select_node", l_select_node},

    // Selection & tools
    {"get_selected_id", l_get_selected_id},
    {"set_tool_mode", l_set_tool_mode},
    {"get_tool_mode", l_get_tool_mode},

    // Pages
    {"get_active_page_id", l_get_active_page_id},
    {"get_page_type", l_get_page_type},
    {"get_page_meta", l_get_page_meta},
    {"set_page_meta", l_set_page_meta},

    // Input
    {"get_mouse_x", l_get_mouse_x},
    {"get_mouse_y", l_get_mouse_y},
    {"get_key_mods", l_get_key_mods},
    {"get_time", l_get_time},

    // Grid
    {"grid_get", l_aslan_grid_get},
    {"grid_set", l_aslan_grid_set},
    {"grid_node_get", l_grid_node_get},
    {"grid_node_set", l_grid_node_set},

    // Node Behaviors (Phase 2)
    {"set_behavior", l_set_behavior},
    {"remove_behavior", l_remove_behavior},
    {"get_behavior", l_get_behavior},

    // Mini-App Architecture (Phase 2)
    {"register_app", l_register_app},

    // Export (Phase 3)
    {"export_svg", l_export_svg},
    {"export_pdf", l_export_pdf},

    // Utility
    {"log", l_log_info},
    {"reload", l_reload},

    {NULL, NULL}};

int luaopen_aslan(lua_State *L) {
  luaL_newlib(L, aslan_lib);

  // Node type constants
  lua_pushinteger(L, NODE_TYPE_ROOT);
  lua_setfield(L, -2, "NODE_ROOT");
  lua_pushinteger(L, NODE_TYPE_RECT);
  lua_setfield(L, -2, "NODE_RECT");
  lua_pushinteger(L, NODE_TYPE_TEXT);
  lua_setfield(L, -2, "NODE_TEXT");
  lua_pushinteger(L, NODE_TYPE_IMAGE);
  lua_setfield(L, -2, "NODE_IMAGE");
  lua_pushinteger(L, NODE_TYPE_FRAME);
  lua_setfield(L, -2, "NODE_FRAME");
  lua_pushinteger(L, NODE_TYPE_GROUP);
  lua_setfield(L, -2, "NODE_GROUP");
  lua_pushinteger(L, NODE_TYPE_NOTE);
  lua_setfield(L, -2, "NODE_NOTE");
  lua_pushinteger(L, NODE_TYPE_GRID);
  lua_setfield(L, -2, "NODE_GRID");
  lua_pushinteger(L, NODE_TYPE_CLIP);
  lua_setfield(L, -2, "NODE_CLIP");

  return 1;
}

// =============================================================================
// LIFECYCLE
// =============================================================================

lua_State *scripting_init(Engine *engine) {
  g_current_engine = engine;
  g_current_doc = engine->doc;

  // Clear tracked scripts and behaviors
  clear_tracked_scripts();
  g_behavior_count = 0;
  memset(g_node_behaviors, 0, sizeof(g_node_behaviors));

  lua_State *L = luaL_newstate();
  luaL_openlibs(L);

  // Register 'aslan' module globally
  luaL_requiref(L, "aslan", luaopen_aslan, 1);
  lua_pop(L, 1);

  // Register 'nk' module
  luaL_requiref(L, "nk", luaopen_nk, 1);
  lua_pop(L, 1);

  // Initialize global tables for behaviors and apps
  lua_newtable(L);
  lua_setglobal(L, "behaviors");

  lua_newtable(L);
  lua_setglobal(L, "aslan_app_registry");

  if (engine->ui_host) {
    scripting_nk_set_context(engine->ui_host->ctx);
  }

  return L;
}

void scripting_shutdown(lua_State *L) {
  if (L)
    lua_close(L);
  g_current_engine = NULL;
  g_current_doc = NULL;
  clear_tracked_scripts();
}

int scripting_run_file(lua_State *L, const char *filepath) {
  char resolved[PATH_MAX];
  resolve_script_path(resolved, filepath);

  // Track for hot-reload
  track_script(resolved);

  if (luaL_dofile(L, resolved) != LUA_OK) {
    const char *error = lua_tostring(L, -1);
    fprintf(stderr, "[LUA ERROR] %s\n", error);
    lua_pop(L, 1);
    return 0;
  }
  return 1;
}

void scripting_call_event(lua_State *L, const char *func_name, float x,
                          float y) {
  if (!L)
    return;

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

int scripting_load_plugins(lua_State *L) {
  if (!L)
    return 0;

  char scripts_path[PATH_MAX];
  if (g_script_base_dir[0] != '\0') {
    snprintf(scripts_path, PATH_MAX, "%s/scripts", g_script_base_dir);
  } else {
    strncpy(scripts_path, "scripts", PATH_MAX - 1);
  }

  DIR *dir = opendir(scripts_path);
  if (!dir) {
    fprintf(stderr, "[WARN] Could not open scripts directory: %s\n",
            scripts_path);
    return 0;
  }

  struct dirent *ent;
  int count = 0;
  while ((ent = readdir(dir)) != NULL) {
    // Skip subdirectories and hidden files
    if (ent->d_name[0] == '.')
      continue;

    // Only load .lua files
    const char *ext = strrchr(ent->d_name, '.');
    if (ext && strcmp(ext, ".lua") == 0) {
      char full_path[PATH_MAX];
      snprintf(full_path, PATH_MAX, "scripts/%s", ent->d_name);
      printf(">> Loading plugin: %s\n", full_path);
      if (scripting_run_file(L, full_path)) {
        count++;
      }
    }
  }
  closedir(dir);

  // Also load apps from scripts/apps/ directory
  char apps_path[PATH_MAX];
  snprintf(apps_path, PATH_MAX, "%s/apps", scripts_path);
  dir = opendir(apps_path);
  if (dir) {
    while ((ent = readdir(dir)) != NULL) {
      if (ent->d_name[0] == '.')
        continue;
      const char *ext = strrchr(ent->d_name, '.');
      if (ext && strcmp(ext, ".lua") == 0) {
        char full_path[PATH_MAX];
        snprintf(full_path, PATH_MAX, "scripts/apps/%s", ent->d_name);
        printf(">> Loading app: %s\n", full_path);
        if (scripting_run_file(L, full_path)) {
          count++;
        }
      }
    }
    closedir(dir);
  }

  return count;
}

void scripting_call_key_event(lua_State *L, const char *func_name, int key,
                              int mods) {
  if (!L)
    return;

  lua_getglobal(L, func_name);
  if (lua_isfunction(L, -1)) {
    lua_pushinteger(L, key);
    lua_pushinteger(L, mods);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
      fprintf(stderr, "[LUA EVENT ERROR] %s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  } else {
    lua_pop(L, 1);
  }
}

void scripting_call_ui_render(lua_State *L) {
  if (!L)
    return;

  // Hot-reload check
  scripting_hot_reload_check(L);

  // Call global on_ui() for plugins
  lua_getglobal(L, "on_ui");
  if (lua_isfunction(L, -1)) {
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
      fprintf(stderr, "[LUA UI ERROR] %s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  } else {
    lua_pop(L, 1);
  }
}

// =============================================================================
// EXPORTED FUNCTIONS FOR RENDERER INTEGRATION
// =============================================================================

// Check if we should use Lua to render this node type
bool scripting_should_render_node(lua_State *L, Node *node) {
  if (!L || !node)
    return false;
  return scripting_has_app_renderer(L, (int)node->type);
}

// Update all node behaviors (called each frame)
void scripting_update_behaviors(lua_State *L, float dt) {
  if (!L)
    return;

  for (int i = 0; i < g_behavior_count; i++) {
    if (!g_node_behaviors[i].active)
      continue;
    scripting_node_on_update(L, g_node_behaviors[i].node_id, dt);
  }
}

#endif
