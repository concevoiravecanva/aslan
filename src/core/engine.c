#include "engine.h"
#include "../platform/ui_host.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Engine* engine_create(void) {
    Engine* engine = (Engine*)calloc(1, sizeof(Engine));
    if (!engine) return NULL;

    engine->doc = doc_create();
    engine->is_running = true;
    
    // Ouverture du journal en mode "append"
    engine->history_file = fopen("history.log", "a");
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
        printf("  move <id> <x> <y>            : Move a node\n");
        printf("  resize <id> <w> <h>          : Resize a node\n");
        printf("  select <id>                  : Select a node\n");
        printf("  undo                         : Undo last command\n");
        printf("  list                         : Show document tree\n");
        printf("  exit                         : Quit application\n");
        success = true;
    }

    // Commande: undo
    else if (strncmp(command_line, "undo", 4) == 0) {
        // L'undo dans un AOF est simple : on ferme le fichier, on le tronque d'une ligne, et on recharge.
        // Pour faire simple ici : on va lire tout le fichier sauf la dernière ligne, 
        // le réécrire, et recharger l'état.
        if (engine->history_file) {
            fclose(engine->history_file);
            engine->history_file = NULL;
            
            // Lecture de toutes les lignes
            FILE* f = fopen("history.log", "r");
            if (f) {
                char** lines = malloc(1000 * sizeof(char*));
                int count = 0;
                char buffer[256];
                while (fgets(buffer, sizeof(buffer), f) && count < 1000) {
                    lines[count++] = strdup(buffer);
                }
                fclose(f);
                
                // Réécriture de toutes les lignes sauf la dernière (si elle n'est pas un commentaire)
                f = fopen("history.log", "w");
                if (f) {
                    // On cherche la dernière ligne qui n'est pas un commentaire
                    int last_valid = -1;
                    for (int i = 0; i < count; i++) {
                        if (lines[i][0] != '-' && lines[i][0] != '\n') last_valid = i;
                    }
                    
                    for (int i = 0; i < count; i++) {
                        if (i == last_valid) continue; 
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
            engine->history_file = fopen("history.log", "a");
        }
        return true; // On ne log pas l'undo lui-même
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
    FILE* f = fopen("history.log", "r");
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
