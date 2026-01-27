#ifndef UI_HOST_H
#define UI_HOST_H

#include "../core/engine.h"
#include "recents.h"
#include <SDL.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <string.h>

// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear configuration
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_MEMSET memset
#define NK_MEMCPY memcpy
#include "../../include/nuklear.h"

// -----------------------------------------------------------------------------
// Canvas Tool Modes
// -----------------------------------------------------------------------------

typedef enum {
  CANVAS_TOOL_SELECT = 0,
  CANVAS_TOOL_FORM_RECT,
  CANVAS_TOOL_FORM_ELLIPSE,
  CANVAS_TOOL_FORM_TRIANGLE,
  CANVAS_TOOL_FORM_LINE,
  CANVAS_TOOL_TEXT,
  CANVAS_TOOL_NOTE,
  CANVAS_TOOL_IMAGE,
  CANVAS_TOOL_PEN
} CanvasToolMode;

// -----------------------------------------------------------------------------
// Canvas Tool Categories and Sub-options
// -----------------------------------------------------------------------------

typedef enum {
  CANVAS_CAT_SELECT = 0,
  CANVAS_CAT_SHAPES,
  CANVAS_CAT_TEXT,
  CANVAS_CAT_MEDIA
} CanvasCategory;

typedef enum {
  CANVAS_SUB_NONE = 0,
  // Shapes
  CANVAS_SUB_SHAPE_RECT,
  CANVAS_SUB_SHAPE_ELLIPSE,
  CANVAS_SUB_SHAPE_TRIANGLE,
  CANVAS_SUB_SHAPE_LINE,
  // Text
  CANVAS_SUB_TEXT_HEADING,
  CANVAS_SUB_TEXT_BODY,
  CANVAS_SUB_TEXT_STICKY,
  // Media
  CANVAS_SUB_MEDIA_IMAGE,
  CANVAS_SUB_MEDIA_VIDEO,
  CANVAS_SUB_MEDIA_AUDIO
} CanvasSubOption;

// -----------------------------------------------------------------------------
// Canvas Interaction State (stored per-canvas in UIHost)
// -----------------------------------------------------------------------------

typedef struct {
  // Current tool
  CanvasToolMode tool;

  // Categorized toolbar state
  CanvasCategory active_cat;
  int show_sub_menu;

  // Camera state
  float cam_x, cam_y;
  float zoom;

  // Canvas bounds (set during render, used in events)
  float bounds_x, bounds_y, bounds_w, bounds_h;
  int mouse_in_canvas;

  // Pan state
  int is_panning;
  int space_held;
  float pan_start_x, pan_start_y;
  float cam_start_x, cam_start_y;

  // Drawing/creating state
  int is_creating;
  float create_start_x, create_start_y; // World coords
  float create_end_x, create_end_y;     // World coords

  // Dragging state
  int is_dragging_node;
  float drag_offset_x, drag_offset_y; // Offset from node origin

  // Mouse state for click detection
  int mouse_just_pressed;  // Left button just pressed this frame
  int mouse_just_released; // Left button just released this frame

  // Forms menu state (Legacy, will be replaced by active_cat logic)
  int show_forms_menu;

  // Pending element creation (from toolbar click)
  int pending_create; // 0=none, 1=text, 2=note, 3=image, 10+shape_index=form

  // Resize state
  int is_resizing;
  int resize_handle; // Which handle: 0=SE, 1=SW, 2=NW, 3=NE
  float resize_start_w, resize_start_h;
  float resize_start_x, resize_start_y;           // Mouse start pos
  float resize_start_node_x, resize_start_node_y; // Node start pos

  // Target canvas block
  uint32_t active_canvas_id;
  Block *active_block; // Direct pointer for convenience

  // Pen Tool State
  uint32_t pen_color; // RGBA
  float pen_thickness;
  int pen_is_marker;             // 0=Pen, 1=Marker
  struct Stroke *current_stroke; // Active stroke being drawn
  uint32_t stroke_block_id;      // Block ID where stroke started
} CanvasInteraction;

// -----------------------------------------------------------------------------
// UI Host Structure
// -----------------------------------------------------------------------------

#include "../media/ffmpeg_pipe.h"

typedef struct UIHost {
  // Nuklear context
  struct nk_context *ctx;
  SDL_Renderer *renderer;
  lua_State *L;

  // Thumbnail Cache for Images/Videos
  ThumbnailCache thumb_cache;
  struct nk_font_atlas *atlas;

  // Fonts
  struct nk_font *font_inter; // Default UI font
  struct nk_font *font_mono;  // Monospace for text blocks
  struct nk_font *font_h1;    // Heading 1 (28pt)
  struct nk_font *font_h2;    // Heading 2 (24pt)
  struct nk_font *font_h3;    // Heading 3 (18pt)
  struct nk_font *font_icons; // Remix Icons
  float font_size_px;

  // Selection state
  Node *selected_node; // Selected node in canvas/design block

  // Status message
  char status[128];

  // Screen state: 0=Home, 1=Editor
  int screen;

  // Theme: 0=Light, 1=Dark
  int dark_mode;

  // Tool mode: 0=select, 1=create rect, 2=create text
  int tool_mode;

  // Current Window Dimensions (updated in ui_render)
  int win_width;
  int win_height;

  // Modal states
  int show_new_page_modal;
  char new_page_name[64];

  int show_save_modal;
  char save_path[256];

  // Block editing state
  uint32_t editing_block_id;  // Block being edited (0=none)
  uint32_t expanded_block_id; // Block currently in full-screen mode (0=none)
  int editing_cursor_pos;
  int text_input_active;

  // Canvas node text editing
  Node *editing_node;       // Node being text-edited (NULL=none)
  char edit_buffer[512];    // Text buffer for editing
  int show_text_edit_popup; // Show text edit popup

  // Canvas/Design interaction
  float drag_start_x, drag_start_y;
  int is_dragging;
  int is_resizing;

  // Scroll state
  float scroll_y;
  float scroll_target_y;

  // Block insertion menu
  int show_insert_menu;
  float insert_menu_x, insert_menu_y;

  // Recent documents
  Recents recents;

  // Canvas interaction state
  CanvasInteraction canvas_state;

  // Design interaction state (simpler than canvas - no pan/zoom/pen)
  struct DesignInteraction {
    int show_forms_menu;
    int pending_create; // 0=none, 1=text, 2=image, 10+=forms
    int mouse_just_pressed;
    int mouse_just_released;
    int is_dragging_node;
    float drag_offset_x, drag_offset_y;
    int is_resizing;
    int resize_handle;
    float resize_start_w, resize_start_h;
    float resize_start_x, resize_start_y;
    float resize_start_node_x, resize_start_node_y;
    uint32_t active_design_id;
    Block *active_block;
  } design_state;

  // Video block interaction state
  struct {
    uint32_t active_video_id;
    Block *active_block;
    int mouse_just_pressed;
    int pending_import;     // 1 if import dialog should open
    int pending_transcribe; // 1 if transcription should start
  } video_state;

  // Audio block interaction state
  struct {
    Block *active_block; // Currently focused audio block
    int mouse_just_pressed;
  } audio_state;

  // Mouse state for canvas
  float mouse_x, mouse_y;
  int mouse_buttons; // Bitmask: 1=left, 2=middle, 4=right

  // Table interaction state
  struct {
    uint32_t active_block_id;
    int show_col_menu;
    int show_col_settings;
    int show_footer_menu;
    int active_col_idx;
    int active_row_idx;  // Keyboard navigation
    int is_editing_cell; // Prevent nav while typing
    float menu_pos_x, menu_pos_y;
    char rename_buffer[64];
  } table_state;

} UIHost;

// -----------------------------------------------------------------------------
// UI Lifecycle
// -----------------------------------------------------------------------------

UIHost *ui_init(SDL_Window *win, SDL_Renderer *renderer, lua_State *L);
void ui_shutdown(UIHost *ui);

// -----------------------------------------------------------------------------
// UI Event Handling & Rendering
// -----------------------------------------------------------------------------

void ui_handle_event(UIHost *ui, SDL_Event *evt);
void ui_render(UIHost *ui, Engine *engine, int win_w, int win_h);

// -----------------------------------------------------------------------------
// Block Rendering Functions
// -----------------------------------------------------------------------------

void ui_render_block(UIHost *ui, Engine *engine, struct nk_context *ctx,
                     Page *page, Block *block, float width);

void ui_render_block_text(UIHost *ui, struct nk_context *ctx, Block *block,
                          float width);
void ui_render_block_heading(UIHost *ui, struct nk_context *ctx, Block *block,
                             float width, int level);
void ui_render_block_separator(UIHost *ui, struct nk_context *ctx, Block *block,
                               float width);
void ui_render_block_table(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width);
void ui_render_block_canvas(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width);
void ui_render_block_design(UIHost *ui, Engine *engine, struct nk_context *ctx,
                            Block *block, float width);
void ui_render_block_video(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width);
void ui_render_block_audio(UIHost *ui, Engine *engine, struct nk_context *ctx,
                           Block *block, float width);
void ui_render_block_lua(UIHost *ui, Engine *engine, struct nk_context *ctx,
                         Block *block, float width);

// -----------------------------------------------------------------------------
// UI Components
// -----------------------------------------------------------------------------

void ui_render_top_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                       int win_w);
void ui_render_bottom_bar(UIHost *ui, Engine *engine, struct nk_context *ctx,
                          int win_w, int win_h);
void ui_render_home(UIHost *ui, Engine *engine, struct nk_context *ctx,
                    int win_w, int win_h);
void ui_render_insert_menu(UIHost *ui, Engine *engine, struct nk_context *ctx);

// -----------------------------------------------------------------------------
// Remix Icon Codepoints (subset)
// -----------------------------------------------------------------------------

#define ICON_TEXT "\xEE\xB4\x8F"      // ri-file-text-line
#define ICON_H1 "\xEE\xB7\xA6"        // ri-h-1
#define ICON_H2 "\xEE\xB7\xA7"        // ri-h-2 (Approx, guessed +1)
#define ICON_SEPARATOR "\xEF\x83\x9E" // ri-separator
#define ICON_CODE "\xEE\xBA\xA9"      // ri-code-line
#define ICON_TABLE "\xEF\x87\x9A"     // ri-table-2
#define ICON_CANVAS "\xEE\xA9\xBA"    // ri-artboard-2-line
#define ICON_DESIGN "\xEE\xBA\x83"    // ri-layout-4-line
#define ICON_VIDEO "\xEF\x8A\x82"     // ri-video-line
#define ICON_MIC "\xEE\xBD\x9E"       // ri-mic-line (audio block)
#define ICON_VOLUME "\xEF\x84\x8B"    // ri-volume-up-line (voice note)

#define ICON_ADD "\xEE\xA8\x93"             // ri-add-line
#define ICON_STOP "\xEF\x83\xA7"            // ri-stop-line
#define ICON_DELETE "\xEE\xB0\xAA"          // ri-delete-bin-line
#define ICON_PLAY "\xEF\x80\x8B"            // ri-play-line
#define ICON_PAUSE "\xEE\xBF\x98"           // ri-pause-line
#define ICON_EXPAND "\xEE\xA9\x8B"          // ri-arrow-down-s-line
#define ICON_COLLAPSE "\xEE\xA9\xB8"        // ri-arrow-up-s-line
#define ICON_HOME "\xEE\xB8\x9D"            // ri-home-4-line
#define ICON_LUA "\xEF\x87\xB6"             // ri-terminal-box-line
#define ICON_FULLSCREEN "\xEE\xB6\x9C"      // ri-fullscreen-line
#define ICON_FULLSCREEN_EXIT "\xEE\xB6\x9A" // ri-fullscreen-exit-line

// Canvas tools icons
#define ICON_CURSOR "\xEE\xAA\x8A"    // ri-cursor-line (select tool)
#define ICON_SHAPE "\xEE\xAD\xA4"     // ri-checkbox-blank-line (rect tool)
#define ICON_PENCIL "\xEE\xBF\x8F"    // ri-pencil-line (pen tool)
#define ICON_TEXT_TOOL "\xEE\x87\x98" // ri-text (text tool)
#define ICON_STICKY "\xEF\x83\xBC"    // ri-sticky-note-line (sticky note)
#define ICON_HAND "\xEE\xB7\xBB"      // ri-hand (pan tool)

// Table column type icons
#define ICON_COL_TEXT "\xEE\x87\x98"     // ri-text
#define ICON_COL_NUMBER "\xEE\xB7\xA4"   // ri-hashtag
#define ICON_COL_CHECKBOX "\xEE\xAD\xB6" // ri-checkbox-line
#define ICON_COL_SELECT "\xEE\xA9\x8B"   // ri-arrow-down-s-line
#define ICON_COL_DATE "\xEE\xAA\xB2"     // ri-calendar-line
#define ICON_COL_RELATION "\xEE\xB0\xB5" // ri-links-line
#define ICON_COL_FORMULA "\xEE\xB6\xBA"  // ri-functions

// Sort icons
#define ICON_SORT_ASC "\xEE\xA9\xB8"  // ri-arrow-up-s-line
#define ICON_SORT_DESC "\xEE\xA9\x8B" // ri-arrow-down-s-line

#endif // UI_HOST_H
