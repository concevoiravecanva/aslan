#ifndef EXPORT_H
#define EXPORT_H

// =============================================================================
// PHASE 3: HIGH-QUALITY EXPORT (SVG & PDF)
// =============================================================================
// A robust C-based generator for high-quality export of the canvas.
// Following the Antirez philosophy: minimal dependencies, readable output.

#include "../document/document.h"
#include <stdbool.h>

// =============================================================================
// EXPORT OPTIONS
// =============================================================================

typedef struct {
  // Output dimensions (if 0, auto-calculate from content bounds)
  int width;
  int height;
  
  // Margins around content (in pixels)
  int margin;
  
  // Background color (RGBA, 0-255)
  int bg_r, bg_g, bg_b, bg_a;
  
  // Whether to include hidden nodes
  bool include_hidden;
  
  // Whether to embed fonts (SVG only)
  bool embed_fonts;
  
  // Font family for text
  char font_family[64];
  
  // Scale factor (1.0 = original size)
  float scale;
  
  // Title for the document
  char title[256];
  
  // Whether to include metadata
  bool include_metadata;
  
  // PDF-specific: page size (A4, Letter, etc.)
  char pdf_page_size[32];
  
  // PDF-specific: orientation (portrait/landscape)
  bool pdf_landscape;
} ExportOptions;

// Initialize export options with sensible defaults
void export_options_init(ExportOptions *opts);

// =============================================================================
// SVG EXPORT
// =============================================================================

// Export document to SVG file
// Returns true on success
bool export_to_svg(const Document *doc, const char *filename, 
                   const ExportOptions *opts);

// Export to SVG string (caller must free result)
char *export_to_svg_string(const Document *doc, const ExportOptions *opts);

// =============================================================================
// PDF EXPORT
// =============================================================================

// Export document to PDF file
// Returns true on success
bool export_to_pdf(const Document *doc, const char *filename,
                   const ExportOptions *opts);

// =============================================================================
// PNG EXPORT (via SDL)
// =============================================================================

// Export visible canvas to PNG file
// Requires SDL renderer context
struct SDL_Renderer;
bool export_to_png(struct SDL_Renderer *renderer, const Document *doc,
                   const char *filename, const ExportOptions *opts);

// =============================================================================
// CONTENT BOUNDS CALCULATION
// =============================================================================

typedef struct {
  float min_x, min_y;
  float max_x, max_y;
  int node_count;
} ContentBounds;

// Calculate the bounding box of all visible content
ContentBounds calculate_content_bounds(const Document *doc, bool include_hidden);

#endif // EXPORT_H
