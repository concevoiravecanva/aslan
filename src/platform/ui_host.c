#include "ui_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../scripting/lua_host.h"

// Globals API (theme)
const char* doc_get_global(const Document* doc, const char* key);

static void ui_apply_theme_dark(struct nk_context* ctx) {
    if (!ctx) return;
    struct nk_color table[NK_COLOR_COUNT];

    // Loosely inspired by Nuklear demo themes.
    table[NK_COLOR_TEXT] = nk_rgb(230, 230, 230);
    table[NK_COLOR_WINDOW] = nk_rgb(30, 33, 36);
    table[NK_COLOR_HEADER] = nk_rgb(45, 48, 52);
    table[NK_COLOR_BORDER] = nk_rgb(65, 65, 65);
    table[NK_COLOR_BUTTON] = nk_rgb(60, 60, 60);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgb(70, 70, 70);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(50, 50, 50);
    table[NK_COLOR_TOGGLE] = nk_rgb(50, 50, 50);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgb(60, 60, 60);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(230, 230, 230);
    table[NK_COLOR_SELECT] = nk_rgb(50, 55, 60);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgb(65, 70, 78);
    table[NK_COLOR_SLIDER] = nk_rgb(60, 60, 60);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgb(95, 95, 95);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(110, 110, 110);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(85, 85, 85);
    table[NK_COLOR_PROPERTY] = nk_rgb(60, 60, 60);
    table[NK_COLOR_EDIT] = nk_rgb(45, 45, 45);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgb(230, 230, 230);
    table[NK_COLOR_COMBO] = nk_rgb(45, 45, 45);
    table[NK_COLOR_CHART] = nk_rgb(45, 45, 45);
    table[NK_COLOR_CHART_COLOR] = nk_rgb(164, 164, 164);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(255, 0, 0);
    table[NK_COLOR_SCROLLBAR] = nk_rgb(45, 45, 45);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(70, 70, 70);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(85, 85, 85);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(60, 60, 60);
    table[NK_COLOR_TAB_HEADER] = nk_rgb(45, 48, 52);

    nk_style_from_table(ctx, table);
}

static void ui_apply_theme_light(struct nk_context* ctx) {
    if (!ctx) return;
    struct nk_color table[NK_COLOR_COUNT];

    table[NK_COLOR_TEXT] = nk_rgb(20, 20, 20);
    table[NK_COLOR_WINDOW] = nk_rgb(245, 245, 245);
    table[NK_COLOR_HEADER] = nk_rgb(235, 235, 235);
    table[NK_COLOR_BORDER] = nk_rgb(180, 180, 180);
    table[NK_COLOR_BUTTON] = nk_rgb(225, 225, 225);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgb(210, 210, 210);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(200, 200, 200);
    table[NK_COLOR_TOGGLE] = nk_rgb(220, 220, 220);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgb(205, 205, 205);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(35, 35, 35);
    table[NK_COLOR_SELECT] = nk_rgb(205, 205, 205);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgb(190, 190, 190);
    table[NK_COLOR_SLIDER] = nk_rgb(220, 220, 220);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgb(160, 160, 160);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(145, 145, 145);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(130, 130, 130);
    table[NK_COLOR_PROPERTY] = nk_rgb(220, 220, 220);
    table[NK_COLOR_EDIT] = nk_rgb(235, 235, 235);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgb(20, 20, 20);
    table[NK_COLOR_COMBO] = nk_rgb(235, 235, 235);
    table[NK_COLOR_CHART] = nk_rgb(230, 230, 230);
    table[NK_COLOR_CHART_COLOR] = nk_rgb(60, 60, 60);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(255, 0, 0);
    table[NK_COLOR_SCROLLBAR] = nk_rgb(235, 235, 235);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(200, 200, 200);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(185, 185, 185);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(170, 170, 170);
    table[NK_COLOR_TAB_HEADER] = nk_rgb(235, 235, 235);

    nk_style_from_table(ctx, table);
}

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

    // Export defaults
    snprintf(ui->export_svg_path, sizeof(ui->export_svg_path), "preview.svg");
    ui->show_export_svg_modal = 0;
    if (getcwd(ui->export_svg_dir, sizeof(ui->export_svg_dir)) == NULL) {
        snprintf(ui->export_svg_dir, sizeof(ui->export_svg_dir), ".");
    }
    snprintf(ui->export_svg_name, sizeof(ui->export_svg_name), "preview.svg");

    // Globals editor defaults
    ui->global_key[0] = 0;
    ui->global_value[0] = 0;

    // Snap/Grid step editor defaults
    ui->snap_step_edit = 10;
    ui->grid_step_edit = 10;
    ui->snap_grid_steps_inited = 0;
    
    return ui;
}

static void reveal_file_in_finder_best_effort(const char* path) {
    if (!path || !path[0]) return;
#if defined(__APPLE__)
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "open -R '%s' >/dev/null 2>&1", path);
    system(cmd);
#else
    (void)path;
#endif
}

static int mkdir_p(const char* dir) {
    if (!dir || !dir[0]) return 0;
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", dir);

    size_t len = strlen(tmp);
    if (len == 0) return 0;
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
    return 0;
}

static void join_path(char* out, size_t cap, const char* dir, const char* name) {
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!name || !name[0]) return;
    if (!dir || !dir[0]) {
        snprintf(out, cap, "%s", name);
        return;
    }
    size_t dlen = strlen(dir);
    if (dir[dlen - 1] == '/') snprintf(out, cap, "%s%s", dir, name);
    else snprintf(out, cap, "%s/%s", dir, name);
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
    // Theme for Inspector UI only (read from globals)
    const char* theme = (engine && engine->doc) ? doc_get_global(engine->doc, "theme") : NULL;
    if (theme && strcmp(theme, "dark") == 0) ui_apply_theme_dark(ui->ctx);
    else ui_apply_theme_light(ui->ctx);

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

        // Export (always available)
        nk_layout_row_dynamic(ui->ctx, 25, 1);
        nk_label(ui->ctx, "Export", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ui->ctx, 32, 1);
        if (nk_button_label(ui->ctx, "Export SVG...")) {
            // open modal
            ui->show_export_svg_modal = 1;
            // keep defaults synced
            if (getcwd(ui->export_svg_dir, sizeof(ui->export_svg_dir)) == NULL) {
                snprintf(ui->export_svg_dir, sizeof(ui->export_svg_dir), ".");
            }
            snprintf(ui->export_svg_name, sizeof(ui->export_svg_name), "%s", ui->export_svg_path[0] ? ui->export_svg_path : "preview.svg");
        }

        // Theme quick toggle (AOF-driven)
        nk_layout_row_dynamic(ui->ctx, 28, 1);
        if (nk_button_label(ui->ctx, "Toggle Theme (dark/light)")) {
            const char* cur = (engine && engine->doc) ? doc_get_global(engine->doc, "theme") : NULL;
            const char* next = (cur && strcmp(cur, "dark") == 0) ? "light" : "dark";
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "set_global theme \"%s\"", next);
            engine_execute_command(engine, cmd);
            snprintf(ui->status, sizeof(ui->status), "Theme: %s", next);
        }

        // Snap/Grid (AOF-driven globals)
        nk_layout_row_dynamic(ui->ctx, 10, 1);
        nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ui->ctx, 25, 1);
        nk_label(ui->ctx, "Snap & Grid", NK_TEXT_LEFT);

        int snap_enabled = 0;
        int grid_enabled = 0;
    int snap_step_doc = 10;
    int grid_step_doc = 10;
        if (engine && engine->doc) {
            const char* se = doc_get_global(engine->doc, "snap.enabled");
            if (se && (strcmp(se, "1") == 0 || strcasecmp(se, "true") == 0 || strcasecmp(se, "yes") == 0)) snap_enabled = 1;
            const char* ge = doc_get_global(engine->doc, "grid.enabled");
            if (ge && (strcmp(ge, "1") == 0 || strcasecmp(ge, "true") == 0 || strcasecmp(ge, "yes") == 0)) grid_enabled = 1;
            const char* ss = doc_get_global(engine->doc, "snap.step");
            if (ss) {
                int v = atoi(ss);
        if (v >= 1 && v <= 200) snap_step_doc = v;
            }
            const char* gs = doc_get_global(engine->doc, "grid.step");
            if (gs) {
                int v = atoi(gs);
        if (v >= 1 && v <= 200) grid_step_doc = v;
            } else {
        // default: keep in sync with snap.step if grid.step isn't set
        grid_step_doc = snap_step_doc;
            }
        }

    // IMPORTANT: don't overwrite the user's edits every frame.
    // We sync from document globals only once (or after a clear/new UI init).
    if (!ui->snap_grid_steps_inited) {
        ui->snap_step_edit = snap_step_doc;
        ui->grid_step_edit = grid_step_doc;
        ui->snap_grid_steps_inited = 1;
    }

        nk_layout_row_dynamic(ui->ctx, 24, 2);
        if (nk_checkbox_label(ui->ctx, "Snap", &snap_enabled)) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "set_global snap.enabled \"%d\"", snap_enabled ? 1 : 0);
            engine_execute_command(engine, cmd);
        }
        if (nk_checkbox_label(ui->ctx, "Grid", &grid_enabled)) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "set_global grid.enabled \"%d\"", grid_enabled ? 1 : 0);
            engine_execute_command(engine, cmd);
        }

        // Steps (separate)
        nk_layout_row_dynamic(ui->ctx, 24, 1);
        nk_property_int(ui->ctx, "Snap step", 1, &ui->snap_step_edit, 200, 1, 1);
        nk_layout_row_dynamic(ui->ctx, 28, 1);
        if (nk_button_label(ui->ctx, "Apply Snap step")) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "set_global snap.step \"%d\"", ui->snap_step_edit);
            engine_execute_command(engine, cmd);
            snprintf(ui->status, sizeof(ui->status), "Snap step: %d", ui->snap_step_edit);
        }

        nk_layout_row_dynamic(ui->ctx, 24, 1);
        nk_property_int(ui->ctx, "Grid step", 1, &ui->grid_step_edit, 200, 1, 1);
        nk_layout_row_dynamic(ui->ctx, 28, 1);
        if (nk_button_label(ui->ctx, "Apply Grid step")) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "set_global grid.step \"%d\"", ui->grid_step_edit);
            engine_execute_command(engine, cmd);
            snprintf(ui->status, sizeof(ui->status), "Grid step: %d", ui->grid_step_edit);
        }

        // Optional: let user resync from document if they want
        nk_layout_row_dynamic(ui->ctx, 24, 1);
        if (nk_button_label(ui->ctx, "Reset steps from document")) {
            ui->snap_step_edit = snap_step_doc;
            ui->grid_step_edit = grid_step_doc;
            snprintf(ui->status, sizeof(ui->status), "Steps reset");
        }

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

        // Globals panel
        nk_layout_row_dynamic(ui->ctx, 10, 1); // Spacer
        nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(ui->ctx, 25, 1);
        nk_label(ui->ctx, "Globals", NK_TEXT_LEFT);

        // List existing globals
        nk_layout_row_dynamic(ui->ctx, 120, 1);
        if (nk_group_begin(ui->ctx, "globals_list", NK_WINDOW_BORDER)) {
            if (engine && engine->doc) {
                for (int i = 0; i < 64; i++) {
                    if (!engine->doc->globals[i].in_use) continue;
                    char row[420];
                    snprintf(row, sizeof(row), "%s = %s", engine->doc->globals[i].key, engine->doc->globals[i].value);
                    nk_layout_row_dynamic(ui->ctx, 22, 1);
                    if (nk_button_label(ui->ctx, row)) {
                        snprintf(ui->global_key, sizeof(ui->global_key), "%s", engine->doc->globals[i].key);
                        snprintf(ui->global_value, sizeof(ui->global_value), "%s", engine->doc->globals[i].value);
                        snprintf(ui->status, sizeof(ui->status), "Edit global: %s", ui->global_key);
                    }
                }
            }
            nk_group_end(ui->ctx);
        }

        // Editor
        nk_layout_row_dynamic(ui->ctx, 20, 1);
        nk_label(ui->ctx, "Key:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ui->ctx, 28, 1);
        nk_edit_string_zero_terminated(ui->ctx, NK_EDIT_FIELD, ui->global_key,
            (int)sizeof(ui->global_key) - 1, nk_filter_default);

        nk_layout_row_dynamic(ui->ctx, 20, 1);
        nk_label(ui->ctx, "Value:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ui->ctx, 28, 1);
        nk_edit_string_zero_terminated(ui->ctx, NK_EDIT_FIELD, ui->global_value,
            (int)sizeof(ui->global_value) - 1, nk_filter_default);

        nk_layout_row_dynamic(ui->ctx, 32, 2);
        if (nk_button_label(ui->ctx, "Set")) {
            if (ui->global_key[0]) {
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "set_global %s \"%s\"", ui->global_key, ui->global_value);
                engine_execute_command(engine, cmd);
                snprintf(ui->status, sizeof(ui->status), "Set global: %s", ui->global_key);
            }
        }
        if (nk_button_label(ui->ctx, "Unset")) {
            if (ui->global_key[0]) {
                char cmd[256];
                snprintf(cmd, sizeof(cmd), "unset_global %s", ui->global_key);
                engine_execute_command(engine, cmd);
                snprintf(ui->status, sizeof(ui->status), "Unset global: %s", ui->global_key);
                ui->global_key[0] = 0;
                ui->global_value[0] = 0;
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

        } else {
            nk_layout_row_dynamic(ui->ctx, 30, 1);
            nk_label(ui->ctx, "No selection", NK_TEXT_CENTERED);
        }
    }
    nk_end(ui->ctx);

    // Modal export window (robust: plain nk_begin, avoids nk_popup_begin asserts)
    if (ui->show_export_svg_modal) {
        struct nk_rect bounds = nk_rect(win_w * 0.5f - 170.0f, win_h * 0.5f - 130.0f, 340.0f, 260.0f);
        if (nk_begin(ui->ctx, "Export SVG", bounds,
            NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_MOVABLE|NK_WINDOW_NO_SCROLLBAR)) {

            nk_layout_row_dynamic(ui->ctx, 20, 1);
            nk_label(ui->ctx, "Directory:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ui->ctx, 28, 1);
            nk_edit_string_zero_terminated(ui->ctx, NK_EDIT_FIELD, ui->export_svg_dir,
                (int)sizeof(ui->export_svg_dir) - 1, nk_filter_default);

            nk_layout_row_dynamic(ui->ctx, 20, 1);
            nk_label(ui->ctx, "Filename:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ui->ctx, 28, 1);
            nk_edit_string_zero_terminated(ui->ctx, NK_EDIT_FIELD, ui->export_svg_name,
                (int)sizeof(ui->export_svg_name) - 1, nk_filter_default);

            nk_layout_row_dynamic(ui->ctx, 18, 1);
            nk_label(ui->ctx, "Tip: name ends with .svg", NK_TEXT_LEFT);

            nk_layout_row_dynamic(ui->ctx, 34, 2);
            if (nk_button_label(ui->ctx, "Cancel")) {
                ui->show_export_svg_modal = 0;
            }
            if (nk_button_label(ui->ctx, "Save")) {
                char name[256];
                snprintf(name, sizeof(name), "%s", ui->export_svg_name[0] ? ui->export_svg_name : "preview.svg");
                size_t nlen = strlen(name);
                if (nlen < 4 || strcmp(name + (nlen - 4), ".svg") != 0) {
                    strncat(name, ".svg", sizeof(name) - strlen(name) - 1);
                }

                mkdir_p(ui->export_svg_dir);

                char out_path[512];
                join_path(out_path, sizeof(out_path), ui->export_svg_dir, name);

                doc_export_to_svg(engine->doc, out_path);
                snprintf(ui->export_svg_path, sizeof(ui->export_svg_path), "%s", name);
                snprintf(ui->status, sizeof(ui->status), "Saved: %s", out_path);
                reveal_file_in_finder_best_effort(out_path);

                ui->show_export_svg_modal = 0;
            }
        }
        nk_end(ui->ctx);
    }

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}
