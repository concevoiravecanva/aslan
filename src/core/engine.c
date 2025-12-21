#include "engine.h"
#include "../platform/ui_host.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Avoid pulling Lua headers into core; we only need to notify the script host
// that the Engine swapped/rebuilt its Document (undo/redo/rewrite).
void scripting_set_document(Document* doc);

// Layers / z-order helpers (document-linked-list based)
bool doc_set_visible(Document* doc, uint32_t id, int visible);
bool doc_bring_to_front(Document* doc, uint32_t id);
bool doc_send_to_back(Document* doc, uint32_t id);
bool doc_bring_forward(Document* doc, uint32_t id);
bool doc_send_backward(Document* doc, uint32_t id);

// Globals API
bool doc_set_global(Document* doc, const char* key, const char* value);
bool doc_unset_global(Document* doc, const char* key);
const char* doc_get_global(const Document* doc, const char* key);
void doc_globals_iterate(const Document* doc, doc_global_iter_fn fn, void* user);

static void rewrite_emit_global_kv(const char* key, const char* value, void* user) {
    FILE* out = (FILE*)user;
    if (!out || !key || !key[0] || !value) return;

    // Minimal escaping for quotes/backslashes so the line stays parseable.
    char escaped[512];
    size_t wi = 0;
    for (size_t i = 0; value[i] != '\0' && wi + 2 < sizeof(escaped); i++) {
        char c = value[i];
        if (c == '\\' || c == '"') {
            if (wi + 2 >= sizeof(escaped)) break;
            escaped[wi++] = '\\';
            escaped[wi++] = c;
        } else {
            escaped[wi++] = c;
        }
    }
    escaped[wi] = 0;

    fprintf(out, "set_global %s \"%s\"\n", key, escaped);
}

static const char* k_history_path = "history.log";
static const char* k_redo_path = "redo.log";

static void prune_last_session_set_rect_and_append(const char* command_line) {
    if (!command_line) return;
    FILE* f = fopen(k_history_path, "r");
    if (!f) {
        FILE* out = fopen(k_history_path, "a");
        if (out) {
            fprintf(out, "%s\n", command_line);
            fclose(out);
        }
        return;
    }

    char** lines = malloc(4096 * sizeof(char*));
    int count = 0;
    char buf[512];
    while (fgets(buf, sizeof(buf), f) && count < 4096) {
        lines[count++] = strdup(buf);
    }
    fclose(f);

    // Find last session marker
    int last_session = -1;
    for (int i = 0; i < count; i++) {
        if (strstr(lines[i], "-- Session started --") != NULL) last_session = i;
    }

    // Keep only the last set_rect after the last session marker (drop earlier ones), then append new.
    int last_set_rect_after_session = -1;
    for (int i = last_session + 1; i < count; i++) {
        const char* l = lines[i];
        while (*l == ' ' || *l == '\t') l++;
        if (strncmp(l, "set_rect ", 9) == 0) last_set_rect_after_session = i;
    }

    f = fopen(k_history_path, "w");
    if (f) {
        for (int i = 0; i < count; i++) {
            if (i > last_session) {
                const char* l = lines[i];
                while (*l == ' ' || *l == '\t') l++;
                // Drop all but the last set_rect of the session.
                if (strncmp(l, "set_rect ", 9) == 0 && i != last_set_rect_after_session) continue;
            }
            fputs(lines[i], f);
        }
        fprintf(f, "%s\n", command_line);
        fclose(f);
    }

    for (int i = 0; i < count; i++) free(lines[i]);
    free(lines);
}

static void clear_history_log(void) {
    FILE* f = fopen(k_history_path, "w");
    if (f) fclose(f);
}

static bool is_mutating_command_line(const char* line) {
    if (!line) return false;
    const char* l = line;
    while (*l == ' ' || *l == '\t') l++;
    if (*l == '\0' || *l == '\n' || *l == '\r' || *l == '-') return false;

    // Treat only "committed" geometry changes as AOF mutations.
    // High-frequency interactive commands (move/resize) are executed but not logged.
    return (strncmp(l, "rect ", 5) == 0 ||
            strncmp(l, "create_rect ", 12) == 0 ||
            strncmp(l, "set_rect ", 9) == 0 ||
            strncmp(l, "create_text ", 12) == 0 ||
            strncmp(l, "doc_type ", 9) == 0 ||
            strncmp(l, "doc_meta ", 9) == 0 ||
            strncmp(l, "set_global ", 11) == 0 ||
            strncmp(l, "unset_global ", 13) == 0 ||
            strncmp(l, "set_visible ", 12) == 0 ||
            strncmp(l, "bring_to_front ", 15) == 0 ||
            strncmp(l, "send_to_back ", 13) == 0 ||
            strncmp(l, "bring_forward ", 14) == 0 ||
            strncmp(l, "send_backward ", 14) == 0);
}

static void clear_redo_log(void) {
    FILE* f = fopen(k_redo_path, "w");
    if (f) fclose(f);
}

static void gather_nodes_recursive(Node* node, Node** out, int* count, int max) {
    if (!node || !out || !count) return;
    if (node->type != NODE_TYPE_ROOT) {
        if (*count < max) out[(*count)++] = node;
    }
    gather_nodes_recursive(node->first_child, out, count, max);
    gather_nodes_recursive(node->next_sibling, out, count, max);
}

static int cmp_node_id_ptr(const void* a, const void* b) {
    const Node* na = *(const Node* const*)a;
    const Node* nb = *(const Node* const*)b;
    if (na->id < nb->id) return -1;
    if (na->id > nb->id) return 1;
    return 0;
}

static bool engine_rewrite_aof(Engine* engine) {
    if (!engine || !engine->doc || !engine->doc->root) return false;

    // fermer history en cours (sinon windows/mac garde un fd ouvert)
    FILE* saved = engine->history_file;
    if (saved) {
        fclose(saved);
        engine->history_file = NULL;
    }

    // snapshot -> fichier temporaire puis rename (safe)
    const char* tmp_path = "history.tmp";
    FILE* out = fopen(tmp_path, "w");
    if (!out) {
        engine->history_file = fopen(k_history_path, "a");
        return false;
    }

    fprintf(out, "-- Aslan AOF rewrite --\n");

    // Document meta first (so the replay configures the doc before creating items)
    if (engine->doc) {
        fprintf(out, "doc_type %s\n", engine->doc->doc_type[0] ? engine->doc->doc_type : "canvas2d");
        fprintf(out, "doc_meta width %d\n", engine->doc->doc_width);
        fprintf(out, "doc_meta height %d\n", engine->doc->doc_height);
    }

    // Document globals
    if (engine->doc) {
        doc_globals_iterate(engine->doc, rewrite_emit_global_kv, out);
    }

    // 1) Snapshot objects deterministically by id
    Node* nodes[4096];
    int count = 0;
    gather_nodes_recursive(engine->doc->root, nodes, &count, 4096);
    qsort(nodes, (size_t)count, sizeof(Node*), cmp_node_id_ptr);

    for (int i = 0; i < count; i++) {
        Node* n = nodes[i];
        if (!n) continue;
        if (n->type == NODE_TYPE_RECT) {
            fprintf(out, "create_rect %u %s %f %f %f %f\n",
                (unsigned)n->id, n->name, n->x, n->y, n->width, n->height);
        } else if (n->type == NODE_TYPE_TEXT) {
            fprintf(out, "create_text %u %s %f %f %f %f \"%s\"\n",
                (unsigned)n->id, n->name, n->x, n->y, n->width, n->height, n->text);
        }
    }

    // 2) Persist visibility state (only when hidden)
    for (int i = 0; i < count; i++) {
        Node* n = nodes[i];
        if (!n) continue;
        if (n->visible == 0) {
            fprintf(out, "set_visible %u 0\n", (unsigned)n->id);
        }
    }

    // 3) Persist z-order: last sibling is front.
    // We encode it by iterating siblings from back->front and bringing each to front.
    // This is deterministic and keeps the command log small.
    Node* child = engine->doc->root ? engine->doc->root->first_child : NULL;
    // collect in array for reverse traversal
    Node* stack[4096];
    int scount = 0;
    while (child && scount < 4096) {
        stack[scount++] = child;
        child = child->next_sibling;
    }
    for (int i = 0; i < scount; i++) {
        // bringing in existing order replays to same order
        fprintf(out, "bring_to_front %u\n", (unsigned)stack[i]->id);
    }

    fclose(out);

    // replace history
    remove(k_history_path);
    if (rename(tmp_path, k_history_path) != 0) {
        // fallback: keep tmp for inspection
        engine->history_file = fopen(k_history_path, "a");
        return false;
    }

    // redo invalid after rewrite
    clear_redo_log();

    if (engine->ui_host) engine->ui_host->selected_node = NULL;
    scripting_set_document(engine->doc);
    engine->history_file = fopen(k_history_path, "a");
    return engine->history_file != NULL;
}

static bool engine_write_snapshot_to_file(Engine* engine, FILE* out, const char* header_line) {
    if (!engine || !engine->doc || !engine->doc->root || !out) return false;

    if (header_line && header_line[0]) {
        fprintf(out, "%s\n", header_line);
    }

    // Document meta first (so the replay configures the doc before creating items)
    fprintf(out, "doc_type %s\n", engine->doc->doc_type[0] ? engine->doc->doc_type : "canvas2d");
    fprintf(out, "doc_meta width %d\n", engine->doc->doc_width);
    fprintf(out, "doc_meta height %d\n", engine->doc->doc_height);

    // Document globals
    doc_globals_iterate(engine->doc, rewrite_emit_global_kv, out);

    // 1) Snapshot objects deterministically by id
    Node* nodes[4096];
    int count = 0;
    gather_nodes_recursive(engine->doc->root, nodes, &count, 4096);
    qsort(nodes, (size_t)count, sizeof(Node*), cmp_node_id_ptr);

    for (int i = 0; i < count; i++) {
        Node* n = nodes[i];
        if (!n) continue;
        if (n->type == NODE_TYPE_RECT) {
            fprintf(out, "create_rect %u %s %f %f %f %f\n",
                (unsigned)n->id, n->name, n->x, n->y, n->width, n->height);
        } else if (n->type == NODE_TYPE_TEXT) {
            fprintf(out, "create_text %u %s %f %f %f %f \"%s\"\n",
                (unsigned)n->id, n->name, n->x, n->y, n->width, n->height, n->text);
        }
    }

    // 2) Persist visibility state (only when hidden)
    for (int i = 0; i < count; i++) {
        Node* n = nodes[i];
        if (!n) continue;
        if (n->visible == 0) {
            fprintf(out, "set_visible %u 0\n", (unsigned)n->id);
        }
    }

    // 3) Persist z-order: last sibling is front.
    // We encode it by iterating siblings from back->front and bringing each to front.
    Node* child = engine->doc->root ? engine->doc->root->first_child : NULL;
    Node* stack[4096];
    int scount = 0;
    while (child && scount < 4096) {
        stack[scount++] = child;
        child = child->next_sibling;
    }
    for (int i = 0; i < scount; i++) {
        fprintf(out, "bring_to_front %u\n", (unsigned)stack[i]->id);
    }

    return true;
}

bool engine_save_snapshot_as_aslan(Engine* engine, const char* out_path) {
    if (!engine || !engine->doc || !engine->doc->root || !out_path || !out_path[0]) return false;

    FILE* out = fopen(out_path, "w");
    if (!out) return false;

    bool ok = engine_write_snapshot_to_file(engine, out, "-- aslan:1 --");
    fclose(out);
    return ok;
}

Engine* engine_create(void) {
    Engine* engine = (Engine*)calloc(1, sizeof(Engine));
    if (!engine) return NULL;

    engine->doc = doc_create();
    engine->is_running = true;
    
    // Ouverture du journal en mode "append"
    engine->history_file = fopen(k_history_path, "a");
    if (engine->history_file) {
        fprintf(engine->history_file, "-- Session started --\n");
        fflush(engine->history_file);
    }
    
    return engine;
}

void engine_destroy(Engine* engine) {
    if (!engine) return;
    if (engine->history_file) fclose(engine->history_file);
    doc_destroy(engine->doc);
    free(engine);
}

// Parsing simple pour le prototype
// Format attendu : "action arg1 arg2 ..."
bool engine_execute_command(Engine* engine, const char* command_line) {
    char name[64];
    float x, y, w, h;
    int id;
    int value;
    bool success = false;
    
    // Commande: exit
    if (strncmp(command_line, "exit", 4) == 0) {
        engine->is_running = false;
        success = true;
    }

    // Commande: clear
    // Reset document + logs (history/redo) to a pristine state.
    else if (strncmp(command_line, "clear", 5) == 0) {
        // Close AOF
        if (engine->history_file) {
            fclose(engine->history_file);
            engine->history_file = NULL;
        }

        // Reset logs
        clear_history_log();
        clear_redo_log();

        // Reset document
        doc_destroy(engine->doc);
        engine->doc = doc_create();

    // Default meta already set by doc_create()

        // Reset UI selection
        if (engine->ui_host) engine->ui_host->selected_node = NULL;

        // Sync Lua host with the new document
        scripting_set_document(engine->doc);

        // Reopen AOF for appends
        engine->history_file = fopen(k_history_path, "a");
        if (engine->history_file) {
            fprintf(engine->history_file, "-- Session started --\n");
            fflush(engine->history_file);
        }
    printf(">> Cleared document and history.\n");
        success = true;
    }

    // Commande: doc_type <type>
    else if (sscanf(command_line, "doc_type %31s", name) == 1) {
        if (engine->doc) {
            memset(engine->doc->doc_type, 0, sizeof(engine->doc->doc_type));
            strncpy(engine->doc->doc_type, name, sizeof(engine->doc->doc_type) - 1);
            success = true;
        }
    }

    // Commande: doc_meta width|height <int>
    else if (sscanf(command_line, "doc_meta %31s %d", name, &id) == 2) {
        if (engine->doc) {
            if (strcmp(name, "width") == 0) {
                engine->doc->doc_width = id;
                success = true;
            } else if (strcmp(name, "height") == 0) {
                engine->doc->doc_height = id;
                success = true;
            }
        }
    }

    // Commande: set_global <key> "value"
    else if (strncmp(command_line, "set_global", 10) == 0) {
        char key[64];
        char value_str[256];
        key[0] = 0;
        value_str[0] = 0;

        if (sscanf(command_line, "set_global %63s \"%255[^\"]\"", key, value_str) == 2) {
            success = doc_set_global(engine->doc, key, value_str);
        }
    }

    // Commande: unset_global <key>
    else if (sscanf(command_line, "unset_global %63s", name) == 1) {
        success = doc_unset_global(engine->doc, name);
    }

    // Commande: new_rect [x y w h]
    // Convenience helper: allocate a new id, create a rect with deterministic name Rect<id>,
    // and select it. This should result in a single AOF entry (create_rect).
    else if (strncmp(command_line, "new_rect", 8) == 0) {
        // Defaults
        float nx = 100.0f, ny = 100.0f, nw = 120.0f, nh = 120.0f;
        // Optional args
        float ax, ay, aw, ah;
        if (sscanf(command_line, "new_rect %f %f %f %f", &ax, &ay, &aw, &ah) == 4) {
            nx = ax; ny = ay; nw = aw; nh = ah;
        }

        uint32_t new_id = engine->doc ? engine->doc->next_node_id : 1;
        snprintf(name, sizeof(name), "Rect%u", (unsigned)new_id);

        Node* node = doc_create_node(engine->doc, NODE_TYPE_RECT, name);
        if (node) {
            node->x = nx;
            node->y = ny;
            node->width = nw;
            node->height = nh;
            doc_add_child(engine->doc->root, node);
            // Ensure deterministic name matches actual assigned id
            snprintf(node->name, sizeof(node->name), "Rect%u", (unsigned)node->id);

            // Select (not logged by design)
            if (engine->ui_host) engine->ui_host->selected_node = node;
            printf(">> New rect created: %s (ID: %u)\n", node->name, (unsigned)node->id);
            success = true;

            // Write as create_rect entry (deterministic AOF)
            if (engine->history_file) {
                fprintf(engine->history_file, "create_rect %u %s %f %f %f %f\n",
                    (unsigned)node->id, node->name, node->x, node->y, node->width, node->height);
                fflush(engine->history_file);
            }

            // New mutations invalidate redo
            clear_redo_log();
        }

        return success;
    }

    // Commande: new_text [x y w h "content"]
    // Creates a TEXT node and logs a single create_text line.
    else if (strncmp(command_line, "new_text", 8) == 0) {
        float nx = 120.0f, ny = 120.0f, nw = 220.0f, nh = 60.0f;
        char content[256];
        content[0] = 0;

        // Try to parse: new_text x y w h "..."
        // If no content, default.
        float ax, ay, aw, ah;
        int matched = sscanf(command_line, "new_text %f %f %f %f \"%255[^\"]\"", &ax, &ay, &aw, &ah, content);
        if (matched >= 4) {
            nx = ax; ny = ay; nw = aw; nh = ah;
        }
        if (content[0] == 0) {
            strncpy(content, "Text", sizeof(content) - 1);
            content[sizeof(content) - 1] = 0;
        }

        uint32_t new_id = engine->doc ? engine->doc->next_node_id : 1;
        snprintf(name, sizeof(name), "Text%u", (unsigned)new_id);

        Node* node = doc_create_node(engine->doc, NODE_TYPE_TEXT, name);
        if (node) {
            node->x = nx;
            node->y = ny;
            node->width = nw;
            node->height = nh;
            strncpy(node->text, content, sizeof(node->text) - 1);
            node->text[sizeof(node->text) - 1] = 0;
            // Keep deterministic names in sync
            snprintf(node->name, sizeof(node->name), "Text%u", (unsigned)node->id);
            doc_add_child(engine->doc->root, node);

            if (engine->ui_host) engine->ui_host->selected_node = node;
            success = true;

            if (engine->history_file) {
                fprintf(engine->history_file, "create_text %u %s %f %f %f %f \"%s\"\n",
                    (unsigned)node->id, node->name, node->x, node->y, node->width, node->height, node->text);
                fflush(engine->history_file);
            }
            clear_redo_log();
        }
        return success;
    }
    
    // Commande: rect <name> <x> <y> <w> <h>
    else if (sscanf(command_line, "rect %63s %f %f %f %f", name, &x, &y, &w, &h) == 5) {
        Node* node = doc_create_node(engine->doc, NODE_TYPE_RECT, name);
        node->x = x; node->y = y; node->width = w; node->height = h;
        doc_add_child(engine->doc->root, node);
        printf(">> Created RECT '%s' (ID: %d)\n", name, node->id);
        success = true;
    }

    // Commande: create_rect <id> <name> <x> <y> <w> <h>
    // Création déterministe avec ID explicite (AOF-friendly)
    else if (sscanf(command_line, "create_rect %d %63s %f %f %f %f", &id, name, &x, &y, &w, &h) == 6) {
        if (id > 0 && !doc_find_node_by_id(engine->doc, (uint32_t)id)) {
            Node* node = doc_create_node(engine->doc, NODE_TYPE_RECT, name);
            // Forcer l'ID
            node->id = (uint32_t)id;

            // Maintenir next_node_id pour éviter les collisions futures
            if (engine->doc->next_node_id <= (uint32_t)id) {
                engine->doc->next_node_id = (uint32_t)id + 1;
            }

            node->x = x;
            node->y = y;
            node->width = w;
            node->height = h;
            doc_add_child(engine->doc->root, node);
            success = true;
        }
    }

    // Commande: create_text <id> <name> <x> <y> <w> <h> "content"
    else if (strncmp(command_line, "create_text", 11) == 0) {
        char content[256];
        content[0] = 0;
        if (sscanf(command_line, "create_text %d %63s %f %f %f %f \"%255[^\"]\"", &id, name, &x, &y, &w, &h, content) >= 6) {
            if (id > 0 && !doc_find_node_by_id(engine->doc, (uint32_t)id)) {
                Node* node = doc_create_node(engine->doc, NODE_TYPE_TEXT, name);
                node->id = (uint32_t)id;
                if (engine->doc->next_node_id <= (uint32_t)id) {
                    engine->doc->next_node_id = (uint32_t)id + 1;
                }
                node->x = x;
                node->y = y;
                node->width = w;
                node->height = h;
                strncpy(node->text, content, sizeof(node->text) - 1);
                node->text[sizeof(node->text) - 1] = 0;
                doc_add_child(engine->doc->root, node);
                success = true;
            }
        }
    }

    // Commande: set_visible <id> <0|1>
    else if (sscanf(command_line, "set_visible %d %d", &id, &value) == 2) {
        if (id > 0) {
            success = doc_set_visible(engine->doc, (uint32_t)id, value);
        }
    }

    // Commande: bring_to_front <id>
    else if (sscanf(command_line, "bring_to_front %d", &id) == 1) {
        if (id > 0) success = doc_bring_to_front(engine->doc, (uint32_t)id);
    }

    // Commande: send_to_back <id>
    else if (sscanf(command_line, "send_to_back %d", &id) == 1) {
        if (id > 0) success = doc_send_to_back(engine->doc, (uint32_t)id);
    }

    // Commande: bring_forward <id>
    else if (sscanf(command_line, "bring_forward %d", &id) == 1) {
        if (id > 0) success = doc_bring_forward(engine->doc, (uint32_t)id);
    }

    // Commande: send_backward <id>
    else if (sscanf(command_line, "send_backward %d", &id) == 1) {
        if (id > 0) success = doc_send_backward(engine->doc, (uint32_t)id);
    }

    // Commande: set_rect <id> <x> <y> <w> <h>
    else if (sscanf(command_line, "set_rect %d %f %f %f %f", &id, &x, &y, &w, &h) == 5) {
        Node* node = doc_find_node_by_id(engine->doc, (uint32_t)id);
        if (node) {
            // Snap-to-grid (AOF-driven via globals)
            int snap_enabled = 0;
            int snap_step = 10;
            const char* se = doc_get_global(engine->doc, "snap.enabled");
            if (se && (strcmp(se, "1") == 0 || strcasecmp(se, "true") == 0 || strcasecmp(se, "yes") == 0)) {
                snap_enabled = 1;
            }
            const char* ss = doc_get_global(engine->doc, "snap.step");
            if (ss) {
                int v = atoi(ss);
                if (v >= 1 && v <= 200) snap_step = v;
            }

            if (snap_enabled) {
                float s = (float)snap_step;
                x = (float)(int)((x / s) + (x >= 0 ? 0.5f : -0.5f)) * s;
                y = (float)(int)((y / s) + (y >= 0 ? 0.5f : -0.5f)) * s;
                w = (float)(int)((w / s) + 0.5f) * s;
                h = (float)(int)((h / s) + 0.5f) * s;
                if (w < s) w = s;
                if (h < s) h = s;
            }

            node->x = x;
            node->y = y;
            node->width = w;
            node->height = h;
            success = true;

            // Log exactly one set_rect per session (commit), keep AOF compact.
            if (engine->history_file) {
                char canonical[256];
                snprintf(canonical, sizeof(canonical), "set_rect %u %f %f %f %f",
                    (unsigned)node->id, node->x, node->y, node->width, node->height);
                prune_last_session_set_rect_and_append(canonical);
            }
        }
    }

    // Commande: move <id> <x> <y>
    else if (sscanf(command_line, "move %d %f %f", &id, &x, &y) == 3) {
        Node* node = doc_find_node_by_id(engine->doc, (uint32_t)id);
        if (node) {
            node->x = x;
            node->y = y;
            success = true;
        }
    }

    // Commande: resize <id> <w> <h>
    else if (sscanf(command_line, "resize %d %f %f", &id, &w, &h) == 3) {
        Node* node = doc_find_node_by_id(engine->doc, (uint32_t)id);
        if (node) {
            node->width = w;
            node->height = h;
            success = true;
        }
    }

    // Commande: select <id>
    else if (sscanf(command_line, "select %d", &id) == 1) {
        Node* node = doc_find_node_by_id(engine->doc, (uint32_t)id);
        if (node && engine->ui_host) {
            engine->ui_host->selected_node = node;
        }
        printf(">> Selected node ID: %d\n", id);
        success = true;
    }
    
    // Commande: list
    else if (strncmp(command_line, "list", 4) == 0) {
        doc_print_tree(engine->doc);
        success = true;
    }

    // Commande: help
    else if (strncmp(command_line, "help", 4) == 0) {
        printf("Available commands:\n");
        printf("  rect <name> <x> <y> <w> <h>  : Create a rectangle\n");
    printf("  new_rect [x y w h]           : Create a new rectangle (AOF: create_rect)\n");
    printf("  new_text [x y w h \"txt\"]    : Create a new text box (AOF: create_text)\n");
        printf("  set_rect <id> <x> <y> <w> <h>: Set full rect geometry\n");
        printf("  rewrite_aof                  : Compact history.log (snapshot)\n");
    printf("  move <id> <x> <y>            : Move a node (interactive; not logged)\n");
    printf("  resize <id> <w> <h>          : Resize a node (interactive; not logged)\n");
        printf("  select <id>                  : Select a node\n");
        printf("  undo                         : Undo last command\n");
        printf("  redo                         : Redo last undone command\n");
    printf("  clear                        : New empty document (clears history/redo)\n");
        printf("  list                         : Show document tree\n");
        printf("  exit                         : Quit application\n");
        success = true;
    }

    // Commande: rewrite_aof
    else if (strncmp(command_line, "rewrite_aof", 11) == 0) {
        success = engine_rewrite_aof(engine);
        if (success) {
            printf(">> AOF rewritten to '%s'\n", k_history_path);
        }
        return true; // pas de log
    }

    // Commande: redo
    else if (strncmp(command_line, "redo", 4) == 0) {
        // On prend la dernière commande mutante dans redo.log, on la remet dans history.log, puis on replay.
        if (engine->history_file) {
            fclose(engine->history_file);
            engine->history_file = NULL;
        }

        FILE* rf = fopen(k_redo_path, "r");
        if (!rf) return true;

        char** lines = malloc(1000 * sizeof(char*));
        int count = 0;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), rf) && count < 1000) {
            lines[count++] = strdup(buffer);
        }
        fclose(rf);

        int last_mutating = -1;
        for (int i = 0; i < count; i++) {
            if (is_mutating_command_line(lines[i])) last_mutating = i;
        }

        if (last_mutating >= 0) {
            // Append to history
            FILE* hf = fopen(k_history_path, "a");
            if (hf) {
                // ensure single newline
                fprintf(hf, "%s", lines[last_mutating]);
                if (lines[last_mutating][strlen(lines[last_mutating]) - 1] != '\n') {
                    fprintf(hf, "\n");
                }
                fclose(hf);
            }

            // Rewrite redo without that line
            rf = fopen(k_redo_path, "w");
            if (rf) {
                for (int i = 0; i < count; i++) {
                    if (i == last_mutating) continue;
                    fprintf(rf, "%s", lines[i]);
                }
                fclose(rf);
            }

            // Reload document from history
            doc_destroy(engine->doc);
            engine->doc = doc_create();
            engine_load_history(engine);

            if (engine->ui_host) engine->ui_host->selected_node = NULL;
            scripting_set_document(engine->doc);
        }

        for (int i = 0; i < count; i++) free(lines[i]);
        free(lines);

        engine->history_file = fopen(k_history_path, "a");
        return true;
    }

    // Commande: undo
    else if (strncmp(command_line, "undo", 4) == 0) {
        // L'undo dans un AOF est simple : on ferme le fichier, on le tronque d'une ligne, et on recharge.
        // Version améliorée: on retire la dernière commande *mutante* (ignore commentaires / select / help / list ...)
        if (engine->history_file) {
            fclose(engine->history_file);
            engine->history_file = NULL;
            
            // Lecture de toutes les lignes (history)
            FILE* f = fopen(k_history_path, "r");
            if (f) {
                char** lines = malloc(1000 * sizeof(char*));
                int count = 0;
                char buffer[256];
                while (fgets(buffer, sizeof(buffer), f) && count < 1000) {
                    lines[count++] = strdup(buffer);
                }
                fclose(f);
                
                // On cherche la dernière commande mutante
                int last_mutating = -1;
                for (int i = 0; i < count; i++) {
                    if (is_mutating_command_line(lines[i])) last_mutating = i;
                }

                // Si on a trouvé une ligne mutante, on la pousse dans redo.log
                if (last_mutating >= 0) {
                    FILE* redo = fopen(k_redo_path, "a");
                    if (redo) {
                        fprintf(redo, "%s", lines[last_mutating]);
                        if (lines[last_mutating][strlen(lines[last_mutating]) - 1] != '\n') {
                            fprintf(redo, "\n");
                        }
                        fclose(redo);
                    }
                }

                // Réécriture de toutes les lignes sauf la dernière commande mutante
                f = fopen(k_history_path, "w");
                if (f) {
                    for (int i = 0; i < count; i++) {
                        if (i == last_mutating) continue;
                        fprintf(f, "%s", lines[i]);
                    }
                    fclose(f);
                }
                
                for (int i = 0; i < count; i++) free(lines[i]);
                free(lines);
            }
            
            // Réinitialiser le document et rejouer
            doc_destroy(engine->doc);
            engine->doc = doc_create();
            engine_load_history(engine);

            if (engine->ui_host) engine->ui_host->selected_node = NULL;
            scripting_set_document(engine->doc);
            
            // Réouvrir le log en append
            engine->history_file = fopen(k_history_path, "a");
        }
        return true; // On ne log pas l'undo lui-même
    }

    // Si c'est une commande mutante, toute nouvelle action invalide la pile redo
    if (success && is_mutating_command_line(command_line)) {
        clear_redo_log();
    }

    // Si la commande a réussi et n'est pas une commande de lecture (list/help), on l'enregistre
    if (success && engine->history_file && 
        strncmp(command_line, "list", 4) != 0 && 
        strncmp(command_line, "help", 4) != 0 &&
    strncmp(command_line, "select", 6) != 0 &&
    strncmp(command_line, "new_rect", 8) != 0 &&
    strncmp(command_line, "new_text", 8) != 0 &&
    strncmp(command_line, "set_rect", 8) != 0 &&
    strncmp(command_line, "move", 4) != 0 &&
    strncmp(command_line, "resize", 6) != 0 &&
        strncmp(command_line, "exit", 4) != 0) {
        fprintf(engine->history_file, "%s\n", command_line);
        fflush(engine->history_file);
    }
    
    return success;
}

void engine_load_history(Engine* engine) {
    FILE* f = fopen(k_history_path, "r");
    if (!f) return;

    printf(">> Replaying history...\n");
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Enlever le saut de ligne
        line[strcspn(line, "\r\n")] = 0;
        
        // Ignorer les commentaires ou lignes vides
        if (line[0] == '-' || line[0] == '\0') continue;
        
        // Exécuter sans ré-enregistrer (on passe NULL pour history_file temporairement ou on gère dans execute)
        // Pour faire simple ici, on appelle engine_execute_command mais on désactive le log
        FILE* temp = engine->history_file;
        engine->history_file = NULL;
        engine_execute_command(engine, line);
        engine->history_file = temp;
    }
    fclose(f);
    printf(">> History replayed.\n");
}
