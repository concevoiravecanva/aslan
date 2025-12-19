#ifndef ENGINE_H
#define ENGINE_H

#include "../document/document.h"
#include <stdbool.h>

typedef struct {
    Document* doc;
    bool is_running;
    // Plus tard : Window* window, LuaState* L, etc.
} Engine;

// Singleton-like access ou passage de contexte
Engine* engine_create(void);
void engine_destroy(Engine* engine);

// Traitement d'une commande textuelle (venant de Lua ou de la Console)
// Retourne true si la commande a été reconnue
bool engine_execute_command(Engine* engine, const char* command_line);

#endif
