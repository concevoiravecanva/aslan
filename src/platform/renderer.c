#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// ASLAN RENDERER v3 - SDL Window & Basic Drawing
// =============================================================================
//
// Note: Block-based content rendering is handled by Nuklear in ui_host.c.
// This file provides SDL window management and optional low-level drawing.
//

// Nuklear (for optional text rendering)
#include "../../include/nuklear.h"

// -----------------------------------------------------------------------------
// Renderer Lifecycle
// -----------------------------------------------------------------------------

Renderer *renderer_create(const char *title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return NULL;
    }

    Renderer *r = (Renderer *)malloc(sizeof(Renderer));
    if (!r) return NULL;

    r->width = width;
    r->height = height;

    r->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );

    if (!r->window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        free(r);
        return NULL;
    }

    r->sdl_renderer = SDL_CreateRenderer(
        r->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!r->sdl_renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(r->window);
        free(r);
        return NULL;
    }

    // Logical size for consistent coordinates on HiDPI
    SDL_RenderSetLogicalSize(r->sdl_renderer, width, height);

    return r;
}

void renderer_destroy(Renderer *r) {
    if (!r) return;
    SDL_DestroyRenderer(r->sdl_renderer);
    SDL_DestroyWindow(r->window);
    free(r);
    SDL_Quit();
}

// -----------------------------------------------------------------------------
// Frame Begin/End
// -----------------------------------------------------------------------------

void renderer_begin(Renderer *r, int dark_mode) {
    if (!r) return;

    // Set background color based on theme
    if (dark_mode) {
        SDL_SetRenderDrawColor(r->sdl_renderer, 30, 30, 35, 255);
    } else {
        SDL_SetRenderDrawColor(r->sdl_renderer, 252, 252, 253, 255);
    }
    SDL_RenderClear(r->sdl_renderer);
}

void renderer_end(Renderer *r) {
    if (!r) return;
    SDL_RenderPresent(r->sdl_renderer);
}

// -----------------------------------------------------------------------------
// Document Drawing (Optional - main UI is in Nuklear)
// -----------------------------------------------------------------------------

static void draw_grid(Renderer *r, int step) {
    if (!r || step < 2) return;

    SDL_SetRenderDrawColor(r->sdl_renderer, 255, 255, 255, 20);

    // Vertical lines
    for (int x = 0; x < r->width; x += step) {
        SDL_RenderDrawLine(r->sdl_renderer, x, 0, x, r->height);
    }

    // Horizontal lines
    for (int y = 0; y < r->height; y += step) {
        SDL_RenderDrawLine(r->sdl_renderer, 0, y, r->width, y);
    }
}

static void draw_node(Renderer *r, const Node *node, float ox, float oy, float zoom) {
    if (!r || !node || !node->visible) return;

    float sx = ox + node->x * zoom;
    float sy = oy + node->y * zoom;
    float sw = node->width * zoom;
    float sh = node->height * zoom;

    SDL_Rect rect = {(int)sx, (int)sy, (int)sw, (int)sh};

    // Extract RGBA from fill_color
    Uint8 fr = (node->fill_color >> 24) & 0xFF;
    Uint8 fg = (node->fill_color >> 16) & 0xFF;
    Uint8 fb = (node->fill_color >> 8) & 0xFF;
    Uint8 fa = node->fill_color & 0xFF;

    SDL_SetRenderDrawColor(r->sdl_renderer, fr, fg, fb, fa);
    SDL_RenderFillRect(r->sdl_renderer, &rect);

    // Stroke
    if (node->stroke_width > 0) {
        Uint8 sr = (node->stroke_color >> 24) & 0xFF;
        Uint8 sg = (node->stroke_color >> 16) & 0xFF;
        Uint8 sb = (node->stroke_color >> 8) & 0xFF;
        Uint8 sa = node->stroke_color & 0xFF;

        SDL_SetRenderDrawColor(r->sdl_renderer, sr, sg, sb, sa);
        SDL_RenderDrawRect(r->sdl_renderer, &rect);
    }
}

static void draw_node_tree(Renderer *r, const Node *node, float ox, float oy, float zoom) {
    if (!node) return;

    // Draw this node (skip ROOT type)
    if (node->type != NODE_TYPE_ROOT) {
        draw_node(r, node, ox, oy, zoom);
    }

    // Draw children
    for (Node *child = node->first_child; child; child = child->next_sibling) {
        draw_node_tree(r, child, ox, oy, zoom);
    }
}

void renderer_draw_document(Renderer *r, const Document *doc, struct nk_context *nk_ctx) {
    if (!r || !doc) return;

    // Get active page
    Page *page = NULL;
    if (doc->active_page_idx >= 0 && doc->active_page_idx < 32) {
        if (doc->pages[doc->active_page_idx].in_use) {
            page = (Page *)&doc->pages[doc->active_page_idx];
        }
    }

    if (!page) return;

    // Optional: Draw grid based on global setting
    const char *grid_enabled = doc_get_global(doc, "grid.enabled");
    if (grid_enabled && (strcmp(grid_enabled, "1") == 0 || strcmp(grid_enabled, "true") == 0)) {
        const char *grid_step_str = doc_get_global(doc, "grid.step");
        int grid_step = grid_step_str ? atoi(grid_step_str) : 20;
        if (grid_step < 5) grid_step = 5;
        if (grid_step > 200) grid_step = 200;
        draw_grid(r, grid_step);
    }

    // Note: Block rendering is handled by Nuklear in ui_host.c
    // This function can be used for additional low-level drawing if needed
}

void renderer_draw_selection(Renderer *r, const Document *doc, Node *selected_node) {
    if (!r || !selected_node) return;

    // Draw selection box around selected node
    float sx = selected_node->x;
    float sy = selected_node->y;
    float sw = selected_node->width;
    float sh = selected_node->height;

    // Selection highlight (blue border)
    SDL_Rect rect = {(int)sx - 2, (int)sy - 2, (int)sw + 4, (int)sh + 4};
    SDL_SetRenderDrawColor(r->sdl_renderer, 80, 140, 200, 255);
    SDL_RenderDrawRect(r->sdl_renderer, &rect);

    // Corner handles
    const int handle_size = 6;
    SDL_Rect handles[4] = {
        {rect.x - handle_size/2, rect.y - handle_size/2, handle_size, handle_size},
        {rect.x + rect.w - handle_size/2, rect.y - handle_size/2, handle_size, handle_size},
        {rect.x - handle_size/2, rect.y + rect.h - handle_size/2, handle_size, handle_size},
        {rect.x + rect.w - handle_size/2, rect.y + rect.h - handle_size/2, handle_size, handle_size}
    };

    SDL_SetRenderDrawColor(r->sdl_renderer, 255, 255, 255, 255);
    for (int i = 0; i < 4; i++) {
        SDL_RenderFillRect(r->sdl_renderer, &handles[i]);
    }
    SDL_SetRenderDrawColor(r->sdl_renderer, 80, 140, 200, 255);
    for (int i = 0; i < 4; i++) {
        SDL_RenderDrawRect(r->sdl_renderer, &handles[i]);
    }
}

// -----------------------------------------------------------------------------
// Canvas/Design Block Rendering (for direct SDL rendering)
// -----------------------------------------------------------------------------

void renderer_draw_canvas_block(Renderer *r, const Block *block, float x, float y, float w, float h) {
    if (!r || !block || block->type != BLOCK_CANVAS) return;

    const CanvasData *cd = &block->data.canvas;

    // Background
    SDL_Rect bounds = {(int)x, (int)y, (int)w, (int)h};
    SDL_SetRenderDrawColor(r->sdl_renderer, 40, 42, 46, 255);
    SDL_RenderFillRect(r->sdl_renderer, &bounds);

    // Border
    SDL_SetRenderDrawColor(r->sdl_renderer, 60, 62, 66, 255);
    SDL_RenderDrawRect(r->sdl_renderer, &bounds);

    // Draw nodes
    if (cd->root) {
        float ox = x - cd->view_x * cd->zoom;
        float oy = y - cd->view_y * cd->zoom;
        draw_node_tree(r, cd->root, ox, oy, cd->zoom);
    }
}

void renderer_draw_design_block(Renderer *r, const Block *block, float x, float y, float w, float h) {
    if (!r || !block || block->type != BLOCK_DESIGN) return;

    const DesignData *dd = &block->data.design;

    // Calculate scale to fit
    float scale_x = w / (float)dd->width;
    float scale_y = h / (float)dd->height;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale > 1.0f) scale = 1.0f;

    float dw = (float)dd->width * scale;
    float dh = (float)dd->height * scale;
    float dx = x + (w - dw) / 2.0f;
    float dy = y + (h - dh) / 2.0f;

    // Background
    SDL_Rect bounds = {(int)dx, (int)dy, (int)dw, (int)dh};
    Uint8 br = (dd->bg_color >> 24) & 0xFF;
    Uint8 bg = (dd->bg_color >> 16) & 0xFF;
    Uint8 bb = (dd->bg_color >> 8) & 0xFF;
    SDL_SetRenderDrawColor(r->sdl_renderer, br, bg, bb, 255);
    SDL_RenderFillRect(r->sdl_renderer, &bounds);

    // Border (accent)
    SDL_SetRenderDrawColor(r->sdl_renderer, 80, 140, 200, 255);
    SDL_RenderDrawRect(r->sdl_renderer, &bounds);

    // Draw nodes
    if (dd->root) {
        draw_node_tree(r, dd->root, dx, dy, scale);
    }
}
