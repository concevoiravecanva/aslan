#ifndef ENGINE_H
#define ENGINE_H

#include "../document/document.h"
#include <stdbool.h>
#include <stdio.h>

// Forward declaration
struct UIHost;

struct Engine {
  Document *doc;
  bool is_running;
  struct UIHost *ui_host;
  FILE *history_file;
  bool is_loading;         // If true, commands are not recorded to history
  char doc_path[1024];     // Chemin vers le dossier du document
  char history_path[1100]; // doc_path + "/history.log"
  char redo_path[1100];    // doc_path + "/redo.log"
};

typedef struct Engine Engine;

// Path handling
void engine_set_document_path(Engine *engine, const char *new_path);

// Singleton-like access ou passage de contexte
Engine *engine_create(void);
void engine_destroy(Engine *engine);

// Traitement d'une commande textuelle (venant de Lua ou de la Console)
// Retourne true si la commande a été reconnue
bool engine_execute_command(Engine *engine, const char *command_line);

// Charge et rejoue le fichier history.log
void engine_load_from_history(Engine *engine);

// Write a canonical snapshot of the current document to a user-chosen path.
// This is the on-disk `.aslan` v1 format (Photoshop-like: single state).
// Returns true on success.
bool engine_save_snapshot_as_aslan(Engine *engine, const char *out_path);

// Load a `.aslan` v1 snapshot from disk (clears current doc).
// Returns true on success.
bool engine_load_snapshot_from_aslan(Engine *engine, const char *in_path);

#endif
