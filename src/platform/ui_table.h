// =============================================================================
// ASLAN UI TABLE - Table Block Rendering and Logic
// =============================================================================

#ifndef UI_TABLE_H
#define UI_TABLE_H

#include "../document/aslan_table.h"
#include "ui_host.h"

// -----------------------------------------------------------------------------
// Table Rendering
// -----------------------------------------------------------------------------

void ui_render_block_table(UIHost *ui, Engine *engine, Block *block,
                           float width);

// -----------------------------------------------------------------------------
// Table Helpers
// -----------------------------------------------------------------------------

const char *table_col_type_icon(AslanColumnType type);
const char *table_col_type_name(AslanColumnType type);

// -----------------------------------------------------------------------------
// Test Function
// -----------------------------------------------------------------------------

int test_ui_table(void);

#endif // UI_TABLE_H
