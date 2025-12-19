#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "document/document.h"
#include "scripting/lua_host.h"
#include "platform/platform.h"
#include "core/engine.h"
#include "platform/renderer.h"
#include "platform/ui_host.h"

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
void run_visual_mode(Engine* engine, Renderer* renderer, UIHost* ui) {
    SDL_Event e;
    
    while (engine->is_running) {
        // 1. Gestion des événements (Input)
        nk_input_begin(ui->ctx);
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                engine->is_running = false;
            }
            
            ui_handle_event(ui, &e);
            
            if (!nk_item_is_any_active(ui->ctx)) {
                int imx, imy;
                SDL_GetMouseState(&imx, &imy);
                float mx = (float)imx;
                float my = (float)imy;

                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    scripting_call_event(ui->L, "on_mouse_down", mx, my);
                }
                else if (e.type == SDL_MOUSEBUTTONUP) {
                    scripting_call_event(ui->L, "on_mouse_up", mx, my);
                }
                else if (e.type == SDL_MOUSEMOTION) {
                    scripting_call_event(ui->L, "on_mouse_move", mx, my);
                }
            }
        }
        nk_input_end(ui->ctx);

        // 2. Rendu
        renderer_begin(renderer);
        renderer_draw_document(renderer, engine->doc);
        
        // Rendu de l'UI par dessus
        int ww, wh;
        SDL_GetWindowSize(renderer->window, &ww, &wh);
        ui_render(ui, engine, ww, wh);
        
        renderer_end(renderer);
        
        SDL_Delay(16);
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    platform_init();
    platform_log("Starting Aslan Visual Engine...");
    
    // 1. Initialisation de l'Engine (Core)
    Engine* engine = engine_create();
    if (!engine) return 1;
    
    // 2. Initialisation du Renderer (SDL2)
    Renderer* renderer = renderer_create("Aslan Editor v0.1", 1024, 768);
    if (!renderer) {
        engine_destroy(engine);
        return 1;
    }

    // 3. Initialisation du Scripting (Logic)
    lua_State* L = scripting_init(engine);

    // 4. Initialisation de l'UI (Nuklear)
    UIHost* ui = ui_init(renderer->window, renderer->sdl_renderer, L);
    engine->ui_host = ui; // Lier l'UI à l'engine

    // 5. Création de quelques objets par défaut
    engine_execute_command(engine, "rect Header 10 10 750 60");
    engine_execute_command(engine, "rect Sidebar 10 80 200 678");
    engine_execute_command(engine, "rect Content 220 80 540 678");
    
    // 6. Chargement des scripts
    scripting_run_file(L, "scripts/test_tool.lua");
    scripting_run_file(L, "scripts/tools.lua");

    // 7. Lancement du mode visuel
    run_visual_mode(engine, renderer, ui);

    // 8. Cleanup
    ui_shutdown(ui);
    renderer_destroy(renderer);
    engine_destroy(engine);
    platform_shutdown();
    
    return 0;
}
