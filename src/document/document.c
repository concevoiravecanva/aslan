#include "document.h"
#include "aslan_table.h"
#include "spatial_index.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// ASLAN DOCUMENT MODEL v3 - Unified Block-Based Implementation
// =============================================================================

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------

static char *aslan_strdup(const char *s) {
  if (!s)
    s = "";
  size_t n = strlen(s);
  char *out = (char *)malloc(n + 1);
  if (!out)
    return NULL;
  memcpy(out, s, n);
  out[n] = '\0';
  return out;
}

static void safe_strcpy(char *dst, const char *src, size_t dst_size) {
  if (!dst || dst_size == 0)
    return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  size_t len = strlen(src);
  if (len >= dst_size)
    len = dst_size - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
}

// -----------------------------------------------------------------------------
// Block Type Helpers
// -----------------------------------------------------------------------------

const char *block_type_to_string(BlockType type) {
  switch (type) {
  case BLOCK_TEXT:
    return "text";
  case BLOCK_HEADING_1:
    return "h1";
  case BLOCK_TABLE:
    return "table";
  case BLOCK_DESIGN:
    return "design";
  case BLOCK_CHART:
    return "chart";
  default:
    return "unknown";
  }
}

BlockType block_type_from_string(const char *str) {
  if (!str)
    return BLOCK_TEXT;
  if (strcmp(str, "text") == 0)
    return BLOCK_TEXT;
  if (strcmp(str, "h1") == 0)
    return BLOCK_HEADING_1;
  if (strcmp(str, "table") == 0)
    return BLOCK_TABLE;
  if (strcmp(str, "canvas") == 0)
    return BLOCK_CANVAS;
  if (strcmp(str, "design") == 0)
    return BLOCK_DESIGN;
  if (strcmp(str, "chart") == 0)
    return BLOCK_CHART;
  return BLOCK_TEXT;
}

const char *node_type_to_string(NodeType type) {
  switch (type) {
  case NODE_TYPE_ROOT:
    return "root";
  case NODE_TYPE_RECT:
    return "rect";
  case NODE_TYPE_TEXT:
    return "text";
  case NODE_TYPE_IMAGE:
    return "image";
  case NODE_TYPE_FRAME:
    return "frame";
  case NODE_TYPE_GROUP:
    return "group";
  case NODE_TYPE_LINE:
    return "line";
  case NODE_TYPE_ELLIPSE:
    return "ellipse";
  case NODE_TYPE_TRIANGLE:
    return "triangle";
  case NODE_TYPE_OCTAGON:
    return "octagon";
  case NODE_TYPE_HEART:
    return "heart";
  case NODE_TYPE_PENTAGON:
    return "pentagon";
  case NODE_TYPE_HEXAGON:
    return "hexagon";
  case NODE_TYPE_STAR:
    return "star";
  case NODE_TYPE_PATH:
    return "path";
  case NODE_TYPE_STROKE:
    return "stroke";
  default:
    return "unknown";
  }
}

NodeType node_type_from_string(const char *str) {
  if (!str)
    return NODE_TYPE_ROOT;
  if (strcmp(str, "rect") == 0)
    return NODE_TYPE_RECT;
  if (strcmp(str, "text") == 0)
    return NODE_TYPE_TEXT;
  if (strcmp(str, "image") == 0)
    return NODE_TYPE_IMAGE;
  if (strcmp(str, "frame") == 0)
    return NODE_TYPE_FRAME;
  if (strcmp(str, "group") == 0)
    return NODE_TYPE_GROUP;
  if (strcmp(str, "line") == 0)
    return NODE_TYPE_LINE;
  if (strcmp(str, "ellipse") == 0)
    return NODE_TYPE_ELLIPSE;
  if (strcmp(str, "triangle") == 0)
    return NODE_TYPE_TRIANGLE;
  if (strcmp(str, "octagon") == 0)
    return NODE_TYPE_OCTAGON;
  if (strcmp(str, "heart") == 0)
    return NODE_TYPE_HEART;
  if (strcmp(str, "pentagon") == 0)
    return NODE_TYPE_PENTAGON;
  if (strcmp(str, "hexagon") == 0)
    return NODE_TYPE_HEXAGON;
  if (strcmp(str, "star") == 0)
    return NODE_TYPE_STAR;
  if (strcmp(str, "path") == 0)
    return NODE_TYPE_PATH;
  if (strcmp(str, "stroke") == 0)
    return NODE_TYPE_STROKE;
  return NODE_TYPE_ROOT;
}

// -----------------------------------------------------------------------------
// Document Lifecycle
// -----------------------------------------------------------------------------

Document *doc_create(void) {
  Document *doc = (Document *)calloc(1, sizeof(Document));
  if (!doc)
    return NULL;

  doc->next_page_id = 1;
  doc->page_count = 0;
  doc->active_page_idx = -1;
  doc->doc_width = 800;
  doc->doc_height = 600;

  return doc;
}

void doc_destroy(Document *doc) {
  if (!doc)
    return;

  // Destroy all pages
  for (int i = 0; i < 32; i++) {
    if (doc->pages[i].in_use) {
      // Destroy all blocks in the page
      Block *b = doc->pages[i].first_block;
      while (b) {
        Block *next = b->next;
        block_destroy(b);
        b = next;
      }
      doc->pages[i].first_block = NULL;
      doc->pages[i].last_block = NULL;
      doc->pages[i].in_use = 0;
    }
  }

  free(doc);
}

void doc_set_title(Document *doc, const char *title) {
  if (!doc)
    return;
  safe_strcpy(doc->title, title, sizeof(doc->title));
}

// -----------------------------------------------------------------------------
// Globals API
// -----------------------------------------------------------------------------

static int global_find_index(const Document *doc, const char *key) {
  if (!doc || !key || !key[0])
    return -1;
  for (int i = 0; i < 64; i++) {
    if (doc->globals[i].in_use && strcmp(doc->globals[i].key, key) == 0) {
      return i;
    }
  }
  return -1;
}

static int global_find_free(const Document *doc) {
  if (!doc)
    return -1;
  for (int i = 0; i < 64; i++) {
    if (!doc->globals[i].in_use)
      return i;
  }
  return -1;
}

bool doc_set_global(Document *doc, const char *key, const char *value) {
  if (!doc || !key || !key[0] || !value)
    return false;

  int idx = global_find_index(doc, key);
  if (idx < 0) {
    idx = global_find_free(doc);
    if (idx < 0)
      return false;
    safe_strcpy(doc->globals[idx].key, key, sizeof(doc->globals[idx].key));
    doc->globals[idx].in_use = 1;
  }

  safe_strcpy(doc->globals[idx].value, value, sizeof(doc->globals[idx].value));
  return true;
}

const char *doc_get_global(const Document *doc, const char *key) {
  int idx = global_find_index(doc, key);
  if (idx < 0)
    return NULL;
  return doc->globals[idx].value;
}

bool doc_unset_global(Document *doc, const char *key) {
  int idx = global_find_index(doc, key);
  if (idx < 0)
    return false;
  doc->globals[idx].in_use = 0;
  doc->globals[idx].key[0] = '\0';
  doc->globals[idx].value[0] = '\0';
  return true;
}

void doc_globals_iterate(const Document *doc, doc_global_iter_fn fn, void *ud) {
  if (!doc || !fn)
    return;
  for (int i = 0; i < 64; i++) {
    if (doc->globals[i].in_use) {
      fn(doc->globals[i].key, doc->globals[i].value, ud);
    }
  }
}

// -----------------------------------------------------------------------------
// Page API
// -----------------------------------------------------------------------------

static int page_find_free_slot(const Document *doc) {
  if (!doc)
    return -1;
  for (int i = 0; i < 32; i++) {
    if (!doc->pages[i].in_use)
      return i;
  }
  return -1;
}

static int page_find_by_id(const Document *doc, uint32_t page_id) {
  if (!doc || page_id == 0)
    return -1;
  for (int i = 0; i < 32; i++) {
    if (doc->pages[i].in_use && doc->pages[i].id == page_id) {
      return i;
    }
  }
  return -1;
}

Page *doc_page_create(Document *doc, const char *name) {
  if (!doc)
    return NULL;

  int slot = page_find_free_slot(doc);
  if (slot < 0)
    return NULL;

  Page *page = &doc->pages[slot];
  memset(page, 0, sizeof(Page));

  page->id = doc->next_page_id++;
  page->in_use = 1;
  page->next_block_id = 1;

  if (name && name[0]) {
    safe_strcpy(page->name, name, sizeof(page->name));
  } else {
    snprintf(page->name, sizeof(page->name), "Page %d", doc->page_count + 1);
  }

  doc->page_count++;

  // Always active index 0 in single-page mode
  doc->active_page_idx = 0;

  return page;
}

Page *doc_page_create_with_id(Document *doc, uint32_t id, const char *name) {
  if (!doc || id == 0)
    return NULL;

  // Check if ID already exists
  if (page_find_by_id(doc, id) >= 0)
    return NULL;

  int slot = page_find_free_slot(doc);
  if (slot < 0)
    return NULL;

  Page *page = &doc->pages[slot];
  memset(page, 0, sizeof(Page));

  page->id = id;
  page->in_use = 1;
  page->next_block_id = 1;

  if (name && name[0]) {
    safe_strcpy(page->name, name, sizeof(page->name));
  } else {
    snprintf(page->name, sizeof(page->name), "Page %u", id);
  }

  // Update next_page_id if needed
  if (id >= doc->next_page_id) {
    doc->next_page_id = id + 1;
  }

  doc->page_count++;

  if (doc->active_page_idx < 0) {
    doc->active_page_idx = slot;
  }

  return page;
}

Page *doc_page_get(Document *doc, uint32_t page_id) {
  int idx = page_find_by_id(doc, page_id);
  if (idx < 0)
    return NULL;
  return &doc->pages[idx];
}

Page *doc_page_get_active(Document *doc) {
  if (!doc)
    return NULL;
  // In single-page mode, index 0 is always the active page
  if (doc->pages[0].in_use)
    return &doc->pages[0];
  return NULL;
}

Page *doc_page_get_by_index(Document *doc, int index) {
  if (!doc || index < 0 || index >= 32)
    return NULL;
  if (!doc->pages[index].in_use)
    return NULL;
  return &doc->pages[index];
}

bool doc_page_delete(Document *doc, uint32_t page_id) {
  int idx = page_find_by_id(doc, page_id);
  if (idx < 0)
    return false;

  // Single Page Mode: Cannot delete the primary page
  if (idx == 0)
    return false;

  Page *page = &doc->pages[idx];

  // Destroy all blocks
  Block *b = page->first_block;
  while (b) {
    Block *next = b->next;
    block_destroy(b);
    b = next;
  }

  page->in_use = 0;
  page->first_block = NULL;
  page->last_block = NULL;
  doc->page_count--;

  // Update active page if needed
  if (doc->active_page_idx == idx) {
    doc->active_page_idx = -1;
    for (int i = 0; i < 32; i++) {
      if (doc->pages[i].in_use) {
        doc->active_page_idx = i;
        break;
      }
    }
  }

  return true;
}

bool doc_page_rename(Document *doc, uint32_t page_id, const char *name) {
  Page *page = doc_page_get(doc, page_id);
  if (!page || !name)
    return false;
  safe_strcpy(page->name, name, sizeof(page->name));
  return true;
}

void doc_page_activate(Document *doc, uint32_t page_id) {
  (void)page_id;
  if (doc) {
    doc->active_page_idx = 0;
  }
}

void doc_page_activate_by_index(Document *doc, int index) {
  if (!doc || index < 0 || index >= 32)
    return;
  if (doc->pages[index].in_use) {
    doc->active_page_idx = index;
  }
}

int doc_page_get_index(const Document *doc, uint32_t page_id) {
  return page_find_by_id(doc, page_id);
}

// -----------------------------------------------------------------------------
// Block Lifecycle
// -----------------------------------------------------------------------------

Block *block_create(Page *page, BlockType type) {
  if (!page)
    return NULL;

  Block *block = (Block *)calloc(1, sizeof(Block));
  if (!block)
    return NULL;

  block->id = page->next_block_id++;
  block->type = type;
  block->is_collapsed = false;
  block->is_focused = false;
  block->height = 0.0f;
  block->highlight_color = 0;
  block->cursor_pos = 0;
  block->selection_start = -1;
  block->selection_end = -1;

  // Initialize text buffer
  block->text_cap = 64;
  block->text = (char *)calloc(block->text_cap, 1);
  block->text_len = 0;

  return block;
}

Block *block_create_with_id(Page *page, uint32_t id, BlockType type) {
  if (!page)
    return NULL;

  Block *block = (Block *)calloc(1, sizeof(Block));
  if (!block)
    return NULL;

  block->id = id;
  block->type = type;
  block->is_collapsed = false;
  block->is_focused = false;

  // Initialize text buffer
  block->text_cap = 64;
  block->text = (char *)calloc(block->text_cap, 1);
  block->text_len = 0;

  // Update page's next_block_id if needed
  if (id >= page->next_block_id) {
    page->next_block_id = id + 1;
  }

  return block;
}

void block_destroy(Block *block) {
  if (!block)
    return;

  // Free text buffer
  free(block->text);

  // Free type-specific data
  switch (block->type) {
  case BLOCK_TABLE:
    block_table_destroy(block);
    break;
  case BLOCK_CANVAS:
    block_canvas_destroy(block);
    break;
  case BLOCK_DESIGN:
    block_design_destroy(block);
    break;
  case BLOCK_CHART:
    block_chart_destroy(block);
    break;
  default:
    break;
  }

  free(block);
}

// -----------------------------------------------------------------------------
// Block List Operations
// -----------------------------------------------------------------------------

void page_append_block(Page *page, Block *block) {
  if (!page || !block)
    return;

  block->prev = page->last_block;
  block->next = NULL;

  if (page->last_block) {
    page->last_block->next = block;
  } else {
    page->first_block = block;
  }
  page->last_block = block;
  page->block_count++;
}

void page_prepend_block(Page *page, Block *block) {
  if (!page || !block)
    return;

  block->prev = NULL;
  block->next = page->first_block;

  if (page->first_block) {
    page->first_block->prev = block;
  } else {
    page->last_block = block;
  }
  page->first_block = block;
  page->block_count++;
}

void page_insert_block_after(Page *page, Block *after, Block *block) {
  if (!page || !block)
    return;

  if (!after) {
    page_prepend_block(page, block);
    return;
  }

  block->prev = after;
  block->next = after->next;

  if (after->next) {
    after->next->prev = block;
  } else {
    page->last_block = block;
  }
  after->next = block;
  page->block_count++;
}

void page_insert_block_before(Page *page, Block *before, Block *block) {
  if (!page || !block)
    return;

  if (!before) {
    page_append_block(page, block);
    return;
  }

  block->next = before;
  block->prev = before->prev;

  if (before->prev) {
    before->prev->next = block;
  } else {
    page->first_block = block;
  }
  before->prev = block;
  page->block_count++;
}

void page_remove_block(Page *page, Block *block) {
  if (!page || !block)
    return;

  if (block->prev) {
    block->prev->next = block->next;
  } else {
    page->first_block = block->next;
  }

  if (block->next) {
    block->next->prev = block->prev;
  } else {
    page->last_block = block->prev;
  }

  block->prev = NULL;
  block->next = NULL;
  page->block_count--;

  if (page->focused_block == block) {
    page->focused_block = NULL;
  }
}

void page_move_block_up(Page *page, Block *block) {
  if (!page || !block || !block->prev)
    return;

  Block *prev = block->prev;

  // Remove block from current position
  if (block->next) {
    block->next->prev = prev;
  } else {
    page->last_block = prev;
  }
  prev->next = block->next;

  // Insert before prev
  block->prev = prev->prev;
  block->next = prev;
  prev->prev = block;

  if (block->prev) {
    block->prev->next = block;
  } else {
    page->first_block = block;
  }
}

void page_move_block_down(Page *page, Block *block) {
  if (!page || !block || !block->next)
    return;

  Block *next = block->next;

  // Remove block from current position
  if (block->prev) {
    block->prev->next = next;
  } else {
    page->first_block = next;
  }
  next->prev = block->prev;

  // Insert after next
  block->next = next->next;
  block->prev = next;
  next->next = block;

  if (block->next) {
    block->next->prev = block;
  } else {
    page->last_block = block;
  }
}

Block *page_find_block_by_id(Page *page, uint32_t block_id) {
  if (!page)
    return NULL;

  for (Block *b = page->first_block; b; b = b->next) {
    if (b->id == block_id)
      return b;
  }
  return NULL;
}

// -----------------------------------------------------------------------------
// Block Text Content
// -----------------------------------------------------------------------------

void block_set_text(Block *block, const char *text) {
  if (!block)
    return;

  size_t len = text ? strlen(text) : 0;

  if (len >= block->text_cap) {
    size_t new_cap = len + 64;
    char *new_text = (char *)realloc(block->text, new_cap);
    if (!new_text)
      return;
    block->text = new_text;
    block->text_cap = new_cap;
  }

  if (text) {
    memcpy(block->text, text, len);
  }
  block->text[len] = '\0';
  block->text_len = len;
}

void block_append_text(Block *block, const char *text) {
  if (!block || !text)
    return;

  size_t add_len = strlen(text);
  size_t new_len = block->text_len + add_len;

  if (new_len >= block->text_cap) {
    size_t new_cap = new_len + 64;
    char *new_text = (char *)realloc(block->text, new_cap);
    if (!new_text)
      return;
    block->text = new_text;
    block->text_cap = new_cap;
  }

  memcpy(block->text + block->text_len, text, add_len);
  block->text_len = new_len;
  block->text[new_len] = '\0';
}

void block_insert_char(Block *block, int pos, char c) {
  if (!block)
    return;
  if (pos < 0)
    pos = 0;
  if ((size_t)pos > block->text_len)
    pos = (int)block->text_len;

  if (block->text_len + 2 > block->text_cap) {
    size_t new_cap = block->text_cap * 2;
    if (new_cap < 128)
      new_cap = 128;
    char *new_text = (char *)realloc(block->text, new_cap);
    if (!new_text)
      return;
    block->text = new_text;
    block->text_cap = new_cap;
  }

  memmove(block->text + pos + 1, block->text + pos, block->text_len - pos + 1);
  block->text[pos] = c;
  block->text_len++;
}

void block_delete_char(Block *block, int pos) {
  if (!block || block->text_len == 0)
    return;
  if (pos < 0 || (size_t)pos >= block->text_len)
    return;

  memmove(block->text + pos, block->text + pos + 1, block->text_len - pos);
  block->text_len--;
}

void block_delete_range(Block *block, int start, int end) {
  if (!block || start >= end)
    return;
  if (start < 0)
    start = 0;
  if ((size_t)end > block->text_len)
    end = (int)block->text_len;

  size_t remove_len = end - start;
  memmove(block->text + start, block->text + end, block->text_len - end + 1);
  block->text_len -= remove_len;
}

void block_convert_to(Block *block, BlockType new_type) {
  if (!block || block->type == new_type)
    return;

  // Clean up old type-specific data
  switch (block->type) {
  case BLOCK_TABLE:
    block_table_destroy(block);
    break;
  case BLOCK_CANVAS:
    block_canvas_destroy(block);
    break;
  case BLOCK_DESIGN:
    block_design_destroy(block);
    break;
  case BLOCK_CHART:
    block_chart_destroy(block);
    break;
  default:
    break;
  }

  block->type = new_type;
}

// -----------------------------------------------------------------------------
// Table Block API
// -----------------------------------------------------------------------------

void block_table_init(Block *block) {
  if (!block || block->type != BLOCK_TABLE)
    return;

  int rows = DEFAULT_TABLE_ROWS;
  int cols = DEFAULT_TABLE_COLS;

  TableData *td = &block->data.table;

  // Free existing columnar table
  if (td->columnar) {
    aslan_table_destroy(td->columnar);
    td->columnar = NULL;
  }

  // Create new columnar table
  td->columnar = aslan_table_create(block->id, "Table");

  if (td->columnar) {
    // Add default columns
    for (int c = 0; c < cols; c++) {
      char col_name[32];
      snprintf(col_name, sizeof(col_name), "Column %d", c + 1);
      // First column is Text, second is Number, rest are Text
      AslanColumnType type = (c == 1) ? ASLAN_COL_NUMBER : ASLAN_COL_TEXT;
      aslan_table_add_column(td->columnar, col_name, type);
    }

    // Add initial rows
    for (int r = 0; r < rows; r++) {
      aslan_table_add_row(td->columnar);
    }
  }

  td->active_row = 0;
  td->active_col = 0;
  td->view_mode = TABLE_VIEW_TABLE;
}

void block_table_destroy(Block *block) {
  if (!block)
    return;

  TableData *td = &block->data.table;

  // Free columnar table
  if (td->columnar) {
    aslan_table_destroy(td->columnar);
    td->columnar = NULL;
  }
}

void block_table_set_cell(Block *block, int row, int col, const char *text) {
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  if (!td->columnar)
    return;

  // Get row_id from view index
  uint64_t row_id = aslan_table_view_row_id(td->columnar, row);
  if (row_id == 0)
    return;

  aslan_table_set_cell(td->columnar, col, row_id, text);
}

const char *block_table_get_cell(const Block *block, int row, int col) {
  if (!block || block->type != BLOCK_TABLE)
    return NULL;

  const TableData *td = &block->data.table;
  if (!td->columnar)
    return NULL;

  // Get row_id from view index
  uint64_t row_id = aslan_table_view_row_id(td->columnar, row);
  if (row_id == 0)
    return NULL;

  return aslan_table_get_text(td->columnar, col, row_id);
}

void block_table_insert_row(Block *block, int row_idx) {
  (void)row_idx; // Columnar always appends at end
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  if (!td->columnar)
    return;

  aslan_table_add_row(td->columnar);
}

void block_table_insert_col(Block *block, int col_idx) {
  (void)col_idx; // Columnar always appends at end
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  if (!td->columnar)
    return;

  char col_name[32];
  snprintf(col_name, sizeof(col_name), "Column %d",
           (int)td->columnar->column_count + 1);
  aslan_table_add_column(td->columnar, col_name, ASLAN_COL_TEXT);
}

void block_table_delete_row(Block *block, int row_idx) {
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  if (!td->columnar)
    return;

  // Get row_id from view index and soft-delete
  uint64_t row_id = aslan_table_view_row_id(td->columnar, row_idx);
  if (row_id != 0) {
    aslan_table_delete_row(td->columnar, row_id);
  }
}

void block_table_delete_col(Block *block, int col_idx) {
  if (!block || block->type != BLOCK_TABLE)
    return;

  TableData *td = &block->data.table;
  if (!td->columnar)
    return;

  aslan_table_remove_column(td->columnar, col_idx);
}

void block_table_set_view(Block *block, TableViewMode mode) {
  if (!block || block->type != BLOCK_TABLE)
    return;
  block->data.table.view_mode = mode;
}

// -----------------------------------------------------------------------------
// Node Operations (for Canvas/Design)
// -----------------------------------------------------------------------------

Node *node_create(uint32_t id, NodeType type, const char *name) {
  Node *node = (Node *)calloc(1, sizeof(Node));
  if (!node)
    return NULL;

  node->id = id;
  node->type = type;
  node->visible = 1;
  node->locked = 0;
  node->fill_color = 0xFFFFFFFF;
  node->stroke_color = 0x000000FF;
  node->stroke_width = 1.0f;
  node->width = 100.0f;
  node->height = 100.0f;
  node->font_size = 12.0f;
  node->font_type = 0;

  if (name) {
    safe_strcpy(node->name, name, sizeof(node->name));
  }

  if (type == NODE_TYPE_CHART) {
    node->chart_data = (ChartData *)calloc(1, sizeof(ChartData));
    if (node->chart_data) {
      node->chart_data->type = CHART_BAR;
      node->chart_data->source_table_id = 0;
      node->chart_data->label_col_idx = 0;
      node->chart_data->value_col_idx = 1;
      node->chart_data->color = 0xFF5578FA; // Default Blue
      node->chart_data->show_values = true;
    }
  }

  return node;
}

void node_destroy(Node *node) {
  if (!node)
    return;

  // Free type-specific dynamic data
  if (node->type == NODE_TYPE_PATH) {
    if (node->path_points) {
      free(node->path_points);
      node->path_points = NULL;
    }
  } else if (node->type == NODE_TYPE_STROKE) {
    if (node->stroke_data) {
      if (node->stroke_data->points) {
        free(node->stroke_data->points);
      }
      free(node->stroke_data);
      node->stroke_data = NULL;
    }
  } else if (node->type == NODE_TYPE_CHART) {
    if (node->chart_data) {
      free(node->chart_data);
      node->chart_data = NULL;
    }
  }
  // Note: Text content is currently static char[256], so no free needed there.
  // Note: Image path is currently static char[256], so no free needed there.

  free(node);
}

void node_destroy_tree(Node *node) {
  if (!node)
    return;

  // Iterative destruction to avoid stack overflow with deep hierarchies/many
  // siblings Process the tree level by level or using a stack if needed, but
  // for our
  // firstChild/nextSibling structure, we can just recurse on children (usually
  // shallow)
  // and iterate on siblings (can be very long).

  // 1. Destroy children (recursive is generally safe here as depth is low in
  // canvas) If depth becomes an issue, we'll need a full iterative traverser.
  if (node->first_child) {
    node_destroy_tree(node->first_child);
    node->first_child = NULL;
  }

  // 2. Destroy siblings iteratively (this is the critical fix for "10k items
  // crash")
  Node *sibling = node->next_sibling;
  while (sibling) {
    Node *next = sibling->next_sibling;

    // Recurse on sibling's children
    if (sibling->first_child) {
      node_destroy_tree(sibling->first_child);
      sibling->first_child = NULL;
    }

    node_destroy(sibling);
    sibling = next;
  }

  // 3. Destroy self
  node_destroy(node);
}

void node_add_child(Node *parent, Node *child) {
  if (!parent || !child)
    return;

  child->parent = parent;
  child->next_sibling = NULL;

  if (!parent->first_child) {
    parent->first_child = child;
  } else {
    Node *cur = parent->first_child;
    while (cur->next_sibling) {
      cur = cur->next_sibling;
    }
    cur->next_sibling = child;
  }
}

void node_remove_from_parent(Node *node) {
  if (!node || !node->parent)
    return;

  Node *parent = node->parent;

  if (parent->first_child == node) {
    parent->first_child = node->next_sibling;
  } else {
    Node *cur = parent->first_child;
    while (cur && cur->next_sibling != node) {
      cur = cur->next_sibling;
    }
    if (cur) {
      cur->next_sibling = node->next_sibling;
    }
  }

  node->parent = NULL;
  node->next_sibling = NULL;
}

void node_set_position(Node *node, float x, float y) {
  if (!node)
    return;
  node->x = x;
  node->y = y;
}

void node_set_size(Node *node, float w, float h) {
  if (!node)
    return;
  node->width = w;
  node->height = h;
}

void node_set_text(Node *node, const char *text) {
  if (!node)
    return;
  safe_strcpy(node->text, text, sizeof(node->text));
}

void node_set_fill_color(Node *node, uint32_t rgba) {
  if (!node)
    return;
  node->fill_color = rgba;
}

void node_set_stroke(Node *node, uint32_t rgba, float width) {
  if (!node)
    return;
  node->stroke_color = rgba;
  node->stroke_width = width;
}

void node_bring_to_front(Node *node) {
  if (!node || !node->parent)
    return;

  Node *parent = node->parent;

  // Already last?
  Node *cur = parent->first_child;
  if (!cur)
    return;
  while (cur->next_sibling)
    cur = cur->next_sibling;
  if (cur == node)
    return;

  // Remove and append
  node_remove_from_parent(node);
  node_add_child(parent, node);
}

void node_send_to_back(Node *node) {
  if (!node || !node->parent)
    return;

  Node *parent = node->parent;
  if (parent->first_child == node)
    return;

  node_remove_from_parent(node);
  node->next_sibling = parent->first_child;
  parent->first_child = node;
  node->parent = parent;
}

void node_move_up(Node *node) {
  if (!node || !node->parent)
    return;

  Node *parent = node->parent;
  if (!node->next_sibling)
    return; // Already at the end (top-most)

  Node *next = node->next_sibling;
  Node *after_next = next->next_sibling;

  // Find node before 'node'
  Node *prev = NULL;
  if (parent->first_child != node) {
    Node *cur = parent->first_child;
    while (cur && cur->next_sibling != node) {
      prev = cur;
      cur = cur->next_sibling;
    }
  }

  // Before: prev -> node -> next -> after_next
  // After:  prev -> next -> node -> after_next

  if (prev) {
    prev->next_sibling = next;
  } else {
    parent->first_child = next;
  }

  next->next_sibling = node;
  node->next_sibling = after_next;
}

void node_move_down(Node *node) {
  if (!node || !node->parent)
    return;

  Node *parent = node->parent;
  if (parent->first_child == node)
    return; // Already at the beginning (bottom-most)

  // Find node before 'node'
  Node *prev = NULL;
  Node *pre_prev = NULL;
  Node *cur = parent->first_child;
  while (cur && cur->next_sibling != node) {
    pre_prev = prev;
    prev = cur;
    cur = cur->next_sibling;
  }

  if (!prev)
    return;

  // Before: pre_prev -> prev -> node -> next
  // After:  pre_prev -> node -> prev -> next

  Node *next = node->next_sibling;

  if (pre_prev) {
    pre_prev->next_sibling = node;
  } else {
    parent->first_child = node;
  }

  node->next_sibling = prev;
  prev->next_sibling = next;
}

// -----------------------------------------------------------------------------
// Canvas Block API
// -----------------------------------------------------------------------------

void block_canvas_init(Block *block) {
  if (!block || block->type != BLOCK_CANVAS)
    return;

  CanvasData *cd = &block->data.canvas;

  cd->root = node_create(0, NODE_TYPE_ROOT, "CanvasRoot");
  cd->next_node_id = 1;
  cd->view_x = 0.0f;
  cd->view_y = 0.0f;
  cd->zoom = 1.0f;
  cd->index = NULL; // spatial_index_create(128) if needed
}

void block_canvas_destroy(Block *block) {
  if (!block)
    return;

  CanvasData *cd = &block->data.canvas;

  if (cd->root) {
    node_destroy_tree(cd->root);
    cd->root = NULL;
  }

  if (cd->index) {
    spatial_index_destroy(cd->index);
    cd->index = NULL;
  }

  // Free strokes (Legacy/Dead code cleanup: cd->strokes is unused in current
  // node-based system) Node-based strokes are freed via
  // node_destroy_tree(cd->root) above.
  /*
  Stroke *s = cd->strokes;
  while (s) {
    Stroke *next = s->next;
    free(s->points);
    free(s);
    s = next;
  }
  */
  cd->strokes = NULL;
}

static Node *find_node_recursive(Node *node, uint32_t id) {
  if (!node)
    return NULL;
  if (node->id == id)
    return node;

  Node *found = find_node_recursive(node->first_child, id);
  if (found)
    return found;

  return find_node_recursive(node->next_sibling, id);
}

Node *block_canvas_create_node(Block *block, NodeType type, const char *name) {
  if (!block || block->type != BLOCK_CANVAS)
    return NULL;

  CanvasData *cd = &block->data.canvas;
  if (!cd->root)
    return NULL;

  Node *node = node_create(cd->next_node_id++, type, name);
  if (!node)
    return NULL;

  node_add_child(cd->root, node);
  return node;
}

Node *block_canvas_find_node(Block *block, uint32_t node_id) {
  if (!block || block->type != BLOCK_CANVAS)
    return NULL;

  CanvasData *cd = &block->data.canvas;
  return find_node_recursive(cd->root, node_id);
}

static Node *find_node_at_recursive(Node *node, float x, float y) {
  if (!node)
    return NULL;

  Node *found = find_node_at_recursive(node->next_sibling, x, y);
  if (found)
    return found;

  found = find_node_at_recursive(node->first_child, x, y);
  if (found)
    return found;

  if (node->type != NODE_TYPE_ROOT && node->visible) {
    if (x >= node->x && x <= node->x + node->width && y >= node->y &&
        y <= node->y + node->height) {
      return node;
    }
  }

  return NULL;
}

Node *block_canvas_find_node_at(Block *block, float x, float y) {
  if (!block || block->type != BLOCK_CANVAS)
    return NULL;

  CanvasData *cd = &block->data.canvas;
  if (!cd->root)
    return NULL;

  return find_node_at_recursive(cd->root->first_child, x, y);
}

bool block_canvas_delete_node(Block *block, uint32_t node_id) {
  if (!block || block->type != BLOCK_CANVAS)
    return false;

  Node *node = block_canvas_find_node(block, node_id);
  if (!node || node->type == NODE_TYPE_ROOT)
    return false;

  node_remove_from_parent(node);
  node_destroy_tree(node);
  return true;
}

void block_canvas_set_view(Block *block, float x, float y, float zoom) {
  if (!block || block->type != BLOCK_CANVAS)
    return;

  CanvasData *cd = &block->data.canvas;
  cd->view_x = x;
  cd->view_y = y;
  cd->zoom = zoom;
}

// Helper to add a stroke to the canvas
void block_canvas_add_stroke(Block *block, Stroke *stroke) {
  if (!block || block->type != BLOCK_CANVAS || !stroke)
    return;

  CanvasData *cd = &block->data.canvas;

  // Append to end of list (simple O(N) append for now, acceptable for < 10k
  // strokes) Optimization: Could store last_stroke pointer in CanvasData
  if (!cd->strokes) {
    cd->strokes = stroke;
  } else {
    Stroke *last = cd->strokes;
    while (last->next) {
      last = last->next;
    }
    last->next = stroke;
  }
}

// Helper to clear all strokes
void block_canvas_clear_strokes(Block *block) {
  if (!block || block->type != BLOCK_CANVAS)
    return;

  CanvasData *cd = &block->data.canvas;
  Stroke *s = cd->strokes;
  while (s) {
    Stroke *next = s->next;
    free(s->points);
    free(s);
    s = next;
  }
  cd->strokes = NULL;
}

// -----------------------------------------------------------------------------
// Design Block API
// -----------------------------------------------------------------------------

void block_design_init(Block *block) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;

  // Initialize pages array with capacity for first page
  dd->page_capacity = 4;
  dd->pages = (DesignPage *)calloc(dd->page_capacity, sizeof(DesignPage));
  dd->page_count = 1;
  dd->current_page = 0;

  // Create first page
  dd->pages[0].root = node_create(0, NODE_TYPE_ROOT, "DesignRoot");
  dd->pages[0].bg_color = 0xFFFFFFFF;

  // Set legacy root pointer to current page's root
  dd->root = dd->pages[0].root;

  dd->next_node_id = 1;
  dd->width = DEFAULT_DESIGN_WIDTH;
  dd->height = DEFAULT_DESIGN_HEIGHT;
  dd->units = 0; // 0 = Pixels
  dd->zoom = 1.0f;
  dd->bg_color = 0xFFFFFFFF;
}

// -----------------------------------------------------------------------------
// Design Block API
// -----------------------------------------------------------------------------

void block_design_destroy(Block *block) {
  if (!block)
    return;

  DesignData *dd = &block->data.design;

  // Destroy all pages
  if (dd->pages) {
    for (int i = 0; i < dd->page_count; i++) {
      if (dd->pages[i].root) {
        node_destroy_tree(dd->pages[i].root);
        dd->pages[i].root = NULL;
      }
    }
    free(dd->pages);
    dd->pages = NULL;
  }

  dd->root = NULL;
  dd->page_count = 0;
  dd->current_page = 0;
}

// Add a new page to design block
int block_design_add_page(Block *block) {
  if (!block || block->type != BLOCK_DESIGN)
    return -1;

  DesignData *dd = &block->data.design;

  // Grow array if needed
  if (dd->page_count >= dd->page_capacity) {
    int new_cap = dd->page_capacity * 2;
    DesignPage *new_pages =
        (DesignPage *)realloc(dd->pages, new_cap * sizeof(DesignPage));
    if (!new_pages)
      return -1;
    dd->pages = new_pages;
    dd->page_capacity = new_cap;
  }

  // Create new page
  int idx = dd->page_count;
  dd->pages[idx].root = node_create(0, NODE_TYPE_ROOT, "DesignRoot");
  dd->pages[idx].bg_color = dd->bg_color;
  dd->page_count++;

  return idx;
}

// Delete a page from design block
bool block_design_delete_page(Block *block, int page_idx) {
  if (!block || block->type != BLOCK_DESIGN)
    return false;

  DesignData *dd = &block->data.design;

  // Can't delete last page
  if (dd->page_count <= 1 || page_idx < 0 || page_idx >= dd->page_count)
    return false;

  // Destroy page's node tree
  if (dd->pages[page_idx].root) {
    node_destroy_tree(dd->pages[page_idx].root);
  }

  // Shift remaining pages
  for (int i = page_idx; i < dd->page_count - 1; i++) {
    dd->pages[i] = dd->pages[i + 1];
  }
  dd->page_count--;

  // Adjust current page if needed
  if (dd->current_page >= dd->page_count) {
    dd->current_page = dd->page_count - 1;
  }

  // Update root pointer
  dd->root = dd->pages[dd->current_page].root;

  return true;
}

// Go to a specific page
void block_design_goto_page(Block *block, int page_idx) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  DesignData *dd = &block->data.design;

  if (page_idx < 0 || page_idx >= dd->page_count)
    return;

  dd->current_page = page_idx;
  dd->root = dd->pages[dd->current_page].root;
}

Node *block_design_create_node(Block *block, NodeType type, const char *name) {
  if (!block || block->type != BLOCK_DESIGN)
    return NULL;

  DesignData *dd = &block->data.design;
  if (!dd->root)
    return NULL;

  Node *node = node_create(dd->next_node_id++, type, name);
  if (!node)
    return NULL;

  node_add_child(dd->root, node);
  return node;
}

Node *block_design_find_node(Block *block, uint32_t node_id) {
  if (!block || block->type != BLOCK_DESIGN)
    return NULL;

  DesignData *dd = &block->data.design;
  return find_node_recursive(dd->root, node_id);
}

bool block_design_delete_node(Block *block, uint32_t node_id) {
  if (!block || block->type != BLOCK_DESIGN)
    return false;

  Node *node = block_design_find_node(block, node_id);
  if (!node || node->type == NODE_TYPE_ROOT)
    return false;

  node_remove_from_parent(node);
  node_destroy_tree(node);
  return true;
}

void block_design_set_size(Block *block, int width, int height) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  // Enforce min/max bounds (100x100 to 4000x4000)
  if (width < 100)
    width = 100;
  if (width > 4000)
    width = 4000;
  if (height < 100)
    height = 100;
  if (height > 4000)
    height = 4000;

  block->data.design.width = width;
  block->data.design.height = height;
}

void block_design_set_bg_color(Block *block, uint32_t color) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  block->data.design.bg_color = color;
}

// -----------------------------------------------------------------------------
// Persistence - AOF Save
// -----------------------------------------------------------------------------

static void escape_string(FILE *fp, const char *str) {
  if (!str) {
    fprintf(fp, "\"\"");
    return;
  }

  fprintf(fp, "\"");
  for (const char *p = str; *p; p++) {
    if (*p == '"')
      fprintf(fp, "\\\"");
    else if (*p == '\\')
      fprintf(fp, "\\\\");
    else if (*p == '\n')
      fprintf(fp, "\\n");
    else if (*p == '\r')
      fprintf(fp, "\\r");
    else if (*p == '\t')
      fprintf(fp, "\\t");
    else
      fputc(*p, fp);
  }
  fprintf(fp, "\"");
}

static void save_node_recursive(FILE *fp, uint32_t page_id, uint32_t block_id,
                                const char *prefix, Node *node) {
  if (!node || node->type == NODE_TYPE_ROOT)
    return;

  // Create node
  fprintf(fp, "%s_node %u %u create %u %s ", prefix, page_id, block_id,
          node->id, node_type_to_string(node->type));
  escape_string(fp, node->name);
  fprintf(fp, "\n");

  // Position and size
  fprintf(fp, "%s_node %u %u move %u %.2f %.2f\n", prefix, page_id, block_id,
          node->id, node->x, node->y);
  fprintf(fp, "%s_node %u %u resize %u %.2f %.2f\n", prefix, page_id, block_id,
          node->id, node->width, node->height);

  // Colors
  fprintf(fp, "%s_node %u %u fill %u %u\n", prefix, page_id, block_id, node->id,
          node->fill_color);
  fprintf(fp, "%s_node %u %u stroke %u %u %.2f\n", prefix, page_id, block_id,
          node->id, node->stroke_color, node->stroke_width);

  // Text content
  if (node->text[0]) {
    fprintf(fp, "%s_node %u %u text %u ", prefix, page_id, block_id, node->id);
    escape_string(fp, node->text);
    fprintf(fp, "\n");
  }

  // Recurse to children and siblings
  for (Node *child = node->first_child; child; child = child->next_sibling) {
    save_node_recursive(fp, page_id, block_id, prefix, child);
  }
}

// -----------------------------------------------------------------------------
// Chart Block API
// -----------------------------------------------------------------------------

void block_chart_init(Block *block) {
  if (!block || block->type != BLOCK_CHART)
    return;
  ChartData *cd = &block->data.chart;
  cd->type = CHART_BAR;
  cd->source_table_id = 0;
  cd->label_col_idx = 0;
  cd->value_col_idx = 1;
  cd->color = 0x3778FAFF; // Aslan accent blue
  cd->show_values = true;
}

void block_chart_destroy(Block *block) {
  // ChartData is inline in the Block union, no heap allocations to free
  (void)block;
}

// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

void doc_save_aof(const Document *doc, FILE *fp) {
  if (!doc || !fp)
    return;

  fprintf(fp, "-- aslan_aof:3 --\n");

  // Document title
  if (doc->title[0]) {
    fprintf(fp, "doc_title ");
    escape_string(fp, doc->title);
    fprintf(fp, "\n");
  }

  // Globals
  for (int i = 0; i < 64; i++) {
    if (doc->globals[i].in_use) {
      fprintf(fp, "set_global %s ", doc->globals[i].key);
      escape_string(fp, doc->globals[i].value);
      fprintf(fp, "\n");
    }
  }

  // Pages and blocks
  for (int pi = 0; pi < 32; pi++) {
    const Page *page = &doc->pages[pi];
    if (!page->in_use)
      continue;

    // Create page
    fprintf(fp, "page_create %u ", page->id);
    escape_string(fp, page->name);
    fprintf(fp, "\n");

    // Blocks
    for (Block *b = page->first_block; b; b = b->next) {
      // Create block
      fprintf(fp, "block_create %u %u %s\n", page->id, b->id,
              block_type_to_string(b->type));

      // Text content
      if (b->text && b->text_len > 0) {
        fprintf(fp, "block_text %u %u ", page->id, b->id);
        escape_string(fp, b->text);
        fprintf(fp, "\n");
      }

      // Collapse state
      if (b->is_collapsed) {
        fprintf(fp, "block_collapse %u %u 1\n", page->id, b->id);
      }

      // Highlight
      if (b->highlight_color > 0) {
        fprintf(fp, "block_highlight %u %u %d\n", page->id, b->id,
                b->highlight_color);
      }

      // Type-specific data
      switch (b->type) {
      case BLOCK_TABLE: {
        const TableData *td = &b->data.table;
        if (td->columnar) {
          size_t rows = aslan_table_view_count(td->columnar);
          size_t cols = td->columnar->column_count;
          fprintf(fp, "block_table_init %u %u %zu %zu\n", page->id, b->id, rows,
                  cols);
          fprintf(fp, "block_table_view %u %u %s\n", page->id, b->id,
                  td->view_mode == TABLE_VIEW_LIST    ? "list"
                  : td->view_mode == TABLE_VIEW_BOARD ? "board"
                                                      : "table");
          // Save column widths (non-default only)
          for (size_t cw = 0; cw < cols; cw++) {
            float w = (float)td->columnar->columns[cw].width_px;
            if (w > 0 && (w < 119.0f || w > 121.0f)) {
              fprintf(fp, "block_table_col_width %u %u %zu %.0f\n", page->id,
                      b->id, cw, w);
            }
          }
          // Save cell data using columnar API
          for (size_t r = 0; r < rows; r++) {
            uint64_t row_id = aslan_table_view_row_id(td->columnar, (int)r);
            for (size_t c = 0; c < cols; c++) {
              char buf[1024];
              const char *cell = aslan_table_get_cell_display(
                  td->columnar, (int)c, row_id, buf, sizeof(buf));
              if (cell && cell[0]) {
                fprintf(fp, "block_table_cell %u %u %zu %zu ", page->id, b->id,
                        r, c);
                escape_string(fp, cell);
                fprintf(fp, "\n");
              }
            }
          }
        }
        break;
      }
      case BLOCK_CANVAS: {
        const CanvasData *cd = &b->data.canvas;
        fprintf(fp, "block_canvas_view %u %u %.2f %.2f %.2f\n", page->id, b->id,
                cd->view_x, cd->view_y, cd->zoom);
        if (cd->root) {
          for (Node *n = cd->root->first_child; n; n = n->next_sibling) {
            save_node_recursive(fp, page->id, b->id, "block_canvas", n);
          }
        }
        break;
      }
      case BLOCK_DESIGN: {
        const DesignData *dd = &b->data.design;
        fprintf(fp, "block_design_init %u %u %d %d\n", page->id, b->id,
                dd->width, dd->height);
        fprintf(fp, "block_design_bg %u %u %u\n", page->id, b->id,
                dd->bg_color);
        if (dd->root) {
          for (Node *n = dd->root->first_child; n; n = n->next_sibling) {
            save_node_recursive(fp, page->id, b->id, "block_design", n);
          }
        }
        break;
      }
      case BLOCK_CHART: {
        const ChartData *cd = &b->data.chart;
        const char *type_str = cd->type == CHART_LINE  ? "line"
                               : cd->type == CHART_PIE ? "pie"
                                                       : "bar";
        fprintf(fp, "block_chart_init %u %u %s %u %d %d\n", page->id, b->id,
                type_str, cd->source_table_id, cd->label_col_idx,
                cd->value_col_idx);
        if (cd->color != 0x3778FAFF) {
          fprintf(fp, "block_chart_color %u %u %u\n", page->id, b->id,
                  cd->color);
        }
        if (!cd->show_values) {
          fprintf(fp, "block_chart_show_values %u %u 0\n", page->id, b->id);
        }
        break;
      }
      default:
        break;
      }
    }
  }

  // Active page
  if (doc->active_page_idx >= 0 && doc->pages[doc->active_page_idx].in_use) {
    fprintf(fp, "page_activate %u\n", doc->pages[doc->active_page_idx].id);
  }
}

void doc_load_aof(Document *doc, FILE *fp) {
  // AOF loading is complex - placeholder for now
  // Would parse line by line and execute commands
  (void)doc;
  (void)fp;
}

void doc_save_snapshot(const Document *doc, const char *filepath) {
  if (!doc || !filepath)
    return;

  FILE *fp = fopen(filepath, "w");
  if (!fp)
    return;

  doc_save_aof(doc, fp);
  fclose(fp);
}

Document *doc_load_snapshot(const char *filepath) {
  if (!filepath)
    return NULL;

  FILE *fp = fopen(filepath, "r");
  if (!fp)
    return NULL;

  Document *doc = doc_create();
  if (doc) {
    doc_load_aof(doc, fp);
  }

  fclose(fp);
  return doc;
}

// -----------------------------------------------------------------------------
// Export
// -----------------------------------------------------------------------------

void doc_export_svg(const Document *doc, const char *filepath) {
  if (!doc || !filepath)
    return;

  FILE *f = fopen(filepath, "w");
  if (!f)
    return;

  fprintf(f, "<?xml version='1.0' encoding='UTF-8'?>\n");
  fprintf(f,
          "<svg width='%d' height='%d' xmlns='http://www.w3.org/2000/svg'>\n",
          doc->doc_width, doc->doc_height);
  fprintf(f, "  <rect width='100%%' height='100%%' fill='#ffffff'/>\n");
  fprintf(f, "  <text x='20' y='40' font-family='sans-serif' font-size='16'>");
  fprintf(f, "ASLAN Document Export</text>\n");
  fprintf(f, "</svg>\n");

  fclose(f);
}

void doc_export_pdf(const Document *doc, const char *filepath) {
  // PDF export would require a library - placeholder
  (void)doc;
  (void)filepath;
}
