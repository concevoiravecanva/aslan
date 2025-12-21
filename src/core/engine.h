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

// Charge et rejoue le fichier history.log
void engine_load_history(Engine* engine);

// Write a canonical snapshot of the current document to a user-chosen path.
// This is the on-disk `.aslan` v1 format (Photoshop-like: single state).
// Returns true on success.
bool engine_save_snapshot_as_aslan(Engine* engine, const char* out_path);

// Load a `.aslan` v1 snapshot from disk (clears current doc).
// Returns true on success.
bool engine_load_snapshot_from_aslan(Engine* engine, const char* in_path);

#endif
