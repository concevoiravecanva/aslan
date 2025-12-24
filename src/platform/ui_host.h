#ifndef UI_HOST_H
#define UI_HOST_H

#include <SDL2/SDL.h>
#include "../core/engine.h"
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// On définit Nuklear ici pour tout le projet
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_MEMSET memset
#define NK_MEMCPY memcpy
#include "../../include/nuklear.h"

typedef struct UIHost {
    struct nk_context* ctx; // Utiliser un pointeur vers le contexte géré par nk_sdl
    struct nk_font_atlas atlas;
    // Default UI font (and future shared font handle)
    struct nk_font* font_inter;
    float font_size_px;
    Node* selected_node;
    lua_State* L; // Ajout du pointeur vers l'état Lua
    // Small UX feedback for the user (last action/command)
    char status[128];

    // App screen state
    // 0 = Home, 1 = Editor
    int screen;

    // Tool mode: 0 = select/move/resize, 1 = create rect
    int tool_mode;

    // Export settings (defaults)
    char export_svg_path[256];

    // Export modal state (Antirez-style: simple in-app dialog)
    int show_export_svg_modal;
    char export_svg_dir[256];
    char export_svg_name[256];

    // Save As .aslan modal state
    int show_save_aslan_modal;
    char save_aslan_dir[256];
    char save_aslan_name[256];

    // Home: Open existing doc (typed path for now)
    char open_aslan_path[512];

    // Home: Recents (MRU)
    char recents_store_path[512];
    int recents_count;
    char recents_paths[16][512];

    // Globals editor state
    char global_key[64];
    char global_value[256];

    // New Page modal state
    int show_new_page_modal;
    int new_page_type_idx; // 0=Space, 1=Page, 2=Table, 3=Note, 4=Clip
    int new_page_preset_idx; // 0=A4, 1=A3, 2=Letter, 3=Custom
    int new_page_orientation; // 0=Portrait, 1=Landscape
    float new_page_width_mm;
    float new_page_height_mm;
    char new_page_name[64];

    // Snap/Grid editor state (persist UI edits across frames)
    int snap_step_edit;
    int grid_step_edit;
    int snap_grid_steps_inited;

    // Text editor state for TEXT nodes
    char text_edit_buf[2048];
    int text_edit_inited;

    // In-situ text editing (Approach #2)
    uint32_t editing_node_id;      // 0 = none
    char inline_edit_buf[2048];
    int inline_edit_inited;
    int inline_edit_wants_focus;
    int inline_edit_dirty;
} UIHost;

UIHost* ui_init(SDL_Window* win, SDL_Renderer* renderer, lua_State* L);
void ui_shutdown(UIHost* ui);

void ui_handle_event(UIHost* ui, SDL_Event* evt);
void ui_render(UIHost* ui, Engine* engine, int win_w, int win_h);

#endif
