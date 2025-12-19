#ifndef ENGINE_H
#define ENGINE_H

#include "../document/document.h"
#include <stdbool.h>
#include <stdio.h>

// Forward declaration
struct UIHost;

struct Engine {
    Document* doc;
    bool is_running;
    struct UIHost* ui_host; // Référence vers l'UI pour la sélection
    FILE* history_file; // Fichier pour le journal des commandes (AOF)
};

typedef struct Engine Engine;

// Singleton-like access ou passage de contexte
Engine* engine_create(void);
void engine_destroy(Engine* engine);

// Traitement d'une commande textuelle (venant de Lua ou de la Console)
// Retourne true si la commande a été reconnue
bool engine_execute_command(Engine* engine, const char* command_line);

#endif
