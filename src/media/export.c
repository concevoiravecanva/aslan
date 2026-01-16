#include "export.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// =============================================================================
// PHASE 3: HIGH-QUALITY EXPORT (SVG & PDF)
// =============================================================================

// =============================================================================
// EXPORT OPTIONS
// =============================================================================

void export_options_init(ExportOptions *opts) {
  if (!opts)
    return;

  memset(opts, 0, sizeof(ExportOptions));

  opts->width = 0;  // Auto-calculate
  opts->height = 0; // Auto-calculate
  opts->margin = 40;

  // Dark background
  opts->bg_r = 30;
  opts->bg_g = 32;
  opts->bg_b = 36;
  opts->bg_a = 255;

  opts->include_hidden = false;
  opts->embed_fonts = true;
  strncpy(opts->font_family, "Inter, sans-serif", sizeof(opts->font_family) - 1);
  opts->scale = 1.0f;
  opts->include_metadata = true;

  strncpy(opts->pdf_page_size, "A4", sizeof(opts->pdf_page_size) - 1);
  opts->pdf_landscape = true;
}

// =============================================================================
// CONTENT BOUNDS CALCULATION
// =============================================================================

static void bounds_update_recursive(Node *node, ContentBounds *bounds,
                                    bool include_hidden) {
  if (!node)
    return;

  // Skip hidden nodes unless requested
  if (!include_hidden && !node->visible && node->type != NODE_TYPE_ROOT) {
    bounds_update_recursive(node->next_sibling, bounds, include_hidden);
    return;
  }

  // Update bounds for non-root nodes
  if (node->type != NODE_TYPE_ROOT && node->width > 0 && node->height > 0) {
    if (node->x < bounds->min_x)
      bounds->min_x = node->x;
    if (node->y < bounds->min_y)
      bounds->min_y = node->y;
    if (node->x + node->width > bounds->max_x)
      bounds->max_x = node->x + node->width;
    if (node->y + node->height > bounds->max_y)
      bounds->max_y = node->y + node->height;
    bounds->node_count++;
  }

  bounds_update_recursive(node->first_child, bounds, include_hidden);
  bounds_update_recursive(node->next_sibling, bounds, include_hidden);
}

ContentBounds calculate_content_bounds(const Document *doc, bool include_hidden) {
  ContentBounds bounds = {
      .min_x = 1e9f,
      .min_y = 1e9f,
      .max_x = -1e9f,
      .max_y = -1e9f,
      .node_count = 0,
  };

  if (!doc || !doc->root)
    return bounds;

  bounds_update_recursive(doc->root, &bounds, include_hidden);

  // If no nodes found, return zero bounds
  if (bounds.node_count == 0) {
    bounds.min_x = 0;
    bounds.min_y = 0;
    bounds.max_x = 800;
    bounds.max_y = 600;
  }

  return bounds;
}

// =============================================================================
// SVG EXPORT
// =============================================================================

// Escape special XML characters
static void svg_escape_text(const char *src, char *dst, size_t dst_size) {
  if (!src || !dst || dst_size == 0)
    return;

  size_t j = 0;
  for (size_t i = 0; src[i] && j < dst_size - 6; i++) {
    switch (src[i]) {
    case '&':
      strcpy(dst + j, "&amp;");
      j += 5;
      break;
    case '<':
      strcpy(dst + j, "&lt;");
      j += 4;
      break;
    case '>':
      strcpy(dst + j, "&gt;");
      j += 4;
      break;
    case '"':
      strcpy(dst + j, "&quot;");
      j += 6;
      break;
    case '\'':
      strcpy(dst + j, "&#39;");
      j += 5;
      break;
    default:
      dst[j++] = src[i];
      break;
    }
  }
  dst[j] = '\0';
}

// Color palette for nodes (based on type and ID)
static void get_node_color(Node *node, int *r, int *g, int *b) {
  switch (node->type) {
  case NODE_TYPE_RECT:
    *r = 70 + (node->id * 17) % 60;
    *g = 130 + (node->id * 23) % 80;
    *b = 180 + (node->id * 31) % 75;
    break;
  case NODE_TYPE_TEXT:
    *r = 60;
    *g = 60;
    *b = 70;
    break;
  case NODE_TYPE_FRAME:
    *r = 50;
    *g = 55;
    *b = 60;
    break;
  case NODE_TYPE_GROUP:
    *r = 80;
    *g = 85;
    *b = 90;
    break;
  case NODE_TYPE_NOTE:
    *r = 45;
    *g = 48;
    *b = 52;
    break;
  case NODE_TYPE_GRID:
    *r = 40;
    *g = 45;
    *b = 50;
    break;
  case NODE_TYPE_CLIP:
    *r = 55;
    *g = 45;
    *b = 50;
    break;
  default:
    *r = 100;
    *g = 100;
    *b = 100;
    break;
  }
}

static void svg_write_node(FILE *f, Node *node, float offset_x, float offset_y,
                           float scale, bool include_hidden,
                           const char *font_family) {
  if (!node || !f)
    return;

  // Skip hidden nodes
  if (!include_hidden && !node->visible && node->type != NODE_TYPE_ROOT) {
    svg_write_node(f, node->next_sibling, offset_x, offset_y, scale,
                   include_hidden, font_family);
    return;
  }

  float x = (node->x - offset_x) * scale;
  float y = (node->y - offset_y) * scale;
  float w = node->width * scale;
  float h = node->height * scale;

  int r, g, b;
  get_node_color(node, &r, &g, &b);

  char escaped_name[256];
  svg_escape_text(node->name, escaped_name, sizeof(escaped_name));

  char escaped_text[512];
  svg_escape_text(node->text, escaped_text, sizeof(escaped_text));

  switch (node->type) {
  case NODE_TYPE_ROOT:
    // Root node is invisible container
    break;

  case NODE_TYPE_RECT:
    fprintf(f, "  <g id=\"node-%u\" class=\"rect\">\n", node->id);
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "rx=\"4\" ry=\"4\" fill=\"rgb(%d,%d,%d)\" "
               "stroke=\"rgb(%d,%d,%d)\" stroke-width=\"1\" />\n",
            x, y, w, h, r, g, b, r + 30, g + 30, b + 30);
    if (escaped_name[0]) {
      fprintf(f, "    <text x=\"%.2f\" y=\"%.2f\" font-family=\"%s\" "
                 "font-size=\"%.1f\" fill=\"#ffffff\" "
                 "dominant-baseline=\"middle\" text-anchor=\"middle\">%s</text>\n",
              x + w / 2, y + h / 2, font_family, 14.0f * scale, escaped_name);
    }
    fprintf(f, "  </g>\n");
    break;

  case NODE_TYPE_TEXT:
    fprintf(f, "  <g id=\"node-%u\" class=\"text\">\n", node->id);
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "rx=\"2\" ry=\"2\" fill=\"rgb(%d,%d,%d)\" />\n",
            x, y, w, h, r, g, b);
    if (escaped_text[0]) {
      fprintf(f, "    <text x=\"%.2f\" y=\"%.2f\" font-family=\"%s\" "
                 "font-size=\"%.1f\" fill=\"#e0e0e0\">%s</text>\n",
              x + 8 * scale, y + 20 * scale, font_family, 14.0f * scale,
              escaped_text);
    }
    fprintf(f, "  </g>\n");
    break;

  case NODE_TYPE_FRAME:
    fprintf(f, "  <g id=\"node-%u\" class=\"frame\">\n", node->id);
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "rx=\"8\" ry=\"8\" fill=\"rgb(%d,%d,%d)\" "
               "stroke=\"#666\" stroke-width=\"1\" stroke-dasharray=\"4,4\" />\n",
            x, y, w, h, r, g, b);
    if (escaped_name[0]) {
      fprintf(f, "    <text x=\"%.2f\" y=\"%.2f\" font-family=\"%s\" "
                 "font-size=\"%.1f\" fill=\"#888\">%s</text>\n",
              x + 8 * scale, y - 6 * scale, font_family, 12.0f * scale,
              escaped_name);
    }
    fprintf(f, "  </g>\n");
    break;

  case NODE_TYPE_GROUP:
    fprintf(f, "  <g id=\"node-%u\" class=\"group\">\n", node->id);
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "fill=\"none\" stroke=\"#555\" stroke-width=\"1\" "
               "stroke-dasharray=\"2,2\" />\n",
            x, y, w, h);
    fprintf(f, "  </g>\n");
    break;

  case NODE_TYPE_NOTE: {
    fprintf(f, "  <g id=\"node-%u\" class=\"note\">\n", node->id);
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "rx=\"4\" ry=\"4\" fill=\"rgb(%d,%d,%d)\" "
               "stroke=\"#555\" stroke-width=\"1\" />\n",
            x, y, w, h, r, g, b);

    // Title bar
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "rx=\"4\" ry=\"4\" fill=\"rgb(%d,%d,%d)\" />\n",
            x, y, w, 28.0f * scale, r + 10, g + 10, b + 10);

    if (escaped_name[0]) {
      fprintf(f, "    <text x=\"%.2f\" y=\"%.2f\" font-family=\"%s\" "
                 "font-size=\"%.1f\" font-weight=\"600\" fill=\"#ddd\">%s</text>\n",
              x + 12 * scale, y + 18 * scale, font_family, 13.0f * scale,
              escaped_name);
    }

    // Note icon
    fprintf(f, "    <text x=\"%.2f\" y=\"%.2f\" font-size=\"%.1f\" "
               "fill=\"#888\">📝</text>\n",
            x + w - 24 * scale, y + 18 * scale, 12.0f * scale);

    fprintf(f, "  </g>\n");
    break;
  }

  case NODE_TYPE_GRID: {
    fprintf(f, "  <g id=\"node-%u\" class=\"grid\">\n", node->id);
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "rx=\"4\" ry=\"4\" fill=\"rgb(%d,%d,%d)\" "
               "stroke=\"#555\" stroke-width=\"1\" />\n",
            x, y, w, h, r, g, b);

    // Header
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "fill=\"rgb(%d,%d,%d)\" />\n",
            x, y, w, 24.0f * scale, r + 8, g + 8, b + 8);

    // Draw some grid lines
    float cell_w = w / 5;
    float cell_h = 20.0f * scale;
    for (int i = 1; i < 5; i++) {
      fprintf(f, "    <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                 "stroke=\"#444\" stroke-width=\"0.5\" />\n",
              x + i * cell_w, y + 24 * scale, x + i * cell_w, y + h);
    }
    for (float row_y = y + 24 * scale + cell_h; row_y < y + h; row_y += cell_h) {
      fprintf(f, "    <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                 "stroke=\"#333\" stroke-width=\"0.5\" />\n",
              x, row_y, x + w, row_y);
    }

    // Table icon
    fprintf(f, "    <text x=\"%.2f\" y=\"%.2f\" font-size=\"%.1f\" "
               "fill=\"#888\">📊</text>\n",
            x + w - 20 * scale, y + 16 * scale, 11.0f * scale);

    fprintf(f, "  </g>\n");
    break;
  }

  case NODE_TYPE_CLIP: {
    fprintf(f, "  <g id=\"node-%u\" class=\"clip\">\n", node->id);
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "rx=\"4\" ry=\"4\" fill=\"rgb(%d,%d,%d)\" "
               "stroke=\"#555\" stroke-width=\"1\" />\n",
            x, y, w, h, r, g, b);

    // Timeline track
    float track_y = y + 30 * scale;
    float track_h = h - 50 * scale;
    fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
               "fill=\"#2a2a30\" />\n",
            x + 10 * scale, track_y, w - 20 * scale, track_h);

    // Sample clips
    float clip_w = (w - 20 * scale) / 3;
    int colors[][3] = {{100, 150, 255}, {150, 200, 100}, {200, 150, 100}};
    for (int i = 0; i < 3; i++) {
      fprintf(f, "    <rect x=\"%.2f\" y=\"%.2f\" width=\"%.2f\" height=\"%.2f\" "
                 "rx=\"2\" ry=\"2\" fill=\"rgb(%d,%d,%d)\" />\n",
              x + 10 * scale + i * clip_w + 2, track_y + 4 * scale,
              clip_w - 4, track_h - 8 * scale,
              colors[i][0], colors[i][1], colors[i][2]);
    }

    // Playhead
    fprintf(f, "    <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
               "stroke=\"#ff5555\" stroke-width=\"2\" />\n",
            x + w * 0.3f, y + 20 * scale, x + w * 0.3f, y + h - 10 * scale);

    // Video icon
    fprintf(f, "    <text x=\"%.2f\" y=\"%.2f\" font-size=\"%.1f\" "
               "fill=\"#888\">🎬</text>\n",
            x + w - 24 * scale, y + 18 * scale, 12.0f * scale);

    fprintf(f, "  </g>\n");
    break;
  }

  default:
    // Generic rectangle for unknown types
    fprintf(f, "  <rect id=\"node-%u\" x=\"%.2f\" y=\"%.2f\" "
               "width=\"%.2f\" height=\"%.2f\" fill=\"#555\" />\n",
            node->id, x, y, w, h);
    break;
  }

  // Recursively process children and siblings
  svg_write_node(f, node->first_child, offset_x, offset_y, scale,
                 include_hidden, font_family);
  svg_write_node(f, node->next_sibling, offset_x, offset_y, scale,
                 include_hidden, font_family);
}

bool export_to_svg(const Document *doc, const char *filename,
                   const ExportOptions *opts) {
  if (!doc || !filename)
    return false;

  ExportOptions default_opts;
  if (!opts) {
    export_options_init(&default_opts);
    opts = &default_opts;
  }

  // Calculate content bounds
  ContentBounds bounds = calculate_content_bounds(doc, opts->include_hidden);

  // Calculate final dimensions
  float content_w = bounds.max_x - bounds.min_x;
  float content_h = bounds.max_y - bounds.min_y;

  int svg_w = opts->width > 0 
      ? opts->width 
      : (int)((content_w + opts->margin * 2) * opts->scale);
  int svg_h = opts->height > 0 
      ? opts->height 
      : (int)((content_h + opts->margin * 2) * opts->scale);

  // Open file
  FILE *f = fopen(filename, "w");
  if (!f) {
    fprintf(stderr, "[EXPORT] Failed to create SVG file: %s\n", filename);
    return false;
  }

  // Write SVG header
  fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
  fprintf(f, "<!-- Generated by Aslan Canvas - %s -->\n", 
          opts->title[0] ? opts->title : "Untitled");
  fprintf(f, "<svg width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\" "
             "xmlns=\"http://www.w3.org/2000/svg\" "
             "xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n",
          svg_w, svg_h, svg_w, svg_h);

  // Metadata
  if (opts->include_metadata) {
    time_t now = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%S", localtime(&now));

    fprintf(f, "  <metadata>\n");
    fprintf(f, "    <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n");
    fprintf(f, "             xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n");
    fprintf(f, "      <rdf:Description>\n");
    fprintf(f, "        <dc:title>%s</dc:title>\n", 
            opts->title[0] ? opts->title : "Aslan Export");
    fprintf(f, "        <dc:date>%s</dc:date>\n", timestr);
    fprintf(f, "        <dc:creator>Aslan Canvas</dc:creator>\n");
    fprintf(f, "      </rdf:Description>\n");
    fprintf(f, "    </rdf:RDF>\n");
    fprintf(f, "  </metadata>\n");
  }

  // Style definitions
  fprintf(f, "  <defs>\n");
  fprintf(f, "    <style type=\"text/css\">\n");
  fprintf(f, "      .rect { filter: drop-shadow(2px 2px 4px rgba(0,0,0,0.3)); }\n");
  fprintf(f, "      .note { filter: drop-shadow(2px 2px 6px rgba(0,0,0,0.4)); }\n");
  fprintf(f, "      .grid { filter: drop-shadow(2px 2px 6px rgba(0,0,0,0.4)); }\n");
  fprintf(f, "      .clip { filter: drop-shadow(2px 2px 6px rgba(0,0,0,0.4)); }\n");
  fprintf(f, "    </style>\n");
  fprintf(f, "  </defs>\n");

  // Background
  fprintf(f, "  <rect width=\"100%%\" height=\"100%%\" "
             "fill=\"rgb(%d,%d,%d)\" />\n",
          opts->bg_r, opts->bg_g, opts->bg_b);

  // Export nodes
  float offset_x = bounds.min_x - opts->margin / opts->scale;
  float offset_y = bounds.min_y - opts->margin / opts->scale;

  svg_write_node(f, doc->root, offset_x, offset_y, opts->scale,
                 opts->include_hidden, opts->font_family);

  // Close SVG
  fprintf(f, "</svg>\n");
  fclose(f);

  printf("[EXPORT] SVG saved to: %s (%dx%d, %d nodes)\n", 
         filename, svg_w, svg_h, bounds.node_count);

  return true;
}

char *export_to_svg_string(const Document *doc, const ExportOptions *opts) {
  // Write to temp file, read back as string
  // This is simpler than managing a dynamic string buffer
  
  const char *tmp_path = "/tmp/aslan_export_temp.svg";
  
  if (!export_to_svg(doc, tmp_path, opts)) {
    return NULL;
  }
  
  FILE *f = fopen(tmp_path, "r");
  if (!f) return NULL;
  
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  
  char *str = (char *)malloc((size_t)size + 1);
  if (!str) {
    fclose(f);
    return NULL;
  }
  
  size_t read = fread(str, 1, (size_t)size, f);
  str[read] = '\0';
  fclose(f);
  
  // Clean up temp file
  remove(tmp_path);
  
  return str;
}

// =============================================================================
// PDF EXPORT
// =============================================================================
// Minimal PDF generator following PDF 1.4 spec.
// No external dependencies - just raw PDF stream output.

typedef struct {
  FILE *f;
  int obj_count;
  long *obj_offsets;
  int obj_cap;
  long stream_start;
} PdfWriter;

static void pdf_writer_init(PdfWriter *pw, FILE *f) {
  pw->f = f;
  pw->obj_count = 0;
  pw->obj_cap = 256;
  pw->obj_offsets = (long *)calloc((size_t)pw->obj_cap, sizeof(long));
  pw->stream_start = 0;
}

static void pdf_writer_free(PdfWriter *pw) {
  free(pw->obj_offsets);
}

static int pdf_new_obj(PdfWriter *pw) {
  if (pw->obj_count >= pw->obj_cap) {
    pw->obj_cap *= 2;
    pw->obj_offsets = (long *)realloc(pw->obj_offsets, 
                                      (size_t)pw->obj_cap * sizeof(long));
  }
  pw->obj_offsets[pw->obj_count] = ftell(pw->f);
  pw->obj_count++;
  fprintf(pw->f, "%d 0 obj\n", pw->obj_count);
  return pw->obj_count;
}

static void pdf_end_obj(PdfWriter *pw) {
  fprintf(pw->f, "endobj\n\n");
  (void)pw;
}

static void pdf_write_node_graphics(FILE *f, Node *node, float offset_x,
                                    float offset_y, float scale, float page_h) {
  if (!node || !f)
    return;

  if (node->type != NODE_TYPE_ROOT && node->visible && 
      node->width > 0 && node->height > 0) {
    
    float x = (node->x - offset_x) * scale;
    // PDF has Y origin at bottom, so flip
    float y = page_h - (node->y - offset_y + node->height) * scale;
    float w = node->width * scale;
    float h = node->height * scale;

    int r, g, b;
    get_node_color(node, &r, &g, &b);

    // Set fill color
    fprintf(f, "%.3f %.3f %.3f rg\n", r / 255.0f, g / 255.0f, b / 255.0f);

    // Draw rectangle
    fprintf(f, "%.2f %.2f %.2f %.2f re f\n", x, y, w, h);

    // Draw border
    fprintf(f, "0.4 0.4 0.4 RG\n");
    fprintf(f, "1 w\n");
    fprintf(f, "%.2f %.2f %.2f %.2f re S\n", x, y, w, h);
  }

  pdf_write_node_graphics(f, node->first_child, offset_x, offset_y, scale, page_h);
  pdf_write_node_graphics(f, node->next_sibling, offset_x, offset_y, scale, page_h);
}

bool export_to_pdf(const Document *doc, const char *filename,
                   const ExportOptions *opts) {
  if (!doc || !filename)
    return false;

  ExportOptions default_opts;
  if (!opts) {
    export_options_init(&default_opts);
    opts = &default_opts;
  }

  // Page dimensions (points, 72 points = 1 inch)
  float page_w, page_h;
  if (strcmp(opts->pdf_page_size, "Letter") == 0) {
    page_w = 612;
    page_h = 792;
  } else if (strcmp(opts->pdf_page_size, "A3") == 0) {
    page_w = 842;
    page_h = 1191;
  } else {
    // Default A4
    page_w = 595;
    page_h = 842;
  }

  if (opts->pdf_landscape) {
    float tmp = page_w;
    page_w = page_h;
    page_h = tmp;
  }

  // Calculate content bounds
  ContentBounds bounds = calculate_content_bounds(doc, opts->include_hidden);
  float content_w = bounds.max_x - bounds.min_x;
  float content_h = bounds.max_y - bounds.min_y;

  // Calculate scale to fit content on page (with margins)
  float margin = 36.0f; // 0.5 inch
  float avail_w = page_w - 2 * margin;
  float avail_h = page_h - 2 * margin;
  float scale_x = avail_w / content_w;
  float scale_y = avail_h / content_h;
  float scale = (scale_x < scale_y) ? scale_x : scale_y;
  if (scale > 1.0f)
    scale = 1.0f;

  // Open file
  FILE *f = fopen(filename, "wb");
  if (!f) {
    fprintf(stderr, "[EXPORT] Failed to create PDF file: %s\n", filename);
    return false;
  }

  PdfWriter pw;
  pdf_writer_init(&pw, f);

  // PDF Header
  fprintf(f, "%%PDF-1.4\n");
  fprintf(f, "%%\xc3\xa4\xc3\xbc\xc3\xb6\xc3\x9f\n"); // Binary marker

  // Object 1: Catalog
  int catalog_obj = pdf_new_obj(&pw);
  fprintf(f, "<< /Type /Catalog /Pages 2 0 R >>\n");
  pdf_end_obj(&pw);

  // Object 2: Pages
  int pages_obj = pdf_new_obj(&pw);
  fprintf(f, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n");
  pdf_end_obj(&pw);

  // Object 3: Page
  int page_obj = pdf_new_obj(&pw);
  fprintf(f, "<< /Type /Page /Parent 2 0 R "
             "/MediaBox [0 0 %.2f %.2f] "
             "/Contents 4 0 R "
             "/Resources << /Font << /F1 5 0 R >> >> >>\n",
          page_w, page_h);
  pdf_end_obj(&pw);

  // Object 4: Page content stream
  int content_obj = pdf_new_obj(&pw);
  fprintf(f, "<< /Length 6 0 R >>\n");
  fprintf(f, "stream\n");
  long stream_start = ftell(f);

  // Draw background
  fprintf(f, "%.3f %.3f %.3f rg\n", 
          opts->bg_r / 255.0f, opts->bg_g / 255.0f, opts->bg_b / 255.0f);
  fprintf(f, "0 0 %.2f %.2f re f\n", page_w, page_h);

  // Transform to center content
  float offset_x = bounds.min_x - margin / scale;
  float offset_y = bounds.min_y - margin / scale;

  // Draw nodes
  pdf_write_node_graphics(f, doc->root, offset_x, offset_y, scale, page_h);

  // Add title text
  if (opts->title[0]) {
    fprintf(f, "BT\n");
    fprintf(f, "/F1 14 Tf\n");
    fprintf(f, "0.8 0.8 0.8 rg\n");
    fprintf(f, "%.2f %.2f Td\n", margin, page_h - margin + 10);
    fprintf(f, "(%s) Tj\n", opts->title);
    fprintf(f, "ET\n");
  }

  long stream_end = ftell(f);
  fprintf(f, "\nendstream\n");
  pdf_end_obj(&pw);

  // Object 5: Font
  int font_obj = pdf_new_obj(&pw);
  fprintf(f, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n");
  pdf_end_obj(&pw);

  // Object 6: Stream length
  int length_obj = pdf_new_obj(&pw);
  fprintf(f, "%ld\n", stream_end - stream_start);
  pdf_end_obj(&pw);

  // Cross-reference table
  long xref_pos = ftell(f);
  fprintf(f, "xref\n");
  fprintf(f, "0 %d\n", pw.obj_count + 1);
  fprintf(f, "0000000000 65535 f \n");
  for (int i = 0; i < pw.obj_count; i++) {
    fprintf(f, "%010ld 00000 n \n", pw.obj_offsets[i]);
  }

  // Trailer
  fprintf(f, "trailer\n");
  fprintf(f, "<< /Size %d /Root 1 0 R >>\n", pw.obj_count + 1);
  fprintf(f, "startxref\n");
  fprintf(f, "%ld\n", xref_pos);
  fprintf(f, "%%%%EOF\n");

  fclose(f);
  pdf_writer_free(&pw);

  printf("[EXPORT] PDF saved to: %s (%.0fx%.0f points, %d nodes)\n",
         filename, page_w, page_h, bounds.node_count);

  // Suppress unused warnings
  (void)catalog_obj;
  (void)pages_obj;
  (void)page_obj;
  (void)content_obj;
  (void)font_obj;
  (void)length_obj;

  return true;
}

// =============================================================================
// PNG EXPORT (via SDL)
// =============================================================================

bool export_to_png(struct SDL_Renderer *renderer, const Document *doc,
                   const char *filename, const ExportOptions *opts) {
  // This would require SDL_image for PNG writing
  // For now, return false and suggest using SVG export
  (void)renderer;
  (void)doc;
  (void)filename;
  (void)opts;
  
  fprintf(stderr, "[EXPORT] PNG export not implemented. Use SVG export instead.\n");
  return false;
}
