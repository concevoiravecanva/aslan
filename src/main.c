#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "document/document.h"
#include "scripting/lua_host.h"
#include "platform/platform.h"
#include "core/engine.h"
#include "platform/renderer.h"
#include "platform/ui_host.h"

#include <libgen.h>
#include <limits.h>

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

    // Double-click detection (Antirez-style: small, local state)
    static uint32_t last_click_ms = 0;
    static float last_click_world_x = 0.0f;
    static float last_click_world_y = 0.0f;
    const uint32_t dbl_click_ms = 350;
    const float dbl_click_dist = 6.0f; // world units (good enough at typical zooms)
    
    while (engine->is_running) {
        // 1. Gestion des événements (Input)
        nk_input_begin(ui->ctx);
        while (SDL_PollEvent(&e) != 0) {
            bool note_consumed_wheel = false;
            if (e.type == SDL_QUIT) {
                engine->is_running = false;
            }

            // Raccourcis clavier (macOS): Cmd+Z (Undo), Cmd+Shift+Z (Redo)
            if (e.type == SDL_KEYDOWN && !nk_item_is_any_active(ui->ctx)) {
                SDL_Keymod mods = SDL_GetModState();
                bool cmd = (mods & KMOD_GUI) != 0;
                bool shift = (mods & KMOD_SHIFT) != 0;
                if (cmd && e.key.keysym.sym == SDLK_z) {
                    engine_execute_command(engine, shift ? "redo" : "undo");
                }
            }
            
            ui_handle_event(ui, &e);

            // NOTE pages: use mouse wheel for vertical scroll (Notion-like).
            // Consume wheel here so it doesn't become zoom/pan in Lua tools.
            if (engine && engine->doc && e.type == SDL_MOUSEWHEEL) {
                bool note_captures_mouse = false;
                uint32_t note_pid = 0;
                AslanNoteDoc* note_doc = NULL;
                uint32_t pid = doc_page_active_id(engine->doc);
                for (int pi = 0; pi < 32; pi++) {
                    if (!engine->doc->pages[pi].in_use) continue;
                    if (engine->doc->pages[pi].page_id != pid) continue;
                    if (engine->doc->pages[pi].type == PAGE_TYPE_NOTE && engine->doc->pages[pi].note) {
                        // SDL wheel: y > 0 is up. We want scroll_y to decrease when wheel up.
                        float dy = (float)e.wheel.y * 40.0f;
                        float ny = engine->doc->pages[pi].note->scroll_y - dy;
                        if (ny < 0.0f) ny = 0.0f;
                        engine->doc->pages[pi].note->scroll_y = ny;
                        note_consumed_wheel = true;
                    }
                    break;
                }
            }

            // Notes input: when a NOTE page is active, consume keyboard/text input
            // (Kilo-like navigation + text input) before Lua tool routing.
            if (engine && engine->doc && !nk_item_is_any_active(ui->ctx) && (!ui || ui->editing_node_id == 0)) {
                uint32_t pid = doc_page_active_id(engine->doc);
                for (int pi = 0; pi < 32; pi++) {
                    if (!engine->doc->pages[pi].in_use) continue;
                    if (engine->doc->pages[pi].page_id != pid) continue;
                    if (engine->doc->pages[pi].type != PAGE_TYPE_NOTE) break;

                    // Ensure text input is enabled for SDL_TEXTINPUT events.
                    if (!SDL_IsTextInputActive()) SDL_StartTextInput();

                    if (engine->doc->pages[pi].note) {
                        if (e.type == SDL_KEYDOWN || e.type == SDL_TEXTINPUT) {
                            AslanNoteDoc* nd = engine->doc->pages[pi].note;
                            int before_row = nd->cursor_row;

                            int consumed = note_handle_input(nd, &e);

                            // Persist edits: for v1 we conservatively snapshot the line that changed.
                            // SDL_TEXTINPUT always mutates the current line.
                            // ENTER creates a new line: persist both previous line and new line.
                            // BACKSPACE could join lines: persist the current line after the operation.
                            if (consumed && engine->history_file) {
                                if (e.type == SDL_TEXTINPUT) {
                                    if (nd->cursor_row >= 0 && nd->cursor_row < nd->row_count) {
                                        const char* t = nd->rows[nd->cursor_row].chars ? nd->rows[nd->cursor_row].chars : "";
                                        fprintf(engine->history_file, "note_set_ln %u %d \"%s\"\n", (unsigned)pid, nd->cursor_row, t);
                                        fflush(engine->history_file);
                                    }
                                } else if (e.type == SDL_KEYDOWN) {
                                    SDL_Keycode sym = e.key.keysym.sym;
                                    if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                                        // After insert_newline, cursor is on new line.
                                        int new_row = nd->cursor_row;
                                        int prev_row = new_row - 1;
                                        if (prev_row >= 0 && prev_row < nd->row_count) {
                                            const char* t = nd->rows[prev_row].chars ? nd->rows[prev_row].chars : "";
                                            fprintf(engine->history_file, "note_set_ln %u %d \"%s\"\n", (unsigned)pid, prev_row, t);
                                        }
                                        if (new_row >= 0 && new_row < nd->row_count) {
                                            const char* t = nd->rows[new_row].chars ? nd->rows[new_row].chars : "";
                                            fprintf(engine->history_file, "note_set_ln %u %d \"%s\"\n", (unsigned)pid, new_row, t);
                                        }
                                        fflush(engine->history_file);
                                    } else if (sym == SDLK_BACKSPACE) {
                                        // Could have changed current row or merged rows.
                                        if (nd->cursor_row >= 0 && nd->cursor_row < nd->row_count) {
                                            const char* t = nd->rows[nd->cursor_row].chars ? nd->rows[nd->cursor_row].chars : "";
                                            fprintf(engine->history_file, "note_set_ln %u %d \"%s\"\n", (unsigned)pid, nd->cursor_row, t);
                                            fflush(engine->history_file);
                                        }
                                        // If merging happened, the old row index might be gone; that's OK for v1.
                                        (void)before_row;
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
            
            // While an inline text editor is active, don't forward mouse events to Lua tools.
            // This keeps the edit widget stable and avoids accidental drags.
            if (!nk_item_is_any_active(ui->ctx) && (!ui || ui->editing_node_id == 0)) {
                int imx, imy;
                SDL_GetMouseState(&imx, &imy);
                float mx = (float)imx;
                float my = (float)imy;

                // Expose raw screen mouse position to Lua tools (used for jitter-free pan).
                scripting_set_mouse_screen(mx, my);

                // Convert screen -> world using active page viewport.
                // screen = (world - view) * zoom + center
                // world  = (screen - center) / zoom + view
                float world_x = mx;
                float world_y = my;
                if (engine && engine->doc) {
                    uint32_t pid = doc_page_active_id(engine->doc);
                    // find page
                    for (int pi = 0; pi < 32; pi++) {
                        if (engine->doc->pages[pi].in_use && engine->doc->pages[pi].page_id == pid) {
                            float vx = engine->doc->pages[pi].view_x;
                            float vy = engine->doc->pages[pi].view_y;
                            float vz = (engine->doc->pages[pi].view_zoom != 0.0f) ? engine->doc->pages[pi].view_zoom : 1.0f;
                            float cx = 1024.0f * 0.5f;
                            float cy = 768.0f * 0.5f;
                            // NOTE: renderer uses logical size 1024x768.
                            world_x = (mx - cx) / vz + vx;
                            world_y = (my - cy) / vz + vy;
                            break;
                        }
                    }
                }

                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    bool note_consumed_click = false;
                    // NOTE page click-to-focus: if active page is NOTE, and click is in note area, set cursor and start text input.
                    if (e.button.button == SDL_BUTTON_LEFT && engine && engine->doc) {
                        uint32_t pid = doc_page_active_id(engine->doc);
                        for (int pi = 0; pi < 32; pi++) {
                            if (!engine->doc->pages[pi].in_use) continue;
                            if (engine->doc->pages[pi].page_id != pid) continue;
                            if (engine->doc->pages[pi].type == PAGE_TYPE_NOTE && engine->doc->pages[pi].note) {
                                // Note area: screen space, x: 80 to width-80, y: 80 to height
                                const float pad_x = 80.0f;
                                const float top_y = 80.0f;
                                const float line_h = 22.0f;
                                const float char_w = 8.0f;
                                if (mx >= pad_x && mx <= 1024.0f - pad_x && my >= top_y) {
                                    AslanNoteDoc* nd = engine->doc->pages[pi].note;
                                    float scroll_y = nd->scroll_y;
                                    int row = (int)((my - top_y + scroll_y) / line_h);
                                    if (row < 0) row = 0;
                                    if (row >= nd->row_count) row = nd->row_count - 1;
                                    if (nd->row_count == 0) row = 0;
                                    int col = (int)((mx - pad_x - 6.0f) / char_w);
                                    if (col < 0) col = 0;
                                    const char* line = (row < nd->row_count) ? nd->rows[row].chars : "";
                                    int line_len = line ? strlen(line) : 0;
                                    if (col > line_len) col = line_len;
                                    nd->cursor_row = row;
                                    nd->cursor_col = col;
                                    SDL_StartTextInput();
                                    note_consumed_click = true;
                                }
                            }
                            break;
                        }
                    }

                    // Double-click: if we clicked twice quickly in roughly the same spot,
                    // and a TEXT node is already selected, enter inline edit mode.
                    if (e.button.button == SDL_BUTTON_LEFT && ui && ui->selected_node && ui->selected_node->type == NODE_TYPE_TEXT) {
                        uint32_t now = SDL_GetTicks();
                        float dx = world_x - last_click_world_x;
                        float dy = world_y - last_click_world_y;
                        float d2 = dx*dx + dy*dy;
                        if (last_click_ms && (now - last_click_ms) <= dbl_click_ms && d2 <= (dbl_click_dist * dbl_click_dist)) {
                            ui->editing_node_id = ui->selected_node->id;
                            ui->inline_edit_inited = 0;
                            ui->inline_edit_wants_focus = 1;
                            // Reset click state so triple-click doesn't retrigger
                            last_click_ms = 0;
                        } else {
                            last_click_ms = now;
                            last_click_world_x = world_x;
                            last_click_world_y = world_y;
                        }
                    }
                    if (!note_consumed_click) {
                        scripting_call_event(ui->L, "on_mouse_down", world_x, world_y);
                    }
                }
                else if (e.type == SDL_MOUSEBUTTONUP) {
                    scripting_call_event(ui->L, "on_mouse_up", world_x, world_y);
                }
                else if (e.type == SDL_MOUSEMOTION) {
                    scripting_call_event(ui->L, "on_mouse_move", world_x, world_y);
                }
                else if (e.type == SDL_MOUSEWHEEL) {
                    // NOTE pages already consumed wheel for scroll above.
                    if (note_consumed_wheel) {
                        continue;
                    }
                    /*
                     * For now we expose wheel as a dedicated callback.
                     * Convention: (delta, mx, my) where delta is vertical wheel (positive = up).
                     */
                    float delta = (float)e.wheel.y;
                    // Pass wheel delta and mouse X in screen space for cursor-anchored zoom.
                    scripting_call_event(ui->L, "on_mouse_wheel", delta, mx);
                    /* NOTE: scripting_call_event takes 2 floats today; we pass (delta, mx).
                       If you want (delta, mx, my), we can extend the C<->Lua event helper. */
                }
            }
        }
        nk_input_end(ui->ctx);

        // 2. Rendu
        renderer_begin(renderer);
        // 3. Rendu du document
    renderer_draw_document(renderer, engine->doc, ui->ctx);
        
        // 4. Rendu de la sélection (Gizmos)
        if (ui->selected_node) {
            renderer_draw_selection(renderer, engine->doc, ui->selected_node);
        }
        
        // 5. Rendu de l'UI par dessus
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

    // Résoudre le dossier du binaire pour que les scripts se chargent même si on lance depuis ailleurs
    if (argv && argv[0]) {
        char exe_path[PATH_MAX];
        strncpy(exe_path, argv[0], sizeof(exe_path) - 1);
        exe_path[sizeof(exe_path) - 1] = 0;
        char* dir = dirname(exe_path);
        if (dir) {
            // scripts/ est à côté du binaire dans ce projet
            scripting_set_base_dir(dir);
        }
    }

    // 4. Initialisation de l'UI (Nuklear)
    UIHost* ui = ui_init(renderer->window, renderer->sdl_renderer, L);
    engine->ui_host = ui; // Lier l'UI à l'engine

    // 5. Rejouer l'historique (AOF)
    engine_load_history(engine);
    
    // 6. Création d'objets par défaut seulement au tout premier démarrage
    // (si history.log n'existe pas ou est vide). Après un 'clear', on veut rester vide.
    bool history_has_content = false;
    FILE* hf = fopen("history.log", "r");
    if (hf) {
        int c = fgetc(hf);
        if (c != EOF) history_has_content = true;
        fclose(hf);
    }

    if (!history_has_content && engine->doc->root->first_child == NULL) {
        engine_execute_command(engine, "rect Header 10 10 750 60");
        engine_execute_command(engine, "rect Sidebar 10 80 200 678");
        engine_execute_command(engine, "rect Content 220 80 540 678");
    }
    
    // 7. Chargement des scripts
    // NOTE: test_tool.lua est un script de test, ne doit pas tourner par défaut (sinon duplication)
    // scripting_run_file(L, "scripts/test_tool.lua");
    scripting_run_file(L, "scripts/tools.lua");

    // 8. Lancement du mode visuel
    run_visual_mode(engine, renderer, ui);

    // 9. Cleanup
    ui_shutdown(ui);
    renderer_destroy(renderer);
    engine_destroy(engine);
    platform_shutdown();
    
    return 0;
}
