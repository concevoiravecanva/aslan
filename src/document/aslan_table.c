#include "aslan_table.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
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
  case ASLAN_COL_RELATION:
    return sizeof(uint64_t);
  case ASLAN_COL_FORMULA:
    return sizeof(double); // Cached result
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
    if (col->type == ASLAN_COL_TEXT) {
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
    if (col->type == ASLAN_COL_TEXT) {
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
  col->width_px = 120.0;
  col->is_primary_key = false;
  col->is_nullable = true;
  col->footer_agg = ASLAN_AGG_NONE;
  col->option_count = 0;
  col->relation_table_id = 0;
  col->relation_display_col = -1;
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

  if (col->type == ASLAN_COL_TEXT) {
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

int aslan_table_find_column(const AslanTable *table, const char *name) {
  if (!table || !name)
    return -1;
  for (size_t i = 0; i < table->column_count; i++) {
    if (strcmp(table->columns[i].name, name) == 0) {
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
  if (col->type != ASLAN_COL_TEXT)
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

bool aslan_table_set_relation(AslanTable *table, int col_idx, uint64_t row_id,
                              uint64_t target_row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_RELATION)
    return false;

  uint64_t *data = (uint64_t *)col->data;
  data[row_idx] = target_row_id;

  if (table->log_callback) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TABLE_SET_CELL %u %lu %d %lu", table->id,
             (unsigned long)row_id, col_idx, (unsigned long)target_row_id);
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
  case ASLAN_COL_RELATION: {
    uint64_t target = value_str ? strtoull(value_str, NULL, 10) : 0;
    return aslan_table_set_relation(table, col_idx, row_id, target);
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
  if (col->type != ASLAN_COL_TEXT)
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
  if (col->type != ASLAN_COL_NUMBER && col->type != ASLAN_COL_FORMULA)
    return 0.0;

  const double *data = (const double *)col->data;
  return data[row_idx];
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

uint64_t aslan_table_get_relation(const AslanTable *table, int col_idx,
                                  uint64_t row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0;
  int row_idx = aslan_table_row_index(table, row_id);
  if (row_idx < 0)
    return 0;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_RELATION)
    return 0;

  const uint64_t *data = (const uint64_t *)col->data;
  return data[row_idx];
}

// Thread-local buffer for display strings
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
  case ASLAN_COL_TEXT: {
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
  case ASLAN_COL_RELATION: {
    uint64_t target = ((uint64_t *)col->data)[row_idx];
    if (target > 0) {
      snprintf(buf, buf_size, "#%lu", (unsigned long)target);
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
// Relation Column Helpers
// -----------------------------------------------------------------------------

void aslan_table_set_relation_target(AslanTable *table, int col_idx,
                                     uint32_t target_table_id,
                                     int display_col) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_RELATION)
    return;

  col->relation_table_id = target_table_id;
  col->relation_display_col = display_col;
}

const char *aslan_table_get_relation_display(const AslanTable *table,
                                             int col_idx, uint64_t row_id,
                                             const AslanTable *target_table,
                                             char *buf, size_t buf_size) {
  if (!buf || buf_size == 0) {
    buf = display_buffer;
    buf_size = sizeof(display_buffer);
  }

  buf[0] = '\0';
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return buf;

  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_RELATION)
    return buf;

  uint64_t target_row_id = aslan_table_get_relation(table, col_idx, row_id);
  if (target_row_id == 0)
    return buf;

  if (!target_table) {
    snprintf(buf, buf_size, "#%lu", (unsigned long)target_row_id);
    return buf;
  }

  int display_col = col->relation_display_col;
  if (display_col >= 0 && display_col < (int)target_table->column_count) {
    return aslan_table_get_cell_display(target_table, display_col,
                                        target_row_id, buf, buf_size);
  }

  snprintf(buf, buf_size, "#%lu", (unsigned long)target_row_id);
  return buf;
}

// -----------------------------------------------------------------------------
// Formula Column
// -----------------------------------------------------------------------------

bool aslan_table_set_formula(AslanTable *table, int col_idx,
                             const char *lua_expr) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return false;

  AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_FORMULA)
    return false;

  free(col->formula);
  col->formula = lua_expr ? strdup(lua_expr) : NULL;
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

// Formula evaluation is handled externally via Lua
// -----------------------------------------------------------------------------
// Lightweight C Math Parser (Shunting-yard algorithm)
// -----------------------------------------------------------------------------

typedef enum {
  TOKEN_NUMBER,
  TOKEN_OPERATOR,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_COLUMN, // [ColumnName]
  TOKEN_EOF,
  TOKEN_ERROR
} TokenType;

typedef struct {
  TokenType type;
  double value;
  char name[64];
  char op;
} Token;

static Token get_token(const char **input) {
  while (**input && (**input == ' ' || **input == '\t'))
    (*input)++;

  if (**input == '\0')
    return (Token){TOKEN_EOF, 0, "", 0};

  char c = **input;
  if ((c >= '0' && c <= '9') || c == '.') {
    char *end;
    double val = strtod(*input, &end);
    *input = end;
    return (Token){TOKEN_NUMBER, val, "", 0};
  }

  if (c == '+' || c == '-' || c == '*' || c == '/') {
    (*input)++;
    return (Token){TOKEN_OPERATOR, 0, "", c};
  }

  if (c == '(') {
    (*input)++;
    return (Token){TOKEN_LPAREN, 0, "", 0};
  }

  if (c == ')') {
    (*input)++;
    return (Token){TOKEN_RPAREN, 0, "", 0};
  }

  if (c == '[') {
    (*input)++;
    const char *start = *input;
    while (**input && **input != ']')
      (*input)++;
    size_t len = *input - start;
    Token t = {TOKEN_COLUMN, 0, "", 0};
    if (len < 64) {
      strncpy(t.name, start, len);
      t.name[len] = '\0';
    }
    if (**input == ']')
      (*input)++;
    return t;
  }

  return (Token){TOKEN_ERROR, 0, "", 0};
}

static int op_precedence(char op) {
  if (op == '+' || op == '-')
    return 1;
  if (op == '*' || op == '/')
    return 2;
  return 0;
}

double aslan_table_eval_formula(const AslanTable *table, int col_idx,
                                uint64_t row_id) {
  if (!table || col_idx < 0 || col_idx >= (int)table->column_count)
    return 0.0;
  const AslanColumn *col = &table->columns[col_idx];
  if (col->type != ASLAN_COL_FORMULA || !col->formula)
    return 0.0;

  const char *expr = col->formula;
  Token output_queue[64];
  int oq_count = 0;
  Token op_stack[64];
  int os_count = 0;

  const char *ptr = expr;
  Token t;
  while ((t = get_token(&ptr)).type != TOKEN_EOF) {
    if (t.type == TOKEN_ERROR)
      return 0.0;
    if (t.type == TOKEN_NUMBER || t.type == TOKEN_COLUMN) {
      output_queue[oq_count++] = t;
    } else if (t.type == TOKEN_OPERATOR) {
      while (os_count > 0 && op_stack[os_count - 1].type == TOKEN_OPERATOR &&
             op_precedence(op_stack[os_count - 1].op) >= op_precedence(t.op)) {
        output_queue[oq_count++] = op_stack[--os_count];
      }
      op_stack[os_count++] = t;
    } else if (t.type == TOKEN_LPAREN) {
      op_stack[os_count++] = t;
    } else if (t.type == TOKEN_RPAREN) {
      while (os_count > 0 && op_stack[os_count - 1].type != TOKEN_LPAREN) {
        output_queue[oq_count++] = op_stack[--os_count];
      }
      if (os_count > 0)
        os_count--; // Pop '('
    }
  }
  while (os_count > 0)
    output_queue[oq_count++] = op_stack[--os_count];

  // Evaluate RPN
  double eval_stack[64];
  int es_count = 0;
  for (int i = 0; i < oq_count; i++) {
    t = output_queue[i];
    if (t.type == TOKEN_NUMBER) {
      eval_stack[es_count++] = t.value;
    } else if (t.type == TOKEN_COLUMN) {
      int c_idx = aslan_table_find_column(table, t.name);
      if (c_idx < 0)
        eval_stack[es_count++] = 0.0;
      else
        eval_stack[es_count++] = aslan_table_get_number(table, c_idx, row_id);
    } else if (t.type == TOKEN_OPERATOR) {
      if (es_count < 2)
        return 0.0;
      double b = eval_stack[--es_count];
      double a = eval_stack[--es_count];
      switch (t.op) {
      case '+':
        eval_stack[es_count++] = a + b;
        break;
      case '-':
        eval_stack[es_count++] = a - b;
        break;
      case '*':
        eval_stack[es_count++] = a * b;
        break;
      case '/':
        eval_stack[es_count++] = b != 0 ? a / b : 0;
        break;
      }
    }
  }

  double result = es_count > 0 ? eval_stack[0] : 0.0;

  // Update cached value if possible
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
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_text);
    break;
  case ASLAN_COL_DATE:
  case ASLAN_COL_RELATION:
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_date);
    break;
  case ASLAN_COL_CHECKBOX:
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_checkbox);
    break;
  case ASLAN_COL_SELECT:
    qsort_r(table->view_index, view_count, sizeof(size_t), &ctx,
            compare_wrapper_date);
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
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_text, &ctx);
    break;
  case ASLAN_COL_DATE:
  case ASLAN_COL_RELATION:
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_date, &ctx);
    break;
  case ASLAN_COL_CHECKBOX:
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_checkbox,
            &ctx);
    break;
  case ASLAN_COL_SELECT:
    qsort_r(table->view_index, view_count, sizeof(size_t), compare_date, &ctx);
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
  return table ? table->active_count : 0;
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
      snprintf(buf, buf_size, "\xCE\xA3 %.2f", sum); // Σ
    }
    break;
  case ASLAN_AGG_AVG:
    if (col->type == ASLAN_COL_NUMBER || col->type == ASLAN_COL_FORMULA) {
      double avg = aslan_table_col_avg(table, col_idx);
      snprintf(buf, buf_size, "\xCE\xBC %.2f", avg); // μ
    }
    break;
  case ASLAN_AGG_COUNT: {
    size_t count = aslan_table_col_count(table, col_idx);
    snprintf(buf, buf_size, "# %zu", count);
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
  case ASLAN_COL_RELATION:
    return "REL";
  case ASLAN_COL_FORMULA:
    return "CALC";
  default:
    return "?";
  }
}

void aslan_table_print_debug(const AslanTable *table) {
  if (!table) {
    printf("[AslanTable] NULL\n");
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
// Lua Bindings (stub - to be implemented)
// -----------------------------------------------------------------------------

int luaopen_aslan_table(void *lua_state) {
  (void)lua_state;
  // TODO: Register Lua metatable for table access
  return 0;
}
