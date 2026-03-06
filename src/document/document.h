#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// =============================================================================
// ASLAN DOCUMENT MODEL v3 - Unified Block-Based Architecture
// =============================================================================
//
// Philosophy: Single document type with mixed blocks (Notion-like)
// All content is a sequence of blocks within pages.
// Blocks can be: Text, Headings, Separators, Tables,
//                Canvas (infinite), Design (finite).
//
// =============================================================================

// Forward declarations
union SDL_Event;
struct Document;
struct Block;
struct Page;
struct lua_State;

// =============================================================================
// BLOCK TYPES
// =============================================================================

typedef enum {
  BLOCK_TEXT = 0,  // Monospace text (Kilo-style editor)
  BLOCK_HEADING_1, // H1 title
  BLOCK_TABLE,     // Notion-style table with views
  BLOCK_DESIGN,    // Finite design surface (Canva-style)

  BLOCK_COUNT
} BlockType;

// Table view modes
typedef enum {
  TABLE_VIEW_TABLE = 0, // Standard table grid
  TABLE_VIEW_GANTT = 1, // Gantt timeline (requires DATE + DURATION columns)
} TableViewMode;

// =============================================================================
// NODE TYPES (for Canvas/Design embedded content)
// =============================================================================

typedef enum {
  NODE_TYPE_ROOT = 0,
  NODE_TYPE_RECT = 1,
  NODE_TYPE_TEXT = 2,
  NODE_TYPE_IMAGE = 3,
  NODE_TYPE_FRAME = 4,   // Container frame (Figma-like)
  NODE_TYPE_GROUP = 5,   // Logical grouping
  NODE_TYPE_LINE = 6,    // Line/arrow
  NODE_TYPE_ELLIPSE = 7, // Circle/ellipse
  NODE_TYPE_TRIANGLE = 8,
  NODE_TYPE_OCTAGON = 9,
  NODE_TYPE_HEART = 10,
  NODE_TYPE_PENTAGON = 11,
  NODE_TYPE_HEXAGON = 12,
  NODE_TYPE_STAR = 13,
  NODE_TYPE_PATH = 14,
  NODE_TYPE_STROKE = 15,
  NODE_TYPE_CHART = 16,
  NODE_TYPE_TABLE_VIEW = 17  // Embedded live-table (read-only content, resizable)
} NodeType;

// -----------------------------------------------------------------------------
// PATH POINT STRUCTURE
// -----------------------------------------------------------------------------

typedef struct {
  float x, y;       // Anchor position (relative to node origin)
  float cp1x, cp1y; // Control point 1 (incoming)
  float cp2x, cp2y; // Control point 2 (outgoing)
  int type;         // 0 = Corner, 1 = Cubic Bezier
} NodePathPoint;

// =============================================================================
// NODE STRUCTURE (for Canvas/Design blocks)
// =============================================================================

typedef struct ChartData ChartData; // Forward declaration

typedef struct Node {
  uint32_t id;
  NodeType type;
  char name[64];

  // Spatial properties
  float x, y;
  float width, height;
  float rotation; // Degrees

  // Appearance
  uint32_t fill_color;   // RGBA
  uint32_t stroke_color; // RGBA
  float stroke_width;
  float corner_radius;
  float opacity;  // 0.0 to 1.0
  int lock_ratio; // Boolean

  // Advanced Appearance
  int has_shadow;
  float shadow_offset_x;
  float shadow_offset_y;
  float shadow_blur;
  uint32_t shadow_color; // RGBA

  int fill_type;         // 0 = Solid, 1 = Linear Gradient
  uint32_t fill_color_2; // Secondary color for gradients
  float gradient_angle;  // In degrees

  // Smart Container properties
  int clip_content;     // Hide children outside bounds
  int layout_type;      // 0 = None, 1 = Vertical Stack, 2 = Horizontal Stack
  float layout_padding; // Spacing between children

  // Text content (for NODE_TYPE_TEXT)
  char text[256];
  float font_size; // Default 20.0f
  int font_type;   // 0=Inter, 1=H1, 2=H2, 3=H3, 4=Mono

  // Advanced Typography
  int text_align;       // 0=Left, 1=Center, 2=Right
  float line_height;    // Multiplier, default 1.2f
  float letter_spacing; // In pixels, default 0.0f

  // Linked text (design only): if non-empty, display value from this column
  char linked_col_name[64];

  // Linked image (design only): if non-empty, use this column for image path
  char linked_image_col_name[64];

  // Line Styles
  int line_start_style; // 0=None, 1=Circle, 2=Arrow
  int line_end_style;   // 0=None, 1=Circle, 2=Arrow

  // Image path (for NODE_TYPE_IMAGE)
  char image_path[256];

  // Image Cropping (Masking)
  float image_scale;  // Zoom inside frame (1.0 = fit)
  float image_crop_x; // Offset X
  float image_crop_y; // Offset Y

  // Visibility
  int visible;
  int locked;
  int is_container;
  int folded;
  // Path data (for NODE_TYPE_PATH)
  NodePathPoint *path_points;
  int path_point_count;

  // Stroke data (for NODE_TYPE_STROKE)
  struct Stroke *stroke_data;

  // Chart data (for NODE_TYPE_CHART)
  ChartData *chart_data;

  // Linked block id (for NODE_TYPE_TABLE_VIEW: references the table block to display)
  uint32_t linked_block_id;

  // Hierarchy (linked list tree)
  struct Node *parent;
  struct Node *first_child;
  struct Node *next_sibling;
} Node;

// =============================================================================
// BLOCK PAYLOAD STRUCTURES
// =============================================================================

// Forward declare columnar table (implemented in aslan_table.h)
#ifndef ASLAN_TABLE_TYPEDEF
#define ASLAN_TABLE_TYPEDEF
typedef struct AslanTable AslanTable;
#endif

// --- Table Block Data ---
typedef struct {
  // UI state
  int active_row;
  int active_col;
  TableViewMode view_mode;
  int visible_rows; // Number of rows to display (0 = auto)
  bool full_width;  // If true, expand to page width
  bool show_search; // If true, show search/filter bar

  // Columnar engine (high-performance column-store)
  struct AslanTable *columnar;
} TableData;

// --- Canvas Block Data (Infinite) ---

typedef struct {
  float x, y;
} Point;

typedef enum {
  STROKE_TYPE_PEN = 0,
  STROKE_TYPE_MARKER,
  STROKE_TYPE_PENCIL
} StrokeType;

typedef struct Stroke {
  Point *points;
  int count;
  int capacity;
  uint32_t color; // RGBA
  float thickness;
  StrokeType type;
  struct Stroke *next;
} Stroke;

// (CanvasData removed — Canvas block type has been deleted)

// --- Design Block Data (Finite, Multi-Page) ---

// Single design page (slide)
typedef struct DesignPage {
  Node *root;          // Node tree root for this page
  uint32_t bg_color;   // Per-page background color
  int bg_type;         // 0=Solid, 1=Linear, 2=Radial
  uint32_t bg_color_2; // Secondary color for gradients

  // Grid overlay
  int grid_type; // 0=None, 1=Dots, 2=Lines, 3=Columns, 4=Isometric, 5=Custom
  float grid_spacing; // Custom grid spacing (default 40px)

  // Guide lines
  float h_guides[16]; // Y positions of horizontal guides
  float v_guides[16]; // X positions of vertical guides
  int h_guide_count;
  int v_guide_count;

  // Rulers
  int show_rulers; // 0=off, 1=on
} DesignPage;

typedef struct {
  DesignPage *pages; // Array of pages
  int page_count;    // Number of pages
  int page_capacity; // Allocated capacity
  int current_page;  // Currently displayed page index
  uint32_t next_node_id;
  int width, height; // Fixed dimensions in pixels (same for all pages)
  int units;         // 0=px, 1=cm, 2=in
  float zoom;        // View zoom
  uint32_t bg_color; // Default background color for new pages

  // Canvas mode toggle
  int no_limits; // 0 = fixed artboard (default), 1 = infinite canvas mode
  float view_x, view_y; // Pan offset (used when no_limits=1)

  // Linked data source table (Data-Merger functionality)
  uint32_t linked_table_id;

  // Legacy: single root for backwards compatibility (points to current page's
  // root)
  Node *root;
} DesignData;

// --- Chart Block Data (Data-Driven) ---

typedef enum { CHART_BAR = 0, CHART_LINE, CHART_PIE, CHART_GANTT } ChartType;

struct ChartData {
  ChartType type;
  uint32_t source_table_id; // ID of the source table block
  int label_col_idx;        // Column index for X-axis/Labels (-1 if none)
  int value_col_idx;        // Column index for Y-axis/Values (-1 if none)

  uint32_t color;   // Main chart color (RGBA)
  bool show_values; // Draw values on chart
};

// =============================================================================
// BLOCK STRUCTURE (Unified)
// =============================================================================

typedef struct Block {
  uint32_t id; // Unique block ID
  BlockType type;

  // Text content (for TEXT, HEADING_*, SEPARATOR, LUA, CALLOUT, CHECKLIST)
  char *text;
  size_t text_len;
  size_t text_cap;

  // Type-specific payload
  union {
    TableData table;
    DesignData design;
    ChartData chart;
  } data;

  // View state
  bool is_collapsed;
  bool is_focused;
  float height;        // Computed render height (pixels)
  float view_scale;    // Block size scale factor (0 = use 1.0 default)
  int highlight_color; // 0=none, 1=yellow, 2=blue, 3=green

  // Cursor state (for text editing within block)
  int cursor_pos;
  int selection_start;
  int selection_end;

  // Multi-column layout (drag-and-drop)
  uint16_t layout_row; // Row group index (same value = same visual row)
  uint8_t layout_col;  // Column position within row (0..4)

  // Doubly-linked list
  struct Block *prev;
  struct Block *next;
} Block;

// =============================================================================
// PAGE STRUCTURE
// =============================================================================

typedef struct Page {
  uint32_t id;
  char name[64];
  int in_use;

  // Block list (doubly-linked)
  Block *first_block;
  Block *last_block;
  int block_count;
  uint32_t next_block_id;

  // Scroll state
  float scroll_y;
  float scroll_target_y; // For smooth scrolling

  // Focused block
  Block *focused_block;
} Page;

// =============================================================================
// DOCUMENT STRUCTURE
// =============================================================================

typedef struct Document {
  char filepath[256];
  char title[128];

  // ID generators
  uint32_t next_page_id;

  // Global metadata (key-value)
  struct {
    char key[64];
    char value[256];
    int in_use;
  } globals[64];

  // Pages (tabs)
  Page pages[32];
  int page_count;
  int active_page_idx; // Index into pages[]

  // Document-level settings
  int doc_width;  // Default page width (for export)
  int doc_height; // Default page height (for export)
} Document;

// =============================================================================
// DOCUMENT API
// =============================================================================

Document *doc_create(void);
void doc_destroy(Document *doc);
void doc_set_title(Document *doc, const char *title);

// --- Globals API ---
bool doc_set_global(Document *doc, const char *key, const char *value);
const char *doc_get_global(const Document *doc, const char *key);
bool doc_unset_global(Document *doc, const char *key);

typedef void (*doc_global_iter_fn)(const char *key, const char *value,
                                   void *ud);
void doc_globals_iterate(const Document *doc, doc_global_iter_fn fn, void *ud);

// =============================================================================
// PAGE API
// =============================================================================

Page *doc_page_create(Document *doc, const char *name);
Page *doc_page_create_with_id(Document *doc, uint32_t id, const char *name);
Page *doc_page_get(Document *doc, uint32_t page_id);
Page *doc_page_get_active(Document *doc);
Page *doc_page_get_by_index(Document *doc, int index);
bool doc_page_delete(Document *doc, uint32_t page_id);
bool doc_page_rename(Document *doc, uint32_t page_id, const char *name);
void doc_page_activate(Document *doc, uint32_t page_id);
void doc_page_activate_by_index(Document *doc, int index);
int doc_page_get_index(const Document *doc, uint32_t page_id);

// =============================================================================
// BLOCK API
// =============================================================================

// --- Block Lifecycle ---
Block *block_create(Page *page, BlockType type);
Block *block_create_with_id(Page *page, uint32_t id, BlockType type);
void block_destroy(Block *block);

// --- Block List Operations ---
void page_append_block(Page *page, Block *block);
void page_prepend_block(Page *page, Block *block);
void page_insert_block_after(Page *page, Block *after, Block *block);
void page_insert_block_before(Page *page, Block *before, Block *block);
void page_remove_block(Page *page, Block *block);
void page_move_block_up(Page *page, Block *block);
void page_move_block_down(Page *page, Block *block);
Block *page_find_block_by_id(Page *page, uint32_t block_id);

// --- Block Text Content ---
void block_set_text(Block *block, const char *text);
void block_append_text(Block *block, const char *text);
void block_insert_char(Block *block, int pos, char c);
void block_delete_char(Block *block, int pos);
void block_delete_range(Block *block, int start, int end);

// --- Block Type Conversion ---
void block_convert_to(Block *block, BlockType new_type);

// =============================================================================
// TABLE BLOCK API
// =============================================================================

#define DEFAULT_TABLE_ROWS 3
#define DEFAULT_TABLE_COLS 3

void block_table_init(Block *block);
void block_table_destroy(Block *block);
void block_table_set_cell(Block *block, int row, int col, const char *text);
const char *block_table_get_cell(const Block *block, int row, int col);
void block_table_insert_row(Block *block, int row_idx);
void block_table_insert_col(Block *block, int col_idx);
void block_table_delete_row(Block *block, int row_idx);
void block_table_delete_col(Block *block, int col_idx);
void block_table_set_view(Block *block, TableViewMode mode);

// (Canvas Block API removed — Canvas block type has been deleted)

// =============================================================================
// DESIGN BLOCK API
// =============================================================================

#define DEFAULT_DESIGN_WIDTH 800
#define DEFAULT_DESIGN_HEIGHT 600

void block_design_init(Block *block);
void block_design_destroy(Block *block);
Node *block_design_create_node(Block *block, NodeType type, const char *name);
Node *block_design_find_node(Block *block, uint32_t node_id);
bool block_design_delete_node(Block *block, uint32_t node_id);
Node *block_design_duplicate_node(Block *block, uint32_t node_id);
void block_design_set_size(Block *block, int width, int height);
void block_design_set_bg_color(Block *block, uint32_t color);

// Multi-page support for presentations
int block_design_add_page(Block *block);
bool block_design_delete_page(Block *block, int page_idx);
void block_design_goto_page(Block *block, int page_idx);

// =============================================================================
// NODE API (for Canvas/Design blocks)
// =============================================================================

Node *node_create(uint32_t id, NodeType type, const char *name);
void node_destroy(Node *node);
void node_destroy_tree(Node *node);
void node_add_child(Node *parent, Node *child);
void node_remove_from_parent(Node *node);
void node_set_position(Node *node, float x, float y);
void node_set_size(Node *node, float w, float h);
void node_set_text(Node *node, const char *text);
void node_set_fill_color(Node *node, uint32_t rgba);
void node_set_stroke(Node *node, uint32_t rgba, float width);
void node_bring_to_front(Node *node);
void node_send_to_back(Node *node);
void node_move_up(Node *node);
void node_move_down(Node *node);
void node_insert_before(Node *node, Node *target);
void node_insert_after(Node *node, Node *target);

// =============================================================================
// PERSISTENCE
// =============================================================================

// AOF (Append-Only File) Operations
void doc_save_aof(const Document *doc, FILE *fp);

// Snapshot Operations
void doc_save_snapshot(const Document *doc, const char *filepath);

// SVG Export (Design block → SVG file)
bool block_design_export_svg(const Block *block, const char *filepath,
                             struct AslanTable *linked_tbl,
                             uint64_t linked_row_id);

// PDF Export (Design block → PDF file via libharu)
bool block_design_export_pdf(const Block *block, const char *filepath,
                             struct AslanTable *linked_tbl,
                             uint64_t linked_row_id);

// =============================================================================
// UTILITY
// =============================================================================

const char *block_type_to_string(BlockType type);
BlockType block_type_from_string(const char *str);
const char *node_type_to_string(NodeType type);
NodeType node_type_from_string(const char *str);

// =============================================================================
// AOF COMMAND GRAMMAR v3
// =============================================================================
//
// -- aslan_aof:3 --
//
// # Document
// doc_title "<title>"
// set_global <key> "<value>"
// unset_global <key>
//
// # Pages
// page_create <page_id> "<name>"
// page_delete <page_id>
// page_rename <page_id> "<name>"
// page_activate <page_id>
//
// # Blocks
// block_create <page_id> <block_id> <type>
// block_delete <page_id> <block_id>
// block_text <page_id> <block_id> "<content>"
// block_move_after <page_id> <block_id> <after_block_id>
// block_collapse <page_id> <block_id> 0|1
// block_highlight <page_id> <block_id> 0|1|2|3
//
// # Table Block
// block_table_init <page_id> <block_id> <rows> <cols>
// block_table_cell <page_id> <block_id> <row> <col> "<text>"
// block_table_view <page_id> <block_id> table|list|board
// block_table_insert_row <page_id> <block_id> <row_idx>
// block_table_insert_col <page_id> <block_id> <col_idx>
// block_table_delete_row <page_id> <block_id> <row_idx>
// block_table_delete_col <page_id> <block_id> <col_idx>
//
// (Canvas Block AOF commands removed)
//
// # Design Block
// block_design_init <page_id> <block_id> <width> <height>
// block_design_bg <page_id> <block_id> <rgba>
// block_design_node <page_id> <block_id> ... (same as canvas_node)
//
//
//
// =============================================================================

#endif // DOCUMENT_H
