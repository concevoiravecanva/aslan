#include "engine.h"
#include "../platform/ui_host.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char* k_history_path = "history.log";
static const char* k_redo_path = "redo.log";

static bool is_mutating_command_line(const char* line) {
    if (!line) return false;
    const char* l = line;
    while (*l == ' ' || *l == '\t') l++;
    if (*l == '\0' || *l == '\n' || *l == '\r' || *l == '-') return false;

    return (strncmp(l, "rect ", 5) == 0 ||
            strncmp(l, "create_rect ", 12) == 0 ||
            strncmp(l, "set_rect ", 9) == 0 ||
            strncmp(l, "move ", 5) == 0 ||
            strncmp(l, "resize ", 7) == 0);
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

    // collect nodes, sort by id for determinism
    Node* nodes[4096];
    int count = 0;
    gather_nodes_recursive(engine->doc->root, nodes, &count, 4096);
    qsort(nodes, (size_t)count, sizeof(Node*), cmp_node_id_ptr);

    for (int i = 0; i < count; i++) {
        Node* n = nodes[i];
        if (!n) continue;
        if (n->type == NODE_TYPE_RECT) {
            // name is safe (no spaces) in current model
            fprintf(out, "create_rect %u %s %f %f %f %f\n",
                (unsigned)n->id, n->name, n->x, n->y, n->width, n->height);
        }
        // TODO: other node types (text/image) later
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

    engine->history_file = fopen(k_history_path, "a");
    return engine->history_file != NULL;
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
    bool success = false;
    
    // Commande: exit
    if (strncmp(command_line, "exit", 4) == 0) {
        engine->is_running = false;
        success = true;
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

    // Commande: set_rect <id> <x> <y> <w> <h>
    else if (sscanf(command_line, "set_rect %d %f %f %f %f", &id, &x, &y, &w, &h) == 5) {
        Node* node = doc_find_node_by_id(engine->doc, (uint32_t)id);
        if (node) {
            node->x = x;
            node->y = y;
            node->width = w;
            node->height = h;
            success = true;
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
        printf("  set_rect <id> <x> <y> <w> <h>: Set full rect geometry\n");
        printf("  rewrite_aof                  : Compact history.log (snapshot)\n");
        printf("  move <id> <x> <y>            : Move a node\n");
        printf("  resize <id> <w> <h>          : Resize a node\n");
        printf("  select <id>                  : Select a node\n");
        printf("  undo                         : Undo last command\n");
        printf("  redo                         : Redo last undone command\n");
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
