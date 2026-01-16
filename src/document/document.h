#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h> // FILE

// =============================================================================
// ASLAN AOF GRAMMAR (v2) - Append-Only File Protocol
// =============================================================================
//
// All commands follow the format: VERB [ARGS...]
// Strings containing spaces must be quoted: "my string"
// IDs are always uint32_t. Coordinates are floats.
//
// --- Document Structure ---
// doc_type <type_string>
// doc_meta <key> <value>
// set_global <key> "<value>"
// unset_global <key>
//
// --- Pages (Surfaces) ---
// page_create <page_id> "<name>"
// page_type <page_id> space|page|table|note|clip|grid
// page_meta <page_id> <key> <value>
// page_activate <page_id>
//
// --- Nodes (Unified Object Model) ---
// create_rect <id> <name> <x> <y> <w> <h>
// create_text <id> <name> <x> <y> <w> <h> "<content>"
// create_image <id> <name> <x> <y> <w> <h> "<path>"
// create_frame <id> <name> <x> <y> <w> <h>
// create_group <id> <name> <x> <y> <w> <h>
// create_note <id> <name> <x> <y> <w> <h>
// create_grid <id> <name> <x> <y> <w> <h> <rows> <cols>
// create_clip <id> <name> <x> <y> <w> <h>
//
// --- Node Manipulation ---
// move <id> <x> <y>
// resize <id> <w> <h>
// set_rect <id> <x> <y> <w> <h>
// set_text <id> "<content>"
// set_attr <id> <key> <value>
// set_visible <id> 0|1
// delete <id>
//
// --- Hierarchy ---
// group_attach <group_id> <child_id>
// reparent <node_id> <new_parent_id>
// bring_to_front <id>
// send_to_back <id>
// bring_forward <id>
// send_backward <id>
//
// --- Note Node Commands ---
// note_row <node_id> <row_idx> <type> "<text>"
// note_row_type <node_id> <row_idx> text|h1|h2|h3|separator|callout|checklist
// note_callout_theme <node_id> <row_idx> info|star|alert|check|idea
// note_checklist_toggle <node_id> <row_idx>
//
// --- Grid Node Commands ---
// grid_cell <node_id> <row> <col> "<text>"
// grid_resize <node_id> <rows> <cols>
//
// --- Clip Node Commands ---
// clip_add <node_id> "<path>"
// clip_set <node_id> <clip_idx> <start_time> <end_time>
// clip_comment <node_id> <clip_idx> "<comment>"
// clip_delete <node_id> <clip_idx>
//
// =============================================================================

// Forward decl
union SDL_Event;
struct SpatialIndex;
typedef struct SpatialIndex SpatialIndex;
struct Document;

// =========================
// Nodes & Graph Hierarchy
// =========================

// Types de nœuds graphiques supportés (Unified Node Model)
typedef enum {
  NODE_TYPE_ROOT = 0,
  NODE_TYPE_RECT = 1,
  NODE_TYPE_TEXT = 2,
  NODE_TYPE_IMAGE = 3,
  NODE_TYPE_FRAME = 4,   // Container frame (like Figma)
  NODE_TYPE_GROUP = 5,   // Logical grouping

  // Unified specialized nodes (Phase 1 completion)
  NODE_TYPE_NOTE = 10,   // Rich-text note (Notion-like)
  NODE_TYPE_GRID = 11,   // Spreadsheet/table
  NODE_TYPE_CLIP = 12,   // Video clip sequence

  // Notion-style inline blocks (used within NOTE nodes)
  NODE_TYPE_BLOCK_TEXT = 20,
  NODE_TYPE_BLOCK_HEADING_1 = 21,
  NODE_TYPE_BLOCK_HEADING_2 = 22,
  NODE_TYPE_BLOCK_HEADING_3 = 23,
  NODE_TYPE_BLOCK_SEPARATOR = 24,
  NODE_TYPE_BLOCK_CALLOUT = 25,
  NODE_TYPE_BLOCK_CHECKLIST = 26
} NodeType;

// Structure générique pour un objet graphique
typedef struct Node {
  uint32_t id;
  NodeType type;
  char name[64];

  // Propriétés spatiales de base
  float x, y;
  float width, height;

  // Payload for certain node types (TEXT content, etc.)
  char text[256];

  // Dynamic payload for specialized behavior (NOTE, GRID, CLIP)
  // Use node_get_*_payload() accessors for type-safe access.
  void *payload;
  size_t payload_size;

  // Visibility (for layers)
  int visible;

  // Hiérarchie
  struct Node *parent;
  struct Node *first_child;
  struct Node *next_sibling;
} Node;

// =========================
// Note Payload (Notion/Kilo-inspired)
// =========================
typedef enum {
  ASLAN_ROW_TEXT = 0,
  ASLAN_ROW_GRID = 1,       // reserved for embedded grid
  ASLAN_ROW_LUA_WIDGET = 2, // reserved for scripted widgets
  ASLAN_ROW_HEADING_1 = 3,
  ASLAN_ROW_HEADING_2 = 4,
  ASLAN_ROW_HEADING_3 = 5,
  ASLAN_ROW_SEPARATOR = 6,
  ASLAN_ROW_CALLOUT = 7,
  ASLAN_ROW_CHECKLIST = 8
} AslanRowType;

typedef enum {
  ASLAN_CALLOUT_INFO = 0,
  ASLAN_CALLOUT_STAR = 1,
  ASLAN_CALLOUT_ALERT = 2,
  ASLAN_CALLOUT_CHECK = 3,
  ASLAN_CALLOUT_IDEA = 4
} AslanCalloutTheme;

typedef struct {
  char *chars;
  size_t len;
  size_t cap;
  AslanCalloutTheme theme;
} AslanCalloutBlock;

typedef struct {
  int checked; // 0/1
} AslanChecklistBlock;

typedef struct {
  AslanRowType type;
  char *chars;
  size_t len;
  size_t cap;
  void *block; // AslanCalloutBlock* or AslanChecklistBlock*

  // Unified Nodes: Link to a child node in the tree (for embedded content)
  struct Node *node;
} AslanRow;

// Note payload structure (stored in Node->payload for NODE_TYPE_NOTE)
// Also used for legacy page-level notes (backward compatibility)
typedef struct {
  struct Document *doc; // Parent doc for node ops
  uint32_t node_id;     // ID of the owning node (unified model)
  uint32_t page_id;     // LEGACY: page ID for page-level notes

  AslanRow *rows;
  int row_count;
  int row_cap;

  int cursor_row;
  int cursor_col;

  bool is_selecting;
  int anchor_row;
  int anchor_col;

  int aof_last_edited_row;
  uint32_t aof_last_edit_time_ms;

  float scroll_y;
  int max_chars_per_visual_line;
} NotePayload;

// Backward compatibility typedef
typedef NotePayload AslanNoteDoc;

// =========================
// Grid Payload (Spreadsheet)
// =========================
typedef struct {
  int rows;
  int cols;
  char ***data;
  int *col_widths;
  int active_row;
  int active_col;
} GridPayload;

// Backward compatibility typedef
typedef GridPayload AslanGrid;

AslanGrid *grid_create(int r, int c);
void grid_destroy(AslanGrid *grid);
void grid_set_cell(AslanGrid *grid, int r, int c, const char *text);
const char *grid_get_cell(const AslanGrid *grid, int r, int c);
void grid_compute_column_widths(AslanGrid *grid, const void *font);
int grid_address_to_coords(const char *address, int *out_r, int *out_c);

// =========================
// Clip Payload (Video Sequence)
// =========================
typedef struct {
  char source_path[256];
  double start_time;
  double end_time;
  bool is_selected;
  char comment[256];
} AslanClip;

typedef struct {
  AslanClip *clips;
  int clip_count;
  int clip_cap;
  int active_index;
  int editing_field;
  double playhead;
  bool is_playing;
} ClipPayload;

// Backward compatibility typedef
typedef ClipPayload AslanClipPage;

AslanClipPage *clip_page_create(void);
void clip_page_destroy(AslanClipPage *page);
int clip_add(AslanClipPage *page, const char *path);
bool clip_delete(AslanClipPage *page, int index);
bool clip_move(AslanClipPage *page, int from, int to);
bool clip_split(AslanClipPage *page, int index, double time);
bool clip_set_source(AslanClipPage *page, int index, const char *path);
bool clip_set_range(AslanClipPage *page, int index, double start, double end);
bool clip_set_comment(AslanClipPage *page, int index, const char *comment);
double clip_get_duration(const AslanClip *clip);
double clip_page_total_duration(const AslanClipPage *page);

// --- Page Type enum ---
typedef enum {
  PAGE_TYPE_SPACE = 0, // Infinite canvas (default)
  PAGE_TYPE_PAGE = 1,  // Fixed-size print layout
  PAGE_TYPE_TABLE = 2, // Reserved
  PAGE_TYPE_NOTE = 3,  // DEPRECATED: Use NODE_TYPE_NOTE instead
  PAGE_TYPE_CLIP = 4,  // DEPRECATED: Use NODE_TYPE_CLIP instead
  PAGE_TYPE_GRID = 5   // DEPRECATED: Use NODE_TYPE_GRID instead
} PageType;

// Le Document est le conteneur principal
typedef struct Document {
  Node *root;
  uint32_t next_node_id;
  char filepath[256];
  char doc_type[32];
  int doc_width;
  int doc_height;

  struct {
    char key[64];
    char value[256];
    int in_use;
  } globals[64];

  struct {
    int in_use;
    uint32_t page_id;
    char name[64];
    Node *root; // Root node for this page's node tree

    // DEPRECATED: Legacy page-level structures (for backward compatibility)
    // New code should use NODE_TYPE_NOTE/GRID/CLIP nodes instead.
    AslanNoteDoc *note;
    AslanGrid *grid;
    AslanClipPage *clip;

    PageType type;
    float width;
    float height;
    float view_x;
    float view_y;
    float view_zoom;
    SpatialIndex *spatial_index;
  } pages[32];
  int page_count;
  uint32_t next_page_id;
  uint32_t active_page_id;
} Document;

// --- Pages API ---
uint32_t doc_page_create(Document *doc, const char *name);
uint32_t doc_page_create_with_id(Document *doc, uint32_t page_id,
                                 const char *name);
uint32_t doc_page_active_id(const Document *doc);
const char *doc_page_name(const Document *doc, uint32_t page_id);
void doc_page_activate(Document *doc, uint32_t page_id);
Node *doc_page_root(Document *doc, uint32_t page_id);

bool doc_page_set_type(Document *doc, uint32_t page_id, PageType type);
PageType doc_page_get_type(const Document *doc, uint32_t page_id);
bool doc_page_set_meta(Document *doc, uint32_t page_id, const char *key,
                       float value);
float doc_page_get_meta(const Document *doc, uint32_t page_id, const char *key);

// --- Globals API ---
typedef void (*doc_global_iter_fn)(const char *key, const char *value,
                                   void *userdata);
void doc_globals_iterate(const Document *doc, doc_global_iter_fn fn,
                         void *userdata);
bool doc_set_global(Document *doc, const char *key, const char *value);
const char *doc_get_global(const Document *doc, const char *key);
bool doc_unset_global(Document *doc, const char *key);

// --- Node API (Core) ---
Node *doc_create_node(Document *doc, NodeType type, const char *name);
Node *doc_create_node_with_id(Document *doc, uint32_t id, NodeType type,
                              const char *name);
Node *doc_find_node_by_id(Document *doc, uint32_t id);
Node *doc_find_node_at(Document *doc, float x, float y);
void doc_node_sync_spatial_index(Document *doc, Node *node, float old_x,
                                 float old_y, float old_w, float old_h);
bool doc_delete_node(Document *doc, uint32_t id);

bool doc_set_visible(Document *doc, uint32_t id, int visible);
bool doc_bring_to_front(Document *doc, uint32_t id);
bool doc_send_to_back(Document *doc, uint32_t id);
bool doc_bring_forward(Document *doc, uint32_t id);
bool doc_send_backward(Document *doc, uint32_t id);

void doc_add_child(Node *parent, Node *child);
bool doc_reparent_node(Document *doc, uint32_t node_id, uint32_t new_parent_id);
void doc_print_tree(const Document *doc);
void doc_recompute_next_ids(Document *doc);

// --- Unified Node Creation (Phase 1 completion) ---
// These functions create specialized nodes with proper payload initialization.
Node *doc_create_note_node(Document *doc, const char *name, float x, float y,
                           float w, float h);
Node *doc_create_grid_node(Document *doc, const char *name, float x, float y,
                           float w, float h, int rows, int cols);
Node *doc_create_clip_node(Document *doc, const char *name, float x, float y,
                           float w, float h);

// --- Payload Accessors (Type-safe access to node payloads) ---
NotePayload *node_get_note_payload(Node *node);
GridPayload *node_get_grid_payload(Node *node);
ClipPayload *node_get_clip_payload(Node *node);

// --- Note Node API ---
void note_payload_init(NotePayload *note, Document *doc, uint32_t node_id);
void note_payload_destroy(NotePayload *note);
void note_insert_row(NotePayload *note, int row_idx, AslanRowType type,
                     const char *text);
void note_set_row_type(NotePayload *note, int row_idx, AslanRowType type);
void note_set_row_text(NotePayload *note, int row_idx, const char *text);
void note_delete_row(NotePayload *note, int row_idx);

// Legacy API (backward compat)
void note_insert_line(AslanNoteDoc *doc, int row_idx, const char *text);
int note_handle_input(AslanNoteDoc *doc, const union SDL_Event *ev);
void engine_serialize_note(const AslanNoteDoc *doc, uint32_t page_id, FILE *fp);

// --- SVG Export ---
void doc_export_to_svg(const Document *doc, const char *filename);

// --- Persistence ---
Document *doc_create(void);
void doc_destroy(Document *doc);

// --- Node Type Helpers ---
const char *node_type_to_string(NodeType type);
NodeType node_type_from_string(const char *str);
bool node_type_is_container(NodeType type);
bool node_type_has_payload(NodeType type);

#endif
