#ifndef UI_HOST_H
#define UI_HOST_H

#include "../core/engine.h"
#include "../vendor/nanovg/nanovg.h"
#include "nvg_widgets.h"
#include "recents.h"
#include "ui_shared.h"
#include <SDL.h>
#include <string.h>

// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Helper functions (Shared between Host and Design)
int open_image_file_dialog(char *out_path, size_t path_size);
int copy_image_to_assets(const char *src_path, const char *doc_dir,
                         char *out_rel_path, size_t out_size);
int mac_get_image_size(const char *path, int *w, int *h);


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
  CANVAS_CAT_MEDIA,
  CANVAS_CAT_BRUSH,
  CANVAS_CAT_MARKER,
  CANVAS_CAT_COLOR,
  CANVAS_CAT_CHANGE
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
  CANVAS_SUB_MEDIA_IMAGE
} CanvasSubOption;

// -----------------------------------------------------------------------------
// Unified Editor & Input State
// -----------------------------------------------------------------------------

typedef enum {
  INTERACTION_NONE = 0,
  INTERACTION_PAN,
  INTERACTION_CREATE_NODE,
  INTERACTION_DRAG_NODE,
  INTERACTION_RESIZE_NODE,
  INTERACTION_ROTATE_NODE,
  INTERACTION_CROP_IMAGE,
  INTERACTION_BEZIER_POINT,
  INTERACTION_BEZIER_CTRL,
  INTERACTION_DRAG_GUIDE,
  INTERACTION_DRAW_PEN
} InputMode;

typedef struct {
  // Tools & Preferences
  CanvasToolMode active_tool;
  CanvasCategory active_category;
  int show_sub_menu;
  
  uint32_t selected_color;
  float pen_thickness;
  int pen_is_marker;

  // View & Camera
  float cam_x, cam_y;
  float zoom;
  
} EditorState;

typedef struct {
  InputMode mode;

  // Pointer context
  float mouse_x, mouse_y;
  int mouse_just_pressed;
  int mouse_just_released;
  
  // View context
  float bounds_x, bounds_y, bounds_w, bounds_h; 
  int mouse_in_view;                            
  int space_held;
  int pan_from_menu;

  // Generic drag/interaction anchors
  float start_x, start_y;             // World coords where interaction began
  float end_x, end_y;                 // E.g. create_end_x
  float start_cam_x, start_cam_y;     // Camera pos at start
  float drag_offset_x, drag_offset_y; // Offset from node origin
  float drag_press_x, drag_press_y;   // Threshold
  float start_w, start_h;             // Node bounds at start
  float start_node_x, start_node_y;   // Node pos at start
  float start_angle;                  // Rotation at start
  float start_node_rotation;          
  float start_font_size;
  int active_handle;                  // e.g., Resize handle index or guide type
  int active_idx;                     // e.g., Bezier point idx, or guide idx
  float crop_start_node_x, crop_start_node_y;
  int bezier_mode;
  int dragging_point_idx;
  int dragging_cp_idx;
  int dragging_guide_type;
  int dragging_guide_idx;
  float scale;
  
  // Creation state
  int pending_create; // 0=none, 1=text, 2=note, 3=image, 10+shape_index=form

  // Contextual targets
  uint32_t active_block_id;
  struct Block *active_block; 
  struct Stroke *current_stroke;    
  struct Node *current_stroke_node; 
  uint32_t stroke_block_id;         
  
  // Guides
  float snap_guide_x;
  float snap_guide_y;
  
  // Color selection
  int show_color_picker_popup;
} InputState;
// -----------------------------------------------------------------------------
// UI Cursors
// -----------------------------------------------------------------------------

typedef enum {
  UI_CURSOR_ARROW = 0,
  UI_CURSOR_IBEAM,
  UI_CURSOR_WAIT,
  UI_CURSOR_CROSSHAIR,
  UI_CURSOR_WAITARROW,
  UI_CURSOR_SIZENWSE,
  UI_CURSOR_SIZENESW,
  UI_CURSOR_SIZEWE,
  UI_CURSOR_SIZENS,
  UI_CURSOR_SIZEALL,
  UI_CURSOR_NO,
  UI_CURSOR_HAND,
  UI_CURSOR_COUNT
} UICursorType;

void ui_set_cursor(struct UIHost *ui, UICursorType type);
void ui_commit_cursor(struct UIHost *ui);

// -----------------------------------------------------------------------------
// UI Host Structure
// -----------------------------------------------------------------------------

typedef struct UIHost {
  // UI Context
  NVGcontext *vg;



  // NanoVG Widgets context
  NvgWidgetCtx nvgw;

  // Fonts (NanoVG font IDs)
  int font_inter; // Default UI font
  int font_title; // Page Title (Extra Bold)
  int font_h1;    // Heading 1 (28pt Bold)
  int font_icons; // Remix Icons
  float font_size_px;

  // Font sizes for text nodes (12, 14, 16, 20, 24, 32, 48, 64, 96)
  int font_inter_sizes[10];

  // Selection state
  Node *selected_node;      // Primary selected node
  Node *selected_nodes[64]; // Multi-selection set (design view)
  int selected_count;       // Number of entries in selected_nodes[]

  // Status message
  char status[128];

  // Screen state: 0=Home, 1=Editor
  int screen;

  // Theme: 0=Light, 1=Dark
  int dark_mode;

  // Engine reference (for document path access)
  Engine *engine;

  // Tool mode: 0=select, 1=create rect, 2=create text
  int tool_mode;

  // Current Window Dimensions (updated in ui_render)
  int win_width;
  int win_height;

  // Custom imported fonts
#define ASLAN_MAX_CUSTOM_FONTS 16
  struct {
    char name[64];   // NanoVG font name
    char path[256];  // Original file path (for display)
  } custom_fonts[ASLAN_MAX_CUSTOM_FONTS];
  int custom_font_count;

  // Cursor handling
  UICursorType cursor_type;

  // Modal states
  int show_new_page_modal;
  char new_page_name[64];

  int show_save_modal;
  char save_path[256];

  // Document settings popup
  int show_doc_menu;
  AslanRect doc_menu_rect;

  // Block editing state
  uint32_t active_block_id;   // Block currently active/selected (showing icons)
  uint32_t editing_block_id;  // Block being edited (0=none)
  uint32_t expanded_block_id; // Block currently in full-screen mode (0=none)
  int editing_cursor_pos;
  int text_input_active;
  int scroll_inhibited; // 1 = UI widget is handling scroll, skip page scroll

  // Block drag-and-drop
  struct {
    int is_active;     // 1 = mouse held on handle, waiting or dragging
    int threshold_met; // 1 = moved past dead zone, actually dragging
    uint32_t dragged_block_id;
    float start_x, start_y; // Where drag started
    float mouse_x, mouse_y; // Current mouse during drag
    int drop_valid;
    uint32_t drop_target_id;
    int drop_position; // 0=above, 1=below, 2=left, 3=right
  } block_drag;

  // Node clipboard (Cmd+C / Cmd+V)
  struct {
    int has_data;
    NodeType type;
    char name[64];
    float x, y, width, height;
    float rotation, corner_radius;
    uint32_t fill_color, stroke_color;
    float stroke_width;
    char text[256];
    float font_size;
    int font_type;
    int line_start_style, line_end_style;
    char image_path[256];
  } clipboard_node;

  // Block clipboard (Cmd+C / Cmd+V on text/heading blocks)
  struct {
    int has_data;
    BlockType type;
    char text[4096];
    int heading_level; // For BLOCK_HEADING
  } clipboard_block;

  // Canvas node text editing
  Node *editing_node;               // Node being text-edited (NULL=none)
  char edit_buffer[512];            // Text buffer for editing
  int show_text_edit_popup;         // Show text edit popup
  AslanRect editing_node_rect; // Screen bounds for overlay editor

  // Design bottom panel tab (0=Properties, 1=Layers)
  int design_bottom_tab;

  // Linked text row navigator: which table row to display in design (0-based)
  int design_linked_row_idx;
  int design_smart_layout_sel;

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
  EditorState editor;
  InputState input;
  // Mouse state for canvas
  float mouse_x, mouse_y;
  float abs_offset_x, abs_offset_y; // Current translation offset
  int mouse_buttons; // Bitmask: 1=left, 2=middle, 4=right

  // Table interaction state
  struct {
    uint32_t active_block_id;
    int show_search_bar;
    int show_col_menu;
    int show_col_settings;
    int show_footer_menu;
    int show_csv_menu;
    int show_select_popup;     // Select cell popup open
    uint64_t select_popup_row; // Row ID for Select popup
    int select_popup_col;
    float select_popup_x;
    float select_popup_y;
    int editing_select_option;   // Which select option is being renamed (-1 = none)
    uint32_t last_option_click_time; // Time of last click on a select option
    int last_option_click_idx;       // Index of last clicked option
    int active_col_idx;
    int active_row_idx;  // Keyboard navigation
    int is_editing_cell; // Prevent nav while typing

    // Multi-cell selection
    int is_selecting;
    int sel_start_col_idx;
    int sel_start_row_idx;
    int sel_end_col_idx;
    int sel_end_row_idx;

    float menu_pos_x, menu_pos_y;
    char rename_buffer[64];
    char option_buffer[64];         // Buffer for adding new Select options
    char date_buffer[32];           // Persistent buffer for date editing
    int date_field_active;          // Trigger focus on first frame
    int selected_col_idx;           // Column header selected (for delete X)
    int renaming_col_idx;           // Column header being renamed inline
    int resizing_col;               // Column being resized (-1 = none)
    float resize_start_x;           // Mouse X at resize start
    float resize_start_w;           // Column width at resize start
    char filter_buffer[128];        // Search/filter text input
    char formula_buffer[512];       // Formula expression editor (for FORMULA /
                                    // FORMULA)
    int is_editing_formula;         // 1 if formula bar is active (for
                                    // click-to-reference)
    int show_formula_popup;         // Formula editor popup open
    int formula_popup_col;          // Column index for formula popup
    int show_formula_help;          // Show formula help popup
    int dragging_col;               // Column being dragged (-1 = none)
    int drop_target_col;            // Column drop target (-1 = none)
    float drag_col_start_x;         // Mouse X at drag start
    int dragging_row;               // Row being dragged (-1 = none)
    int drop_target_row;            // Row drop target (-1 = none)
    float drag_row_start_y;         // Mouse Y at drag start
    int active_color_picker_option; // Option index for color picker (-1 = none)
    // Date picker popup state
    int show_date_picker;     // 1 = date picker popup is open
    int date_picker_year;     // Currently viewed year
    int date_picker_month;    // Currently viewed month (0-11)
    uint64_t date_picker_row; // Row ID for the date being edited
    int date_picker_col;      // Column index for the date being edited
    float date_picker_x;      // Popup X position
    float date_picker_y;      // Popup Y position
    // Link editor popup state
    int show_link_popup;         // 1 = link edit popup open
    uint64_t link_popup_row;     // Row being edited
    int link_popup_col;          // Column being edited
    float link_popup_x;          // Popup X
    float link_popup_y;          // Popup Y
    char link_label_buffer[256]; // Editable label
    char link_url_buffer[1024];  // Editable URL
    int context_menu_mode;       // 1 = column, 2 = row
    int context_menu_col;        // which column
    uint64_t context_menu_row;   // which row
    int show_type_menu;          // 1 = show "+" type menu
    float type_menu_x, type_menu_y;
    uint32_t last_hdr_click_time; // SDL_GetTicks() of last header click
    int last_hdr_click_col;       // Column index of last header click
    // Per-column widths (0 = unset → use default 120px)
    float col_widths[64];
    // Scroll offsets
    float row_scroll_x;
    float row_scroll_y;
    int dragging_scroll_x; // 1 = dragging horizontal scrollbar
    int dragging_scroll_y; // 1 = dragging vertical scrollbar
    float scroll_drag_start; // starting offset when drag began
    // Footer aggregation type per column: 0=sum 1=avg 2=count 3=min 4=max
    int footer_agg[64];
    // Footer aggregation popup
    int show_footer_popup;    // column index, -1 = none
    float footer_popup_x, footer_popup_y;
  } table_state;

  // Gantt view state (per session — not persisted)
  struct {
    int64_t
        view_start_day; // Days since Unix epoch for leftmost column (0 = auto)
    float day_w;        // Pixels per day (0 = default 40px)
  } gantt_state;

  // Artboard rects used for scissor + border rendering
  AslanRect artboard_clip; // = art_rect, set before layout
  AslanRect
      artboard_rect; // same art_rect, used for border stroke after group

  // Floating color picker (avoids ROM-mode issue from combo popups inside
  // groups)
  int color_picker_open;      // 1 = picker window is visible
  uint32_t *color_picker_ptr; // pointer to the RGBA32 color being edited
  float color_picker_x;       // screen position of the picker window
  float color_picker_y;
  float color_picker_h;       // current hue   [0..1]
  float color_picker_s;       // current sat   [0..1]
  float color_picker_v;       // current value [0..1]
  float color_picker_a;       // current alpha [0..1]
  int   color_picker_drag;    // 0=none 1=sv-square 2=hue-bar 3=alpha-bar

  // Persistent text-input buffers for property panel (survives frame resets)
  char  prop_input_bufs[20][64]; // indexed by nvgw_id(node, N) slot N
  void *prop_input_node;         // which node owns these buffers (NULL = stale)
  float prop_scroll_y;           // vertical scroll offset for Properties panel

  // Layer sidebar state
  uint32_t renaming_node_id;
  char node_rename_buffer[64];
  int rename_needs_focus; // 1 = set focus to editor on first frame
  uint32_t sidebar_drag_node_id;
  uint32_t sidebar_drop_target_id;
  uint32_t hovered_node_id;
  int sidebar_drag_started;
  int sidebar_drag_drop_above; // 1 = insert before target, 0 = insert after

  // Sidebar state: 0=Properties/Layers, 1=Shape Selection
  int sidebar_mode;

  // Right-click context menu for canvas/design nodes
  int show_context_menu;
  float context_menu_x, context_menu_y;
  Node *context_menu_node;

  // Delayed export
  int pending_export;
  int pending_export_format; // 0 = PNG, 1 = JPEG
  uint32_t pending_export_block_id;
  char pending_export_path[1024];

  // Professional Numeric Input State
  char ds_num_edit_buf[64];
  void *ds_num_edit_ptr;
  float ds_num_orig_val;

  // Event-driven rendering: redraw only when needed
  int needs_redraw;         // 1 = re-render needed, 0 = idle
  int has_active_animation; // 1 = cursor blink, transitions, etc.

  // Deferred image dialog (Emscripten: must open AFTER frame completes)
  int      deferred_image_dialog;   // 1 = open dialog after next renderer_end
  float    deferred_image_nx;
  float    deferred_image_ny;
  uint32_t deferred_image_block_id;
  uint32_t deferred_image_node_id;  // 0 = create new node, else fill existing

} UIHost;

// -----------------------------------------------------------------------------
// UI Lifecycle
// -----------------------------------------------------------------------------

UIHost *ui_init(SDL_Window *win);
void ui_shutdown(UIHost *ui);
void ui_request_redraw(UIHost *ui); // Mark UI as needing redraw

// -----------------------------------------------------------------------------
// Shared Color Constants (defined in ui_host.c)
// -----------------------------------------------------------------------------

extern AslanColor COLOR_BG;
extern AslanColor COLOR_PANEL;
extern AslanColor COLOR_BORDER;
extern AslanColor COLOR_TEXT;
extern AslanColor COLOR_TEXT_DARK;
extern AslanColor COLOR_TEXT_DIM;
extern AslanColor COLOR_ACCENT;

// -----------------------------------------------------------------------------
// Shared UI Components
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// UI Event Handling & Rendering
// -----------------------------------------------------------------------------

void ui_handle_event(UIHost *ui, SDL_Event *evt);
void ui_host_handle_file_drop(UIHost *ui, const char *file_path, int mx,
                              int my);
void ui_host_import_csv(UIHost *ui, Engine *engine);
int open_csv_file_dialog(char *out_path, size_t path_size);
int save_csv_file_dialog(char *out_path, size_t path_size, const char *default_name);
void ui_host_export_svg(UIHost *ui, Engine *engine);
void ui_host_export_svg_merge(UIHost *ui, Engine *engine,
                              struct AslanTable *linked_tbl);
void ui_host_export_png_jpeg(UIHost *ui, Engine *engine, int is_jpeg);
void ui_host_export_pdf(UIHost *ui, Engine *engine);
const char *ui_host_import_font(UIHost *ui, const char *path);
const char *ui_host_open_and_import_font(UIHost *ui);
void ui_host_copy_node(UIHost *ui, Engine *engine);
void ui_host_paste_node(UIHost *ui, Engine *engine);
void ui_begin_frame(UIHost *ui);
void ui_update(UIHost *ui, Engine *engine);
void ui_render(UIHost *ui, Engine *engine, int win_w, int win_h);
void ui_render_document_menu(UIHost *ui);

// -----------------------------------------------------------------------------
// Block Rendering Functions
// -----------------------------------------------------------------------------

void ui_render_block(UIHost *ui, Engine *engine,
                     Page *page, Block *block, float width);

void ui_render_block_text(UIHost *ui, Engine *engine,
                          Page *page, Block *block, float width);
void ui_render_block_heading(UIHost *ui, Block *block,
                             float width, int level);
void ui_render_block_table(UIHost *ui, Engine *engine, Block *block,
                           float width, float height);
void ui_render_table_topbar_actions(UIHost *ui, Engine *engine,
                                    Block *block,
                                    float right_edge, float bar_h);
// ui_render_block_design is in ui_design.c
void ui_render_chart_node(UIHost *ui, Node *node,
                          AslanRect bounds, Page *page);
void ui_render_chart_node_config_bar(UIHost *ui, Engine *engine,
                                     Node *node,
                                     AslanRect bounds, Page *page);
void ui_render_chart_impl(UIHost *ui, ChartData *cd,
                          AslanRect bounds, Page *page);

// Layer panel components
void ui_render_layer_inspector(UIHost *ui,
                               Block *block);

// Image Utilities
int open_image_file_dialog(char *out_path, size_t path_size);
int copy_image_to_assets(const char *src_path, const char *dest_dir,
                         char *out_rel_path, size_t rel_path_size);
int mac_get_image_size(const char *path, int *w, int *h);

// -----------------------------------------------------------------------------
// UI Components
// -----------------------------------------------------------------------------

void ui_render_top_bar(UIHost *ui, Engine *engine,
                       int win_w);
void ui_render_insert_menu(UIHost *ui, Engine *engine);

// -----------------------------------------------------------------------------
// Remix Icon Codepoints (subset)
// -----------------------------------------------------------------------------

#define ICON_TEXT "\xEF\x88\x81"   // \uf201
#define ICON_H1 "\xEE\xB7\xA6"     // \uede6
#define ICON_CODE "\xEE\xAE\xA9"   // \ueba9
#define ICON_TABLE "\xEF\x87\x9A"  // ri-table-2
#define ICON_CANVAS "\xEE\xA9\xBA" // ri-artboard-2-line
#define ICON_DESIGN "\xEE\xBA\x83" // ri-layout-4-line

#define ICON_ADD "\xEE\xA8\x93"             // ri-add-line
#define ICON_STOP "\xEF\x83\xA7"            // ri-stop-line
#define ICON_DELETE "\xEE\xB0\xAA"          // ri-delete-bin-line
#define ICON_PLAY "\xEF\x80\x8B"            // ri-play-line
#define ICON_PAUSE "\xEE\xBF\x98"           // ri-pause-line
#define ICON_EXPAND "\xEE\xA9\x8B"          // ri-arrow-down-s-line
#define ICON_COLLAPSE "\xEE\xA9\xB8"        // ri-arrow-up-s-line
#define ICON_HOME "\xEE\xB8\x9D"            // ri-home-4-line
#define ICON_LUA "\xEE\xAE\xA9"             // \ueba9
#define ICON_CHART "\xEE\xBF\xBA"           // ri-pie-chart-2-line
#define ICON_FULLSCREEN "\xEE\xB6\x9C"      // ri-fullscreen-line
#define ICON_FULLSCREEN_EXIT "\xEE\xB6\x9A" // ri-fullscreen-exit-line
#define ICON_EYE "\xEE\xB2\xB5"             // ri-eye-line (U+ECB5)
#define ICON_EYE_OFF "\xEE\xB2\xB7"         // ri-eye-off-line (U+ECB7)
#define ICON_LOCK "\xEE\xB7\xD1"            // ri-lock-line
#define ICON_LOCK_UNLOCK "\xEE\xB7\xD9"     // ri-lock-unlock-line
#define ICON_MORE "\xEE\xAB\xA3"            // ri-settings-4-line
#define ICON_SETTINGS "\xEE\xBD\x92"        // ri-settings-3-line
#define ICON_VIEW_SETTINGS "\xEE\xB8\xA8"   // U+EE28  view settings gear

// Canvas tools icons
#define ICON_CURSOR "\xEE\xAA\x8A"      // ri-cursor-line (select tool)
#define ICON_SHAPE "\xEF\x83\xB0"       // ri-shape-2-line
#define ICON_BRUSH "\xEF\x81\x89"       // ri-quill-pen-fill
#define ICON_MARKER "\xEE\xBD\x9D"      // ri-pen-nib-fill
#define ICON_TEXT_TOOL "\xEE\x87\x98"   // ri-text (text tool)
#define ICON_STICKY "\xEF\x83\xBC"      // ri-sticky-note-line (sticky note)
#define ICON_HAND "\xEE\xB1\xA0"        // \uec60
#define ICON_IMAGE "\xEE\xB9\x85"       // ri-image-2-line
#define ICON_MAGIC "\xEE\xBF\x9E"       // ri-pen-nib-line
#define ICON_ICON "\xEF\x8E\xBF"        // ri-remixicon-line
#define ICON_LINKED_TEXT "\xEE\xBA\xB2" // \uEEB2 (linked/data text)
// Table column type icons
#define ICON_COL_TEXT "\xEF\x88\x81"     // \uf201
#define ICON_COL_NUMBER "\xEE\xBE\xAA"   // \uefaa
#define ICON_COL_CHECKBOX "\xEE\xAE\x85" // \ueb85
#define ICON_CHECK_FILL "\xEE\xAD\xB5"   // ri-checkbox-fill
#define ICON_COL_SELECT "\xEF\x91\x8B"   // \uf44b
#define ICON_COL_DATE "\xEE\xAC\xA7"     // \ueb27
#define ICON_COL_RELATION "\xEE\xB0\xB5" // ri-links-line
#define ICON_COL_FORMULA "\xEF\x93\x9C"  // \uf4dc
#define ICON_COL_FILE "\xEF\x84\x95"     // ri-file-line
#define ICON_COL_IMAGE "\xEE\xB9\x85"    // ri-image-2-line
#define ICON_COL_LINK "\xEF\x81\xA1"     // ri-link

// Sort icons
#define ICON_SORT_ASC "\xEE\xA9\xB8"  // ri-arrow-up-s-line
#define ICON_SORT_DESC "\xEE\xA9\x8B" // ri-arrow-down-s-line

// Table extras
#define ICON_SEARCH "\xEF\x83\x91"            // ri-search-line
#define ICON_EXPORT "\xEF\x90\xB6"            // \uf436
#define ICON_EXPAND_FULLSCREEN "\xEF\x90\xB6" // \uf436
#define ICON_UNCHECK "\xEE\xAD\xA4"           // ri-checkbox-blank-line
#define ICON_TABLE_LINE "\xEF\x83\xA4"        // ri-table-line
#define ICON_ADD_LINE "\xEE\xA8\x8E"          // ri-add-line

// Split view icons
#define ICON_SPLIT "\xEE\xBD\x9A"     // ri-layout-column-line
#define ICON_KANBAN "\xEE\xB8\xBC"    // ri-layout-masonry-line
#define ICON_CALENDAR "\xEE\xAC\xA7"  // ri-calendar-line (same as date)
#define ICON_DASHBOARD "\xEE\xAA\xB4" // ri-dashboard-line

#endif // UI_HOST_H
