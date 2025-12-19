#include "engine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Engine* engine_create(void) {
    Engine* engine = (Engine*)calloc(1, sizeof(Engine));
    if (!engine) return NULL;

    engine->doc = doc_create();
    engine->is_running = true;
    
    return engine;
}

void engine_destroy(Engine* engine) {
    if (!engine) return;
    doc_destroy(engine->doc);
    free(engine);
}

// Parsing simple pour le prototype
// Format attendu : "action arg1 arg2 ..."
bool engine_execute_command(Engine* engine, const char* command_line) {
    char cmd[32];
    char name[64];
    float x, y, w, h;
    
    // Copie locale pour tokenization si besoin (ici sscanf suffit pour le proto)
    
    // Commande: exit
    if (strncmp(command_line, "exit", 4) == 0) {
        engine->is_running = false;
        return true;
    }
    
    // Commande: rect <name> <x> <y> <w> <h>
    if (sscanf(command_line, "rect %63s %f %f %f %f", name, &x, &y, &w, &h) == 5) {
        Node* node = doc_create_node(engine->doc, NODE_TYPE_RECT, name);
        node->x = x; node->y = y; node->width = w; node->height = h;
        doc_add_child(engine->doc->root, node);
        printf(">> Created RECT '%s' (ID: %d)\n", name, node->id);
        return true;
    }
    
    // Commande: list
    if (strncmp(command_line, "list", 4) == 0) {
        doc_print_tree(engine->doc);
        return true;
    }

    // Commande: help
    if (strncmp(command_line, "help", 4) == 0) {
        printf("Available commands:\n");
        printf("  rect <name> <x> <y> <w> <h>  : Create a rectangle\n");
        printf("  list                         : Show document tree\n");
        printf("  exit                         : Quit application\n");
        return true;
    }

    return false;
}
