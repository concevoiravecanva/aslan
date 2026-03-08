#ifndef CLAY_HOST_H
#define CLAY_HOST_H

// =============================================================================
// ASLAN Clay Host - Step 1: Panel Layout & Background Rendering
// =============================================================================
// Clay manages layout computation and background rendering (SDL2 primitives).
// Nuklear manages all widget content inside the regions Clay defines.
// =============================================================================

#include <SDL.h>
#include <stdbool.h>

// Pixel regions returned after Clay computes the split-view layout.
// Nuklear uses these to position its own standalone windows.
typedef struct {
    float left_x, left_w;
    float spl_x,  spl_w;
    float right_x, right_w;
    float panel_h;
} AslanSplitRects;

// ClayHost is opaque — internals live only in clay_host.c
typedef struct ClayHost ClayHost;

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
ClayHost *clay_host_init(SDL_Renderer *renderer, int win_w, int win_h);
void      clay_host_destroy(ClayHost *ch);

// Call when the window is resized
void clay_host_update_size(ClayHost *ch, int win_w, int win_h);

// -----------------------------------------------------------------------------
// Per-frame call: computes layout via Clay, renders backgrounds via SDL2,
// returns the computed panel rects for Nuklear to use.
// -----------------------------------------------------------------------------
AslanSplitRects clay_host_render_split(
    ClayHost *ch,
    bool dark_mode,
    float ratio,
    bool spl_hover,
    bool spl_drag,
    int win_w,
    int win_h
);

#endif // CLAY_HOST_H
