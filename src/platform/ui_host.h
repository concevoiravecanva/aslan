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
    Node* selected_node;
    lua_State* L; // Ajout du pointeur vers l'état Lua
} UIHost;

UIHost* ui_init(SDL_Window* win, SDL_Renderer* renderer, lua_State* L);
void ui_shutdown(UIHost* ui);

void ui_handle_event(UIHost* ui, SDL_Event* evt);
void ui_render(UIHost* ui, Engine* engine, int win_w, int win_h);

#endif
