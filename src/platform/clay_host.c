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
    // Per-frame mouse state (set by clay_host_begin_frame)
    int            mouse_x, mouse_y;
    bool           mouse_pressed_this_frame;
    bool           last_mouse_left_down;
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
// Per-frame input update — call once at the start of each frame
// =============================================================================

void clay_host_begin_frame(ClayHost *ch) {
    if (!ch) return;
    int mx, my;
    Uint32 mstate = SDL_GetMouseState(&mx, &my);
    bool down = (mstate & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    ch->mouse_pressed_this_frame = down && !ch->last_mouse_left_down;
    ch->last_mouse_left_down     = down;
    ch->mouse_x = mx;
    ch->mouse_y = my;
}

// =============================================================================
// Top navigation bar (Step 4)
// =============================================================================

// Helper: render a single line of text centred vertically in a rect
static void sdl_draw_text(SDL_Renderer *r, TTF_Font *font,
                          const char *text, SDL_Color col,
                          float x, float y, float h) {
    if (!font || !text || !text[0]) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, col);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    if (tex) {
        SDL_FRect dst = {
            x,
            y + (h - (float)surf->h) * 0.5f,
            (float)surf->w,
            (float)surf->h
        };
        SDL_RenderCopyF(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

AslanNavbarResult clay_host_render_navbar(ClayHost *ch, bool dark_mode,
                                           int win_w, int win_h,
                                           const char *doc_title) {
    (void)win_h;
    AslanNavbarResult result = {0};
    if (!ch) return result;

    SDL_Renderer *r = ch->renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // ── Colours ──────────────────────────────────────────────────────────────
    SDL_Color bg, sep, title_c, btn_bg, btn_bg_hov, btn_txt;
    if (dark_mode) {
        bg        = (SDL_Color){ 24,  26,  32, 255};
        sep       = (SDL_Color){ 42,  44,  52, 255};
        title_c   = (SDL_Color){130, 132, 145, 255};
        btn_bg    = (SDL_Color){ 40,  42,  52, 255};
        btn_bg_hov= (SDL_Color){ 54,  56,  68, 255};
        btn_txt   = (SDL_Color){170, 172, 185, 255};
    } else {
        bg        = (SDL_Color){250, 250, 252, 255};
        sep       = (SDL_Color){220, 220, 226, 255};
        title_c   = (SDL_Color){140, 140, 155, 255};
        btn_bg    = (SDL_Color){238, 238, 244, 255};
        btn_bg_hov= (SDL_Color){225, 225, 232, 255};
        btn_txt   = (SDL_Color){ 80,  82,  96, 255};
    }

    // ── Navbar background ────────────────────────────────────────────────────
    SDL_FRect bar = {0, 0, (float)win_w, CLAY_NAVBAR_H};
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRectF(r, &bar);

    // Bottom separator
    SDL_FRect sep_line = {0, CLAY_NAVBAR_H - 1.0f, (float)win_w, 1.0f};
    SDL_SetRenderDrawColor(r, sep.r, sep.g, sep.b, sep.a);
    SDL_RenderFillRectF(r, &sep_line);

    // ── Document title (left-aligned) ────────────────────────────────────────
    TTF_Font *font = ch->fonts[ASLAN_FONT_INTER].font;
    if (font) TTF_SetFontSize(font, 13);
    if (font && doc_title && doc_title[0]) {
        sdl_draw_text(r, font, doc_title, title_c, 16.0f, 0, CLAY_NAVBAR_H);
    }

    // ── Theme toggle pill (right-aligned) ────────────────────────────────────
    const char *toggle_label = dark_mode ? "Light mode" : "Dark mode";
    int tw = 0, th = 0;
    if (font) TTF_SizeUTF8(font, toggle_label, &tw, &th);

    float pill_w  = (float)tw + 22.0f;
    float pill_h  = 24.0f;
    float pill_x  = (float)win_w - pill_w - 12.0f;
    float pill_y  = (CLAY_NAVBAR_H - pill_h) * 0.5f;
    SDL_FRect pill = {pill_x, pill_y, pill_w, pill_h};

    bool pill_hov = (ch->mouse_x >= (int)pill_x &&
                     ch->mouse_x <  (int)(pill_x + pill_w) &&
                     ch->mouse_y >= (int)pill_y &&
                     ch->mouse_y <  (int)(pill_y + pill_h));

    sdl_fill_rounded_rect(r, pill, 6.0f, pill_hov ? btn_bg_hov : btn_bg);
    if (font)
        sdl_draw_text(r, font, toggle_label, btn_txt,
                      pill_x + 11.0f, pill_y, pill_h);

    if (pill_hov && ch->mouse_pressed_this_frame)
        result.theme_toggled = 1;

    if (font) TTF_SetFontSize(font, 14);
    return result;
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
        .tab_h   = 0.0f,
    };
    ch->last_rects = rects;
    return rects;
}

// =============================================================================
// Left-panel tab bar (Step 2)
// =============================================================================

#define ASLAN_TAB_BAR_H 38.0f

float clay_host_render_tabs(ClayHost *ch, bool dark_mode,
                             float panel_x, float panel_w,
                             int *active_tab,
                             float mouse_x, float mouse_y,
                             bool mouse_clicked) {
    if (!ch || !active_tab) return 0.0f;
    if (*active_tab < 0) *active_tab = 0;

    SDL_Renderer *r = ch->renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // ── Colour palette ───────────────────────────────────────────────────────
    SDL_Color tab_bar_bg, active_pill, active_txt, inactive_txt, border_col;
    if (dark_mode) {
        tab_bar_bg   = (SDL_Color){ 28,  30,  36, 255};
        active_pill  = (SDL_Color){ 50,  52,  62, 255};
        active_txt   = (SDL_Color){220, 222, 230, 255};
        inactive_txt = (SDL_Color){110, 112, 125, 255};
        border_col   = (SDL_Color){ 48,  50,  60, 255};
    } else {
        tab_bar_bg   = (SDL_Color){252, 252, 253, 255};
        active_pill  = (SDL_Color){255, 255, 255, 255};
        active_txt   = (SDL_Color){ 30,  30,  40, 255};
        inactive_txt = (SDL_Color){130, 130, 145, 255};
        border_col   = (SDL_Color){220, 220, 225, 255};
    }

    // ── Tab bar background ───────────────────────────────────────────────────
    SDL_FRect bar = {panel_x, 0.0f, panel_w, ASLAN_TAB_BAR_H};
    SDL_SetRenderDrawColor(r, tab_bar_bg.r, tab_bar_bg.g, tab_bar_bg.b, tab_bar_bg.a);
    SDL_RenderFillRectF(r, &bar);

    // Bottom separator
    SDL_SetRenderDrawColor(r, border_col.r, border_col.g, border_col.b, border_col.a);
    SDL_FRect sep = {panel_x, ASLAN_TAB_BAR_H - 1.0f, panel_w, 1.0f};
    SDL_RenderFillRectF(r, &sep);

    // ── Tab pills ────────────────────────────────────────────────────────────
    const char *tab_labels[] = {"Table", "Gantt"};
    const int   tab_count    = 2;
    const float pill_h       = 26.0f;
    const float pill_y       = (ASLAN_TAB_BAR_H - pill_h) * 0.5f;
    const float pad_x        = 14.0f;

    // Use Inter font at 13px for tab labels
    TTF_Font *font = ch->fonts[ASLAN_FONT_INTER].font;
    if (font) TTF_SetFontSize(font, 13);

    float tab_x = panel_x + 8.0f;
    for (int i = 0; i < tab_count; i++) {
        int tw = 0, th = 0;
        if (font)
            TTF_SizeUTF8(font, tab_labels[i], &tw, &th);
        else
            tw = (int)(strlen(tab_labels[i]) * 8), th = 14;

        float pill_w = (float)tw + pad_x * 2.0f;
        SDL_FRect pill = {tab_x, pill_y, pill_w, pill_h};

        // Hit test
        if (mouse_clicked &&
            mouse_x >= pill.x && mouse_x < pill.x + pill.w &&
            mouse_y >= 0.0f   && mouse_y < ASLAN_TAB_BAR_H) {
            *active_tab = i;
        }

        bool is_active = (*active_tab == i);

        // Pill background (active only)
        if (is_active) {
            sdl_fill_rounded_rect(r, pill, 6.0f, active_pill);
        }

        // Tab label via SDL_ttf
        if (font) {
            SDL_Color tc = is_active ? active_txt : inactive_txt;
            SDL_Surface *surf = TTF_RenderUTF8_Blended(font, tab_labels[i], tc);
            if (surf) {
                SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex) {
                    SDL_FRect dst = {
                        pill.x + (pill.w - (float)surf->w) * 0.5f,
                        pill.y + (pill.h - (float)surf->h) * 0.5f,
                        (float)surf->w, (float)surf->h
                    };
                    SDL_RenderCopyF(r, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        tab_x += pill_w + 4.0f;
    }

    // Reset font size
    if (font) TTF_SetFontSize(font, 14);

    return ASLAN_TAB_BAR_H;
}

// =============================================================================
// Floating bottom toolbar (Step 3) — rendered AFTER nk_sdl_render() so it
// sits on top of all Nuklear content.
// =============================================================================

#define CLAY_TB_BTN   36.0f   // button size
#define CLAY_TB_GAP    8.0f   // gap between buttons
#define CLAY_TB_PAD   12.0f   // horizontal padding inside the pill
#define CLAY_TB_H     52.0f   // pill height
#define CLAY_TB_MB    20.0f   // margin from bottom of window

AslanToolbarResult clay_host_render_toolbar(ClayHost *ch, bool dark_mode,
                                             int win_w, int win_h,
                                             const char **icons_utf8,
                                             int btn_count) {
    AslanToolbarResult result = {-1};
    if (!ch || btn_count <= 0) return result;

    SDL_Renderer *r = ch->renderer;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    // ── Mouse state (set by clay_host_begin_frame) ──────────────────────────
    int  mx            = ch->mouse_x;
    int  my            = ch->mouse_y;
    bool mouse_pressed = ch->mouse_pressed_this_frame;

    // ── Geometry ─────────────────────────────────────────────────────────────
    float toolbar_w = (float)btn_count * CLAY_TB_BTN
                    + (float)(btn_count - 1) * CLAY_TB_GAP
                    + CLAY_TB_PAD * 2.0f;
    float tx = ((float)win_w - toolbar_w) / 2.0f;
    float ty = (float)win_h - CLAY_TB_H - CLAY_TB_MB;

    // ── Colours ──────────────────────────────────────────────────────────────
    SDL_Color bg, border_c, icon_c, icon_hov, btn_hov_bg;
    if (dark_mode) {
        bg         = (SDL_Color){ 42,  44,  52, 248};
        border_c   = (SDL_Color){ 60,  62,  74, 255};
        icon_c     = (SDL_Color){192, 194, 204, 255};
        icon_hov   = (SDL_Color){230, 232, 242, 255};
        btn_hov_bg = (SDL_Color){ 56,  58,  70, 255};
    } else {
        bg         = (SDL_Color){255, 255, 255, 248};
        border_c   = (SDL_Color){215, 215, 222, 255};
        icon_c     = (SDL_Color){ 70,  72,  84, 255};
        icon_hov   = (SDL_Color){ 25,  27,  38, 255};
        btn_hov_bg = (SDL_Color){240, 240, 246, 255};
    }

    // ── Layered drop shadow ──────────────────────────────────────────────────
    sdl_fill_rounded_rect(r,
        (SDL_FRect){tx,      ty + 8.0f, toolbar_w, CLAY_TB_H}, 26.0f,
        (SDL_Color){0,0,0,10});
    sdl_fill_rounded_rect(r,
        (SDL_FRect){tx + 1,  ty + 4.0f, toolbar_w, CLAY_TB_H}, 26.0f,
        (SDL_Color){0,0,0,18});
    sdl_fill_rounded_rect(r,
        (SDL_FRect){tx + 1,  ty + 2.0f, toolbar_w, CLAY_TB_H}, 26.0f,
        (SDL_Color){0,0,0,8});

    // ── Border (1px frame, slightly larger than the bg pill) ─────────────────
    sdl_fill_rounded_rect(r,
        (SDL_FRect){tx - 1, ty - 1, toolbar_w + 2, CLAY_TB_H + 2}, 27.0f,
        border_c);

    // ── Background pill ───────────────────────────────────────────────────────
    sdl_fill_rounded_rect(r,
        (SDL_FRect){tx, ty, toolbar_w, CLAY_TB_H}, 26.0f, bg);

    // ── Buttons ───────────────────────────────────────────────────────────────
    TTF_Font *icon_font = ch->fonts[ASLAN_FONT_ICONS].font;
    if (icon_font) TTF_SetFontSize(icon_font, 20);

    float btn_y = ty + (CLAY_TB_H - CLAY_TB_BTN) / 2.0f;

    for (int i = 0; i < btn_count; i++) {
        float bx = tx + CLAY_TB_PAD + (float)i * (CLAY_TB_BTN + CLAY_TB_GAP);
        SDL_FRect btn = {bx, btn_y, CLAY_TB_BTN, CLAY_TB_BTN};

        bool hovered = ((float)mx >= bx && (float)mx < bx + CLAY_TB_BTN &&
                        (float)my >= btn_y && (float)my < btn_y + CLAY_TB_BTN);

        if (hovered) {
            sdl_fill_rounded_rect(r, btn, 8.0f, btn_hov_bg);
            if (mouse_pressed)
                result.clicked_btn = i;
        }

        // Icon via icon font
        if (icon_font && icons_utf8 && icons_utf8[i]) {
            SDL_Color ic = hovered ? icon_hov : icon_c;
            SDL_Surface *surf = TTF_RenderUTF8_Blended(icon_font, icons_utf8[i], ic);
            if (surf) {
                SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex) {
                    SDL_FRect dst = {
                        bx  + (CLAY_TB_BTN - (float)surf->w) * 0.5f,
                        btn_y + (CLAY_TB_BTN - (float)surf->h) * 0.5f,
                        (float)surf->w, (float)surf->h
                    };
                    SDL_RenderCopyF(r, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }
    }

    if (icon_font) TTF_SetFontSize(icon_font, 18);
    return result;
}
