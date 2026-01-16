#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h> // FILE

// Forward decl
union SDL_Event;
struct SpatialIndex;
typedef struct SpatialIndex SpatialIndex;
struct Document;

// =========================
// Nodes & Graph Hierarchy
// =========================

// Types de nœuds graphiques supportés
typedef enum {
  NODE_TYPE_ROOT,
  NODE_TYPE_RECT,
  NODE_TYPE_TEXT,
  NODE_TYPE_IMAGE,
  // Notion-style blocks
  NODE_TYPE_BLOCK_TEXT,
  NODE_TYPE_BLOCK_HEADING_1,
  NODE_TYPE_BLOCK_HEADING_2,
  NODE_TYPE_BLOCK_HEADING_3,
  NODE_TYPE_BLOCK_SEPARATOR,
  NODE_TYPE_BLOCK_CALLOUT,
  NODE_TYPE_BLOCK_CHECKLIST
} NodeType;

// Structure générique pour un objet graphique
typedef struct Node {
  uint32_t id;
  NodeType type;
  char name[64];

  // Propriétés spatiales de base
  float x, y;
  float width, height;

  // Payload for certain node types
  char text[256];

  // Dynamic payload for specialized behavior (BLOCK, GRID, etc.)
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
// Notes (Notion/Kilo-inspired)
// =========================
typedef enum {
  ASLAN_ROW_TEXT = 0,
  ASLAN_ROW_GRID = 1,       // reserved
  ASLAN_ROW_LUA_WIDGET = 2, // reserved
  ASLAN_ROW_HEADING_1 = 3,  // H1 block (large title)
  ASLAN_ROW_HEADING_2 = 4,  // H2 block (medium title)
  ASLAN_ROW_HEADING_3 = 5,  // H3 block (small title)
  ASLAN_ROW_SEPARATOR = 6,  // Visual separator line
  ASLAN_ROW_CALLOUT =
      7, // Callout block (icon + pastel background + accent bar)
  ASLAN_ROW_CHECKLIST = 8 // Checklist block (checkbox + strikethrough)
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
  void *block;

  // Unified Nodes: Link to the node in the tree
  struct Node *node;
} AslanRow;

typedef struct {
  struct Document *doc; // Parent doc for node ops
  uint32_t page_id;

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
} AslanNoteDoc;

// =========================
// Grid (Tableau) (v1)
// =========================
typedef struct {
  int rows;
  int cols;
  char ***data;
  int *col_widths;
  int active_row;
  int active_col;
} AslanGrid;

AslanGrid *grid_create(int r, int c);
void grid_destroy(AslanGrid *grid);
void grid_set_cell(AslanGrid *grid, int r, int c, const char *text);
void grid_compute_column_widths(AslanGrid *grid, const void *font);
int grid_address_to_coords(const char *address, int *out_r, int *out_c);

// Notes API
void note_insert_line(AslanNoteDoc *doc, int row_idx, const char *text);
int note_handle_input(AslanNoteDoc *doc, const union SDL_Event *ev);
void engine_serialize_note(const AslanNoteDoc *doc, uint32_t page_id, FILE *fp);

// =========================
// Clip (Video Sequence Editor) (v1)
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
} AslanClipPage;

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
  PAGE_TYPE_SPACE = 0,
  PAGE_TYPE_PAGE = 1,
  PAGE_TYPE_TABLE = 2,
  PAGE_TYPE_NOTE = 3,
  PAGE_TYPE_CLIP = 4,
  PAGE_TYPE_GRID = 5
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
    Node *root;

    // DEPRECATED: Moving to Node tree
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

// --- Node API ---
Node *doc_create_node(Document *doc, NodeType type, const char *name);
Node *doc_find_node_by_id(Document *doc, uint32_t id);
Node *doc_find_node_at(Document *doc, float x, float y);
void doc_node_sync_spatial_index(Document *doc, Node *node, float old_x,
                                 float old_y, float old_w, float old_h);

bool doc_set_visible(Document *doc, uint32_t id, int visible);
bool doc_bring_to_front(Document *doc, uint32_t id);
bool doc_send_to_back(Document *doc, uint32_t id);
bool doc_bring_forward(Document *doc, uint32_t id);
bool doc_send_backward(Document *doc, uint32_t id);

void doc_add_child(Node *parent, Node *child);
bool doc_reparent_node(Document *doc, uint32_t node_id, uint32_t new_parent_id);
void doc_print_tree(const Document *doc);
void doc_recompute_next_ids(Document *doc);

// --- SVG Export ---
void doc_export_to_svg(const Document *doc, const char *filename);

// --- Persistence ---
Document *doc_create(void);
void doc_destroy(Document *doc);

#endif
