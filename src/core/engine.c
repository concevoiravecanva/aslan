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
        printf("  move <id> <x> <y>            : Move a node\n");
        printf("  resize <id> <w> <h>          : Resize a node\n");
        printf("  select <id>                  : Select a node\n");
        printf("  list                         : Show document tree\n");
        printf("  exit                         : Quit application\n");
        success = true;
    }

    // Si la commande a réussi et qu'elle modifie l'état, on la logue
    if (success && engine->history_file && 
        (strncmp(command_line, "rect", 4) == 0 || 
         strncmp(command_line, "move", 4) == 0 || 
         strncmp(command_line, "resize", 4) == 0)) {
        fprintf(engine->history_file, "%s\n", command_line);
        fflush(engine->history_file);
    }

    return success;
}
