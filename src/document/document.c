#include "document.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <SDL2/SDL.h>

// Nuklear font measurement API (used only here; keep it out of core headers).
#include "../../include/nuklear.h"

// Local strdup (C99, minimal)
static char* aslan_strdup(const char* s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

// =========================
// Grid (Tableau) (v1)
// =========================

AslanGrid* grid_create(int r, int c) {
    if (r < 1) r = 1;
    if (c < 1) c = 1;

    AslanGrid* g = (AslanGrid*)calloc(1, sizeof(AslanGrid));
    if (!g) return NULL;
    g->rows = r;
    g->cols = c;

    g->data = (char***)calloc((size_t)r, sizeof(char**));
    if (!g->data) {
        free(g);
        return NULL;
    }

    for (int i = 0; i < r; i++) {
        g->data[i] = (char**)calloc((size_t)c, sizeof(char*));
        if (!g->data[i]) {
            grid_destroy(g);
            return NULL;
        }
        // cells default to NULL
    }

    g->col_widths = (int*)calloc((size_t)c, sizeof(int));
    if (!g->col_widths) {
        grid_destroy(g);
        return NULL;
    }
    // Default widths (simple + editable later)
    for (int j = 0; j < c; j++) g->col_widths[j] = 140;

    // UI focus defaults
    g->active_row = 0;
    g->active_col = 0;

    return g;
}

int grid_address_to_coords(const char* address, int* out_r, int* out_c) {
    if (!address || !address[0] || !out_r || !out_c) return 0;

    // Skip leading whitespace
    const char* p = address;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) return 0;

    // Column: A-Z (case-insensitive)
    char col_ch = *p;
    if (col_ch >= 'a' && col_ch <= 'z') col_ch = (char)(col_ch - 'a' + 'A');
    if (col_ch < 'A' || col_ch > 'Z') return 0;
    int col = (int)(col_ch - 'A');
    p++;

    // Row: 1..
    if (*p < '0' || *p > '9') return 0;
    int row_1 = 0;
    while (*p >= '0' && *p <= '9') {
        int d = *p - '0';
        // guard overflow (rows are small anyway)
        if (row_1 > 1000000) return 0;
        row_1 = row_1 * 10 + d;
        p++;
    }
    if (row_1 <= 0) return 0;

    // Allow trailing whitespace only
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '\0') return 0;

    *out_r = row_1 - 1;
    *out_c = col;
    return 1;
}

void grid_compute_column_widths(AslanGrid* grid, const void* font_ptr) {
    const struct nk_user_font* font = (const struct nk_user_font*)font_ptr;
    if (!grid || !font) return;
    if (grid->rows <= 0 || grid->cols <= 0) return;
    if (!grid->data || !grid->col_widths) return;
    if (!font->width) return;

    // Tuning: keep grid readable but bounded.
    const float pad_px = 16.0f;      // left+right padding around text
    const float min_px = 60.0f;      // minimum column width
    const float max_px = 420.0f;     // maximum column width to prevent layout breakage

    for (int c = 0; c < grid->cols; c++) {
        float best = min_px;

        for (int r = 0; r < grid->rows; r++) {
            const char* txt = (grid->data[r] && grid->data[r][c]) ? grid->data[r][c] : "";
            int len = (int)strlen(txt);
            float w = font->width(font->userdata, font->height, txt, len) + pad_px;
            if (w > best) best = w;
            if (best >= max_px) {
                best = max_px;
                break;
            }
        }

        if (best < min_px) best = min_px;
        if (best > max_px) best = max_px;
        grid->col_widths[c] = (int)(best + 0.5f);
    }
}

void grid_destroy(AslanGrid* grid) {
    if (!grid) return;
    if (grid->data) {
        for (int r = 0; r < grid->rows; r++) {
            if (!grid->data[r]) continue;
            for (int c = 0; c < grid->cols; c++) {
                free(grid->data[r][c]);
                grid->data[r][c] = NULL;
            }
            free(grid->data[r]);
            grid->data[r] = NULL;
        }
        free(grid->data);
        grid->data = NULL;
    }
    free(grid->col_widths);
    grid->col_widths = NULL;
    free(grid);
}

void grid_set_cell(AslanGrid* grid, int r, int c, const char* text) {
    if (!grid) return;
    if (r < 0 || r >= grid->rows) return;
    if (c < 0 || c >= grid->cols) return;
    char* dup = aslan_strdup(text);
    if (!dup) return;
    free(grid->data[r][c]);
    grid->data[r][c] = dup;
}

static int global_find_index(const Document* doc, const char* key) {
    if (!doc || !key || !key[0]) return -1;
    for (int i = 0; i < (int)(sizeof(doc->globals) / sizeof(doc->globals[0])); i++) {
        if (doc->globals[i].in_use && strncmp(doc->globals[i].key, key, sizeof(doc->globals[i].key)) == 0) {
            return i;
        }
    }
    return -1;
}

static int global_find_free_slot(Document* doc) {
    if (!doc) return -1;
    for (int i = 0; i < (int)(sizeof(doc->globals) / sizeof(doc->globals[0])); i++) {
        if (!doc->globals[i].in_use) return i;
    }
    return -1;
}

static Node* detach_child(Node* parent, Node* child) {
    if (!parent || !child) return NULL;
    Node* prev = NULL;
    Node* cur = parent->first_child;
    while (cur) {
        if (cur == child) {
            if (prev) prev->next_sibling = cur->next_sibling;
            else parent->first_child = cur->next_sibling;
            cur->next_sibling = NULL;
            cur->parent = parent;
            return cur;
        }
        prev = cur;
        cur = cur->next_sibling;
    }
    return NULL;
}

static void append_child(Node* parent, Node* child) {
    if (!parent || !child) return;
    child->parent = parent;
    child->next_sibling = NULL;
    if (!parent->first_child) {
        parent->first_child = child;
        return;
    }
    Node* cur = parent->first_child;
    while (cur->next_sibling) cur = cur->next_sibling;
    cur->next_sibling = child;
}

static void insert_child_after(Node* parent, Node* child, Node* after) {
    if (!parent || !child) return;
    child->parent = parent;
    if (!after) {
        child->next_sibling = parent->first_child;
        parent->first_child = child;
        return;
    }
    child->next_sibling = after->next_sibling;
    after->next_sibling = child;
}

static Node* make_root_node(void) {
    Node* r = (Node*)calloc(1, sizeof(Node));
    if (!r) return NULL;
    r->id = 0;
    r->type = NODE_TYPE_ROOT;
    strcpy(r->name, "Root");
    return r;
}

Document* doc_create(void) {
    Document* doc = (Document*)malloc(sizeof(Document));
    if (!doc) return NULL;
    
    doc->next_node_id = 1;
    memset(doc->filepath, 0, sizeof(doc->filepath));
    
    // Pages defaults
    memset(doc->pages, 0, sizeof(doc->pages));
    doc->page_count = 0;
    doc->next_page_id = 1;
    doc->active_page_id = 0;

    // Create page 0
    doc->pages[0].in_use = 1;
    doc->pages[0].page_id = doc->next_page_id++;
    strncpy(doc->pages[0].name, "Page 1", sizeof(doc->pages[0].name) - 1);
    doc->pages[0].root = make_root_node();
    
    // Default type: Space (infinite)
    doc->pages[0].type = PAGE_TYPE_SPACE;
    doc->pages[0].view_zoom = 1.0f;
    
    doc->page_count = 1;
    doc->active_page_id = doc->pages[0].page_id;

    // Back-compat: root points to active page root
    doc->root = doc->pages[0].root;

    // Document meta defaults
    memset(doc->doc_type, 0, sizeof(doc->doc_type));
    strncpy(doc->doc_type, "canvas2d", sizeof(doc->doc_type) - 1);
    doc->doc_width = 800;
    doc->doc_height = 600;

    // Globals defaults
    memset(doc->globals, 0, sizeof(doc->globals));
    
    return doc;
}

static void free_node_recursive(Node* node) {
    if (!node) return;
    free_node_recursive(node->first_child);
    free_node_recursive(node->next_sibling);
    free(node);
}

// =========================
// Notes (v1)
// =========================

static void note_row_set_text(AslanRow* r, const char* text) {
    if (!r) return;
    if (!text) text = "";

    size_t n = strlen(text);
    if (n + 1 > r->cap) {
        size_t new_cap = r->cap ? r->cap : 32;
        while (new_cap < n + 1) new_cap *= 2;
        char* new_chars = (char*)realloc(r->chars, new_cap);
        if (!new_chars) return;
        r->chars = new_chars;
        r->cap = new_cap;
    }
    memcpy(r->chars, text, n);
    r->chars[n] = '\0';
    r->len = n;
}

static void note_row_insert_char(AslanRow* r, int at, char c) {
    if (!r) return;
    if (at < 0) at = 0;
    if ((size_t)at > r->len) at = (int)r->len;

    if (r->len + 2 > r->cap) {
        size_t new_cap = r->cap ? r->cap : 32;
        while (new_cap < r->len + 2) new_cap *= 2;
        char* new_chars = (char*)realloc(r->chars, new_cap);
        if (!new_chars) return;
        r->chars = new_chars;
        r->cap = new_cap;
    }

    memmove(r->chars + at + 1, r->chars + at, (r->len - (size_t)at) + 1);
    r->chars[at] = c;
    r->len++;
}

static void note_row_delete_char(AslanRow* r, int at) {
    if (!r) return;
    if (r->len == 0) return;
    if (at <= 0) return;
    if ((size_t)at > r->len) at = (int)r->len;

    size_t idx = (size_t)at - 1;
    memmove(r->chars + idx, r->chars + idx + 1, (r->len - idx));
    r->len--;
}

void note_insert_line(AslanNoteDoc* doc, int row_idx, const char* text) {
    if (!doc) return;
    if (row_idx < 0) row_idx = 0;
    if (row_idx > doc->row_count) row_idx = doc->row_count;

    if (doc->row_count + 1 > doc->row_cap) {
        int new_cap = doc->row_cap ? doc->row_cap * 2 : 16;
        while (new_cap < doc->row_count + 1) new_cap *= 2;
        AslanRow* new_rows = (AslanRow*)realloc(doc->rows, (size_t)new_cap * sizeof(AslanRow));
        if (!new_rows) return;
        doc->rows = new_rows;
        doc->row_cap = new_cap;
    }

    if (row_idx < doc->row_count) {
        memmove(&doc->rows[row_idx + 1], &doc->rows[row_idx], (size_t)(doc->row_count - row_idx) * sizeof(AslanRow));
    }

    AslanRow* r = &doc->rows[row_idx];
    memset(r, 0, sizeof(*r));
    r->type = ASLAN_ROW_TEXT;
    note_row_set_text(r, text);

    doc->row_count++;
    if (doc->cursor_row >= row_idx) doc->cursor_row++;
}

static void note_doc_clamp_cursor(AslanNoteDoc* doc) {
    if (!doc) return;
    if (doc->row_count <= 0) {
        doc->cursor_row = 0;
        doc->cursor_col = 0;
        return;
    }

    if (doc->cursor_row < 0) doc->cursor_row = 0;
    if (doc->cursor_row >= doc->row_count) doc->cursor_row = doc->row_count - 1;

    int max_col = (int)doc->rows[doc->cursor_row].len;
    if (doc->cursor_col < 0) doc->cursor_col = 0;
    if (doc->cursor_col > max_col) doc->cursor_col = max_col;
}

static void note_doc_insert_newline(AslanNoteDoc* doc) {
    if (!doc) return;
    if (doc->row_count == 0) {
        note_insert_line(doc, 0, "");
        doc->cursor_row = 0;
        doc->cursor_col = 0;
        return;
    }

    note_doc_clamp_cursor(doc);
    AslanRow* r = &doc->rows[doc->cursor_row];
    // The current row will be modified (split) -> mark it pending for AOF.
    doc->aof_last_edited_row = doc->cursor_row;
    doc->aof_last_edit_time_ms = SDL_GetTicks();
    int at = doc->cursor_col;
    if (at < 0) at = 0;
    if ((size_t)at > r->len) at = (int)r->len;

    const char* tail = r->chars ? (r->chars + at) : "";
    note_insert_line(doc, doc->cursor_row + 1, tail);
    if (r->chars) r->chars[at] = '\0';
    r->len = (size_t)at;

    doc->cursor_row++;
    doc->cursor_col = 0;
}

static void note_doc_join_with_previous_line(AslanNoteDoc* doc) {
    if (!doc) return;
    note_doc_clamp_cursor(doc);
    if (doc->cursor_row <= 0) return;
    if (doc->cursor_row >= doc->row_count) return;

    int cur = doc->cursor_row;
    int prev = cur - 1;
    AslanRow* rprev = &doc->rows[prev];
    AslanRow* rcur = &doc->rows[cur];

    size_t prev_len = rprev->len;
    size_t cur_len = rcur->len;

    // Ensure previous line has room
    if (prev_len + cur_len + 1 > rprev->cap) {
        size_t new_cap = rprev->cap ? rprev->cap : 32;
        while (new_cap < prev_len + cur_len + 1) new_cap *= 2;
        char* new_chars = (char*)realloc(rprev->chars, new_cap);
        if (!new_chars) return;
        rprev->chars = new_chars;
        rprev->cap = new_cap;
    }

    if (cur_len && rcur->chars) {
        memcpy(rprev->chars + prev_len, rcur->chars, cur_len);
        rprev->len = prev_len + cur_len;
        rprev->chars[rprev->len] = '\0';
    }

    // Previous row content changed -> mark it pending for AOF.
    doc->aof_last_edited_row = prev;
    doc->aof_last_edit_time_ms = SDL_GetTicks();

    // Delete current line
    free(rcur->chars);
    rcur->chars = NULL;
    rcur->len = 0;
    rcur->cap = 0;

    if (cur < doc->row_count - 1) {
        memmove(&doc->rows[cur], &doc->rows[cur + 1], (size_t)(doc->row_count - cur - 1) * sizeof(AslanRow));
    }
    doc->row_count--;

    doc->cursor_row = prev;
    doc->cursor_col = (int)prev_len;
    note_doc_clamp_cursor(doc);
}

static void note_sel_clear(AslanNoteDoc* doc) {
    if (!doc) return;
    doc->is_selecting = false;
    doc->anchor_row = doc->cursor_row;
    doc->anchor_col = doc->cursor_col;
}

static int note_sel_has_range(const AslanNoteDoc* doc) {
    if (!doc) return 0;
    if (!doc->is_selecting) return 0;
    return !(doc->anchor_row == doc->cursor_row && doc->anchor_col == doc->cursor_col);
}

static void note_sel_normalize(const AslanNoteDoc* doc,
                               int* out_sr, int* out_sc,
                               int* out_er, int* out_ec) {
    int ar = doc ? doc->anchor_row : 0;
    int ac = doc ? doc->anchor_col : 0;
    int cr = doc ? doc->cursor_row : 0;
    int cc = doc ? doc->cursor_col : 0;

    int sr = ar, sc = ac, er = cr, ec = cc;
    if (sr > er || (sr == er && sc > ec)) {
        sr = cr; sc = cc;
        er = ar; ec = ac;
    }

    if (out_sr) *out_sr = sr;
    if (out_sc) *out_sc = sc;
    if (out_er) *out_er = er;
    if (out_ec) *out_ec = ec;
}

static void note_doc_delete_selection(AslanNoteDoc* doc) {
    if (!doc) return;
    note_doc_clamp_cursor(doc);
    if (!note_sel_has_range(doc)) return;

    int sr = 0, sc = 0, er = 0, ec = 0;
    note_sel_normalize(doc, &sr, &sc, &er, &ec);
    if (sr < 0) sr = 0;
    if (er < 0) er = 0;
    if (sr >= doc->row_count) sr = doc->row_count - 1;
    if (er >= doc->row_count) er = doc->row_count - 1;
    if (sr < 0 || er < 0 || doc->row_count <= 0) {
        note_sel_clear(doc);
        return;
    }

    AslanRow* r0 = &doc->rows[sr];
    AslanRow* r1 = &doc->rows[er];
    int max_sc = (int)(r0 ? r0->len : 0);
    int max_ec = (int)(r1 ? r1->len : 0);
    if (sc < 0) sc = 0;
    if (ec < 0) ec = 0;
    if (sc > max_sc) sc = max_sc;
    if (ec > max_ec) ec = max_ec;

    if (sr == er) {
        // Delete within one line: [sc, ec)
        if (r0 && r0->chars && ec > sc) {
            memmove(r0->chars + sc, r0->chars + ec, (r0->len - (size_t)ec) + 1);
            r0->len -= (size_t)(ec - sc);
        }
        doc->cursor_row = sr;
        doc->cursor_col = sc;
        note_doc_clamp_cursor(doc);
        doc->aof_last_edited_row = sr;
        doc->aof_last_edit_time_ms = SDL_GetTicks();
        note_sel_clear(doc);
        return;
    }

    // Multi-line delete: keep prefix of sr up to sc, keep suffix of er from ec.
    const char* suffix = "";
    size_t suffix_len = 0;
    if (r1 && r1->chars && ec <= (int)r1->len) {
        suffix = r1->chars + ec;
        suffix_len = r1->len - (size_t)ec;
    }

    // Truncate start row at sc.
    if (r0 && r0->chars) {
        if (sc <= (int)r0->len) {
            r0->chars[sc] = '\0';
            r0->len = (size_t)sc;
        }
    }

    // Append suffix to start row.
    if (suffix_len > 0 && r0) {
        if (r0->len + suffix_len + 1 > r0->cap) {
            size_t new_cap = r0->cap ? r0->cap : 32;
            while (new_cap < r0->len + suffix_len + 1) new_cap *= 2;
            char* new_chars = (char*)realloc(r0->chars, new_cap);
            if (!new_chars) return;
            r0->chars = new_chars;
            r0->cap = new_cap;
        }
        memcpy(r0->chars + r0->len, suffix, suffix_len);
        r0->len += suffix_len;
        r0->chars[r0->len] = '\0';
    }

    // Free removed rows content.
    for (int r = sr + 1; r <= er; r++) {
        free(doc->rows[r].chars);
        doc->rows[r].chars = NULL;
        doc->rows[r].len = 0;
        doc->rows[r].cap = 0;
    }

    // Remove rows [sr+1 .. er]
    int remove_count = er - sr;
    if (remove_count > 0) {
        int tail_start = er + 1;
        if (tail_start < doc->row_count) {
            memmove(&doc->rows[sr + 1], &doc->rows[tail_start],
                    (size_t)(doc->row_count - tail_start) * sizeof(AslanRow));
        }
        doc->row_count -= remove_count;
    }

    doc->cursor_row = sr;
    doc->cursor_col = sc;
    note_doc_clamp_cursor(doc);
    doc->aof_last_edited_row = sr;
    doc->aof_last_edit_time_ms = SDL_GetTicks();
    note_sel_clear(doc);
}

static char* note_doc_copy_selection_text(const AslanNoteDoc* doc) {
    if (!doc) return NULL;
    if (!note_sel_has_range(doc)) return NULL;

    int sr = 0, sc = 0, er = 0, ec = 0;
    note_sel_normalize(doc, &sr, &sc, &er, &ec);
    if (sr < 0 || er < 0 || sr >= doc->row_count || er >= doc->row_count) return NULL;

    size_t total = 0;
    for (int r = sr; r <= er; r++) {
        const AslanRow* row = &doc->rows[r];
        int a = 0;
        int b = (int)(row ? row->len : 0);
        if (r == sr) a = sc;
        if (r == er) b = ec;
        if (a < 0) a = 0;
        if (b < 0) b = 0;
        if (a > (int)row->len) a = (int)row->len;
        if (b > (int)row->len) b = (int)row->len;
        if (b < a) b = a;
        total += (size_t)(b - a);
        if (r != er) total += 1; // '\n'
    }

    char* out = (char*)malloc(total + 1);
    if (!out) return NULL;
    size_t at = 0;
    for (int r = sr; r <= er; r++) {
        const AslanRow* row = &doc->rows[r];
        int a = 0;
        int b = (int)(row ? row->len : 0);
        if (r == sr) a = sc;
        if (r == er) b = ec;
        if (a < 0) a = 0;
        if (b < 0) b = 0;
        if (a > (int)row->len) a = (int)row->len;
        if (b > (int)row->len) b = (int)row->len;
        if (b < a) b = a;
        size_t n = (size_t)(b - a);
        if (n > 0 && row && row->chars) {
            memcpy(out + at, row->chars + a, n);
            at += n;
        }
        if (r != er) out[at++] = '\n';
    }
    out[at] = '\0';
    return out;
}

int note_handle_input(AslanNoteDoc* doc, const SDL_Event* ev) {
    if (!doc || !ev) return 0;

    if (doc->row_count == 0) {
        note_insert_line(doc, 0, "");
        doc->cursor_row = 0;
        doc->cursor_col = 0;
    }

    switch (ev->type) {
        case SDL_KEYDOWN: {
            SDL_Keycode sym = ev->key.keysym.sym;
            SDL_Keymod mod = ev->key.keysym.mod;
            // On macOS, relying only on event.mod can be flaky depending on input method.
            // Use a conservative fallback to the global mod state.
            if (mod == KMOD_NONE) mod = SDL_GetModState();
            const int shift = (mod & KMOD_SHIFT) != 0;
            const int shortcut = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;

            // Select all
            if (shortcut && sym == SDLK_a) {
                note_doc_clamp_cursor(doc);
                doc->is_selecting = true;
                doc->anchor_row = 0;
                doc->anchor_col = 0;
                doc->cursor_row = doc->row_count - 1;
                if (doc->cursor_row < 0) doc->cursor_row = 0;
                doc->cursor_col = (doc->row_count > 0) ? (int)doc->rows[doc->cursor_row].len : 0;
                return 1;
            }

            // Clipboard shortcuts
            if (shortcut && (sym == SDLK_c || sym == SDLK_x || sym == SDLK_v)) {
                if (sym == SDLK_c || sym == SDLK_x) {
                    char* txt = note_doc_copy_selection_text(doc);
                    if (txt) {
                        SDL_SetClipboardText(txt);
                        free(txt);
                        if (sym == SDLK_x) {
                            note_doc_delete_selection(doc);
                        }
                        return 1;
                    }
                    // No selection: let it fall through (no-op)
                } else if (sym == SDLK_v) {
                    char* clip = SDL_GetClipboardText();
                    if (clip) {
                        // Replace selection if any
                        if (note_sel_has_range(doc)) {
                            note_doc_delete_selection(doc);
                        }

                        // Insert clipboard, supporting newlines.
                        note_doc_clamp_cursor(doc);
                        int row = doc->cursor_row;
                        int col = doc->cursor_col;
                        AslanRow* r = &doc->rows[row];

                        const char* p = clip;
                        while (*p) {
                            const char* nl = strchr(p, '\n');
                            size_t chunk_len = nl ? (size_t)(nl - p) : strlen(p);

                            // Insert chunk into current row at col
                            for (size_t i = 0; i < chunk_len; i++) {
                                unsigned char ch = (unsigned char)p[i];
                                if (!isprint(ch) && ch != '\t') continue;
                                note_row_insert_char(r, col, (char)ch);
                                col++;
                            }
                            doc->cursor_col = col;
                            doc->aof_last_edited_row = doc->cursor_row;
                            doc->aof_last_edit_time_ms = SDL_GetTicks();

                            if (!nl) break;

                            // newline -> split
                            note_doc_insert_newline(doc);
                            row = doc->cursor_row;
                            col = doc->cursor_col;
                            r = &doc->rows[row];

                            p = nl + 1;
                        }

                        SDL_free(clip);
                        note_sel_clear(doc);
                        return 1;
                    }
                }
            }

            // Selection anchor handling: shift-held movements extend selection.
            // Without shift, any navigation clears selection.
            if (!shift) {
                // If user starts navigating without shift, clear selection.
                if (sym == SDLK_UP || sym == SDLK_DOWN || sym == SDLK_LEFT || sym == SDLK_RIGHT ||
                    sym == SDLK_HOME || sym == SDLK_END) {
                    note_sel_clear(doc);
                }
            } else {
                // Start selection on first shift-navigation.
                if (!doc->is_selecting && (sym == SDLK_UP || sym == SDLK_DOWN || sym == SDLK_LEFT || sym == SDLK_RIGHT ||
                                           sym == SDLK_HOME || sym == SDLK_END)) {
                    doc->is_selecting = true;
                    doc->anchor_row = doc->cursor_row;
                    doc->anchor_col = doc->cursor_col;
                }
            }

            switch (sym) {
                case SDLK_UP:
                    doc->cursor_row--;
                    note_doc_clamp_cursor(doc);
                    return 1;
                case SDLK_DOWN:
                    doc->cursor_row++;
                    note_doc_clamp_cursor(doc);
                    return 1;
                case SDLK_LEFT:
                    doc->cursor_col--;
                    note_doc_clamp_cursor(doc);
                    return 1;
                case SDLK_RIGHT:
                    doc->cursor_col++;
                    note_doc_clamp_cursor(doc);
                    return 1;
                case SDLK_HOME:
                    doc->cursor_col = 0;
                    note_doc_clamp_cursor(doc);
                    return 1;
                case SDLK_END:
                    note_doc_clamp_cursor(doc);
                    doc->cursor_col = (int)doc->rows[doc->cursor_row].len;
                    return 1;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (note_sel_has_range(doc)) {
                        note_doc_delete_selection(doc);
                    }
                    note_doc_insert_newline(doc);
                    note_sel_clear(doc);
                    return 1;
                case SDLK_BACKSPACE: {
                    note_doc_clamp_cursor(doc);
                    if (note_sel_has_range(doc)) {
                        note_doc_delete_selection(doc);
                        return 1;
                    }
                    AslanRow* r = &doc->rows[doc->cursor_row];
                    if (doc->cursor_col > 0) {
                        note_row_delete_char(r, doc->cursor_col);
                        doc->cursor_col--;

                        // Current row content changed -> mark pending for AOF.
                        doc->aof_last_edited_row = doc->cursor_row;
                        doc->aof_last_edit_time_ms = SDL_GetTicks();
                    } else {
                        // Join with previous line when at beginning.
                        note_doc_join_with_previous_line(doc);
                    }
                    note_sel_clear(doc);
                    return 1;
                }
                default:
                    break;
            }
            break;
        }
        case SDL_TEXTINPUT: {
            note_doc_clamp_cursor(doc);
            if (note_sel_has_range(doc)) {
                note_doc_delete_selection(doc);
            }
            AslanRow* r = &doc->rows[doc->cursor_row];
            const char* s = ev->text.text;
            for (int i = 0; s[i]; i++) {
                unsigned char ch = (unsigned char)s[i];
                if (ch == '\n' || ch == '\r') continue;
                if (!isprint(ch) && ch != '\t') continue;
                note_row_insert_char(r, doc->cursor_col, (char)ch);
                doc->cursor_col++;
            }

            // Current row content changed -> mark pending for AOF.
            doc->aof_last_edited_row = doc->cursor_row;
            doc->aof_last_edit_time_ms = SDL_GetTicks();
            note_sel_clear(doc);
            return 1;
        }
        default:
            break;
    }

    return 0;
}

void doc_destroy(Document* doc) {
    if (!doc) return;
    // Free all page roots
    for (int i = 0; i < (int)(sizeof(doc->pages) / sizeof(doc->pages[0])); i++) {
        // Free page-owned note payload
        if (doc->pages[i].in_use && doc->pages[i].note) {
            AslanNoteDoc* nd = doc->pages[i].note;
            for (int r = 0; r < nd->row_count; r++) {
                free(nd->rows[r].chars);
                nd->rows[r].chars = NULL;
                nd->rows[r].len = 0;
                nd->rows[r].cap = 0;
            }
            free(nd->rows);
            nd->rows = NULL;
            nd->row_count = 0;
            nd->row_cap = 0;
            free(nd);
            doc->pages[i].note = NULL;
        }

        // Free page-owned grid payload
        if (doc->pages[i].in_use && doc->pages[i].grid) {
            grid_destroy(doc->pages[i].grid);
            doc->pages[i].grid = NULL;
        }

        if (doc->pages[i].in_use && doc->pages[i].root) {
            free_node_recursive(doc->pages[i].root);
            doc->pages[i].root = NULL;
        }
    }
    free(doc);
}

static int page_find_index_by_id(const Document* doc, uint32_t page_id) {
    if (!doc || page_id == 0) return -1;
    for (int i = 0; i < (int)(sizeof(doc->pages) / sizeof(doc->pages[0])); i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) return i;
    }
    return -1;
}

static int page_find_free_slot(Document* doc) {
    if (!doc) return -1;
    for (int i = 0; i < (int)(sizeof(doc->pages) / sizeof(doc->pages[0])); i++) {
        if (!doc->pages[i].in_use) return i;
    }
    return -1;
}

static uint32_t doc_page_create_internal(Document* doc, uint32_t forced_page_id, const char* name) {
    if (!doc) return 0;
    int slot = page_find_free_slot(doc);
    if (slot < 0) return 0;

    if (forced_page_id != 0) {
        // Ensure uniqueness
        if (page_find_index_by_id(doc, forced_page_id) >= 0) return 0;
    }

    Node* r = make_root_node();
    if (!r) return 0;

    doc->pages[slot].in_use = 1;
    doc->pages[slot].page_id = forced_page_id != 0 ? forced_page_id : doc->next_page_id++;
    memset(doc->pages[slot].name, 0, sizeof(doc->pages[slot].name));
    if (name && name[0]) strncpy(doc->pages[slot].name, name, sizeof(doc->pages[slot].name) - 1);
    else {
        // default name: Page N
        snprintf(doc->pages[slot].name, sizeof(doc->pages[slot].name), "Page %d", doc->page_count + 1);
    }
    doc->pages[slot].root = r;
    doc->pages[slot].note = NULL;
    doc->page_count++;

    // Keep next_page_id monotonic even with forced ids (avoid collisions).
    if (forced_page_id != 0 && forced_page_id >= doc->next_page_id) {
        doc->next_page_id = forced_page_id + 1;
    }

    // Auto-select new page
    doc->active_page_id = doc->pages[slot].page_id;
    doc->root = doc->pages[slot].root;
    return doc->pages[slot].page_id;
}

uint32_t doc_page_create(Document* doc, const char* name) {
    return doc_page_create_internal(doc, 0, name);
}

uint32_t doc_page_create_with_id(Document* doc, uint32_t page_id, const char* name) {
    if (!doc || page_id == 0) return 0;
    return doc_page_create_internal(doc, page_id, name);
}

bool doc_page_select(Document* doc, uint32_t page_id) {
    if (!doc || page_id == 0) return false;
    if (doc->active_page_id == page_id) return true; // idempotent
    int idx = page_find_index_by_id(doc, page_id);
    if (idx < 0) return false;
    doc->active_page_id = page_id;
    doc->root = doc->pages[idx].root;
    return true;
}

uint32_t doc_page_active_id(const Document* doc) {
    if (!doc) return 0;
    return doc->active_page_id;
}

const char* doc_page_name(const Document* doc, uint32_t page_id) {
    if (!doc || page_id == 0) return NULL;
    int idx = page_find_index_by_id(doc, page_id);
    if (idx < 0) return NULL;
    return doc->pages[idx].name;
}

bool doc_set_global(Document* doc, const char* key, const char* value) {
    if (!doc || !key || !key[0] || !value) return false;

    int idx = global_find_index(doc, key);
    if (idx < 0) {
        idx = global_find_free_slot(doc);
        if (idx < 0) return false;
        memset(doc->globals[idx].key, 0, sizeof(doc->globals[idx].key));
        strncpy(doc->globals[idx].key, key, sizeof(doc->globals[idx].key) - 1);
        doc->globals[idx].in_use = 1;
    }

    memset(doc->globals[idx].value, 0, sizeof(doc->globals[idx].value));
    strncpy(doc->globals[idx].value, value, sizeof(doc->globals[idx].value) - 1);
    return true;
}

bool doc_unset_global(Document* doc, const char* key) {
    if (!doc || !key || !key[0]) return false;
    int idx = global_find_index(doc, key);
    if (idx < 0) return false;
    memset(doc->globals[idx].key, 0, sizeof(doc->globals[idx].key));
    memset(doc->globals[idx].value, 0, sizeof(doc->globals[idx].value));
    doc->globals[idx].in_use = 0;
    return true;
}

const char* doc_get_global(const Document* doc, const char* key) {
    if (!doc || !key || !key[0]) return NULL;
    int idx = global_find_index(doc, key);
    if (idx < 0) return NULL;
    return doc->globals[idx].value;
}

void doc_globals_iterate(const Document* doc, doc_global_iter_fn fn, void* user) {
    if (!doc || !fn) return;
    for (int i = 0; i < (int)(sizeof(doc->globals) / sizeof(doc->globals[0])); i++) {
        if (!doc->globals[i].in_use) continue;
        fn(doc->globals[i].key, doc->globals[i].value, user);
    }
}

Node* doc_create_node(Document* doc, NodeType type, const char* name) {
    Node* node = (Node*)calloc(1, sizeof(Node));
    if (!node) return NULL;
    
    node->id = doc->next_node_id++;
    node->type = type;
    node->visible = 1;
    if (name) {
        strncpy(node->name, name, 63);
        node->name[63] = 0;
    }
    
    return node;
}

void doc_add_child(Node* parent, Node* child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    child->next_sibling = NULL; // Sécurité
    
    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        // Ajout en fin de liste
        Node* current = parent->first_child;
        while (current->next_sibling) {
            current = current->next_sibling;
        }
        current->next_sibling = child;
    }
}

static Node* find_node_recursive(Node* current, uint32_t id) {
    if (!current) return NULL;
    if (current->id == id) return current;
    
    Node* found = find_node_recursive(current->first_child, id);
    if (found) return found;
    
    return find_node_recursive(current->next_sibling, id);
}

Node* doc_find_node_by_id(Document* doc, uint32_t id) {
    if (!doc || !doc->root) return NULL;
    return find_node_recursive(doc->root, id);
}

bool doc_set_visible(Document* doc, uint32_t id, int visible) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || node->type == NODE_TYPE_ROOT) return false;
    node->visible = visible ? 1 : 0;
    return true;
}

bool doc_bring_to_front(Document* doc, uint32_t id) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || !node->parent) return false;
    Node* parent = node->parent;
    // already last?
    Node* cur = parent->first_child;
    if (!cur) return false;
    while (cur->next_sibling) cur = cur->next_sibling;
    if (cur == node) return true;

    Node* detached = detach_child(parent, node);
    if (!detached) return false;
    append_child(parent, detached);
    return true;
}

bool doc_send_to_back(Document* doc, uint32_t id) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || !node->parent) return false;
    Node* parent = node->parent;
    if (parent->first_child == node) return true;

    Node* detached = detach_child(parent, node);
    if (!detached) return false;
    insert_child_after(parent, detached, NULL);
    return true;
}

bool doc_bring_forward(Document* doc, uint32_t id) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || !node->parent) return false;
    Node* parent = node->parent;

    Node* prev = NULL;
    Node* cur = parent->first_child;
    while (cur && cur != node) {
        prev = cur;
        cur = cur->next_sibling;
    }
    if (!cur) return false;
    Node* next = cur->next_sibling;
    if (!next) return true; // already at front

    // swap cur and next
    if (prev) prev->next_sibling = next;
    else parent->first_child = next;
    cur->next_sibling = next->next_sibling;
    next->next_sibling = cur;
    return true;
}

bool doc_send_backward(Document* doc, uint32_t id) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || !node->parent) return false;
    Node* parent = node->parent;
    if (parent->first_child == node) return true; // already at back

    Node* prevprev = NULL;
    Node* prev = parent->first_child;
    while (prev && prev->next_sibling != node) {
        prevprev = prev;
        prev = prev->next_sibling;
    }
    if (!prev) return false;

    // swap prev and node
    if (prevprev) prevprev->next_sibling = node;
    else parent->first_child = node;
    prev->next_sibling = node->next_sibling;
    node->next_sibling = prev;
    return true;
}

static Node* find_node_at_recursive(Node* node, float x, float y) {
    if (!node) return NULL;

    // On cherche d'abord dans les enfants (pour avoir le nœud le plus "au-dessus")
    Node* found = find_node_at_recursive(node->next_sibling, x, y);
    if (found) return found;

    found = find_node_at_recursive(node->first_child, x, y);
    if (found) return found;

    // Vérification si le point est dans le rectangle du nœud actuel
    if (node->type != NODE_TYPE_ROOT) {
        if (x >= node->x && x <= node->x + node->width &&
            y >= node->y && y <= node->y + node->height) {
            return node;
        }
    }

    return NULL;
}

Node* doc_find_node_at(Document* doc, float x, float y) {
    if (!doc || !doc->root) return NULL;
    // This function operates in WORLD coordinates.
    // Screen->world conversion should be done at the call-site (where window center is known).
    return find_node_at_recursive(doc->root->first_child, x, y);
}

static void print_node_recursive(Node* node, int depth) {
    if (!node) return;
    
    for (int i = 0; i < depth; i++) printf("  ");
    printf("- [%d] %s (Type: %d) at (%.1f, %.1f)\n", 
           node->id, node->name, node->type, node->x, node->y);
           
    print_node_recursive(node->first_child, depth + 1);
    print_node_recursive(node->next_sibling, depth);
}

static void scan_max_node_id_recursive(Node* node, uint32_t* max_id) {
    if (!node || !max_id) return;
    if (node->type != NODE_TYPE_ROOT) {
        if (node->id > *max_id) *max_id = node->id;
    }
    scan_max_node_id_recursive(node->first_child, max_id);
    scan_max_node_id_recursive(node->next_sibling, max_id);
}

void doc_recompute_next_ids(Document* doc) {
    if (!doc) return;

    uint32_t max_node_id = 0;
    uint32_t max_page_id = 0;

    for (int i = 0; i < (int)(sizeof(doc->pages) / sizeof(doc->pages[0])); i++) {
        if (!doc->pages[i].in_use) continue;
        if (doc->pages[i].page_id > max_page_id) max_page_id = doc->pages[i].page_id;
        if (doc->pages[i].root) {
            scan_max_node_id_recursive(doc->pages[i].root, &max_node_id);
        }
    }

    // next ids are max+1 (but never 0)
    doc->next_node_id = max_node_id + 1;
    if (doc->next_node_id == 0) doc->next_node_id = 1;

    doc->next_page_id = max_page_id + 1;
    if (doc->next_page_id == 0) doc->next_page_id = 1;
}

void doc_print_tree(const Document* doc) {
    printf("=== Document Tree ===\n");
    if (doc && doc->root) {
        print_node_recursive(doc->root, 0);
    } else {
        printf("(Empty)\n");
    }
    printf("=====================\n");
}

// --- SVG Export ---

static void write_svg_node_recursive(FILE* f, Node* node) {
    if (!node) return;

    // On dessine seulement si c'est un RECT pour l'instant
    if (node->type == NODE_TYPE_RECT) {
        // Couleur aléatoire basée sur l'ID pour distinguer les blocs
        int r = (node->id * 50) % 255;
        int g = (node->id * 80) % 255;
        int b = (node->id * 110) % 255;
        
        fprintf(f, "  <g>\n");
        fprintf(f, "    <rect x='%.2f' y='%.2f' width='%.2f' height='%.2f' "
                   "fill='rgb(%d,%d,%d)' stroke='black' stroke-width='2' />\n",
                   node->x, node->y, node->width, node->height, r, g, b);
        
        // Ajout du texte pour le nom
        fprintf(f, "    <text x='%.2f' y='%.2f' font-family='Arial' font-size='14' fill='white'>%s</text>\n",
                   node->x + 5, node->y + 20, node->name);
        fprintf(f, "  </g>\n");
    }

    write_svg_node_recursive(f, node->first_child);
    write_svg_node_recursive(f, node->next_sibling);
}

void doc_export_to_svg(const Document* doc, const char* filename) {
    if (!doc || !doc->root) return;

    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("Error: Could not create SVG file %s\n", filename);
        return;
    }

    // En-tête SVG standard
    fprintf(f, "<?xml version='1.0' encoding='UTF-8' standalone='no'?>\n");
    fprintf(f, "<svg width='800' height='600' xmlns='http://www.w3.org/2000/svg'>\n");
    fprintf(f, "  <rect width='100%%' height='100%%' fill='#f0f0f0' />\n"); // Fond gris clair

    write_svg_node_recursive(f, doc->root);

    fprintf(f, "</svg>\n");
    fclose(f);
    printf("Exported SVG to %s\n", filename);
}

// --- Page Type & Meta API ---

bool doc_page_set_type(Document* doc, uint32_t page_id, PageType type) {
    if (!doc) return false;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            doc->pages[i].type = type;

            // Notes ownership: allocate on first transition to NOTE.
            if (type == PAGE_TYPE_NOTE && doc->pages[i].note == NULL) {
                AslanNoteDoc* nd = (AslanNoteDoc*)calloc(1, sizeof(AslanNoteDoc));
                if (!nd) return false;
                // Always ensure there's at least one empty line.
                note_insert_line(nd, 0, "");
                nd->cursor_row = 0;
                nd->cursor_col = 0;
                nd->is_selecting = false;
                nd->anchor_row = 0;
                nd->anchor_col = 0;
                nd->aof_last_edited_row = -1;
                nd->aof_last_edit_time_ms = 0;
                doc->pages[i].note = nd;
            }

            // Grid ownership: allocate on first transition to GRID.
            if (type == PAGE_TYPE_GRID && doc->pages[i].grid == NULL) {
                // Minimal default: 8x4 table
                AslanGrid* g = grid_create(8, 4);
                if (!g) return false;
                doc->pages[i].grid = g;
            }
            return true;
        }
    }
    return false;
}

PageType doc_page_get_type(const Document* doc, uint32_t page_id) {
    if (!doc) return PAGE_TYPE_SPACE;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            return doc->pages[i].type;
        }
    }
    return PAGE_TYPE_SPACE;
}

bool doc_page_set_meta(Document* doc, uint32_t page_id, const char* key, float value) {
    if (!doc || !key) return false;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            if (strcmp(key, "width") == 0) doc->pages[i].width = value;
            else if (strcmp(key, "height") == 0) doc->pages[i].height = value;
            else if (strcmp(key, "viewport.x") == 0) doc->pages[i].view_x = value;
            else if (strcmp(key, "viewport.y") == 0) doc->pages[i].view_y = value;
            else if (strcmp(key, "viewport.zoom") == 0) doc->pages[i].view_zoom = value;
            else return false;
            return true;
        }
    }
    return false;
}

float doc_page_get_meta(const Document* doc, uint32_t page_id, const char* key) {
    if (!doc || !key) return 0.0f;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            if (strcmp(key, "width") == 0) return doc->pages[i].width;
            else if (strcmp(key, "height") == 0) return doc->pages[i].height;
            else if (strcmp(key, "viewport.x") == 0) return doc->pages[i].view_x;
            else if (strcmp(key, "viewport.y") == 0) return doc->pages[i].view_y;
            else if (strcmp(key, "viewport.zoom") == 0) return doc->pages[i].view_zoom;
        }
    }
    return 0.0f;
}
