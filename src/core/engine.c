#include "engine.h"
#include "../document/aslan_table.h"
#include "../platform/ui_host.h"
#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// =============================================================================
// ASLAN ENGINE v3 - Block-Based AOF Command Processor
// =============================================================================

// Notify scripting host of document changes
void scripting_set_document(Document *doc);

// -----------------------------------------------------------------------------
// AOF Tokenizer
// -----------------------------------------------------------------------------

typedef struct {
  char *argv[32];
  int argc;
  char _raw[2048];
} AofCommand;

static char *unescape_string(const char *src, char *dst, size_t dst_size) {
  if (!src || !dst || dst_size == 0)
    return dst;

  size_t di = 0;
  for (size_t si = 0; src[si] && di + 1 < dst_size; si++) {
    if (src[si] == '\\' && src[si + 1]) {
      si++;
      switch (src[si]) {
      case 'n':
        dst[di++] = '\n';
        break;
      case 'r':
        dst[di++] = '\r';
        break;
      case 't':
        dst[di++] = '\t';
        break;
      case '"':
        dst[di++] = '"';
        break;
      case '\\':
        dst[di++] = '\\';
        break;
      default:
        dst[di++] = src[si];
        break;
      }
    } else {
      dst[di++] = src[si];
    }
  }
  dst[di] = '\0';
  return dst;
}

static AofCommand aof_tokenize(const char *line) {
  AofCommand cmd;
  memset(&cmd, 0, sizeof(cmd));
  if (!line || !*line)
    return cmd;

  strncpy(cmd._raw, line, sizeof(cmd._raw) - 1);
  char *p = cmd._raw;
  bool in_quotes = false;
  char *token_start = NULL;

  while (*p) {
    if (*p == '"') {
      if (in_quotes) {
        *p = '\0';
        if (token_start && cmd.argc < 32) {
          cmd.argv[cmd.argc++] = token_start;
        }
        token_start = NULL;
        in_quotes = false;
      } else {
        in_quotes = true;
        token_start = p + 1;
      }
    } else if (!in_quotes && (*p == ' ' || *p == '\t')) {
      if (token_start) {
        *p = '\0';
        if (cmd.argc < 32) {
          cmd.argv[cmd.argc++] = token_start;
        }
        token_start = NULL;
      }
    } else {
      if (!token_start) {
        token_start = p;
      }
    }
    p++;
  }

  if (token_start && cmd.argc < 32) {
    cmd.argv[cmd.argc++] = token_start;
  }

  return cmd;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Path handling
// -----------------------------------------------------------------------------

void engine_set_document_path(Engine *engine, const char *new_path) {
  if (!engine || !new_path)
    return;

  // Si un fichier est déjà ouvert, on le ferme proprement avant de changer de
  // chemin
  bool was_open = (engine->history_file != NULL);
  if (was_open) {
    fclose(engine->history_file);
    engine->history_file = NULL;
  }

  // 1. Stocker le chemin de base
  strncpy(engine->doc_path, new_path, sizeof(engine->doc_path) - 1);
  engine->doc_path[sizeof(engine->doc_path) - 1] = '\0';

  // 2. Construire les chemins absolus vers les fichiers de log
  snprintf(engine->history_path, sizeof(engine->history_path), "%s/history.log",
           engine->doc_path);
  snprintf(engine->redo_path, sizeof(engine->redo_path), "%s/redo.log",
           engine->doc_path);

  // 3. Créer le répertoire s'il n'existe pas (MacOS/Linux)
  mkdir(engine->doc_path, 0755);

  // 4. Si on avait un fichier ouvert, on le réouvre sur le nouveau chemin
  if (was_open) {
    engine->history_file = fopen(engine->history_path, "a");
    if (engine->history_file) {
      // Si on vient de "." vers autre chose, on peut le noter,
      // mais si c'est l'init de main.c (argc >= 2), on veut rester discret.
      if (strcmp(engine->doc_path, ".") != 0) {
        fprintf(engine->history_file, "-- Document path set to %s --\n",
                engine->doc_path);
        fflush(engine->history_file);
      }
    }
  }
}

static void clear_history_log(Engine *engine) {
  FILE *f = fopen(engine->history_path, "w");
  if (f)
    fclose(f);
}

static void clear_redo_log(Engine *engine) {
  FILE *f = fopen(engine->redo_path, "w");
  if (f)
    fclose(f);
}

// -----------------------------------------------------------------------------
// AOF Emission
// -----------------------------------------------------------------------------

static void engine_emit_command(Engine *engine, const char *line) {
  if (!engine || !engine->history_file || !line)
    return;

  // Do not re-emit commands during document loading
  if (engine->is_loading)
    return;

  fprintf(engine->history_file, "%s\n", line);
  fflush(engine->history_file);
}

__attribute__((unused)) static void escape_and_emit(Engine *engine,
                                                    const char *fmt, ...) {
  if (!engine || !engine->history_file)
    return;

  char buf[2048];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  engine_emit_command(engine, buf);
}

// -----------------------------------------------------------------------------
// Comment/Empty Line Detection
// -----------------------------------------------------------------------------

static bool is_comment_or_empty(const char *line) {
  if (!line)
    return true;
  while (*line == ' ' || *line == '\t')
    line++;
  if (*line == '\0' || *line == '\n' || *line == '\r')
    return true;
  if (*line == '-' && *(line + 1) == '-')
    return true;
  return false;
}

static void strip_trailing_newlines(char *s) {
  if (!s)
    return;
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
    s[n - 1] = '\0';
    n--;
  }
}

// -----------------------------------------------------------------------------
// Engine Lifecycle
// -----------------------------------------------------------------------------

Engine *engine_create(void) {
  Engine *engine = (Engine *)calloc(1, sizeof(Engine));
  if (!engine)
    return NULL;

  engine->doc = doc_create();
  engine->is_running = true;

  // Default path is current directory
  engine_set_document_path(engine, ".");

  engine->history_file = fopen(engine->history_path, "a");
  if (engine->history_file) {
    fprintf(engine->history_file, "-- Session started --\n");
    fflush(engine->history_file);
  }

  return engine;
}

void engine_destroy(Engine *engine) {
  if (!engine)
    return;
  if (engine->history_file)
    fclose(engine->history_file);
  // Free undo/redo stacks
  for (int i = 0; i < engine->undo_top; i++)
    free(engine->undo_stack[i].buf);
  for (int i = 0; i < engine->redo_top; i++)
    free(engine->redo_stack[i].buf);
  doc_destroy(engine->doc);
  free(engine);
}

// -----------------------------------------------------------------------------
// Block Helper - Find page and block by IDs
// -----------------------------------------------------------------------------

static Page *find_page(Engine *engine, uint32_t page_id) {
  if (!engine || !engine->doc)
    return NULL;
  return doc_page_get(engine->doc, page_id);
}

static Block *find_block(Engine *engine, uint32_t page_id, uint32_t block_id) {
  Page *page = find_page(engine, page_id);
  if (!page)
    return NULL;
  return page_find_block_by_id(page, block_id);
}

// -----------------------------------------------------------------------------
// Command Execution
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Command Dispatch Table handlers
// -----------------------------------------------------------------------------

typedef bool (*CommandHandler)(Engine *engine, const AofCommand *cmd,
                               const char *command_line);

typedef struct {
  const char *verb;
  CommandHandler handler;
} CommandEntry;

// SYSTEM COMMANDS
static bool handle_exit(Engine *engine, const AofCommand *cmd,
                        const char *command_line) {
  (void)cmd;
  (void)command_line;
  engine->is_running = false;
  return true;
}

static bool handle_clear(Engine *engine, const AofCommand *cmd,
                         const char *command_line) {
  (void)cmd;
  (void)command_line;
  if (engine->history_file) {
    fclose(engine->history_file);
    engine->history_file = NULL;
  }
  clear_history_log(engine);
  clear_redo_log(engine);

  doc_destroy(engine->doc);
  engine->doc = doc_create();

  if (engine->ui_host) {
    engine->ui_host->selected_node = NULL;
  }
  scripting_set_document(engine->doc);

  // Clear undo/redo stacks (document changed radically)
  for (int i = 0; i < engine->undo_top; i++) {
    free(engine->undo_stack[i].buf);
    engine->undo_stack[i].buf = NULL;
  }
  engine->undo_top = 0;
  for (int i = 0; i < engine->redo_top; i++) {
    free(engine->redo_stack[i].buf);
    engine->redo_stack[i].buf = NULL;
  }
  engine->redo_top = 0;
  engine->last_snapshot_ticks = 0;
  engine->last_snapshot_verb[0] = '\0';

  engine->history_file = fopen(engine->history_path, "a");
  if (engine->history_file) {
    fprintf(engine->history_file, "-- Session started --\n");
    fflush(engine->history_file);
  }
  return true;
}

// DOCUMENT COMMANDS
static bool handle_doc_title(Engine *engine, const AofCommand *cmd,
                             const char *command_line) {
  if (cmd->argc < 2)
    return false;
  char unescaped[256];
  unescape_string(cmd->argv[1], unescaped, sizeof(unescaped));
  doc_set_title(engine->doc, unescaped);
  engine_emit_command(engine, command_line);
  return true;
}

static bool handle_set_global(Engine *engine, const AofCommand *cmd,
                              const char *command_line) {
  if (cmd->argc < 3)
    return false;
  char unescaped[256];
  unescape_string(cmd->argv[2], unescaped, sizeof(unescaped));
  bool success = doc_set_global(engine->doc, cmd->argv[1], unescaped);
  if (success)
    engine_emit_command(engine, command_line);
  return success;
}

static bool handle_unset_global(Engine *engine, const AofCommand *cmd,
                                const char *command_line) {
  if (cmd->argc < 2)
    return false;
  bool success = doc_unset_global(engine->doc, cmd->argv[1]);
  if (success)
    engine_emit_command(engine, command_line);
  return success;
}

static bool handle_doc_save_as(Engine *engine, const AofCommand *cmd,
                               const char *command_line) {
  (void)command_line;
  if (cmd->argc < 2)
    return false;

  const char *new_path = cmd->argv[1];

  // Changer le chemin (cela fermera/réouvrira le history_file s'il était
  // ouvert)
  engine_set_document_path(engine, new_path);

  // Pour un "Save As", on veut souvent que le nouveau fichier contienne l'état
  // actuel Si on vient d'ouvrir le fichier en mode "a" via
  // engine_set_document_path, on peut y injecter le dump AOF.
  if (engine->history_file) {
    doc_save_aof(engine->doc, engine->history_file);
    fflush(engine->history_file);
    return true;
  }
  return false;
}

static bool handle_doc_save_snapshot(Engine *engine, const AofCommand *cmd,
                                     const char *command_line) {
  (void)command_line;
  if (cmd->argc < 2)
    return false;
  return engine_save_snapshot_as_aslan(engine, cmd->argv[1]);
}

// PAGE COMMANDS
static bool handle_page_create(Engine *engine, const AofCommand *cmd,
                               const char *command_line) {
  (void)command_line;
  if (cmd->argc < 2)
    return false;
  uint32_t forced_id = 0;
  const char *page_name = NULL;

  if (cmd->argc >= 3) {
    forced_id = (uint32_t)atoi(cmd->argv[1]);
    page_name = cmd->argv[2];
  } else {
    page_name = cmd->argv[1];
  }

  char unescaped[64];
  unescape_string(page_name, unescaped, sizeof(unescaped));

  Page *page = NULL;
  if (forced_id > 0) {
    page = doc_page_get(engine->doc, forced_id);
    if (page)
      return true;
    page = doc_page_create_with_id(engine->doc, forced_id, unescaped);
  } else {
    page = doc_page_create(engine->doc, unescaped);
  }

  if (page) {
    char canon[256];
    snprintf(canon, sizeof(canon), "page_create %u \"%s\"", page->id,
             page->name);
    engine_emit_command(engine, canon);
    return true;
  }
  return false;
}

static bool handle_page_delete(Engine *engine, const AofCommand *cmd,
                               const char *command_line) {
  if (cmd->argc < 2)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  bool success = doc_page_delete(engine->doc, page_id);
  if (success)
    engine_emit_command(engine, command_line);
  return success;
}

static bool handle_page_rename(Engine *engine, const AofCommand *cmd,
                               const char *command_line) {
  if (cmd->argc < 3)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  char unescaped[64];
  unescape_string(cmd->argv[2], unescaped, sizeof(unescaped));
  bool success = doc_page_rename(engine->doc, page_id, unescaped);
  if (success)
    engine_emit_command(engine, command_line);
  return success;
}

static bool handle_page_activate(Engine *engine, const AofCommand *cmd,
                                 const char *command_line) {
  if (cmd->argc < 2)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  doc_page_activate(engine->doc, page_id);
  if (engine->ui_host)
    engine->ui_host->selected_node = NULL;
  engine_emit_command(engine, command_line);
  return true;
}

// BLOCK COMMANDS
static bool handle_block_create(Engine *engine, const AofCommand *cmd,
                                const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  BlockType type = block_type_from_string(cmd->argv[3]);

  Page *page = find_page(engine, page_id);
  if (!page)
    return false;

  if (page_find_block_by_id(page, block_id))
    return true;

  Block *block = block_create_with_id(page, block_id, type);
  if (block) {
    page_append_block(page, block);
    switch (type) {
    case BLOCK_TABLE:
      block_table_init(block);
      break;
    case BLOCK_DESIGN:
      block_design_init(block);
      break;
    case BLOCK_CHART:
      block_chart_init(block);
      break;
    default:
      break;
    }
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_delete(Engine *engine, const AofCommand *cmd,
                                const char *command_line) {
  if (cmd->argc < 3)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Page *page = find_page(engine, page_id);
  if (page) {
    Block *block = page_find_block_by_id(page, block_id);
    if (block) {
      page_remove_block(page, block);
      block_destroy(block);
      engine_emit_command(engine, command_line);
      return true;
    }
  }
  return false;
}

static bool handle_block_text(Engine *engine, const AofCommand *cmd,
                              const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (block) {
    char unescaped[4096];
    unescape_string(cmd->argv[3], unescaped, sizeof(unescaped));
    block_set_text(block, unescaped);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_move_after(Engine *engine, const AofCommand *cmd,
                                    const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  uint32_t after_id = (uint32_t)atoi(cmd->argv[3]);
  Page *page = find_page(engine, page_id);
  if (page) {
    Block *block = page_find_block_by_id(page, block_id);
    Block *after = after_id ? page_find_block_by_id(page, after_id) : NULL;
    if (block) {
      page_remove_block(page, block);
      page_insert_block_after(page, after, block);
      engine_emit_command(engine, command_line);
      return true;
    }
  }
  return false;
}

static bool handle_block_collapse(Engine *engine, const AofCommand *cmd,
                                  const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int collapsed = atoi(cmd->argv[3]);
  Block *block = find_block(engine, page_id, block_id);
  if (block) {
    block->is_collapsed = collapsed != 0;
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_highlight(Engine *engine, const AofCommand *cmd,
                                   const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int color = atoi(cmd->argv[3]);
  Block *block = find_block(engine, page_id, block_id);
  if (block) {
    block->highlight_color = color;
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

// TABLE BLOCK COMMANDS
static bool handle_block_table_init(Engine *engine, const AofCommand *cmd,
                                    const char *command_line) {
  if (cmd->argc < 5)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int rows = atoi(cmd->argv[3]);
  int cols = atoi(cmd->argv[4]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE) {
    block_table_init(block);
    int current_rows = DEFAULT_TABLE_ROWS;
    while (current_rows < rows) {
      block_table_insert_row(block, -1);
      current_rows++;
    }
    int current_cols = DEFAULT_TABLE_COLS;
    while (current_cols < cols) {
      block_table_insert_col(block, -1);
      current_cols++;
    }
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_table_cell(Engine *engine, const AofCommand *cmd,
                                    const char *command_line) {
  if (cmd->argc < 6)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int row = atoi(cmd->argv[3]);
  int col = atoi(cmd->argv[4]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE) {
    char unescaped[1024];
    unescape_string(cmd->argv[5], unescaped, sizeof(unescaped));
    block_table_set_cell(block, row, col, unescaped);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_table_view(Engine *engine, const AofCommand *cmd,
                                    const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  const char *view_str = cmd->argv[3];
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE) {
    TableViewMode mode = TABLE_VIEW_TABLE;
    if (strcmp(view_str, "list") == 0)
      mode = TABLE_VIEW_LIST;
    else if (strcmp(view_str, "board") == 0)
      mode = TABLE_VIEW_BOARD;
    else if (strcmp(view_str, "calendar") == 0)
      mode = TABLE_VIEW_CALENDAR;
    block_table_set_view(block, mode);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_table_insert_row(Engine *engine, const AofCommand *cmd,
                                          const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int row_idx = atoi(cmd->argv[3]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE) {
    block_table_insert_row(block, row_idx);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_table_insert_col(Engine *engine, const AofCommand *cmd,
                                          const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int col_idx = atoi(cmd->argv[3]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE) {
    block_table_insert_col(block, col_idx);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_table_delete_row(Engine *engine, const AofCommand *cmd,
                                          const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int row_idx = atoi(cmd->argv[3]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE) {
    block_table_delete_row(block, row_idx);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_table_delete_col(Engine *engine, const AofCommand *cmd,
                                          const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int col_idx = atoi(cmd->argv[3]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE) {
    block_table_delete_col(block, col_idx);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_table_col_width(Engine *engine, const AofCommand *cmd,
                                         const char *command_line) {
  if (cmd->argc < 5)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int col_idx = atoi(cmd->argv[3]);
  float width = (float)atof(cmd->argv[4]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE && block->data.table.columnar) {
    AslanTable *tbl = block->data.table.columnar;
    if (col_idx >= 0 && col_idx < (int)tbl->column_count) {
      tbl->columns[col_idx].width_px = width;
    }
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

// block_table_col_color <page_id> <block_id> <col_idx> <rgba_u32>
static bool handle_block_table_col_color(Engine *engine, const AofCommand *cmd,
                                         const char *command_line) {
  if (cmd->argc < 5)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int col_idx = atoi(cmd->argv[3]);
  uint32_t color = (uint32_t)strtoul(cmd->argv[4], NULL, 10);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE && block->data.table.columnar) {
    AslanTable *tbl = block->data.table.columnar;
    if (col_idx >= 0 && col_idx < (int)tbl->column_count) {
      tbl->columns[col_idx].bg_color = color;
    }
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

// block_table_col_info <page_id> <block_id> <col_idx> <type_int> <name>
static bool handle_block_table_col_info(Engine *engine, const AofCommand *cmd,
                                        const char *command_line) {
  if (cmd->argc < 6)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int col_idx = atoi(cmd->argv[3]);
  int type_int = atoi(cmd->argv[4]);
  const char *name = cmd->argv[5];
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE && block->data.table.columnar) {
    AslanTable *tbl = block->data.table.columnar;
    if (col_idx >= 0 && col_idx < (int)tbl->column_count) {
      tbl->columns[col_idx].type = (AslanColumnType)type_int;
      strncpy(tbl->columns[col_idx].name, name,
              sizeof(tbl->columns[col_idx].name) - 1);
    }
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

// block_table_col_formula <page_id> <block_id> <col_idx> <formula_escaped>
static bool handle_block_table_col_formula(Engine *engine,
                                           const AofCommand *cmd,
                                           const char *command_line) {
  if (cmd->argc < 5)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  int col_idx = atoi(cmd->argv[3]);
  const char *formula = cmd->argv[4];
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_TABLE && block->data.table.columnar) {
    AslanTable *tbl = block->data.table.columnar;
    char unescaped[1024];
    unescape_string(formula, unescaped, sizeof(unescaped));
    aslan_table_set_formula(tbl, col_idx, unescaped);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

// DESIGN BLOCK COMMANDS
static bool handle_block_design_init(Engine *engine, const AofCommand *cmd,
                                     const char *command_line) {
  if (cmd->argc < 5)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_DESIGN) {
    block_design_init(block);
    block_design_set_size(block, atoi(cmd->argv[3]), atoi(cmd->argv[4]));
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_design_bg(Engine *engine, const AofCommand *cmd,
                                   const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_DESIGN) {
    block_design_set_bg_color(block, (uint32_t)strtoul(cmd->argv[3], NULL, 0));
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_design_nolimits(Engine *engine, const AofCommand *cmd,
                                         const char *command_line) {
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_DESIGN) {
    block->data.design.no_limits = atoi(cmd->argv[3]);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_design_view(Engine *engine, const AofCommand *cmd,
                                     const char *command_line) {
  if (cmd->argc < 6)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_DESIGN) {
    block->data.design.view_x = (float)atof(cmd->argv[3]);
    block->data.design.view_y = (float)atof(cmd->argv[4]);
    block->data.design.zoom = (float)atof(cmd->argv[5]);
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_design_resize(Engine *engine, const AofCommand *cmd,
                                       const char *command_line) {
  if (cmd->argc < 5)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_DESIGN) {
    block_design_set_size(block, atoi(cmd->argv[3]), atoi(cmd->argv[4]));
    block->data.design.units = (cmd->argc >= 6) ? atoi(cmd->argv[5]) : 0;
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

static bool handle_block_design_node(Engine *engine, const AofCommand *cmd,
                                     const char *command_line) {
  if (cmd->argc < 5)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  const char *action = cmd->argv[3];
  Block *block = find_block(engine, page_id, block_id);
  if (block && block->type == BLOCK_DESIGN) {
    if (strcmp(action, "create") == 0 && cmd->argc >= 7) {
      uint32_t node_id = (uint32_t)atoi(cmd->argv[4]);
      NodeType type = node_type_from_string(cmd->argv[5]);
      char unescaped[64];
      unescape_string(cmd->argv[6], unescaped, sizeof(unescaped));
      if (!block_design_find_node(block, node_id)) {
        Node *node = node_create(node_id, type, unescaped);
        if (node && block->data.design.root) {
          node_add_child(block->data.design.root, node);
          if (node_id >= block->data.design.next_node_id)
            block->data.design.next_node_id = node_id + 1;
        }
      }
    } else if (strcmp(action, "delete") == 0 && cmd->argc >= 5) {
      block_design_delete_node(block, (uint32_t)atoi(cmd->argv[4]));
    } else if (strcmp(action, "move") == 0 && cmd->argc >= 7) {
      Node *node = block_design_find_node(block, (uint32_t)atoi(cmd->argv[4]));
      if (node)
        node_set_position(node, (float)atof(cmd->argv[5]),
                          (float)atof(cmd->argv[6]));
    } else if (strcmp(action, "resize") == 0 && cmd->argc >= 7) {
      Node *node = block_design_find_node(block, (uint32_t)atoi(cmd->argv[4]));
      if (node)
        node_set_size(node, (float)atof(cmd->argv[5]),
                      (float)atof(cmd->argv[6]));
    } else if (strcmp(action, "text") == 0 && cmd->argc >= 6) {
      Node *node = block_design_find_node(block, (uint32_t)atoi(cmd->argv[4]));
      if (node) {
        char unescaped[256];
        unescape_string(cmd->argv[5], unescaped, sizeof(unescaped));
        node_set_text(node, unescaped);
      }
    } else if (strcmp(action, "fill") == 0 && cmd->argc >= 6) {
      Node *node = block_design_find_node(block, (uint32_t)atoi(cmd->argv[4]));
      if (node)
        node_set_fill_color(node, (uint32_t)strtoul(cmd->argv[5], NULL, 0));
    } else if (strcmp(action, "stroke") == 0 && cmd->argc >= 7) {
      Node *node = block_design_find_node(block, (uint32_t)atoi(cmd->argv[4]));
      if (node)
        node_set_stroke(node, (uint32_t)strtoul(cmd->argv[5], NULL, 0),
                        (float)atof(cmd->argv[6]));
    } else if (strcmp(action, "linked_col") == 0 && cmd->argc >= 6) {
      Node *node = block_design_find_node(block, (uint32_t)atoi(cmd->argv[4]));
      if (node) {
        char unescaped[64];
        unescape_string(cmd->argv[5], unescaped, sizeof(unescaped));
        strncpy(node->linked_col_name, unescaped,
                sizeof(node->linked_col_name) - 1);
        node->linked_col_name[sizeof(node->linked_col_name) - 1] = '\0';
      }
    } else {
      return false;
    }
    engine_emit_command(engine, command_line);
    return true;
  }
  return false;
}

// =========================================================================
// CHART BLOCK COMMANDS
// =========================================================================

static bool handle_block_chart_init(Engine *engine, const AofCommand *cmd,
                                    const char *command_line) {
  // block_chart_init <page_id> <block_id> <type> <source_table_id> <label_col>
  // <value_col>
  if (cmd->argc < 7)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (!block || block->type != BLOCK_CHART)
    return false;

  ChartData *cd = &block->data.chart;
  const char *type_str = cmd->argv[3];
  if (strcmp(type_str, "line") == 0)
    cd->type = CHART_LINE;
  else if (strcmp(type_str, "pie") == 0)
    cd->type = CHART_PIE;
  else
    cd->type = CHART_BAR;
  cd->source_table_id = (uint32_t)atoi(cmd->argv[4]);
  cd->label_col_idx = atoi(cmd->argv[5]);
  cd->value_col_idx = atoi(cmd->argv[6]);
  engine_emit_command(engine, command_line);
  return true;
}

static bool handle_block_chart_color(Engine *engine, const AofCommand *cmd,
                                     const char *command_line) {
  // block_chart_color <page_id> <block_id> <rgba>
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (!block || block->type != BLOCK_CHART)
    return false;

  block->data.chart.color = (uint32_t)strtoul(cmd->argv[3], NULL, 0);
  engine_emit_command(engine, command_line);
  return true;
}

static bool handle_block_chart_show_values(Engine *engine,
                                           const AofCommand *cmd,
                                           const char *command_line) {
  // block_chart_show_values <page_id> <block_id> 0|1
  if (cmd->argc < 4)
    return false;
  uint32_t page_id = (uint32_t)atoi(cmd->argv[1]);
  uint32_t block_id = (uint32_t)atoi(cmd->argv[2]);
  Block *block = find_block(engine, page_id, block_id);
  if (!block || block->type != BLOCK_CHART)
    return false;

  block->data.chart.show_values = atoi(cmd->argv[3]) != 0;
  engine_emit_command(engine, command_line);
  return true;
}

// block_layout <page_id> <block_id> <row> <col>
static bool handle_block_layout(Engine *engine, const AofCommand *cmd,
                                const char *command_line) {
  if (cmd->argc < 5)
    return false;
  uint32_t page_id = (uint32_t)strtoul(cmd->argv[1], NULL, 10);
  uint32_t block_id = (uint32_t)strtoul(cmd->argv[2], NULL, 10);
  Block *block = find_block(engine, page_id, block_id);
  if (!block)
    return false;
  block->layout_row = (uint16_t)atoi(cmd->argv[3]);
  block->layout_col = (uint8_t)atoi(cmd->argv[4]);
  engine_emit_command(engine, command_line);
  return true;
}

// -----------------------------------------------------------------------------
// Undo/Redo — Snapshot-based
// NOTE: Operations that bypass engine_execute_command (canvas arrow-key nudge,
// Cmd+D duplicate, Delete key) are not captured in the undo stack.
// -----------------------------------------------------------------------------

// Serialise current document into a heap buffer via open_memstream.
static char *engine_snapshot_current(Engine *engine, size_t *out_size) {
  if (!engine || !engine->doc)
    return NULL;
  char *ptr = NULL;
  size_t size = 0;
  FILE *mem = open_memstream(&ptr, &size);
  if (!mem)
    return NULL;
  doc_save_aof(engine->doc, mem);
  fclose(mem);
  if (out_size)
    *out_size = size;
  return ptr;
}

// Verbs that must NOT trigger an undo snapshot.
static bool engine_verb_is_non_mutating(const char *verb) {
  static const char *const skip[] = {"exit",
                                     "clear",
                                     "doc_save_as",
                                     "doc_save_snapshot",
                                     "page_activate",
                                     "block_design_view",
                                     "undo",
                                     "redo",
                                     NULL};
  for (int i = 0; skip[i]; i++) {
    if (strcmp(verb, skip[i]) == 0)
      return true;
  }
  return false;
}

// Push undo snapshot before a mutating command executes.
static void engine_push_undo_snapshot(Engine *engine, const char *verb) {
  if (engine->is_loading)
    return;
  if (engine_verb_is_non_mutating(verb))
    return;

  // Coalescing: skip if same verb within 500ms
  uint32_t now = SDL_GetTicks();
  if (engine->last_snapshot_ticks != 0 &&
      strcmp(verb, engine->last_snapshot_verb) == 0 &&
      (now - engine->last_snapshot_ticks) < 500) {
    return;
  }

  size_t snap_size = 0;
  char *snap = engine_snapshot_current(engine, &snap_size);
  if (!snap)
    return;

  // Evict oldest if full
  if (engine->undo_top >= ENGINE_UNDO_MAX) {
    free(engine->undo_stack[0].buf);
    memmove(&engine->undo_stack[0], &engine->undo_stack[1],
            (ENGINE_UNDO_MAX - 1) * sizeof(engine->undo_stack[0]));
    engine->undo_top = ENGINE_UNDO_MAX - 1;
  }

  engine->undo_stack[engine->undo_top].buf = snap;
  engine->undo_stack[engine->undo_top].size = snap_size;
  engine->undo_top++;

  // Clear redo stack
  for (int i = 0; i < engine->redo_top; i++) {
    free(engine->redo_stack[i].buf);
    engine->redo_stack[i].buf = NULL;
  }
  engine->redo_top = 0;

  engine->last_snapshot_ticks = now;
  strncpy(engine->last_snapshot_verb, verb,
          sizeof(engine->last_snapshot_verb) - 1);
  engine->last_snapshot_verb[sizeof(engine->last_snapshot_verb) - 1] = '\0';
}

// Restore document from an AOF snapshot string.
static void engine_restore_from_snapshot(Engine *engine, const char *buf,
                                         size_t size) {
  (void)size;

  doc_destroy(engine->doc);
  engine->doc = doc_create();
  if (engine->ui_host) {
    engine->ui_host->selected_node = NULL;
  }
  scripting_set_document(engine->doc);

  engine->is_loading = true;

  const char *p = buf;
  while (*p) {
    const char *eol = p;
    while (*eol && *eol != '\n')
      eol++;

    char line[2048];
    size_t len = (size_t)(eol - p);
    if (len >= sizeof(line))
      len = sizeof(line) - 1;
    memcpy(line, p, len);
    line[len] = '\0';

    strip_trailing_newlines(line);
    if (!is_comment_or_empty(line)) {
      engine_execute_command(engine, line);
    }

    p = (*eol == '\n') ? eol + 1 : eol;
  }

  engine->is_loading = false;

  if (engine->ui_host) {
    ui_request_redraw(engine->ui_host);
  }
}

static bool handle_undo(Engine *engine, const AofCommand *cmd,
                        const char *command_line) {
  (void)cmd;
  (void)command_line;

  if (engine->undo_top == 0)
    return false;

  // Push current state onto redo stack
  if (engine->redo_top < ENGINE_UNDO_MAX) {
    size_t now_size = 0;
    char *now_snap = engine_snapshot_current(engine, &now_size);
    if (now_snap) {
      engine->redo_stack[engine->redo_top].buf = now_snap;
      engine->redo_stack[engine->redo_top].size = now_size;
      engine->redo_top++;
    }
  }

  // Pop undo stack
  engine->undo_top--;
  char *snap = engine->undo_stack[engine->undo_top].buf;
  size_t sz = engine->undo_stack[engine->undo_top].size;
  engine->undo_stack[engine->undo_top].buf = NULL;
  engine->undo_stack[engine->undo_top].size = 0;

  engine_restore_from_snapshot(engine, snap, sz);
  free(snap);

  engine->last_snapshot_ticks = 0;
  engine->last_snapshot_verb[0] = '\0';
  return true;
}

static bool handle_redo(Engine *engine, const AofCommand *cmd,
                        const char *command_line) {
  (void)cmd;
  (void)command_line;

  if (engine->redo_top == 0)
    return false;

  // Push current state onto undo stack
  if (engine->undo_top < ENGINE_UNDO_MAX) {
    size_t now_size = 0;
    char *now_snap = engine_snapshot_current(engine, &now_size);
    if (now_snap) {
      engine->undo_stack[engine->undo_top].buf = now_snap;
      engine->undo_stack[engine->undo_top].size = now_size;
      engine->undo_top++;
    }
  }

  // Pop redo stack
  engine->redo_top--;
  char *snap = engine->redo_stack[engine->redo_top].buf;
  size_t sz = engine->redo_stack[engine->redo_top].size;
  engine->redo_stack[engine->redo_top].buf = NULL;
  engine->redo_stack[engine->redo_top].size = 0;

  engine_restore_from_snapshot(engine, snap, sz);
  free(snap);

  engine->last_snapshot_ticks = 0;
  engine->last_snapshot_verb[0] = '\0';
  return true;
}

static const CommandEntry k_dispatch_table[] = {
    {"exit", handle_exit},
    {"clear", handle_clear},
    {"undo", handle_undo},
    {"redo", handle_redo},
    {"doc_title", handle_doc_title},
    {"set_global", handle_set_global},
    {"unset_global", handle_unset_global},
    {"doc_save_as", handle_doc_save_as},
    {"doc_save_snapshot", handle_doc_save_snapshot},
    {"page_create", handle_page_create},
    {"page_delete", handle_page_delete},
    {"page_rename", handle_page_rename},
    {"page_activate", handle_page_activate},
    {"block_create", handle_block_create},
    {"block_delete", handle_block_delete},
    {"block_text", handle_block_text},
    {"block_move_after", handle_block_move_after},
    {"block_collapse", handle_block_collapse},
    {"block_highlight", handle_block_highlight},
    {"block_table_init", handle_block_table_init},
    {"block_table_cell", handle_block_table_cell},
    {"block_table_view", handle_block_table_view},
    {"block_table_insert_row", handle_block_table_insert_row},
    {"block_table_insert_col", handle_block_table_insert_col},
    {"block_table_delete_row", handle_block_table_delete_row},
    {"block_table_delete_col", handle_block_table_delete_col},
    {"block_table_col_width", handle_block_table_col_width},
    {"block_table_col_color", handle_block_table_col_color},
    {"block_table_col_info", handle_block_table_col_info},
    {"block_table_col_formula", handle_block_table_col_formula},
    {"block_design_init", handle_block_design_init},
    {"block_design_bg", handle_block_design_bg},
    {"block_design_nolimits", handle_block_design_nolimits},
    {"block_design_view", handle_block_design_view},
    {"block_design_resize", handle_block_design_resize},
    {"block_design_node", handle_block_design_node},
    {"block_chart_init", handle_block_chart_init},
    {"block_chart_color", handle_block_chart_color},
    {"block_chart_show_values", handle_block_chart_show_values},
    {"block_layout", handle_block_layout},
    {NULL, NULL}};

bool engine_execute_command(Engine *engine, const char *command_line) {
  if (!engine || !command_line || !*command_line)
    return false;

  AofCommand cmd = aof_tokenize(command_line);
  if (cmd.argc == 0)
    return false;

  const char *verb = cmd.argv[0];

  // Push undo snapshot before mutating commands (skipped during replay)
  engine_push_undo_snapshot(engine, verb);

  for (int i = 0; k_dispatch_table[i].verb != NULL; i++) {
    if (strcmp(verb, k_dispatch_table[i].verb) == 0) {
      return k_dispatch_table[i].handler(engine, &cmd, command_line);
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
// History Loading
// -----------------------------------------------------------------------------

void engine_load_from_history(Engine *engine) {
  if (!engine)
    return;

  FILE *f = fopen(engine->history_path, "r");
  if (!f)
    return;

  engine->is_loading = true;

  char line[2048];
  while (fgets(line, sizeof(line), f)) {
    strip_trailing_newlines(line);
    if (is_comment_or_empty(line))
      continue;

    engine_execute_command(engine, line);
  }

  fclose(f);
  engine->is_loading = false;

  if (engine->ui_host) {
    ui_request_redraw(engine->ui_host);
  }
}

// -----------------------------------------------------------------------------
// Snapshot Save/Load
// -----------------------------------------------------------------------------

bool engine_save_snapshot_as_aslan(Engine *engine, const char *out_path) {
  if (!engine || !engine->doc || !out_path || !out_path[0])
    return false;

  FILE *out = fopen(out_path, "w");
  if (!out)
    return false;

  doc_save_aof(engine->doc, out);
  fclose(out);
  return true;
}

bool engine_load_snapshot_from_aslan(Engine *engine, const char *in_path) {
  if (!engine || !in_path || !in_path[0])
    return false;

  FILE *f = fopen(in_path, "r");
  if (!f)
    return false;

  // Clear current document
  engine_execute_command(engine, "clear");

  char line[2048];
  bool saw_any = false;

  while (fgets(line, sizeof(line), f)) {
    strip_trailing_newlines(line);
    if (is_comment_or_empty(line))
      continue;

    // Skip version header
    if (strncmp(line, "-- aslan", 8) == 0)
      continue;

    if (engine_execute_command(engine, line)) {
      saw_any = true;
    }
  }

  fclose(f);
  return saw_any;
}
