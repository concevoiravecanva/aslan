#ifndef NVG_WIDGETS_H
#define NVG_WIDGETS_H

#include "../vendor/nanovg/nanovg.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Widget Context - minimal state for primitive immediate mode GUI
typedef struct {
  NVGcontext *vg;

  // Input state
  float mouse_x;
  float mouse_y;
  bool mouse_down; // Is Primary mouse button held?
  bool mouse_down_prev;
  bool mouse_clicked;  // Was Primary mouse button clicked this frame?
  bool mouse_released; // Was Primary mouse button released this frame?
  float scroll_y;      // Scroll wheel delta this frame

  // Keyboard state
  char text_input[32]; // UTF-8 text input this frame
  bool key_backspace;  // Was backspace pressed?
  bool key_enter;      // Was enter pressed?

  // UI State
  uint32_t hot_id;    // Widget mouse is hovering
  uint32_t active_id; // Widget currently being interacted with (e.g. clicked)
  uint32_t focus_id;  // Text input focus

  // Fonts
  int font_main;
  int font_icons;

} NvgWidgetCtx;

// Initialize the widget context
void nvgw_init(NvgWidgetCtx *ctx, NVGcontext *vg);

// Setup fonts to use for rendering
void nvgw_set_fonts(NvgWidgetCtx *ctx, int font_main, int font_icons);

// Update input state per frame
void nvgw_input_begin(NvgWidgetCtx *ctx);
void nvgw_input_mouse(NvgWidgetCtx *ctx, float mx, float my, bool mdown,
                      float scroll_y);
void nvgw_input_keyboard(NvgWidgetCtx *ctx, const char *text_input,
                         bool backspace, bool enter);
void nvgw_input_end(NvgWidgetCtx *ctx);

// Generate generic ID based on string or pointer
uint32_t nvgw_id(const void *data, int len);
uint32_t nvgw_id_str(const char *str);

// --- Widgets ---

// Label / Text
void nvgw_label(NvgWidgetCtx *ctx, float x, float y, float w, float h,
                const char *text, int align, NVGcolor color, float font_size);

// Button. Returns true if clicked.
bool nvgw_button(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                 float h, const char *text);
bool nvgw_button_icon(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                      float h, const char *icon_utf8);

// Panel / Background
void nvgw_panel(NvgWidgetCtx *ctx, float x, float y, float w, float h,
                NVGcolor bg_color, float corner_radius);

// Single-line Text Input. Returns true if Enter was pressed or focus lost
// (editing finished).
bool nvgw_text_input(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                     float h, char *buffer, size_t buffer_size,
                     float font_size);

// Checkbox. Returns true if toggled. Modifies *checked.
bool nvgw_checkbox(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w,
                   float h, const char *text, bool *checked);

// Scroll Area (applies scissor and handles scroll delta)
// Returns the new content_offset_y
void nvgw_begin_scroll_area(NvgWidgetCtx *ctx, float x, float y, float w,
                            float h, float content_h, float *offset_y);
void nvgw_end_scroll_area(NvgWidgetCtx *ctx);

#endif // NVG_WIDGETS_H

// Color Picker (simple version). Returns true if color was changed.
bool nvgw_color_picker(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w, float h, NVGcolor *color);

// Date Picker (simple version). Returns true if date was changed.
bool nvgw_date_picker(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w, float h, int *year, int *month, int *day);

// Dropdown Select. Returns true if an option was selected.
bool nvgw_dropdown_select(NvgWidgetCtx *ctx, uint32_t id, float x, float y, float w, float h, const char **options, int option_count, int *selected_index);
