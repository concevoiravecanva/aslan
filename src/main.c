#include "core/engine.h"
#include "document/document.h"
#include "platform/platform.h"
#include "platform/renderer.h"
#include "platform/ui_host.h"
#include "platform/ui_shared.h"
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <libgen.h>
#include <limits.h>

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
// Visual Mode (Main Application Loop)
// -----------------------------------------------------------------------------

void run_visual_mode(Engine *engine, Renderer *renderer, UIHost *ui) {
  SDL_Event e;

  // Double-click detection
  static uint32_t last_click_ms = 0;
  static float last_click_world_x = 0.0f;
  static float last_click_world_y = 0.0f;
  const uint32_t dbl_click_ms = 350;
  const float dbl_click_dist = 6.0f;

  // Initial render needed
  ui->needs_redraw = 1;

  while (engine->is_running) {
    // Event-driven: wait for events instead of polling
    // Use 16ms timeout when animation active, 100ms for cursor blink, etc.
    int timeout_ms = ui->has_active_animation ? 16 : 100;
    int has_event = SDL_WaitEventTimeout(&e, timeout_ms);

    // 1. Input handling
    nk_input_begin(ui->ctx);

    // Process events (either the one we got or check for more)
    if (has_event) {
      ui->needs_redraw = 1; // Any event triggers redraw
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
        if (e.type == SDL_KEYDOWN && !nk_item_is_any_active(ui->ctx)) {
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

        // Handle mouse/keyboard events for Lua tools
        if (!nk_item_is_any_active(ui->ctx) &&
            (!ui || ui->editing_block_id == 0)) {

          int imx, imy;
          SDL_GetMouseState(&imx, &imy);
          float mx = (float)imx;
          float my = (float)imy;

          float world_x = mx;
          float world_y = my;

          if (e.type == SDL_MOUSEBUTTONDOWN) {
            // Double-click detection for TEXT node editing
            if (e.button.button == SDL_BUTTON_LEFT && ui && ui->selected_node &&
                ui->selected_node->type == NODE_TYPE_TEXT) {
              uint32_t now = SDL_GetTicks();
              float dx = world_x - last_click_world_x;
              float dy = world_y - last_click_world_y;
              float d2 = dx * dx + dy * dy;

              if (last_click_ms && (now - last_click_ms) <= dbl_click_ms &&
                  d2 <= (dbl_click_dist * dbl_click_dist)) {
                ui->editing_block_id = ui->selected_node->id;
                last_click_ms = 0;
              } else {
                last_click_ms = now;
                last_click_world_x = world_x;
                last_click_world_y = world_y;
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

    nk_input_end(ui->ctx);

    // Only render if needed (event occurred or timeout for animations)
    if (ui->needs_redraw) {
      // Clear BEFORE render so mid-render ui_request_redraw() survives
      ui->needs_redraw = 0;

      // 2. Render background
      renderer_begin(renderer, ui->dark_mode);

      // 3. Render document
      renderer_draw_document(renderer, engine->doc, ui->ctx);

      // 4. Render selection handles
      if (ui->selected_node) {
        renderer_draw_selection(renderer, engine->doc, ui->selected_node);
      }

      // 5. Render UI overlay
      int ww, wh;
      SDL_GetWindowSize(renderer->window, &ww, &wh);
      ui_render(ui, engine, ww, wh);

      // 6. Present frame
      renderer_end(renderer);
    }

    // No SDL_Delay needed - WaitEventTimeout handles timing
  }
}

// -----------------------------------------------------------------------------
// Main Entry Point
// -----------------------------------------------------------------------------

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  platform_init();
  platform_log("Starting Aslan Visual Engine v0.1.7 (Interactive Phase 8)...");

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

  // 3. Initialize UI (Nuklear)
  UIHost *ui = ui_init(renderer->window, renderer->sdl_renderer);
  engine->ui_host = ui;

  // 4. Replay history (AOF)
  engine_load_from_history(engine);

  // 5. Create default content only on first launch
  if (engine->doc->page_count == 0) {
    engine_execute_command(engine, "page_create 1 \"New Note\"");
    engine_execute_command(engine, "block_create 1 1 h1");
    engine_execute_command(engine, "block_text 1 1 \"New Note\"");
    engine_execute_command(engine, "page_activate 1");
  } else {
    // Ensure a page is active if history was loaded
    if (!doc_page_get_active(engine->doc)) {
      engine_execute_command(engine, "page_activate 1");
    }
  }

  // 6. Run visual mode
  run_visual_mode(engine, renderer, ui);

  // 7. Cleanup
  ui_shutdown(ui);
  renderer_destroy(renderer);
  engine_destroy(engine);
  platform_shutdown();

  return 0;
}
