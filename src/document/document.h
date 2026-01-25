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
// Blocks can be: Text, Headings, Separators, Lua Scripts, Tables,
//                Canvas (infinite), Design (finite), Video.
//
// =============================================================================

// Forward declarations
union SDL_Event;
struct SpatialIndex;
typedef struct SpatialIndex SpatialIndex;
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
  BLOCK_HEADING_2, // H2 subtitle
  BLOCK_HEADING_3, // H3 section
  BLOCK_SEPARATOR, // Elegant visual divider
  BLOCK_LUA,       // Executable Lua script block
  BLOCK_TABLE,     // Notion-style table with views
  BLOCK_CANVAS,    // Infinite canvas (Miro-style)
  BLOCK_DESIGN,    // Finite design surface (Canva-style)

  BLOCK_VIDEO = 10,
  BLOCK_COUNT
} BlockType;

// Table view modes
typedef enum {
  TABLE_VIEW_TABLE = 0, // Standard table grid
  TABLE_VIEW_LIST,      // Vertical list
  TABLE_VIEW_BOARD      // Kanban-style board
} TableViewMode;

// =============================================================================
// NODE TYPES (for Canvas/Design embedded content)
// =============================================================================

typedef enum {
  NODE_TYPE_ROOT = 0,
  NODE_TYPE_RECT = 1,
  NODE_TYPE_TEXT = 2,
  NODE_TYPE_IMAGE = 3,
  NODE_TYPE_FRAME = 4,  // Container frame (Figma-like)
  NODE_TYPE_GROUP = 5,  // Logical grouping
  NODE_TYPE_LINE = 6,      // Line/arrow
  NODE_TYPE_ELLIPSE = 7,   // Circle/ellipse
  NODE_TYPE_TRIANGLE = 8   // Triangle
} NodeType;

// =============================================================================
// NODE STRUCTURE (for Canvas/Design blocks)
// =============================================================================

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

  // Text content (for NODE_TYPE_TEXT)
  char text[256];

  // Image path (for NODE_TYPE_IMAGE)
  char image_path[256];

  // Visibility
  int visible;
  int locked;

  // Hierarchy (linked list tree)
  struct Node *parent;
  struct Node *first_child;
  struct Node *next_sibling;
} Node;

// =============================================================================
// BLOCK PAYLOAD STRUCTURES
// =============================================================================

// --- Table Block Data ---
typedef struct {
  char **cells; // Flat array: cells[row * cols + col]
  int rows;
  int cols;
  int *col_widths; // Computed column widths
  int active_row;
  int active_col;
  TableViewMode view_mode;
} TableData;

// --- Canvas Block Data (Infinite) ---
typedef struct {
  Node *root; // Node tree root
  uint32_t next_node_id;
  float view_x, view_y; // Pan offset
  float zoom;           // Zoom level (1.0 = 100%)
  SpatialIndex *index;  // Hit-testing acceleration
} CanvasData;

// --- Design Block Data (Finite) ---
typedef struct {
  Node *root; // Node tree root
  uint32_t next_node_id;
  int width, height; // Fixed dimensions in pixels
  float zoom;        // View zoom
  uint32_t bg_color; // Background color
} DesignData;

// --- Video Block Data ---
typedef struct {
  char source_path[256];
  double start_time;
  double end_time;
  bool is_selected;
  char comment[256];
} VideoClip;

typedef struct {
  VideoClip *clips;
  int clip_count;
  int clip_cap;
  int active_index;
  double playhead;
  bool is_playing;
  bool is_fullscreen;
} VideoData;

// --- Lua Script Block Data ---
typedef struct {
  char *code; // Lua source code
  size_t code_len;
  size_t code_cap;
  char *output; // Last execution output
  size_t output_len;
  bool has_error;
  bool is_running;
} LuaData;

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
    CanvasData canvas;
    DesignData design;
    VideoData video;
    LuaData lua;
  } data;

  // View state
  bool is_collapsed;
  bool is_focused;
  float height;        // Computed render height (pixels)
  int highlight_color; // 0=none, 1=yellow, 2=blue, 3=green

  // Cursor state (for text editing within block)
  int cursor_pos;
  int selection_start;
  int selection_end;

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

void block_table_init(Block *block, int rows, int cols);
void block_table_destroy(Block *block);
void block_table_set_cell(Block *block, int row, int col, const char *text);
const char *block_table_get_cell(const Block *block, int row, int col);
void block_table_insert_row(Block *block, int row_idx);
void block_table_insert_col(Block *block, int col_idx);
void block_table_delete_row(Block *block, int row_idx);
void block_table_delete_col(Block *block, int col_idx);
void block_table_set_view(Block *block, TableViewMode mode);

// =============================================================================
// CANVAS BLOCK API
// =============================================================================

void block_canvas_init(Block *block);
void block_canvas_destroy(Block *block);
Node *block_canvas_create_node(Block *block, NodeType type, const char *name);
Node *block_canvas_find_node(Block *block, uint32_t node_id);
Node *block_canvas_find_node_at(Block *block, float x, float y);
bool block_canvas_delete_node(Block *block, uint32_t node_id);
void block_canvas_set_view(Block *block, float x, float y, float zoom);

// =============================================================================
// DESIGN BLOCK API
// =============================================================================

void block_design_init(Block *block, int width, int height);
void block_design_destroy(Block *block);
Node *block_design_create_node(Block *block, NodeType type, const char *name);
Node *block_design_find_node(Block *block, uint32_t node_id);
bool block_design_delete_node(Block *block, uint32_t node_id);
void block_design_set_size(Block *block, int width, int height);
void block_design_set_bg_color(Block *block, uint32_t color);

// =============================================================================
// VIDEO BLOCK API
// =============================================================================

void block_video_init(Block *block);
void block_video_destroy(Block *block);
int block_video_add_clip(Block *block, const char *path);
bool block_video_delete_clip(Block *block, int index);
bool block_video_set_clip_range(Block *block, int index, double start,
                                double end);
bool block_video_set_clip_comment(Block *block, int index, const char *comment);
double block_video_total_duration(const Block *block);
void block_video_set_playhead(Block *block, double time);
void block_video_toggle_play(Block *block);
void block_video_toggle_fullscreen(Block *block);

// =============================================================================
// LUA BLOCK API
// =============================================================================

void block_lua_init(Block *block);
void block_lua_destroy(Block *block);
void block_lua_set_code(Block *block, const char *code);
void block_lua_execute(Block *block, struct lua_State *L);
void block_lua_clear_output(Block *block);

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

// =============================================================================
// PERSISTENCE
// =============================================================================

// AOF (Append-Only File) Operations
void doc_save_aof(const Document *doc, FILE *fp);
void doc_load_aof(Document *doc, FILE *fp);

// Snapshot Operations
void doc_save_snapshot(const Document *doc, const char *filepath);
Document *doc_load_snapshot(const char *filepath);

// Export
void doc_export_svg(const Document *doc, const char *filepath);
void doc_export_pdf(const Document *doc, const char *filepath);

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
// # Canvas Block
// block_canvas_node <page_id> <block_id> create <node_id> <type> "<name>"
// block_canvas_node <page_id> <block_id> delete <node_id>
// block_canvas_node <page_id> <block_id> move <node_id> <x> <y>
// block_canvas_node <page_id> <block_id> resize <node_id> <w> <h>
// block_canvas_node <page_id> <block_id> text <node_id> "<content>"
// block_canvas_node <page_id> <block_id> fill <node_id> <rgba>
// block_canvas_node <page_id> <block_id> stroke <node_id> <rgba> <width>
// block_canvas_view <page_id> <block_id> <x> <y> <zoom>
//
// # Design Block
// block_design_init <page_id> <block_id> <width> <height>
// block_design_bg <page_id> <block_id> <rgba>
// block_design_node <page_id> <block_id> ... (same as canvas_node)
//
// # Video Block
// block_video_clip_add <page_id> <block_id> "<path>"
// block_video_clip_delete <page_id> <block_id> <clip_idx>
// block_video_clip_range <page_id> <block_id> <clip_idx> <start> <end>
// block_video_clip_comment <page_id> <block_id> <clip_idx> "<comment>"
// block_video_playhead <page_id> <block_id> <time>
//
// # Lua Block
// block_lua_code <page_id> <block_id> "<code>"
//
// # Callout Block
// block_callout_theme <page_id> <block_id> info|alert|star|check|idea
//
// # Checklist Block
// block_checklist_toggle <page_id> <block_id>
//
// =============================================================================

#endif // DOCUMENT_H
