#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "document/document.h"
#include "scripting/lua_host.h"
#include "platform/platform.h"
#include "core/engine.h"
#include "platform/renderer.h"

// Boucle principale simplifiée
void main_loop(Document* doc) {
    bool running = true;
    int frame = 0;
    
    printf("Entering main loop...\n");
    
    // Simulation d'une boucle d'application (Game Loop pattern)
    // Dans une vraie app, ceci serait piloté par les événements OS (SDL_PollEvent, etc.)
    while (running && frame < 5) { // On limite à 5 frames pour le prototype
        frame++;
        
        // 1. Input (Stub)
        platform_poll_events();
        
        // 2. Update (Logic)
        // scripting_on_update(L, dt);
        
        // 3. Render (Stub)
        // renderer_draw(doc);
        
        printf("Frame %d: App is running...\n", frame);
        platform_sleep(100); // Simule le temps de frame (100ms)
    }
    
    printf("Exiting main loop.\n");
}

// Boucle principale interactive (CLI)
void run_interactive_mode(Engine* engine) {
    char buffer[256];
    
    printf("\n=== Aslan Interactive Shell ===\n");
    printf("Type 'help' for commands.\n");
    
    while (engine->is_running) {
        printf("> ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
        
        // Nettoyage du saut de ligne
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strlen(buffer) == 0) continue;
        
        if (!engine_execute_command(engine, buffer)) {
            printf("Unknown command. Type 'help'.\n");
        }
        
        // Mise à jour automatique de la vue (Preview)
        doc_export_to_svg(engine->doc, "preview.svg");
    }
}

// Boucle principale visuelle
void run_visual_mode(Engine* engine, Renderer* renderer) {
    SDL_Event e;
    Node* dragged_node = NULL;
    float offset_x = 0, offset_y = 0;
    
    printf("\n=== Aslan Visual Mode ===\n");
    printf("Interactions:\n");
    printf("  - Drag & Drop blocks with the mouse\n");
    printf("  - Close the window to exit\n");
    
    while (engine->is_running) {
        // 1. Gestion des événements (Input)
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                engine->is_running = false;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    float mx = (float)e.button.x;
                    float my = (float)e.button.y;
                    dragged_node = doc_find_node_at(engine->doc, mx, my);
                    if (dragged_node) {
                        offset_x = mx - dragged_node->x;
                        offset_y = my - dragged_node->y;
                        printf("Selected: %s (ID: %d)\n", dragged_node->name, dragged_node->id);
                    }
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    dragged_node = NULL;
                }
            }
            else if (e.type == SDL_MOUSEMOTION) {
                if (dragged_node) {
                    dragged_node->x = (float)e.motion.x - offset_x;
                    dragged_node->y = (float)e.motion.y - offset_y;
                }
            }
        }
        
        // 2. Rendu (Se met à jour automatiquement à chaque frame)
        renderer_begin(renderer);
        renderer_draw_document(renderer, engine->doc);
        renderer_end(renderer);
        
        SDL_Delay(16); // ~60 FPS
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv; // Unused
    
    platform_init();
    platform_log("Starting Aslan Engine (CMake Build)...");
    
    // 1. Initialisation de l'Engine (Core)
    Engine* engine = engine_create();
    if (!engine) return 1;
    
    // 2. Initialisation du Renderer (SDL2)
    Renderer* renderer = renderer_create("Aslan Editor v0.1", 1024, 768);
    if (!renderer) {
        platform_log("Failed to create renderer.");
        engine_destroy(engine);
        return 1;
    }

    // 3. Initialisation du Scripting (Logic)
    lua_State* L = scripting_init(engine->doc);
    if (!L) {
        platform_log("Failed to initialize Lua.");
    } else {
        platform_log("Lua scripting engine initialized.");
    }

    // 4. Chargement d'un script de démarrage
    const char* script_path = "scripts/test_tool.lua";
    if (L) {
        platform_log("Running script: %s", script_path);
        scripting_run_file(L, script_path);
    }
    
    // 5. Lancement du mode visuel
    run_visual_mode(engine, renderer);
    
    // 6. Nettoyage
    if (L) scripting_shutdown(L);
    renderer_destroy(renderer);
    engine_destroy(engine);
    platform_shutdown();
    
    return 0;
}
