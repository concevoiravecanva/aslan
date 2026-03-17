#ifndef UI_CORE_H
#define UI_CORE_H

#include "ui_host.h"

// UI cursor helpers
void ui_set_cursor(UIHost *ui, UICursorType type);
void ui_commit_cursor(UIHost *ui);

#endif // UI_CORE_H
