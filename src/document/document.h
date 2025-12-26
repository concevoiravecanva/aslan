#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h> // FILE

// Keep Nuklear types out of the core document API.
// Grid autosizing uses the Nuklear font interface, but we pass it as an opaque pointer here
// to avoid pulling Nuklear headers into all translation units.

// Forward decl to avoid pulling SDL into core headers.
// SDL defines SDL_Event as a union typedef.
union SDL_Event;

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
    ASLAN_ROW_CALLOUT = 7,    // Callout block (icon + pastel background + accent bar)
    ASLAN_ROW_CHECKLIST = 8   // Checklist block (checkbox + strikethrough)
} AslanRowType;

// NOTE Callout themes (kept small/stable for AOF durability)
typedef enum {
    ASLAN_CALLOUT_INFO = 0,
    ASLAN_CALLOUT_STAR = 1,
    ASLAN_CALLOUT_ALERT = 2,
    ASLAN_CALLOUT_CHECK = 3,
    ASLAN_CALLOUT_IDEA = 4
} AslanCalloutTheme;

// NOTE Callout block payload (owned by the row; persisted via AOF)
typedef struct {
    // User content (UTF-8). We keep it dynamic like text rows (same edit path, easier).
    char* chars;
    size_t len;
    size_t cap;

    // Theme index drives icon + bg + accent color.
    AslanCalloutTheme theme;
} AslanCalloutBlock;

// NOTE Checklist payload (row->chars stores the editable text)
typedef struct {
    int checked; // 0/1
} AslanChecklistBlock;

typedef struct {
    AslanRowType type;

    // Kilo-style line storage (only used for ASLAN_ROW_TEXT in v1)
    char* chars;
    size_t len;
    size_t cap;

    // Reserved for BLOCK rows
    void* block;
} AslanRow;

typedef struct {
    AslanRow* rows;
    int row_count;
    int row_cap;

    int cursor_row;
    int cursor_col;

    // --- Selection (volatile UI state; not persisted) ---
    bool is_selecting;
    int anchor_row;
    int anchor_col;

    // --- AOF persistence helpers (debounce) ---
    // When >= 0, this row has unflushed edits pending for history.log.
    int aof_last_edited_row;
    // Timestamp (SDL ticks, ms) of the last edit affecting aof_last_edited_row.
    uint32_t aof_last_edit_time_ms;

    // Screen-space vertical scroll for NOTE pages.
    float scroll_y;

    // Soft-wrap support: max characters per visual line (updated by UI each frame).
    int max_chars_per_visual_line;
} AslanNoteDoc;

// =========================
// Grid (Tableau) (v1)
// =========================
typedef struct {
    int rows;
    int cols;

    // 2D array of strings. data[r][c] is either NULL or a heap-allocated C string.
    char*** data;

    // Per-column width in pixels (UI hint; owned by the grid).
    int* col_widths;

    // --- UI state (not persisted) ---
    // Currently focused/editing cell (0-based). Use -1 when no cell is active.
    int active_row;
    int active_col;
} AslanGrid;

AslanGrid* grid_create(int r, int c);
void grid_destroy(AslanGrid* grid);
void grid_set_cell(AslanGrid* grid, int r, int c, const char* text);

// Auto-size: compute per-column widths from content (pixels).
// `font` should be a Nuklear font (e.g. JetBrains Mono) so width measurement matches rendering.
void grid_compute_column_widths(AslanGrid* grid, const void* font);

// Excel-style address helper: "A1" -> (0,0), "B3" -> (2,1). Columns A-Z only for now.
// Returns 1 on success, 0 on parse error.
int grid_address_to_coords(const char* address, int* out_r, int* out_c);

// Notes pillar #1: insertion with dynamic array growth
void note_insert_line(AslanNoteDoc* doc, int row_idx, const char* text);

// Notes pillar #2: input/navigation (option 2: arrows + transition-to-edit)
// Returns 1 if event was consumed.
int note_handle_input(AslanNoteDoc* doc, const union SDL_Event* ev);

// Notes pillar #3: snapshot serialization logic (used by rewrite_aof)
void engine_serialize_note(const AslanNoteDoc* doc, uint32_t page_id, FILE* fp);

// Types de nœuds graphiques supportés
typedef enum {
    NODE_TYPE_ROOT,
    NODE_TYPE_RECT,
    NODE_TYPE_TEXT,
    NODE_TYPE_IMAGE
} NodeType;

// Structure générique pour un objet graphique
// Conçue pour être simple et extensible (POD - Plain Old Data)
typedef struct Node {
    uint32_t id;
    NodeType type;
    char name[64];
    
    // Propriétés spatiales de base
    float x, y;
    float width, height;
    
    // Payload for certain node types
    // - TEXT: string content
    char text[256];
    
    // Visibility (for layers)
    int visible;
    
    // Hiérarchie
    struct Node* parent;
    struct Node* first_child;
    struct Node* next_sibling;
} Node;

// =========================
// Clip (Video Sequence Editor) (v1)
// =========================
typedef struct {
    char source_path[256];   // Path to source video file
    double start_time;       // IN point (seconds)
    double end_time;         // OUT point (seconds)
    bool is_selected;        // Selection state for UI
    char comment[256];       // Optional comment (Descript-style)
} AslanClip;

typedef struct {
    AslanClip* clips;        // Dynamic array of clips
    int clip_count;          // Number of clips
    int clip_cap;            // Capacity of clips array
    
    int active_index;        // Currently focused clip (-1 if none)
    int editing_field;       // 0=none, 1=source, 2=start, 3=end, 4=comment
    
    // Playhead position for preview (seconds from start of sequence)
    double playhead;
    bool is_playing;
} AslanClipPage;

// Clip API
AslanClipPage* clip_page_create(void);
void clip_page_destroy(AslanClipPage* page);

// Core operations
int clip_add(AslanClipPage* page, const char* path);           // Returns index of new clip
bool clip_delete(AslanClipPage* page, int index);              // Delete clip at index
bool clip_move(AslanClipPage* page, int from, int to);         // Reorder clips
bool clip_split(AslanClipPage* page, int index, double time);  // Split clip at time

// Setters
bool clip_set_source(AslanClipPage* page, int index, const char* path);
bool clip_set_range(AslanClipPage* page, int index, double start, double end);
bool clip_set_comment(AslanClipPage* page, int index, const char* comment);

// Utility
double clip_get_duration(const AslanClip* clip);               // end - start
double clip_page_total_duration(const AslanClipPage* page);    // Sum of all clip durations

// --- Page Type enum ---
typedef enum {
    // IMPORTANT: keep numeric values stable for AOF/back-compat.
    // Legacy names:
    // - BOARD => SPACE
    // - PRINT => PAGE
    PAGE_TYPE_SPACE = 0, // Infinite canvas (Miro-like)
    PAGE_TYPE_PAGE  = 1, // Fixed dimensions (A4, etc.)
    // Placeholders (UI-visible, behavior TBD)
    PAGE_TYPE_TABLE = 2,
    PAGE_TYPE_NOTE  = 3,
    PAGE_TYPE_CLIP  = 4,
    PAGE_TYPE_GRID  = 5
} PageType;

// Le Document est le conteneur principal
typedef struct {
    // Back-compat: points to the active page root.
    Node* root;
    uint32_t next_node_id;
    char filepath[256];

    // --- Document-level metadata (future multi-page foundation) ---
    // Antirez-style: keep it simple and AOF-driven.
    char doc_type[32];
    int doc_width;
    int doc_height;

    // --- Document globals (future multi-page foundation) ---
    // Simple string key/value store, driven by AOF.
    // Note: values are stored untyped for now; higher-level systems can parse.
    struct {
        char key[64];
        char value[256];
        int in_use;
    } globals[64];

    // --- Pages (MVP) ---
    // Pages are created explicitly (e.g., via Home -> New Page).
    // For now pages are all canvas2d and share the same globals store.
    // Each page has its own Node tree root.
    struct {
        int in_use;
        uint32_t page_id;
        char name[64];
        Node* root;

    // Optional payloads by page type
    // (Owned by the page; freed in doc_destroy.)
    AslanNoteDoc* note;
    AslanGrid* grid;
    AslanClipPage* clip;
        
        // Page Type & Meta
        PageType type;
        
        // Print meta (mm)
        float width;
        float height;
        
        // Board meta (viewport)
        float view_x;
        float view_y;
        float view_zoom;
    } pages[32];
    int page_count;
    uint32_t next_page_id;
    uint32_t active_page_id;
} Document;

// --- Pages API ---
uint32_t doc_page_create(Document* doc, const char* name);
// Create a page with a caller-provided id.
// Used for deterministic replay/snapshots where ids must be stable across load/save.
// Returns the created page id on success, 0 on failure.
uint32_t doc_page_create_with_id(Document* doc, uint32_t page_id, const char* name);
bool doc_page_select(Document* doc, uint32_t page_id);
uint32_t doc_page_active_id(const Document* doc);
const char* doc_page_name(const Document* doc, uint32_t page_id);

// Page Type & Meta API
bool doc_page_set_type(Document* doc, uint32_t page_id, PageType type);
PageType doc_page_get_type(const Document* doc, uint32_t page_id);
bool doc_page_set_meta(Document* doc, uint32_t page_id, const char* key, float value);
float doc_page_get_meta(const Document* doc, uint32_t page_id, const char* key);

// API C pour la manipulation du document
Document* doc_create(void);
void doc_destroy(Document* doc);

// After bulk loads/replays, recompute monotonic id counters from the current content.
// Ensures further creations won't collide with existing ids.
void doc_recompute_next_ids(Document* doc);

Node* doc_create_node(Document* doc, NodeType type, const char* name);
void doc_add_child(Node* parent, Node* child);
Node* doc_find_node_by_id(Document* doc, uint32_t id);

// --- Layers / z-order helpers ---
// Z-order is defined by sibling order under the same parent.
// The last sibling is drawn last and is therefore on top.
bool doc_set_visible(Document* doc, uint32_t id, int visible);
bool doc_bring_to_front(Document* doc, uint32_t id);
bool doc_send_to_back(Document* doc, uint32_t id);
bool doc_bring_forward(Document* doc, uint32_t id);
bool doc_send_backward(Document* doc, uint32_t id);

// Trouve le nœud le plus profond à une position donnée (pour le clic souris)
Node* doc_find_node_at(Document* doc, float x, float y);

// --- Globals API ---
// Returns true if set/unset succeeded.
bool doc_set_global(Document* doc, const char* key, const char* value);
bool doc_unset_global(Document* doc, const char* key);
// Returns NULL if not present.
const char* doc_get_global(const Document* doc, const char* key);

typedef void (*doc_global_iter_fn)(const char* key, const char* value, void* user);
void doc_globals_iterate(const Document* doc, doc_global_iter_fn fn, void* user);

// Export du document en format SVG pour visualisation rapide
void doc_export_to_svg(const Document* doc, const char* filename);

void doc_print_tree(const Document* doc); // Pour le debug

#endif
