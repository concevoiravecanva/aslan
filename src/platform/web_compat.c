// =============================================================================
// ASLAN - Web Compatibility Layer (Emscripten)
// =============================================================================
// Provides web-specific implementations of:
//   - mac_image functions (via stb_image)
//   - File dialog functions (via browser File API + ASYNCIFY)
//   - Download trigger (via JS Blob download)
//   - CSV file dialogs
// =============================================================================

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// mac_image stubs — use image_loader.c (which wraps stb_image on non-Apple)
#include "image_loader.h"

void *mac_load_image_rgba(const char *path, int *w, int *h) {
  return image_load_rgba(path, w, h);
}

int mac_get_image_size(const char *path, int *w, int *h) {
  return image_get_size(path, w, h);
}

// -----------------------------------------------------------------------------
// Async file picker state (shared across dialog functions)
// -----------------------------------------------------------------------------

static volatile int  g_file_ready  = 0;  // 1=ready, -1=cancelled, 0=pending
static char          g_file_result[512] = {0};

// Called from JS when file is written to VFS
EMSCRIPTEN_KEEPALIVE void web_on_file_ready(const char *path) {
  strncpy(g_file_result, path, sizeof(g_file_result) - 1);
  g_file_result[sizeof(g_file_result) - 1] = '\0';
  g_file_ready = 1;
}

EMSCRIPTEN_KEEPALIVE void web_on_file_cancelled(void) {
  g_file_ready = -1;
}

// Trigger a browser <input type="file"> and wait for result via ASYNCIFY
static int web_pick_file(const char *accept, char *out_path, size_t path_size) {
  g_file_ready  = 0;
  g_file_result[0] = '\0';

  EM_ASM({
    var accept = UTF8ToString($0);
    var inp = document.createElement('input');
    inp.type = 'file';
    inp.accept = accept;
    inp.style.display = 'none';
    document.body.appendChild(inp);

    var cleanup = function() {
      if (inp.parentNode) inp.parentNode.removeChild(inp);
    };

    inp.onchange = function(e) {
      var file = e.target.files[0];
      cleanup();
      if (!file) {
        Module.ccall('web_on_file_cancelled', null, [], []);
        return;
      }
      var reader = new FileReader();
      reader.onload = function(ev) {
        var bytes = new Uint8Array(ev.target.result);
        try { FS.mkdir('/tmp'); } catch(e) {}
        var path = '/tmp/' + file.name;
        FS.writeFile(path, bytes);
        Module.ccall('web_on_file_ready', null, ['string'], [path]);
      };
      reader.onerror = function() {
        cleanup();
        Module.ccall('web_on_file_cancelled', null, [], []);
      };
      reader.readAsArrayBuffer(file);
    };

    // Some browsers fire 'cancel' event
    inp.addEventListener('cancel', function() {
      cleanup();
      Module.ccall('web_on_file_cancelled', null, [], []);
    });

    inp.click();
  }, accept);

  // Wait for result (ASYNCIFY: emscripten_sleep yields to browser event loop)
  int timeout_ms = 120000; // 2 minutes max
  while (g_file_ready == 0 && timeout_ms > 0) {
    emscripten_sleep(100);
    timeout_ms -= 100;
  }

  if (g_file_ready == 1 && g_file_result[0]) {
    strncpy(out_path, g_file_result, path_size - 1);
    out_path[path_size - 1] = '\0';
    return 1;
  }
  return 0;
}

// -----------------------------------------------------------------------------
// File dialog functions (replace tinyfiledialogs on web)
// -----------------------------------------------------------------------------

int open_image_file_dialog(char *out_path, size_t path_size) {
  return web_pick_file("image/*", out_path, path_size);
}

int open_csv_file_dialog(char *out_path, size_t path_size) {
  return web_pick_file(".csv,.txt,text/csv", out_path, path_size);
}

int save_csv_file_dialog(char *out_path, size_t path_size,
                         const char *default_name) {
  const char *base = default_name ? default_name : "Export.csv";
  snprintf(out_path, path_size, "/tmp/%s", base);
  return 1;
}

// Font import
const char *ui_host_open_and_import_font(void *ui_ptr);  // forward decl
static int open_font_file_dialog_web(char *out_path, size_t path_size) {
  return web_pick_file(".ttf,.otf,font/ttf,font/otf", out_path, path_size);
}

// -----------------------------------------------------------------------------
// Download trigger — convert a VFS file into a browser download
// -----------------------------------------------------------------------------

void web_trigger_download(const char *vfs_path) {
  if (!vfs_path || !vfs_path[0]) return;
  EM_ASM({
    var path = UTF8ToString($0);
    var filename = path.split('/').pop();
    try {
      var data = FS.readFile(path);
      var mime = 'application/octet-stream';
      if (filename.endsWith('.svg')) mime = 'image/svg+xml';
      else if (filename.endsWith('.png')) mime = 'image/png';
      else if (filename.endsWith('.jpg') || filename.endsWith('.jpeg')) mime = 'image/jpeg';
      else if (filename.endsWith('.csv')) mime = 'text/csv';
      var blob = new Blob([data], {type: mime});
      var url  = URL.createObjectURL(blob);
      var a    = document.createElement('a');
      a.href     = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    } catch(e) {
      console.error('[Aslan] Download failed for', path, ':', e);
    }
  }, vfs_path);
}

// Called once per frame from main.c — no-op for now, kept for future use
void web_frame_tick(void) {}

#endif // __EMSCRIPTEN__
