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
    Node* dragged_node = NULL;
    float offset_x = 0, offset_y = 0;
    
    while (engine->is_running) {
        // 1. Gestion des événements (Input)
        nk_input_begin(ui->ctx);
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                engine->is_running = false;
            }
            
            // On laisse Nuklear intercepter les événements en premier
            ui_handle_event(ui, &e);
            
            // Si Nuklear n'a pas capturé la souris, on gère le canvas
            if (!nk_item_is_any_active(ui->ctx)) {
                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        int imx, imy;
                        SDL_GetMouseState(&imx, &imy);
                        float mx = (float)imx;
                        float my = (float)imy;
                        
                        dragged_node = doc_find_node_at(engine->doc, mx, my);
                        ui->selected_node = dragged_node; // Mise à jour de la sélection UI
                        
                        if (dragged_node) {
                            offset_x = mx - dragged_node->x;
                            offset_y = my - dragged_node->y;
                        }
                    }
                }
                else if (e.type == SDL_MOUSEBUTTONUP) {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        dragged_node = NULL;
                    }
                }
            }
        }
        nk_input_end(ui->ctx);

        // Gestion du déplacement
        if (dragged_node && !nk_item_is_any_active(ui->ctx)) {
            int imx, imy;
            Uint32 buttons = SDL_GetMouseState(&imx, &imy);
            if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                dragged_node->x = (float)imx - offset_x;
                dragged_node->y = (float)imy - offset_y;
            } else {
                dragged_node = NULL;
            }
        }
        
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
    lua_State* L = scripting_init(engine->doc);

    // 4. Initialisation de l'UI (Nuklear)
    UIHost* ui = ui_init(renderer->window, renderer->sdl_renderer, L);

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
