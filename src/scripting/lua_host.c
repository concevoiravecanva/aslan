#include "lua_host.h"
#include "../core/engine.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// =============================================================================
// ASLAN LUA HOST v3 - Block-Based Document Scripting
// =============================================================================

#ifdef NO_LUA

// Stub implementation when Lua is disabled
lua_State *scripting_init(Engine *engine) {
  (void)engine;
  printf("[WARN] Lua support disabled (NO_LUA defined).\n");
  return (lua_State *)1;
}

void scripting_shutdown(lua_State *L) { (void)L; }

int scripting_run_file(lua_State *L, const char *filepath) {
  (void)L;
  printf("[WARN] Cannot run script '%s': Lua disabled.\n", filepath);
  return 0;
}

void scripting_set_document(Document *doc) { (void)doc; }
void scripting_set_engine(Engine *engine) { (void)engine; }
int scripting_load_plugins(lua_State *L) {
  (void)L;
  return 0;
}
void scripting_hot_reload_check(lua_State *L) { (void)L; }

#else

// =============================================================================
// Global State
// =============================================================================

static Document *g_current_doc = NULL;
static Engine *g_current_engine = NULL;
static char g_script_base_dir[PATH_MAX] = {0};

// =============================================================================
// Hot-Reload System
// =============================================================================

#define MAX_TRACKED_SCRIPTS 64

typedef struct {
  char path[PATH_MAX];
  time_t last_modified;
  bool active;
} TrackedScript;

static TrackedScript g_tracked_scripts[MAX_TRACKED_SCRIPTS];
static int g_tracked_count = 0;

static time_t get_file_mtime(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    return st.st_mtime;
  }
  return 0;
}

static int track_script(const char *path) {
  for (int i = 0; i < g_tracked_count; i++) {
    if (g_tracked_scripts[i].active &&
        strcmp(g_tracked_scripts[i].path, path) == 0) {
      g_tracked_scripts[i].last_modified = get_file_mtime(path);
      return i;
    }
  }

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

void scripting_hot_reload_check(lua_State *L) {
  if (!L)
    return;

  static int check_counter = 0;
  check_counter++;
  if (check_counter < 60)
    return;
  check_counter = 0;

  for (int i = 0; i < g_tracked_count; i++) {
    if (!g_tracked_scripts[i].active)
      continue;

    time_t current_mtime = get_file_mtime(g_tracked_scripts[i].path);
    if (current_mtime > g_tracked_scripts[i].last_modified) {
      printf("[HOT-RELOAD] Reloading: %s\n", g_tracked_scripts[i].path);

      if (luaL_dofile(L, g_tracked_scripts[i].path) != LUA_OK) {
        fprintf(stderr, "[LUA ERROR] %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
      }

      g_tracked_scripts[i].last_modified = current_mtime;
    }
  }
}

// =============================================================================
// Setters
// =============================================================================

void scripting_set_document(Document *doc) { g_current_doc = doc; }

void scripting_set_engine(Engine *engine) {
  g_current_engine = engine;
  if (engine) {
    g_current_doc = engine->doc;
  }
}

void scripting_set_base_dir(const char *base_dir) {
  if (base_dir) {
    strncpy(g_script_base_dir, base_dir, PATH_MAX - 1);
    g_script_base_dir[PATH_MAX - 1] = '\0';
  } else {
    g_script_base_dir[0] = '\0';
  }
}

// Mouse/key state for Lua tools
static float g_mouse_screen_x = 0.0f;
static float g_mouse_screen_y = 0.0f;
static int g_key_mods = 0;

void scripting_set_mouse_screen(float sx, float sy) {
  g_mouse_screen_x = sx;
  g_mouse_screen_y = sy;
}

void scripting_set_key_mods(int mods) { g_key_mods = mods; }

void scripting_call_event(lua_State *L, const char *func_name, float x,
                          float y) {
  if (!L || L == (lua_State *)1 || !func_name)
    return;

  lua_getglobal(L, func_name);
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  lua_pushnumber(L, x);
  lua_pushnumber(L, y);

  if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
    fprintf(stderr, "[LUA] %s error: %s\n", func_name, lua_tostring(L, -1));
    lua_pop(L, 1);
  }
}

void scripting_call_key_event(lua_State *L, const char *func_name, int key,
                              int mods) {
  if (!L || L == (lua_State *)1 || !func_name)
    return;

  lua_getglobal(L, func_name);
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  lua_pushinteger(L, key);
  lua_pushinteger(L, mods);

  if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
    fprintf(stderr, "[LUA] %s error: %s\n", func_name, lua_tostring(L, -1));
    lua_pop(L, 1);
  }
}

// =============================================================================
// Lua Bindings: Command Execution
// =============================================================================

// lua: aslan.execute(command) -> bool
static int l_execute(lua_State *L) {
  const char *cmd = luaL_checkstring(L, 1);
  if (!g_current_engine) {
    lua_pushboolean(L, 0);
    return 1;
  }

  bool success = engine_execute_command(g_current_engine, cmd);
  lua_pushboolean(L, success);
  return 1;
}

// =============================================================================
// Lua Bindings: Document
// =============================================================================

// lua: aslan.get_page_count() -> int
static int l_get_page_count(lua_State *L) {
  if (!g_current_doc) {
    lua_pushinteger(L, 0);
    return 1;
  }
  lua_pushinteger(L, g_current_doc->page_count);
  return 1;
}

// lua: aslan.get_active_page_id() -> int
static int l_get_active_page_id(lua_State *L) {
  if (!g_current_doc || g_current_doc->active_page_idx < 0) {
    lua_pushinteger(L, 0);
    return 1;
  }
  Page *page = &g_current_doc->pages[g_current_doc->active_page_idx];
  lua_pushinteger(L, page->id);
  return 1;
}

// lua: aslan.get_page_name(page_id) -> string or nil
static int l_get_page_name(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_doc)
    return 0;

  Page *page = doc_page_get(g_current_doc, page_id);
  if (page) {
    lua_pushstring(L, page->name);
    return 1;
  }
  return 0;
}

// lua: aslan.create_page(name) -> page_id
static int l_create_page(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  if (!g_current_doc) {
    lua_pushinteger(L, 0);
    return 1;
  }

  Page *page = doc_page_create(g_current_doc, name);
  lua_pushinteger(L, page ? page->id : 0);
  return 1;
}

// lua: aslan.activate_page(page_id)
static int l_activate_page(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  if (g_current_doc) {
    doc_page_activate(g_current_doc, page_id);
  }
  return 0;
}

// =============================================================================
// Lua Bindings: Blocks
// =============================================================================

// lua: aslan.get_block_count(page_id) -> int
static int l_get_block_count(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  if (!g_current_doc) {
    lua_pushinteger(L, 0);
    return 1;
  }

  Page *page = doc_page_get(g_current_doc, page_id);
  lua_pushinteger(L, page ? page->block_count : 0);
  return 1;
}

// lua: aslan.create_block(page_id, type_name) -> block_id
static int l_create_block(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  const char *type_name = luaL_checkstring(L, 2);

  if (!g_current_doc) {
    lua_pushinteger(L, 0);
    return 1;
  }

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page) {
    lua_pushinteger(L, 0);
    return 1;
  }

  BlockType type = block_type_from_string(type_name);
  Block *block = block_create(page, type);

  if (block) {
    // Initialize type-specific data
    switch (type) {
    case BLOCK_TABLE:
      block_table_init(block, 3, 3);
      break;
    case BLOCK_CANVAS:
      block_canvas_init(block);
      break;
    case BLOCK_DESIGN:
      block_design_init(block, 800, 600);
      break;
    case BLOCK_VIDEO:
      block_video_init(block);
      break;
    case BLOCK_LUA:
      block_lua_init(block);
      break;
    default:
      break;
    }
    page_append_block(page, block);
    lua_pushinteger(L, block->id);
  } else {
    lua_pushinteger(L, 0);
  }
  return 1;
}

// lua: aslan.set_block_text(page_id, block_id, text)
static int l_set_block_text(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t block_id = (uint32_t)luaL_checkinteger(L, 2);
  const char *text = luaL_checkstring(L, 3);

  if (!g_current_doc)
    return 0;

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page)
    return 0;

  Block *block = page_find_block_by_id(page, block_id);
  if (block) {
    block_set_text(block, text);
  }
  return 0;
}

// lua: aslan.get_block_text(page_id, block_id) -> string or nil
static int l_get_block_text(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t block_id = (uint32_t)luaL_checkinteger(L, 2);

  if (!g_current_doc)
    return 0;

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page)
    return 0;

  Block *block = page_find_block_by_id(page, block_id);
  if (block && block->text) {
    lua_pushstring(L, block->text);
    return 1;
  }
  return 0;
}

// lua: aslan.delete_block(page_id, block_id) -> bool
static int l_delete_block(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t block_id = (uint32_t)luaL_checkinteger(L, 2);

  if (!g_current_doc) {
    lua_pushboolean(L, 0);
    return 1;
  }

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page) {
    lua_pushboolean(L, 0);
    return 1;
  }

  Block *block = page_find_block_by_id(page, block_id);
  if (block) {
    page_remove_block(page, block);
    block_destroy(block);
    lua_pushboolean(L, 1);
  } else {
    lua_pushboolean(L, 0);
  }
  return 1;
}

// =============================================================================
// Lua Bindings: Table Blocks
// =============================================================================

// lua: aslan.table_set_cell(page_id, block_id, row, col, text)
static int l_table_set_cell(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t block_id = (uint32_t)luaL_checkinteger(L, 2);
  int row = (int)luaL_checkinteger(L, 3);
  int col = (int)luaL_checkinteger(L, 4);
  const char *text = luaL_checkstring(L, 5);

  if (!g_current_doc)
    return 0;

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page)
    return 0;

  Block *block = page_find_block_by_id(page, block_id);
  if (block && block->type == BLOCK_TABLE) {
    block_table_set_cell(block, row, col, text);
  }
  return 0;
}

// lua: aslan.table_get_cell(page_id, block_id, row, col) -> string or nil
static int l_table_get_cell(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t block_id = (uint32_t)luaL_checkinteger(L, 2);
  int row = (int)luaL_checkinteger(L, 3);
  int col = (int)luaL_checkinteger(L, 4);

  if (!g_current_doc)
    return 0;

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page)
    return 0;

  Block *block = page_find_block_by_id(page, block_id);
  if (block && block->type == BLOCK_TABLE) {
    const char *cell = block_table_get_cell(block, row, col);
    if (cell) {
      lua_pushstring(L, cell);
      return 1;
    }
  }
  return 0;
}

// =============================================================================
// Lua Bindings: Canvas Blocks
// =============================================================================

// lua: aslan.canvas_create_node(page_id, block_id, type, name) -> node_id
static int l_canvas_create_node(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t block_id = (uint32_t)luaL_checkinteger(L, 2);
  const char *type_str = luaL_checkstring(L, 3);
  const char *name = luaL_checkstring(L, 4);

  if (!g_current_doc) {
    lua_pushinteger(L, 0);
    return 1;
  }

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page) {
    lua_pushinteger(L, 0);
    return 1;
  }

  Block *block = page_find_block_by_id(page, block_id);
  if (block && block->type == BLOCK_CANVAS) {
    NodeType type = node_type_from_string(type_str);
    Node *node = block_canvas_create_node(block, type, name);
    lua_pushinteger(L, node ? node->id : 0);
  } else {
    lua_pushinteger(L, 0);
  }
  return 1;
}

// lua: aslan.canvas_set_node_pos(page_id, block_id, node_id, x, y)
static int l_canvas_set_node_pos(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t block_id = (uint32_t)luaL_checkinteger(L, 2);
  uint32_t node_id = (uint32_t)luaL_checkinteger(L, 3);
  float x = (float)luaL_checknumber(L, 4);
  float y = (float)luaL_checknumber(L, 5);

  if (!g_current_doc)
    return 0;

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page)
    return 0;

  Block *block = page_find_block_by_id(page, block_id);
  if (block && block->type == BLOCK_CANVAS) {
    Node *node = block_canvas_find_node(block, node_id);
    if (node) {
      node_set_position(node, x, y);
    }
  }
  return 0;
}

// lua: aslan.canvas_set_node_size(page_id, block_id, node_id, w, h)
static int l_canvas_set_node_size(lua_State *L) {
  uint32_t page_id = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t block_id = (uint32_t)luaL_checkinteger(L, 2);
  uint32_t node_id = (uint32_t)luaL_checkinteger(L, 3);
  float w = (float)luaL_checknumber(L, 4);
  float h = (float)luaL_checknumber(L, 5);

  if (!g_current_doc)
    return 0;

  Page *page = doc_page_get(g_current_doc, page_id);
  if (!page)
    return 0;

  Block *block = page_find_block_by_id(page, block_id);
  if (block && block->type == BLOCK_CANVAS) {
    Node *node = block_canvas_find_node(block, node_id);
    if (node) {
      node_set_size(node, w, h);
    }
  }
  return 0;
}

// =============================================================================
// Lua Bindings: Globals
// =============================================================================

// lua: aslan.set_global(key, value)
static int l_set_global(lua_State *L) {
  const char *key = luaL_checkstring(L, 1);
  const char *value = luaL_checkstring(L, 2);

  if (g_current_doc) {
    doc_set_global(g_current_doc, key, value);
  }
  return 0;
}

// lua: aslan.get_global(key) -> string or nil
static int l_get_global(lua_State *L) {
  const char *key = luaL_checkstring(L, 1);

  if (g_current_doc) {
    const char *value = doc_get_global(g_current_doc, key);
    if (value) {
      lua_pushstring(L, value);
      return 1;
    }
  }
  return 0;
}

// =============================================================================
// Lua Bindings: Utility
// =============================================================================

// lua: aslan.print(msg)
static int l_print(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  printf("[LUA] %s\n", msg);
  return 0;
}

// =============================================================================
// Registration
// =============================================================================

static const luaL_Reg aslan_funcs[] = {
    // Core
    {"execute", l_execute},
    {"print", l_print},

    // Document
    {"get_page_count", l_get_page_count},
    {"get_active_page_id", l_get_active_page_id},
    {"get_page_name", l_get_page_name},
    {"create_page", l_create_page},
    {"activate_page", l_activate_page},

    // Blocks
    {"get_block_count", l_get_block_count},
    {"create_block", l_create_block},
    {"set_block_text", l_set_block_text},
    {"get_block_text", l_get_block_text},
    {"delete_block", l_delete_block},

    // Table blocks
    {"table_set_cell", l_table_set_cell},
    {"table_get_cell", l_table_get_cell},

    // Canvas blocks
    {"canvas_create_node", l_canvas_create_node},
    {"canvas_set_node_pos", l_canvas_set_node_pos},
    {"canvas_set_node_size", l_canvas_set_node_size},

    // Globals
    {"set_global", l_set_global},
    {"get_global", l_get_global},

    {NULL, NULL}};

// =============================================================================
// Initialization
// =============================================================================

lua_State *scripting_init(Engine *engine) {
  lua_State *L = luaL_newstate();
  if (!L) {
    fprintf(stderr, "[ERROR] Failed to create Lua state\n");
    return NULL;
  }

  luaL_openlibs(L);

  // Register aslan module
  lua_newtable(L);
  luaL_setfuncs(L, aslan_funcs, 0);
  lua_setglobal(L, "aslan");

  // Store engine reference
  g_current_engine = engine;
  if (engine) {
    g_current_doc = engine->doc;
  }

  // Load formula evaluation script
  if (luaL_dofile(L, "scripts/formula_eval.lua") != LUA_OK) {
    printf("[WARN] Failed to load formula_eval.lua: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
  } else {
    printf("[LUA] Loaded scripts/formula_eval.lua\n");
  }

  printf("[LUA] Scripting initialized\n");
  return L;
}

void scripting_shutdown(lua_State *L) {
  if (L && L != (lua_State *)1) {
    lua_close(L);
  }
  g_current_doc = NULL;
  g_current_engine = NULL;
  g_tracked_count = 0;
  printf("[LUA] Scripting shutdown\n");
}

int scripting_run_file(lua_State *L, const char *filepath) {
  if (!L || L == (lua_State *)1)
    return 0;

  char resolved[PATH_MAX];
  if (filepath[0] == '/') {
    strncpy(resolved, filepath, PATH_MAX - 1);
  } else if (g_script_base_dir[0]) {
    snprintf(resolved, PATH_MAX, "%s/%s", g_script_base_dir, filepath);
  } else {
    strncpy(resolved, filepath, PATH_MAX - 1);
  }

  if (luaL_dofile(L, resolved) != LUA_OK) {
    fprintf(stderr, "[LUA ERROR] %s: %s\n", filepath, lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
  }

  track_script(resolved);
  printf("[LUA] Loaded: %s\n", filepath);
  return 1;
}

int scripting_load_plugins(lua_State *L) {
  if (!L || L == (lua_State *)1)
    return 0;

  const char *plugin_dir = "scripts/plugins";
  DIR *dir = opendir(plugin_dir);
  if (!dir) {
    printf("[LUA] No plugins directory found at %s\n", plugin_dir);
    return 0;
  }

  int loaded = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type != DT_REG)
      continue;

    size_t len = strlen(entry->d_name);
    if (len < 5 || strcmp(entry->d_name + len - 4, ".lua") != 0)
      continue;

    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s/%s", plugin_dir, entry->d_name);

    if (scripting_run_file(L, path)) {
      loaded++;
    }
  }

  closedir(dir);
  printf("[LUA] Loaded %d plugins\n", loaded);
  return loaded;
}

#endif // NO_LUA
