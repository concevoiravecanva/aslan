#ifndef RENDERER_H
#define RENDERER_H

#include "../document/document.h"
#include <SDL2/SDL.h>

// Forward declare nuklear types (we only need pointers here)
struct nk_context;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* sdl_renderer;
    int width;
    int height;
} Renderer;

Renderer* renderer_create(const char* title, int width, int height);
void renderer_destroy(Renderer* r);

void renderer_begin(Renderer* r);
void renderer_draw_document(Renderer* r, const Document* doc, struct nk_context* nk_ctx);
void renderer_draw_selection(Renderer* r, const Document* doc, Node* selected_node);
void renderer_end(Renderer* r);

#endif
