#include "renderer.h"
#include "ui_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// OpenGL (macOS Core Profile 3.2)
#ifdef __APPLE__
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/gl3.h>
#else
#  include <GL/gl.h>
#endif

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

  // Request OpenGL 3.2 Core Profile (macOS max; also fine on Linux/Windows)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  Renderer *r = (Renderer *)malloc(sizeof(Renderer));
  if (!r)
    return NULL;

  r->sdl_renderer = NULL; // Not used in GL mode

  r->window = SDL_CreateWindow(
      title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height,
      SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI |
          SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP);

  if (!r->window) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    free(r);
    return NULL;
  }

  r->gl_ctx = SDL_GL_CreateContext(r->window);
  if (!r->gl_ctx) {
    printf("OpenGL context could not be created! SDL_Error: %s\n", SDL_GetError());
    SDL_DestroyWindow(r->window);
    free(r);
    return NULL;
  }
  SDL_GL_MakeCurrent(r->window, r->gl_ctx);
  SDL_GL_SetSwapInterval(1); // vsync

  // Sync dimensions from the drawable (HiDPI-aware)
  renderer_update_size(r);

  // Enable drag-and-drop file events
  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
  ASLAN_DEBUG("SDL_DROPFILE events enabled");

  return r;
}

void renderer_destroy(Renderer *r) {
  if (!r)
    return;
  if (r->gl_ctx)
    SDL_GL_DeleteContext(r->gl_ctx);
  SDL_DestroyWindow(r->window);
  free(r);
  SDL_Quit();
}

void renderer_update_size(Renderer *r) {
  if (!r) return;
  // Use drawable size (accounts for HiDPI / Retina)
  SDL_GL_GetDrawableSize(r->window, &r->width, &r->height);
}

float renderer_get_dpi_scale(Renderer *r) {
  if (!r || !r->window) return 1.0f;
  int window_w, window_h, draw_w, draw_h;
  SDL_GetWindowSize(r->window, &window_w, &window_h);
  SDL_GL_GetDrawableSize(r->window, &draw_w, &draw_h);
  if (window_w > 0)
    return (float)draw_w / (float)window_w;
  return 1.0f;
}

// -----------------------------------------------------------------------------
// Frame Begin/End
// -----------------------------------------------------------------------------

void renderer_begin(Renderer *r, int dark_mode) {
  if (!r) return;
  int w, h;
  SDL_GL_GetDrawableSize(r->window, &w, &h);
  glViewport(0, 0, w, h);
  if (dark_mode)
    glClearColor(30.0f/255.0f, 30.0f/255.0f, 35.0f/255.0f, 1.0f);
  else
    glClearColor(252.0f/255.0f, 252.0f/255.0f, 253.0f/255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_end(Renderer *r) {
  if (!r) return;
  SDL_GL_SwapWindow(r->window);
}

// -----------------------------------------------------------------------------
// Document Drawing (Optional - main UI is in Nuklear)
// -----------------------------------------------------------------------------

static void draw_grid(Renderer *r, int step) {
  if (!r || !r->sdl_renderer || step < 2)
    return;

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

static void draw_node(Renderer *r, const Node *node, float ox, float oy,
                      float zoom) {
  if (!r || !node || !node->visible)
    return;

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

static void draw_node_tree(Renderer *r, const Node *node, float ox, float oy,
                           float zoom) {
  if (!node)
    return;

  // Draw this node (skip ROOT type)
  if (node->type != NODE_TYPE_ROOT) {
    draw_node(r, node, ox, oy, zoom);
  }

  // Draw children
  for (Node *child = node->first_child; child; child = child->next_sibling) {
    draw_node_tree(r, child, ox, oy, zoom);
  }
}

void renderer_draw_document(Renderer *r, const Document *doc,
                            struct nk_context *nk_ctx) {
  (void)nk_ctx;
  if (!r || !doc || !r->sdl_renderer)
    return;

  // Get active page
  Page *page = NULL;
  if (doc->active_page_idx >= 0 && doc->active_page_idx < 32) {
    if (doc->pages[doc->active_page_idx].in_use) {
      page = (Page *)&doc->pages[doc->active_page_idx];
    }
  }

  if (!page)
    return;

  // Optional: Draw grid based on global setting
  const char *grid_enabled = doc_get_global(doc, "grid.enabled");
  if (grid_enabled &&
      (strcmp(grid_enabled, "1") == 0 || strcmp(grid_enabled, "true") == 0)) {
    const char *grid_step_str = doc_get_global(doc, "grid.step");
    int grid_step = grid_step_str ? atoi(grid_step_str) : 20;
    if (grid_step < 5)
      grid_step = 5;
    if (grid_step > 200)
      grid_step = 200;
    draw_grid(r, grid_step);
  }

  // Note: Block rendering is handled by Nuklear in ui_host.c
  // This function can be used for additional low-level drawing if needed
}

void renderer_draw_selection(Renderer *r, const Document *doc,
                             Node *selected_node) {
  (void)doc;
  if (!r || !selected_node || !r->sdl_renderer)
    return;

  // Draw selection box around selected node
  float sx = selected_node->x;
  float sy = selected_node->y;
  float sw = selected_node->width;
  float sh = selected_node->height;

  // Selection highlight (blue border)
  SDL_Rect rect = {(int)sx - 2, (int)sy - 2, (int)sw + 4, (int)sh + 4};
  SDL_SetRenderDrawColor(r->sdl_renderer, 110, 110, 130, 255);
  SDL_RenderDrawRect(r->sdl_renderer, &rect);

  // Corner handles
  const int handle_size = 6;
  SDL_Rect handles[4] = {
      {rect.x - handle_size / 2, rect.y - handle_size / 2, handle_size,
       handle_size},
      {rect.x + rect.w - handle_size / 2, rect.y - handle_size / 2, handle_size,
       handle_size},
      {rect.x - handle_size / 2, rect.y + rect.h - handle_size / 2, handle_size,
       handle_size},
      {rect.x + rect.w - handle_size / 2, rect.y + rect.h - handle_size / 2,
       handle_size, handle_size}};

  SDL_SetRenderDrawColor(r->sdl_renderer, 255, 255, 255, 255);
  for (int i = 0; i < 4; i++) {
    SDL_RenderFillRect(r->sdl_renderer, &handles[i]);
  }
  SDL_SetRenderDrawColor(r->sdl_renderer, 110, 110, 130, 255);
  for (int i = 0; i < 4; i++) {
    SDL_RenderDrawRect(r->sdl_renderer, &handles[i]);
  }
}

// (renderer_draw_canvas_block removed — Canvas block type has been deleted)\n

void renderer_draw_design_block(Renderer *r, const Block *block, float x,
                                float y, float w, float h) {
  if (!r || !block || block->type != BLOCK_DESIGN || !r->sdl_renderer)
    return;

  const DesignData *dd = &block->data.design;

  // Calculate scale to fit
  float scale_x = w / (float)dd->width;
  float scale_y = h / (float)dd->height;
  float scale = scale_x < scale_y ? scale_x : scale_y;
  if (scale > 1.0f)
    scale = 1.0f;

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

  // Border (neutral)
  SDL_SetRenderDrawColor(r->sdl_renderer, 110, 110, 130, 255);
  SDL_RenderDrawRect(r->sdl_renderer, &bounds);

  // Draw nodes
  if (dd->root) {
    draw_node_tree(r, dd->root, dx, dy, scale);
  }
}
