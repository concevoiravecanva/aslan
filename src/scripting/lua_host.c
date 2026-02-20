#include "lua_host.h"
#include "../core/engine.h"
#include "../document/aslan_table.h"
#include "../platform/ui_shared.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
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
  ASLAN_WARN("Lua support disabled (NO_LUA defined)");
  return (lua_State *)1;
}

void scripting_shutdown(lua_State *L) { (void)L; }

int scripting_run_file(lua_State *L, const char *filepath) {
  (void)L;
  ASLAN_WARN("Cannot run script '%s': Lua disabled", filepath);
  return 0;
}

int scripting_run_code(lua_State *L, void *lua_data) {
  (void)L;
  (void)lua_data;
  return 0;
}

int scripting_run_visual(lua_State *L, void *lua_data) {
  (void)L;
  (void)lua_data;
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
char lua_output_buffer[4096] = "";

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
    ASLAN_WARN("Maximum tracked scripts reached");
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
      ASLAN_INFO("Hot-reload: %s", g_tracked_scripts[i].path);

      if (luaL_dofile(L, g_tracked_scripts[i].path) != LUA_OK) {
        ASLAN_ERROR("Lua hot-reload error: %s", lua_tostring(L, -1));
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
    ASLAN_ERROR("Lua %s error: %s", func_name, lua_tostring(L, -1));
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
    ASLAN_ERROR("Lua %s error: %s", func_name, lua_tostring(L, -1));
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
      block_table_init(block);
      break;
    case BLOCK_DESIGN:
      block_design_init(block);
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

// (Canvas Lua bindings removed — Canvas block type has been deleted)

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

// lua: Aslan.get_cell(row, col)
static int l_notebook_get_cell(lua_State *L) {
  int row = (int)luaL_checkinteger(L, 1);
  int col = (int)luaL_checkinteger(L, 2);

  Page *page = doc_page_get_active(g_current_doc);
  if (!page)
    return 0;

  // Find first table on page
  Block *tbl_block = NULL;
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == BLOCK_TABLE) {
      tbl_block = b;
      break;
    }
  }

  if (tbl_block && tbl_block->data.table.columnar) {
    AslanTable *tbl = tbl_block->data.table.columnar;
    if (row >= 0 && (size_t)row < aslan_table_view_count(tbl) && col >= 0 &&
        (size_t)col < tbl->column_count) {
      uint64_t row_id = aslan_table_view_row_id(tbl, row);
      char buf[ASLAN_TABLE_MAX_STRING_LEN];
      aslan_table_get_cell_display(tbl, col, row_id, buf, sizeof(buf));
      lua_pushstring(L, buf);
      return 1;
    }
  }
  return 0;
}

// Redirige la fonction print de Lua vers le buffer global
static int l_aslan_print(lua_State *L) {
  int n = lua_gettop(L);
  if (n == 0) {
    strncat(lua_output_buffer, "\n",
            sizeof(lua_output_buffer) - strlen(lua_output_buffer) - 1);
    ASLAN_DEBUG("Lua print (empty)");
    return 0;
  }

  // Build the message for the global buffer
  char temp_msg_buffer[4096] = {
      0}; // Temporary buffer for the current print line
  size_t current_temp_len = 0;

  for (int i = 1; i <= n; i++) {
    size_t len;
    const char *s = luaL_tolstring(L, i, &len); // Converts any value to string
    if (s == NULL) {
      s = "nil";
      len = 3;
    }

    if (i > 1) {
      if (current_temp_len + 1 < sizeof(temp_msg_buffer)) {
        strncat(temp_msg_buffer, "\t",
                sizeof(temp_msg_buffer) - current_temp_len - 1);
        current_temp_len++;
      }
    }
    if (current_temp_len + len < sizeof(temp_msg_buffer)) {
      strncat(temp_msg_buffer, s,
              sizeof(temp_msg_buffer) - current_temp_len - 1);
      current_temp_len += len;
    }
    lua_pop(L, 1);
  }

  // Concatène dans le buffer global (limité à 4096)
  strncat(lua_output_buffer, temp_msg_buffer,
          sizeof(lua_output_buffer) - strlen(lua_output_buffer) - 1);
  strncat(lua_output_buffer, "\n",
          sizeof(lua_output_buffer) - strlen(lua_output_buffer) - 1);

  ASLAN_DEBUG("Lua print: %s", temp_msg_buffer);

  return 0;
}

// =============================================================================
// Registration
// =============================================================================

static const luaL_Reg aslan_funcs[] = {
    // Core
    {"execute", l_execute},
    {"print", l_aslan_print},

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

    // Globals
    {"set_global", l_set_global},
    {"get_global", l_get_global},

    {NULL, NULL}};

// =============================================================================
// Initialization
// =============================================================================

lua_State *aslan_lua_init(Engine *engine) {
  lua_State *L = luaL_newstate();
  if (!L)
    return NULL;

  // Sandbox: Load only safe libraries
  luaL_requiref(L, "_G", luaopen_base, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "table", luaopen_table, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "string", luaopen_string, 1);
  lua_pop(L, 1);
  luaL_requiref(L, "math", luaopen_math, 1);
  lua_pop(L, 1);

  // Register Aslan global table
  static const luaL_Reg aslan_lib[] = {{"get_cell", l_notebook_get_cell},
                                       {NULL, NULL}};
  luaL_newlib(L, aslan_lib);
  lua_setglobal(L, "Aslan");

  // Redirect print standard
  lua_register(L, "print", l_aslan_print);
  g_current_engine = engine;
  if (engine)
    g_current_doc = engine->doc;

  ASLAN_INFO("Lua notebook sandbox initialized");
  return L;
}

lua_State *scripting_init(Engine *engine) {
  lua_State *L = luaL_newstate();
  if (!L) {
    ASLAN_ERROR("Failed to create Lua state");
    return NULL;
  }

  luaL_openlibs(L);

  // Register aslan module
  lua_newtable(L);
  luaL_setfuncs(L, aslan_funcs, 0);
  lua_setglobal(L, "aslan");

  // CRITICAL: Redirect standard print to our buffer bridge
  lua_register(L, "print", l_aslan_print);
  // Store engine reference
  g_current_engine = engine;
  if (engine) {
    g_current_doc = engine->doc;
  }

  // Load formula evaluation script
  if (luaL_dofile(L, "scripts/formula_eval.lua") != LUA_OK) {
    ASLAN_WARN("Failed to load formula_eval.lua: %s", lua_tostring(L, -1));
    lua_pop(L, 1);
  } else {
    ASLAN_INFO("Loaded scripts/formula_eval.lua");
  }

  ASLAN_INFO("Lua scripting initialized");
  return L;
}

void scripting_shutdown(lua_State *L) {
  if (L && L != (lua_State *)1) {
    lua_close(L);
  }
  g_current_doc = NULL;
  g_current_engine = NULL;
  g_tracked_count = 0;
  ASLAN_INFO("Lua scripting shutdown");
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
    ASLAN_ERROR("Lua load error %s: %s", filepath, lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
  }

  track_script(resolved);
  ASLAN_INFO("Lua loaded: %s", filepath);
  return 1;
}

int scripting_load_plugins(lua_State *L) {
  if (!L || L == (lua_State *)1)
    return 0;

  const char *plugin_dir = "scripts/plugins";
  DIR *dir = opendir(plugin_dir);
  if (!dir) {
    ASLAN_DEBUG("No plugins directory found at %s", plugin_dir);
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
  ASLAN_INFO("Loaded %d Lua plugins", loaded);
  return loaded;
}

#endif // NO_LUA
