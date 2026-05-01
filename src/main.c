#include "core/engine.h"
#include "document/document.h"
#include "platform/renderer.h"
#include "platform/ui_host.h"
#include "platform/ui_shared.h"
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <libgen.h>
#include <limits.h>
#endif

// =============================================================================
// ASLAN - Main Entry Point
// =============================================================================

// -----------------------------------------------------------------------------
// Interactive CLI Mode (for debugging)
// -----------------------------------------------------------------------------

void run_interactive_mode(Engine *engine) {
  char buffer[256];

  printf("\n=== Aslan Interactive Shell ===\n");
  printf("Type 'help' for commands.\n");

  while (engine->is_running) {
    printf("> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
      break;

    buffer[strcspn(buffer, "\n")] = 0;

    if (strlen(buffer) == 0)
      continue;

    if (!engine_execute_command(engine, buffer)) {
      printf("Unknown command. Type 'help'.\n");
    }
  }
}

// -----------------------------------------------------------------------------
// Per-frame event + render logic (shared by native and Emscripten)
// -----------------------------------------------------------------------------

static void process_events_and_render(Engine *engine, Renderer *renderer,
                                      UIHost *ui,
                                      uint32_t *last_click_ms,
                                      float *last_click_world_x,
                                      float *last_click_world_y) {
  const uint32_t dbl_click_ms  = 350;
  const float    dbl_click_dist = 6.0f;

  SDL_Event e;

#ifdef __EMSCRIPTEN__
  // Non-blocking poll in Emscripten (requestAnimationFrame drives timing)
  int has_event = SDL_PollEvent(&e);
  if (has_event) ui->needs_redraw = 1;
#else
  int timeout_ms = ui->has_active_animation ? 16 : 100;
  int has_event  = SDL_WaitEventTimeout(&e, timeout_ms);
  if (has_event) ui->needs_redraw = 1;
#endif

  ui_begin_frame(ui);

  if (has_event) {
    do {
        if (e.type == SDL_DROPFILE) {
          ASLAN_DEBUG("SDL_DROPFILE event detected in event loop");
        }

        // Quit event
        if (e.type == SDL_QUIT) {
          engine->is_running = false;
        }

        // Window resize for HiDPI support
        if (e.type == SDL_WINDOWEVENT &&
            (e.window.event == SDL_WINDOWEVENT_RESIZED ||
             e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             e.window.event == SDL_WINDOWEVENT_EXPOSED)) {
          renderer_update_size(renderer);
          ui->needs_redraw = 1;
        }

        if (e.type == SDL_MOUSEBUTTONDOWN) {
          ASLAN_DEBUG("MAIN SDL_MOUSEBUTTONDOWN: (%d, %d) btn=%d", e.button.x,
                      e.button.y, e.button.button);
        }

        // Global keyboard shortcuts (Cmd = macOS, Ctrl = Linux/Windows)
        if (e.type == SDL_KEYDOWN && !ui->text_input_active) {
          SDL_Keymod mods = SDL_GetModState();
          bool cmd = (mods & KMOD_GUI) != 0;
          bool ctrl = (mods & KMOD_CTRL) != 0;
          bool shift = (mods & KMOD_SHIFT) != 0;
          bool mod = cmd || ctrl;

          // Cmd+Z / Cmd+Shift+Z — Undo / Redo
          if (mod && e.key.keysym.sym == SDLK_z) {
            engine_execute_command(engine, shift ? "redo" : "undo");
          }
          // Cmd+C — Copy selected node
          if (mod && !shift && e.key.keysym.sym == SDLK_c) {
            ui_host_copy_node(ui, engine);
          }
          // Cmd+V — Paste node from clipboard
          if (mod && !shift && e.key.keysym.sym == SDLK_v) {
            ui_host_paste_node(ui, engine);
          }
          // Cmd+S — Save snapshot
          if (mod && e.key.keysym.sym == SDLK_s) {
            if (engine->doc_path[0]) {
              engine_save_snapshot_as_aslan(engine, engine->doc_path);
            }
          }
          // Cmd+I — Import CSV as new table block
          if (mod && e.key.keysym.sym == SDLK_i) {
            ui_host_import_csv(ui, engine);
          }
          // Cmd+E — Export design block as SVG
          if (mod && e.key.keysym.sym == SDLK_e) {
            ui_host_export_svg(ui, engine);
          }
          // Cmd+N — New table block on active page
          if (mod && e.key.keysym.sym == SDLK_n) {
            Page *page = doc_page_get_active(engine->doc);
            if (page) {
              char cmd_buf[128];
              snprintf(cmd_buf, sizeof(cmd_buf), "block_create %u 0 table",
                       page->id);
              engine_execute_command(engine, cmd_buf);
            }
          }
          // Escape — exit table full-screen edit mode back to canvas
          if (e.key.keysym.sym == SDLK_ESCAPE && ui->expanded_block_id != 0) {
            ui->expanded_block_id = 0;
            ui->needs_redraw = 1;
          }
          // Delete / Backspace — Delete active block (only if no node selected)
          if ((e.key.keysym.sym == SDLK_DELETE ||
               e.key.keysym.sym == SDLK_BACKSPACE) &&
              !mod && ui->active_block_id != 0 && ui->selected_node == NULL) {
            Page *page = doc_page_get_active(engine->doc);
            if (page) {
              char cmd_buf[128];
              snprintf(cmd_buf, sizeof(cmd_buf), "block_delete %u %u", page->id,
                       ui->active_block_id);
              engine_execute_command(engine, cmd_buf);
              ui->active_block_id = 0;
            }
          }
        }

        // Forward event to UI
        ui_handle_event(ui, &e);

        // NanoVG Widgets keyboard/text input is already handled
        // inside ui_handle_event() via nvgw_input_keyboard().
        // Only forward scroll events here.
        if (e.type == SDL_MOUSEWHEEL) {
          ui->nvgw.scroll_x += e.wheel.x;
          ui->nvgw.scroll_y += e.wheel.y;
        }

        // Handle mouse/keyboard events for Lua tools
        if (!ui->text_input_active &&
            (!ui || ui->editing_block_id == 0)) {

          int imx, imy;
          SDL_GetMouseState(&imx, &imy);
          float mx = (float)imx;
          float my = (float)imy;

          float world_x = mx;
          float world_y = my;

          if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (e.button.button == SDL_BUTTON_LEFT && ui && ui->selected_node) {
              uint32_t now = SDL_GetTicks();
              float dx = world_x - *last_click_world_x;
              float dy = world_y - *last_click_world_y;
              float d2 = dx * dx + dy * dy;
              bool is_dbl = (*last_click_ms &&
                             (now - *last_click_ms) <= dbl_click_ms &&
                             d2 <= (dbl_click_dist * dbl_click_dist));

              if (ui->selected_node->type == NODE_TYPE_TEXT) {
                if (is_dbl) {
                  ui->editing_block_id = ui->selected_node->id;
                  *last_click_ms = 0;
                } else {
                  *last_click_ms = now;
                  *last_click_world_x = world_x;
                  *last_click_world_y = world_y;
                }
              } else if (ui->selected_node->type == NODE_TYPE_TABLE_VIEW) {
                if (is_dbl) {
                  ui->expanded_block_id = ui->selected_node->linked_block_id;
                  ui->needs_redraw = 1;
                  *last_click_ms = 0;
                } else {
                  *last_click_ms = now;
                  *last_click_world_x = world_x;
                  *last_click_world_y = world_y;
                }
              } else if (ui->selected_node->type == NODE_TYPE_CHART) {
                if (is_dbl) {
                  Node *cn = ui->selected_node;
                  uint32_t tbl_id = cn->chart_data ? cn->chart_data->source_table_id : 0;
                  if (tbl_id == 0) {
                    // No linked table yet — create one and link it
                    Page *page = doc_page_get_active(engine->doc);
                    if (page) {
                      tbl_id = page->next_block_id;
                      char cmd_buf[128];
                      snprintf(cmd_buf, sizeof(cmd_buf), "block_create %u %u table",
                               page->id, tbl_id);
                      engine_execute_command(engine, cmd_buf);
                      if (cn->chart_data)
                        cn->chart_data->source_table_id = tbl_id;
                    }
                  }
                  if (tbl_id != 0) {
                    ui->expanded_block_id = tbl_id;
                    ui->needs_redraw = 1;
                  }
                  *last_click_ms = 0;
                } else {
                  *last_click_ms = now;
                  *last_click_world_x = world_x;
                  *last_click_world_y = world_y;
                }
              }
            }
          }
        }

        // 5. Handle File Drag & Drop (always process, regardless of UI state)
        if (e.type == SDL_DROPFILE) {
          char *dropped_filedir = e.drop.file;
          int mx, my;
          SDL_GetMouseState(&mx, &my);
          ASLAN_DEBUG("SDL_DROPFILE event received");
          ui_host_handle_file_drop(ui, dropped_filedir, mx, my);
          SDL_free(dropped_filedir);
        }
      } while (SDL_PollEvent(&e)); // Drain remaining events
    }

  // Finalize NanoVG Widgets input per frame
  int gmx, gmy;
  uint32_t gmstate = SDL_GetMouseState(&gmx, &gmy);
  nvgw_input_mouse(&ui->nvgw, (float)gmx, (float)gmy,
                   (gmstate & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0,
                   ui->nvgw.scroll_x, ui->nvgw.scroll_y);

  // Always redraw on Emscripten (requestAnimationFrame drives timing)
#ifdef __EMSCRIPTEN__
  ui->needs_redraw = 1;
#endif

  if (ui->needs_redraw) {
    ui->needs_redraw = 0;
    renderer_begin(renderer, ui->dark_mode);
    renderer_draw_document(renderer, engine->doc);
    if (ui->selected_node)
      renderer_draw_selection(renderer, engine->doc, ui->selected_node);
    int ww, wh;
    SDL_GetWindowSize(renderer->window, &ww, &wh);
    ui_render(ui, engine, ww, wh);
    renderer_end(renderer);
  }

#ifdef __EMSCRIPTEN__
  // Deferred image dialog: open AFTER renderer_end so the frame is visible
  if (ui->deferred_image_dialog) {
    ui->deferred_image_dialog = 0;
    char path[512] = {0};
    extern int open_image_file_dialog(char *, size_t);
    extern int copy_image_to_assets(const char *, const char *, char *, size_t);
    if (open_image_file_dialog(path, sizeof(path))) {
      char rel[256] = {0};
      if (copy_image_to_assets(path, ".", rel, sizeof(rel))) {
        Page *page = doc_page_get_active(engine->doc);
        if (page) {
          Block *block = page_find_block_by_id(page, ui->deferred_image_block_id);
          if (block && block->type == BLOCK_DESIGN) {
            DesignData *dd = &block->data.design;
            if (ui->deferred_image_node_id != 0) {
              // Fill existing container node
              Node *n = block_design_find_node(block, ui->deferred_image_node_id);
              if (n) {
                strncpy(n->image_path, rel, sizeof(n->image_path) - 1);
                n->image_path[sizeof(n->image_path) - 1] = '\0';
              }
            } else {
              // Create new image node
              Node *n = node_create(dd->next_node_id++, NODE_TYPE_IMAGE, NULL);
              if (n && dd->root) {
                n->x = ui->deferred_image_nx;
                n->y = ui->deferred_image_ny;
                n->width = 200.0f; n->height = 150.0f;
                n->visible = 1; n->lock_ratio = 1;
                n->parent = dd->root;
                strncpy(n->image_path, rel, sizeof(n->image_path) - 1);
                if (!dd->root->first_child) dd->root->first_child = n;
                else {
                  Node *last = dd->root->first_child;
                  while (last->next_sibling) last = last->next_sibling;
                  last->next_sibling = n;
                }
                ui->selected_node = n;
              }
            }
            ui->needs_redraw = 1;
          }
        }
      }
    }
  }
#endif

  nvgw_input_end(&ui->nvgw);
}

// -----------------------------------------------------------------------------
// Visual Mode (native desktop: blocking while loop)
// -----------------------------------------------------------------------------

void run_visual_mode(Engine *engine, Renderer *renderer, UIHost *ui) {
  uint32_t last_click_ms    = 0;
  float    last_click_world_x = 0.0f;
  float    last_click_world_y = 0.0f;
  ui->needs_redraw = 1;

  while (engine->is_running) {
    process_events_and_render(engine, renderer, ui,
                              &last_click_ms,
                              &last_click_world_x,
                              &last_click_world_y);
  }
}

// -----------------------------------------------------------------------------
// Emscripten main loop globals + callback
// -----------------------------------------------------------------------------
#ifdef __EMSCRIPTEN__
static Engine   *g_em_engine   = NULL;
static Renderer *g_em_renderer = NULL;
static UIHost   *g_em_ui       = NULL;
static uint32_t  g_em_last_click_ms      = 0;
static float     g_em_last_click_world_x = 0.0f;
static float     g_em_last_click_world_y = 0.0f;

static void em_main_loop_cb(void) {
  process_events_and_render(g_em_engine, g_em_renderer, g_em_ui,
                            &g_em_last_click_ms,
                            &g_em_last_click_world_x,
                            &g_em_last_click_world_y);
}
#endif

// -----------------------------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------------------------

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  printf("Starting Aslan Visual Engine v0.1.7 (Interactive Phase 8)...\n");

  // 1. Initialize Engine (Core)
  Engine *engine = engine_create();
  if (!engine)
    return 1;

  // Handle command-line project path
  if (argc >= 2) {
    engine_set_document_path(engine, argv[1]);
  }

  // 2. Initialize Renderer (SDL2)
  Renderer *renderer = renderer_create("Aslan Editor v0.1", 1024, 768);
  if (!renderer) {
    engine_destroy(engine);
    return 1;
  }

  // 3. Initialize UI (Aslan/NanoVG)
  UIHost *ui = ui_init(renderer->window);
  engine->ui_host = ui;

  // 4. Replay history (AOF)
  engine_load_from_history(engine);

  // 5. Create default content only on first launch
  if (engine->doc->page_count == 0) {
    engine_execute_command(engine, "page_create 1 \"New Page\"");
    engine_execute_command(engine, "block_create 1 1 design");
    engine_execute_command(engine, "page_activate 1");
  } else {
    // Ensure a page is active if history was loaded
    if (!doc_page_get_active(engine->doc)) {
      engine_execute_command(engine, "page_activate 1");
    }
  }

#ifdef __EMSCRIPTEN__
  // Emscripten: hand control to requestAnimationFrame loop
  g_em_engine   = engine;
  g_em_renderer = renderer;
  g_em_ui       = ui;
  ui->needs_redraw = 1;
  emscripten_set_main_loop(em_main_loop_cb, 0, 1); // 0fps = rAF, 1 = no return
  // (never reached)
  return 0;
#else
  // 6. Run visual mode
  run_visual_mode(engine, renderer, ui);

  // 7. Cleanup
  ui_shutdown(ui);
  renderer_destroy(renderer);
  engine_destroy(engine);

  return 0;
#endif
}
