// =============================================================================
// ASLAN UI SHARED - Internal utilities, logging, and constants
// =============================================================================
// This header is included by all UI modules for shared functionality.

#ifndef UI_SHARED_H
#define UI_SHARED_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

// =============================================================================
// Logging System
// =============================================================================

#define ASLAN_LOG_DEBUG 0
#define ASLAN_LOG_INFO 1
#define ASLAN_LOG_WARN 2
#define ASLAN_LOG_ERROR 3

#ifndef ASLAN_LOG_LEVEL
#define ASLAN_LOG_LEVEL ASLAN_LOG_WARN
#endif

#define ASLAN_LOG(level, fmt, ...)                                             \
  do {                                                                         \
    if ((level) >= ASLAN_LOG_LEVEL) {                                          \
      static const char *_lvl[] = {"DEBUG", "INFO", "WARN", "ERROR"};          \
      fprintf(stderr, "[ASLAN][%s] %s:%d: " fmt "\n", _lvl[level], __func__,   \
              __LINE__, ##__VA_ARGS__);                                        \
    }                                                                          \
  } while (0)

#define ASLAN_DEBUG(fmt, ...) ASLAN_LOG(ASLAN_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define ASLAN_INFO(fmt, ...) ASLAN_LOG(ASLAN_LOG_INFO, fmt, ##__VA_ARGS__)
#define ASLAN_WARN(fmt, ...) ASLAN_LOG(ASLAN_LOG_WARN, fmt, ##__VA_ARGS__)
#define ASLAN_ERROR(fmt, ...) ASLAN_LOG(ASLAN_LOG_ERROR, fmt, ##__VA_ARGS__)

// =============================================================================
// Assertions
// =============================================================================

#define ASLAN_ASSERT(cond)                                                     \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ASLAN_ERROR("Assertion failed: %s", #cond);                              \
      assert(cond);                                                            \
    }                                                                          \
  } while (0)

#define ASLAN_ASSERT_PTR(ptr) ASLAN_ASSERT((ptr) != NULL)

// =============================================================================
// Color Constants (Notion-style Gray Hierarchy)
// =============================================================================

// Light mode
#define UI_COLOR_BG 0xFBFBFAFF // #FBFBFA
#define UI_COLOR_PANEL 0xFBFBFAFF
#define UI_COLOR_BORDER 0xEDECE9FF // #EDECE9
#define UI_COLOR_TEXT 0x37352FFF   // #37352F (Anthracite)
#define UI_COLOR_TEXT_DIM 0x8C8C8CFF
#define UI_COLOR_ACCENT 0x6E6E82FF // Neutral grey-blue (#6E6E82)

// Highlights
#define UI_COLOR_HIGHLIGHT_YELLOW 0xFFF06450 // 80 alpha
#define UI_COLOR_HIGHLIGHT_BLUE 0x8C8C8C30   // Neutral grey highight
#define UI_COLOR_HIGHLIGHT_GREEN 0x64DC8C50

// =============================================================================
// UI Constants
// =============================================================================

#define UI_SIDE_PADDING 60.0f

// =============================================================================
// Utility Macros
// =============================================================================

#define UI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define UI_MAX(a, b) ((a) > (b) ? (a) : (b))
#define UI_CLAMP(x, lo, hi) UI_MIN(UI_MAX(x, lo), hi)

// =============================================================================
// Test Function Prototype
// =============================================================================

// Each module implements: int test_<module>(void);
// Returns 0 on success, non-zero on failure.

// Node style types for creation (shared between canvas and design)
typedef enum {
  NODE_STYLE_RECT,
  NODE_STYLE_ELLIPSE,
  NODE_STYLE_TRIANGLE,
  NODE_STYLE_LINE,
  NODE_STYLE_TEXT // Plain text, no background
} NodeStyleType;

#endif // UI_SHARED_H
