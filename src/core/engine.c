#include "engine.h"
#include "../platform/ui_host.h"
#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (!src || !dst || dst_size == 0) return dst;

    size_t di = 0;
    for (size_t si = 0; src[si] && di + 1 < dst_size; si++) {
        if (src[si] == '\\' && src[si + 1]) {
            si++;
            switch (src[si]) {
                case 'n': dst[di++] = '\n'; break;
                case 'r': dst[di++] = '\r'; break;
                case 't': dst[di++] = '\t'; break;
                case '"': dst[di++] = '"'; break;
                case '\\': dst[di++] = '\\'; break;
                default: dst[di++] = src[si]; break;
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
    if (!line || !*line) return cmd;

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
// AOF File Paths
// -----------------------------------------------------------------------------

static const char *k_history_path = "history.log";
static const char *k_redo_path = "redo.log";

static void clear_history_log(void) {
    FILE *f = fopen(k_history_path, "w");
    if (f) fclose(f);
}

static void clear_redo_log(void) {
    FILE *f = fopen(k_redo_path, "w");
    if (f) fclose(f);
}

// -----------------------------------------------------------------------------
// AOF Emission
// -----------------------------------------------------------------------------

static void engine_emit_command(Engine *engine, const char *line) {
    if (!engine || !engine->history_file || !line) return;
    fprintf(engine->history_file, "%s\n", line);
    fflush(engine->history_file);
}

static void escape_and_emit(Engine *engine, const char *fmt, ...) {
    if (!engine || !engine->history_file) return;

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
    if (!line) return true;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '\n' || *line == '\r') return true;
    if (*line == '-' && *(line + 1) == '-') return true;
    return false;
}

static void strip_trailing_newlines(char *s) {
    if (!s) return;
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
    if (!engine) return NULL;

    engine->doc = doc_create();
    engine->is_running = true;

    engine->history_file = fopen(k_history_path, "a");
    if (engine->history_file) {
        fprintf(engine->history_file, "-- Session started --\n");
        fflush(engine->history_file);
    }

    return engine;
}

void engine_destroy(Engine *engine) {
    if (!engine) return;
    if (engine->history_file) fclose(engine->history_file);
    doc_destroy(engine->doc);
    free(engine);
}

// -----------------------------------------------------------------------------
// Block Helper - Find page and block by IDs
// -----------------------------------------------------------------------------

static Page *find_page(Engine *engine, uint32_t page_id) {
    if (!engine || !engine->doc) return NULL;
    return doc_page_get(engine->doc, page_id);
}

static Block *find_block(Engine *engine, uint32_t page_id, uint32_t block_id) {
    Page *page = find_page(engine, page_id);
    if (!page) return NULL;
    return page_find_block_by_id(page, block_id);
}

// -----------------------------------------------------------------------------
// Command Execution
// -----------------------------------------------------------------------------

bool engine_execute_command(Engine *engine, const char *command_line) {
    if (!engine || !command_line || !*command_line) return false;

    AofCommand cmd = aof_tokenize(command_line);
    if (cmd.argc == 0) return false;

    const char *verb = cmd.argv[0];
    bool success = false;

    // =========================================================================
    // SYSTEM COMMANDS
    // =========================================================================

    if (strcmp(verb, "exit") == 0) {
        engine->is_running = false;
        success = true;
    }

    else if (strcmp(verb, "clear") == 0) {
        if (engine->history_file) {
            fclose(engine->history_file);
            engine->history_file = NULL;
        }
        clear_history_log();
        clear_redo_log();

        doc_destroy(engine->doc);
        engine->doc = doc_create();

        if (engine->ui_host) {
            engine->ui_host->selected_node = NULL;
        }
        scripting_set_document(engine->doc);

        engine->history_file = fopen(k_history_path, "a");
        if (engine->history_file) {
            fprintf(engine->history_file, "-- Session started --\n");
            fflush(engine->history_file);
        }
        success = true;
    }

    // =========================================================================
    // DOCUMENT COMMANDS
    // =========================================================================

    else if (strcmp(verb, "doc_title") == 0 && cmd.argc >= 2) {
        char unescaped[256];
        unescape_string(cmd.argv[1], unescaped, sizeof(unescaped));
        doc_set_title(engine->doc, unescaped);
        engine_emit_command(engine, command_line);
        success = true;
    }

    else if (strcmp(verb, "set_global") == 0 && cmd.argc >= 3) {
        char unescaped[256];
        unescape_string(cmd.argv[2], unescaped, sizeof(unescaped));
        success = doc_set_global(engine->doc, cmd.argv[1], unescaped);
        if (success) engine_emit_command(engine, command_line);
    }

    else if (strcmp(verb, "unset_global") == 0 && cmd.argc >= 2) {
        success = doc_unset_global(engine->doc, cmd.argv[1]);
        if (success) engine_emit_command(engine, command_line);
    }

    // =========================================================================
    // PAGE COMMANDS
    // =========================================================================

    else if (strcmp(verb, "page_create") == 0 && cmd.argc >= 2) {
        uint32_t forced_id = 0;
        const char *page_name = NULL;

        if (cmd.argc >= 3) {
            forced_id = (uint32_t)atoi(cmd.argv[1]);
            page_name = cmd.argv[2];
        } else {
            page_name = cmd.argv[1];
        }

        char unescaped[64];
        unescape_string(page_name, unescaped, sizeof(unescaped));

        Page *page = NULL;
        if (forced_id > 0) {
            // Check if already exists (idempotent)
            page = doc_page_get(engine->doc, forced_id);
            if (page) {
                success = true;
                return success;
            }
            page = doc_page_create_with_id(engine->doc, forced_id, unescaped);
        } else {
            page = doc_page_create(engine->doc, unescaped);
        }

        if (page) {
            char canon[256];
            snprintf(canon, sizeof(canon), "page_create %u \"%s\"",
                     page->id, page->name);
            engine_emit_command(engine, canon);
            success = true;
        }
    }

    else if (strcmp(verb, "page_delete") == 0 && cmd.argc >= 2) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        success = doc_page_delete(engine->doc, page_id);
        if (success) engine_emit_command(engine, command_line);
    }

    else if (strcmp(verb, "page_rename") == 0 && cmd.argc >= 3) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        char unescaped[64];
        unescape_string(cmd.argv[2], unescaped, sizeof(unescaped));
        success = doc_page_rename(engine->doc, page_id, unescaped);
        if (success) engine_emit_command(engine, command_line);
    }

    else if (strcmp(verb, "page_activate") == 0 && cmd.argc >= 2) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        doc_page_activate(engine->doc, page_id);
        if (engine->ui_host) engine->ui_host->selected_node = NULL;
        engine_emit_command(engine, command_line);
        success = true;
    }

    // =========================================================================
    // BLOCK COMMANDS
    // =========================================================================

    else if (strcmp(verb, "block_create") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        BlockType type = block_type_from_string(cmd.argv[3]);

        Page *page = find_page(engine, page_id);
        if (page) {
            // Check idempotent
            Block *existing = page_find_block_by_id(page, block_id);
            if (existing) {
                success = true;
                return success;
            }

            Block *block = block_create_with_id(page, block_id, type);
            if (block) {
                page_append_block(page, block);

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

                engine_emit_command(engine, command_line);
                success = true;
            }
        }
    }

    else if (strcmp(verb, "block_delete") == 0 && cmd.argc >= 3) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);

        Page *page = find_page(engine, page_id);
        if (page) {
            Block *block = page_find_block_by_id(page, block_id);
            if (block) {
                page_remove_block(page, block);
                block_destroy(block);
                engine_emit_command(engine, command_line);
                success = true;
            }
        }
    }

    else if (strcmp(verb, "block_text") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);

        Block *block = find_block(engine, page_id, block_id);
        if (block) {
            char unescaped[4096];
            unescape_string(cmd.argv[3], unescaped, sizeof(unescaped));
            block_set_text(block, unescaped);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_move_after") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        uint32_t after_id = (uint32_t)atoi(cmd.argv[3]);

        Page *page = find_page(engine, page_id);
        if (page) {
            Block *block = page_find_block_by_id(page, block_id);
            Block *after = after_id ? page_find_block_by_id(page, after_id) : NULL;

            if (block) {
                page_remove_block(page, block);
                page_insert_block_after(page, after, block);
                engine_emit_command(engine, command_line);
                success = true;
            }
        }
    }

    else if (strcmp(verb, "block_collapse") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int collapsed = atoi(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block) {
            block->is_collapsed = collapsed != 0;
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_highlight") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int color = atoi(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block) {
            block->highlight_color = color;
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    // =========================================================================
    // TABLE BLOCK COMMANDS
    // =========================================================================

    else if (strcmp(verb, "block_table_init") == 0 && cmd.argc >= 5) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int rows = atoi(cmd.argv[3]);
        int cols = atoi(cmd.argv[4]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_TABLE) {
            block_table_init(block, rows, cols);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_table_cell") == 0 && cmd.argc >= 6) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int row = atoi(cmd.argv[3]);
        int col = atoi(cmd.argv[4]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_TABLE) {
            char unescaped[1024];
            unescape_string(cmd.argv[5], unescaped, sizeof(unescaped));
            block_table_set_cell(block, row, col, unescaped);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_table_view") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        const char *view_str = cmd.argv[3];

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_TABLE) {
            TableViewMode mode = TABLE_VIEW_TABLE;
            if (strcmp(view_str, "list") == 0) mode = TABLE_VIEW_LIST;
            else if (strcmp(view_str, "board") == 0) mode = TABLE_VIEW_BOARD;
            block_table_set_view(block, mode);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_table_insert_row") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int row_idx = atoi(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_TABLE) {
            block_table_insert_row(block, row_idx);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_table_insert_col") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int col_idx = atoi(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_TABLE) {
            block_table_insert_col(block, col_idx);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_table_delete_row") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int row_idx = atoi(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_TABLE) {
            block_table_delete_row(block, row_idx);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_table_delete_col") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int col_idx = atoi(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_TABLE) {
            block_table_delete_col(block, col_idx);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    // =========================================================================
    // CANVAS BLOCK COMMANDS
    // =========================================================================

    else if (strcmp(verb, "block_canvas_view") == 0 && cmd.argc >= 6) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        float x = (float)atof(cmd.argv[3]);
        float y = (float)atof(cmd.argv[4]);
        float zoom = (float)atof(cmd.argv[5]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_CANVAS) {
            block_canvas_set_view(block, x, y, zoom);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_canvas_node") == 0 && cmd.argc >= 5) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        const char *action = cmd.argv[3];

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_CANVAS) {
            if (strcmp(action, "create") == 0 && cmd.argc >= 7) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                NodeType type = node_type_from_string(cmd.argv[5]);
                char unescaped[64];
                unescape_string(cmd.argv[6], unescaped, sizeof(unescaped));

                // Check if node exists
                Node *existing = block_canvas_find_node(block, node_id);
                if (!existing) {
                    Node *node = node_create(node_id, type, unescaped);
                    if (node && block->data.canvas.root) {
                        node_add_child(block->data.canvas.root, node);
                        if (node_id >= block->data.canvas.next_node_id) {
                            block->data.canvas.next_node_id = node_id + 1;
                        }
                    }
                }
                engine_emit_command(engine, command_line);
                success = true;
            }
            else if (strcmp(action, "delete") == 0 && cmd.argc >= 5) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                block_canvas_delete_node(block, node_id);
                engine_emit_command(engine, command_line);
                success = true;
            }
            else if (strcmp(action, "move") == 0 && cmd.argc >= 7) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                float x = (float)atof(cmd.argv[5]);
                float y = (float)atof(cmd.argv[6]);
                Node *node = block_canvas_find_node(block, node_id);
                if (node) {
                    node_set_position(node, x, y);
                    engine_emit_command(engine, command_line);
                    success = true;
                }
            }
            else if (strcmp(action, "resize") == 0 && cmd.argc >= 7) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                float w = (float)atof(cmd.argv[5]);
                float h = (float)atof(cmd.argv[6]);
                Node *node = block_canvas_find_node(block, node_id);
                if (node) {
                    node_set_size(node, w, h);
                    engine_emit_command(engine, command_line);
                    success = true;
                }
            }
            else if (strcmp(action, "text") == 0 && cmd.argc >= 6) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                char unescaped[256];
                unescape_string(cmd.argv[5], unescaped, sizeof(unescaped));
                Node *node = block_canvas_find_node(block, node_id);
                if (node) {
                    node_set_text(node, unescaped);
                    engine_emit_command(engine, command_line);
                    success = true;
                }
            }
            else if (strcmp(action, "fill") == 0 && cmd.argc >= 6) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                uint32_t rgba = (uint32_t)strtoul(cmd.argv[5], NULL, 0);
                Node *node = block_canvas_find_node(block, node_id);
                if (node) {
                    node_set_fill_color(node, rgba);
                    engine_emit_command(engine, command_line);
                    success = true;
                }
            }
            else if (strcmp(action, "stroke") == 0 && cmd.argc >= 7) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                uint32_t rgba = (uint32_t)strtoul(cmd.argv[5], NULL, 0);
                float width = (float)atof(cmd.argv[6]);
                Node *node = block_canvas_find_node(block, node_id);
                if (node) {
                    node_set_stroke(node, rgba, width);
                    engine_emit_command(engine, command_line);
                    success = true;
                }
            }
        }
    }

    // =========================================================================
    // DESIGN BLOCK COMMANDS
    // =========================================================================

    else if (strcmp(verb, "block_design_init") == 0 && cmd.argc >= 5) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int width = atoi(cmd.argv[3]);
        int height = atoi(cmd.argv[4]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_DESIGN) {
            block_design_init(block, width, height);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_design_bg") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        uint32_t color = (uint32_t)strtoul(cmd.argv[3], NULL, 0);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_DESIGN) {
            block_design_set_bg_color(block, color);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_design_node") == 0 && cmd.argc >= 5) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        const char *action = cmd.argv[3];

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_DESIGN) {
            if (strcmp(action, "create") == 0 && cmd.argc >= 7) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                NodeType type = node_type_from_string(cmd.argv[5]);
                char unescaped[64];
                unescape_string(cmd.argv[6], unescaped, sizeof(unescaped));

                Node *existing = block_design_find_node(block, node_id);
                if (!existing) {
                    Node *node = node_create(node_id, type, unescaped);
                    if (node && block->data.design.root) {
                        node_add_child(block->data.design.root, node);
                        if (node_id >= block->data.design.next_node_id) {
                            block->data.design.next_node_id = node_id + 1;
                        }
                    }
                }
                engine_emit_command(engine, command_line);
                success = true;
            }
            else if (strcmp(action, "delete") == 0 && cmd.argc >= 5) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                block_design_delete_node(block, node_id);
                engine_emit_command(engine, command_line);
                success = true;
            }
            else if (strcmp(action, "move") == 0 && cmd.argc >= 7) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                float x = (float)atof(cmd.argv[5]);
                float y = (float)atof(cmd.argv[6]);
                Node *node = block_design_find_node(block, node_id);
                if (node) {
                    node_set_position(node, x, y);
                    engine_emit_command(engine, command_line);
                    success = true;
                }
            }
            else if (strcmp(action, "resize") == 0 && cmd.argc >= 7) {
                uint32_t node_id = (uint32_t)atoi(cmd.argv[4]);
                float w = (float)atof(cmd.argv[5]);
                float h = (float)atof(cmd.argv[6]);
                Node *node = block_design_find_node(block, node_id);
                if (node) {
                    node_set_size(node, w, h);
                    engine_emit_command(engine, command_line);
                    success = true;
                }
            }
        }
    }

    // =========================================================================
    // VIDEO BLOCK COMMANDS
    // =========================================================================

    else if (strcmp(verb, "block_video_clip_add") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_VIDEO) {
            char unescaped[256];
            unescape_string(cmd.argv[3], unescaped, sizeof(unescaped));
            block_video_add_clip(block, unescaped);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_video_clip_delete") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int clip_idx = atoi(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_VIDEO) {
            block_video_delete_clip(block, clip_idx);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_video_clip_range") == 0 && cmd.argc >= 6) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int clip_idx = atoi(cmd.argv[3]);
        double start = atof(cmd.argv[4]);
        double end = atof(cmd.argv[5]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_VIDEO) {
            block_video_set_clip_range(block, clip_idx, start, end);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_video_clip_comment") == 0 && cmd.argc >= 5) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        int clip_idx = atoi(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_VIDEO) {
            char unescaped[256];
            unescape_string(cmd.argv[4], unescaped, sizeof(unescaped));
            block_video_set_clip_comment(block, clip_idx, unescaped);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    else if (strcmp(verb, "block_video_playhead") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        double time = atof(cmd.argv[3]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_VIDEO) {
            block_video_set_playhead(block, time);
            // Don't emit playhead changes (high frequency)
            success = true;
        }
    }

    // =========================================================================
    // LUA BLOCK COMMANDS
    // =========================================================================

    else if (strcmp(verb, "block_lua_code") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_LUA) {
            char unescaped[4096];
            unescape_string(cmd.argv[3], unescaped, sizeof(unescaped));
            block_lua_set_code(block, unescaped);
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    // =========================================================================
    // CALLOUT BLOCK COMMANDS
    // =========================================================================

    else if (strcmp(verb, "block_callout_theme") == 0 && cmd.argc >= 4) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);
        const char *theme_str = cmd.argv[3];

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_CALLOUT) {
            CalloutTheme theme = CALLOUT_INFO;
            if (strcmp(theme_str, "alert") == 0) theme = CALLOUT_ALERT;
            else if (strcmp(theme_str, "star") == 0) theme = CALLOUT_STAR;
            else if (strcmp(theme_str, "check") == 0) theme = CALLOUT_CHECK;
            else if (strcmp(theme_str, "idea") == 0) theme = CALLOUT_IDEA;
            block->data.callout.theme = theme;
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    // =========================================================================
    // CHECKLIST BLOCK COMMANDS
    // =========================================================================

    else if (strcmp(verb, "block_checklist_toggle") == 0 && cmd.argc >= 3) {
        uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
        uint32_t block_id = (uint32_t)atoi(cmd.argv[2]);

        Block *block = find_block(engine, page_id, block_id);
        if (block && block->type == BLOCK_CHECKLIST) {
            block->data.checklist.is_checked = !block->data.checklist.is_checked;
            engine_emit_command(engine, command_line);
            success = true;
        }
    }

    return success;
}

// -----------------------------------------------------------------------------
// History Loading
// -----------------------------------------------------------------------------

void engine_load_history(Engine *engine) {
    if (!engine) return;

    FILE *f = fopen(k_history_path, "r");
    if (!f) return;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        strip_trailing_newlines(line);
        if (is_comment_or_empty(line)) continue;

        // Don't re-emit commands during replay
        FILE *saved = engine->history_file;
        engine->history_file = NULL;

        engine_execute_command(engine, line);

        engine->history_file = saved;
    }

    fclose(f);
}

// -----------------------------------------------------------------------------
// Snapshot Save/Load
// -----------------------------------------------------------------------------

bool engine_save_snapshot_as_aslan(Engine *engine, const char *out_path) {
    if (!engine || !engine->doc || !out_path || !out_path[0]) return false;

    FILE *out = fopen(out_path, "w");
    if (!out) return false;

    doc_save_aof(engine->doc, out);
    fclose(out);
    return true;
}

bool engine_load_snapshot_from_aslan(Engine *engine, const char *in_path) {
    if (!engine || !in_path || !in_path[0]) return false;

    FILE *f = fopen(in_path, "r");
    if (!f) return false;

    // Clear current document
    engine_execute_command(engine, "clear");

    char line[2048];
    bool saw_any = false;

    while (fgets(line, sizeof(line), f)) {
        strip_trailing_newlines(line);
        if (is_comment_or_empty(line)) continue;

        // Skip version header
        if (strncmp(line, "-- aslan", 8) == 0) continue;

        if (engine_execute_command(engine, line)) {
            saw_any = true;
        }
    }

    fclose(f);
    return saw_any;
}
