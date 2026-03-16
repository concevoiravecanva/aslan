#include "document.h"
#include "aslan_table.h"
#include <hpdf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// ASLAN DOCUMENT MODEL v3 - Unified Block-Based Implementation
// =============================================================================

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------

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
  if (strcmp(str, "design") == 0)
    return BLOCK_DESIGN;
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
  block->layout_row = (uint16_t)page->block_count;
  block->layout_col = 0;

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
  block->layout_row = (uint16_t)page->block_count;
  block->layout_col = 0;

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
  case BLOCK_DESIGN:
    block_design_destroy(block);
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
  case BLOCK_DESIGN:
    block_design_destroy(block);
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

  TableData *td = &block->data.table;

  // Free existing columnar table
  if (td->columnar) {
    aslan_table_destroy(td->columnar);
    td->columnar = NULL;
  }

  // Create new columnar table
  td->columnar = aslan_table_create(block->id, "Table");

  if (td->columnar) {
    // Default 2 columns: Name (Text) and Number (Number)
    aslan_table_add_column(td->columnar, "Name", ASLAN_COL_TEXT);
    aslan_table_add_column(td->columnar, "Number", ASLAN_COL_NUMBER);

    // Add initial rows
    for (int r = 0; r < rows; r++) {
      aslan_table_add_row(td->columnar);
    }
  }

  td->active_row = 0;
  td->active_col = 0;
  td->view_mode = TABLE_VIEW_TABLE;
  td->show_search = false;
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
  node->opacity = 1.0f;
  node->lock_ratio = 0;

  node->has_shadow = 0;
  node->shadow_offset_x = 0.0f;
  node->shadow_offset_y = 10.0f;
  node->shadow_blur = 5.0f;
  node->shadow_color = 0x00000040; // 25% black

  node->fill_type = 0;
  node->fill_color_2 = 0xFFFFFFFF;
  node->gradient_angle = 90.0f;

  node->clip_content = 0;
  node->layout_type = 0;
  node->layout_padding = 10.0f;

  // Typography defaults
  node->text_align = 0;        // Left
  node->line_height = 1.2f;    // Default leading
  node->letter_spacing = 0.0f; // Normal tracking

  // Image defaults
  node->image_scale = 1.0f;
  node->image_crop_x = 0.0f;
  node->image_crop_y = 0.0f;

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

void node_insert_before(Node *node, Node *target) {
  if (!node || !target || node == target || !target->parent)
    return;
  // Remove node from its current position
  node_remove_from_parent(node);
  node->parent = target->parent;

  Node *parent = target->parent;
  if (parent->first_child == target) {
    node->next_sibling = target;
    parent->first_child = node;
    return;
  }

  // Find node before target
  Node *prev = parent->first_child;
  while (prev && prev->next_sibling != target) {
    prev = prev->next_sibling;
  }

  if (prev) {
    prev->next_sibling = node;
    node->next_sibling = target;
  }
}

void node_insert_after(Node *node, Node *target) {
  if (!node || !target || node == target || !target->parent)
    return;
  // Remove node from its current position
  node_remove_from_parent(node);
  node->parent = target->parent;

  Node *next = target->next_sibling;
  target->next_sibling = node;
  node->next_sibling = next;
}

// (Canvas Block API removed — Canvas block type has been deleted)

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
  dd->pages[idx].bg_type = 0;
  dd->pages[idx].bg_color_2 = 0xFFFFFFFF;
  dd->pages[idx].grid_type = 0;
  dd->pages[idx].grid_spacing = 40.0f;
  dd->pages[idx].h_guide_count = 0;
  dd->pages[idx].v_guide_count = 0;
  dd->pages[idx].show_rulers = 0;
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

Node *block_design_duplicate_node(Block *block, uint32_t node_id) {
  if (!block || block->type != BLOCK_DESIGN)
    return NULL;

  Node *orig = block_design_find_node(block, node_id);
  if (!orig || orig->type == NODE_TYPE_ROOT || !orig->parent)
    return NULL;

  DesignData *dd = &block->data.design;

  // Shallow copy of properties (no child tree)
  Node *dup = (Node *)calloc(1, sizeof(Node));
  if (!dup)
    return NULL;

  *dup = *orig;
  dup->id = dd->next_node_id++;
  dup->x += 12.0f;
  dup->y += 12.0f;
  // Clear hierarchy and non-trivial data pointers (shallow copy)
  dup->parent = NULL;
  dup->first_child = NULL;
  dup->next_sibling = NULL;
  dup->chart_data = NULL;
  dup->stroke_data = NULL;
  dup->path_points = NULL;
  dup->path_point_count = 0;

  // Insert immediately after the original in the parent
  node_add_child(orig->parent, dup);
  return dup;
}

void block_design_set_size(Block *block, float width, float height) {
  if (!block || block->type != BLOCK_DESIGN)
    return;

  // Enforce min/max bounds (100x100 to 4000x4000)
  if (width < 100.0f)
    width = 100.0f;
  if (width > 4000.0f)
    width = 4000.0f;
  if (height < 100.0f)
    height = 100.0f;
  if (height > 4000.0f)
    height = 4000.0f;

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

  // Linked column (design only)
  if (node->linked_col_name[0]) {
    fprintf(fp, "%s_node %u %u linked_col %u ", prefix, page_id, block_id,
            node->id);
    escape_string(fp, node->linked_col_name);
    fprintf(fp, "\n");
  }

  // Recurse to children and siblings
  for (Node *child = node->first_child; child; child = child->next_sibling) {
    save_node_recursive(fp, page_id, block_id, prefix, child);
  }
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

      // Layout (multi-column)
      if (b->layout_row != 0 || b->layout_col != 0) {
        fprintf(fp, "block_layout %u %u %u %u\n", page->id, b->id,
                (unsigned)b->layout_row, (unsigned)b->layout_col);
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
          fprintf(fp, "block_table_view %u %u table\n", page->id, b->id);
          // Save column info (type + name) for non-default columns
          for (size_t ci = 0; ci < cols; ci++) {
            AslanColumn *acol = &td->columnar->columns[ci];
            fprintf(fp, "block_table_col_info %u %u %zu %d ", page->id, b->id,
                    ci, (int)acol->type);
            escape_string(fp, acol->name);
            fprintf(fp, "\n");
            // Save formula expression
            if (acol->type == ASLAN_COL_FORMULA && acol->formula &&
                acol->formula[0]) {
              fprintf(fp, "block_table_col_formula %u %u %zu ", page->id, b->id,
                      ci);
              escape_string(fp, acol->formula);
              fprintf(fp, "\n");
            }
          }
          // Save column widths (non-default only)
          for (size_t cw = 0; cw < cols; cw++) {
            float w = (float)td->columnar->columns[cw].width_px;
            if (w > 0 && (w < 119.0f || w > 121.0f)) {
              fprintf(fp, "block_table_col_width %u %u %zu %.0f\n", page->id,
                      b->id, cw, w);
            }
          }
          // Save column background colors (non-zero only)
          for (size_t cc = 0; cc < cols; cc++) {
            uint32_t bgc = td->columnar->columns[cc].bg_color;
            if (bgc != 0) {
              fprintf(fp, "block_table_col_color %u %u %zu %u\n", page->id,
                      b->id, cc, bgc);
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
      case BLOCK_DESIGN: {
        const DesignData *dd = &b->data.design;
        fprintf(fp, "block_design_init %u %u %.2f %.2f\n", page->id, b->id,
                dd->width, dd->height);
        if (dd->units != 0) {
          fprintf(fp, "block_design_resize %u %u %.2f %.2f %d\n", page->id,
                  b->id, dd->width, dd->height, dd->units);
        }
        fprintf(fp, "block_design_bg %u %u %u\n", page->id, b->id,
                dd->bg_color);
        if (dd->no_limits) {
          fprintf(fp, "block_design_nolimits %u %u 1\n", page->id, b->id);
          fprintf(fp, "block_design_view %u %u %.2f %.2f %.2f\n", page->id,
                  b->id, dd->view_x, dd->view_y, dd->zoom);
        }
        if (dd->linked_table_id) {
          fprintf(fp, "block_design_link %u %u %u\n", page->id, b->id,
                  dd->linked_table_id);
        }
        if (dd->root) {
          for (Node *n = dd->root->first_child; n; n = n->next_sibling) {
            save_node_recursive(fp, page->id, b->id, "block_design", n);
          }
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

// =============================================================================
// SVG Export
// =============================================================================

// Convert RGBA uint32 to SVG color string: "rgba(r,g,b,a)"
static void svg_color(uint32_t rgba, char *buf, size_t len) {
  uint8_t r = (rgba >> 24) & 0xFF;
  uint8_t g = (rgba >> 16) & 0xFF;
  uint8_t b = (rgba >> 8) & 0xFF;
  uint8_t a = rgba & 0xFF;
  if (a == 255)
    snprintf(buf, len, "rgb(%d,%d,%d)", r, g, b);
  else
    snprintf(buf, len, "rgba(%d,%d,%d,%.2f)", r, g, b, a / 255.0);
}

// Write SVG for a single node (recursive for children)
static void svg_write_node(FILE *fp, const Node *node,
                           struct AslanTable *linked_tbl,
                           uint64_t linked_row_id) {
  if (!node || !node->visible)
    return;

  char fill[64], stroke[64];
  svg_color(node->fill_color, fill, sizeof(fill));
  svg_color(node->stroke_color, stroke, sizeof(stroke));

  // Rotation transform
  char transform[128] = "";
  if (node->rotation != 0.0f) {
    float cx = node->x + node->width / 2.0f;
    float cy = node->y + node->height / 2.0f;
    snprintf(transform, sizeof(transform),
             " transform=\"rotate(%.1f %.1f %.1f)\"", node->rotation, cx, cy);
  }

  switch (node->type) {
  case NODE_TYPE_RECT:
  case NODE_TYPE_FRAME:
    fprintf(
        fp,
        "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\""
        " rx=\"%.1f\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
        node->x, node->y, node->width, node->height, node->corner_radius, fill,
        stroke, node->stroke_width, transform);
    break;

  case NODE_TYPE_ELLIPSE: {
    float cx = node->x + node->width / 2.0f;
    float cy = node->y + node->height / 2.0f;
    fprintf(fp,
            "  <ellipse cx=\"%.1f\" cy=\"%.1f\" rx=\"%.1f\" ry=\"%.1f\""
            " fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
            cx, cy, node->width / 2.0f, node->height / 2.0f, fill, stroke,
            node->stroke_width, transform);
    break;
  }

  case NODE_TYPE_TEXT: {
    fprintf(fp,
            "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"%.1f\""
            " fill=\"%s\"%s>",
            node->x, node->y + node->font_size, node->font_size, fill,
            transform);
    const char *display_text = node->text;
    char linked_val_buf[256];
    if (node->linked_col_name[0] && linked_tbl) {
      for (int lk = 0; lk < (int)linked_tbl->column_count; lk++) {
        if (strcmp(linked_tbl->columns[lk].name, node->linked_col_name) == 0) {
          const char *val = aslan_table_get_cell_display(
              linked_tbl, lk, linked_row_id, linked_val_buf,
              sizeof(linked_val_buf));
          if (val && val[0])
            display_text = val;
          break;
        }
      }
    }
    // Escape XML entities in text
    for (const char *p = display_text; *p; p++) {
      switch (*p) {
      case '<':
        fputs("&lt;", fp);
        break;
      case '>':
        fputs("&gt;", fp);
        break;
      case '&':
        fputs("&amp;", fp);
        break;
      case '"':
        fputs("&quot;", fp);
        break;
      default:
        fputc(*p, fp);
        break;
      }
    }
    fprintf(fp, "</text>\n");
    break;
  }

  case NODE_TYPE_LINE:
    fprintf(fp,
            "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\""
            " stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
            node->x, node->y, node->x + node->width, node->y + node->height,
            stroke, node->stroke_width, transform);
    break;

  case NODE_TYPE_TRIANGLE: {
    float x = node->x, y = node->y, w = node->width, h = node->height;
    fprintf(fp,
            "  <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\""
            " fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
            x + w / 2, y, x, y + h, x + w, y + h, fill, stroke,
            node->stroke_width, transform);
    break;
  }

  case NODE_TYPE_PENTAGON: {
    float cx = node->x + node->width / 2, cy = node->y + node->height / 2;
    float rx = node->width / 2, ry = node->height / 2;
    fprintf(fp, "  <polygon points=\"");
    for (int i = 0; i < 5; i++) {
      double a = (i * 2 * 3.14159265 / 5) - 3.14159265 / 2;
      fprintf(fp, "%.1f,%.1f ", cx + rx * cos(a), cy + ry * sin(a));
    }
    fprintf(fp, "\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
            fill, stroke, node->stroke_width, transform);
    break;
  }

  case NODE_TYPE_HEXAGON: {
    float cx = node->x + node->width / 2, cy = node->y + node->height / 2;
    float rx = node->width / 2, ry = node->height / 2;
    fprintf(fp, "  <polygon points=\"");
    for (int i = 0; i < 6; i++) {
      double a = (i * 2 * 3.14159265 / 6) - 3.14159265 / 2;
      fprintf(fp, "%.1f,%.1f ", cx + rx * cos(a), cy + ry * sin(a));
    }
    fprintf(fp, "\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
            fill, stroke, node->stroke_width, transform);
    break;
  }

  case NODE_TYPE_OCTAGON: {
    float cx = node->x + node->width / 2, cy = node->y + node->height / 2;
    float rx = node->width / 2, ry = node->height / 2;
    fprintf(fp, "  <polygon points=\"");
    for (int i = 0; i < 8; i++) {
      double a = (i * 2 * 3.14159265 / 8) - 3.14159265 / 2;
      fprintf(fp, "%.1f,%.1f ", cx + rx * cos(a), cy + ry * sin(a));
    }
    fprintf(fp, "\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
            fill, stroke, node->stroke_width, transform);
    break;
  }

  case NODE_TYPE_STAR: {
    float cx = node->x + node->width / 2, cy = node->y + node->height / 2;
    float rx = node->width / 2, ry = node->height / 2;
    fprintf(fp, "  <polygon points=\"");
    for (int i = 0; i < 10; i++) {
      double a = (i * 2 * 3.14159265 / 10) - 3.14159265 / 2;
      float r = (i % 2 == 0) ? 1.0f : 0.4f;
      fprintf(fp, "%.1f,%.1f ", cx + rx * r * cos(a), cy + ry * r * sin(a));
    }
    fprintf(fp, "\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
            fill, stroke, node->stroke_width, transform);
    break;
  }

  case NODE_TYPE_HEART: {
    // Heart as a simplified SVG path
    float x = node->x, y = node->y, w = node->width, h = node->height;
    fprintf(fp,
            "  <path d=\"M%.1f,%.1f C%.1f,%.1f %.1f,%.1f %.1f,%.1f"
            " C%.1f,%.1f %.1f,%.1f %.1f,%.1f Z\""
            " fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
            x + w / 2, y + h * 0.3, x + w * 0.1, y, x, y + h * 0.4, x + w / 2,
            y + h, x + w, y + h * 0.4, x + w * 0.9, y, x + w / 2, y + h * 0.3,
            fill, stroke, node->stroke_width, transform);
    break;
  }

  case NODE_TYPE_PATH:
    if (node->path_points && node->path_point_count > 0) {
      fprintf(fp, "  <path d=\"M%.1f,%.1f", node->x + node->path_points[0].x,
              node->y + node->path_points[0].y);
      for (int i = 1; i < node->path_point_count; i++) {
        NodePathPoint *pt = &node->path_points[i];
        if (pt->type == 1) {
          NodePathPoint *prev = &node->path_points[i - 1];
          fprintf(fp, " C%.1f,%.1f %.1f,%.1f %.1f,%.1f", node->x + prev->cp2x,
                  node->y + prev->cp2y, node->x + pt->cp1x, node->y + pt->cp1y,
                  node->x + pt->x, node->y + pt->y);
        } else {
          fprintf(fp, " L%.1f,%.1f", node->x + pt->x, node->y + pt->y);
        }
      }
      fprintf(fp, "\" fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
              fill, stroke, node->stroke_width, transform);
    }
    break;

  case NODE_TYPE_STROKE:
    if (node->stroke_data) {
      Stroke *s = node->stroke_data;
      if (s->count > 0) {
        fprintf(fp, "  <polyline points=\"");
        for (int i = 0; i < s->count; i++) {
          fprintf(fp, "%.1f,%.1f ", s->points[i].x, s->points[i].y);
        }
        fprintf(fp,
                "\" fill=\"none\" stroke=\"%s\" stroke-width=\"%.1f\""
                " stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n",
                stroke, node->stroke_width > 0 ? node->stroke_width : 2.0f);
      }
    }
    break;

  case NODE_TYPE_IMAGE: {
    // Embed image as base64 data URI
    if (node->image_path[0]) {
      FILE *img = fopen(node->image_path, "rb");
      if (img) {
        fseek(img, 0, SEEK_END);
        long img_size = ftell(img);
        fseek(img, 0, SEEK_SET);
        if (img_size > 0 && img_size < 10 * 1024 * 1024) {
          unsigned char *img_data = (unsigned char *)malloc(img_size);
          if (img_data &&
              fread(img_data, 1, img_size, img) == (size_t)img_size) {
            // Detect MIME type from extension
            const char *mime = "image/png";
            const char *ext = strrchr(node->image_path, '.');
            if (ext) {
              if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
                mime = "image/jpeg";
              else if (strcmp(ext, ".gif") == 0)
                mime = "image/gif";
              else if (strcmp(ext, ".webp") == 0)
                mime = "image/webp";
            }
            // Base64 encode
            static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn"
                                      "opqrstuvwxyz0123456789+/";
            fprintf(
                fp,
                "  <image x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\""
                " href=\"data:%s;base64,",
                node->x, node->y, node->width, node->height, mime);
            for (long i = 0; i < img_size; i += 3) {
              unsigned int n = (unsigned int)img_data[i] << 16;
              if (i + 1 < img_size)
                n |= (unsigned int)img_data[i + 1] << 8;
              if (i + 2 < img_size)
                n |= (unsigned int)img_data[i + 2];
              fputc(b64[(n >> 18) & 0x3F], fp);
              fputc(b64[(n >> 12) & 0x3F], fp);
              fputc((i + 1 < img_size) ? b64[(n >> 6) & 0x3F] : '=', fp);
              fputc((i + 2 < img_size) ? b64[n & 0x3F] : '=', fp);
            }
            fprintf(fp, "\" preserveAspectRatio=\"none\"%s/>\n", transform);
          }
          free(img_data);
        }
        fclose(img);
      } else {
        // Image file not found — draw placeholder
        fprintf(fp,
                "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\""
                " fill=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\"%s/>\n",
                node->x, node->y, node->width, node->height, fill, stroke,
                node->stroke_width, transform);
        fprintf(
            fp,
            "  <text x=\"%.1f\" y=\"%.1f\" font-size=\"12\""
            " fill=\"rgb(120,120,120)\" text-anchor=\"middle\">Image</text>\n",
            node->x + node->width / 2, node->y + node->height / 2 + 4);
      }
    }
    break;
  }

  case NODE_TYPE_CHART:
  case NODE_TYPE_TABLE_VIEW: {
    // Try to embed pre-rendered snapshot from /tmp/
    char snap_path[256];
    snprintf(snap_path, sizeof(snap_path), "/tmp/aslan_node_%u.png", node->id);
    FILE *snap = fopen(snap_path, "rb");
    if (snap) {
      fseek(snap, 0, SEEK_END);
      long snap_size = ftell(snap);
      fseek(snap, 0, SEEK_SET);
      if (snap_size > 0 && snap_size < 10 * 1024 * 1024) {
        unsigned char *snap_data = (unsigned char *)malloc(snap_size);
        if (snap_data &&
            fread(snap_data, 1, snap_size, snap) == (size_t)snap_size) {
          static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnop"
                                    "qrstuvwxyz0123456789+/";
          fprintf(
              fp,
              "  <image x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\""
              " href=\"data:image/png;base64,",
              node->x, node->y, node->width, node->height);
          for (long i = 0; i < snap_size; i += 3) {
            unsigned int n = (unsigned int)snap_data[i] << 16;
            if (i + 1 < snap_size)
              n |= (unsigned int)snap_data[i + 1] << 8;
            if (i + 2 < snap_size)
              n |= (unsigned int)snap_data[i + 2];
            fputc(b64[(n >> 18) & 0x3F], fp);
            fputc(b64[(n >> 12) & 0x3F], fp);
            fputc((i + 1 < snap_size) ? b64[(n >> 6) & 0x3F] : '=', fp);
            fputc((i + 2 < snap_size) ? b64[n & 0x3F] : '=', fp);
          }
          fprintf(fp, "\" preserveAspectRatio=\"none\"%s/>\n", transform);
        }
        free(snap_data);
      }
      fclose(snap);
    }
    break;
  }

  default:
    // GROUP, ROOT — skip
    break;
  }

  // Recurse into children (for FRAME, GROUP, ROOT)
  for (Node *child = node->first_child; child; child = child->next_sibling) {
    svg_write_node(fp, child, linked_tbl, linked_row_id);
  }
}

bool block_design_export_svg(const Block *block, const char *filepath,
                             struct AslanTable *linked_tbl,
                             uint64_t linked_row_id) {
  if (!block || block->type != BLOCK_DESIGN || !filepath)
    return false;

  const DesignData *dd = &block->data.design;
  if (!dd->pages || dd->current_page < 0 || dd->current_page >= dd->page_count)
    return false;

  const DesignPage *page = &dd->pages[dd->current_page];
  if (!page->root)
    return false;

  FILE *fp = fopen(filepath, "w");
  if (!fp)
    return false;

  int w = dd->width;
  int h = dd->height;
  char bg[64];
  uint32_t bg_color = page->bg_color ? page->bg_color : 0xFFFFFFFF;
  svg_color(bg_color, bg, sizeof(bg));

  fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(fp,
          "<svg xmlns=\"http://www.w3.org/2000/svg\" "
          "width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n",
          w, h, w, h);

  if (page->bg_type > 0) {
    char bg2[64];
    svg_color(page->bg_color_2, bg2, sizeof(bg2));
    fprintf(fp, "  <defs>\n");
    if (page->bg_type == 1) {
      fprintf(fp, "    <linearGradient id=\"page_bg\" x1=\"0%%\" y1=\"0%%\" "
                  "x2=\"0%%\" y2=\"100%%\">\n");
      fprintf(fp, "      <stop offset=\"0%%\" stop-color=\"%s\"/>\n", bg);
      fprintf(fp, "      <stop offset=\"100%%\" stop-color=\"%s\"/>\n", bg2);
      fprintf(fp, "    </linearGradient>\n");
    } else {
      fprintf(fp, "    <radialGradient id=\"page_bg\" cx=\"50%%\" cy=\"50%%\" "
                  "r=\"50%%\">\n");
      fprintf(fp, "      <stop offset=\"0%%\" stop-color=\"%s\"/>\n", bg2);
      fprintf(fp, "      <stop offset=\"100%%\" stop-color=\"%s\"/>\n", bg);
      fprintf(fp, "    </radialGradient>\n");
    }
    fprintf(fp, "  </defs>\n");
    fprintf(fp, "  <rect width=\"%d\" height=\"%d\" fill=\"url(#page_bg)\"/>\n",
            w, h);
  } else {
    fprintf(fp, "  <rect width=\"%d\" height=\"%d\" fill=\"%s\"/>\n", w, h, bg);
  }

  // Export all nodes in current page
  for (Node *node = page->root->first_child; node; node = node->next_sibling) {
    svg_write_node(fp, node, linked_tbl, linked_row_id);
  }

  fprintf(fp, "</svg>\n");
  fclose(fp);
  return true;
}

// =============================================================================
// PDF EXPORT (libharu)
// =============================================================================

static void pdf_set_fill_color(HPDF_Page page, uint32_t rgba) {
  float r = ((rgba >> 24) & 0xFF) / 255.0f;
  float g = ((rgba >> 16) & 0xFF) / 255.0f;
  float b = ((rgba >>  8) & 0xFF) / 255.0f;
  HPDF_Page_SetRGBFill(page, r, g, b);
}

static void pdf_set_stroke_color(HPDF_Page page, uint32_t rgba) {
  float r = ((rgba >> 24) & 0xFF) / 255.0f;
  float g = ((rgba >> 16) & 0xFF) / 255.0f;
  float b = ((rgba >>  8) & 0xFF) / 255.0f;
  HPDF_Page_SetRGBStroke(page, r, g, b);
}

static void pdf_write_node(HPDF_Page page, HPDF_Doc pdf, float page_h,
                           const Node *node, struct AslanTable *linked_tbl,
                           uint64_t linked_row_id) {
  if (!node || !node->visible)
    return;

  float x = node->x;
  float y = page_h - node->y - node->height;
  float w = node->width;
  float h = node->height;

  pdf_set_fill_color(page, node->fill_color);
  pdf_set_stroke_color(page, node->stroke_color);
  if (node->stroke_width > 0)
    HPDF_Page_SetLineWidth(page, node->stroke_width);

  switch (node->type) {
  case NODE_TYPE_RECT:
  case NODE_TYPE_FRAME:
    HPDF_Page_Rectangle(page, x, y, w, h);
    if (node->stroke_width > 0) HPDF_Page_FillStroke(page);
    else                        HPDF_Page_Fill(page);
    break;

  case NODE_TYPE_ELLIPSE: {
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float rx = w * 0.5f,     ry = h * 0.5f;
    float kx = rx * 0.5523f, ky = ry * 0.5523f;
    HPDF_Page_MoveTo(page, cx, cy + ry);
    HPDF_Page_CurveTo(page, cx+kx, cy+ry, cx+rx, cy+ky, cx+rx, cy);
    HPDF_Page_CurveTo(page, cx+rx, cy-ky, cx+kx, cy-ry, cx,    cy-ry);
    HPDF_Page_CurveTo(page, cx-kx, cy-ry, cx-rx, cy-ky, cx-rx, cy);
    HPDF_Page_CurveTo(page, cx-rx, cy+ky, cx-kx, cy+ry, cx,    cy+ry);
    if (node->stroke_width > 0) HPDF_Page_FillStroke(page);
    else                        HPDF_Page_Fill(page);
    break;
  }

  case NODE_TYPE_TEXT: {
    HPDF_Font font = HPDF_GetFont(pdf, "Helvetica", NULL);
    float fs = node->font_size > 0 ? node->font_size : 14.0f;
    HPDF_Page_SetFontAndSize(page, font, fs);
    pdf_set_fill_color(page, node->fill_color);
    const char *display_text = node->text;
    char linked_val_buf[256];
    if (node->linked_col_name[0] && linked_tbl) {
      for (int lk = 0; lk < (int)linked_tbl->column_count; lk++) {
        if (strcmp(linked_tbl->columns[lk].name, node->linked_col_name) == 0) {
          const char *val = aslan_table_get_cell_display(
              linked_tbl, lk, linked_row_id,
              linked_val_buf, sizeof(linked_val_buf));
          if (val && val[0]) display_text = val;
          break;
        }
      }
    }
    HPDF_Page_BeginText(page);
    HPDF_Page_TextOut(page, node->x, page_h - node->y - fs, display_text);
    HPDF_Page_EndText(page);
    break;
  }

  case NODE_TYPE_LINE:
    HPDF_Page_MoveTo(page, node->x, page_h - node->y);
    HPDF_Page_LineTo(page, node->x + node->width,
                     page_h - node->y - node->height);
    HPDF_Page_Stroke(page);
    break;

  case NODE_TYPE_TRIANGLE: {
    float nx = node->x, ny = node->y;
    HPDF_Page_MoveTo(page, nx + w * 0.5f, page_h - ny);
    HPDF_Page_LineTo(page, nx,            page_h - ny - h);
    HPDF_Page_LineTo(page, nx + w,        page_h - ny - h);
    HPDF_Page_ClosePath(page);
    if (node->stroke_width > 0) HPDF_Page_FillStroke(page);
    else                        HPDF_Page_Fill(page);
    break;
  }

  case NODE_TYPE_PENTAGON:
  case NODE_TYPE_HEXAGON:
  case NODE_TYPE_OCTAGON: {
    int sides = (node->type == NODE_TYPE_PENTAGON) ? 5
              : (node->type == NODE_TYPE_HEXAGON)  ? 6 : 8;
    float cx = node->x + w * 0.5f;
    float cy_pdf = page_h - node->y - h * 0.5f;
    float rx = w * 0.5f, ry = h * 0.5f;
    for (int i = 0; i < sides; i++) {
      double a = (i * 2.0 * 3.14159265 / sides) - 3.14159265 / 2.0;
      float px = cx + rx * (float)cos(a);
      float py = cy_pdf + ry * (float)sin(a);
      if (i == 0) HPDF_Page_MoveTo(page, px, py);
      else        HPDF_Page_LineTo(page, px, py);
    }
    HPDF_Page_ClosePath(page);
    if (node->stroke_width > 0) HPDF_Page_FillStroke(page);
    else                        HPDF_Page_Fill(page);
    break;
  }

  case NODE_TYPE_STAR: {
    float cx = node->x + w * 0.5f;
    float cy_pdf = page_h - node->y - h * 0.5f;
    float rx = w * 0.5f, ry = h * 0.5f;
    for (int i = 0; i < 10; i++) {
      double a = (i * 2.0 * 3.14159265 / 10.0) - 3.14159265 / 2.0;
      float r = (i % 2 == 0) ? 1.0f : 0.4f;
      float px = cx + rx * r * (float)cos(a);
      float py = cy_pdf + ry * r * (float)sin(a);
      if (i == 0) HPDF_Page_MoveTo(page, px, py);
      else        HPDF_Page_LineTo(page, px, py);
    }
    HPDF_Page_ClosePath(page);
    if (node->stroke_width > 0) HPDF_Page_FillStroke(page);
    else                        HPDF_Page_Fill(page);
    break;
  }

  case NODE_TYPE_PATH:
    if (node->path_points && node->path_point_count > 0) {
      HPDF_Page_MoveTo(page,
          node->x + node->path_points[0].x,
          page_h - node->y - node->path_points[0].y);
      for (int i = 1; i < node->path_point_count; i++) {
        NodePathPoint *pt   = &node->path_points[i];
        NodePathPoint *prev = &node->path_points[i - 1];
        if (pt->type == 1)
          HPDF_Page_CurveTo(page,
              node->x + prev->cp2x, page_h - node->y - prev->cp2y,
              node->x + pt->cp1x,  page_h - node->y - pt->cp1y,
              node->x + pt->x,     page_h - node->y - pt->y);
        else
          HPDF_Page_LineTo(page,
              node->x + pt->x, page_h - node->y - pt->y);
      }
      HPDF_Page_ClosePath(page);
      if (node->stroke_width > 0) HPDF_Page_FillStroke(page);
      else                        HPDF_Page_Fill(page);
    }
    break;

  case NODE_TYPE_STROKE:
    if (node->stroke_data && node->stroke_data->count > 0) {
      Stroke *s = node->stroke_data;
      HPDF_Page_MoveTo(page, s->points[0].x, page_h - s->points[0].y);
      for (int i = 1; i < s->count; i++)
        HPDF_Page_LineTo(page, s->points[i].x, page_h - s->points[i].y);
      HPDF_Page_Stroke(page);
    }
    break;

  case NODE_TYPE_IMAGE:
    if (node->image_path[0]) {
      HPDF_Image img = NULL;
      const char *ext = strrchr(node->image_path, '.');
      if (ext) {
        if (strcmp(ext, ".png") == 0)
          img = HPDF_LoadPngImageFromFile(pdf, node->image_path);
        else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
          img = HPDF_LoadJpegImageFromFile(pdf, node->image_path);
      }
      if (img)
        HPDF_Page_DrawImage(page, img, x, y, w, h);
      else {
        // Fallback: grey placeholder rectangle
        pdf_set_fill_color(page, 0xF0F0F0FF);
        HPDF_Page_Rectangle(page, x, y, w, h);
        HPDF_Page_Fill(page);
      }
    }
    break;

  case NODE_TYPE_CHART:
  case NODE_TYPE_TABLE_VIEW: {
    // Use pre-rendered snapshot if available (written by prerender_special_nodes)
    char snap_path[256];
    snprintf(snap_path, sizeof(snap_path), "/tmp/aslan_node_%u.png", node->id);
    HPDF_Image snap = HPDF_LoadPngImageFromFile(pdf, snap_path);
    if (snap)
      HPDF_Page_DrawImage(page, snap, x, y, w, h);
    break;
  }

  default:
    break;
  }

  // Recurse into children
  for (Node *child = node->first_child; child; child = child->next_sibling)
    pdf_write_node(page, pdf, page_h, child, linked_tbl, linked_row_id);
}

bool block_design_export_pdf(const Block *block, const char *filepath,
                             struct AslanTable *linked_tbl,
                             uint64_t linked_row_id) {
  if (!block || block->type != BLOCK_DESIGN || !filepath)
    return false;

  const DesignData *dd = &block->data.design;
  if (!dd->pages || dd->current_page < 0 ||
      dd->current_page >= dd->page_count)
    return false;

  const DesignPage *dp = &dd->pages[dd->current_page];
  if (!dp->root)
    return false;

  HPDF_Doc pdf = HPDF_New(NULL, NULL);
  if (!pdf)
    return false;

  float w = (float)(dd->width  > 0 ? dd->width  : 800);
  float h = (float)(dd->height > 0 ? dd->height : 600);

  HPDF_Page page = HPDF_AddPage(pdf);
  HPDF_Page_SetWidth(page, w);
  HPDF_Page_SetHeight(page, h);

  // Background
  uint32_t bg = dp->bg_color ? dp->bg_color : 0xFFFFFFFF;
  pdf_set_fill_color(page, bg);
  HPDF_Page_Rectangle(page, 0, 0, w, h);
  HPDF_Page_Fill(page);

  for (Node *node = dp->root->first_child; node; node = node->next_sibling)
    pdf_write_node(page, pdf, h, node, linked_tbl, linked_row_id);

  HPDF_SaveToFile(pdf, filepath);
  HPDF_Free(pdf);
  return true;
}

