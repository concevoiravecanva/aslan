#include "aslan_table.h"
#include "../platform/ui_shared.h"
#include "../vendor/tinyexpr.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// =============================================================================
// ASLAN COLUMNAR TABLE ENGINE - Implementation
// =============================================================================

// -----------------------------------------------------------------------------
// Memory Alignment Helpers (SIMD-ready)
// -----------------------------------------------------------------------------

static void *aligned_alloc_safe(size_t alignment, size_t size) {
  if (size == 0)
    return NULL;
  size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

#if defined(_WIN32)
  return _aligned_malloc(aligned_size, alignment);
#elif defined(__APPLE__) || defined(__unix__)
  void *ptr = NULL;
  if (posix_memalign(&ptr, alignment, aligned_size) != 0) {
    return NULL;
  }
  return ptr;
#else
  void *raw = malloc(aligned_size + alignment + sizeof(void *));
  if (!raw)
    return NULL;
  void *aligned = (void *)(((uintptr_t)raw + alignment + sizeof(void *)) &
                           ~(alignment - 1));
  ((void **)aligned)[-1] = raw;
  return aligned;
#endif
}

static void aligned_free_safe(void *ptr) {
  if (!ptr)
    return;
#if defined(_WIN32)
  _aligned_free(ptr);
#elif defined(__APPLE__) || defined(__unix__)
  free(ptr);
#else
  free(((void **)ptr)[-1]);
#endif
}

static void *aligned_realloc_safe(void *old_ptr, size_t alignment,
                                  size_t old_size, size_t new_size) {
  if (new_size == 0) {
    aligned_free_safe(old_ptr);
    return NULL;
  }
  if (!old_ptr) {
    return aligned_alloc_safe(alignment, new_size);
  }

  void *new_ptr = aligned_alloc_safe(alignment, new_size);
  if (!new_ptr)
    return NULL;

  size_t copy_size = (old_size < new_size) ? old_size : new_size;
  memcpy(new_ptr, old_ptr, copy_size);
  aligned_free_safe(old_ptr);

  return new_ptr;
}

// -----------------------------------------------------------------------------
// Column Data Size Helpers
// -----------------------------------------------------------------------------

static size_t column_element_size(AslanColumnType type) {
  switch (type) {
  case ASLAN_COL_TEXT:
    return sizeof(char *);
  case ASLAN_COL_NUMBER:
    return sizeof(double);
  case ASLAN_COL_CHECKBOX:
    return sizeof(uint8_t);
  case ASLAN_COL_SELECT:
    return sizeof(int32_t);
  case ASLAN_COL_DATE:
    return sizeof(int64_t);
  case ASLAN_COL_FORMULA:
    return sizeof(double); // Cached result (C parser)
  case ASLAN_COL_FILE:
  case ASLAN_COL_IMAGE:
  case ASLAN_COL_LINK:
    return sizeof(char *); // Heap-allocated string
  default:
    return sizeof(double);
  }
}

// -----------------------------------------------------------------------------
// Bitmask Helpers
// -----------------------------------------------------------------------------

static inline void bitmask_set(uint8_t *mask, size_t idx) {
  mask[idx / 8] |= (1 << (idx % 8));
}

static inline void bitmask_clear(uint8_t *mask, size_t idx) {
  mask[idx / 8] &= ~(1 << (idx % 8));
}

static inline bool bitmask_get(const uint8_t *mask, size_t idx) {
  return (mask[idx / 8] & (1 << (idx % 8))) != 0;
}

// -----------------------------------------------------------------------------
// Table Lifecycle
// -----------------------------------------------------------------------------

AslanTable *aslan_table_create(uint32_t id, const char *name) {
  AslanTable *table = (AslanTable *)calloc(1, sizeof(AslanTable));
  if (!table)
    return NULL;

  table->id = id;
  strncpy(table->name, name ? name : "Untitled", sizeof(table->name) - 1);

  table->column_count = 0;
  table->row_count = 0;
  table->row_capacity = 0;
  table->next_row_id = 1;
  table->active_count = 0;

  table->row_ids = NULL;
  table->active_mask = NULL;
  table->view_index = NULL;
  table->view_dirty = true;

  table->sort_column = -1;
  table->sort_ascending = true;

  table->log_callback = NULL;
  table->log_userdata = NULL;

  return table;
}

void aslan_table_destroy(AslanTable *table) {
  if (!table)
    return;

  for (size_t c = 0; c < table->column_count; c++) {
    AslanColumn *col = &table->columns[c];

    // Free string data for TEXT columns
    if (col->type == ASLAN_COL_TEXT || col->type == ASLAN_COL_FILE ||
        col->type == ASLAN_COL_IMAGE || col->type == ASLAN_COL_LINK) {
      char **strings = (char **)col->data;
      if (strings) {
        for (size_t r = 0; r < col->count; r++) {
          free(strings[r]);
        }
      }
    }

    // Free formula expression
    free(col->formula);

    aligned_free_safe(col->data);
  }

  free(table->row_ids);
  free(table->active_mask);
  free(table->view_index);
  free(table);
}

void aslan_table_clear(AslanTable *table) {
  if (!table)
    return;

  for (size_t c = 0; c < table->column_count; c++) {
    AslanColumn *col = &table->columns[c];
    if (col->type == ASLAN_COL_TEXT || col->type == ASLAN_COL_FILE ||
        col->type == ASLAN_COL_IMAGE || col->type == ASLAN_COL_LINK) {
      char **strings = (char **)col->data;
      if (strings) {
        for (size_t r = 0; r < col->count; r++) {
          free(strings[r]);
          strings[r] = NULL;
        }
      }
    }
    col->count = 0;
  }

  table->row_count = 0;
  table->active_count = 0;
  table->next_row_id = 1;
  table->view_dirty = true;

  if (table->active_mask && table->row_capacity > 0) {
    memset(table->active_mask, 0, (table->row_capacity + 7) / 8);
  }
}

// -----------------------------------------------------------------------------
// Column Management
// -----------------------------------------------------------------------------

int aslan_table_add_column(AslanTable *table, const char *name,
                           AslanColumnType type) {
  if (!table || !name)
    return -1;
  if (table->column_count >= ASLAN_TABLE_MAX_COLUMNS)
    return -1;
  if (aslan_table_find_column(table, name) >= 0)
    return -1;

  int idx = (int)table->column_count;
  AslanColumn *col = &table->columns[idx];

  memset(col, 0, sizeof(AslanColumn));
  strncpy(col->name, name, sizeof(col->name) - 1);
  col->type = type;
  col->count = 0;
  col->capacity = 0;
  col->data = NULL;
  col->width_px = 100.0;
  col->is_primary_key = false;
  col->is_nullable = true;
  col->footer_agg = ASLAN_AGG_NONE;
  col->option_count = 0;
  col->formula = NULL;

  // If table already has rows, allocate space
  if (table->row_count > 0) {
    size_t elem_size = column_element_size(type);
    size_t capacity = table->row_capacity;

    col->data = aligned_alloc_safe(ASLAN_TABLE_ALIGNMENT, capacity * elem_size);
    if (!col->data)
      return -1;

    memset(col->data, 0, capacity * elem_size);

    // Initialize SELECT columns to -1 (no selection)
    if (type == ASLAN_COL_SELECT) {
      int32_t *sel = (int32_t *)col->data;
      for (size_t i = 0; i < capacity; i++) {
        sel[i] = -1;
      }
    }

    col->count = table->row_count;
    col->capacity = capacity;
  }

  table->column_count++;
  return idx;
}

bool aslan_table_remove_column(AslanTable *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;

  AslanColumn *col = &table->columns[col_idx];

  if (col->type == ASLAN_COL_TEXT || col->type == ASLAN_COL_FILE ||
      col->type == ASLAN_COL_IMAGE || col->type == ASLAN_COL_LINK) {
    char **strings = (char **)col->data;
    if (strings) {
      for (size_t r = 0; r < col->count; r++) {
        free(strings[r]);
      }
    }
  }
  free(col->formula);
  aligned_free_safe(col->data);

  for (int i = col_idx; i < (int)table->column_count - 1; i++) {
    table->columns[i] = table->columns[i + 1];
  }
  table->column_count--;

  if (table->sort_column == col_idx) {
    table->sort_column = -1;
    table->view_dirty = true;
  } else if (table->sort_column > col_idx) {
    table->sort_column--;
  }

  return true;
}

bool aslan_table_swap_columns(AslanTable *table, int col_a, int col_b) {
  if (!table || col_a < 0 || col_b < 0 || col_a >= (int)table->column_count ||
      col_b >= (int)table->column_count || col_a == col_b)
    return false;

  AslanColumn tmp = table->columns[col_a];
  table->columns[col_a] = table->columns[col_b];
  table->columns[col_b] = tmp;

  if (table->sort_column == col_a) {
    table->sort_column = col_b;
  } else if (table->sort_column == col_b) {
    table->sort_column = col_a;
  }

  return true;
}

bool aslan_table_move_column(AslanTable *table, int from_idx, int to_idx) {
  if (!table || from_idx < 0 || to_idx < 0 ||
      from_idx >= (int)table->column_count ||
      to_idx >= (int)table->column_count || from_idx == to_idx)
    return false;

  AslanColumn tmp = table->columns[from_idx];
  if (from_idx < to_idx) {
    memmove(&table->columns[from_idx], &table->columns[from_idx + 1],
            (to_idx - from_idx) * sizeof(AslanColumn));
  } else {
    memmove(&table->columns[to_idx + 1], &table->columns[to_idx],
            (from_idx - to_idx) * sizeof(AslanColumn));
  }
  table->columns[to_idx] = tmp;

  if (table->sort_column == from_idx) {
    table->sort_column = to_idx;
  } else {
    if (from_idx < to_idx && table->sort_column > from_idx &&
        table->sort_column <= to_idx) {
      table->sort_column--;
    } else if (from_idx > to_idx && table->sort_column >= to_idx &&
               table->sort_column < from_idx) {
      table->sort_column++;
    }
  }

  table->view_dirty = true;
  return true;
}

int aslan_table_find_column(const AslanTable *table, const char *name) {
  if (!table || !name)
    return -1;
  for (size_t i = 0; i < table->column_count; i++) {
    if (strcasecmp(table->columns[i].name, name) == 0) {
      return (int)i;
    }
  }
  return -1;
}

AslanColumnType aslan_table_column_type(const AslanTable *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count) {
    return ASLAN_COL_TEXT;
  }
  return table->columns[col_idx].type;
}

bool aslan_table_rename_column(AslanTable *table, int col_idx,
                               const char *new_name) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count || !new_name)
    return false;
  strncpy(table->columns[col_idx].name, new_name,
          sizeof(table->columns[col_idx].name) - 1);
  table->columns[col_idx].name[sizeof(table->columns[col_idx].name) - 1] = '\0';
  return true;
}

// -----------------------------------------------------------------------------
// Row Management
// -----------------------------------------------------------------------------

static bool ensure_row_capacity(AslanTable *table, size_t needed) {
  if (needed <= table->row_capacity)
    return true;

  size_t new_capacity =
      ((needed + ASLAN_TABLE_BLOCK_SIZE - 1) / ASLAN_TABLE_BLOCK_SIZE) *
      ASLAN_TABLE_BLOCK_SIZE;

  uint64_t *new_ids =
      (uint64_t *)realloc(table->row_ids, new_capacity * sizeof(uint64_t));
  if (!new_ids)
    return false;
  table->row_ids = new_ids;

  size_t old_mask_bytes = (table->row_capacity + 7) / 8;
  size_t new_mask_bytes = (new_capacity + 7) / 8;
  uint8_t *new_mask = (uint8_t *)realloc(table->active_mask, new_mask_bytes);
  if (!new_mask)
    return false;
  table->active_mask = new_mask;
  if (new_mask_bytes > old_mask_bytes) {
    memset(new_mask + old_mask_bytes, 0, new_mask_bytes - old_mask_bytes);
  }

  size_t *new_view =
      (size_t *)realloc(table->view_index, new_capacity * sizeof(size_t));
  if (!new_view)
    return false;
  table->view_index = new_view;

  for (size_t c = 0; c < table->column_count; c++) {
    AslanColumn *col = &table->columns[c];
    size_t elem_size = column_element_size(col->type);
    size_t old_size = col->capacity * elem_size;
    size_t new_size = new_capacity * elem_size;

    void *new_data = aligned_realloc_safe(col->data, ASLAN_TABLE_ALIGNMENT,
                                          old_size, new_size);
    if (!new_data && new_size > 0)
      return false;

    if (new_data && new_size > old_size) {
      memset((char *)new_data + old_size, 0, new_size - old_size);

      // Initialize SELECT columns to -1 for new rows
      if (col->type == ASLAN_COL_SELECT) {
        int32_t *sel = (int32_t *)new_data;
        for (size_t i = col->capacity; i < new_capacity; i++) {
          sel[i] = -1;
        }
      }
    }

    col->data = new_data;

    col->capacity = new_capacity;
  }

  table->row_capacity = new_capacity;
  return true;
}

uint64_t aslan_table_add_row(AslanTable *table) {
  if (!table)
    return 0;
  if (!ensure_row_capacity(table, table->row_count + 1))
    return 0;

  size_t idx = table->row_count;
  uint64_t row_id = table->next_row_id++;

  table->row_ids[idx] = row_id;
  bitmask_set(table->active_mask, idx);
  table->view_index[idx] = idx;

  for (size_t c = 0; c < table->column_count; c++) {
    table->columns[c].count = table->row_count + 1;
  }

  table->row_count++;
  table->active_count++;
  table->view_dirty = true;

  if (table->log_callback) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TABLE_ADD_ROW %u %lu", table->id,
             (unsigned long)row_id);
    table->log_callback(cmd, table->log_userdata);
  }

  return row_id;
}

int aslan_table_row_index(const AslanTable *table, uint64_t row_id) {
  if (!table || row_id == 0)
    return -1;
  for (size_t i = 0; i < table->row_count; i++) {
    if (table->row_ids[i] == row_id) {
      return (int)i;
    }
  }
  return -1;
}

uint64_t aslan_table_row_id_at(const AslanTable *table, int index) {
  if (!table || index < 0 || index >= (int)table->row_count)
    return 0;
  return table->row_ids[index];
}

bool aslan_table_move_row(AslanTable *table, int from_idx, int to_idx) {
  if (!table || from_idx < 0 || to_idx < 0 ||
      from_idx >= (int)table->row_count || to_idx >= (int)table->row_count ||
      from_idx == to_idx)
    return false;

  uint64_t from_id = table->row_ids[from_idx];
  bool from_active = bitmask_get(table->active_mask, from_idx);

  if (from_idx < to_idx) {
    memmove(&table->row_ids[from_idx], &table->row_ids[from_idx + 1],
            (to_idx - from_idx) * sizeof(uint64_t));
    for (int i = from_idx; i < to_idx; i++) {
      if (bitmask_get(table->active_mask, i + 1))
        bitmask_set(table->active_mask, i);
      else
        bitmask_clear(table->active_mask, i);
    }
  } else {
    memmove(&table->row_ids[to_idx + 1], &table->row_ids[to_idx],
            (from_idx - to_idx) * sizeof(uint64_t));
    for (int i = from_idx; i > to_idx; i--) {
      if (bitmask_get(table->active_mask, i - 1))
        bitmask_set(table->active_mask, i);
      else
        bitmask_clear(table->active_mask, i);
    }
  }

  table->row_ids[to_idx] = from_id;
  if (from_active)
    bitmask_set(table->active_mask, to_idx);
  else
    bitmask_clear(table->active_mask, to_idx);

  for (size_t c = 0; c < table->column_count; c++) {
    AslanColumn *col = &table->columns[c];
    if (col->type == ASLAN_COL_FORMULA)
      continue;
    size_t elem = column_element_size(col->type);
    if (elem > 256)
      continue;

    char temp[256];
    memcpy(temp, (char *)col->data + from_idx * elem, elem);

    if (from_idx < to_idx) {
      memmove((char *)col->data + from_idx * elem,
              (char *)col->data + (from_idx + 1) * elem,
              (to_idx - from_idx) * elem);
    } else {
      memmove((char *)col->data + (to_idx + 1) * elem,
              (char *)col->data + to_idx * elem, (from_idx - to_idx) * elem);
    }
    memcpy((char *)col->data + to_idx * elem, temp, elem);
  }

  table->view_dirty = true;
  return true;
}

bool aslan_table_delete_row(AslanTable *table, uint64_t row_id) {
  if (!table)
    return false;
  int idx = aslan_table_row_index(table, row_id);
  if (idx < 0)
    return false;
  if (!bitmask_get(table->active_mask, idx))
    return false;

  bitmask_clear(table->active_mask, idx);
  table->active_count--;
  table->view_dirty = true;

  if (table->log_callback) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TABLE_DELETE_ROW %u %lu", table->id,
             (unsigned long)row_id);
    table->log_callback(cmd, table->log_userdata);
  }
  return true;
}

bool aslan_table_restore_row(AslanTable *table, uint64_t row_id) {
  if (!table)
    return false;
  int idx = aslan_table_row_index(table, row_id);
  if (idx < 0)
    return false;
  if (bitmask_get(table->active_mask, idx))
    return false;

  bitmask_set(table->active_mask, idx);
  table->active_count++;
  table->view_dirty = true;
  return true;
}

uint64_t aslan_table_duplicate_row(AslanTable *table, uint64_t source_row_id) {
  if (!table)
    return 0;
  int src_idx = aslan_table_row_index(table, source_row_id);
  if (src_idx < 0 || !bitmask_get(table->active_mask, src_idx))
    return 0;

  uint64_t new_id = aslan_table_add_row(table);
  if (new_id == 0)
    return 0;
  int dst_idx = aslan_table_row_index(table, new_id);
  if (dst_idx < 0)
    return 0;

  for (size_t c = 0; c < table->column_count; c++) {
    AslanColumn *col = &table->columns[c];
    if (col->type == ASLAN_COL_FORMULA)
      continue; // Computed, skip
    if (col->type == ASLAN_COL_TEXT || col->type == ASLAN_COL_FILE ||
        col->type == ASLAN_COL_IMAGE || col->type == ASLAN_COL_LINK) {
      char **strings = (char **)col->data;
      if (strings[src_idx] && strings[src_idx][0]) {
        free(strings[dst_idx]);
        strings[dst_idx] = strdup(strings[src_idx]);
      }
    } else {
      size_t elem = column_element_size(col->type);
      memcpy((char *)col->data + (size_t)dst_idx * elem,
             (char *)col->data + (size_t)src_idx * elem, elem);
    }
  }

  table->view_dirty = true;
  return new_id;
}

bool aslan_table_row_active(const AslanTable *table, uint64_t row_id) {
  if (!table)
    return false;
  int idx = aslan_table_row_index(table, row_id);
  if (idx < 0)
    return false;
  return bitmask_get(table->active_mask, idx);
}

// -----------------------------------------------------------------------------
// Cell Setters
// -----------------------------------------------------------------------------

bool aslan_table_set_text(AslanTable *table, int col_idx, uint64_t row_id,
                          const char *value) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_TEXT && col->type != ASLAN_COL_FILE &&
      col->type != ASLAN_COL_IMAGE && col->type != ASLAN_COL_LINK)
    return false;

  char **strings = (char **)col->data;
  free(strings[row_idx]);

  if (value) {
    size_t len = strlen(value);
    if (len > ASLAN_TABLE_MAX_STRING_LEN)
      len = ASLAN_TABLE_MAX_STRING_LEN;
    strings[row_idx] = (char *)malloc(len + 1);
    if (strings[row_idx]) {
      strncpy(strings[row_idx], value, len);
      strings[row_idx][len] = '\0';
    }
  } else {
    strings[row_idx] = NULL;
  }

  if (table->log_callback) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "TABLE_SET_CELL %u %lu %d \"%s\"", table->id,
             (unsigned long)row_id, col_idx, value ? value : "");
    table->log_callback(cmd, table->log_userdata);
  }
  return true;
}

bool aslan_table_set_number(AslanTable *table, int col_idx, uint64_t row_id,
                            double value) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_NUMBER)
    return false;

  double *data = (double *)col->data;
  data[row_idx] = value;

  if (table->log_callback) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TABLE_SET_CELL %u %lu %d %g", table->id,
             (unsigned long)row_id, col_idx, value);
    table->log_callback(cmd, table->log_userdata);
  }
  return true;
}

bool aslan_table_set_checkbox(AslanTable *table, int col_idx, uint64_t row_id,
                              bool checked) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_CHECKBOX)
    return false;

  uint8_t *data = (uint8_t *)col->data;
  data[row_idx] = checked ? 1 : 0;

  if (table->log_callback) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TABLE_SET_CELL %u %lu %d %d", table->id,
             (unsigned long)row_id, col_idx, checked ? 1 : 0);
    table->log_callback(cmd, table->log_userdata);
  }
  return true;
}

bool aslan_table_set_select(AslanTable *table, int col_idx, uint64_t row_id,
                            int option_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_SELECT)
    return false;
  if (option_idx >= col->option_count)
    option_idx = -1;

  int32_t *data = (int32_t *)col->data;
  data[row_idx] = option_idx;

  if (table->log_callback) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TABLE_SET_CELL %u %lu %d %d", table->id,
             (unsigned long)row_id, col_idx, option_idx);
    table->log_callback(cmd, table->log_userdata);
  }
  return true;
}

bool aslan_table_set_date(AslanTable *table, int col_idx, uint64_t row_id,
                          int64_t timestamp_ms) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_DATE)
    return false;

  int64_t *data = (int64_t *)col->data;
  data[row_idx] = timestamp_ms;

  if (table->log_callback) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TABLE_SET_CELL %u %lu %d %lld", table->id,
             (unsigned long)row_id, col_idx, (long long)timestamp_ms);
    table->log_callback(cmd, table->log_userdata);
  }
  return true;
}


bool aslan_table_set_cell(AslanTable *table, int col_idx, uint64_t row_id,
                          const char *value_str) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;

  AslanColumn *col = &table->columns[col_idx];

  switch (col->type) {
  case ASLAN_COL_TEXT:
  case ASLAN_COL_FILE:
  case ASLAN_COL_IMAGE:
  case ASLAN_COL_LINK:
    return aslan_table_set_text(table, col_idx, row_id, value_str);
  case ASLAN_COL_NUMBER: {
    double val = value_str ? strtod(value_str, NULL) : 0.0;
    return aslan_table_set_number(table, col_idx, row_id, val);
  }
  case ASLAN_COL_CHECKBOX: {
    bool val = value_str &&
               (strcmp(value_str, "true") == 0 || strcmp(value_str, "1") == 0 ||
                strcmp(value_str, "yes") == 0);
    return aslan_table_set_checkbox(table, col_idx, row_id, val);
  }
  case ASLAN_COL_SELECT: {
    int idx = value_str ? atoi(value_str) : -1;
    return aslan_table_set_select(table, col_idx, row_id, idx);
  }
  case ASLAN_COL_DATE: {
    int64_t ts = value_str ? strtoll(value_str, NULL, 10) : 0;
    return aslan_table_set_date(table, col_idx, row_id, ts);
  }
  case ASLAN_COL_FORMULA:
    return false; // Formulas are read-only
  default:
    return false;
  }
}

// -----------------------------------------------------------------------------
// Cell Getters
// -----------------------------------------------------------------------------

const char *aslan_table_get_text(const AslanTable *table, int col_idx,
                                 uint64_t row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return NULL;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return NULL;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_TEXT && col->type != ASLAN_COL_FILE &&
      col->type != ASLAN_COL_IMAGE && col->type != ASLAN_COL_LINK)
    return NULL;

  const char **strings = (const char **)col->data;
  return strings[row_idx];
}

double aslan_table_get_number(const AslanTable *table, int col_idx,
                              uint64_t row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0.0;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return 0.0;

  const AslanColumn *col = &table->columns[col_idx];
  switch (col->type) {
  case ASLAN_COL_NUMBER:
  case ASLAN_COL_FORMULA: {
    const double *data = (const double *)col->data;
    return data[row_idx];
  }
  case ASLAN_COL_CHECKBOX: {
    const uint8_t *data = (const uint8_t *)col->data;
    return (double)data[row_idx];
  }
  case ASLAN_COL_SELECT: {
    const int32_t *data = (const int32_t *)col->data;
    return (double)data[row_idx];
  }
  case ASLAN_COL_DATE: {
    const int64_t *data = (const int64_t *)col->data;
    return (double)data[row_idx];
  }
  default:
    return 0.0;
  }
}

bool aslan_table_get_checkbox(const AslanTable *table, int col_idx,
                              uint64_t row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return false;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_CHECKBOX)
    return false;

  const uint8_t *data = (const uint8_t *)col->data;
  return data[row_idx] != 0;
}

int aslan_table_get_select(const AslanTable *table, int col_idx,
                           uint64_t row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return -1;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return -1;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_SELECT)
    return -1;

  const int32_t *data = (const int32_t *)col->data;
  return data[row_idx];
}

int64_t aslan_table_get_date(const AslanTable *table, int col_idx,
                             uint64_t row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return 0;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_DATE)
    return 0;

  const int64_t *data = (const int64_t *)col->data;
  return data[row_idx];
}


static __thread char display_buffer[256];

const char *aslan_table_get_cell_display(const AslanTable *table, int col_idx,
                                         uint64_t row_id, char *buf,
                                         size_t buf_size) {
  if (!buf || buf_size == 0) {
    buf = display_buffer;
    buf_size = sizeof(display_buffer);
  }

  if (!table || col_idx < 0 || col_idx >= (int)table->column_count) {
    buf[0] = '\0';
    return buf;
  }

  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0) {
    buf[0] = '\0';
    return buf;
  }

  const AslanColumn *col = &table->columns[col_idx];

  switch (col->type) {
  case ASLAN_COL_TEXT:
  case ASLAN_COL_FILE:
  case ASLAN_COL_IMAGE:
  case ASLAN_COL_LINK: {
    const char *str = ((char **)col->data)[row_idx];
    snprintf(buf, buf_size, "%s", str ? str : "");
    break;
  }
  case ASLAN_COL_NUMBER: {
    double val = ((double *)col->data)[row_idx];
    if (val == (int64_t)val) {
      snprintf(buf, buf_size, "%lld", (long long)val);
    } else {
      snprintf(buf, buf_size, "%.2f", val);
    }
    break;
  }
  case ASLAN_COL_CHECKBOX: {
    uint8_t val = ((uint8_t *)col->data)[row_idx];
    snprintf(buf, buf_size, "%s", val ? "\xE2\x9C\x93" : ""); // ✓ or empty
    break;
  }
  case ASLAN_COL_SELECT: {
    int32_t idx = ((int32_t *)col->data)[row_idx];
    if (idx >= 0 && idx < col->option_count) {
      snprintf(buf, buf_size, "%s", col->options[idx].label);
    } else {
      buf[0] = '\0';
    }
    break;
  }
  case ASLAN_COL_DATE: {
    int64_t ts = ((int64_t *)col->data)[row_idx];
    if (ts > 0) {
      time_t t = (time_t)(ts / 1000);
      struct tm *tm = localtime(&t);
      strftime(buf, buf_size, "%Y-%m-%d", tm);
    } else {
      buf[0] = '\0';
    }
    break;
  }
  case ASLAN_COL_FORMULA: {
    double val = ((double *)col->data)[row_idx];
    snprintf(buf, buf_size, "%.2f", val);
    break;
  }
  default:
    buf[0] = '\0';
  }

  return buf;
}

// -----------------------------------------------------------------------------
// Select Options Management
// -----------------------------------------------------------------------------

int aslan_table_add_option(AslanTable *table, int col_idx, const char *label,
                           uint32_t color) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return -1;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_SELECT)
    return -1;
  if (col->option_count >= ASLAN_MAX_SELECT_OPTIONS)
    return -1;

  int idx = col->option_count;
  strncpy(col->options[idx].label, label ? label : "",
          ASLAN_SELECT_OPTION_LEN - 1);
  col->options[idx].label[ASLAN_SELECT_OPTION_LEN - 1] = '\0';
  col->options[idx].color = color;
  col->option_count++;

  return idx;
}

bool aslan_table_remove_option(AslanTable *table, int col_idx, int option_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_SELECT)
    return false;
  if (option_idx < 0 || option_idx >= col->option_count)
    return false;

  // Shift options down
  for (int i = option_idx; i < col->option_count - 1; i++) {
    col->options[i] = col->options[i + 1];
  }
  col->option_count--;

  // Update cells that referenced this or higher indices
  int32_t *data = (int32_t *)col->data;
  for (size_t r = 0; r < col->count; r++) {
    if (data[r] == option_idx) {
      data[r] = -1; // Reset to none
    } else if (data[r] > option_idx) {
      data[r]--; // Shift down
    }
  }

  return true;
}

const char *aslan_table_get_option_label(const AslanTable *table, int col_idx,
                                         int option_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return NULL;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_SELECT)
    return NULL;
  if (option_idx < 0 || option_idx >= col->option_count)
    return NULL;

  return col->options[option_idx].label;
}

bool aslan_table_set_option_color(AslanTable *table, int col_idx,
                                  int option_idx, uint32_t color) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;
  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_SELECT || option_idx < 0 ||
      option_idx >= col->option_count)
    return false;
  col->options[option_idx].color = color;
  return true;
}

bool aslan_table_update_option_color(AslanTable *table, int col_idx,
                                     int option_idx, uint32_t color) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_SELECT)
    return false;

  if (option_idx < 0 || option_idx >= col->option_count)
    return false;

  col->options[option_idx].color = color;
  col->options[option_idx].color = color;
  return true;
}

uint32_t aslan_table_get_option_color(const AslanTable *table, int col_idx,
                                      int option_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_SELECT)
    return 0;
  if (option_idx < 0 || option_idx >= col->option_count)
    return 0;

  return col->options[option_idx].color;
}


// -----------------------------------------------------------------------------
// Formula Column
// -----------------------------------------------------------------------------

bool aslan_table_set_formula(AslanTable *table, int col_idx, const char *expr) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count) {
    return false;
  }

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_FORMULA) {
    return false;
  }

  free(col->formula);
  col->formula = expr ? strdup(expr) : NULL;

  return true;
}

const char *aslan_table_get_formula(const AslanTable *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return NULL;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_FORMULA)
    return NULL;

  return col->formula;
}
// -----------------------------------------------------------------------------
// Formula Evaluation via TinyExpr (Excel-like syntax)
// -----------------------------------------------------------------------------
// Replaces the old Shunting-yard parser with a robust 3rd party evaluator.
// Supports: +, -, *, /, %, ^, sqrt, sin, cos, tan, etc.
// Syntax: =[Price] * [Quantity] * 1.2
// Variables are automatically bound to column values for the current row.

double aslan_table_eval_formula(const AslanTable *table, int col_idx,
                                uint64_t row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0.0;
  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_FORMULA || !col->formula) {
    return 0.0;
  }

  const char *expr = col->formula;
  // Skip leading '=' if present (Excel style, optional)
  while (*expr == ' ' || *expr == '\t')
    expr++;
  if (*expr == '=')
    expr++;

  if (*expr == '\0')
    return 0.0;

  // 1. Preprocess formula (Replace [ColName] with colINDEX variables)
  // tinyexpr needs valid C identifiers (no spaces/brackets).
  // We map [Name] -> colINDEX (e.g., col1, col2) in the string.
  char parse_buf[4096];
  size_t pb_idx = 0;

  te_variable vars[ASLAN_TABLE_MAX_COLUMNS];
  double var_values[ASLAN_TABLE_MAX_COLUMNS];
  char var_names[ASLAN_TABLE_MAX_COLUMNS][16];
  bool col_used[ASLAN_TABLE_MAX_COLUMNS];
  memset(col_used, 0, sizeof(col_used));
  int var_count = 0;

  const char *p = expr;
  while (*p && pb_idx < sizeof(parse_buf) - 1) {
    if (*p == '[') {
      // Start of column name reference
      p++;
      const char *start = p;
      while (*p && *p != ']')
        p++;
      size_t name_len = p - start;

      bool found = false;
      if (name_len > 0 && name_len < 64) {
        char name_buf[64];
        memcpy(name_buf, start, name_len);
        name_buf[name_len] = '\0';

        // Trim leading/trailing spaces
        char *trimmed = name_buf;
        while (*trimmed == ' ')
          trimmed++;
        char *end = trimmed + strlen(trimmed) - 1;
        while (end >= trimmed && *end == ' ') {
          *end = '\0';
          end--;
        }

        // Find column by name (case-insensitive)
        int ref_idx = aslan_table_find_column(table, trimmed);
        if (ref_idx >= 0) {
          // Found: replace with unique variable name col%d
          char vname[16];
          snprintf(vname, sizeof(vname), "col%d", ref_idx);
          size_t vn_len = strlen(vname);

          if (pb_idx + vn_len < sizeof(parse_buf)) {
            memcpy(parse_buf + pb_idx, vname, vn_len);
            pb_idx += vn_len;
          }

          // Mark column as used so we can bind it later
          col_used[ref_idx] = true;
          found = true;
        }
      }

      if (!found) {
        // Unknown column or empty brackets; treat as 0 to avoid syntax error
        if (pb_idx < sizeof(parse_buf) - 1)
          parse_buf[pb_idx++] = '0';
      }

      if (*p == ']')
        p++; // consume closing bracket
    } else {
      // Copy other characters as-is
      parse_buf[pb_idx++] = *p++;
    }
  }
  parse_buf[pb_idx] = '\0';

  // 2. Build tinyexpr variable list
  // Only bind columns that were actually referenced in the formula
  for (int c = 0; c < (int)table->column_count; c++) {
    if (col_used[c]) {
      // Generate the same variable name: col%d
      snprintf(var_names[var_count], 16, "col%d", c);

      // Fetch value for the current row (handles recursion via recursive eval
      // calls if needed) Note: This recursively calls eval_formula if the
      // referenced column is a formula
      var_values[var_count] = aslan_table_get_number(table, c, row_id);

      vars[var_count].name = var_names[var_count];
      vars[var_count].address = &var_values[var_count];
      vars[var_count].type = TE_VARIABLE;
      vars[var_count].context = NULL;

      var_count++;
    }
  }

  // 3. Compile and Evaluate
  // We compile and eval in one pass per row for simplicity.
  // Performance optimization (caching compiled expression) can be added later
  // if needed.
  int err = 0;
  te_expr *n = te_compile(parse_buf, vars, var_count, &err);
  double result = 0.0;

  if (n) {
    result = te_eval(n);
    te_free(n);
  } else {
    result = 0.0;
  }

  // 4. Update Cached Result
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx >= 0) {
    ((double *)col->data)[row_idx] = result;
  }

  return result;
}
void aslan_table_eval_formulas(AslanTable *table) {
  if (!table)
    return;
  for (size_t c = 0; c < table->column_count; c++) {
    if (table->columns[c].type == ASLAN_COL_FORMULA) {
      for (size_t r = 0; r < table->row_count; r++) {
        if (bitmask_get(table->active_mask, r)) {
          uint64_t row_id = table->row_ids[r];
          aslan_table_eval_formula(table, (int)c, row_id);
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Sorting
// -----------------------------------------------------------------------------

typedef struct {
  const AslanTable *table;
  int col_idx;
  bool ascending;
} SortContext;

static int compare_number(const void *a, const void *b, void *ctx_ptr) {
  const SortContext *ctx = (const SortContext *)ctx_ptr;
  size_t idx_a = *(const size_t *)a;
  size_t idx_b = *(const size_t *)b;

  const double *data = (const double *)ctx->table->columns[ctx->col_idx].data;
  double val_a = data[idx_a];
  double val_b = data[idx_b];

  int cmp = (val_a > val_b) - (val_a < val_b);
  return ctx->ascending ? cmp : -cmp;
}

static int compare_text(const void *a, const void *b, void *ctx_ptr) {
  const SortContext *ctx = (const SortContext *)ctx_ptr;
  size_t idx_a = *(const size_t *)a;
  size_t idx_b = *(const size_t *)b;

  const char **strings = (const char **)ctx->table->columns[ctx->col_idx].data;
  const char *str_a = strings[idx_a] ? strings[idx_a] : "";
  const char *str_b = strings[idx_b] ? strings[idx_b] : "";

  int cmp = strcmp(str_a, str_b);
  return ctx->ascending ? cmp : -cmp;
}

static int compare_date(const void *a, const void *b, void *ctx_ptr) {
  const SortContext *ctx = (const SortContext *)ctx_ptr;
  size_t idx_a = *(const size_t *)a;
  size_t idx_b = *(const size_t *)b;

  const int64_t *data = (const int64_t *)ctx->table->columns[ctx->col_idx].data;
  int64_t val_a = data[idx_a];
  int64_t val_b = data[idx_b];

  int cmp = (val_a > val_b) - (val_a < val_b);
  return ctx->ascending ? cmp : -cmp;
}

static int compare_checkbox(const void *a, const void *b, void *ctx_ptr) {
  const SortContext *ctx = (const SortContext *)ctx_ptr;
  size_t idx_a = *(const size_t *)a;
  size_t idx_b = *(const size_t *)b;

  const uint8_t *data = (const uint8_t *)ctx->table->columns[ctx->col_idx].data;
  int cmp = (int)data[idx_a] - (int)data[idx_b];
  return ctx->ascending ? cmp : -cmp;
}

static int compare_select(const void *a, const void *b, void *ctx_ptr) {
  const SortContext *ctx = (const SortContext *)ctx_ptr;
  size_t idx_a = *(const size_t *)a;
  size_t idx_b = *(const size_t *)b;

  const AslanColumn *col = &ctx->table->columns[ctx->col_idx];
  const int32_t *data = (const int32_t *)col->data;
  int32_t opt_a = data[idx_a];
  int32_t opt_b = data[idx_b];

  // Empty (-1) sorts last
  if (opt_a < 0 && opt_b < 0) return 0;
  if (opt_a < 0) return ctx->ascending ? 1 : -1;
  if (opt_b < 0) return ctx->ascending ? -1 : 1;

  // Sort by option label alphabetically
  const char *la = (opt_a < col->option_count) ? col->options[opt_a].label : "";
  const char *lb = (opt_b < col->option_count) ? col->options[opt_b].label : "";
  int cmp = strcmp(la, lb);
  return ctx->ascending ? cmp : -cmp;
}

#if defined(__APPLE__)
static int compare_wrapper_number(void *ctx, const void *a, const void *b) {
  return compare_number(a, b, ctx);
}
static int compare_wrapper_text(void *ctx, const void *a, const void *b) {
  return compare_text(a, b, ctx);
}
static int compare_wrapper_date(void *ctx, const void *a, const void *b) {
  return compare_date(a, b, ctx);
}
static int compare_wrapper_checkbox(void *ctx, const void *a, const void *b) {
  return compare_checkbox(a, b, ctx);
}
static int compare_wrapper_select(void *ctx, const void *a, const void *b) {
  return compare_select(a, b, ctx);
}
#endif


void aslan_table_sort(AslanTable *table, int col_idx, bool ascending) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return;
  if (table->row_count == 0)
    return;

  table->sort_column = col_idx;
  table->sort_ascending = ascending;

  size_t view_count = 0;
  for (size_t i = 0; i < table->row_count; i++) {
    if (bitmask_get(table->active_mask, i)) {
      table->view_index[view_count++] = i;
    }
  }

  if (view_count == 0) {
    table->view_dirty = false;
    return;
  }

  SortContext ctx = {table, col_idx, ascending};
  AslanColumnType type = table->columns[col_idx].type;

#if defined(__APPLE__)
  switch (type) {
  case ASLAN_COL_NUMBER:
  case ASLAN_COL_FORMULA:
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_number);
    break;
  case ASLAN_COL_TEXT:
  case ASLAN_COL_FILE:
  case ASLAN_COL_IMAGE:
  case ASLAN_COL_LINK:
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_text);
    break;
  case ASLAN_COL_DATE:
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_date);
    break;
  case ASLAN_COL_CHECKBOX:
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_checkbox);
    break;
  case ASLAN_COL_SELECT:
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_select);
    break;
  default:
    break;
  }
#else
  switch (type) {
  case ASLAN_COL_NUMBER:
  case ASLAN_COL_FORMULA:
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_number,
            &ctx);
    break;
  case ASLAN_COL_TEXT:
  case ASLAN_COL_FILE:
  case ASLAN_COL_IMAGE:
  case ASLAN_COL_LINK:
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_text, &ctx);
    break;
  case ASLAN_COL_DATE:
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_date, &ctx);
    break;
  case ASLAN_COL_CHECKBOX:
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_checkbox,
            &ctx);
    break;
  case ASLAN_COL_SELECT:
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_select, &ctx);
    break;
  default:
    break;
  }
#endif

  table->view_dirty = false;
}

void aslan_table_sort_clear(AslanTable *table) {
  if (!table)
    return;
  table->sort_column = -1;

  size_t view_count = 0;
  for (size_t i = 0; i < table->row_count; i++) {
    if (bitmask_get(table->active_mask, i)) {
      table->view_index[view_count++] = i;
    }
  }
  table->view_dirty = false;
}

uint64_t aslan_table_view_row_id(const AslanTable *table, int view_idx) {
  if (!table || view_idx < 0)
    return 0;

  size_t active_seen = 0;
  for (size_t i = 0; i < table->row_count; i++) {
    size_t real_idx = table->view_index[i];
    if (bitmask_get(table->active_mask, real_idx)) {
      if ((int)active_seen == view_idx) {
        return table->row_ids[real_idx];
      }
      active_seen++;
    }
  }
  return 0;
}

size_t aslan_table_view_count(const AslanTable *table) {
  if (!table)
    return 0;
  if (table->filter_text[0] != '\0')
    return table->filtered_count;
  return table->active_count;
}

// Case-insensitive substring search (portable C99)
static bool ci_contains(const char *haystack, const char *needle) {
  if (!haystack || !needle || !needle[0])
    return true;
  size_t nlen = strlen(needle);
  size_t hlen = strlen(haystack);
  if (nlen > hlen)
    return false;
  for (size_t i = 0; i <= hlen - nlen; i++) {
    bool match = true;
    for (size_t j = 0; j < nlen && match; j++) {
      char a = haystack[i + j];
      char b = needle[j];
      if (a >= 'A' && a <= 'Z')
        a += 32;
      if (b >= 'A' && b <= 'Z')
        b += 32;
      if (a != b)
        match = false;
    }
    if (match)
      return true;
  }
  return false;
}

void aslan_table_filter_rebuild(AslanTable *table) {
  if (!table)
    return;

  bool has_filter = (table->filter_text[0] != '\0');
  size_t view_count = 0;

  for (size_t i = 0; i < table->row_count; i++) {
    if (!bitmask_get(table->active_mask, i))
      continue;

    if (has_filter) {
      uint64_t rid = table->row_ids[i];
      bool match = false;
      char buf[256];
      for (size_t c = 0; c < table->column_count && !match; c++) {
        aslan_table_get_cell_display(table, (int)c, rid, buf, sizeof(buf));
        if (buf[0] && ci_contains(buf, table->filter_text))
          match = true;
      }
      if (!match)
        continue;
    }

    table->view_index[view_count++] = i;
  }

  table->filtered_count = view_count;

  // Re-apply sort if active
  if (table->sort_column >= 0 && view_count > 1) {
    table->view_dirty = true;
    aslan_table_sort(table, table->sort_column, table->sort_ascending);
  }
}

// -----------------------------------------------------------------------------
// Aggregation Functions
// -----------------------------------------------------------------------------

double aslan_table_col_sum(const AslanTable *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0.0;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_NUMBER && col->type != ASLAN_COL_FORMULA)
    return 0.0;

  double sum = 0.0;
  const double *data = (const double *)col->data;

  for (size_t i = 0; i < table->row_count; i++) {
    if (bitmask_get(table->active_mask, i)) {
      sum += data[i];
    }
  }
  return sum;
}

double aslan_table_col_avg(const AslanTable *table, int col_idx) {
  size_t count = aslan_table_col_count(table, col_idx);
  if (count == 0)
    return 0.0;
  return aslan_table_col_sum(table, col_idx) / (double)count;
}

double aslan_table_col_min(const AslanTable *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0.0;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_NUMBER && col->type != ASLAN_COL_FORMULA)
    return 0.0;

  double min_val = INFINITY;
  bool found = false;
  const double *data = (const double *)col->data;

  for (size_t i = 0; i < table->row_count; i++) {
    if (bitmask_get(table->active_mask, i)) {
      if (!found || data[i] < min_val) {
        min_val = data[i];
        found = true;
      }
    }
  }
  return found ? min_val : 0.0;
}

double aslan_table_col_max(const AslanTable *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0.0;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_NUMBER && col->type != ASLAN_COL_FORMULA)
    return 0.0;

  double max_val = -INFINITY;
  bool found = false;
  const double *data = (const double *)col->data;

  for (size_t i = 0; i < table->row_count; i++) {
    if (bitmask_get(table->active_mask, i)) {
      if (!found || data[i] > max_val) {
        max_val = data[i];
        found = true;
      }
    }
  }
  return found ? max_val : 0.0;
}

size_t aslan_table_col_count(const AslanTable *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0;

  const AslanColumn *col = &table->columns[col_idx];
  size_t count = 0;

  for (size_t i = 0; i < table->row_count; i++) {
    if (!bitmask_get(table->active_mask, i))
      continue;

    if (col->type == ASLAN_COL_TEXT) {
      if (((char **)col->data)[i] != NULL)
        count++;
    } else {
      count++;
    }
  }
  return count;
}

double aslan_table_col_percent(const AslanTable *table, int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0.0;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_CHECKBOX)
    return 0.0;

  size_t total = 0;
  size_t checked = 0;
  const uint8_t *data = (const uint8_t *)col->data;

  for (size_t i = 0; i < table->row_count; i++) {
    if (bitmask_get(table->active_mask, i)) {
      total++;
      if (data[i])
        checked++;
    }
  }

  return total > 0 ? (double)checked / (double)total * 100.0 : 0.0;
}

// -----------------------------------------------------------------------------
// Footer Aggregation
// -----------------------------------------------------------------------------

void aslan_table_set_footer_agg(AslanTable *table, int col_idx,
                                AslanAggregationType agg) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return;
  table->columns[col_idx].footer_agg = agg;
}

AslanAggregationType aslan_table_get_footer_agg(const AslanTable *table,
                                                int col_idx) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return ASLAN_AGG_NONE;
  return table->columns[col_idx].footer_agg;
}

const char *aslan_table_get_footer_display(const AslanTable *table, int col_idx,
                                           char *buf, size_t buf_size) {
  if (!buf || buf_size == 0) {
    buf = display_buffer;
    buf_size = sizeof(display_buffer);
  }
  buf[0] = '\0';

  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return buf;

  const AslanColumn *col = &table->columns[col_idx];

  switch (col->footer_agg) {
  case ASLAN_AGG_NONE:
    break;
  case ASLAN_AGG_SUM:
    if (col->type == ASLAN_COL_NUMBER || col->type == ASLAN_COL_FORMULA) {
      double sum = aslan_table_col_sum(table, col_idx);
      snprintf(buf, buf_size, "Sum %.2f", sum);
    }
    break;
  case ASLAN_AGG_AVG:
    if (col->type == ASLAN_COL_NUMBER || col->type == ASLAN_COL_FORMULA) {
      double avg = aslan_table_col_avg(table, col_idx);
      snprintf(buf, buf_size, "Avg %.2f", avg);
    }
    break;
  case ASLAN_AGG_COUNT: {
    size_t count = aslan_table_col_count(table, col_idx);
    snprintf(buf, buf_size, "Count %zu", count);
    break;
  }
  case ASLAN_AGG_PERCENT:
    if (col->type == ASLAN_COL_CHECKBOX) {
      double pct = aslan_table_col_percent(table, col_idx);
      snprintf(buf, buf_size, "%.0f%%", pct);
    }
    break;
  case ASLAN_AGG_MIN:
    if (col->type == ASLAN_COL_NUMBER || col->type == ASLAN_COL_FORMULA) {
      double min = aslan_table_col_min(table, col_idx);
      snprintf(buf, buf_size, "Min %.2f", min);
    }
    break;
  case ASLAN_AGG_MAX:
    if (col->type == ASLAN_COL_NUMBER || col->type == ASLAN_COL_FORMULA) {
      double max = aslan_table_col_max(table, col_idx);
      snprintf(buf, buf_size, "Max %.2f", max);
    }
    break;
  default:
    break;
  }

  return buf;
}

// -----------------------------------------------------------------------------
// AOF Logging
// -----------------------------------------------------------------------------

void aslan_table_set_logger(AslanTable *table,
                            void (*callback)(const char *cmd, void *userdata),
                            void *userdata) {
  if (!table)
    return;
  table->log_callback = callback;
  table->log_userdata = userdata;
}

char *aslan_table_aof_set_cell(uint32_t table_id, uint64_t row_id, int col_idx,
                               const char *value_str) {
  char *cmd = (char *)malloc(512);
  if (!cmd)
    return NULL;

  snprintf(cmd, 512, "TABLE_SET_CELL %u %lu %d \"%s\"", table_id,
           (unsigned long)row_id, col_idx, value_str ? value_str : "");
  return cmd;
}

// -----------------------------------------------------------------------------
// Debug Output
// -----------------------------------------------------------------------------

static const char *column_type_name(AslanColumnType type) {
  switch (type) {
  case ASLAN_COL_TEXT:
    return "TEXT";
  case ASLAN_COL_NUMBER:
    return "NUM";
  case ASLAN_COL_CHECKBOX:
    return "CHK";
  case ASLAN_COL_SELECT:
    return "SEL";
  case ASLAN_COL_DATE:
    return "DATE";
  case ASLAN_COL_FORMULA:
    return "CALC";
  default:
    return "?";
  }
}

void aslan_table_print_debug(const AslanTable *table) {
  if (!table) {
    ASLAN_DEBUG("AslanTable: NULL");
    return;
  }

  printf("\n+------------------------------------------------------------+\n");
  printf("| TABLE: %-50s |\n", table->name);
  printf("+------------------------------------------------------------+\n");
  printf("| ID: %-4u  Columns: %-3zu  Rows: %-5zu (Active: %-5zu)       |\n",
         table->id, table->column_count, table->row_count, table->active_count);
  printf("+------------------------------------------------------------+\n");

  // Print column headers with types
  printf("| %-6s |", "ID");
  for (size_t c = 0; c < table->column_count && c < 5; c++) {
    printf(" %-10.10s |", table->columns[c].name);
  }
  printf("\n| %-6s |", "");
  for (size_t c = 0; c < table->column_count && c < 5; c++) {
    printf(" %-10s |", column_type_name(table->columns[c].type));
  }
  printf("\n+--------+");
  for (size_t c = 0; c < table->column_count && c < 5; c++) {
    printf("------------+");
  }
  printf("\n");

  // Print rows
  int printed = 0;
  for (size_t i = 0; i < table->row_count && printed < 10; i++) {
    if (!bitmask_get(table->active_mask, i))
      continue;

    uint64_t row_id = table->row_ids[i];
    printf("| %-6lu |", (unsigned long)row_id);

    for (size_t c = 0; c < table->column_count && c < 5; c++) {
      char buf[32];
      aslan_table_get_cell_display(table, (int)c, row_id, buf, sizeof(buf));
      printf(" %-10.10s |", buf);
    }
    printf("\n");
    printed++;
  }

  // Print footer
  printf("+--------+");
  for (size_t c = 0; c < table->column_count && c < 5; c++) {
    printf("------------+");
  }
  printf("\n| FOOTER |");
  for (size_t c = 0; c < table->column_count && c < 5; c++) {
    char buf[32];
    aslan_table_get_footer_display(table, (int)c, buf, sizeof(buf));
    printf(" %-10.10s |", buf);
  }
  printf(
      "\n+------------------------------------------------------------+\n\n");
}

// -----------------------------------------------------------------------------
// Memory Statistics
// -----------------------------------------------------------------------------

void aslan_table_mem_stats(const AslanTable *table, AslanTableMemStats *stats) {
  if (!stats)
    return;
  memset(stats, 0, sizeof(AslanTableMemStats));
  if (!table)
    return;

  stats->overhead_bytes = sizeof(AslanTable);

  stats->index_bytes += table->row_capacity * sizeof(uint64_t);
  stats->index_bytes += (table->row_capacity + 7) / 8;
  stats->index_bytes += table->row_capacity * sizeof(size_t);

  for (size_t c = 0; c < table->column_count; c++) {
    const AslanColumn *col = &table->columns[c];
    size_t elem_size = column_element_size(col->type);
    stats->data_bytes += col->capacity * elem_size;

    if (col->type == ASLAN_COL_TEXT) {
      char **strings = (char **)col->data;
      if (strings) {
        for (size_t r = 0; r < col->count; r++) {
          if (strings[r]) {
            stats->data_bytes += strlen(strings[r]) + 1;
          }
        }
      }
    }

    if (col->formula) {
      stats->data_bytes += strlen(col->formula) + 1;
    }
  }

  stats->total_bytes =
      stats->data_bytes + stats->index_bytes + stats->overhead_bytes;
}

// -----------------------------------------------------------------------------
// CSV Export
// -----------------------------------------------------------------------------

// Write a CSV field, quoting if it contains comma, quote, or newline.
static void csv_write_field(FILE *fp, const char *val) {
  if (!val || val[0] == '\0')
    return;
  int need_quote = 0;
  for (const char *p = val; *p; p++) {
    if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
      need_quote = 1;
      break;
    }
  }
  if (need_quote) {
    fputc('"', fp);
    for (const char *p = val; *p; p++) {
      if (*p == '"')
        fputc('"', fp);
      fputc(*p, fp);
    }
    fputc('"', fp);
  } else {
    fputs(val, fp);
  }
}

bool aslan_table_export_csv(const AslanTable *table, const char *filepath) {
  if (!table || !filepath)
    return false;
  FILE *fp = fopen(filepath, "w");
  if (!fp)
    return false;

  // Header row
  for (size_t c = 0; c < table->column_count; c++) {
    if (c > 0)
      fputc(',', fp);
    csv_write_field(fp, table->columns[c].name);
  }
  fputc('\n', fp);

  // Data rows
  size_t row_count = aslan_table_view_count(table);
  for (size_t r = 0; r < row_count; r++) {
    uint64_t row_id = aslan_table_view_row_id(table, r);
    for (size_t c = 0; c < table->column_count; c++) {
      if (c > 0)
        fputc(',', fp);
      char buf[512];
      const char *val =
          aslan_table_get_cell_display(table, (int)c, row_id, buf, sizeof(buf));
      csv_write_field(fp, val);
    }
    fputc('\n', fp);
  }

  fclose(fp);
  return true;
}

// -----------------------------------------------------------------------------
// CSV Import
// -----------------------------------------------------------------------------

// Parse a single CSV line into fields. Returns field count.
// Handles RFC 4180 quoting. Fields are written into `fields[]` pointing
// into the mutable `line` buffer (destructive parse).
static int csv_parse_line(char *line, char *fields[], int max_fields) {
  int count = 0;
  char *p = line;
  while (*p && count < max_fields) {
    if (*p == '"') {
      // Quoted field
      p++;
      fields[count] = p;
      char *dst = p;
      while (*p) {
        if (*p == '"') {
          if (*(p + 1) == '"') {
            *dst++ = '"';
            p += 2;
          } else {
            p++; // closing quote
            break;
          }
        } else {
          *dst++ = *p++;
        }
      }
      *dst = '\0';
      if (*p == ',')
        p++;
    } else {
      // Unquoted field
      fields[count] = p;
      while (*p && *p != ',' && *p != '\n' && *p != '\r')
        p++;
      if (*p == ',') {
        *p = '\0';
        p++;
      } else {
        *p = '\0';
      }
    }
    count++;
  }
  return count;
}

bool aslan_table_import_csv(AslanTable *table, const char *filepath) {
  if (!table || !filepath)
    return false;
  FILE *fp = fopen(filepath, "r");
  if (!fp)
    return false;

  char line[8192];

  // Read header line to create columns
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return false;
  }
  // Strip trailing newline
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    line[--len] = '\0';

  char *headers[64];
  int col_count = csv_parse_line(line, headers, 64);
  if (col_count <= 0) {
    fclose(fp);
    return false;
  }

  // Create columns (all TEXT type — universal)
  for (int c = 0; c < col_count; c++) {
    aslan_table_add_column(table, headers[c], ASLAN_COL_TEXT);
  }

  // Read data rows
  while (fgets(line, sizeof(line), fp)) {
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    if (len == 0)
      continue;

    char *fields[64];
    int field_count = csv_parse_line(line, fields, 64);

    uint64_t row_id = aslan_table_add_row(table);
    for (int c = 0; c < field_count && c < col_count; c++) {
      if (fields[c][0] != '\0') {
        aslan_table_set_cell(table, c, row_id, fields[c]);
      }
    }
  }

  fclose(fp);
  return true;
}
