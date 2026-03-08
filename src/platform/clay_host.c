// =============================================================================
// ASLAN Clay Host - SDL2 Backend + Layout Engine
// =============================================================================

// Clay single-header implementation (must be in exactly one .c file)
#define CLAY_IMPLEMENTATION
#include "../vendor/clay.h"

#include "clay_host.h"

#include <SDL.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Internal types ───────────────────────────────────────────────────────────
#define ASLAN_FONT_INTER   0
#define ASLAN_FONT_BOLD    1
#define ASLAN_FONT_ICONS   2
#define ASLAN_FONT_COUNT   3

typedef struct {
    uint32_t  fontId;
    TTF_Font *font;
} AslanFont;

struct ClayHost {
    void          *memory;
    SDL_Renderer  *renderer;
    AslanFont      fonts[ASLAN_FONT_COUNT];
    AslanSplitRects last_rects;
};

// =============================================================================
// Minimal Clay → SDL2 Renderer  (rectangles + borders + scissor)
// Adapted from the official Clay SDL2 renderer (MIT / zlib).
// Text rendering via Clay is reserved for Step 2+; Nuklear handles text now.
// =============================================================================

static int g_circle_segments = 16;

// Rounded-rect fill using SDL_RenderGeometry triangle fan
static void sdl_fill_rounded_rect(SDL_Renderer *r, SDL_FRect rect,
                                  float radius, SDL_Color c) {
    if (radius <= 0.0f) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_RenderFillRectF(r, &rect);
        return;
    }

    const float max_r = SDL_min(rect.w, rect.h) / 2.0f;
    const float cr    = SDL_min(radius, max_r);
    const int   segs  = SDL_max(g_circle_segments, (int)(cr * 0.5f));

    SDL_Vertex verts[512];
    int        idx[512];
    int        vc = 0, ic = 0;

    // Centre rectangle (4 verts)
    verts[vc++] = (SDL_Vertex){{rect.x + cr,         rect.y + cr        }, c, {0,0}};
    verts[vc++] = (SDL_Vertex){{rect.x + rect.w - cr, rect.y + cr        }, c, {1,0}};
    verts[vc++] = (SDL_Vertex){{rect.x + rect.w - cr, rect.y + rect.h - cr}, c, {1,1}};
    verts[vc++] = (SDL_Vertex){{rect.x + cr,          rect.y + rect.h - cr}, c, {0,1}};

    idx[ic++]=0; idx[ic++]=1; idx[ic++]=3;
    idx[ic++]=1; idx[ic++]=2; idx[ic++]=3;

    // Corner fans
    const float step = (float)(M_PI / 2.0) / (float)segs;
    for (int i = 0; i < segs; i++) {
        float a1 = (float)i * step;
        float a2 = ((float)i + 1.0f) * step;
        for (int j = 0; j < 4; j++) {
            float cx, cy, sx, sy;
            switch (j) {
                case 0: cx = rect.x + cr;          cy = rect.y + cr;           sx=-1; sy=-1; break;
                case 1: cx = rect.x + rect.w - cr; cy = rect.y + cr;           sx= 1; sy=-1; break;
                case 2: cx = rect.x + rect.w - cr; cy = rect.y + rect.h - cr;  sx= 1; sy= 1; break;
                default:cx = rect.x + cr;          cy = rect.y + rect.h - cr;  sx=-1; sy= 1; break;
            }
            verts[vc++] = (SDL_Vertex){{cx + SDL_cosf(a1)*cr*sx, cy + SDL_sinf(a1)*cr*sy}, c, {0,0}};
            verts[vc++] = (SDL_Vertex){{cx + SDL_cosf(a2)*cr*sx, cy + SDL_sinf(a2)*cr*sy}, c, {0,0}};
            idx[ic++] = j;
            idx[ic++] = vc - 2;
            idx[ic++] = vc - 1;
        }
    }

    // Edge rectangles
    // Top
    verts[vc++]=(SDL_Vertex){{rect.x+cr,         rect.y         },c,{0,0}};
    verts[vc++]=(SDL_Vertex){{rect.x+rect.w-cr,  rect.y         },c,{1,0}};
    idx[ic++]=0;idx[ic++]=vc-2;idx[ic++]=vc-1;
    idx[ic++]=1;idx[ic++]=0;  idx[ic++]=vc-1;
    // Right
    verts[vc++]=(SDL_Vertex){{rect.x+rect.w,     rect.y+cr      },c,{1,0}};
    verts[vc++]=(SDL_Vertex){{rect.x+rect.w,     rect.y+rect.h-cr},c,{1,1}};
    idx[ic++]=1;idx[ic++]=vc-2;idx[ic++]=vc-1;
    idx[ic++]=2;idx[ic++]=1;  idx[ic++]=vc-1;
    // Bottom
    verts[vc++]=(SDL_Vertex){{rect.x+rect.w-cr,  rect.y+rect.h  },c,{1,1}};
    verts[vc++]=(SDL_Vertex){{rect.x+cr,          rect.y+rect.h  },c,{0,1}};
    idx[ic++]=2;idx[ic++]=vc-2;idx[ic++]=vc-1;
    idx[ic++]=3;idx[ic++]=2;  idx[ic++]=vc-1;
    // Left
    verts[vc++]=(SDL_Vertex){{rect.x,             rect.y+rect.h-cr},c,{0,1}};
    verts[vc++]=(SDL_Vertex){{rect.x,             rect.y+cr       },c,{0,0}};
    idx[ic++]=3;idx[ic++]=vc-2;idx[ic++]=vc-1;
    idx[ic++]=0;idx[ic++]=3;  idx[ic++]=vc-1;

    SDL_RenderGeometry(r, NULL, verts, vc, idx, ic);
}

// Process a Clay render command array (rectangles, borders, scissor only)
static void clay_sdl2_render(SDL_Renderer *r,
                             Clay_RenderCommandArray cmds) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < (int)cmds.length; i++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
        Clay_BoundingBox   bb  = cmd->boundingBox;

        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_Color c  = cmd->renderData.rectangle.backgroundColor;
            float      cr = cmd->renderData.rectangle.cornerRadius.topLeft;
            SDL_FRect  fr = { bb.x, bb.y, bb.width, bb.height };
            SDL_Color  sc = { (Uint8)c.r, (Uint8)c.g, (Uint8)c.b, (Uint8)c.a };
            if (cr > 0.0f) {
                sdl_fill_rounded_rect(r, fr, cr, sc);
            } else {
                SDL_SetRenderDrawColor(r, sc.r, sc.g, sc.b, sc.a);
                SDL_RenderFillRectF(r, &fr);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *bd = &cmd->renderData.border;
            SDL_Color sc = {
                (Uint8)bd->color.r, (Uint8)bd->color.g,
                (Uint8)bd->color.b, (Uint8)bd->color.a
            };
            SDL_SetRenderDrawColor(r, sc.r, sc.g, sc.b, sc.a);
            float maxR = SDL_min(bb.width, bb.height) / 2.0f;
            if (bd->width.top > 0) {
                float cL = SDL_min(bd->cornerRadius.topLeft, maxR);
                float cR = SDL_min(bd->cornerRadius.topRight, maxR);
                SDL_FRect fr = { bb.x + cL, bb.y,
                                 bb.width - cL - cR, (float)bd->width.top };
                SDL_RenderFillRectF(r, &fr);
            }
            if (bd->width.bottom > 0) {
                float cL = SDL_min(bd->cornerRadius.bottomLeft, maxR);
                float cR = SDL_min(bd->cornerRadius.bottomRight, maxR);
                SDL_FRect fr = { bb.x + cL,
                                 bb.y + bb.height - (float)bd->width.bottom,
                                 bb.width - cL - cR, (float)bd->width.bottom };
                SDL_RenderFillRectF(r, &fr);
            }
            if (bd->width.left > 0) {
                float cT = SDL_min(bd->cornerRadius.topLeft, maxR);
                float cB = SDL_min(bd->cornerRadius.bottomLeft, maxR);
                SDL_FRect fr = { bb.x, bb.y + cT,
                                 (float)bd->width.left, bb.height - cT - cB };
                SDL_RenderFillRectF(r, &fr);
            }
            if (bd->width.right > 0) {
                float cT = SDL_min(bd->cornerRadius.topRight, maxR);
                float cB = SDL_min(bd->cornerRadius.bottomRight, maxR);
                SDL_FRect fr = { bb.x + bb.width - (float)bd->width.right,
                                 bb.y + cT,
                                 (float)bd->width.right, bb.height - cT - cB };
                SDL_RenderFillRectF(r, &fr);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            SDL_Rect sr = { (int)bb.x, (int)bb.y, (int)bb.width, (int)bb.height };
            SDL_RenderSetClipRect(r, &sr);
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            SDL_RenderSetClipRect(r, NULL);
            break;
        default:
            // TEXT and IMAGE handled by Nuklear in Step 1
            break;
        }
    }
}

// =============================================================================
// Text measurement (required by Clay even if we don't use Clay text yet)
// =============================================================================

static Clay_Dimensions measure_text(Clay_StringSlice text,
                                    Clay_TextElementConfig *cfg,
                                    void *userData) {
    AslanFont *fonts = (AslanFont *)userData;
    if (!fonts[cfg->fontId].font)
        return (Clay_Dimensions){0.0f, 0.0f};

    TTF_Font *f = fonts[cfg->fontId].font;
    TTF_SetFontSize(f, (int)cfg->fontSize);

    char buf[512] = {0};
    int  len = text.length < 511 ? (int)text.length : 511;
    memcpy(buf, text.chars, len);

    int w = 0, h = 0;
    TTF_SizeUTF8(f, buf, &w, &h);
    return (Clay_Dimensions){.width = (float)w, .height = (float)h};
}

static void clay_error(Clay_ErrorData err) {
    fprintf(stderr, "[Clay] %.*s\n", (int)err.errorText.length,
            err.errorText.chars);
}

// =============================================================================
// Lifecycle
// =============================================================================

ClayHost *clay_host_init(SDL_Renderer *renderer, int win_w, int win_h) {
    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL_ttf init failed: %s\n", TTF_GetError());
        return NULL;
    }

    ClayHost *ch = (ClayHost *)calloc(1, sizeof(ClayHost));
    if (!ch) return NULL;

    ch->renderer = renderer;

    // Load fonts for Clay text (used in Step 2+ toolbar / labels)
    ch->fonts[ASLAN_FONT_INTER].fontId = ASLAN_FONT_INTER;
    ch->fonts[ASLAN_FONT_INTER].font =
        TTF_OpenFont("assets/fonts/Inter-Regular.ttf", 14);

    ch->fonts[ASLAN_FONT_BOLD].fontId = ASLAN_FONT_BOLD;
    ch->fonts[ASLAN_FONT_BOLD].font =
        TTF_OpenFont("assets/fonts/Inter_28pt-Bold.ttf", 14);

    ch->fonts[ASLAN_FONT_ICONS].fontId = ASLAN_FONT_ICONS;
    ch->fonts[ASLAN_FONT_ICONS].font =
        TTF_OpenFont("assets/remixicon.ttf", 18);

    // Initialize Clay arena
    uint32_t mem_size = Clay_MinMemorySize();
    ch->memory = malloc(mem_size);
    if (!ch->memory) {
        free(ch);
        TTF_Quit();
        return NULL;
    }

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(
        mem_size, ch->memory);
    Clay_Initialize(arena,
                    (Clay_Dimensions){(float)win_w, (float)win_h},
                    (Clay_ErrorHandler){clay_error, NULL});
    Clay_SetMeasureTextFunction(measure_text, ch->fonts);

    return ch;
}

void clay_host_destroy(ClayHost *ch) {
    if (!ch) return;
    for (int i = 0; i < ASLAN_FONT_COUNT; i++) {
        if (ch->fonts[i].font)
            TTF_CloseFont(ch->fonts[i].font);
    }
    TTF_Quit();
    free(ch->memory);
    free(ch);
}

void clay_host_update_size(ClayHost *ch, int win_w, int win_h) {
    if (!ch) return;
    Clay_SetLayoutDimensions((Clay_Dimensions){(float)win_w, (float)win_h});
}

// =============================================================================
// Per-frame split-view background rendering
// =============================================================================

AslanSplitRects clay_host_render_split(ClayHost *ch,
                                       bool dark_mode,
                                       float ratio,
                                       bool spl_hover,
                                       bool spl_drag,
                                       int win_w, int win_h) {
    if (!ch) return (AslanSplitRects){0};

    // ── Color palette ───────────────────────────────────────────────────────
    Clay_Color left_bg, right_bg, spl_bg;

    if (dark_mode) {
        left_bg  = (Clay_Color){28,  30,  36,  255};
        right_bg = (Clay_Color){24,  26,  32,  255};
        spl_bg   = (spl_hover || spl_drag)
                   ? (Clay_Color){55, 57, 68, 255}
                   : (Clay_Color){42, 44, 52, 255};
    } else {
        left_bg  = (Clay_Color){252, 252, 253, 255};
        right_bg = (Clay_Color){255, 255, 255, 255};
        spl_bg   = (spl_hover || spl_drag)
                   ? (Clay_Color){218, 218, 225, 255}
                   : (Clay_Color){235, 235, 238, 255};
    }

    // ── Compute pixel sizes ─────────────────────────────────────────────────
    if (ratio < 0.03f) ratio = 0.03f;
    if (ratio > 0.97f) ratio = 0.97f;

    const float spl_w   = 6.0f;
    float       left_w  = (float)win_w * ratio;
    float       right_w = (float)win_w - left_w - spl_w;
    if (left_w  < 30.0f) left_w  = 30.0f;
    if (right_w < 30.0f) right_w = 30.0f;

    // ── Clay layout ─────────────────────────────────────────────────────────
    Clay_SetLayoutDimensions((Clay_Dimensions){(float)win_w, (float)win_h});
    Clay_BeginLayout();

    CLAY(CLAY_ID("AslanRoot"), (Clay_ElementDeclaration){
        .layout = {
            .sizing = {
                .width  = CLAY_SIZING_FIXED((float)win_w),
                .height = CLAY_SIZING_FIXED((float)win_h),
            },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .childGap = 0,
        },
    }) {
        CLAY(CLAY_ID("LeftPanel"), (Clay_ElementDeclaration){
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_FIXED(left_w),
                    .height = CLAY_SIZING_GROW(),
                },
            },
            .backgroundColor = left_bg,
        }) {}

        CLAY(CLAY_ID("Splitter"), (Clay_ElementDeclaration){
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_FIXED(spl_w),
                    .height = CLAY_SIZING_GROW(),
                },
            },
            .backgroundColor = spl_bg,
        }) {}

        CLAY(CLAY_ID("RightPanel"), (Clay_ElementDeclaration){
            .layout = {
                .sizing = {
                    .width  = CLAY_SIZING_GROW(),
                    .height = CLAY_SIZING_GROW(),
                },
            },
            .backgroundColor = right_bg,
        }) {}
    }

    Clay_RenderCommandArray cmds = Clay_EndLayout();
    clay_sdl2_render(ch->renderer, cmds);

    // ── Splitter handle dots (3 dots centred on splitter) ───────────────────
    Clay_Color dot_col = dark_mode
        ? ((spl_hover || spl_drag)
           ? (Clay_Color){120, 122, 135, 255}
           : (Clay_Color){ 80,  82,  92, 255})
        : ((spl_hover || spl_drag)
           ? (Clay_Color){140, 140, 150, 255}
           : (Clay_Color){180, 180, 188, 255});

    float dot_x = left_w + spl_w * 0.5f - 1.5f;
    float dot_y = (float)win_h * 0.5f - 10.0f;
    SDL_SetRenderDrawColor(ch->renderer,
                           (Uint8)dot_col.r, (Uint8)dot_col.g,
                           (Uint8)dot_col.b, (Uint8)dot_col.a);
    for (int d = 0; d < 3; d++) {
        SDL_FRect dot = { dot_x, dot_y + (float)d * 7.0f, 3.0f, 3.0f };
        SDL_RenderFillRectF(ch->renderer, &dot);
    }

    // ── Cache and return rects ───────────────────────────────────────────────
    AslanSplitRects rects = {
        .left_x  = 0.0f,
        .left_w  = left_w,
        .spl_x   = left_w,
        .spl_w   = spl_w,
        .right_x = left_w + spl_w,
        .right_w = right_w,
        .panel_h = (float)win_h,
    };
    ch->last_rects = rects;
    return rects;
}
