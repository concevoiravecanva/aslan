#include "ui_host.h"
#include "../scripting/lua_host.h"
#include "recents.h"
#include <SDL.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

// Globals API (theme)
const char *doc_get_global(const Document *doc, const char *key);

// Clip API forward declarations
double clip_page_total_duration(const AslanClipPage *page);

// Page type API
bool doc_page_set_type(Document *doc, uint32_t page_id, PageType type);

// Callout functions removed - simplified Note page

// =========================
// GRID page UI (v1)
// =========================
// NOTE: We pass engine/doc so the grid can emit AOF commands (history.log
// persistence).
static void ui_render_grid_page(struct nk_context *ctx, Engine *engine,
                                uint32_t page_id, AslanGrid *grid) {
  if (!ctx || !grid)
    return;
  if (grid->rows <= 0 || grid->cols <= 0)
    return;
  if (!grid->data || !grid->col_widths)
    return;

  // Auto-size columns based on current content. Keep it cheap for now.
  // Use JetBrains Mono (same as NOTE) when available so widths are stable.
  const struct nk_user_font *grid_font = ctx->style.font;
  if (grid_font)
    grid_compute_column_widths(grid, (const void *)grid_font);

  if (grid->active_row < 0)
    grid->active_row = 0;
  if (grid->active_col < 0)
    grid->active_col = 0;
  if (grid->active_row >= grid->rows)
    grid->active_row = grid->rows - 1;
  if (grid->active_col >= grid->cols)
    grid->active_col = grid->cols - 1;

  // Antirez-style minimal: simple row loop with fixed-height edits.
  const float row_h = 26.0f;

  // Navigation request (applied after the loop to avoid fighting Nuklear
  // layout)
  int nav_dr = 0;
  int nav_dc = 0;
  int grid_cell_active_after = 0;
  int should_unfocus = 0;

  for (int r = 0; r < grid->rows; r++) {
    // sum widths for nk_layout_row_static is overrated; we layout each cell in
    // a row by calling nk_layout_row_begin/end and pushing widths.
    nk_layout_row_begin(ctx, NK_STATIC, row_h, grid->cols);
    for (int c = 0; c < grid->cols; c++) {
      int w = grid->col_widths[c];
      if (w < 40)
        w = 40;
      nk_layout_row_push(ctx, (float)w);

      // Cell buffer: keep it local; editing will be wired to persist changes.
      char buf[256];
      buf[0] = 0;
      const char *original =
          (grid->data[r] && grid->data[r][c]) ? grid->data[r][c] : "";
      strncpy(buf, original, sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = 0;

      // Render as editable field.
      // Important: do NOT use NK_EDIT_ALLOW_TAB or NK_EDIT_MULTILINE here.
      // We want TAB/ENTER to navigate like a spreadsheet.
      const nk_flags cell_flags =
          NK_EDIT_FIELD | NK_EDIT_AUTO_SELECT | NK_EDIT_SIG_ENTER;

      const int is_model_active_cell =
          (r == grid->active_row && c == grid->active_col);
      if (is_model_active_cell) {
        /* Ensure the active cell gets focus when we navigate. */
        nk_edit_focus(ctx, NK_EDIT_FIELD);
      }

      nk_flags edit_ret = nk_edit_string_zero_terminated(
          ctx, cell_flags, buf, (int)sizeof(buf) - 1, nk_filter_default);

      // Click-to-focus: if this widget became active, record its coords.
      if (nk_item_is_any_active(ctx)) {
        grid->active_row = r;
        grid->active_col = c;
        grid_cell_active_after = 1;
      }

      // Persist changes if the buffer differs from original.
      // IMPORTANT: don't call grid_set_cell here if we also call
      // engine_execute_command. Otherwise we apply twice and can cause
      // duplicate history lines.
      if (strcmp(buf, original) != 0) {
        if (engine) {
          // Persist to AOF via engine command (so it appears in history.log).
          // Escape quotes but keep it minimal.
          char escaped[256];
          int j = 0;
          for (int k = 0; buf[k] && j < (int)sizeof(escaped) - 2; k++) {
            if (buf[k] == '"')
              escaped[j++] = '\\';
            escaped[j++] = buf[k];
          }
          escaped[j] = 0;
          char cmd[512];
          snprintf(cmd, sizeof(cmd), "grid_set %u %d %d \"%s\"",
                   (unsigned)page_id, r, c, escaped);
          engine_execute_command(engine, cmd);
        } else {
          grid_set_cell(grid, r, c, buf[0] ? buf : NULL);
        }
      }

      // Spreadsheet-style commit/navigation.
      // Nuklear will set NK_EDIT_COMMITED when Enter is pressed with
      // NK_EDIT_SIG_ENTER. For Tab we still need to intercept; it otherwise
      // triggers navigation in Nuklear.
      if (grid_cell_active_after && grid->active_row == r &&
          grid->active_col == c) {
        if (edit_ret & NK_EDIT_COMMITED) {
          nav_dr = 1;
          should_unfocus = 1;
        }
      }
    }
    nk_layout_row_end(ctx);
  }

  // TAB moves right.
  // Detect *after* rendering so we know whether a cell ended up active this
  // frame. Enter is handled via NK_EDIT_COMMITED above (more reliable than raw
  // key state).
  if (grid_cell_active_after) {
    if (nk_input_is_key_pressed(&ctx->input, NK_KEY_TAB)) {
      nav_dc = 1;
      should_unfocus = 1;
    }
  }

  // Apply navigation after rendering so focus moves cleanly.
  if (nav_dr || nav_dc) {
    int nr = grid->active_row + nav_dr;
    int nc = grid->active_col + nav_dc;
    if (nc >= grid->cols) {
      nc = 0;
      nr++;
    }
    if (nr >= grid->rows)
      nr = grid->rows - 1;
    if (nr < 0)
      nr = 0;
    if (nc < 0)
      nc = 0;

    grid->active_row = nr;
    grid->active_col = nc;

    // Force next widget focus by removing current active state.
    if (should_unfocus)
      nk_edit_unfocus(ctx);
  }
}

// =========================
// CLIP page UI (Video Sequence Editor)
// =========================
// Format time as MM:SS.ms
static void format_time(double seconds, char *buf, size_t buf_size) {
  if (seconds < 0)
    seconds = 0;
  int mins = (int)(seconds / 60.0);
  double secs = seconds - (double)(mins * 60);
  snprintf(buf, buf_size, "%02d:%05.2f", mins, secs);
}

// Parse time from MM:SS or MM:SS.ms format
static double parse_time(const char *str) {
  if (!str || !str[0])
    return 0.0;
  int mins = 0;
  double secs = 0.0;
  if (sscanf(str, "%d:%lf", &mins, &secs) >= 1) {
    return (double)mins * 60.0 + secs;
  }
  // Try just seconds
  if (sscanf(str, "%lf", &secs) == 1) {
    return secs;
  }
  return 0.0;
}

// Extract filename from path
static const char *get_filename(const char *path) {
  if (!path || !path[0])
    return "(no file)";
  const char *last_slash = strrchr(path, '/');
  if (!last_slash)
    last_slash = strrchr(path, '\\');
  return last_slash ? last_slash + 1 : path;
}

static void ui_render_clip_page(struct nk_context *ctx, Engine *engine,
                                uint32_t page_id, AslanClipPage *clip_page) {
  if (!ctx || !clip_page)
    return;

  // Ensure SDL text input is active
  SDL_StartTextInput();

  const float row_h = 32.0f;
  const float gutter_w = 40.0f;
  const float filename_w = 180.0f;
  const float time_w = 80.0f;
  const float timeline_min_w = 200.0f;

  // Colors
  struct nk_color bg_normal = nk_rgb(40, 42, 46);
  struct nk_color bg_selected = nk_rgb(60, 80, 100);
  struct nk_color timeline_bg = nk_rgb(50, 52, 56);
  struct nk_color timeline_fill = nk_rgb(80, 140, 200);
  struct nk_color text_dim = nk_rgb(140, 140, 140);
  struct nk_color gutter_color = nk_rgb(60, 100, 180);

  // Header row
  nk_layout_row_begin(ctx, NK_STATIC, row_h, 5);
  {
    nk_layout_row_push(ctx, gutter_w);
    nk_label(ctx, "#", NK_TEXT_CENTERED);

    nk_layout_row_push(ctx, filename_w);
    nk_label(ctx, "Source", NK_TEXT_LEFT);

    nk_layout_row_push(ctx, time_w);
    nk_label(ctx, "IN", NK_TEXT_CENTERED);

    nk_layout_row_push(ctx, timeline_min_w);
    nk_label(ctx, "Timeline", NK_TEXT_CENTERED);

    nk_layout_row_push(ctx, time_w);
    nk_label(ctx, "OUT", NK_TEXT_CENTERED);
  }
  nk_layout_row_end(ctx);

  // Separator
  nk_layout_row_dynamic(ctx, 2, 1);
  {
    struct nk_rect sep_bounds = nk_widget_bounds(ctx);
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    nk_fill_rect(canvas, sep_bounds, 0, nk_rgb(70, 70, 70));
    nk_spacing(ctx, 1);
  }

  // Track navigation requests
  int nav_dir = 0; // -1 = up, +1 = down
  int click_index = -1;

  // Handle keyboard navigation
  if (nk_input_is_key_pressed(&ctx->input, NK_KEY_UP))
    nav_dir = -1;
  if (nk_input_is_key_pressed(&ctx->input, NK_KEY_DOWN))
    nav_dir = 1;

  // Render each clip as a row
  for (int i = 0; i < clip_page->clip_count; i++) {
    AslanClip *clip = &clip_page->clips[i];
    bool is_active = (i == clip_page->active_index);

    // Row background color
    struct nk_color row_bg = is_active ? bg_selected : bg_normal;

    nk_layout_row_begin(ctx, NK_STATIC, row_h, 5);
    {
      // Gutter (line number)
      nk_layout_row_push(ctx, gutter_w);
      {
        struct nk_rect gutter_bounds = nk_widget_bounds(ctx);
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        nk_fill_rect(canvas, gutter_bounds, 0, gutter_color);

        char line_num[16];
        snprintf(line_num, sizeof(line_num), "%d", i + 1);
        nk_label_colored(ctx, line_num, NK_TEXT_CENTERED,
                         nk_rgb(255, 255, 255));
      }

      // Filename
      nk_layout_row_push(ctx, filename_w);
      {
        struct nk_rect cell_bounds = nk_widget_bounds(ctx);
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        nk_fill_rect(canvas, cell_bounds, 0, row_bg);

        const char *fname = get_filename(clip->source_path);

        // Click detection for this cell
        if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                            cell_bounds)) {
          click_index = i;
        }

        // If editing this field
        if (is_active && clip_page->editing_field == 1) {
          char buf[256];
          strncpy(buf, clip->source_path, sizeof(buf) - 1);
          buf[sizeof(buf) - 1] = '\0';

          nk_flags edit_ret = nk_edit_string_zero_terminated(
              ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER | NK_EDIT_AUTO_SELECT, buf,
              sizeof(buf) - 1, nk_filter_default);

          if (edit_ret & NK_EDIT_COMMITED) {
            // Save change
            if (engine) {
              char cmd[512];
              snprintf(cmd, sizeof(cmd), "clip_set_source %u %d \"%s\"",
                       (unsigned)page_id, i, buf);
              engine_execute_command(engine, cmd);
            }
            clip_page->editing_field = 0;
          }
        } else {
          nk_label_colored(ctx, fname, NK_TEXT_LEFT, nk_rgb(220, 220, 220));
        }
      }

      // IN time
      nk_layout_row_push(ctx, time_w);
      {
        struct nk_rect cell_bounds = nk_widget_bounds(ctx);
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        nk_fill_rect(canvas, cell_bounds, 0, row_bg);

        char time_str[32];
        format_time(clip->start_time, time_str, sizeof(time_str));

        if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                            cell_bounds)) {
          click_index = i;
          if (is_active)
            clip_page->editing_field = 2;
        }

        if (is_active && clip_page->editing_field == 2) {
          char buf[32];
          strncpy(buf, time_str, sizeof(buf) - 1);
          buf[sizeof(buf) - 1] = '\0';

          nk_flags edit_ret = nk_edit_string_zero_terminated(
              ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER | NK_EDIT_AUTO_SELECT, buf,
              sizeof(buf) - 1, nk_filter_default);

          if (edit_ret & NK_EDIT_COMMITED) {
            double new_time = parse_time(buf);
            if (engine) {
              char cmd[256];
              snprintf(cmd, sizeof(cmd), "clip_set_range %u %d %f %f",
                       (unsigned)page_id, i, new_time, clip->end_time);
              engine_execute_command(engine, cmd);
            }
            clip_page->editing_field = 0;
          }
        } else {
          nk_label_colored(ctx, time_str, NK_TEXT_CENTERED, text_dim);
        }
      }

      // Timeline bar
      nk_layout_row_push(ctx, timeline_min_w);
      {
        struct nk_rect bar_bounds = nk_widget_bounds(ctx);
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

        // Background
        nk_fill_rect(canvas, bar_bounds, 4, timeline_bg);

        // Calculate fill ratio (assuming max clip duration display of 60s for
        // visualization)
        double duration = clip->end_time - clip->start_time;
        if (duration < 0)
          duration = 0;
        float fill_ratio = (float)(duration / 60.0);
        if (fill_ratio > 1.0f)
          fill_ratio = 1.0f;

        // Fill bar
        struct nk_rect fill_rect = bar_bounds;
        fill_rect.w = bar_bounds.w * fill_ratio;
        fill_rect.x += 2;
        fill_rect.y += 2;
        fill_rect.w -= 4;
        fill_rect.h -= 4;
        if (fill_rect.w > 0) {
          nk_fill_rect(canvas, fill_rect, 2,
                       is_active ? timeline_fill : nk_rgb(60, 100, 140));
        }

        // Duration text overlay
        char dur_str[32];
        snprintf(dur_str, sizeof(dur_str), "%.1fs", duration);
        nk_label_colored(ctx, dur_str, NK_TEXT_CENTERED, nk_rgb(200, 200, 200));

        if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                            bar_bounds)) {
          click_index = i;
        }
      }

      // OUT time
      nk_layout_row_push(ctx, time_w);
      {
        struct nk_rect cell_bounds = nk_widget_bounds(ctx);
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        nk_fill_rect(canvas, cell_bounds, 0, row_bg);

        char time_str[32];
        format_time(clip->end_time, time_str, sizeof(time_str));

        if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT,
                                            cell_bounds)) {
          click_index = i;
          if (is_active)
            clip_page->editing_field = 3;
        }

        if (is_active && clip_page->editing_field == 3) {
          char buf[32];
          strncpy(buf, time_str, sizeof(buf) - 1);
          buf[sizeof(buf) - 1] = '\0';

          nk_flags edit_ret = nk_edit_string_zero_terminated(
              ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER | NK_EDIT_AUTO_SELECT, buf,
              sizeof(buf) - 1, nk_filter_default);

          if (edit_ret & NK_EDIT_COMMITED) {
            double new_time = parse_time(buf);
            if (engine) {
              char cmd[256];
              snprintf(cmd, sizeof(cmd), "clip_set_range %u %d %f %f",
                       (unsigned)page_id, i, clip->start_time, new_time);
              engine_execute_command(engine, cmd);
            }
            clip_page->editing_field = 0;
          }
        } else {
          nk_label_colored(ctx, time_str, NK_TEXT_CENTERED, text_dim);
        }
      }
    }
    nk_layout_row_end(ctx);

    // Comment line (if any)
    if (clip->comment[0] != '\0' ||
        (is_active && clip_page->editing_field == 4)) {
      nk_layout_row_begin(ctx, NK_STATIC, row_h - 6, 2);
      {
        nk_layout_row_push(ctx, gutter_w);
        nk_spacing(ctx, 1);

        nk_layout_row_push(ctx, filename_w + time_w + timeline_min_w + time_w);

        if (is_active && clip_page->editing_field == 4) {
          char buf[256];
          strncpy(buf, clip->comment, sizeof(buf) - 1);
          buf[sizeof(buf) - 1] = '\0';

          nk_flags edit_ret = nk_edit_string_zero_terminated(
              ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, buf, sizeof(buf) - 1,
              nk_filter_default);

          if (edit_ret & NK_EDIT_COMMITED) {
            if (engine) {
              char escaped[512];
              int j = 0;
              for (int k = 0; buf[k] && j < (int)sizeof(escaped) - 2; k++) {
                if (buf[k] == '"')
                  escaped[j++] = '\\';
                escaped[j++] = buf[k];
              }
              escaped[j] = '\0';

              char cmd[768];
              snprintf(cmd, sizeof(cmd), "clip_set_comment %u %d \"%s\"",
                       (unsigned)page_id, i, escaped);
              engine_execute_command(engine, cmd);
            }
            clip_page->editing_field = 0;
          }
        } else {
          char comment_display[280];
          snprintf(comment_display, sizeof(comment_display), "  -- %s",
                   clip->comment);
          nk_label_colored(ctx, comment_display, NK_TEXT_LEFT, text_dim);
        }
      }
      nk_layout_row_end(ctx);
    }
  }

  // Handle click to select
  if (click_index >= 0 && click_index < clip_page->clip_count) {
    if (clip_page->active_index != click_index) {
      clip_page->active_index = click_index;
      clip_page->editing_field = 0;
    }
  }

  // Apply keyboard navigation
  if (nav_dir != 0 && clip_page->clip_count > 0) {
    clip_page->editing_field = 0;
    int new_idx = clip_page->active_index + nav_dir;
    if (new_idx < 0)
      new_idx = 0;
    if (new_idx >= clip_page->clip_count)
      new_idx = clip_page->clip_count - 1;
    clip_page->active_index = new_idx;
  }

  // Toolbar / Actions
  nk_layout_row_dynamic(ctx, 8, 1);
  nk_spacing(ctx, 1);

  nk_layout_row_dynamic(ctx, 28, 5);
  {
    if (nk_button_label(ctx, "+ Add Clip")) {
      if (engine) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "clip_add %u \"\"", (unsigned)page_id);
        engine_execute_command(engine, cmd);
        // Select the new clip
        clip_page->active_index = clip_page->clip_count - 1;
      }
    }

    if (nk_button_label(ctx, "Delete")) {
      if (engine && clip_page->active_index >= 0) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "clip_del %u %d", (unsigned)page_id,
                 clip_page->active_index);
        engine_execute_command(engine, cmd);
      }
    }

    if (nk_button_label(ctx, "Move Up")) {
      if (engine && clip_page->active_index > 0) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "clip_move %u %d %d", (unsigned)page_id,
                 clip_page->active_index, clip_page->active_index - 1);
        engine_execute_command(engine, cmd);
      }
    }

    if (nk_button_label(ctx, "Move Down")) {
      if (engine && clip_page->active_index >= 0 &&
          clip_page->active_index < clip_page->clip_count - 1) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "clip_move %u %d %d", (unsigned)page_id,
                 clip_page->active_index, clip_page->active_index + 1);
        engine_execute_command(engine, cmd);
      }
    }

    if (nk_button_label(ctx, "Split (S)")) {
      if (engine && clip_page->active_index >= 0) {
        AslanClip *clip = &clip_page->clips[clip_page->active_index];
        double mid = (clip->start_time + clip->end_time) / 2.0;
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "clip_split %u %d %f", (unsigned)page_id,
                 clip_page->active_index, mid);
        engine_execute_command(engine, cmd);
      }
    }
  }

  // Status bar
  nk_layout_row_dynamic(ctx, 4, 1);
  nk_spacing(ctx, 1);

  nk_layout_row_dynamic(ctx, 20, 1);
  {
    double total = clip_page_total_duration(clip_page);
    char status[128];
    snprintf(status, sizeof(status), "Clips: %d | Total Duration: %.1fs",
             clip_page->clip_count, total);
    nk_label_colored(ctx, status, NK_TEXT_LEFT, text_dim);
  }

  // Handle 'S' key for split
  if (ctx->input.keyboard.text_len > 0) {
    for (int i = 0; i < ctx->input.keyboard.text_len; i++) {
      if (ctx->input.keyboard.text[i] == 's' ||
          ctx->input.keyboard.text[i] == 'S') {
        if (clip_page->editing_field == 0 && engine &&
            clip_page->active_index >= 0) {
          AslanClip *clip = &clip_page->clips[clip_page->active_index];
          double mid = (clip->start_time + clip->end_time) / 2.0;
          char cmd[64];
          snprintf(cmd, sizeof(cmd), "clip_split %u %d %f", (unsigned)page_id,
                   clip_page->active_index, mid);
          engine_execute_command(engine, cmd);
        }
      }
    }
  }

  // Handle Delete/Backspace key
  if (nk_input_is_key_pressed(&ctx->input, NK_KEY_DEL) ||
      (clip_page->editing_field == 0 &&
       nk_input_is_key_pressed(&ctx->input, NK_KEY_BACKSPACE))) {
    if (engine && clip_page->active_index >= 0) {
      char cmd[64];
      snprintf(cmd, sizeof(cmd), "clip_del %u %d", (unsigned)page_id,
               clip_page->active_index);
      engine_execute_command(engine, cmd);
    }
  }

  // Handle Enter to start editing filename
  if (clip_page->editing_field == 0 &&
      nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
    clip_page->editing_field = 1; // Start editing filename
  }

  // Handle Tab to cycle through fields
  if (nk_input_is_key_pressed(&ctx->input, NK_KEY_TAB)) {
    clip_page->editing_field = (clip_page->editing_field + 1) % 5;
  }

  // Handle Escape to cancel editing
  if (nk_input_is_key_pressed(&ctx->input, NK_KEY_TEXT_RESET_MODE)) {
    clip_page->editing_field = 0;
  }
}

static void ui_apply_theme_dark(struct nk_context *ctx) {
  if (!ctx)
    return;
  struct nk_color table[NK_COLOR_COUNT];

  // Loosely inspired by Nuklear demo themes.
  table[NK_COLOR_TEXT] = nk_rgb(230, 230, 230);
  table[NK_COLOR_WINDOW] = nk_rgb(30, 33, 36);
  table[NK_COLOR_HEADER] = nk_rgb(45, 48, 52);
  table[NK_COLOR_BORDER] = nk_rgb(65, 65, 65);
  table[NK_COLOR_BUTTON] = nk_rgb(60, 60, 60);
  table[NK_COLOR_BUTTON_HOVER] = nk_rgb(70, 70, 70);
  table[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(50, 50, 50);
  table[NK_COLOR_TOGGLE] = nk_rgb(50, 50, 50);
  table[NK_COLOR_TOGGLE_HOVER] = nk_rgb(60, 60, 60);
  table[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(230, 230, 230);
  table[NK_COLOR_SELECT] = nk_rgb(50, 55, 60);
  table[NK_COLOR_SELECT_ACTIVE] = nk_rgb(65, 70, 78);
  table[NK_COLOR_SLIDER] = nk_rgb(60, 60, 60);
  table[NK_COLOR_SLIDER_CURSOR] = nk_rgb(95, 95, 95);
  table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(110, 110, 110);
  table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(85, 85, 85);
  table[NK_COLOR_PROPERTY] = nk_rgb(60, 60, 60);
  table[NK_COLOR_EDIT] = nk_rgb(45, 45, 45);
  table[NK_COLOR_EDIT_CURSOR] = nk_rgb(230, 230, 230);
  table[NK_COLOR_COMBO] = nk_rgb(45, 45, 45);
  table[NK_COLOR_CHART] = nk_rgb(45, 45, 45);
  table[NK_COLOR_CHART_COLOR] = nk_rgb(164, 164, 164);
  table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(255, 0, 0);
  table[NK_COLOR_SCROLLBAR] = nk_rgb(45, 45, 45);
  table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(70, 70, 70);
  table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(85, 85, 85);
  table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(60, 60, 60);
  table[NK_COLOR_TAB_HEADER] = nk_rgb(45, 48, 52);

  nk_style_from_table(ctx, table);
}

static void ui_apply_theme_light(struct nk_context *ctx) {
  if (!ctx)
    return;
  struct nk_color table[NK_COLOR_COUNT];

  table[NK_COLOR_TEXT] = nk_rgb(20, 20, 20);
  table[NK_COLOR_WINDOW] = nk_rgb(245, 245, 245);
  table[NK_COLOR_HEADER] = nk_rgb(235, 235, 235);
  table[NK_COLOR_BORDER] = nk_rgb(180, 180, 180);
  table[NK_COLOR_BUTTON] = nk_rgb(225, 225, 225);
  table[NK_COLOR_BUTTON_HOVER] = nk_rgb(210, 210, 210);
  table[NK_COLOR_BUTTON_ACTIVE] = nk_rgb(200, 200, 200);
  table[NK_COLOR_TOGGLE] = nk_rgb(220, 220, 220);
  table[NK_COLOR_TOGGLE_HOVER] = nk_rgb(205, 205, 205);
  table[NK_COLOR_TOGGLE_CURSOR] = nk_rgb(35, 35, 35);
  table[NK_COLOR_SELECT] = nk_rgb(205, 205, 205);
  table[NK_COLOR_SELECT_ACTIVE] = nk_rgb(190, 190, 190);
  table[NK_COLOR_SLIDER] = nk_rgb(220, 220, 220);
  table[NK_COLOR_SLIDER_CURSOR] = nk_rgb(160, 160, 160);
  table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgb(145, 145, 145);
  table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgb(130, 130, 130);
  table[NK_COLOR_PROPERTY] = nk_rgb(220, 220, 220);
  table[NK_COLOR_EDIT] = nk_rgb(235, 235, 235);
  table[NK_COLOR_EDIT_CURSOR] = nk_rgb(20, 20, 20);
  table[NK_COLOR_COMBO] = nk_rgb(235, 235, 235);
  table[NK_COLOR_CHART] = nk_rgb(230, 230, 230);
  table[NK_COLOR_CHART_COLOR] = nk_rgb(60, 60, 60);
  table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgb(255, 0, 0);
  table[NK_COLOR_SCROLLBAR] = nk_rgb(235, 235, 235);
  table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgb(200, 200, 200);
  table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgb(185, 185, 185);
  table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgb(170, 170, 170);
  table[NK_COLOR_TAB_HEADER] = nk_rgb(235, 235, 235);

  nk_style_from_table(ctx, table);
}

// Implémentation spécifique SDL2 pour Nuklear
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#define NK_SDL_RENDERER_SDL_H <SDL.h>
#include "../../include/nuklear.h"
#include "nuklear_sdl_renderer.h"

UIHost *ui_init(SDL_Window *win, SDL_Renderer *renderer, lua_State *L) {
  UIHost *ui = (UIHost *)calloc(1, sizeof(UIHost));

  ui->L = L;
  // Initialisation de Nuklear avec le renderer SDL2
  ui->ctx = nk_sdl_init(win, renderer);

  // Fonts
  ui->font_inter = NULL;
  ui->font_mono = NULL;
  ui->font_h1 = NULL;
  ui->font_h2 = NULL;
  ui->font_h3 = NULL;
  ui->font_icons = NULL;
  ui->font_size_px = 14.0f;

  // Load fonts into Nuklear atlas.
  // We load Inter from disk (assets/fonts/...) to keep binaries out of git.
  struct nk_font_atlas *atlas;
  nk_sdl_font_stash_begin(&atlas);
  if (atlas) {
    ui->font_inter = nk_font_atlas_add_from_file(
        atlas, "assets/fonts/Inter-Regular.ttf", ui->font_size_px, 0);
    // NOTE editor monospace font (ships in repo under assets/).
    ui->font_mono = nk_font_atlas_add_from_file(
        atlas, "assets/fonts/JetBrains_Mono/static/JetBrainsMono-Regular.ttf",
        ui->font_size_px, 0);

    // Remix Icon font (icons).
    // NOTE: Keep size close to UI font size so icons align with text.
    {
      // RemixIcon uses Unicode Private Use Area.
      // Without a rune range, Nuklear may skip baking those glyphs -> missing
      // icons.
      static const nk_rune icon_range[] = {0xE000, 0xF8FF, 0};
      struct nk_font_config icon_cfg = nk_font_config(0);
      icon_cfg.range = icon_range;
      // Make icons a bit larger for callouts.
      ui->font_icons = nk_font_atlas_add_from_file(
          atlas, "assets/remixicon.ttf", 20.0f, &icon_cfg);
    }

    // Heading fonts for NOTE blocks (JetBrains Mono in different weights)
    ui->font_h3 = nk_font_atlas_add_from_file(
        atlas, "assets/fonts/JetBrains_Mono/static/JetBrainsMono-Medium.ttf",
        18.0f, 0);
    ui->font_h2 = nk_font_atlas_add_from_file(
        atlas, "assets/fonts/JetBrains_Mono/static/JetBrainsMono-SemiBold.ttf",
        24.0f, 0);
    ui->font_h1 = nk_font_atlas_add_from_file(
        atlas, "assets/fonts/JetBrains_Mono/static/JetBrainsMono-Bold.ttf",
        36.0f, 0);
  }
  nk_sdl_font_stash_end();

  // Apply Inter as UI default when available.
  if (ui->font_inter) {
    nk_style_set_font(ui->ctx, &ui->font_inter->handle);
  }

  snprintf(ui->status, sizeof(ui->status), "Ready");
  ui->tool_mode = 0;

  // Default modality: Select.
  // Keep Lua in sync so scripts see a known initial state.
  if (ui->L) {
    lua_getglobal(ui->L, "aslan");
    if (lua_istable(ui->L, -1)) {
      lua_getfield(ui->L, -1, "set_tool_mode");
      if (lua_isfunction(ui->L, -1)) {
        lua_pushinteger(ui->L, ui->tool_mode);
        if (lua_pcall(ui->L, 1, 0, 0) != LUA_OK) {
          lua_pop(ui->L, 1);
        }
      } else {
        lua_pop(ui->L, 1);
      }
      lua_pop(ui->L, 1);
    } else {
      lua_pop(ui->L, 1);
    }
  }

  // Export defaults
  snprintf(ui->export_svg_path, sizeof(ui->export_svg_path), "preview.svg");
  ui->show_export_svg_modal = 0;
  if (getcwd(ui->export_svg_dir, sizeof(ui->export_svg_dir)) == NULL) {
    snprintf(ui->export_svg_dir, sizeof(ui->export_svg_dir), ".");
  }
  snprintf(ui->export_svg_name, sizeof(ui->export_svg_name), "preview.svg");

  // Save As .aslan defaults
  ui->show_save_aslan_modal = 0;
  if (getcwd(ui->save_aslan_dir, sizeof(ui->save_aslan_dir)) == NULL) {
    snprintf(ui->save_aslan_dir, sizeof(ui->save_aslan_dir), ".");
  }
  snprintf(ui->save_aslan_name, sizeof(ui->save_aslan_name), "document.aslan");

  // Globals editor defaults
  ui->global_key[0] = 0;
  ui->global_value[0] = 0;

  // New Page modal defaults
  ui->show_new_page_modal = 0;
  ui->new_page_type_idx = 0;    // Space
  ui->new_page_preset_idx = 0;  // A4
  ui->new_page_orientation = 0; // Portrait
  ui->new_page_width_mm = 210.0f;
  ui->new_page_height_mm = 297.0f;
  snprintf(ui->new_page_name, sizeof(ui->new_page_name), "New Page");

  // Snap/Grid step editor defaults
  ui->snap_step_edit = 10;
  ui->grid_step_edit = 10;
  ui->snap_grid_steps_inited = 0;

  // Home defaults
  ui->screen = 0; // Home
  ui->open_aslan_path[0] = 0;

  // Recents storage: simple local file in the working directory for now.
  // (Later we can move it to an app data dir.)
  snprintf(ui->recents_store_path, sizeof(ui->recents_store_path),
           "recents.txt");
  ui->recents_count = 0;
  for (int i = 0; i < 16; i++)
    ui->recents_paths[i][0] = 0;

  // Load recents from disk
  {
    Recents r;
    recents_load(&r, ui->recents_store_path);
    ui->recents_count = r.count;
    for (int i = 0; i < r.count && i < 16; i++) {
      snprintf(ui->recents_paths[i], sizeof(ui->recents_paths[i]), "%s",
               r.paths[i]);
    }
  }

  return ui;
}

static void ui_recents_add_and_persist(UIHost *ui, const char *path) {
  if (!ui || !path || !path[0])
    return;
  Recents r;
  recents_load(&r, ui->recents_store_path);
  recents_add(&r, path);
  recents_save(&r, ui->recents_store_path);

  ui->recents_count = r.count;
  for (int i = 0; i < 16; i++)
    ui->recents_paths[i][0] = 0;
  for (int i = 0; i < r.count && i < 16; i++) {
    snprintf(ui->recents_paths[i], sizeof(ui->recents_paths[i]), "%s",
             r.paths[i]);
  }
}

static void reveal_file_in_finder_best_effort(const char *path) {
  if (!path || !path[0])
    return;
#if defined(__APPLE__)
  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "open -R '%s' >/dev/null 2>&1", path);
  system(cmd);
#else
  (void)path;
#endif
}

static int pick_file_mac_aslan(char *out_path, size_t cap) {
  if (!out_path || cap == 0)
    return 0;
  out_path[0] = 0;

#if defined(__APPLE__)
  // Use AppleScript to open a file chooser.
  // Returns a POSIX path (stdout). User cancel -> empty.
  const char *cmd = "osascript -e 'try' "
                    "-e 'set f to choose file with prompt \"Open .aslan\" of "
                    "type {\"public.data\"}' "
                    "-e 'POSIX path of f' "
                    "-e 'on error number -128' "
                    "-e '' "
                    "-e 'end try'";

  FILE *p = popen(cmd, "r");
  if (!p)
    return 0;

  if (!fgets(out_path, (int)cap, p)) {
    pclose(p);
    out_path[0] = 0;
    return 0;
  }
  pclose(p);

  // Trim \n/\r
  size_t n = strlen(out_path);
  while (n > 0 && (out_path[n - 1] == '\n' || out_path[n - 1] == '\r')) {
    out_path[n - 1] = 0;
    n--;
  }
  return out_path[0] != 0;
#else
  (void)out_path;
  (void)cap;
  return 0;
#endif
}

static int mkdir_p(const char *dir) {
  if (!dir || !dir[0])
    return 0;
  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s", dir);

  size_t len = strlen(tmp);
  if (len == 0)
    return 0;
  if (tmp[len - 1] == '/')
    tmp[len - 1] = 0;

  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  mkdir(tmp, 0755);
  return 0;
}

static void join_path(char *out, size_t cap, const char *dir,
                      const char *name) {
  if (!out || cap == 0)
    return;
  out[0] = 0;
  if (!name || !name[0])
    return;
  if (!dir || !dir[0]) {
    snprintf(out, cap, "%s", name);
    return;
  }
  size_t dlen = strlen(dir);
  if (dir[dlen - 1] == '/')
    snprintf(out, cap, "%s%s", dir, name);
  else
    snprintf(out, cap, "%s/%s", dir, name);
}

void ui_shutdown(UIHost *ui) {
  nk_sdl_shutdown();
  free(ui);
}

void ui_handle_event(UIHost *ui, SDL_Event *evt) {
  (void)ui;
  nk_sdl_handle_event(evt);
}

// Helper functions for dynamic line layout in NOTE pages
static float get_line_height(AslanRowType type) {
  switch (type) {
  case ASLAN_ROW_HEADING_1:
    return 56.0f; // Larger, more prominent headings
  case ASLAN_ROW_SEPARATOR:
    return 32.0f; // More breathing room
  case ASLAN_ROW_GRID:
    return 50.0f; // Simplified placeholder (reduced from 120px)
  case ASLAN_ROW_EMBED:
    return 50.0f; // Simplified placeholder (reduced from 320px)
  case ASLAN_ROW_HIGHLIGHT:
    return 26.0f; // Same as regular text
  default:
    return 26.0f; // Regular text
  }
}

static float get_line_y_position(AslanNoteDoc *note, int target_row) {
  float y_pos = 0.0f;
  for (int i = 0; i < target_row && i < note->row_count; i++) {
    y_pos += get_line_height(note->rows[i].type);
  }
  return y_pos;
}

static const struct nk_user_font *
note_row_font(const UIHost *ui, const struct nk_user_font *fallback,
              AslanRowType type) {
  if (!ui)
    return fallback;
  switch (type) {
  case ASLAN_ROW_HEADING_1:
    return (ui->font_h1 ? &ui->font_h1->handle : fallback);
  default:
    return fallback;
  }
}

static float note_measure_char_w(const struct nk_user_font *font) {
  float char_w = 8.0f;
  if (font && font->width) {
    const char *cell = "M";
    char_w = font->width(font->userdata, font->height, cell, 1);
    if (char_w < 1.0f)
      char_w = 8.0f;
  }
  return char_w;
}

void ui_render(UIHost *ui, Engine *engine, int win_w, int win_h) {
  // Track selection changes so we can reset inspector edit buffers (e.g., text)
  static uint32_t last_selected_id = 0;
  uint32_t cur_selected_id =
      (ui && ui->selected_node) ? ui->selected_node->id : 0;
  if (cur_selected_id != last_selected_id) {
    ui->text_edit_inited = 0;
    last_selected_id = cur_selected_id;
  }

  // Theme for Inspector UI only (read from globals)
  const char *theme = NULL;
  if (engine && engine->doc) {
    theme = doc_get_global(engine->doc, "theme");
  }
  if (theme && ((uintptr_t)theme > 0x1000) && strcmp(theme, "dark") == 0)
    ui_apply_theme_dark(ui->ctx);
  else
    ui_apply_theme_light(ui->ctx);

  // Home screen (Antirez-style: a focused, simple hub)
  if (ui->screen == 0) {
    float w = 560.0f;
    float h = 520.0f;
    struct nk_rect bounds =
        nk_rect(win_w * 0.5f - w * 0.5f, win_h * 0.5f - h * 0.5f, w, h);

    if (nk_begin(ui->ctx, "Aslan Home", bounds,
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR)) {

      nk_layout_row_dynamic(ui->ctx, 24, 1);
      nk_label(ui->ctx, "Home", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ui->ctx, 30, 2);
      if (nk_button_label(ui->ctx, "New document")) {
        // True new document: reset state/history, then let the user pick the
        // first page.
        if (engine)
          engine_execute_command(engine, "clear");
        ui->selected_node = NULL;
        ui->editing_node_id = 0;
        ui->inline_edit_inited = 0;
        ui->inline_edit_dirty = 0;
        ui->text_edit_inited = 0;

        // Show the New Page modal so the user chooses what to create.
        // Do not create a blank/"No active page" state.
        ui->show_new_page_modal = 1;
        // Reset defaults
        ui->new_page_type_idx = 0;    // Space
        ui->new_page_preset_idx = 0;  // A4
        ui->new_page_orientation = 0; // Portrait
        ui->new_page_width_mm = 210.0f;
        ui->new_page_height_mm = 297.0f;
        // First page of a new doc
        snprintf(ui->new_page_name, sizeof(ui->new_page_name), "Page 1");

        snprintf(ui->status, sizeof(ui->status), "Choose page type");
      }
      if (nk_button_label(ui->ctx, "Open...")) {
        char picked[512];
        if (pick_file_mac_aslan(picked, sizeof(picked))) {
          snprintf(ui->open_aslan_path, sizeof(ui->open_aslan_path), "%s",
                   picked);
          if (engine_load_snapshot_from_aslan(engine, picked)) {
            ui_recents_add_and_persist(ui, picked);
            ui->selected_node = NULL;
            ui->screen = 1;
            snprintf(ui->status, sizeof(ui->status), "Opened: %s", picked);
          } else {
            snprintf(ui->status, sizeof(ui->status), "Open failed: %s", picked);
          }
        } else {
          // User cancelled or not supported.
          snprintf(ui->status, sizeof(ui->status), "Open cancelled");
        }
      }

      nk_layout_row_dynamic(ui->ctx, 10, 1);
      nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Recent creations", NK_TEXT_LEFT);

      if (ui->recents_count <= 0) {
        nk_layout_row_dynamic(ui->ctx, 20, 1);
        nk_label(ui->ctx, "(No recent files yet)", NK_TEXT_LEFT);
      } else {
        for (int i = 0; i < ui->recents_count && i < 16; i++) {
          if (!ui->recents_paths[i][0])
            continue;
          nk_layout_row_dynamic(ui->ctx, 26, 1);
          if (nk_button_label(ui->ctx, ui->recents_paths[i])) {
            snprintf(ui->open_aslan_path, sizeof(ui->open_aslan_path), "%s",
                     ui->recents_paths[i]);
            if (engine_load_snapshot_from_aslan(engine, ui->recents_paths[i])) {
              ui_recents_add_and_persist(ui, ui->recents_paths[i]);
              ui->selected_node = NULL;
              ui->screen = 1;
              snprintf(ui->status, sizeof(ui->status), "Opened: %s",
                       ui->recents_paths[i]);
            } else {
              snprintf(ui->status, sizeof(ui->status), "Open failed: %s",
                       ui->recents_paths[i]);
            }
          }
        }
      }

      nk_layout_row_dynamic(ui->ctx, 10, 1);
      nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Templates", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "(Coming soon)", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ui->ctx, 10, 1);
      nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Settings", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "(Coming soon)", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ui->ctx, 10, 1);
      nk_label(ui->ctx, "---", NK_TEXT_CENTERED);

      nk_layout_row_dynamic(ui->ctx, 18, 1);
      nk_label(ui->ctx, ui->status, NK_TEXT_LEFT);
    }
    nk_end(ui->ctx);

    // Modal New Page window (also available on Home)
    if (ui->show_new_page_modal) {
      struct nk_rect bounds =
          nk_rect(win_w * 0.5f - 200.0f, win_h * 0.5f - 200.0f, 400.0f, 400.0f);
      if (nk_begin(ui->ctx, "New Page", bounds,
                   NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE |
                       NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(ui->ctx, 20, 1);
        nk_label(ui->ctx, "Page Name:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ui->ctx, 28, 1);
        nk_edit_string_zero_terminated(
            ui->ctx, NK_EDIT_FIELD, ui->new_page_name,
            (int)sizeof(ui->new_page_name) - 1, nk_filter_default);

        nk_layout_row_dynamic(ui->ctx, 20, 1);
        nk_label(ui->ctx, "Type:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ui->ctx, 28, 2);
        if (nk_option_label(ui->ctx, "Space (Infinite)",
                            ui->new_page_type_idx == 0))
          ui->new_page_type_idx = 0;
        if (nk_option_label(ui->ctx, "Page (Fixed)",
                            ui->new_page_type_idx == 1))
          ui->new_page_type_idx = 1;

        nk_layout_row_dynamic(ui->ctx, 28, 3);
        if (nk_option_label(ui->ctx, "Note (TBD)", ui->new_page_type_idx == 2))
          ui->new_page_type_idx = 2;
        if (nk_option_label(ui->ctx, "Clip (TBD)", ui->new_page_type_idx == 3))
          ui->new_page_type_idx = 3;
        if (nk_option_label(ui->ctx, "Grid (Table)",
                            ui->new_page_type_idx == 4))
          ui->new_page_type_idx = 4;

        if (ui->new_page_type_idx == 1) { // Page options
          nk_layout_row_dynamic(ui->ctx, 20, 1);
          nk_label(ui->ctx, "Preset:", NK_TEXT_LEFT);
          nk_layout_row_dynamic(ui->ctx, 28, 4);
          if (nk_option_label(ui->ctx, "A4", ui->new_page_preset_idx == 0)) {
            ui->new_page_preset_idx = 0;
            ui->new_page_width_mm = 210.0f;
            ui->new_page_height_mm = 297.0f;
          }
          if (nk_option_label(ui->ctx, "A3", ui->new_page_preset_idx == 1)) {
            ui->new_page_preset_idx = 1;
            ui->new_page_width_mm = 297.0f;
            ui->new_page_height_mm = 420.0f;
          }
          if (nk_option_label(ui->ctx, "Letter",
                              ui->new_page_preset_idx == 2)) {
            ui->new_page_preset_idx = 2;
            ui->new_page_width_mm = 215.9f;
            ui->new_page_height_mm = 279.4f;
          }
          if (nk_option_label(ui->ctx, "Custom", ui->new_page_preset_idx == 3))
            ui->new_page_preset_idx = 3;

          nk_layout_row_dynamic(ui->ctx, 20, 1);
          nk_label(ui->ctx, "Orientation:", NK_TEXT_LEFT);
          nk_layout_row_dynamic(ui->ctx, 28, 2);
          if (nk_option_label(ui->ctx, "Portrait",
                              ui->new_page_orientation == 0)) {
            if (ui->new_page_orientation != 0) {
              float tmp = ui->new_page_width_mm;
              ui->new_page_width_mm = ui->new_page_height_mm;
              ui->new_page_height_mm = tmp;
            }
            ui->new_page_orientation = 0;
          }
          if (nk_option_label(ui->ctx, "Landscape",
                              ui->new_page_orientation == 1)) {
            if (ui->new_page_orientation != 1) {
              float tmp = ui->new_page_width_mm;
              ui->new_page_width_mm = ui->new_page_height_mm;
              ui->new_page_height_mm = tmp;
            }
            ui->new_page_orientation = 1;
          }

          nk_layout_row_dynamic(ui->ctx, 20, 1);
          nk_label(ui->ctx, "Dimensions (mm):", NK_TEXT_LEFT);
          nk_layout_row_dynamic(ui->ctx, 28, 2);
          nk_property_float(ui->ctx, "W", 50.0f, &ui->new_page_width_mm,
                            2000.0f, 1.0f, 1.0f);
          nk_property_float(ui->ctx, "H", 50.0f, &ui->new_page_height_mm,
                            2000.0f, 1.0f, 1.0f);
        }

        nk_layout_row_dynamic(ui->ctx, 34, 2);
        if (nk_button_label(ui->ctx, "Cancel")) {
          ui->show_new_page_modal = 0;
        }
        if (nk_button_label(ui->ctx, "Create")) {
          // 1. Create page
          char cmd[128];
          snprintf(cmd, sizeof(cmd), "page_create \"%s\"",
                   ui->new_page_name[0] ? ui->new_page_name : "Page");
          engine_execute_command(engine, cmd);

          // Get the newly created page id (it's the active one)
          uint32_t pid =
              (engine && engine->doc) ? doc_page_active_id(engine->doc) : 0;

          // 2. Set type and meta
          if (pid && ui->new_page_type_idx == 1) { // Page
            snprintf(cmd, sizeof(cmd), "page_type %u page", (unsigned)pid);
            engine_execute_command(engine, cmd);
            snprintf(cmd, sizeof(cmd), "page_meta %u width %.2f", (unsigned)pid,
                     ui->new_page_width_mm);
            engine_execute_command(engine, cmd);
            snprintf(cmd, sizeof(cmd), "page_meta %u height %.2f",
                     (unsigned)pid, ui->new_page_height_mm);
            engine_execute_command(engine, cmd);
          } else if (pid && ui->new_page_type_idx == 0) { // Space
            snprintf(cmd, sizeof(cmd), "page_type %u space", (unsigned)pid);
            engine_execute_command(engine, cmd);
            // Default viewport
            snprintf(cmd, sizeof(cmd), "page_meta %u viewport.x 0",
                     (unsigned)pid);
            engine_execute_command(engine, cmd);
            snprintf(cmd, sizeof(cmd), "page_meta %u viewport.y 0",
                     (unsigned)pid);
            engine_execute_command(engine, cmd);
            snprintf(cmd, sizeof(cmd), "page_meta %u viewport.zoom 1.0",
                     (unsigned)pid);
            engine_execute_command(engine, cmd);
          } else if (pid) {
            const char *t = "space";
            if (ui->new_page_type_idx == 2)
              t = "note";
            else if (ui->new_page_type_idx == 3)
              t = "clip";
            else if (ui->new_page_type_idx == 4)
              t = "grid";
            snprintf(cmd, sizeof(cmd), "page_type %u %s", (unsigned)pid, t);
            engine_execute_command(engine, cmd);
          }

          ui->show_new_page_modal = 0;
          ui->screen = 1; // enter editor view
          ui->selected_node = NULL;
          snprintf(ui->status, sizeof(ui->status), "Created page: %s",
                   ui->new_page_name);
        }
      }
      nk_end(ui->ctx);
    }

    nk_sdl_render(NK_ANTI_ALIASING_ON);
    return;
  }

  // On synchronise la sélection de l'UI avec celle de l'engine si nécessaire
  // (L'engine est la source de vérité pour les commandes 'select')

  // Page content (center panel)
  // IMPORTANT: this panel sits on top of the canvas and will capture mouse
  // input. We only show it for GRID pages (spreadsheet UI). Other page types
  // should use the canvas directly like before.
  if (engine && engine->doc) {
    uint32_t active_id = engine->doc->active_page_id;
    int pid = -1;
    for (int i = 0; i < 32; i++) {
      if (engine->doc->pages[i].in_use &&
          engine->doc->pages[i].page_id == active_id) {
        pid = i;
        break;
      }
    }

    if (pid >= 0 && engine->doc->pages[pid].type == PAGE_TYPE_GRID) {
      const float left_pad = 10.0f;
      const float top_pad = 10.0f;
      const float right_panel_w = 250.0f;
      const float bottom_bar_h = 44.0f; // Matches Pages bar height
      struct nk_rect page_bounds = nk_rect(
          left_pad, top_pad, (float)win_w - right_panel_w - left_pad * 2.0f,
          (float)win_h - top_pad - bottom_bar_h - 10.0f);

      if (nk_begin(ui->ctx, "Page", page_bounds,
                   NK_WINDOW_BORDER | NK_WINDOW_TITLE |
                       NK_WINDOW_SCROLL_AUTO_HIDE)) {
        const char *pname = engine->doc->pages[pid].name;
        char header[160];
        snprintf(header, sizeof(header), "Grid: %s  (id #%u)",
                 pname ? pname : "(unnamed)",
                 (unsigned)engine->doc->pages[pid].page_id);
        nk_layout_row_dynamic(ui->ctx, 24, 1);
        nk_label(ui->ctx, header, NK_TEXT_LEFT);

        nk_layout_row_dynamic(ui->ctx, 8, 1);
        nk_label(ui->ctx, " ", NK_TEXT_LEFT);

        ui_render_grid_page(ui->ctx, engine,
                            (uint32_t)engine->doc->pages[pid].page_id,
                            engine->doc->pages[pid].grid);
      }
      nk_end(ui->ctx);
    }

    // CLIP page rendering
    if (pid >= 0 && engine->doc->pages[pid].type == PAGE_TYPE_CLIP) {
      const float left_pad = 10.0f;
      const float top_pad = 10.0f;
      const float right_panel_w = 250.0f;
      const float bottom_bar_h = 44.0f;
      struct nk_rect page_bounds = nk_rect(
          left_pad, top_pad, (float)win_w - right_panel_w - left_pad * 2.0f,
          (float)win_h - top_pad - bottom_bar_h - 10.0f);

      if (nk_begin(ui->ctx, "Page", page_bounds,
                   NK_WINDOW_BORDER | NK_WINDOW_TITLE |
                       NK_WINDOW_SCROLL_AUTO_HIDE)) {
        const char *pname = engine->doc->pages[pid].name;
        char header[160];
        snprintf(header, sizeof(header), "Clip: %s  (Video Sequence Editor)",
                 pname ? pname : "(unnamed)");
        nk_layout_row_dynamic(ui->ctx, 24, 1);
        nk_label(ui->ctx, header, NK_TEXT_LEFT);

        nk_layout_row_dynamic(ui->ctx, 8, 1);
        nk_spacing(ui->ctx, 1);

        // Ensure clip page exists
        if (!engine->doc->pages[pid].clip) {
          doc_page_set_type(engine->doc, engine->doc->pages[pid].page_id,
                            PAGE_TYPE_CLIP);
        }

        if (engine->doc->pages[pid].clip) {
          ui_render_clip_page(ui->ctx, engine,
                              (uint32_t)engine->doc->pages[pid].page_id,
                              engine->doc->pages[pid].clip);
        }
      }
      nk_end(ui->ctx);
    }
  }

  // ==========================================================================
  // SIDEBAR - Modern, clean design
  // ==========================================================================
  // Hide sidebar for NOTE pages - they should be distraction-free
  int is_note_page = 0;
  if (engine && engine->doc) {
    uint32_t apid = doc_page_active_id(engine->doc);
    for (int i = 0; i < 32; i++) {
      if (!engine->doc->pages[i].in_use)
        continue;
      if (engine->doc->pages[i].page_id != apid)
        continue;
      if (engine->doc->pages[i].type == PAGE_TYPE_NOTE)
        is_note_page = 1;
      break;
    }
  }

  // For NOTE pages: modern minimalist toolbar
  if (is_note_page) {
    ui->selected_node =
        NULL; // Force clear selection to avoid inspector "square"

    float bar_w = 320.0f;
    float bar_h = 44.0f;
    float bar_x = (float)win_w * 0.5f - bar_w * 0.5f;
    float bar_y = 12.0f;

    // Modern floating toolbar (minimalist - only essential buttons)
    if (nk_begin(ui->ctx, "##note_toolbar", nk_rect(bar_x, bar_y, bar_w, bar_h),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {

      nk_layout_row_dynamic(ui->ctx, 32, 4); // 4 buttons only

      if (nk_button_label(ui->ctx, "Home")) {
        ui->screen = 0;
      }
      if (nk_button_label(ui->ctx, "H1")) {
        engine_execute_command(engine, "note_insert_block h1");
        // CRITICAL: Force note to capture input for next few frames
        // This bypasses nk_item_is_any_active() check in main.c
        ui->note_force_input = 5; // 5 frames should be enough
        if (!SDL_IsTextInputActive()) {
          SDL_StartTextInput();
        }
      }
      if (nk_button_label(ui->ctx, "Sep")) {
        engine_execute_command(engine, "note_insert_block separator");
        ui->note_force_input = 5;
        if (!SDL_IsTextInputActive()) {
          SDL_StartTextInput();
        }
      }
      // Fluo button: Cycles through colors (Yellow → Blue → Green → Off)
      // Simple full-line highlighting (stable, works with existing documents)
      {
        // Define fluo colors for button
        struct nk_color fluo_btn_colors[4] = {
            nk_rgb(80, 80, 80),    // 0 = Off (gray)
            nk_rgb(255, 235, 100), // 1 = Yellow (warmer)
            nk_rgb(160, 220, 255), // 2 = Blue (softer)
            nk_rgb(180, 255, 180)  // 3 = Green (pastel)
        };

        // Style button with current color
        struct nk_style_button fluo_style = ui->ctx->style.button;
        int idx = ui->fluo_color_index;
        if (idx < 0 || idx > 3)
          idx = 0;

        if (idx > 0) {
          // Active color - show colored button
          fluo_style.normal = nk_style_item_color(fluo_btn_colors[idx]);
          fluo_style.hover = nk_style_item_color(nk_rgb(
              (fluo_btn_colors[idx].r + 20) > 255 ? 255
                                                  : fluo_btn_colors[idx].r + 20,
              (fluo_btn_colors[idx].g + 20) > 255 ? 255
                                                  : fluo_btn_colors[idx].g + 20,
              (fluo_btn_colors[idx].b + 20) > 255
                  ? 255
                  : fluo_btn_colors[idx].b + 20));
          fluo_style.active = fluo_style.normal;
          fluo_style.text_normal = nk_rgb(0, 0, 0);
          fluo_style.text_hover = nk_rgb(0, 0, 0);
          fluo_style.text_active = nk_rgb(0, 0, 0);
        }

        if (nk_button_label_styled(ui->ctx, &fluo_style, "Fluo")) {
          // Cycle to next color: 0 → 1 → 2 → 3 → 0
          ui->fluo_color_index = (ui->fluo_color_index + 1) % 4;

          // Apply color to current line or selected lines
          if (engine && engine->doc) {
            uint32_t pid = doc_page_active_id(engine->doc);
            for (int pi = 0; pi < 32; pi++) {
              if (!engine->doc->pages[pi].in_use)
                continue;
              if (engine->doc->pages[pi].page_id != pid)
                continue;
              if (engine->doc->pages[pi].type != PAGE_TYPE_NOTE)
                break;

              AslanNoteDoc *nd = engine->doc->pages[pi].note;
              if (nd && nd->rows) {
                // Check if there's a selection
                bool has_selection =
                    nd->is_selecting || (nd->anchor_row != nd->cursor_row);

                int start_row = nd->cursor_row;
                int end_row = nd->cursor_row;

                if (has_selection) {
                  start_row = nd->anchor_row;
                  end_row = nd->cursor_row;
                  if (start_row > end_row) {
                    int t = start_row;
                    start_row = end_row;
                    end_row = t;
                  }
                }

                // Clamp
                if (start_row < 0)
                  start_row = 0;
                if (end_row >= nd->row_count)
                  end_row = nd->row_count - 1;

                // Apply highlight color to each row
                for (int r = start_row; r <= end_row; r++) {
                  AslanRow *row = &nd->rows[r];
                  row->highlight_color = ui->fluo_color_index;
                }

                // Clear selection after applying
                nd->is_selecting = false;
              }
              break;
            }
          }
        }
      }

      // TABLE and EMBED buttons REMOVED - Not in MVP
      // Focus on simple, working features (Antirez philosophy)
    }
    nk_end(ui->ctx);
  } else {
    // Regular sidebar for canvas/other pages
    if (nk_begin(ui->ctx, "Tools", nk_rect(win_w - 220, 0, 220, win_h),
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {

      nk_layout_row_dynamic(ui->ctx, 16, 1);
      nk_spacing(ui->ctx, 1);

      // Home button
      nk_layout_row_dynamic(ui->ctx, 36, 1);
      if (nk_button_label(ui->ctx, "Home")) {
        ui->screen = 0;
        snprintf(ui->status, sizeof(ui->status), "Home");
      }

      nk_layout_row_dynamic(ui->ctx, 12, 1);
      nk_spacing(ui->ctx, 1);

      // Section: Actions
      nk_layout_row_dynamic(ui->ctx, 18, 1);
      nk_label(ui->ctx, "Actions", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ui->ctx, 32, 2);
      if (nk_button_label(ui->ctx, "Undo")) {
        engine_execute_command(engine, "undo");
      }
      if (nk_button_label(ui->ctx, "Redo")) {
        engine_execute_command(engine, "redo");
      }

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_spacing(ui->ctx, 1);

      // Section: Export
      nk_layout_row_dynamic(ui->ctx, 18, 1);
      nk_label(ui->ctx, "Export", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ui->ctx, 32, 1);
      if (nk_button_label(ui->ctx, "Save As...")) {
        ui->show_save_aslan_modal = 1;
        if (getcwd(ui->save_aslan_dir, sizeof(ui->save_aslan_dir)) == NULL) {
          snprintf(ui->save_aslan_dir, sizeof(ui->save_aslan_dir), ".");
        }
        if (!ui->save_aslan_name[0])
          snprintf(ui->save_aslan_name, sizeof(ui->save_aslan_name),
                   "document.aslan");
      }

      if (nk_button_label(ui->ctx, "Export SVG")) {
        ui->show_export_svg_modal = 1;
        if (getcwd(ui->export_svg_dir, sizeof(ui->export_svg_dir)) == NULL) {
          snprintf(ui->export_svg_dir, sizeof(ui->export_svg_dir), ".");
        }
        snprintf(ui->export_svg_name, sizeof(ui->export_svg_name), "%s",
                 ui->export_svg_path[0] ? ui->export_svg_path : "preview.svg");
      }

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_spacing(ui->ctx, 1);

      // Section: View
      nk_layout_row_dynamic(ui->ctx, 18, 1);
      nk_label(ui->ctx, "View", NK_TEXT_LEFT);

      int grid_enabled = 0;
      int snap_enabled = 0;
      if (engine && engine->doc) {
        const char *ge = doc_get_global(engine->doc, "grid.enabled");
        if (ge && (strcmp(ge, "1") == 0 || strcasecmp(ge, "true") == 0))
          grid_enabled = 1;
        const char *se = doc_get_global(engine->doc, "snap.enabled");
        if (se && (strcmp(se, "1") == 0 || strcasecmp(se, "true") == 0))
          snap_enabled = 1;
      }

      nk_layout_row_dynamic(ui->ctx, 24, 2);
      if (nk_checkbox_label(ui->ctx, "Grid", &grid_enabled)) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "set_global grid.enabled \"%d\"",
                 grid_enabled ? 1 : 0);
        engine_execute_command(engine, cmd);
      }
      if (nk_checkbox_label(ui->ctx, "Snap", &snap_enabled)) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "set_global snap.enabled \"%d\"",
                 snap_enabled ? 1 : 0);
        engine_execute_command(engine, cmd);
      }
    }
    nk_end(ui->ctx);
  }

  // Selection inspector (only for canvas, not note)
  if (!is_note_page && ui->selected_node) {
    Node *n = ui->selected_node;
    float insp_w = 220.0f;
    float insp_h = 300.0f;
    float insp_x = (float)win_w - insp_w - 16.0f;
    float insp_y = (float)win_h - insp_h - 60.0f;

    if (nk_begin(ui->ctx, "Selection", nk_rect(insp_x, insp_y, insp_w, insp_h),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE)) {

      // Node info
      char info[128];
      snprintf(info, sizeof(info), "%s (#%u)", n->name, (unsigned)n->id);
      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, info, NK_TEXT_LEFT);

      nk_layout_row_dynamic(ui->ctx, 8, 1);
      nk_spacing(ui->ctx, 1);

      // Position
      nk_layout_row_dynamic(ui->ctx, 18, 1);
      nk_label(ui->ctx, "Position", NK_TEXT_LEFT);

      int ix = (int)n->x, iy = (int)n->y;
      nk_layout_row_dynamic(ui->ctx, 24, 2);
      nk_property_int(ui->ctx, "X", -9999, &ix, 9999, 1, 1);
      nk_property_int(ui->ctx, "Y", -9999, &iy, 9999, 1, 1);

      if (ix != (int)n->x || iy != (int)n->y) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "move %u %d %d", (unsigned)n->id, ix, iy);
        engine_execute_command(engine, cmd);
      }

      // Size
      nk_layout_row_dynamic(ui->ctx, 18, 1);
      nk_label(ui->ctx, "Size", NK_TEXT_LEFT);

      int iw = (int)n->width, ih = (int)n->height;
      nk_layout_row_dynamic(ui->ctx, 24, 2);
      nk_property_int(ui->ctx, "W", 1, &iw, 9999, 1, 1);
      nk_property_int(ui->ctx, "H", 1, &ih, 9999, 1, 1);

      if (iw != (int)n->width || ih != (int)n->height) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "resize %u %d %d", (unsigned)n->id, iw, ih);
        engine_execute_command(engine, cmd);
      }

      nk_layout_row_dynamic(ui->ctx, 12, 1);
      nk_spacing(ui->ctx, 1);

      // Delete
      nk_layout_row_dynamic(ui->ctx, 32, 1);
      if (nk_button_label(ui->ctx, "Delete")) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "delete %u", (unsigned)n->id);
        engine_execute_command(engine, cmd);
        ui->selected_node = NULL;
      }
    }
    nk_end(ui->ctx);
  }

  // Legacy inspector code removed - now using modern sidebar above

  // Inline (in-situ) text editing overlay
  // Contract:
  // - Activated by UI (double-click hook will set ui->editing_node_id).
  // - While active, we show an edit box right on top of the text node.
  // - Enter commits, Esc cancels.
  // - Click outside commits (handled by losing active state + dirty flag).
  if (engine && engine->doc && ui->editing_node_id) {
    Node *n = doc_find_node_by_id(engine->doc, ui->editing_node_id);
    if (!n || n->type != NODE_TYPE_TEXT) {
      ui->editing_node_id = 0;
      ui->inline_edit_inited = 0;
      ui->inline_edit_dirty = 0;
    } else {
      // Initialize buffer on entry
      if (!ui->inline_edit_inited) {
        snprintf(ui->inline_edit_buf, sizeof(ui->inline_edit_buf), "%s",
                 n->text);
        ui->inline_edit_inited = 1;
        ui->inline_edit_wants_focus = 1;
        ui->inline_edit_dirty = 0;
      }

      // World -> screen (match logical 1024x768 center like main.c)
      float vx = 0.0f, vy = 0.0f, vz = 1.0f;
      uint32_t pid = doc_page_active_id(engine->doc);
      for (int pi = 0; pi < 32; pi++) {
        if (engine->doc->pages[pi].in_use &&
            engine->doc->pages[pi].page_id == pid) {
          vx = engine->doc->pages[pi].view_x;
          vy = engine->doc->pages[pi].view_y;
          vz = (engine->doc->pages[pi].view_zoom != 0.0f)
                   ? engine->doc->pages[pi].view_zoom
                   : 1.0f;
          break;
        }
      }
      float cx = 1024.0f * 0.5f;
      float cy = 768.0f * 0.5f;
      float sx = (n->x - vx) * vz + cx;
      float sy = (n->y - vy) * vz + cy;
      float sw = n->width * vz;
      float sh = n->height * vz;
      if (sw < 40)
        sw = 40;
      if (sh < 26)
        sh = 26;

      // Antirez-style: minimal chrome.
      struct nk_style_window old_w = ui->ctx->style.window;
      struct nk_style_edit old_e = ui->ctx->style.edit;

      ui->ctx->style.window.background = nk_rgba(0, 0, 0, 0);
      ui->ctx->style.window.fixed_background =
          nk_style_item_color(nk_rgba(0, 0, 0, 0));
      ui->ctx->style.window.border = 0;
      ui->ctx->style.window.rounding = 0;
      ui->ctx->style.window.padding = nk_vec2(0, 0);
      ui->ctx->style.window.group_padding = nk_vec2(0, 0);

      // Edit style: subtle underline-ish border
      ui->ctx->style.edit.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      ui->ctx->style.edit.hover = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      ui->ctx->style.edit.active = nk_style_item_color(nk_rgba(0, 0, 0, 0));
      ui->ctx->style.edit.border = 1;
      ui->ctx->style.edit.rounding = 0;
      ui->ctx->style.edit.border_color = nk_rgb(0, 120, 215);
      ui->ctx->style.edit.cursor_normal = nk_rgb(235, 235, 235);
      ui->ctx->style.edit.cursor_hover = nk_rgb(235, 235, 235);
      ui->ctx->style.edit.text_normal = nk_rgb(235, 235, 235);
      ui->ctx->style.edit.text_hover = nk_rgb(235, 235, 235);
      ui->ctx->style.edit.text_active = nk_rgb(235, 235, 235);
      ui->ctx->style.edit.padding = nk_vec2(6, 6);

      if (nk_begin(ui->ctx, "_inline_text_edit", nk_rect(sx, sy, sw, sh),
                   NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(ui->ctx, sh, 1);

        if (ui->inline_edit_wants_focus) {
          // Give focus to this widget this frame.
          nk_edit_focus(ui->ctx, NK_EDIT_ALWAYS_INSERT_MODE);
          ui->inline_edit_wants_focus = 0;
        }

        int before_len = (int)strlen(ui->inline_edit_buf);
        nk_flags fl = NK_EDIT_BOX | NK_EDIT_SELECTABLE | NK_EDIT_AUTO_SELECT;
        nk_edit_string_zero_terminated(ui->ctx, fl, ui->inline_edit_buf,
                                       (int)sizeof(ui->inline_edit_buf) - 1,
                                       nk_filter_default);
        int after_len = (int)strlen(ui->inline_edit_buf);
        if (after_len != before_len)
          ui->inline_edit_dirty = 1;

        // Commit/cancel shortcuts
        if (nk_input_is_key_pressed(&ui->ctx->input, NK_KEY_ENTER)) {
          ui->inline_edit_dirty = 1; // commit even if unchanged (idempotent)
          // commit below after nk_end
          ui->editing_node_id = ui->editing_node_id; // keep
        }
        // Nuklear doesn't expose a dedicated ESC key in its key enum.
        // We use NK_KEY_TEXT_RESET_MODE as a "cancel" shortcut.
        if (nk_input_is_key_pressed(&ui->ctx->input, NK_KEY_TEXT_RESET_MODE)) {
          // Cancel
          ui->editing_node_id = 0;
          ui->inline_edit_inited = 0;
          ui->inline_edit_dirty = 0;
        }
      }
      nk_end(ui->ctx);

      // If we are still editing and Enter was pressed, commit now.
      if (ui->editing_node_id &&
          nk_input_is_key_pressed(&ui->ctx->input, NK_KEY_ENTER)) {
        char cmd[2200];
        char escaped[2100];
        int j = 0;
        for (int i = 0; ui->inline_edit_buf[i] && j < (int)sizeof(escaped) - 2;
             i++) {
          if (ui->inline_edit_buf[i] == '"')
            escaped[j++] = '\\';
          escaped[j++] = ui->inline_edit_buf[i];
        }
        escaped[j] = '\0';
        snprintf(cmd, sizeof(cmd), "set_text %u \"%s\"", (unsigned)n->id,
                 escaped);
        engine_execute_command(engine, cmd);
        ui->editing_node_id = 0;
        ui->inline_edit_inited = 0;
        ui->inline_edit_dirty = 0;
        snprintf(ui->status, sizeof(ui->status), "Text updated");
      }

      ui->ctx->style.window = old_w;
      ui->ctx->style.edit = old_e;
    }
  }

  // Bottom Pages bar (Editor)
  {
    float bar_h = 44.0f;
    struct nk_rect b = nk_rect(0, (float)win_h - bar_h, (float)win_w, bar_h);
    if (nk_begin(ui->ctx, "Pages", b,
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
      uint32_t active =
          (engine && engine->doc) ? doc_page_active_id(engine->doc) : 0;

      // One row: + then tabs.
      nk_layout_row_dynamic(ui->ctx, 30, 10);

      if (nk_button_label(ui->ctx, "+")) {
        // Open New Page modal instead of immediate creation
        ui->show_new_page_modal = 1;
        // Reset defaults
        ui->new_page_type_idx = 0;    // Space
        ui->new_page_preset_idx = 0;  // A4
        ui->new_page_orientation = 0; // Portrait
        ui->new_page_width_mm = 210.0f;
        ui->new_page_height_mm = 297.0f;
        if (engine && engine->doc) {
          snprintf(ui->new_page_name, sizeof(ui->new_page_name), "Page %d",
                   engine->doc->page_count + 1);
        } else {
          snprintf(ui->new_page_name, sizeof(ui->new_page_name), "Page 1");
        }
      }

      if (engine && engine->doc) {
        for (int i = 0; i < 32; i++) {
          if (!engine->doc->pages[i].in_use)
            continue;
          const char *name = engine->doc->pages[i].name;
          uint32_t pid = engine->doc->pages[i].page_id;
          if (!name || !name[0])
            name = "(unnamed)";

          char label[96];
          if (pid == active)
            snprintf(label, sizeof(label), "[%s]", name);
          else
            snprintf(label, sizeof(label), "%s", name);

          if (nk_button_label(ui->ctx, label)) {
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "page_select %u", (unsigned)pid);
            engine_execute_command(engine, cmd);
            snprintf(ui->status, sizeof(ui->status), "Page: %s", name);
          }
        }
      }
    }
    nk_end(ui->ctx);
  }

  // Modal save-as window for `.aslan` snapshots
  if (ui->show_save_aslan_modal) {
    struct nk_rect bounds =
        nk_rect(win_w * 0.5f - 170.0f, win_h * 0.5f - 130.0f, 340.0f, 260.0f);
    if (nk_begin(ui->ctx, "Save As (.aslan)", bounds,
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE |
                     NK_WINDOW_NO_SCROLLBAR)) {

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Directory:", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ui->ctx, 28, 1);
      nk_edit_string_zero_terminated(ui->ctx, NK_EDIT_FIELD, ui->save_aslan_dir,
                                     (int)sizeof(ui->save_aslan_dir) - 1,
                                     nk_filter_default);

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Filename:", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ui->ctx, 28, 1);
      nk_edit_string_zero_terminated(
          ui->ctx, NK_EDIT_FIELD, ui->save_aslan_name,
          (int)sizeof(ui->save_aslan_name) - 1, nk_filter_default);

      nk_layout_row_dynamic(ui->ctx, 18, 1);
      nk_label(ui->ctx, "Tip: name ends with .aslan", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ui->ctx, 34, 2);
      if (nk_button_label(ui->ctx, "Cancel")) {
        ui->show_save_aslan_modal = 0;
      }
      if (nk_button_label(ui->ctx, "Save")) {
        char name[256];
        snprintf(name, sizeof(name), "%s",
                 ui->save_aslan_name[0] ? ui->save_aslan_name
                                        : "document.aslan");
        size_t nlen = strlen(name);
        if (nlen < 6 || strcmp(name + (nlen - 6), ".aslan") != 0) {
          strncat(name, ".aslan", sizeof(name) - strlen(name) - 1);
        }

        mkdir_p(ui->save_aslan_dir);

        char out_path[512];
        join_path(out_path, sizeof(out_path), ui->save_aslan_dir, name);

        if (engine_save_snapshot_as_aslan(engine, out_path)) {
          snprintf(ui->status, sizeof(ui->status), "Saved: %s", out_path);
          reveal_file_in_finder_best_effort(out_path);
          ui_recents_add_and_persist(ui, out_path);
        } else {
          snprintf(ui->status, sizeof(ui->status), "Save failed: %s", out_path);
        }

        ui->show_save_aslan_modal = 0;
      }
    }
    nk_end(ui->ctx);
  }

  // Modal export window (robust: plain nk_begin, avoids nk_popup_begin asserts)
  if (ui->show_export_svg_modal) {
    struct nk_rect bounds =
        nk_rect(win_w * 0.5f - 170.0f, win_h * 0.5f - 130.0f, 340.0f, 260.0f);
    if (nk_begin(ui->ctx, "Export SVG", bounds,
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE |
                     NK_WINDOW_NO_SCROLLBAR)) {

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Directory:", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ui->ctx, 28, 1);
      nk_edit_string_zero_terminated(ui->ctx, NK_EDIT_FIELD, ui->export_svg_dir,
                                     (int)sizeof(ui->export_svg_dir) - 1,
                                     nk_filter_default);

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Filename:", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ui->ctx, 28, 1);
      nk_edit_string_zero_terminated(
          ui->ctx, NK_EDIT_FIELD, ui->export_svg_name,
          (int)sizeof(ui->export_svg_name) - 1, nk_filter_default);

      nk_layout_row_dynamic(ui->ctx, 18, 1);
      nk_label(ui->ctx, "Tip: name ends with .svg", NK_TEXT_LEFT);

      nk_layout_row_dynamic(ui->ctx, 34, 2);
      if (nk_button_label(ui->ctx, "Cancel")) {
        ui->show_export_svg_modal = 0;
      }
      if (nk_button_label(ui->ctx, "Save")) {
        char name[256];
        snprintf(name, sizeof(name), "%s",
                 ui->export_svg_name[0] ? ui->export_svg_name : "preview.svg");
        size_t nlen = strlen(name);
        if (nlen < 4 || strcmp(name + (nlen - 4), ".svg") != 0) {
          strncat(name, ".svg", sizeof(name) - strlen(name) - 1);
        }

        mkdir_p(ui->export_svg_dir);

        char out_path[512];
        join_path(out_path, sizeof(out_path), ui->export_svg_dir, name);

        doc_export_to_svg(engine->doc, out_path);
        snprintf(ui->export_svg_path, sizeof(ui->export_svg_path), "%s", name);
        snprintf(ui->status, sizeof(ui->status), "Saved: %s", out_path);
        reveal_file_in_finder_best_effort(out_path);

        ui->show_export_svg_modal = 0;
      }
    }
    nk_end(ui->ctx);
  }

  // Modal New Page window
  if (ui->show_new_page_modal) {
    struct nk_rect bounds =
        nk_rect(win_w * 0.5f - 200.0f, win_h * 0.5f - 200.0f, 400.0f, 400.0f);
    if (nk_begin(ui->ctx, "New Page", bounds,
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE |
                     NK_WINDOW_NO_SCROLLBAR)) {

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Page Name:", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ui->ctx, 28, 1);
      nk_edit_string_zero_terminated(ui->ctx, NK_EDIT_FIELD, ui->new_page_name,
                                     (int)sizeof(ui->new_page_name) - 1,
                                     nk_filter_default);

      nk_layout_row_dynamic(ui->ctx, 20, 1);
      nk_label(ui->ctx, "Type:", NK_TEXT_LEFT);
      nk_layout_row_dynamic(ui->ctx, 28, 2);
      if (nk_option_label(ui->ctx, "Space (Infinite)",
                          ui->new_page_type_idx == 0))
        ui->new_page_type_idx = 0;
      if (nk_option_label(ui->ctx, "Page (Fixed)", ui->new_page_type_idx == 1))
        ui->new_page_type_idx = 1;

      if (nk_option_label(ui->ctx, "Note", ui->new_page_type_idx == 2))
        ui->new_page_type_idx = 2;
      if (nk_option_label(ui->ctx, "Grid (Table)", ui->new_page_type_idx == 4))
        ui->new_page_type_idx = 4;

      if (ui->new_page_type_idx == 1) { // Page options
        nk_layout_row_dynamic(ui->ctx, 20, 1);
        nk_label(ui->ctx, "Preset:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ui->ctx, 28, 4);
        if (nk_option_label(ui->ctx, "A4", ui->new_page_preset_idx == 0)) {
          ui->new_page_preset_idx = 0;
          ui->new_page_width_mm = 210.0f;
          ui->new_page_height_mm = 297.0f;
        }
        if (nk_option_label(ui->ctx, "A3", ui->new_page_preset_idx == 1)) {
          ui->new_page_preset_idx = 1;
          ui->new_page_width_mm = 297.0f;
          ui->new_page_height_mm = 420.0f;
        }
        if (nk_option_label(ui->ctx, "Letter", ui->new_page_preset_idx == 2)) {
          ui->new_page_preset_idx = 2;
          ui->new_page_width_mm = 215.9f;
          ui->new_page_height_mm = 279.4f;
        }
        if (nk_option_label(ui->ctx, "Custom", ui->new_page_preset_idx == 3))
          ui->new_page_preset_idx = 3;

        nk_layout_row_dynamic(ui->ctx, 20, 1);
        nk_label(ui->ctx, "Orientation:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ui->ctx, 28, 2);
        if (nk_option_label(ui->ctx, "Portrait",
                            ui->new_page_orientation == 0)) {
          if (ui->new_page_orientation != 0) {
            float tmp = ui->new_page_width_mm;
            ui->new_page_width_mm = ui->new_page_height_mm;
            ui->new_page_height_mm = tmp;
          }
          ui->new_page_orientation = 0;
        }
        if (nk_option_label(ui->ctx, "Landscape",
                            ui->new_page_orientation == 1)) {
          if (ui->new_page_orientation != 1) {
            float tmp = ui->new_page_width_mm;
            ui->new_page_width_mm = ui->new_page_height_mm;
            ui->new_page_height_mm = tmp;
          }
          ui->new_page_orientation = 1;
        }

        nk_layout_row_dynamic(ui->ctx, 20, 1);
        nk_label(ui->ctx, "Dimensions (mm):", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ui->ctx, 28, 2);
        nk_property_float(ui->ctx, "W", 50.0f, &ui->new_page_width_mm, 2000.0f,
                          1.0f, 1.0f);
        nk_property_float(ui->ctx, "H", 50.0f, &ui->new_page_height_mm, 2000.0f,
                          1.0f, 1.0f);
      }

      nk_layout_row_dynamic(ui->ctx, 34, 2);
      if (nk_button_label(ui->ctx, "Cancel")) {
        ui->show_new_page_modal = 0;
      }
      if (nk_button_label(ui->ctx, "Create")) {
        // 1. Create page
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "page_create \"%s\"",
                 ui->new_page_name[0] ? ui->new_page_name : "Page");
        engine_execute_command(engine, cmd);

        // Get the newly created page id (it's the active one)
        uint32_t pid = doc_page_active_id(engine->doc);

        // 2. Set type and meta
        if (ui->new_page_type_idx == 1) { // Page
          snprintf(cmd, sizeof(cmd), "page_type %u page", (unsigned)pid);
          engine_execute_command(engine, cmd);
          snprintf(cmd, sizeof(cmd), "page_meta %u width %.2f", (unsigned)pid,
                   ui->new_page_width_mm);
          engine_execute_command(engine, cmd);
          snprintf(cmd, sizeof(cmd), "page_meta %u height %.2f", (unsigned)pid,
                   ui->new_page_height_mm);
          engine_execute_command(engine, cmd);
        } else if (ui->new_page_type_idx == 0) { // Space
          snprintf(cmd, sizeof(cmd), "page_type %u space", (unsigned)pid);
          engine_execute_command(engine, cmd);
          // Default viewport
          snprintf(cmd, sizeof(cmd), "page_meta %u viewport.x 0",
                   (unsigned)pid);
          engine_execute_command(engine, cmd);
          snprintf(cmd, sizeof(cmd), "page_meta %u viewport.y 0",
                   (unsigned)pid);
          engine_execute_command(engine, cmd);
          snprintf(cmd, sizeof(cmd), "page_meta %u viewport.zoom 1.0",
                   (unsigned)pid);
          engine_execute_command(engine, cmd);
        } else {
          const char *t = "space";
          if (ui->new_page_type_idx == 2)
            t = "note";
          else if (ui->new_page_type_idx == 3)
            t = "clip";
          else if (ui->new_page_type_idx == 4)
            t = "grid";
          snprintf(cmd, sizeof(cmd), "page_type %u %s", (unsigned)pid, t);
          engine_execute_command(engine, cmd);
        }

        ui->show_new_page_modal = 0;
        snprintf(ui->status, sizeof(ui->status), "Created page: %s",
                 ui->new_page_name);
      }
    }
    nk_end(ui->ctx);
  }

  // Draw TEXT nodes + NOTE pages into Nuklear's overlay command buffer.
  // This doesn't create a window, so it won't interfere with input routing.
  if (engine && engine->doc && engine->doc->root && ui->ctx->style.font) {
    // Get viewport transform
    float vx = 0.0f, vy = 0.0f, vz = 1.0f;
    uint32_t pid = doc_page_active_id(engine->doc);
    PageType active_pt = PAGE_TYPE_SPACE;
    AslanNoteDoc *active_note = NULL;
    for (int pi = 0; pi < 32; pi++) {
      if (engine->doc->pages[pi].in_use &&
          engine->doc->pages[pi].page_id == pid) {
        vx = engine->doc->pages[pi].view_x;
        vy = engine->doc->pages[pi].view_y;
        vz = (engine->doc->pages[pi].view_zoom != 0.0f)
                 ? engine->doc->pages[pi].view_zoom
                 : 1.0f;
        active_pt = engine->doc->pages[pi].type;
        active_note = engine->doc->pages[pi].note;
        break;
      }
    }
    float cx = 1024.0f * 0.5f;
    float cy = 768.0f * 0.5f;

    // Initialize and start the overlay buffer.
    // NOTE: clipping must be ON, otherwise NOTE text can draw outside the paper
    // panel and appears to "disappear"/"move" when scrolling.
    nk_command_buffer_init(&ui->ctx->overlay, &ui->ctx->memory, NK_CLIPPING_ON);
    nk_start_buffer(ui->ctx, &ui->ctx->overlay);

    struct nk_color col = nk_rgb(235, 235, 235);

    // ==========================================================================
    // NOTE PAGE OVERLAY - Notion-style Clean Design
    // ==========================================================================
    // Light, minimal, focused on writing. Like a clean sheet of paper.

    if (active_pt == PAGE_TYPE_NOTE && active_note && active_note->rows &&
        active_note->row_count > 0) {

      // ========================================================================
      // MODERN LAYOUT (must match renderer.c)
      // ========================================================================
      const float max_content_w = 800.0f;
      float content_w = (float)win_w * 0.8f;
      if (content_w > max_content_w)
        content_w = max_content_w;

      float margin_left = ((float)win_w - content_w) * 0.5f;
      if (margin_left < 40.0f)
        margin_left = 40.0f; // Minimum margin

      const float top_margin = 80.0f; // Breathing room at top
      const float line_h = 30.0f;     // Comfortable line height (slightly more)

      // Derived dimensions
      const float content_x = margin_left;
      const float content_top = top_margin;
      const float text_pad_x = 0.0f; // No extra padding needed

      // Legacy compatibility aliases
      const float pad_x = content_x;
      const float top_y = top_margin;
      const float w = content_w;

      // ========================================================================
      // SCROLL HANDLING
      // ========================================================================
      float scroll_y =
          (active_note->scroll_y > 0.0f) ? active_note->scroll_y : 0.0f;
      {
        float viewport_h = (float)win_h - content_top - 48.0f;
        if (viewport_h < 0.0f)
          viewport_h = 0.0f;
        float content_h = 0.0f;
        for (int i = 0; i < active_note->row_count; i++) {
          content_h += get_line_height(active_note->rows[i].type);
        }
        float max_scroll = content_h - viewport_h;
        if (max_scroll < 0.0f)
          max_scroll = 0.0f;
        if (scroll_y > max_scroll)
          scroll_y = max_scroll;
        active_note->scroll_y = scroll_y;
      }

      // ========================================================================
      // MODERN COLOR PALETTE (2024 Apple Notes / Notion inspired)
      // ========================================================================
      struct nk_color txt_col =
          nk_rgb(45, 45, 45); // Softer black (easier on eyes)
      struct nk_color txt_heading =
          nk_rgb(20, 20, 20); // Strong black for headings
      struct nk_color sel_bg =
          nk_rgb(200, 230, 255); // Soft blue selection (Apple style)
      struct nk_color sel_txt = nk_rgb(20, 20, 20); // Dark text on selection
      struct nk_color cursor_col = nk_rgb(0, 122, 255); // iOS blue cursor

      // ========================================================================
      // CLIPPING: Clip text to content area
      // ========================================================================
      nk_push_scissor(&ui->ctx->overlay,
                      nk_rect(0, 0, (float)win_w, (float)win_h));

      // Dedicated font for NOTE (monospace) so caret alignment is
      // deterministic.
      const struct nk_user_font *note_font = ui->ctx->style.font;
      if (ui->font_mono)
        note_font = &ui->font_mono->handle;

      // Base monospace cell width (used when a row falls back to mono).
      float char_w_mono = note_measure_char_w(note_font);

      // ========================================================================
      // HIGHLIGHTS (Fluo) - PER-LINE with MULTI-COLOR (stable, simple)
      // ========================================================================
      // Define colors for each highlight type (0=none, 1=yellow, 2=blue,
      // 3=green)
      struct nk_color fluo_colors_main[4] = {
          nk_rgba(0, 0, 0, 0),         // 0 = none
          nk_rgba(255, 235, 100, 160), // 1 = yellow (warmer)
          nk_rgba(160, 220, 255, 160), // 2 = blue (softer)
          nk_rgba(180, 255, 180, 160)  // 3 = green (pastel)
      };
      struct nk_color fluo_colors_shadow[4] = {
          nk_rgba(0, 0, 0, 0),         // 0 = none
          nk_rgba(235, 200, 50, 180),  // 1 = yellow shadow
          nk_rgba(120, 180, 235, 180), // 2 = blue shadow
          nk_rgba(140, 230, 140, 180)  // 3 = green shadow
      };
      struct nk_color fluo_colors_light[4] = {
          nk_rgba(0, 0, 0, 0),         // 0 = none
          nk_rgba(255, 255, 200, 140), // 1 = yellow light
          nk_rgba(220, 240, 255, 140), // 2 = blue light
          nk_rgba(220, 255, 220, 140)  // 3 = green light
      };

      float y_fluo = top_y;
      for (int i = 0; i < active_note->row_count; i++) {
        float row_h = get_line_height(active_note->rows[i].type);
        AslanRow *row = &active_note->rows[i];
        float py = y_fluo - scroll_y;

        // Check for highlight_color (new system: 1=yellow, 2=blue, 3=green)
        int cidx = row->highlight_color;
        if (cidx < 0 || cidx > 3)
          cidx = 0;

        // Also check old ASLAN_ROW_HIGHLIGHT type for backwards compatibility
        if (row->type == ASLAN_ROW_HIGHLIGHT && cidx == 0) {
          cidx = 1; // Default to yellow for old highlights
        }

        // Draw highlight if any color is set
        if (cidx > 0 && py + row_h >= top_y && py < (float)win_h) {
          const char *text = (row->chars && row->len > 0) ? row->chars : "";
          int text_len = (row->chars) ? (int)row->len : 0;

          float text_width = 50.0f; // Default minimum
          if (note_font && note_font->width && text_len > 0) {
            text_width = note_font->width(note_font->userdata,
                                          note_font->height, text, text_len);
          }
          text_width += 12.0f;
          if (text_width < 20.0f)
            text_width = 20.0f;
          if (text_width > w)
            text_width = w;

          // MARKER EFFECT: Multiple layers for realistic look
          float marker_h = row_h * 0.65f; // 65% of line height
          float marker_y =
              py + (row_h - marker_h) * 0.4f; // Slightly above center

          // Layer 1: Main marker color
          nk_fill_rect(&ui->ctx->overlay,
                       nk_rect(pad_x - 2, marker_y, text_width, marker_h), 0.0f,
                       fluo_colors_main[cidx]);

          // Layer 2: Bottom shadow (darker edge)
          nk_fill_rect(
              &ui->ctx->overlay,
              nk_rect(pad_x - 2, marker_y + marker_h - 3, text_width, 3), 0.0f,
              fluo_colors_shadow[cidx]);

          // Layer 3: Top highlight (lighter edge)
          nk_fill_rect(&ui->ctx->overlay,
                       nk_rect(pad_x - 2, marker_y, text_width, 2), 0.0f,
                       fluo_colors_light[cidx]);
        }

        y_fluo += row_h;
      }

      // Selection (range between cursor and anchor). We draw:
      // 1) normal text in black
      // 2) selection background rects
      // 3) selected text in white (redrawn on top)
      int sel_active = (active_note->is_selecting &&
                        !(active_note->anchor_row == active_note->cursor_row &&
                          active_note->anchor_col == active_note->cursor_col));
      int sel_sr = 0, sel_sc = 0, sel_er = 0, sel_ec = 0;
      if (sel_active) {
        sel_sr = active_note->anchor_row;
        sel_sc = active_note->anchor_col;
        sel_er = active_note->cursor_row;
        sel_ec = active_note->cursor_col;
        if (sel_sr > sel_er || (sel_sr == sel_er && sel_sc > sel_ec)) {
          int tr = sel_sr, tc = sel_sc;
          sel_sr = sel_er;
          sel_sc = sel_ec;
          sel_er = tr;
          sel_ec = tc;
        }
        if (sel_sr < 0)
          sel_sr = 0;
        if (sel_er < 0)
          sel_er = 0;
        if (sel_sr >= active_note->row_count)
          sel_sr = active_note->row_count - 1;
        if (sel_er >= active_note->row_count)
          sel_er = active_note->row_count - 1;
        if (sel_sr < 0 || sel_er < 0)
          sel_active = 0;
      }

      for (int i = 0; i < active_note->row_count; i++) {
        AslanRow *row = &active_note->rows[i];
        const char *t = (row->chars ? row->chars : "");

        // Adapt rendering based on block type (Antirez approach: type
        // determines rendering)
        float row_line_h = line_h;
        struct nk_color row_txt_col = txt_col;
        const struct nk_user_font *row_font = note_font;

        // ======================================================================
        // HEADINGS: Modern typography hierarchy
        // ======================================================================
        if (row->type == ASLAN_ROW_HEADING_1) {
          row_line_h = 64.0f; // Even more prominent
          row_txt_col = txt_heading;
          if (ui->font_h1)
            row_font = &ui->font_h1->handle;
        } else if (row->type == ASLAN_ROW_SEPARATOR) {
          // Separator: clean horizontal rule (minimal, Apple Notes style)
          float y_pos = top_y + get_line_y_position(active_note, i) +
                        get_line_height(row->type) * 0.5f - scroll_y;

          struct nk_command_buffer *canvas = &ui->ctx->overlay;
          // Subtle, thin gray line
          nk_stroke_line(canvas, pad_x, y_pos, pad_x + w, y_pos, 1.0f,
                         nk_rgb(220, 220, 220));
          continue;
        } else if (row->type == ASLAN_ROW_GRID ||
                   row->type == ASLAN_ROW_EMBED) {
          // GRID and EMBED completely removed from MVP
          // These types should not exist in the document anymore
          // If they do (from old files), just skip rendering them silently
          continue;
        }

        // ======================================================================
        // TEXT RENDERING: For TEXT and HIGHLIGHT types
        // ======================================================================
        // Highlight rows get yellow background (rendered in Pass 1 above)
        // Here we just render the text content

        // Calculate Y position using helper function (accumulate row heights
        // for proper spacing)
        float y_pos =
            top_y + get_line_y_position(active_note, i) + 3.0f - scroll_y;

        // Render text with the appropriate heading font
        int t_len = 0;
        if ((uintptr_t)t > 0x1000) {
          t_len = (int)strlen(t);
        }
        if (t_len > 0) {
          nk_draw_text(&ui->ctx->overlay,
                       nk_rect(pad_x + text_pad_x, y_pos,
                               w - (text_pad_x * 2.0f), row_line_h),
                       t, t_len, row_font, nk_rgba(0, 0, 0, 0), row_txt_col);
        } else if (i == 0 && row->type == ASLAN_ROW_HEADING_1) {
          // Placeholder for empty title
          nk_draw_text(&ui->ctx->overlay,
                       nk_rect(pad_x + text_pad_x, y_pos,
                               w - (text_pad_x * 2.0f), row_line_h),
                       "Untitled", 8, row_font, nk_rgba(0, 0, 0, 0),
                       nk_rgb(200, 200, 200));
        } else if (i == 1 && row->type == ASLAN_ROW_TEXT &&
                   active_note->row_count > 0) {
          // Subtle placeholder for start of note
          nk_draw_text(&ui->ctx->overlay,
                       nk_rect(pad_x + text_pad_x, y_pos,
                               w - (text_pad_x * 2.0f), row_line_h),
                       "Start writing...", 16, row_font, nk_rgba(0, 0, 0, 0),
                       nk_rgb(220, 220, 220));
        }

        if (sel_active && i >= sel_sr && i <= sel_er && (uintptr_t)t > 0x1000) {
          int line_len = (int)strlen(t);
          int a = 0;
          int b = line_len;
          if (i == sel_sr)
            a = sel_sc;
          if (i == sel_er)
            b = sel_ec;
          if (a < 0)
            a = 0;
          if (b < 0)
            b = 0;
          if (a > line_len)
            a = line_len;
          if (b > line_len)
            b = line_len;
          if (b < a)
            b = a;

          if (b > a) {
            // Selection must use the same font metrics and Y positioning as the
            // text.
            float row_char_w = note_measure_char_w(row_font);
            float rx = pad_x + text_pad_x + (float)a * row_char_w;
            float ry = y_pos;
            float rw = (float)(b - a) * row_char_w;
            float rh = row_font ? row_font->height : (line_h - 4.0f);
            float row_h = get_line_height(row->type);
            if (rh > row_h - 4.0f)
              rh = row_h - 4.0f;

            nk_fill_rect(&ui->ctx->overlay, nk_rect(rx, ry, rw, rh), 2.0f,
                         sel_bg);

            // Redraw selected substring in white.
            char buf[2048];
            int n = b - a;
            if (n > 0) {
              if (n >= (int)sizeof(buf))
                n = (int)sizeof(buf) - 1;
              memcpy(buf, t + a, (size_t)n);
              buf[n] = 0;
              nk_draw_text(&ui->ctx->overlay, nk_rect(rx, ry, rw, row_line_h),
                           buf, n, row_font, nk_rgba(0, 0, 0, 0), sel_txt);
            }
          }
        }
      }

      // ========================================================================
      // CARET: Clean thin cursor (Notion-style)
      // ========================================================================
      if (active_note->cursor_row >= 0 &&
          active_note->cursor_row < active_note->row_count) {
        AslanRow *cursor_row = &active_note->rows[active_note->cursor_row];
        const struct nk_user_font *cursor_font =
            note_row_font(ui, note_font, cursor_row->type);
        float cursor_char_w = (cursor_font == note_font)
                                  ? char_w_mono
                                  : note_measure_char_w(cursor_font);

        int col = active_note->cursor_col;
        if (col < 0)
          col = 0;

        float cursor_y_offset =
            get_line_y_position(active_note, active_note->cursor_row);

        float cy0 = top_y + cursor_y_offset + 4.0f - scroll_y;

        // Special case: separator line
        if (cursor_row->type == ASLAN_ROW_SEPARATOR) {
          goto note_caret_done; // No cursor on separators
        }

        float cx0 = pad_x + text_pad_x + (float)col * cursor_char_w;

        float cursor_row_height = get_line_height(cursor_row->type);
        float caret_h =
            cursor_font ? cursor_font->height : (cursor_row_height - 6.0f);
        if (caret_h > cursor_row_height - 6.0f)
          caret_h = cursor_row_height - 6.0f;
        float cy1 = cy0 + caret_h;

        // Draw thin caret line (1px for clean look)
        nk_stroke_line(&ui->ctx->overlay, cx0, cy0, cx0, cy1, 1.5f, cursor_col);

      note_caret_done:
        (void)0;
      }
    }

    for (Node *n = engine->doc->root->first_child; n; n = n->next_sibling) {
      if (!n->visible)
        continue;
      if (n->type != NODE_TYPE_TEXT)
        continue;

      float sx = (n->x - vx) * vz + cx;
      float sy = (n->y - vy) * vz + cy;
      float sw = n->width * vz;
      float sh = n->height * vz;
      if (sw < 1)
        sw = 1;
      if (sh < 1)
        sh = 1;

      const char *txt = (n->text[0] ? n->text : "Text");
      // Extra safety: only use txt if it looks like a valid pointer
      if ((uintptr_t)txt > 0x1000) {
        int txt_len = (int)strlen(txt);
        nk_draw_text(&ui->ctx->overlay,
                     nk_rect(sx + 6.0f, sy + 6.0f, sw - 12.0f, sh - 12.0f), txt,
                     txt_len, ui->ctx->style.font, nk_rgba(0, 0, 0, 0), col);
      }
    }

    nk_finish_buffer(ui->ctx, &ui->ctx->overlay);
  }

  if (ui->L) {
    scripting_call_ui_render(ui->L);
  }

  nk_sdl_render(NK_ANTI_ALIASING_ON);
}
