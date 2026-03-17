// =============================================================================
// ASLAN UI HOST v3 - Block-Based Document Interface
// =============================================================================

// Nuklear has been completely removed.
#include "ui_host.h"
#include "../document/aslan_table.h"

#include "clay_host.h"
#include "ui_canvas.h"
#include "ui_core.h"
#include "ui_design.h"
#include <SDL.h>

// NanoVG OpenGL3 backend
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif
#define NANOVG_GL3
#include "../vendor/nanovg/nanovg_gl.h"
#include "../vendor/nanovg/nanovg_gl_utils.h"

#define MAX_VERTEX_BUFFER (512 * 1024)
#define MAX_ELEMENT_BUFFER (128 * 1024)

#include "../vendor/stb_image_write.h"
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

// -----------------------------------------------------------------------------
// File Dialog Helper (macOS)
// -----------------------------------------------------------------------------

// Opens a native file dialog to select an image file
// Returns 1 on success, 0 on cancel/error. Path is written to out_path.
int open_image_file_dialog(char *out_path, size_t path_size) {
  // Use osascript to open macOS native file dialog
  const char *script =
      "osascript -e 'set theFile to choose file of type {\"public.image\"} "
      "with prompt \"Select an image\"' -e 'POSIX path of theFile' 2>/dev/null";

  FILE *fp = popen(script, "r");
  if (!fp)
    return 0;

  char result[512] = {0};
  if (fgets(result, sizeof(result), fp) != NULL) {
    // Remove trailing newline
    size_t len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') {
      result[len - 1] = '\0';
      len--;
    }
    if (len > 0) {
      strncpy(out_path, result, path_size - 1);
      out_path[path_size - 1] = '\0';
      pclose(fp);
      return 1;
    }
  }

  pclose(fp);
  return 0;
}

// Opens a native file dialog to select a font file (.ttf / .otf)
static int open_font_file_dialog(char *out_path, size_t path_size) {
  const char *script =
      "osascript -e 'set theFile to choose file of type "
      "{\"public.truetype-ttf-font\", \"public.opentype-font\", "
      "\"org.gnu.gnu-type-font\", \"public.font\"} "
      "with prompt \"Select a font file (.ttf or .otf)\"' "
      "-e 'POSIX path of theFile' 2>/dev/null";
  FILE *fp = popen(script, "r");
  if (!fp) return 0;
  char result[512] = {0};
  if (fgets(result, sizeof(result), fp)) {
    size_t len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') result[--len] = '\0';
    if (len > 0) {
      strncpy(out_path, result, path_size - 1);
      out_path[path_size - 1] = '\0';
      pclose(fp);
      return 1;
    }
  }
  pclose(fp);
  return 0;
}

// Import a font file, load it into NanoVG, register in UIHost.
// Returns the NanoVG font name on success (points into ui->custom_fonts), or NULL.
const char *ui_host_import_font(UIHost *ui, const char *path) {
  if (!ui || !path || ui->custom_font_count >= ASLAN_MAX_CUSTOM_FONTS)
    return NULL;

  // Derive a short display name from filename (strip directory + extension)
  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;
  char name[64];
  strncpy(name, base, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  char *dot = strrchr(name, '.');
  if (dot) *dot = '\0';

  // Ensure unique name (avoid collisions with existing NanoVG fonts)
  char unique[64];
  snprintf(unique, sizeof(unique), "custom_%s", name);

  int fid = nvgCreateFont(ui->vg, unique, path);
  if (fid < 0) {
    ASLAN_WARN("Failed to load font: %s", path);
    return NULL;
  }

  int idx = ui->custom_font_count++;
  strncpy(ui->custom_fonts[idx].name, unique, 63);
  strncpy(ui->custom_fonts[idx].path, path,   255);
  ASLAN_DEBUG("Imported font '%s' from %s", unique, path);
  return ui->custom_fonts[idx].name;
}

// Opens dialog + imports font. Returns the font name or NULL.
const char *ui_host_open_and_import_font(UIHost *ui) {
  char path[512];
  if (!open_font_file_dialog(path, sizeof(path))) return NULL;
  return ui_host_import_font(ui, path);
}

// Opens a native file dialog to select a CSV file for import
int open_csv_file_dialog(char *out_path, size_t path_size) {
  const char *script =
      "osascript -e 'set theFile to choose file of type "
      "{\"public.comma-separated-values-text\", \"public.text\"} "
      "with prompt \"Select a CSV file\"' -e 'POSIX path of theFile' "
      "2>/dev/null";
  FILE *fp = popen(script, "r");
  if (!fp)
    return 0;
  char result[512] = {0};
  if (fgets(result, sizeof(result), fp) != NULL) {
    size_t len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') {
      result[len - 1] = '\0';
      len--;
    }
    if (len > 0) {
      strncpy(out_path, result, path_size - 1);
      out_path[path_size - 1] = '\0';
      pclose(fp);
      return 1;
    }
  }
  pclose(fp);
  return 0;
}

// Copy file to assets directory, returns path relative to document
// Creates assets/ directory if needed
// Opens a native file save dialog for CSV
// Returns 1 on success, 0 on cancel/error. Path is written to out_path.
int save_csv_file_dialog(char *out_path, size_t path_size,
                         const char *default_name) {
  char script[1024];
  snprintf(
      script, sizeof(script),
      "osascript -e 'set theName to \"%s\"' "
      "-e 'set theFile to choose file name default name theName default "
      "location (path to desktop folder) with prompt \"Export Table as CSV\"' "
      "-e 'POSIX path of theFile' 2>/dev/null",
      default_name ? default_name : "Export.csv");

  FILE *fp = popen(script, "r");
  if (!fp)
    return 0;

  char result[512] = {0};
  if (fgets(result, sizeof(result), fp) != NULL) {
    // Remove trailing newline
    size_t len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') {
      result[len - 1] = '\0';
      len--;
    }
    if (len > 0) {
      strncpy(out_path, result, path_size - 1);
      out_path[path_size - 1] = '\0';
      pclose(fp);
      return 1;
    }
  }

  pclose(fp);
  return 0;
}

// General native file save dialog via AppleScript
static int save_native_file_dialog(char *out_path, size_t path_size,
                                   const char *default_name,
                                   const char *prompt) {
  char script[1024];
  snprintf(script, sizeof(script),
           "osascript -e 'set theName to \"%s\"' "
           "-e 'set theFile to choose file name default name theName default "
           "location (path to desktop folder) with prompt \"%s\"' "
           "-e 'POSIX path of theFile' 2>/dev/null",
           default_name ? default_name : "Export.file",
           prompt ? prompt : "Export");
  FILE *fp = popen(script, "r");
  if (!fp)
    return 0;
  char result[512] = {0};
  if (fgets(result, sizeof(result), fp) != NULL) {
    size_t len = strlen(result);
    if (len > 0 && result[len - 1] == '\n') {
      result[len - 1] = '\0';
      len--;
    }
    if (len > 0) {
      strncpy(out_path, result, path_size - 1);
      out_path[path_size - 1] = '\0';
      pclose(fp);
      return 1;
    }
  }
  pclose(fp);
  return 0;
}

// Export active design block as SVG
// Copy selected node or active block into clipboard
void ui_host_copy_node(UIHost *ui, Engine *engine) {
  if (!ui)
    return;

  // Priority 1: copy selected node (canvas/design)
  if (ui->selected_node) {
    Node *src = ui->selected_node;
    ui->clipboard_node.has_data = 1;
    ui->clipboard_node.type = src->type;
    strncpy(ui->clipboard_node.name, src->name,
            sizeof(ui->clipboard_node.name) - 1);
    ui->clipboard_node.x = src->x;
    ui->clipboard_node.y = src->y;
    ui->clipboard_node.width = src->width;
    ui->clipboard_node.height = src->height;
    ui->clipboard_node.rotation = src->rotation;
    ui->clipboard_node.corner_radius = src->corner_radius;
    ui->clipboard_node.fill_color = src->fill_color;
    ui->clipboard_node.stroke_color = src->stroke_color;
    ui->clipboard_node.stroke_width = src->stroke_width;
    ui->clipboard_node.font_size = src->font_size;
    ui->clipboard_node.font_type = src->font_type;
    ui->clipboard_node.line_start_style = src->line_start_style;
    ui->clipboard_node.line_end_style = src->line_end_style;
    strncpy(ui->clipboard_node.text, src->text,
            sizeof(ui->clipboard_node.text) - 1);
    strncpy(ui->clipboard_node.image_path, src->image_path,
            sizeof(ui->clipboard_node.image_path) - 1);
    // Clear block clipboard to avoid confusion on paste
    ui->clipboard_block.has_data = 0;
    ASLAN_DEBUG("Clipboard: copied node '%s' (type=%d)", src->name, src->type);
    return;
  }

  // Priority 2: copy active text/heading block (needs engine to look up block)
  if (ui->active_block_id != 0 && engine) {
    Page *page = doc_page_get_active(engine->doc);
    if (page) {
      Block *blk = page_find_block_by_id(page, ui->active_block_id);
      if (blk && (blk->type == BLOCK_TEXT || blk->type == BLOCK_HEADING_1)) {
        ui->clipboard_block.has_data = 1;
        ui->clipboard_block.type = blk->type;
        strncpy(ui->clipboard_block.text, blk->text ? blk->text : "",
                sizeof(ui->clipboard_block.text) - 1);
        // Clear node clipboard
        ui->clipboard_node.has_data = 0;
        ASLAN_DEBUG("Clipboard: copied block id=%u type=%d", blk->id,
                    blk->type);
      }
    }
  }
}

// Paste clipboard node into active canvas or design block
void ui_host_paste_node(UIHost *ui, Engine *engine) {
  if (!ui || (!ui->clipboard_node.has_data && !ui->clipboard_block.has_data) ||
      !engine)
    return;

  Page *page = doc_page_get_active(engine->doc);
  if (!page)
    return;

  // --- Branch: paste a copied text/heading block ---
  if (ui->clipboard_block.has_data && !ui->clipboard_node.has_data) {
    uint32_t new_id = page->next_block_id;
    char cmd_buf[256];
    snprintf(cmd_buf, sizeof(cmd_buf), "block_create %u %u %s", page->id,
             new_id, block_type_to_string(ui->clipboard_block.type));
    engine_execute_command(engine, cmd_buf);

    // Set text via block_text command (text may contain spaces → quoted)
    Block *new_block = page_find_block_by_id(page, new_id);
    if (new_block && ui->clipboard_block.text[0]) {
      char text_buf[4096 + 64];
      snprintf(text_buf, sizeof(text_buf), "block_text %u %u \"%s\"", page->id,
               new_id, ui->clipboard_block.text);
      engine_execute_command(engine, text_buf);
    }

    ui->active_block_id = new_id;
    ui_request_redraw(ui);
    ASLAN_DEBUG("Clipboard: pasted block id=%u type=%s", new_id,
                block_type_to_string(ui->clipboard_block.type));
    return;
  }

  // Find an appropriate design block for paste target
  Block *target = NULL;
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == BLOCK_DESIGN) {
      target = b;
      break;
    }
  }
  // Fall back to any design block
  if (!target) {
    for (Block *b = page->first_block; b; b = b->next) {
      if (b->type == BLOCK_DESIGN) {
        target = b;
        break;
      }
    }
  }
  if (!target)
    return;

  // Reserve node ID
  uint32_t new_id = target->data.design.next_node_id;

  // Issue create command — triggers undo snapshot via engine
  char cmd_buf[256];
  snprintf(cmd_buf, sizeof(cmd_buf),
           "block_design_node %u %u create %u %s \"%s\"", page->id, target->id,
           new_id, node_type_to_string(ui->clipboard_node.type),
           ui->clipboard_node.name);
  engine_execute_command(engine, cmd_buf);

  // Find the freshly created node and patch all properties
  Node *pasted = block_design_find_node(target, new_id);
  if (!pasted)
    return;

  // Offset paste by +20px so it doesn't overlap the source
  pasted->x = ui->clipboard_node.x + 20.0f;
  pasted->y = ui->clipboard_node.y + 20.0f;
  pasted->width = ui->clipboard_node.width;
  pasted->height = ui->clipboard_node.height;
  pasted->rotation = ui->clipboard_node.rotation;
  pasted->corner_radius = ui->clipboard_node.corner_radius;
  pasted->fill_color = ui->clipboard_node.fill_color;
  pasted->stroke_color = ui->clipboard_node.stroke_color;
  pasted->stroke_width = ui->clipboard_node.stroke_width;
  pasted->font_size = ui->clipboard_node.font_size;
  pasted->font_type = ui->clipboard_node.font_type;
  pasted->line_start_style = ui->clipboard_node.line_start_style;
  pasted->line_end_style = ui->clipboard_node.line_end_style;
  pasted->visible = 1;
  strncpy(pasted->text, ui->clipboard_node.text, sizeof(pasted->text) - 1);
  strncpy(pasted->image_path, ui->clipboard_node.image_path,
          sizeof(pasted->image_path) - 1);

  // Select the pasted node
  ui->selected_node = pasted;
  ui_request_redraw(ui);

  ASLAN_DEBUG("Clipboard: pasted node '%s' as id=%u", pasted->name, new_id);
}

// stubbed out SDL_Renderer based helpers
void ui_host_export_svg(UIHost *ui, Engine *engine) {
  (void)ui;
  Page *page = doc_page_get_active(engine->doc);
  if (!page)
    return;

  // Find the first design block on this page
  Block *design_block = NULL;
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == BLOCK_DESIGN) {
      design_block = b;
      break;
    }
  }
  if (!design_block)
    return;

  char filename[512];
  char default_name[128];
  snprintf(default_name, sizeof(default_name), "%s.svg", page->name);
  if (save_native_file_dialog(filename, sizeof(filename), default_name,
                              "Export Design as SVG")) {

    if (block_design_export_svg(design_block, filename, NULL, 0)) {
      ASLAN_DEBUG("Exported SVG to: %s", filename);
    }
  }
}

void ui_host_export_svg_merge(UIHost *ui, Engine *engine,
                              struct AslanTable *linked_tbl) {
  (void)ui;
  Page *page = doc_page_get_active(engine->doc);
  if (!page || !linked_tbl || linked_tbl->active_count == 0)
    return;

  // Find the first design block on this page
  Block *design_block = NULL;
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == BLOCK_DESIGN) {
      if (b->data.design.linked_table_id == linked_tbl->id) {
        design_block = b;
        break;
      }
    }
  }
  // Fallback
  if (!design_block) {
    for (Block *b = page->first_block; b; b = b->next) {
      if (b->type == BLOCK_DESIGN) {
        design_block = b;
        break;
      }
    }
  }

  if (!design_block)
    return;

  char base_filename[512];
  char default_name[128];
  snprintf(default_name, sizeof(default_name), "%s_Merge.svg", page->name);
  if (save_native_file_dialog(base_filename, sizeof(base_filename),
                              default_name, "Export Design as SVG")) {
    // Strip .svg to get base prefix
    char prefix[512];
    strncpy(prefix, base_filename, sizeof(prefix) - 1);
    prefix[sizeof(prefix) - 1] = '\0';
    char *ext = strstr(prefix, ".svg");
    if (ext)
      *ext = '\0';


    for (int i = 0; i < (int)linked_tbl->active_count; i++) {
      char out_file[512];
      snprintf(out_file, sizeof(out_file), "%s_%d.svg", prefix, i + 1);
      uint64_t row_id = aslan_table_view_row_id(linked_tbl, i);
      if (block_design_export_svg(design_block, out_file, linked_tbl, row_id)) {
        ASLAN_DEBUG("Exported SVG merge row %d to: %s", i + 1, out_file);
      }
    }
  }
}

// Render the design artboard to a pixel buffer. Returns RGBA malloc'd buffer
// (caller must free). Sets *out_w and *out_h to artboard dimensions.
static unsigned char *render_design_to_pixels(UIHost *ui, Engine *engine,
                                              Block *block,
                                              int *out_w, int *out_h) {
  DesignData *dd = &block->data.design;
  if (!dd->pages || dd->current_page < 0 ||
      dd->current_page >= dd->page_count)
    return NULL;

  int w = dd->width  > 0 ? (int)dd->width  : 800;
  int h = dd->height > 0 ? (int)dd->height : 600;
  *out_w = w;
  *out_h = h;

  // Create offscreen FBO at artboard dimensions
  NVGLUframebuffer *fb = nvgluCreateFramebuffer(ui->vg, w, h, 0);
  if (!fb)
    return NULL;

  nvgluBindFramebuffer(fb);
  glViewport(0, 0, w, h);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  nvgBeginFrame(ui->vg, (float)w, (float)h, 1.0f);

  // Background
  DesignPage *dp = &dd->pages[dd->current_page];
  uint32_t bg = dp->bg_color ? dp->bg_color : 0xFFFFFFFF;
  NVGcolor bg_col = nvgRGBA((bg >> 24) & 0xFF, (bg >> 16) & 0xFF,
                            (bg >>  8) & 0xFF,  bg & 0xFF);
  nvgBeginPath(ui->vg);
  nvgRect(ui->vg, 0, 0, (float)w, (float)h);
  nvgFillColor(ui->vg, bg_col);
  nvgFill(ui->vg);

  // Render all nodes at scale=1, origin (0,0) — no sidebar, no toolbar
  AslanRect bounds = {0, 0, (float)w, (float)h};
  if (dd->root) {
    for (Node *node = dd->root->first_child; node;
         node = node->next_sibling) {
      ui_render_node_tree(ui, engine, block, dd, node, bounds,
                         1.0f, 0.0f, 0.0f);
    }
  }

  nvgEndFrame(ui->vg);

  // Read pixels (OpenGL gives bottom-up, we flip to top-down)
  unsigned char *pixels = (unsigned char *)malloc((size_t)w * h * 4);
  if (!pixels) {
    nvgluDeleteFramebuffer(fb);
    nvgluBindFramebuffer(NULL);
    glViewport(0, 0, ui->win_width, ui->win_height);
    return NULL;
  }
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // Flip vertically
  unsigned char *row = (unsigned char *)malloc((size_t)w * 4);
  if (row) {
    for (int y = 0; y < h / 2; y++) {
      memcpy(row,                       pixels + (size_t) y          * w * 4, (size_t)w * 4);
      memcpy(pixels + (size_t) y          * w * 4,
             pixels + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
      memcpy(pixels + (size_t)(h - 1 - y) * w * 4, row, (size_t)w * 4);
    }
    free(row);
  }

  nvgluDeleteFramebuffer(fb);
  nvgluBindFramebuffer(NULL);
  glViewport(0, 0, ui->win_width, ui->win_height);

  return pixels;
}

void ui_host_export_png_jpeg(UIHost *ui, Engine *engine, int is_jpeg) {
  Page *page = doc_page_get_active(engine->doc);
  if (!page)
    return;

  Block *design_block = NULL;
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == BLOCK_DESIGN) { design_block = b; break; }
  }
  if (!design_block)
    return;

  char filename[1024];
  char default_name[128];
  snprintf(default_name, sizeof(default_name), "%s.%s", page->name,
           is_jpeg ? "jpg" : "png");

  if (!save_native_file_dialog(filename, sizeof(filename), default_name,
                               is_jpeg ? "Export Design as JPEG"
                                       : "Export Design as PNG"))
    return;

  int w = 0, h = 0;
  unsigned char *pixels =
      render_design_to_pixels(ui, engine, design_block, &w, &h);
  if (!pixels) {
    ASLAN_WARN("Raster export: failed to render design to pixels");
    return;
  }

  int ok = 0;
  if (is_jpeg) {
    ok = stbi_write_jpg(filename, w, h, 4, pixels, 92);
  } else {
    ok = stbi_write_png(filename, w, h, 4, pixels, w * 4);
  }

  free(pixels);

  if (ok)
    ASLAN_DEBUG("Exported %s to: %s", is_jpeg ? "JPEG" : "PNG", filename);
  else
    ASLAN_WARN("Raster export: stb_image_write failed for %s", filename);
}

void ui_host_export_pdf(UIHost *ui, Engine *engine) {
  (void)ui;
  Page *page = doc_page_get_active(engine->doc);
  if (!page)
    return;

  Block *design_block = NULL;
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == BLOCK_DESIGN) {
      design_block = b;
      break;
    }
  }
  if (!design_block)
    return;

  char filename[512];
  char default_name[128];
  snprintf(default_name, sizeof(default_name), "%s.pdf", page->name);
  if (save_native_file_dialog(filename, sizeof(filename), default_name,
                              "Export Design as PDF")) {

    if (block_design_export_pdf(design_block, filename, NULL, 0))
      ASLAN_DEBUG("Exported PDF to: %s", filename);
    else
      ASLAN_WARN("PDF export failed for: %s", filename);
  }
}

// Import a CSV file: open dialog, create table block, populate from CSV
void ui_host_import_csv(UIHost *ui, Engine *engine) {
  (void)ui;
  char csv_path[512];
  if (!open_csv_file_dialog(csv_path, sizeof(csv_path)))
    return;

  Page *page = doc_page_get_active(engine->doc);
  if (!page)
    return;

  // Create a new table block (id = 0 → auto-assigned)
  uint32_t new_id = page->next_block_id; // predict the ID
  char cmd_buf[128];
  snprintf(cmd_buf, sizeof(cmd_buf), "block_create %u %u table", page->id,
           new_id);
  engine_execute_command(engine, cmd_buf);

  // Find the newly created block
  Block *block = page_find_block_by_id(page, new_id);
  if (!block || block->type != BLOCK_TABLE || !block->data.table.columnar)
    return;

  // Import CSV into the table
  aslan_table_import_csv(block->data.table.columnar, csv_path);

  // Extract filename for table name
  const char *fname = strrchr(csv_path, '/');
  fname = fname ? fname + 1 : csv_path;
  char tname[64];
  strncpy(tname, fname, sizeof(tname) - 1);
  tname[sizeof(tname) - 1] = '\0';
  // Strip .csv extension
  char *dot = strrchr(tname, '.');
  if (dot)
    *dot = '\0';
  snprintf(block->data.table.columnar->name, 64, "%s", tname);

  ASLAN_DEBUG("Imported CSV: %s (%zu cols, %zu rows)", csv_path,
              block->data.table.columnar->column_count,
              block->data.table.columnar->active_count);
}

int copy_image_to_assets(const char *src_path, const char *doc_dir,
                         char *out_rel_path, size_t rel_path_size) {
  // Create assets directory
  char assets_dir[512];
  snprintf(assets_dir, sizeof(assets_dir), "%s/assets", doc_dir);
  mkdir(assets_dir, 0755); // Ignore error if exists

  ASLAN_DEBUG("Copying image: %s -> %s", src_path, assets_dir);

  // Get filename from path
  const char *filename = strrchr(src_path, '/');
  filename = filename ? filename + 1 : src_path;

  // Generate unique filename if needed (add timestamp)
  char dest_filename[256];
  time_t now = time(NULL);
  snprintf(dest_filename, sizeof(dest_filename), "%ld_%s", (long)now, filename);

  // Full destination path
  char dest_path[512];
  snprintf(dest_path, sizeof(dest_path), "%s/%s", assets_dir, dest_filename);

  // Copy file
  FILE *src = fopen(src_path, "rb");
  if (!src)
    return 0;

  FILE *dst = fopen(dest_path, "wb");
  if (!dst) {
    fclose(src);
    return 0;
  }

  char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
    fwrite(buf, 1, n, dst);
  }

  fclose(src);
  fclose(dst);

  // Return relative path
  snprintf(out_rel_path, rel_path_size, "assets/%s", dest_filename);
  return 1;
}

// Nuklear SDL OpenGL3 implementation removed

// -----------------------------------------------------------------------------
// Colors (exported for use by other UI modules)
// -----------------------------------------------------------------------------

AslanColor COLOR_BG = {255, 255, 255, 255}; // Pure white
AslanColor COLOR_PANEL = {255, 255, 255, 255};
AslanColor COLOR_BORDER = {228, 228, 231, 255}; // Subtle border
AslanColor COLOR_TEXT = {220, 220, 220, 255};
AslanColor COLOR_TEXT_DARK = {36, 36, 39, 255}; // Near-black
AslanColor COLOR_TEXT_DIM = {140, 140, 145, 255};
AslanColor COLOR_ACCENT = {110, 110, 130, 255};
// Unused highlight colors removed

// -----------------------------------------------------------------------------
// UI Initialization
// -----------------------------------------------------------------------------

UIHost *ui_init(SDL_Window *win) {
  UIHost *ui = (UIHost *)calloc(1, sizeof(UIHost));
  if (!ui)
    return NULL;

  ui->screen = 1; // Start in Editor (Skip Dashboard)
  ui->font_size_px = 16.0f;
  ui->sidebar_mode = 0; // Standard sidebar (Properties/Layers)

  // Load recents
  recents_load(&ui->recents, "recents.log");

  // Nuklear removed

  ASLAN_INFO("UI Init started (Aslan v0.1.7)");

  // Load fonts

  // Font paths removed
  // Nuklear fonts and styles removed.
  // NanoVG fonts are handled independently.

  ui->table_state.renaming_col_idx = -1;
  ui->table_state.resizing_col     = -1;
  ui->table_state.last_hdr_click_col = -1;
  ui->table_state.dragging_col     = -1;
  ui->table_state.drop_target_col  = -1;
  ui->table_state.show_footer_popup = -1;

  ui->canvas_state.selected_color = 0x000000FF; // Default black
  ui->canvas_state.pen_thickness = 2.0f;
  ui->canvas_state.pen_is_marker = 0;
  ui->design_state.selected_color = 0x000000FF;
  ui->design_state.pen_thickness = 2.0f;
  ui->design_state.pen_is_marker = 0;

  // Initialize NanoVG
  ui->vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  if (!ui->vg) {
    ASLAN_ERROR("Failed to initialize NanoVG!");
  }

  // Initialize Clay (layout engine + background rendering)
  {
    int ww = 1280, wh = 720;
    SDL_GetWindowSize(win, &ww, &wh);
    ui->clay = clay_host_init(ui->vg, ww, wh);
    if (!ui->clay) {
      ASLAN_WARN("Clay host init failed — running without Clay backgrounds");
    }
  }

  // Initialize NanoVG Widgets Context
  nvgw_init(&ui->nvgw, ui->vg);
  nvgw_set_fonts(&ui->nvgw, nvgFindFont(ui->vg, "Inter"),
                 nvgFindFont(ui->vg, "Icons"));

  return ui;
}

void ui_shutdown(UIHost *ui) {
  if (!ui)
    return;
  if (ui->vg) {
    // Cleanup de NanoVG si nécessaire (dépend de l'implémentation GL3)
    // nvgDeleteGL3 n'est pas exposé dans nanovg.h mais dans nanovg_gl.h
    // Pour simplifier on laisse l'OS nettoyer le contexte GL si on n'a pas
    // d'en-tête dédié facile à inclure ici sans polluer.
  }
  clay_host_destroy(ui->clay);
  ui->clay = NULL;
  free(ui);
}

void ui_request_redraw(UIHost *ui) {
  if (ui)
    ui->needs_redraw = 1;
}

// -----------------------------------------------------------------------------
// Event Handling
// -----------------------------------------------------------------------------

void ui_host_handle_file_drop(UIHost *ui, const char *file_path, int mx,
                              int my) {
  if (!ui || !file_path)
    return;

  ASLAN_DEBUG("File dropped: %s at (%d, %d)", file_path, mx, my);

  // Get document directory (use current working directory as fallback)
  char doc_dir[512] = ".";
  if (ui->engine && ui->engine->doc && ui->engine->doc->filepath[0]) {
    // Extract directory from document path
    strncpy(doc_dir, ui->engine->doc->filepath, sizeof(doc_dir) - 1);
    char *last_slash = strrchr(doc_dir, '/');
    if (last_slash) {
      *last_slash = '\0';
    } else {
      strcpy(doc_dir, ".");
    }
  }
  ASLAN_DEBUG("Document directory: %s", doc_dir);

  // 1. Try Canvas blocks
  CanvasInteraction *cs = &ui->canvas_state;
  ASLAN_DEBUG("Canvas bounds: x=%.1f y=%.1f w=%.1f h=%.1f", cs->bounds_x,
              cs->bounds_y, cs->bounds_w, cs->bounds_h);

  if (mx >= cs->bounds_x && mx < cs->bounds_x + cs->bounds_w &&
      my >= cs->bounds_y && my < cs->bounds_y + cs->bounds_h) {
    ASLAN_DEBUG("Mouse is in canvas bounds");
    if (cs->active_block && cs->active_block->type == BLOCK_DESIGN) {
      ASLAN_DEBUG("Active design block found");
      DesignData *dd = &cs->active_block->data.design;
      float wx = cs->cam_x + (mx - cs->bounds_x) / cs->zoom;
      float wy = cs->cam_y + (my - cs->bounds_y) / cs->zoom;
      ASLAN_DEBUG("World coords: wx=%.1f wy=%.1f (cam: %.1f,%.1f zoom: %.2f)",
                  wx, wy, cs->cam_x, cs->cam_y, cs->zoom);
      Node *hit = design_find_node_at(dd, wx, wy);
      if (hit) {
        ASLAN_DEBUG("Hit node found, is_container=%d", hit->is_container);
        if (hit->is_container) {
          char rel[256];
          if (copy_image_to_assets(file_path, doc_dir, rel, sizeof(rel))) {
            strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
            ASLAN_DEBUG("Set container image: %s", hit->image_path);
          } else {
            ASLAN_WARN("Failed to copy image to assets");
          }
          return;
        }
      } else {
        ASLAN_DEBUG("No node hit at world coords");
      }
    } else {
      ASLAN_DEBUG("No active canvas block");
    }
  } else {
    ASLAN_DEBUG("Mouse NOT in canvas bounds");
  }

  // 2. Try Design blocks
  DesignInteraction *ds = &ui->design_state;
  ASLAN_DEBUG("Design bounds: x=%.1f y=%.1f w=%.1f h=%.1f", ds->bounds_x,
              ds->bounds_y, ds->bounds_w, ds->bounds_h);

  if (mx >= ds->bounds_x && mx < ds->bounds_x + ds->bounds_w &&
      my >= ds->bounds_y && my < ds->bounds_y + ds->bounds_h) {
    ASLAN_DEBUG("Mouse is in design bounds");
    if (ds->active_block && ds->active_block->type == BLOCK_DESIGN) {
      ASLAN_DEBUG("Active design block found");
      DesignData *dd = &ds->active_block->data.design;
      float scale = ds->scale;
      float design_x = (mx - ds->bounds_x) / (scale > 0 ? scale : 1.0f);
      float design_y = (my - ds->bounds_y) / (scale > 0 ? scale : 1.0f);
      ASLAN_DEBUG("Design coords: x=%.1f y=%.1f (scale: %.2f)", design_x,
                  design_y, scale);
      Node *hit = design_find_node_at(dd, design_x, design_y);
      if (hit) {
        ASLAN_DEBUG("Hit node found, is_container=%d", hit->is_container);
        if (hit->is_container) {
          char rel[256];
          if (copy_image_to_assets(file_path, doc_dir, rel, sizeof(rel))) {
            strncpy(hit->image_path, rel, sizeof(hit->image_path) - 1);
            ASLAN_DEBUG("Set container image: %s", hit->image_path);
          } else {
            ASLAN_WARN("Failed to copy image to assets");
          }
          return;
        }
      } else {
        ASLAN_DEBUG("No node hit at design coords");
      }
    } else {
      ASLAN_DEBUG("No active design block");
    }
  } else {
    ASLAN_DEBUG("Mouse NOT in design bounds");
  }

  ASLAN_DEBUG("File drop not handled");
}

void ui_handle_event(UIHost *ui, SDL_Event *evt) {
  if (!ui || !evt)
    return;

  // Log mouse events at debug level (silenced by default)
  if (evt->type == SDL_MOUSEBUTTONDOWN) {
    ASLAN_DEBUG("Button Down: button=%d at (%d, %d)", evt->button.button,
                evt->button.x, evt->button.y);
  } else if (evt->type == SDL_MOUSEBUTTONUP) {
    ASLAN_DEBUG("Button Up: button=%d at (%d, %d)", evt->button.button,
                evt->button.x, evt->button.y);
  }

  // Pass to Nuklear first
  // nvgw_handle_event handled in main loop
  // NOTE: mouse_just_pressed/released are reset in ui_render after processing

  // Track mouse position
  if (evt->type == SDL_MOUSEMOTION) {
    ui->mouse_x = (float)evt->motion.x;
    ui->mouse_y = (float)evt->motion.y;

    // Block drag tracking
    if (ui->block_drag.is_active) {
      ui->block_drag.mouse_x = ui->mouse_x;
      ui->block_drag.mouse_y = ui->mouse_y;
      // Dead zone: require 6px movement before committing to drag
      if (!ui->block_drag.threshold_met) {
        float dx = ui->mouse_x - ui->block_drag.start_x;
        float dy = ui->mouse_y - ui->block_drag.start_y;
        if (dx * dx + dy * dy > 36.0f) { // 6px radius
          ui->block_drag.threshold_met = 1;
        }
      }
      if (ui->block_drag.threshold_met) {
        ui->block_drag.drop_valid = 0; // Reset, recalc in render
      }
      ui_request_redraw(ui);
    }

    // Check if mouse is in canvas
    CanvasInteraction *cs = &ui->canvas_state;
    cs->mouse_in_canvas = (ui->mouse_x >= cs->bounds_x &&
                           ui->mouse_x < cs->bounds_x + cs->bounds_w &&
                           ui->mouse_y >= cs->bounds_y &&
                           ui->mouse_y < cs->bounds_y + cs->bounds_h);

    // Handle canvas panning
    if (cs->is_panning) {
      float dx = ui->mouse_x - cs->pan_start_x;
      float dy = ui->mouse_y - cs->pan_start_y;
      cs->cam_x = cs->cam_start_x - dx / cs->zoom;
      cs->cam_y = cs->cam_start_y - dy / cs->zoom;
    }

    // Handle design panning (no_limits mode)
    DesignInteraction *ds = &ui->design_state;
    if (ds->is_panning) {
      float dx = ui->mouse_x - ds->pan_start_x;
      float dy = ui->mouse_y - ds->pan_start_y;
      float zm = ds->zoom > 0.01f ? ds->zoom : 1.0f;
      ds->cam_x = ds->cam_start_x - dx / zm;
      ds->cam_y = ds->cam_start_y - dy / zm;
    }

    // Handle node dragging
    if (cs->is_dragging_node && ui->selected_node) {
      float world_x = cs->cam_x + (ui->mouse_x - cs->bounds_x) / cs->zoom;
      float world_y = cs->cam_y + (ui->mouse_y - cs->bounds_y) / cs->zoom;
      ui->selected_node->x = world_x - cs->drag_offset_x;
      ui->selected_node->y = world_y - cs->drag_offset_y;
    }

    cs->create_end_y = cs->cam_y + (ui->mouse_y - cs->bounds_y) / cs->zoom;
  }


  // Track mouse buttons
  if (evt->type == SDL_MOUSEBUTTONDOWN) {
    CanvasInteraction *cs = &ui->canvas_state;

    // IMPORTANT: Update mouse position from click event (not just motion)
    ui->mouse_x = (float)evt->button.x;
    ui->mouse_y = (float)evt->button.y;

    // Update mouse_in_canvas check
    cs->mouse_in_canvas = (ui->mouse_x >= cs->bounds_x &&
                           ui->mouse_x < cs->bounds_x + cs->bounds_w &&
                           ui->mouse_y >= cs->bounds_y &&
                           ui->mouse_y < cs->bounds_y + cs->bounds_h);

    if (evt->button.button == SDL_BUTTON_LEFT) {
      ui->mouse_buttons |= 1;
      cs->mouse_just_pressed = 1;
      ui->design_state.mouse_just_pressed = 1; // Also track for design

      ASLAN_DEBUG("SDL click down: (%d, %d)", evt->button.x, evt->button.y);

      // Start panning with space + left click (canvas)
      if (cs->space_held && cs->mouse_in_canvas) {
        cs->is_panning = 1;
        cs->pan_start_x = ui->mouse_x;
        cs->pan_start_y = ui->mouse_y;
        cs->cam_start_x = cs->cam_x;
        cs->cam_start_y = cs->cam_y;
      }
      // Start panning with space + left click (design)
      DesignInteraction *ds_sp = &ui->design_state;
      if (ds_sp->space_held) {
        ds_sp->is_panning = 1;
        ds_sp->pan_start_x = ui->mouse_x;
        ds_sp->pan_start_y = ui->mouse_y;
        ds_sp->cam_start_x = ds_sp->cam_x;
        ds_sp->cam_start_y = ds_sp->cam_y;
      }
    } else if (evt->button.button == SDL_BUTTON_MIDDLE) {
      ui->mouse_buttons |= 2;
      // Start panning with middle mouse button
      if (cs->mouse_in_canvas) {
        cs->is_panning = 1;
        cs->pan_start_x = ui->mouse_x;
        cs->pan_start_y = ui->mouse_y;
        cs->cam_start_x = cs->cam_x;
        cs->cam_start_y = cs->cam_y;
      }
    } else if (evt->button.button == SDL_BUTTON_RIGHT) {
      ui->mouse_buttons |= 4;
    }
  }

  if (evt->type == SDL_MOUSEBUTTONUP) {
    CanvasInteraction *cs = &ui->canvas_state;

    // Update mouse position from release event
    ui->mouse_x = (float)evt->button.x;
    ui->mouse_y = (float)evt->button.y;

    if (evt->button.button == SDL_BUTTON_LEFT) {
      ui->mouse_buttons &= ~1;
      cs->mouse_just_released = 1;
      ui->design_state.mouse_just_released = 1; // Also track for design

      // Block drag-and-drop: execute drop on mouse release
      if (ui->block_drag.is_active) {
        if (ui->block_drag.threshold_met && ui->block_drag.drop_valid &&
            ui->engine) {
          Page *page = doc_page_get_active(ui->engine->doc);
          if (page) {
            Block *dragged =
                page_find_block_by_id(page, ui->block_drag.dragged_block_id);
            Block *target =
                page_find_block_by_id(page, ui->block_drag.drop_target_id);
            if (dragged && target && dragged != target) {
              int pos = ui->block_drag.drop_position;
              uint16_t target_row = target->layout_row;

              // Remember old row of dragged block for cleanup
              uint16_t old_row = dragged->layout_row;

              // Remove dragged from its current position
              page_remove_block(page, dragged);

              // Check if old row is now empty (no blocks left in that row)
              // If so, compact: decrement all rows above old_row
              int old_row_empty = 1;
              for (Block *r = page->first_block; r; r = r->next) {
                if (r->layout_row == old_row) {
                  old_row_empty = 0;
                  break;
                }
              }
              if (old_row_empty) {
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row > old_row)
                    r->layout_row--;
                }
                // Adjust target_row if it was affected
                if (target_row > old_row)
                  target_row--;
              }

              // Reassign layout_col in old row (may have shifted)
              if (!old_row_empty) {
                uint8_t ci = 0;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == old_row)
                    r->layout_col = ci++;
                }
              }

              if (pos == 0) {
                // Above: insert before first block in target row, own new row
                // Shift target row and everything after
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row >= target_row)
                    r->layout_row++;
                }
                // Find first block in target row (now target_row+1)
                Block *first_in_row = NULL;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == target_row + 1) {
                    first_in_row = r;
                    break;
                  }
                }
                if (first_in_row)
                  page_insert_block_before(page, first_in_row, dragged);
                else
                  page_insert_block_after(page, target, dragged);
                dragged->layout_row = target_row;
                dragged->layout_col = 0;

              } else if (pos == 1) {
                // Below: insert after last block in target row, own new row
                Block *last_in_row = NULL;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == target_row)
                    last_in_row = r;
                }
                uint16_t new_row = target_row + 1;
                // Shift everything after target row
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row >= new_row)
                    r->layout_row++;
                }
                if (last_in_row)
                  page_insert_block_after(page, last_in_row, dragged);
                else
                  page_insert_block_after(page, target, dragged);
                dragged->layout_row = new_row;
                dragged->layout_col = 0;

              } else if (pos == 2) {
                // Left: place in same row as target, before target
                page_insert_block_before(page, target, dragged);
                dragged->layout_row = target_row;
                // Reassign layout_col for all blocks in this row
                uint8_t ci = 0;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == target_row)
                    r->layout_col = ci++;
                }

              } else if (pos == 3) {
                // Right: place in same row as target, after target
                page_insert_block_after(page, target, dragged);
                dragged->layout_row = target_row;
                uint8_t ci = 0;
                for (Block *r = page->first_block; r; r = r->next) {
                  if (r->layout_row == target_row)
                    r->layout_col = ci++;
                }
              }
            }
          }
        }
        ui->block_drag.is_active = 0;
        ui->block_drag.threshold_met = 0;
        ui->block_drag.drop_valid = 0;
        ui_request_redraw(ui);
      }

      // Stop panning
      cs->is_panning = 0;

      // Stop dragging node
      cs->is_dragging_node = 0;

      // Stop resizing
      cs->is_resizing = 0;

      // Stop design dragging/resizing/panning
      ui->design_state.is_dragging_node = 0;
      ui->design_state.is_resizing = 0;
      ui->design_state.is_panning = 0;
    }

  } else if (evt->button.button == SDL_BUTTON_MIDDLE) {
    ui->mouse_buttons &= ~2;
    ui->canvas_state.is_panning = 0;
  } else if (evt->button.button == SDL_BUTTON_RIGHT) {
    ui->mouse_buttons &= ~4;
  }

  // Start of keyboard handling

  // Track spacebar for pan mode (canvas + design)
  if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_SPACE) {
    ui->canvas_state.space_held = 1;
    ui->design_state.space_held = 1;
    ASLAN_DEBUG("Spacebar DOWN");
  }
  if (evt->type == SDL_KEYUP && evt->key.keysym.sym == SDLK_SPACE) {
    // Only clear space_held if pan wasn't activated from menu
    if (!ui->canvas_state.pan_from_menu) {
      ui->canvas_state.space_held = 0;
    }
    ui->canvas_state.is_panning = 0;
    if (!ui->design_state.pan_from_menu) {
      ui->design_state.space_held = 0;
    }
    ui->design_state.is_panning = 0;
    ASLAN_DEBUG("Spacebar UP");
  }

  // Inline text editing keyboard handling (Freeform-style)
  if (ui->editing_node) {
    if (evt->type == SDL_KEYDOWN) {
      if (evt->key.keysym.sym == SDLK_RETURN ||
          evt->key.keysym.sym == SDLK_KP_ENTER) {
        strncpy(ui->editing_node->text, ui->edit_buffer,
                sizeof(ui->editing_node->text) - 1);
        ui->editing_node->text[sizeof(ui->editing_node->text) - 1] = '\0';
        ui->editing_node = NULL;
        SDL_StartTextInput(); // keep text input alive for widget edit
                              // widgets
      } else if (evt->key.keysym.sym == SDLK_ESCAPE) {
        ui->editing_node = NULL;
        SDL_StartTextInput(); // keep text input alive for widget edit
                              // widgets
      } else if (evt->key.keysym.sym == SDLK_BACKSPACE) {
        size_t len = strlen(ui->edit_buffer);
        if (len > 0)
          ui->edit_buffer[len - 1] = '\0';
      }
    } else if (evt->type == SDL_TEXTINPUT) {
      size_t len = strlen(ui->edit_buffer);
      size_t input_len = strlen(evt->text.text);
      if (len + input_len < sizeof(ui->edit_buffer) - 1)
        strcat(ui->edit_buffer, evt->text.text);
      // Feed NanoVG widgets
      nvgw_input_keyboard(&ui->nvgw, evt->text.text, false, false);
    }
    return; // Consume all keyboard events while editing text
  }

  // Always feed text input to NanoVG widgets if not editing node
  if (evt->type == SDL_TEXTINPUT) {
    nvgw_input_keyboard(&ui->nvgw, evt->text.text, false, false);
  }

  // --- Canvas/Design keyboard shortcuts (only when NOT editing text) ---
  if (evt->type == SDL_KEYDOWN) {
    SDL_Keymod mods = SDL_GetModState();
    int cmd_held = (mods & KMOD_GUI) || (mods & KMOD_CTRL);
    int shift_held = (mods & KMOD_SHIFT);
    SDL_Keycode key = evt->key.keysym.sym;

    // Feed NanoVG widgets
    if (key == SDLK_BACKSPACE)
      nvgw_input_keyboard(&ui->nvgw, NULL, true, false);
    else if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
      nvgw_input_keyboard(&ui->nvgw, NULL, false, true);
    else if (key == SDLK_TAB) {
      if (shift_held) ui->nvgw.key_tab_back = true;
      else            ui->nvgw.key_tab      = true;
    } else if (key == SDLK_ESCAPE)
      ui->nvgw.key_escape = true;

    // Clipboard paste into focused text input (Cmd+V / Ctrl+V)
    if (cmd_held && key == SDLK_v && ui->nvgw.focus_id != 0) {
      char *clip = SDL_GetClipboardText();
      if (clip && clip[0] != '\0') {
        // Determine which buffer the focused input owns and append directly
        if (ui->table_state.is_editing_cell
            && ui->table_state.active_block_id != 0) {
          size_t len = strlen(ui->table_state.rename_buffer);
          strncat(ui->table_state.rename_buffer, clip,
                  sizeof(ui->table_state.rename_buffer) - len - 1);
        } else if (ui->table_state.show_formula_popup) {
          size_t len = strlen(ui->table_state.formula_buffer);
          strncat(ui->table_state.formula_buffer, clip,
                  sizeof(ui->table_state.formula_buffer) - len - 1);
        } else if (ui->table_state.renaming_col_idx >= 0) {
          size_t len = strlen(ui->table_state.rename_buffer);
          strncat(ui->table_state.rename_buffer, clip,
                  sizeof(ui->table_state.rename_buffer) - len - 1);
        } else if (ui->table_state.show_select_popup) {
          size_t len = strlen(ui->table_state.option_buffer);
          strncat(ui->table_state.option_buffer, clip,
                  sizeof(ui->table_state.option_buffer) - len - 1);
        } else {
          // Generic fallback: feed into nvgw text_input (truncates to 31 chars)
          nvgw_input_keyboard(&ui->nvgw, clip, false, false);
        }
        ui_request_redraw(ui);
      }
      if (clip) SDL_free(clip);
      return; // Paste was handled by focused input — don't fall through to table paste
    }

    // Determine active block (canvas or design)
    Block *active_block = NULL;
    int is_canvas = 0, is_design = 0;
    if (ui->canvas_state.active_block &&
        ui->canvas_state.active_canvas_id != 0) {
      active_block = ui->canvas_state.active_block;
      is_canvas = 1;
    } else if (ui->design_state.active_block &&
               ui->design_state.active_design_id != 0) {
      active_block = ui->design_state.active_block;
      is_design = 1;
    }

    // --- Escape: close table popups / cancel inline edit first ---
    if (key == SDLK_ESCAPE) {
      if (ui->table_state.show_formula_popup) {
        ui->table_state.show_formula_popup = 0;
        ui->nvgw.focus_id = 0;
        ui->nvgw.active_id = 0;
        ui_request_redraw(ui);
        return;
      }
      if (ui->table_state.show_select_popup || ui->table_state.show_date_picker) {
        ui->table_state.show_select_popup = 0;
        ui->table_state.show_date_picker = 0;
        ui->table_state.is_editing_cell = 0;
        ui->nvgw.focus_id = 0;
        ui_request_redraw(ui);
        return;
      }
      if (ui->table_state.is_editing_cell || ui->table_state.renaming_col_idx >= 0) {
        // key_escape is already set above; render_rows will cancel the edit.
        // Also cancel column rename directly here.
        ui->table_state.renaming_col_idx = -1;
        ui_request_redraw(ui);
        // Don't return — let key_escape propagate to render_rows for cell edit.
      }
      if (ui->table_state.show_col_menu || ui->table_state.show_col_settings ||
          ui->table_state.show_footer_menu ||
          ui->table_state.show_formula_help) {
        ui->table_state.show_col_menu = 0;
        ui->table_state.show_col_settings = 0;
        ui->table_state.show_footer_menu = 0;
        ui->table_state.show_formula_help = 0;
        return;
      }
      if (ui->selected_node) {
        ui->selected_node = NULL;
      }
      if (is_canvas) {
        ui->canvas_state.bezier_mode = 0;
        ui->canvas_state.tool = CANVAS_TOOL_SELECT;
        ui->canvas_state.active_cat = CANVAS_CAT_SELECT;
        ui->canvas_state.show_sub_menu = 0;
      }
      if (is_design) {
        ui->design_state.bezier_mode = 0;
        ui->design_state.tool = CANVAS_TOOL_SELECT;
        ui->design_state.active_cat = CANVAS_CAT_SELECT;
        ui->design_state.show_sub_menu = 0;
      }
      return;
    }

    // --- Delete / Backspace: delete selected node(s) ---
    if ((key == SDLK_DELETE || key == SDLK_BACKSPACE) && active_block &&
        is_design) {
      // Delete all multi-selected nodes first
      for (int _di = 0; _di < ui->selected_count; _di++) {
        Node *n = ui->selected_nodes[_di];
        if (n && n != ui->selected_node)
          block_design_delete_node(active_block, n->id);
      }
      ui->selected_count = 0;
      // Delete primary selected node
      if (ui->selected_node) {
        block_design_delete_node(active_block, ui->selected_node->id);
        ui->selected_node = NULL;
      }
      return;
    }

    // --- Cmd+D: Duplicate selected node(s) ---
    if (cmd_held && key == SDLK_d && active_block && is_design) {
      if (ui->selected_count > 0) {
        Node *last_dup = NULL;
        for (int _di = 0; _di < ui->selected_count; _di++) {
          Node *n = ui->selected_nodes[_di];
          if (n)
            last_dup = block_design_duplicate_node(active_block, n->id);
        }
        if (last_dup)
          ui->selected_node = last_dup;
        ui->selected_count = 0;
      } else if (ui->selected_node) {
        Node *dup =
            block_design_duplicate_node(active_block, ui->selected_node->id);
        if (dup)
          ui->selected_node = dup;
      }
      return;
    }

    // --- Cmd+Shift+G: Ungroup selected GROUP node ---
    if (cmd_held && shift_held && key == SDLK_g && ui->selected_node &&
        ui->selected_node->type == NODE_TYPE_GROUP && active_block &&
        is_design) {
      Node *grp = ui->selected_node;
      Node *child = grp->first_child;
      while (child) {
        Node *next = child->next_sibling;
        node_remove_from_parent(child);
        child->x += grp->x; // restore world position
        child->y += grp->y;
        node_insert_before(child, grp);
        child = next;
      }
      block_design_delete_node(active_block, grp->id);
      ui->selected_node = NULL;
      ui->selected_count = 0;
      return;
    }

    // --- Cmd+G: Group selected node(s) into a GROUP container ---
    if (cmd_held && !shift_held && key == SDLK_g && active_block && is_design &&
        (ui->selected_node || ui->selected_count > 0)) {
      // Build node list
      Node *nodes[65];
      int node_count = 0;
      if (ui->selected_count > 1) {
        for (int _gi = 0; _gi < ui->selected_count; _gi++)
          nodes[node_count++] = ui->selected_nodes[_gi];
      } else if (ui->selected_node) {
        nodes[node_count++] = ui->selected_node;
      }
      if (node_count == 0)
        return;

      // Compute bounding box
      float min_x = nodes[0]->x, min_y = nodes[0]->y;
      float max_x = nodes[0]->x + nodes[0]->width;
      float max_y = nodes[0]->y + nodes[0]->height;
      for (int _gi = 1; _gi < node_count; _gi++) {
        Node *n = nodes[_gi];
        if (n->x < min_x)
          min_x = n->x;
        if (n->y < min_y)
          min_y = n->y;
        if (n->x + n->width > max_x)
          max_x = n->x + n->width;
        if (n->y + n->height > max_y)
          max_y = n->y + n->height;
      }

      Node *group =
          block_design_create_node(active_block, NODE_TYPE_GROUP, "Group");
      if (group) {
        group->x = min_x;
        group->y = min_y;
        group->width = max_x - min_x;
        group->height = max_y - min_y;
        group->visible = 1;
        // Place group where the first node was
        node_insert_before(group, nodes[0]);
        // Reparent all nodes into group, adjusting their local coords
        for (int _gi = 0; _gi < node_count; _gi++) {
          Node *n = nodes[_gi];
          node_remove_from_parent(n);
          n->x -= min_x;
          n->y -= min_y;
          node_add_child(group, n);
        }
        ui->selected_node = group;
        ui->selected_count = 0;
      }
      return;
    }

    // --- Arrow keys: nudge selected node ---
    if ((key == SDLK_UP || key == SDLK_DOWN || key == SDLK_LEFT ||
         key == SDLK_RIGHT) &&
        ui->selected_node) {
      float step = shift_held ? 10.0f : 1.0f;
      if (key == SDLK_UP)
        ui->selected_node->y -= step;
      else if (key == SDLK_DOWN)
        ui->selected_node->y += step;
      else if (key == SDLK_LEFT)
        ui->selected_node->x -= step;
      else if (key == SDLK_RIGHT)
        ui->selected_node->x += step;

      // Clamp for design (finite artboard)
      if (is_design && active_block) {
        DesignData *dd = &active_block->data.design;
        if (ui->selected_node->x < 0)
          ui->selected_node->x = 0;
        if (ui->selected_node->y < 0)
          ui->selected_node->y = 0;
        if (ui->selected_node->x + ui->selected_node->width > dd->width)
          ui->selected_node->x = dd->width - ui->selected_node->width;
      }
      return;
    }

    // --- Table Copy/Paste logic (Cmd+C / Cmd+V) ---
    if (cmd_held && (key == SDLK_c || key == SDLK_v) &&
        ui->table_state.active_block_id != 0) {
      // Find the active table block
      Block *tbl_block = NULL;
      if (active_block && active_block->id == ui->table_state.active_block_id) {
        tbl_block = active_block;
      }
      // If we couldn't find the table block directly, check the active
      // ones again
      if (!tbl_block) {
        Page *active_page = doc_page_get_active(ui->engine->doc);
        if (active_page) {
          Block *curr = active_page->first_block;
          while (curr) {
            if (curr->id == ui->table_state.active_block_id) {
              tbl_block = curr;
              break;
            }
            curr = curr->next;
          }
        }
      }

      if (tbl_block && tbl_block->type == BLOCK_TABLE &&
          tbl_block->data.table.columnar) {
        AslanTable *tbl = tbl_block->data.table.columnar;

        int r1 = ui->table_state.sel_start_row_idx;
        int r2 = ui->table_state.sel_end_row_idx;
        int min_r = r1 < r2 ? r1 : r2;
        int max_r = r1 > r2 ? r1 : r2;

        int c1 = ui->table_state.sel_start_col_idx;
        int c2 = ui->table_state.sel_end_col_idx;
        int min_c = c1 < c2 ? c1 : c2;
        int max_c = c1 > c2 ? c1 : c2;

        if (key == SDLK_c) {
          // --- COPY ---
          // Serialize selection region to TSV
          size_t buf_cap = 4096;
          char *tsv_buf = (char *)malloc(buf_cap);
          if (tsv_buf) {
            tsv_buf[0] = '\0';
            size_t buf_len = 0;
            for (int r = min_r; r <= max_r; r++) {
              uint64_t row_id = aslan_table_row_id_at(tbl, r);
              for (int c = min_c; c <= max_c; c++) {
                char cell_str[512] = {0};
                aslan_table_get_cell_display(tbl, c, row_id, cell_str,
                                             sizeof(cell_str));

                size_t slen = strlen(cell_str);
                if (buf_len + slen + 2 > buf_cap) {
                  buf_cap *= 2;
                  tsv_buf = (char *)realloc(tsv_buf, buf_cap);
                }
                strcat(tsv_buf, cell_str);
                buf_len += slen;
                if (c < max_c) {
                  strcat(tsv_buf, "\t");
                  buf_len++;
                }
              }
              if (r < max_r) {
                strcat(tsv_buf, "\n");
                buf_len++;
              }
            }
            SDL_SetClipboardText(tsv_buf);
            free(tsv_buf);
            ASLAN_DEBUG("Copied table cells to clipboard (%d rows, %d cols)",
                        (max_r - min_r + 1), (max_c - min_c + 1));
          }
        } else if (key == SDLK_v) {
          // --- PASTE ---
          if (SDL_HasClipboardText()) {
            char *text = SDL_GetClipboardText();
            if (text) {
              // Paste starts at the active/selected cell (min_c, min_r)
              int start_r =
                  (min_r >= 0) ? min_r : ui->table_state.active_row_idx;
              int start_c =
                  (min_c >= 0) ? min_c : ui->table_state.active_col_idx;

              if (start_r >= 0 && start_c >= 0) {
                int cur_r = start_r;
                char *line_saveptr = NULL;
                char *cell_saveptr = NULL;

                // strtok modifies original string, so copy it
                char *text_copy = strdup(text);
                char *line = strtok_r(text_copy, "\n\r", &line_saveptr);

                while (line) {
                  uint64_t row_id = aslan_table_row_id_at(tbl, cur_r);
                  // If row doesn't exist, we could add new rows, but for
                  // now just drop
                  if (row_id == 0) {
                    // Note: We could use aslan_table_add_row(tbl)
                    // dynamically here if we want to expand the table on
                    // paste
                    break;
                  }

                  int cur_c = start_c;
                  char *cell_val = strtok_r(line, "\t", &cell_saveptr);
                  while (cell_val) {
                    if (cur_c < (int)tbl->column_count) {
                      // Some types might not like raw strings, but our
                      // cell setter handles most
                      aslan_table_set_cell(tbl, cur_c, row_id, cell_val);
                    }
                    cur_c++;
                    cell_val = strtok_r(NULL, "\t", &cell_saveptr);
                  }

                  cur_r++;
                  line = strtok_r(NULL, "\n\r", &line_saveptr);
                }
                free(text_copy);
                aslan_table_eval_formulas(tbl);
                ui_request_redraw(ui);
                ASLAN_DEBUG("Pasted text into table cells");
              }
              SDL_free(text);
            }
          }
        }
      }
      return; // Handled
    }

    // --- Cmd+D: duplicate selected node ---
    if (cmd_held && key == SDLK_d && ui->selected_node && active_block) {
      Node *src = ui->selected_node;
      Node *dup = NULL;

      if (is_design) {
        dup = node_create(active_block->data.design.next_node_id++, src->type,
                          src->name);
      }

      if (dup) {
        // Copy visual properties
        dup->x = src->x + 20.0f;
        dup->y = src->y + 20.0f;
        dup->width = src->width;
        dup->height = src->height;
        dup->rotation = src->rotation;
        dup->fill_color = src->fill_color;
        dup->stroke_color = src->stroke_color;
        dup->stroke_width = src->stroke_width;
        dup->corner_radius = src->corner_radius;
        dup->font_size = src->font_size;
        dup->font_type = src->font_type;
        dup->line_start_style = src->line_start_style;
        dup->line_end_style = src->line_end_style;
        dup->visible = 1;
        dup->is_container = src->is_container;
        strncpy(dup->text, src->text, sizeof(dup->text) - 1);
        strncpy(dup->image_path, src->image_path, sizeof(dup->image_path) - 1);

        // Add to tree
        Node *root = NULL;
        if (is_design)
          root = active_block->data.design.root;

        if (root) {
          dup->parent = root;
          node_add_child(root, dup);
          ui->selected_node = dup;
        }
      }
      return;
    }
  }

  // Handle canvas zoom or page scroll with mouse wheel
  if (evt->type == SDL_MOUSEWHEEL) {
    CanvasInteraction *cs = &ui->canvas_state;

    // Initialize zoom if not set
    if (cs->zoom < 0.01f)
      cs->zoom = 1.0f;

    // Check for modifier keys
    SDL_Keymod mods = SDL_GetModState();
    bool cmd_or_ctrl = (mods & KMOD_GUI) || (mods & KMOD_CTRL);

    if (cs->mouse_in_canvas && cmd_or_ctrl) {
      // Zoom Canvas only when Cmd/Ctrl is held
      float old_zoom = cs->zoom;
      float zoom_factor = 1.1f;
      if (evt->wheel.y > 0)
        cs->zoom *= zoom_factor;
      else if (evt->wheel.y < 0)
        cs->zoom /= zoom_factor;

      if (cs->zoom < 0.1f)
        cs->zoom = 0.1f;
      if (cs->zoom > 5.0f)
        cs->zoom = 5.0f;

      float mouse_rel_x = ui->mouse_x - cs->bounds_x;
      float mouse_rel_y = ui->mouse_y - cs->bounds_y;
      float world_x = cs->cam_x + mouse_rel_x / old_zoom;
      float world_y = cs->cam_y + mouse_rel_y / old_zoom;
      cs->cam_x = world_x - mouse_rel_x / cs->zoom;
      cs->cam_y = world_y - mouse_rel_y / cs->zoom;
      ASLAN_DEBUG("Canvas Zoom: %.2f (wheel.y=%d)", cs->zoom, evt->wheel.y);
    } else if (!ui->scroll_inhibited) {
      // Global Page Scroll (Default behavior)
      float scroll_amount = 60.0f;
      ui->scroll_target_y -= evt->wheel.y * scroll_amount;
      if (ui->scroll_target_y < 0)
        ui->scroll_target_y = 0;
      ASLAN_DEBUG("Page Scroll: %.1f (wheel.y=%d)", ui->scroll_target_y,
                  evt->wheel.y);
    }
    // Flag is reset at start of frame to allow tools to inhibit next scroll event
  }
}

// -----------------------------------------------------------------------------
// Top Bar - Block Insertion Menu
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Block Renderers
// -----------------------------------------------------------------------------

// Overlay toolbar: draws compact icons at (x_right, y_top) without
// consuming layout space. Elegant floating pill with subtle border.
/*
  if (_btn_hovered && mouse_down && !ui->block_drag.is_active) {
    ui->block_drag.is_active = 1;
    ui->block_drag.dragged_block_id = block->id;
    ui->block_drag.start_x = mx;
    ui->block_drag.start_y = my;
    ui->block_drag.mouse_x = mx;
    ui->block_drag.mouse_y = my;
    ui->block_drag.threshold_met = 0;
    ui->block_drag.drop_valid = 0;
  }

  // 2. Search icon (TABLE only)
  if (block->type == BLOCK_TABLE) {
    OVERLAY_ICON_BTN(ICON_SEARCH);
    if (_btn_clicked) {
      block->data.table.show_search = !block->data.table.show_search;
      ui->table_state.active_block_id = block->id;
    }
  }

  // 3. Export icon
  if (allow_export) {
    OVERLAY_ICON_BTN(ICON_EXPORT);
    if (_btn_clicked) {
      if (block->type == BLOCK_TABLE) {
        char filename[1024];
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        char clean_name[64];
        if (block->data.table.columnar &&
            block->data.table.columnar->name[0] != '\0') {
          strncpy(clean_name, block->data.table.columnar->name,
                  sizeof(clean_name) - 1);
          clean_name[sizeof(clean_name) - 1] = '\0';
        } else {
          strcpy(clean_name, "Table");
        }

        for (char *p = clean_name; *p; p++) {
          if (*p == ' ' || *p == '/' || *p == '\\')
            *p = '_';
        }

        char default_name[256];
        snprintf(default_name, sizeof(default_name), "%s_%04d%02d%02d.csv",
                 clean_name, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);

        if (save_csv_file_dialog(filename, sizeof(filename), default_name)) {
          aslan_table_export_csv(block->data.table.columnar, filename);
          ASLAN_DEBUG("Exported CSV to: %s", filename);
        } else {
          ASLAN_DEBUG("Export cancelled by user");
        }
      }
    }
  }

  // 4. Resize buttons (shrink / grow)
  if (allow_resize) {
    bool shrink_clicked = nvgw_button(&ui->nvgw, nvgw_id_str("shrink"), ix, iy,
                                      btn_w, btn_h, "-");
    ix += btn_w + gap;
    if (shrink_clicked) {
      float vs = block->view_scale;
      if (vs < 0.1f)
        vs = 1.0f;
      vs -= 0.15f;
      if (vs < 0.3f)
        vs = 0.3f;
      block->view_scale = vs;
    }

    bool grow_clicked =
        nvgw_button(&ui->nvgw, nvgw_id_str("grow"), ix, iy, btn_w, btn_h, "+");
    ix += btn_w + gap;
    if (grow_clicked) {
      float vs = block->view_scale;
      if (vs < 0.1f)
        vs = 1.0f;
      vs += 0.15f;
      if (vs > 3.0f)
        vs = 3.0f;
      block->view_scale = vs;
    }
  }

  // 5. Fullscreen icon
  if (allow_fullscreen) {
    if (is_expanded) {
      OVERLAY_ICON_BTN(ICON_FULLSCREEN_EXIT);
      if (_btn_clicked)
        ui->expanded_block_id = 0;
    } else {
      OVERLAY_ICON_BTN(ICON_FULLSCREEN);
      if (_btn_clicked)
        ui->expanded_block_id = block->id;
    }
  }

  // 6. Trash icon
  if (show_trash) {
    OVERLAY_ICON_BTN(ICON_DELETE);
    if (_btn_clicked) {
      char cmd[128];
      snprintf(cmd, sizeof(cmd), "block_delete %u %u", page->id, block->id);
      engine_execute_command(engine, cmd);
    }
  }

#undef OVERLAY_ICON_BTN
}
*/

void ui_render_block_text(UIHost *ui, Engine *engine,
                          Page *page, Block *block, float width) {
  (void)ui; (void)engine; (void)page; (void)block; (void)width;
  // Native rendering is handled by Clay and NanoVG.
}

void ui_render_block_heading(UIHost *ui, Block *block, float width, int level) {
  (void)ui; (void)block; (void)width; (void)level;
  // Native rendering is handled by Clay and NanoVG.
}

// -----------------------------------------------------------------------------
// Block Dispatcher
// -----------------------------------------------------------------------------

void ui_render_block(UIHost *ui, Engine *engine,
                     Page *page, Block *block, float width) {
  if (!block)
    return;

  ASLAN_DEBUG("Rendering block %u (Type: %d)", block->id, block->type);
  switch (block->type) {
  case BLOCK_TEXT:
    ui_render_block_text(ui, engine, page, block, width);
    break;
  case BLOCK_HEADING_1:
    ui_render_block_heading(ui, block, width, 1);
    // Sync page name with first H1 block
    if (page && block == page->first_block && block->text && block->text[0]) {
      if (strcmp(page->name, block->text) != 0) {
        strncpy(page->name, block->text, sizeof(page->name) - 1);
        page->name[sizeof(page->name) - 1] = '\0';
      }
    }
    break;
  case BLOCK_TABLE:
    ui_render_block_table(ui, engine, block, width, block->height > 0 ? block->height : 400.0f);
    break;
  case BLOCK_DESIGN:
    ui_render_block_design(ui, engine, block, width, block->height > 0 ? block->height : 600.0f);
    break;
  default:
    break;
  }
}



// -----------------------------------------------------------------------------
// Block Height Measurement (Adaptive Layout)
// -----------------------------------------------------------------------------

float ui_measure_block_height(UIHost *ui, Block *block, float width) {
  (void)ui;
  if (!block)
    return 100.0f;

  // Get block scale factor (default 1.0)
  float vs = block->view_scale;
  if (vs < 0.1f)
    vs = 1.0f;

  switch (block->type) {
  case BLOCK_TEXT: {
    int lines = 1;
    if (block->text) {
      for (const char *p = block->text; *p; p++)
        if (*p == '\n')
          lines++;
    }
    // Line height 22 + padding (tightened from +4 to +2)
    return (float)lines * 22.0f + 2.0f;
  }
  case BLOCK_HEADING_1:
    // H1 renders at 42px + group padding (reduced from 46)
    return 44.0f;

  case BLOCK_TABLE: {
    // Header 34 + Footer 30 + Padding 10 = 74
    float h = 74.0f;

    // Add row heights
    if (block->data.table.columnar) {
      // Use visible_rows if set, otherwise active rows in table
      int rows = block->data.table.visible_rows;
      if (rows <= 0) {
        rows = (int)aslan_table_view_count(block->data.table.columnar);
      }
      // Cap max auto-height to avoid massive tables
      if (rows > 15)
        rows = 15;
      if (rows < 3)
        rows = 3; // Min 3 rows visual

      h += (float)rows * 30.0f; // 30px per row
    } else {
      h += 3 * 30.0f; // Default
    }

    if (block->data.table.show_search) {
      h += 30.0f;
    }

    return h * vs;
  }

  case BLOCK_DESIGN: {
    // Design renders at aspect ratio, max 400.0f + 52.0f toolbar
    float aspect_h = 400.0f;
    if (block->data.design.width > 0 && width > 0) {
      float scale = width / (float)block->data.design.width;
      aspect_h = (float)block->data.design.height * scale;
      if (aspect_h > 400.0f)
        aspect_h = 400.0f;
    }
    return (aspect_h + 52.0f + 250.0f) * vs;
  }

  default:
    return 100.0f;
  }
}

// =============================================================================
// SPLIT VIEW — Data-Driven Layout
// =============================================================================

// Find first block of given type in page
static Block *page_find_first_block_of_type(Page *page, BlockType type) {
  for (Block *b = page->first_block; b; b = b->next) {
    if (b->type == type)
      return b;
  }
  return NULL;
}

// =============================================================================
// Gantt View
// =============================================================================

/// Render a single panel view: 0=Table (left), 1=Design (right)
static void ui_render_panel_view(UIHost *ui, Engine *engine,
                                 Page *page, int view,
                                 float x, float y,
                                 float view_w, float view_h) {
  if (!page) return;

  ui->abs_offset_x = x;
  ui->abs_offset_y = y;

  nvgSave(ui->vg);
  nvgTranslate(ui->vg, x, y);

  if (view == 0) {
    // Left panel: Table view
    Block *table = page_find_first_block_of_type(page, BLOCK_TABLE);
    if (table) {
      ui_render_block_table(ui, engine, table, view_w, view_h);
    }
  } else if (view == 1) {
    // Right panel: Design view
    Block *design = page_find_first_block_of_type(page, BLOCK_DESIGN);
    if (design) {
      ui_render_block_design(ui, engine, design, view_w, view_h);
    }
  }

  nvgRestore(ui->vg);
}

// =============================================================================
// Split-view: Clay owns backgrounds, NanoVG owns content
// =============================================================================

// Process splitter mouse interaction handled by NanoVG/Clay now.
static void handle_splitter_input(UIHost *ui, int win_w,
                                  int win_h, float left_w, float splitter_w) {
  (void)win_h;
  float mx = ui->mouse_x;
  float my = ui->mouse_y;
  int mdown = (ui->mouse_buttons & 1);

  // Splitter rect
  float sx = left_w;
  float sy = 0;
  float sw = splitter_w;
  float sh = (float)win_h;

  int hovered = (mx >= sx && mx < sx + sw && my >= sy && my < sy + sh);

  if (hovered || ui->split_view.is_dragging_splitter) {
    ui->cursor_type = UI_CURSOR_SIZEWE;
  }

  if (hovered && ui->nvgw.mouse_clicked) {
    ui->split_view.is_dragging_splitter = 1;
  }

  if (ui->split_view.is_dragging_splitter) {
    if (!mdown) {
      ui->split_view.is_dragging_splitter = 0;
    } else {
      float new_ratio = mx / (float)win_w;
      if (new_ratio < 0.05f) new_ratio = 0.05f;
      if (new_ratio > 0.95f) new_ratio = 0.95f;
      ui->split_view.ratio = new_ratio;
      ui_request_redraw(ui);
    }
  }
}

static void ui_render_split_view(UIHost *ui, Engine *engine,
                                 int win_w, int win_h) {
  Page *page = doc_page_get_active(engine->doc);
  if (!page)
    return;

  float ratio = ui->split_view.ratio;
  if (ratio < 0.03f) ratio = 0.03f;
  if (ratio > 0.97f) ratio = 0.97f;

  const float spl_w = 6.0f;
  float left_w = (float)win_w * ratio;
  float right_w = (float)win_w - left_w - spl_w;
  if (left_w < 30.0f) left_w = 30.0f;
  if (right_w < 30.0f) right_w = 30.0f;

  handle_splitter_input(ui, win_w, win_h, left_w, spl_w);

  if (ui->clay) {
    clay_host_render_split(ui->clay, ui->dark_mode != 0, ratio, 0,
                           ui->split_view.is_dragging_splitter != 0, win_w,
                           win_h);
  }

  // Left Panel
  float view_x_left = 6.0f;
  float view_y_left = 5.0f;
  float view_w_left = left_w - 12.0f;
  float view_h_left = (float)win_h - 10.0f;
  if (view_w_left > 0 && view_h_left > 0) {
    ui_render_panel_view(ui, engine, page, 0, view_x_left, view_y_left,
                         view_w_left, view_h_left);
  }

  // Right Panel
  float view_x_right = left_w + spl_w + 6.0f;
  float view_y_right = 5.0f;
  float view_w_right = right_w - 12.0f;
  float view_h_right = (float)win_h - 10.0f;
  if (view_w_right > 0 && view_h_right > 0) {
    ui_render_panel_view(ui, engine, page, 1, view_x_right, view_y_right,
                         view_w_right, view_h_right);
  }

  // Fade overlay: panel fades to white when width < 15% of window
  NVGcontext *vg = ui->vg;
  NVGcolor bg = ui->dark_mode
    ? nvgRGBA(28, 28, 30, 255)
    : nvgRGBA(255, 255, 255, 255);

  // Left panel fade
  {
    float frac = left_w / (float)win_w;
    if (frac < 0.15f) {
      float t = 1.0f - (frac - 0.10f) / 0.05f;
      if (t < 0.0f) t = 0.0f;
      if (t > 1.0f) t = 1.0f;
      NVGcolor col = bg;
      col.a = t;
      nvgBeginPath(vg);
      nvgRect(vg, 0, 0, left_w, (float)win_h);
      nvgFillColor(vg, col);
      nvgFill(vg);
    }
  }

  // Right panel fade
  {
    float frac = right_w / (float)win_w;
    if (frac < 0.15f) {
      float t = 1.0f - (frac - 0.10f) / 0.05f;
      if (t < 0.0f) t = 0.0f;
      if (t > 1.0f) t = 1.0f;
      NVGcolor col = bg;
      col.a = t;
      nvgBeginPath(vg);
      nvgRect(vg, left_w + spl_w, 0, right_w + spl_w, (float)win_h);
      nvgFillColor(vg, col);
      nvgFill(vg);
    }
  }
}

void ui_begin_frame(UIHost *ui) {
  if (!ui) return;
  ui->scroll_inhibited = 0;
  nvgw_input_begin(&ui->nvgw);
  if (ui->clay) {
    clay_host_begin_frame(ui->clay);
  }
}

void ui_render(UIHost *ui, Engine *engine, int win_w, int win_h) {
  // Update window dims for smart placement
  ui->win_width = win_w;
  ui->win_height = win_h;
  if (!ui || !engine)
    return;

  ui->abs_offset_x = 0;
  ui->abs_offset_y = 0;

  // Begin NanoVG frame
  nvgBeginFrame(ui->vg, (float)win_w, (float)win_h, 1.0f);

  if (ui->pending_export) {
    ui->pending_export = 0;
    ASLAN_WARN("Export not available in GL mode (SDL renderer is missing)");
  }

  // Store engine reference for file drop handling
  ui->engine = engine;



  // Initialize cursor for this frame (will be committed at the end)
  ui->cursor_type = UI_CURSOR_ARROW;

  // Apply current theme


  // Main content area - always Editor
  if (ui->expanded_block_id != 0) {
    // Expanded block view
    Page *page = doc_page_get_active(engine->doc);
    if (page) {
      Block *block = page_find_block_by_id(page, ui->expanded_block_id);
      if (block) {
        ui_render_block(ui, engine, page, block, (float)win_w);
      }
      if (ui->show_doc_menu) {
        ui_render_document_menu(ui);
      }
    }
  } else {
    // Split view mode (Data-Driven) — default and only editor mode
    // Auto-activate split view with default ratio if needed
    if (ui->split_view.ratio < 0.01f)
      ui->split_view.ratio = 0.1f;
    ui->split_view.active = 1;

    // Clay renders backgrounds; Widgets create two standalone panel
    // windows. No SplitMain wrapper window needed — Clay handles the
    // Render split view if active. If in full design mode, this consumes
    // the outer frame.
    ui_render_split_view(ui, engine, win_w, win_h);

    // --- Floating HSV color picker ---
    if (ui->color_picker_open && ui->color_picker_ptr) {
      // Ensure nvgw offset is at screen root so hit-tests are absolute
      nvgw_set_offset(&ui->nvgw, 0.0f, 0.0f);

      const float pw = 220.0f, ph = 270.0f;
      float px = ui->color_picker_x;
      float py = ui->color_picker_y;
      if (px + pw > (float)win_w) px = (float)win_w - pw - 4.0f;
      if (py + ph > (float)win_h) py = (float)win_h - ph - 4.0f;
      if (px < 0) px = 0.0f;
      if (py < 0) py = 0.0f;

      NVGcontext *vg = ui->vg;
      bool dark = ui->dark_mode != 0;

      // Shadow
      nvgBeginPath(vg);
      nvgRoundedRect(vg, px+2, py+4, pw, ph, 8.0f);
      nvgFillColor(vg, nvgRGBA(0,0,0,30));
      nvgFill(vg);

      // Panel background
      nvgBeginPath(vg);
      nvgRoundedRect(vg, px, py, pw, ph, 8.0f);
      nvgFillColor(vg, dark ? nvgRGBA(38,40,46,255) : nvgRGBA(255,255,255,255));
      nvgFill(vg);
      nvgBeginPath(vg);
      nvgRoundedRect(vg, px, py, pw, ph, 8.0f);
      nvgStrokeColor(vg, dark ? nvgRGBA(60,62,72,255) : nvgRGBA(220,220,225,255));
      nvgStrokeWidth(vg, 1.0f);
      nvgStroke(vg);

      float h = ui->color_picker_h;
      float s = ui->color_picker_s;
      float v = ui->color_picker_v;
      float a = ui->color_picker_a;
      uint32_t *cp = ui->color_picker_ptr;

      // Layout
      float sx = px + 10.0f, sy = py + 10.0f;
      float sw = pw - 20.0f, sq_h = 140.0f;
      float bar_h = 14.0f, bar_gap = 8.0f;

      // ── SV Square ────────────────────────────────────────────────
      // Hue base color (full saturation & value)
      NVGcolor hue_c = nvgHSLA(h, 1.0f, 0.5f, 255);
      // White → hue (saturation axis)
      NVGpaint sp = nvgLinearGradient(vg, sx, sy, sx+sw, sy,
                      nvgRGBA(255,255,255,255), hue_c);
      nvgBeginPath(vg); nvgRect(vg, sx, sy, sw, sq_h);
      nvgFillPaint(vg, sp); nvgFill(vg);
      // Transparent → black (value axis)
      NVGpaint vp = nvgLinearGradient(vg, sx, sy, sx, sy+sq_h,
                      nvgRGBA(0,0,0,0), nvgRGBA(0,0,0,255));
      nvgBeginPath(vg); nvgRect(vg, sx, sy, sw, sq_h);
      nvgFillPaint(vg, vp); nvgFill(vg);
      // Border
      nvgBeginPath(vg); nvgRect(vg, sx, sy, sw, sq_h);
      nvgStrokeColor(vg, nvgRGBA(0,0,0,40)); nvgStrokeWidth(vg,1); nvgStroke(vg);
      // Crosshair
      float cx_pos = sx + s * sw;
      float cy_pos = sy + (1.0f - v) * sq_h;
      nvgBeginPath(vg); nvgCircle(vg, cx_pos, cy_pos, 5.0f);
      nvgStrokeColor(vg, nvgRGBA(255,255,255,220)); nvgStrokeWidth(vg,2); nvgStroke(vg);
      nvgBeginPath(vg); nvgCircle(vg, cx_pos, cy_pos, 5.0f);
      nvgStrokeColor(vg, nvgRGBA(0,0,0,100)); nvgStrokeWidth(vg,1); nvgStroke(vg);

      // SV drag
      bool in_sv = (ui->nvgw.mouse_x >= sx && ui->nvgw.mouse_x < sx+sw &&
                    ui->nvgw.mouse_y >= sy && ui->nvgw.mouse_y < sy+sq_h);
      if (in_sv && ui->nvgw.mouse_clicked) ui->color_picker_drag = 1;
      if (ui->color_picker_drag == 1) {
        if (ui->nvgw.mouse_down) {
          s = (ui->nvgw.mouse_x - sx) / sw;
          v = 1.0f - (ui->nvgw.mouse_y - sy) / sq_h;
          if (s < 0) s = 0; if (s > 1) s = 1;
          if (v < 0) v = 0; if (v > 1) v = 1;
        } else { ui->color_picker_drag = 0; }
      }

      // ── Hue bar ───────────────────────────────────────────────────
      float hy = sy + sq_h + bar_gap;
      for (int i = 0; i < 6; i++) {
        NVGcolor c1 = nvgHSLA((float)i / 6.0f,       1.0f, 0.5f, 255);
        NVGcolor c2 = nvgHSLA((float)(i+1) / 6.0f,   1.0f, 0.5f, 255);
        NVGpaint hp2 = nvgLinearGradient(vg, sx + i*sw/6.0f, hy,
                                         sx + (i+1)*sw/6.0f, hy, c1, c2);
        nvgBeginPath(vg);
        nvgRect(vg, sx + (float)i*sw/6.0f, hy, sw/6.0f + 1.0f, bar_h);
        nvgFillPaint(vg, hp2); nvgFill(vg);
      }
      nvgBeginPath(vg); nvgRect(vg, sx, hy, sw, bar_h);
      nvgStrokeColor(vg, nvgRGBA(0,0,0,40)); nvgStrokeWidth(vg,1); nvgStroke(vg);
      // Hue cursor
      float hx_pos = sx + h * sw;
      nvgBeginPath(vg); nvgRect(vg, hx_pos - 3, hy - 2, 6, bar_h + 4);
      nvgFillColor(vg, nvgRGBA(255,255,255,230)); nvgFill(vg);
      nvgBeginPath(vg); nvgRect(vg, hx_pos - 3, hy - 2, 6, bar_h + 4);
      nvgStrokeColor(vg, nvgRGBA(0,0,0,120)); nvgStrokeWidth(vg,1); nvgStroke(vg);

      bool in_hue = (ui->nvgw.mouse_x >= sx && ui->nvgw.mouse_x < sx+sw &&
                     ui->nvgw.mouse_y >= hy && ui->nvgw.mouse_y < hy+bar_h);
      if (in_hue && ui->nvgw.mouse_clicked) ui->color_picker_drag = 2;
      if (ui->color_picker_drag == 2) {
        if (ui->nvgw.mouse_down) {
          h = (ui->nvgw.mouse_x - sx) / sw;
          if (h < 0) h = 0; if (h > 1) h = 1;
        } else { ui->color_picker_drag = 0; }
      }

      // ── Alpha bar ─────────────────────────────────────────────────
      float ay = hy + bar_h + bar_gap;
      // Checkerboard hint
      for (int i = 0; i < (int)(sw/8); i++) {
        NVGcolor checker = (i % 2 == 0) ? nvgRGBA(200,200,200,255) : nvgRGBA(240,240,240,255);
        nvgBeginPath(vg); nvgRect(vg, sx + i*8.0f, ay, 8.0f, bar_h);
        nvgFillColor(vg, checker); nvgFill(vg);
      }
      NVGcolor cur_col = nvgHSLA(h, s, v * 0.5f + (1.0f-s)*0.5f, 255); // approximate
      (void)cur_col;
      NVGpaint ap = nvgLinearGradient(vg, sx, ay, sx+sw, ay,
                      nvgRGBA(0,0,0,0),
                      nvgHSLA(h, s, s*v*0.5f + (1.0f-s)*0.5f, 255));
      nvgBeginPath(vg); nvgRect(vg, sx, ay, sw, bar_h);
      nvgFillPaint(vg, ap); nvgFill(vg);
      nvgBeginPath(vg); nvgRect(vg, sx, ay, sw, bar_h);
      nvgStrokeColor(vg, nvgRGBA(0,0,0,40)); nvgStrokeWidth(vg,1); nvgStroke(vg);
      float ax_pos = sx + a * sw;
      nvgBeginPath(vg); nvgRect(vg, ax_pos - 3, ay - 2, 6, bar_h + 4);
      nvgFillColor(vg, nvgRGBA(255,255,255,230)); nvgFill(vg);
      nvgBeginPath(vg); nvgRect(vg, ax_pos - 3, ay - 2, 6, bar_h + 4);
      nvgStrokeColor(vg, nvgRGBA(0,0,0,120)); nvgStrokeWidth(vg,1); nvgStroke(vg);

      bool in_alpha = (ui->nvgw.mouse_x >= sx && ui->nvgw.mouse_x < sx+sw &&
                       ui->nvgw.mouse_y >= ay && ui->nvgw.mouse_y < ay+bar_h);
      if (in_alpha && ui->nvgw.mouse_clicked) ui->color_picker_drag = 3;
      if (ui->color_picker_drag == 3) {
        if (ui->nvgw.mouse_down) {
          a = (ui->nvgw.mouse_x - sx) / sw;
          if (a < 0) a = 0; if (a > 1) a = 1;
        } else { ui->color_picker_drag = 0; }
      }

      // ── Color preview & old value ──────────────────────────────────
      float preview_y = ay + bar_h + bar_gap;
      float half_sw = sw / 2.0f - 2.0f;
      // New color
      NVGcolor new_col = nvgHSLA(h, s, 0.0f, (uint8_t)(a * 255.0f));
      // Compute RGB from HSV manually (nvgHSLA is HSL not HSV)
      {
        float hh = h * 6.0f;
        int   hi = (int)hh % 6;
        float ff = hh - (float)hi;
        float p  = v * (1.0f - s);
        float q  = v * (1.0f - s * ff);
        float t2 = v * (1.0f - s * (1.0f - ff));
        float r, g, b2;
        switch (hi) {
          case 0: r=v; g=t2; b2=p; break;
          case 1: r=q; g=v;  b2=p; break;
          case 2: r=p; g=v;  b2=t2; break;
          case 3: r=p; g=q;  b2=v; break;
          case 4: r=t2;g=p;  b2=v; break;
          default:r=v; g=p;  b2=q; break;
        }
        new_col = nvgRGBAf(r, g, b2, a);
        // Write back to the target color
        *cp = ((uint32_t)(r  * 255.0f) << 24) |
              ((uint32_t)(g  * 255.0f) << 16) |
              ((uint32_t)(b2 * 255.0f) << 8)  |
               (uint32_t)(a  * 255.0f);
      }
      nvgBeginPath(vg); nvgRect(vg, sx, preview_y, half_sw, 20.0f);
      nvgFillColor(vg, new_col); nvgFill(vg);
      // Old color (original before drag started - use current ptr for now)
      nvgBeginPath(vg); nvgRect(vg, sx + half_sw + 4, preview_y, half_sw, 20.0f);
      nvgFillColor(vg, dark ? nvgRGBA(60,62,70,255) : nvgRGBA(230,230,235,255)); nvgFill(vg);
      nvgFontSize(vg, 11.0f); nvgFontFace(vg, "Inter");
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(vg, dark ? nvgRGBA(180,182,195,255) : nvgRGBA(120,120,130,255));
      nvgText(vg, sx + half_sw + 4 + half_sw/2.0f, preview_y + 10.0f, "original", NULL);

      // Persist updated HSV
      ui->color_picker_h = h;
      ui->color_picker_s = s;
      ui->color_picker_v = v;
      ui->color_picker_a = a;
      ui_request_redraw(ui);

      // Close when clicking outside
      if (ui->nvgw.mouse_clicked) {
        if (ui->nvgw.mouse_x < px || ui->nvgw.mouse_x > px + pw ||
            ui->nvgw.mouse_y < py || ui->nvgw.mouse_y > py + ph) {
          ui->color_picker_open  = 0;
          ui->color_picker_ptr   = NULL;
          ui->color_picker_drag  = 0;
        }
      }
    }
  }

  // Reset per-frame input state (after all processing is done)
  ui->canvas_state.mouse_just_pressed = 0;
  ui->canvas_state.mouse_just_released = 0;
  ui->design_state.mouse_just_pressed = 0;
  ui->design_state.mouse_just_released = 0;

  // Safety net: Reset table resizing state if mouse is released globally
  if (ui->table_state.resizing_col >= 0 && !ui->nvgw.mouse_down) {
    ui->table_state.resizing_col = -1;
  }
  // Safety net: Reset column drag state if mouse is released globally
  if (ui->table_state.dragging_col >= 0 && !ui->nvgw.mouse_down) {
    ui->table_state.dragging_col = -1;
    ui->table_state.drop_target_col = -1;
  }

  // Deselect column header when clicking outside a table header area
  if (ui->nvgw.mouse_clicked && ui->table_state.selected_col_idx >= 0) {
    // Selection reset handled by widget interaction or frame end
  }

  // Override cursor during block drag (only when past dead zone)
  if (ui->block_drag.is_active && ui->block_drag.threshold_met) {
    ui->cursor_type = UI_CURSOR_HAND;
  }

  // Finalize cursor for this frame
  ui_commit_cursor(ui);

  // End NanoVG Widgets input frame
  nvgw_input_end(&ui->nvgw);

  // Keep text_input_active in sync with nvgw focus so that global shortcuts
  // (Cmd+Z, Cmd+V, etc. in main.c) are suppressed while a text field is focused
  ui->text_input_active = (ui->nvgw.focus_id != 0) ? 1 : 0;

  // Status bar at the bottom
  if (ui->status[0] != '\0') {
    NVGcolor bg =
        ui->dark_mode ? nvgRGBA(30, 30, 32, 255) : nvgRGBA(240, 240, 240, 255);
    NVGcolor text_color =
        ui->dark_mode ? nvgRGBA(220, 220, 220, 255) : nvgRGBA(36, 36, 39, 255);

    nvgBeginPath(ui->vg);
    nvgRect(ui->vg, 0, (float)win_h - 24, (float)win_w, 24);
    nvgFillColor(ui->vg, bg);
    nvgFill(ui->vg);

    nvgFontSize(ui->vg, 16.0f);
    nvgFontFace(ui->vg, "Inter");
    nvgFillColor(ui->vg, text_color);
    nvgTextAlign(ui->vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ui->vg, 10, (float)win_h - 12, ui->status, NULL);

    // Clear status if clicked anywhere else
    if (ui->nvgw.mouse_clicked) {
      ui->status[0] = '\0';
    }
  }

  // End NanoVG frame
  nvgEndFrame(ui->vg);
}

// -----------------------------------------------------------------------------
// Home Screen (Dashboard)
// -----------------------------------------------------------------------------

// Helper: Create new document with default setup
// home_create_new_document moved to ui_core.c

// ui_render_home moved to ui_core.c
void ui_render_document_menu(UIHost *ui) {
  if (!ui->show_doc_menu)
    return;

  float bounds_x = ui->doc_menu_rect.x;
  float bounds_y = ui->doc_menu_rect.y;
  float bounds_w = ui->doc_menu_rect.w;
  float bounds_h = ui->doc_menu_rect.h;

  float px = bounds_x + bounds_w - 200;
  float py = bounds_y + bounds_h + 4;
  float pw = 200;
  float ph = 100;

  // Close if clicking outside the popup AND the button that opens it
  if (ui->nvgw.mouse_clicked) {
    bool in_popup = (ui->nvgw.mouse_x >= px && ui->nvgw.mouse_x <= px + pw &&
                     ui->nvgw.mouse_y >= py && ui->nvgw.mouse_y <= py + ph);
    bool in_button = (ui->nvgw.mouse_x >= bounds_x &&
                      ui->nvgw.mouse_x <= bounds_x + bounds_w &&
                      ui->nvgw.mouse_y >= bounds_y &&
                      ui->nvgw.mouse_y <= bounds_y + bounds_h);
    if (!in_popup && !in_button) {
      ui->show_doc_menu = 0;
      return;
    }
  }

  NVGcolor bg =
      ui->dark_mode ? nvgRGBA(45, 46, 50, 255) : nvgRGBA(255, 255, 255, 255);
  NVGcolor border =
      ui->dark_mode ? nvgRGBA(60, 60, 68, 255) : nvgRGBA(228, 228, 231, 255);

  nvgw_panel(&ui->nvgw, px, py, pw, ph, bg, 8.0f);

  nvgBeginPath(ui->vg);
  nvgRoundedRect(ui->vg, px, py, pw, ph, 8.0f);
  nvgStrokeColor(ui->vg, border);
  nvgStrokeWidth(ui->vg, 1.0f);
  nvgStroke(ui->vg);

  if (nvgw_button(&ui->nvgw, nvgw_id_str("doc_save"), px + 10, py + 10, pw - 20,
                  24, "Save (.aslan)")) {
    char path[1024];
    if (ui->engine->doc_path[0] != '\0') {
      snprintf(path, sizeof(path), "%s/snapshot.aslan", ui->engine->doc_path);
      engine_save_snapshot_as_aslan(ui->engine, path);
      ASLAN_INFO("Application saved to %s", path);
    } else {
      ui->show_save_modal = 1;
    }
    ui->show_doc_menu = 0;
  }

  if (nvgw_button(&ui->nvgw, nvgw_id_str("doc_save_as"), px + 10, py + 40,
                  pw - 20, 24, "Save As... (.aslan)")) {
    ui->show_save_modal = 1;
    ui->show_doc_menu = 0;
  }

  char theme_label[64];
  snprintf(theme_label, sizeof(theme_label), "Mode: %s",
           ui->dark_mode ? "Light" : "Dark");
  if (nvgw_button(&ui->nvgw, nvgw_id_str("doc_theme"), px + 10, py + 70,
                  pw - 20, 24, theme_label)) {
    ui->dark_mode = !ui->dark_mode;
  
    ui->show_doc_menu = 0;
  }
}
