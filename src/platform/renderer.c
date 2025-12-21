#include "renderer.h"
#include <stdio.h>

// Globals API (declared in document.c)
const char* doc_get_global(const Document* doc, const char* key);

Renderer* renderer_create(const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return NULL;
    }

    Renderer* r = (Renderer*)malloc(sizeof(Renderer));
    r->width = width;
    r->height = height;

    r->window = SDL_CreateWindow(title, 
                                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    
    if (!r->window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        free(r);
        return NULL;
    }

    r->sdl_renderer = SDL_CreateRenderer(r->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!r->sdl_renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(r->window);
        free(r);
        return NULL;
    }

    // Force la taille logique pour que les coordonnées souris (points) 
    // correspondent aux coordonnées de dessin (pixels) sur Retina.
    SDL_RenderSetLogicalSize(r->sdl_renderer, width, height);

    return r;
}

void renderer_destroy(Renderer* r) {
    if (!r) return;
    SDL_DestroyRenderer(r->sdl_renderer);
    SDL_DestroyWindow(r->window);
    free(r);
    SDL_Quit();
}

void renderer_begin(Renderer* r) {
    SDL_SetRenderDrawColor(r->sdl_renderer, 40, 44, 52, 255); // Fond gris foncé (style éditeur)
    SDL_RenderClear(r->sdl_renderer);
}

static int parse_bool_global(const Document* doc, const char* key) {
    const char* v = doc ? doc_get_global(doc, key) : NULL;
    if (!v) return 0;
    return (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0);
}

static int parse_int_global(const Document* doc, const char* key, int def_value) {
    const char* v = doc ? doc_get_global(doc, key) : NULL;
    if (!v) return def_value;
    int n = atoi(v);
    return n;
}

static void renderer_draw_grid(Renderer* r, const Document* doc) {
    if (!r || !doc) return;

    if (!parse_bool_global(doc, "grid.enabled")) return;

    // Step: prefer grid.step, fallback to snap.step.
    int step = parse_int_global(doc, "grid.step", 0);
    if (step <= 0) step = parse_int_global(doc, "snap.step", 10);
    if (step < 2) step = 2;
    if (step > 200) step = 200;

    // Light grid over dark background.
    SDL_SetRenderDrawColor(r->sdl_renderer, 255, 255, 255, 24);

    // Vertical lines
    for (int x = 0; x < r->width; x += step) {
        SDL_RenderDrawLine(r->sdl_renderer, x, 0, x, r->height);
    }

    // Horizontal lines
    for (int y = 0; y < r->height; y += step) {
        SDL_RenderDrawLine(r->sdl_renderer, 0, y, r->width, y);
    }

    // Axis emphasis every 5 steps (subtle)
    const int major = step * 5;
    if (major >= 10) {
        SDL_SetRenderDrawColor(r->sdl_renderer, 255, 255, 255, 46);
        for (int x = 0; x < r->width; x += major) {
            SDL_RenderDrawLine(r->sdl_renderer, x, 0, x, r->height);
        }
        for (int y = 0; y < r->height; y += major) {
            SDL_RenderDrawLine(r->sdl_renderer, 0, y, r->width, y);
        }
    }
}

static void draw_node_recursive(Renderer* r, Node* node) {
    if (!node) return;

    if (node->visible == 0) {
        draw_node_recursive(r, node->first_child);
        draw_node_recursive(r, node->next_sibling);
        return;
    }

    if (node->type == NODE_TYPE_RECT) {
        SDL_Rect rect = { (int)node->x, (int)node->y, (int)node->width, (int)node->height };
        
        // Couleur basée sur l'ID pour le prototype
        int color_seed = node->id * 12345;
        Uint8 red = (color_seed & 0xFF0000) >> 16;
        Uint8 green = (color_seed & 0x00FF00) >> 8;
        Uint8 blue = (color_seed & 0x0000FF);

        // Dessin du rectangle plein
        SDL_SetRenderDrawColor(r->sdl_renderer, red, green, blue, 255);
        SDL_RenderFillRect(r->sdl_renderer, &rect);
        
        // Bordure
        SDL_SetRenderDrawColor(r->sdl_renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(r->sdl_renderer, &rect);
    }

    if (node->type == NODE_TYPE_TEXT) {
        SDL_Rect rect = { (int)node->x, (int)node->y, (int)node->width, (int)node->height };

        // Background (dark)
        SDL_SetRenderDrawColor(r->sdl_renderer, 20, 20, 20, 180);
        SDL_RenderFillRect(r->sdl_renderer, &rect);

        // Border (yellow)
        SDL_SetRenderDrawColor(r->sdl_renderer, 240, 200, 0, 255);
        SDL_RenderDrawRect(r->sdl_renderer, &rect);

        // We don't have a text renderer yet; use a small inner marker
        SDL_Rect marker = { rect.x + 6, rect.y + 6, 10, 10 };
        SDL_RenderDrawRect(r->sdl_renderer, &marker);
    }

    draw_node_recursive(r, node->first_child);
    draw_node_recursive(r, node->next_sibling);
}

void renderer_draw_document(Renderer* r, const Document* doc) {
    if (!doc || !doc->root) return;
    renderer_draw_grid(r, doc);
    draw_node_recursive(r, doc->root);
}

void renderer_draw_selection(Renderer* r, Node* selected_node) {
    if (!selected_node) return;

    // On utilise une épaisseur de trait plus visible
    SDL_Rect rect = { (int)selected_node->x, (int)selected_node->y, (int)selected_node->width, (int)selected_node->height };
    
    // 1. Bordure de sélection (Bleu vif)
    SDL_SetRenderDrawColor(r->sdl_renderer, 0, 120, 215, 255);
    SDL_RenderDrawRect(r->sdl_renderer, &rect);
    
    // On dessine une deuxième bordure intérieure pour plus de visibilité
    SDL_Rect inner = { rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2 };
    SDL_RenderDrawRect(r->sdl_renderer, &inner);
    
    // 2. Poignées de redimensionnement (carrés) dans chaque coin, *à l'intérieur* de la forme
    const int handle_size = 18;

    SDL_Rect handle_tl = { (int)selected_node->x,
                           (int)selected_node->y,
                           handle_size, handle_size };
    SDL_Rect handle_tr = { (int)(selected_node->x + selected_node->width - handle_size),
                           (int)selected_node->y,
                           handle_size, handle_size };
    SDL_Rect handle_bl = { (int)selected_node->x,
                           (int)(selected_node->y + selected_node->height - handle_size),
                           handle_size, handle_size };
    SDL_Rect handle_br = { (int)(selected_node->x + selected_node->width - handle_size),
                           (int)(selected_node->y + selected_node->height - handle_size),
                           handle_size, handle_size };

    SDL_SetRenderDrawColor(r->sdl_renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(r->sdl_renderer, &handle_tl);
    SDL_RenderFillRect(r->sdl_renderer, &handle_tr);
    SDL_RenderFillRect(r->sdl_renderer, &handle_bl);
    SDL_RenderFillRect(r->sdl_renderer, &handle_br);

    SDL_SetRenderDrawColor(r->sdl_renderer, 0, 120, 215, 255);
    SDL_RenderDrawRect(r->sdl_renderer, &handle_tl);
    SDL_RenderDrawRect(r->sdl_renderer, &handle_tr);
    SDL_RenderDrawRect(r->sdl_renderer, &handle_bl);
    SDL_RenderDrawRect(r->sdl_renderer, &handle_br);
}

void renderer_end(Renderer* r) {
    SDL_RenderPresent(r->sdl_renderer);
}
