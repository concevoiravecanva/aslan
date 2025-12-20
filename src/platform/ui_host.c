#include "ui_host.h"
#include <stdio.h>
#include "../scripting/lua_host.h"

// Implémentation spécifique SDL2 pour Nuklear
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#define NK_SDL_RENDERER_SDL_H <SDL2/SDL.h>
#include "../../include/nuklear.h"
#include "nuklear_sdl_renderer.h"

UIHost* ui_init(SDL_Window* win, SDL_Renderer* renderer, lua_State* L) {
    UIHost* ui = (UIHost*)calloc(1, sizeof(UIHost));
    
    ui->L = L;
    // Initialisation de Nuklear avec le renderer SDL2
    ui->ctx = nk_sdl_init(win, renderer);
    
    // Chargement de la police par défaut
    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    nk_sdl_font_stash_end();

    snprintf(ui->status, sizeof(ui->status), "Ready");
    ui->tool_mode = 0;
    
    return ui;
}

void ui_shutdown(UIHost* ui) {
    nk_sdl_shutdown();
    free(ui);
}

void ui_handle_event(UIHost* ui, SDL_Event* evt) {
    (void)ui;
    nk_sdl_handle_event(evt);
}

void ui_render(UIHost* ui, Engine* engine, int win_w, int win_h) {
    // On synchronise la sélection de l'UI avec celle de l'engine si nécessaire
    // (L'engine est la source de vérité pour les commandes 'select')
    
    // Panneau Inspecteur à droite
    if (nk_begin(ui->ctx, "Inspector", nk_rect(win_w - 250, 0, 250, win_h),
        NK_WINDOW_BORDER|NK_WINDOW_TITLE)) {

        // Section Historique (Undo/Redo/AOF)
        nk_layout_row_dynamic(ui->ctx, 25, 1);
        nk_label(ui->ctx, "History", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ui->ctx, 30, 3);
        if (nk_button_label(ui->ctx, "Undo")) {
            engine_execute_command(engine, "undo");
            snprintf(ui->status, sizeof(ui->status), "Undo");
        }
        if (nk_button_label(ui->ctx, "Redo")) {
            engine_execute_command(engine, "redo");
            snprintf(ui->status, sizeof(ui->status), "Redo");
        }
        if (nk_button_label(ui->ctx, "Compact")) {
            engine_execute_command(engine, "rewrite_aof");
            snprintf(ui->status, sizeof(ui->status), "Compacted AOF");
        }

        nk_layout_row_dynamic(ui->ctx, 10, 1); // Spacer
        nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

    // Status line (last action)
    nk_layout_row_dynamic(ui->ctx, 22, 1);
    nk_label(ui->ctx, ui->status[0] ? ui->status : "", NK_TEXT_LEFT);

        // Tool mode toggle
        nk_layout_row_dynamic(ui->ctx, 28, 1);
        int create_mode = (ui->tool_mode == 1);
        if (nk_checkbox_label(ui->ctx, "Create Rect (draw)", &create_mode)) {
            ui->tool_mode = create_mode ? 1 : 0;
            snprintf(ui->status, sizeof(ui->status), create_mode ? "Tool: Create Rect" : "Tool: Select");
            // also inform Lua so script can react immediately
            lua_getglobal(ui->L, "aslan");
            if (lua_istable(ui->L, -1)) {
                lua_getfield(ui->L, -1, "set_tool_mode");
                if (lua_isfunction(ui->L, -1)) {
                    lua_pushinteger(ui->L, ui->tool_mode);
                    if (lua_pcall(ui->L, 1, 0, 0) != LUA_OK) {
                        lua_pop(ui->L, 1);
                    }
                } else {
                    lua_pop(ui->L, 1);
                }
                lua_pop(ui->L, 1);
            } else {
                lua_pop(ui->L, 1);
            }
        }
        
        // Section Outils
        nk_layout_row_dynamic(ui->ctx, 25, 1);
        nk_label(ui->ctx, "Tools", NK_TEXT_LEFT);

        // 2x2 grid of buttons
        nk_layout_row_dynamic(ui->ctx, 30, 2);
        if (nk_button_label(ui->ctx, "New Rect")) {
            engine_execute_command(engine, "new_rect");
            snprintf(ui->status, sizeof(ui->status), "New Rect");
        }
        if (nk_button_label(ui->ctx, "New Text")) {
            engine_execute_command(engine, "new_text");
            snprintf(ui->status, sizeof(ui->status), "New Text");
        }
        if (nk_button_label(ui->ctx, "Random Rect")) {
            scripting_run_file(ui->L, "scripts/create_random.lua");
            snprintf(ui->status, sizeof(ui->status), "Random Rect");
        }
        if (nk_button_label(ui->ctx, "Clear All")) {
            engine_execute_command(engine, "clear");
            snprintf(ui->status, sizeof(ui->status), "Cleared");
        }

        // Layers panel (z-order + visibility)
        nk_layout_row_dynamic(ui->ctx, 10, 1); // Spacer
        nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ui->ctx, 25, 1);
        nk_label(ui->ctx, "Layers", NK_TEXT_LEFT);

        // Layers list (top-most first)
        {
            struct nk_style_button btn_style = ui->ctx->style.button;
            Node* selected = ui->selected_node;

            // List height: keep it compact
            nk_layout_row_dynamic(ui->ctx, 160, 1);
            if (nk_group_begin(ui->ctx, "layers_list", NK_WINDOW_BORDER)) {
                Node* nodes[512];
                int count = 0;
                Node* cur = engine->doc && engine->doc->root ? engine->doc->root->first_child : NULL;
                while (cur && count < (int)(sizeof(nodes)/sizeof(nodes[0]))) {
                    nodes[count++] = cur;
                    cur = cur->next_sibling;
                }

                // Draw from front->back (reverse linked list)
                for (int i = count - 1; i >= 0; i--) {
                    Node* n = nodes[i];
                    if (!n) continue;

                    // highlight selected
                    if (selected && n->id == selected->id) {
                        ui->ctx->style.button.normal = nk_style_item_color(nk_rgb(60, 100, 160));
                        ui->ctx->style.button.hover = nk_style_item_color(nk_rgb(70, 110, 180));
                        ui->ctx->style.button.active = nk_style_item_color(nk_rgb(50, 90, 150));
                    } else {
                        ui->ctx->style.button = btn_style;
                    }

                    nk_layout_row_dynamic(ui->ctx, 24, 1);
                    char label[96];
                    snprintf(label, sizeof(label), "%s  (#%u)%s", n->name, (unsigned)n->id, n->visible ? "" : "  [hidden]");
                    if (nk_button_label(ui->ctx, label)) {
                        char cmd[64];
                        snprintf(cmd, sizeof(cmd), "select %u", (unsigned)n->id);
                        engine_execute_command(engine, cmd);
                        ui->selected_node = engine->ui_host ? engine->ui_host->selected_node : ui->selected_node;
                        snprintf(ui->status, sizeof(ui->status), "Selected #%u", (unsigned)n->id);
                    }
                }

                ui->ctx->style.button = btn_style;
                nk_group_end(ui->ctx);
            }
        }

        // Layer actions (require selection)
        if (ui->selected_node) {
            Node* n = ui->selected_node;

            nk_layout_row_dynamic(ui->ctx, 26, 1);
            int vis = n->visible ? 1 : 0;
            if (nk_checkbox_label(ui->ctx, "Visible", &vis)) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "set_visible %u %d", (unsigned)n->id, vis ? 1 : 0);
                engine_execute_command(engine, cmd);
                snprintf(ui->status, sizeof(ui->status), vis ? "Visible" : "Hidden");
            }

            nk_layout_row_dynamic(ui->ctx, 30, 2);
            if (nk_button_label(ui->ctx, "Up")) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "bring_forward %u", (unsigned)n->id);
                engine_execute_command(engine, cmd);
                snprintf(ui->status, sizeof(ui->status), "Layer Up");
            }
            if (nk_button_label(ui->ctx, "Down")) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "send_backward %u", (unsigned)n->id);
                engine_execute_command(engine, cmd);
                snprintf(ui->status, sizeof(ui->status), "Layer Down");
            }

            nk_layout_row_dynamic(ui->ctx, 30, 2);
            if (nk_button_label(ui->ctx, "To Front")) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "bring_to_front %u", (unsigned)n->id);
                engine_execute_command(engine, cmd);
                snprintf(ui->status, sizeof(ui->status), "To Front");
            }
            if (nk_button_label(ui->ctx, "To Back")) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "send_to_back %u", (unsigned)n->id);
                engine_execute_command(engine, cmd);
                snprintf(ui->status, sizeof(ui->status), "To Back");
            }
        }

        nk_layout_row_dynamic(ui->ctx, 10, 1); // Spacer
        nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

        if (ui->selected_node) {
            Node* n = ui->selected_node;
            nk_layout_row_dynamic(ui->ctx, 25, 1);
            nk_label(ui->ctx, "Node Properties", NK_TEXT_LEFT);
            
            // Name
            nk_layout_row_dynamic(ui->ctx, 30, 2);
            nk_label(ui->ctx, "Name:", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(ui->ctx, NK_EDIT_FIELD, n->name, 63, nk_filter_default);

            // Position & Size
            nk_layout_row_dynamic(ui->ctx, 25, 1);
            nk_property_float(ui->ctx, "#X:", -1000, &n->x, 2000, 1.0f, 1.0f);
            nk_property_float(ui->ctx, "#Y:", -1000, &n->y, 2000, 1.0f, 1.0f);
            nk_property_float(ui->ctx, "#W:", 0, &n->width, 2000, 1.0f, 1.0f);
            nk_property_float(ui->ctx, "#H:", 0, &n->height, 2000, 1.0f, 1.0f);

            nk_layout_row_dynamic(ui->ctx, 30, 1);
            if (nk_button_label(ui->ctx, "Export SVG")) {
                doc_export_to_svg(engine->doc, "preview.svg");
            }
        } else {
            nk_layout_row_dynamic(ui->ctx, 30, 1);
            nk_label(ui->ctx, "No selection", NK_TEXT_CENTERED);
        }
    }
    nk_end(ui->ctx);

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}
