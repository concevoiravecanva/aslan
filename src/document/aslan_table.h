#ifndef ASLAN_TABLE_H
#define ASLAN_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// =============================================================================
// ASLAN COLUMNAR TABLE ENGINE
// =============================================================================
//
// High-performance column-store table with:
// - Contiguous memory storage per column (cache-friendly, SIMD-ready)
// - Virtual index for sorting (doesn't move actual data)
// - Soft delete via bitmask
// - Persistent row IDs for external references (Canvas)
// - Block allocation to minimize realloc overhead
//

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------

#define ASLAN_TABLE_BLOCK_SIZE 64       // Rows allocated per block
#define ASLAN_TABLE_MAX_COLUMNS 64      // Maximum columns per table
#define ASLAN_TABLE_MAX_STRING_LEN 4096 // Max string cell length
#define ASLAN_TABLE_ALIGNMENT 32        // Memory alignment for SIMD (AVX)

// -----------------------------------------------------------------------------
// Column Types (6 essential types for productivity)
// -----------------------------------------------------------------------------

typedef enum {
  ASLAN_COL_TEXT = 0, // char* - Free text (names, descriptions)
  ASLAN_COL_NUMBER,   // double - Decimal/integer (prices, quantities)
  ASLAN_COL_CHECKBOX, // uint8_t - Done/Not done, Paid/Unpaid
  ASLAN_COL_SELECT,   // int32_t - Index into options array (categories)
  ASLAN_COL_DATE,     // int64_t - Unix timestamp (for timeline linking)
  ASLAN_COL_FORMULA,  // double - C shunting-yard parser (simple math)
  ASLAN_COL_FILE,     // char* - Path to a local file
  ASLAN_COL_LINK,     // char* - URL/Link (Label|URL)
  ASLAN_COL_IMAGE,    // char* - Path to an image in assets/
  ASLAN_COL_TYPE_COUNT
} AslanColumnType;

// -----------------------------------------------------------------------------
// Footer Aggregation Types
// -----------------------------------------------------------------------------

typedef enum {
  ASLAN_AGG_NONE = 0, // No aggregation
  ASLAN_AGG_SUM,      // Σ Sum (numbers only)
  ASLAN_AGG_AVG,      // μ Average (numbers only)
  ASLAN_AGG_COUNT,    // Count non-empty cells
  ASLAN_AGG_PERCENT,  // % of checked (checkboxes only)
  ASLAN_AGG_MIN,      // Minimum value
  ASLAN_AGG_MAX,      // Maximum value
  ASLAN_AGG_COUNT_TYPES
} AslanAggregationType;

// -----------------------------------------------------------------------------
// Select Option (for dropdown columns)
// -----------------------------------------------------------------------------

#define ASLAN_MAX_SELECT_OPTIONS 32
#define ASLAN_SELECT_OPTION_LEN 64

typedef struct {
  char label[ASLAN_SELECT_OPTION_LEN]; // Display text
  uint32_t color;                      // RGBA color for tag display
} AslanSelectOption;

// -----------------------------------------------------------------------------
// Column Structure (Single Column of Data)
// -----------------------------------------------------------------------------

typedef struct {
  char name[64];        // Column name (e.g., "Prix", "Date")
  AslanColumnType type; // Data type

  void *data;      // Contiguous data array (aligned memory)
  size_t count;    // Number of rows with data
  size_t capacity; // Allocated capacity (in elements)

  // Column metadata
  double width_px;     // Display width in pixels
  uint32_t bg_color;   // Column background tint RGBA (0 = none)
  bool is_primary_key; // If true, values must be unique
  bool is_nullable;    // If true, allows NULL values

  // Footer aggregation
  AslanAggregationType footer_agg; // What to show in footer

  // For SELECT columns: dropdown options
  AslanSelectOption options[ASLAN_MAX_SELECT_OPTIONS];
  int option_count;

  // For FORMULA columns: expression
  char *formula; // C formula: "[Price] * 1.20"

  // Storage info:
  // TEXT:        char** (array of string pointers)
  // NUMBER:      double* (aligned array)
  // CHECKBOX:    uint8_t* (0 or 1)
  // SELECT:      int32_t* (index into options, -1 = none)
  // DATE:        int64_t* (Unix timestamp ms)
  // RELATION:    uint64_t* (row IDs)
  // FORMULA:     double* (cached C-evaluated values)
} AslanColumn;

// -----------------------------------------------------------------------------
// Table Structure (Collection of Columns)
// -----------------------------------------------------------------------------

struct AslanTable {
  uint32_t id;   // Unique table ID
  char name[64]; // Table name

  // Columns (column-store architecture)
  AslanColumn columns[ASLAN_TABLE_MAX_COLUMNS];
  size_t column_count;

  // Row management
  size_t row_count;    // Logical row count (including deleted)
  size_t row_capacity; // Allocated row capacity

  // Persistent row IDs (never reused, monotonically increasing)
  uint64_t *row_ids;    // Array of unique row IDs
  uint64_t next_row_id; // Next ID to assign

  // Soft delete bitmask (1 bit per row: 1=active, 0=deleted)
  uint8_t *active_mask;
  size_t active_count; // Count of non-deleted rows

  // Virtual index for sorted display (indices into row arrays)
  size_t *view_index; // Sorted view indices
  bool view_dirty;    // True if sort is stale

  // Sort state
  int sort_column;     // Column index for sorting (-1=none)
  bool sort_ascending; // Sort direction

  // Filter state
  char filter_text[128]; // Text filter (empty = no filter)
  size_t filtered_count; // Rows passing filter (= active_count if no filter)

  // AOF logging callback (set externally)
  void (*log_callback)(const char *cmd, void *userdata);
  void *log_userdata;
};

#ifndef ASLAN_TABLE_TYPEDEF
#define ASLAN_TABLE_TYPEDEF
typedef struct AslanTable AslanTable;
#endif

// -----------------------------------------------------------------------------
// Table Lifecycle
// -----------------------------------------------------------------------------

// Create a new empty table
AslanTable *aslan_table_create(uint32_t id, const char *name);

// Destroy table and free all memory
void aslan_table_destroy(AslanTable *table);

// Clear all data but keep structure
void aslan_table_clear(AslanTable *table);

// -----------------------------------------------------------------------------
// Column Management
// -----------------------------------------------------------------------------

// Add a new column (returns column index, or -1 on error)
int aslan_table_add_column(AslanTable *table, const char *name,
                           AslanColumnType type);

// Remove a column by index
bool aslan_table_remove_column(AslanTable *table, int col_idx);
bool aslan_table_swap_columns(AslanTable *table, int col_a, int col_b);
bool aslan_table_move_column(AslanTable *table, int from_idx, int to_idx);

// Find column index by name (returns -1 if not found)
int aslan_table_find_column(const AslanTable *table, const char *name);

// Get column type
AslanColumnType aslan_table_column_type(const AslanTable *table, int col_idx);

// Rename a column
bool aslan_table_rename_column(AslanTable *table, int col_idx,
                               const char *new_name);

// -----------------------------------------------------------------------------
// Row Management
// -----------------------------------------------------------------------------

// Add a new row (returns persistent row_id, or 0 on error)
uint64_t aslan_table_add_row(AslanTable *table);

// Soft delete a row by row_id (marks as inactive)
bool aslan_table_delete_row(AslanTable *table, uint64_t row_id);

// Restore a soft-deleted row
bool aslan_table_restore_row(AslanTable *table, uint64_t row_id);

// Hard delete: permanently remove all soft-deleted rows (compacts memory)
void aslan_table_compact(AslanTable *table);

// Check if row is active (not deleted)
bool aslan_table_row_active(const AslanTable *table, uint64_t row_id);

// Get internal index from row_id (returns -1 if not found)
int aslan_table_row_index(const AslanTable *table, uint64_t row_id);

// Move a row from one index to another
bool aslan_table_move_row(AslanTable *table, int from_idx, int to_idx);

// Get row_id from internal index
uint64_t aslan_table_row_id_at(const AslanTable *table, int index);

// -----------------------------------------------------------------------------
// Cell Setters (by row_id for stability)
// -----------------------------------------------------------------------------

bool aslan_table_set_text(AslanTable *table, int col_idx, uint64_t row_id,
                          const char *value);
bool aslan_table_set_number(AslanTable *table, int col_idx, uint64_t row_id,
                            double value);
bool aslan_table_set_checkbox(AslanTable *table, int col_idx, uint64_t row_id,
                              bool checked);
bool aslan_table_set_select(AslanTable *table, int col_idx, uint64_t row_id,
                            int option_idx);
bool aslan_table_set_date(AslanTable *table, int col_idx, uint64_t row_id,
                          int64_t timestamp_ms);
// Generic setter (auto-converts based on column type)
bool aslan_table_set_cell(AslanTable *table, int col_idx, uint64_t row_id,
                          const char *value_str);

// -----------------------------------------------------------------------------
// Cell Getters (by row_id)
// -----------------------------------------------------------------------------

const char *aslan_table_get_text(const AslanTable *table, int col_idx,
                                 uint64_t row_id);
double aslan_table_get_number(const AslanTable *table, int col_idx,
                              uint64_t row_id);
bool aslan_table_get_checkbox(const AslanTable *table, int col_idx,
                              uint64_t row_id);
int aslan_table_get_select(const AslanTable *table, int col_idx,
                           uint64_t row_id);
int64_t aslan_table_get_date(const AslanTable *table, int col_idx,
                             uint64_t row_id);
// Get cell as string (for display)
const char *aslan_table_get_cell_display(const AslanTable *table, int col_idx,
                                         uint64_t row_id, char *buf,
                                         size_t buf_size);

// -----------------------------------------------------------------------------
// Select Column Options
// -----------------------------------------------------------------------------

// Add a new option to a SELECT column (returns index, or -1 on failure)
int aslan_table_add_option(AslanTable *table, int col_idx, const char *label,
                           uint32_t color);

// Update an existing option's color
bool aslan_table_update_option_color(AslanTable *table, int col_idx,
                                     int option_idx, uint32_t color);

// Remove option from SELECT column
bool aslan_table_remove_option(AslanTable *table, int col_idx, int option_idx);

// Get option label by index
const char *aslan_table_get_option_label(const AslanTable *table, int col_idx,
                                         int option_idx);

// Get option color by index
uint32_t aslan_table_get_option_color(const AslanTable *table, int col_idx,
                                      int option_idx);

// Set option color by index
bool aslan_table_set_option_color(AslanTable *table, int col_idx,
                                  int option_idx, uint32_t color);

// Formula Column (C parser)
// -----------------------------------------------------------------------------

// Set formula expression for a FORMULA column
bool aslan_table_set_formula(AslanTable *table, int col_idx, const char *expr);

// Get formula expression
const char *aslan_table_get_formula(const AslanTable *table, int col_idx);

// Evaluate formula for a row (C parser — simple math)
double aslan_table_eval_formula(const AslanTable *table, int col_idx,
                                uint64_t row_id);

// -----------------------------------------------------------------------------
// Sorting & View
// -----------------------------------------------------------------------------

// Sort table by column (updates view_index, doesn't move data)
void aslan_table_sort(AslanTable *table, int col_idx, bool ascending);

// Clear sort (view shows insertion order)
void aslan_table_sort_clear(AslanTable *table);
void aslan_table_filter_rebuild(AslanTable *table);
uint64_t aslan_table_duplicate_row(AslanTable *table, uint64_t source_row_id);

// Get row_id at display position (respects sort and filters)
uint64_t aslan_table_view_row_id(const AslanTable *table, int view_idx);

// Get number of visible rows (active, non-deleted)
size_t aslan_table_view_count(const AslanTable *table);

// -----------------------------------------------------------------------------
// Aggregation Functions (Column-wise, SIMD-friendly)
// -----------------------------------------------------------------------------

double aslan_table_col_sum(const AslanTable *table, int col_idx);
double aslan_table_col_avg(const AslanTable *table, int col_idx);
double aslan_table_col_min(const AslanTable *table, int col_idx);
double aslan_table_col_max(const AslanTable *table, int col_idx);
size_t aslan_table_col_count(const AslanTable *table,
                             int col_idx); // Non-null count
double aslan_table_col_percent(const AslanTable *table,
                               int col_idx); // % checked (CHECKBOX only)

// Set/get footer aggregation type for a column
void aslan_table_set_footer_agg(AslanTable *table, int col_idx,
                                AslanAggregationType agg);
AslanAggregationType aslan_table_get_footer_agg(const AslanTable *table,
                                                int col_idx);

// Get footer value as formatted string (uses column's footer_agg setting)
const char *aslan_table_get_footer_display(const AslanTable *table, int col_idx,
                                           char *buf, size_t buf_size);

// -----------------------------------------------------------------------------
// AOF Logging (Persistence)
// -----------------------------------------------------------------------------

// Set logging callback for AOF persistence
void aslan_table_set_logger(AslanTable *table,
                            void (*callback)(const char *cmd, void *userdata),
                            void *userdata);

// Generate AOF command string (caller must free)
char *aslan_table_aof_set_cell(uint32_t table_id, uint64_t row_id, int col_idx,
                               const char *value_str);

// -----------------------------------------------------------------------------
// Debug & Serialization
// -----------------------------------------------------------------------------

// Print table to stdout (for debugging)
void aslan_table_print_debug(const AslanTable *table);

// Get memory usage statistics
typedef struct {
  size_t total_bytes;
  size_t data_bytes;
  size_t index_bytes;
  size_t overhead_bytes;
} AslanTableMemStats;

void aslan_table_mem_stats(const AslanTable *table, AslanTableMemStats *stats);

// -----------------------------------------------------------------------------
// Evaluate all formula columns (call after data changes)
void aslan_table_eval_formulas(AslanTable *table);

// CSV export/import
bool aslan_table_export_csv(const AslanTable *table, const char *filepath);
bool aslan_table_import_csv(AslanTable *table, const char *filepath);

#endif // ASLAN_TABLE_H
