#include "engine.h"
#include "../platform/ui_host.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Avoid pulling Lua headers into core; we only need to notify the script host
// that the Engine swapped/rebuilt its Document (undo/redo/rewrite).
void scripting_set_document(Document *doc);

// Layers / z-order helpers (document-linked-list based)
bool doc_set_visible(Document *doc, uint32_t id, int visible);
bool doc_bring_to_front(Document *doc, uint32_t id);
bool doc_send_to_back(Document *doc, uint32_t id);
bool doc_bring_forward(Document *doc, uint32_t id);
bool doc_send_backward(Document *doc, uint32_t id);

// Globals API
bool doc_set_global(Document *doc, const char *key, const char *value);
bool doc_unset_global(Document *doc, const char *key);
const char *doc_get_global(const Document *doc, const char *key);
void doc_globals_iterate(const Document *doc, doc_global_iter_fn fn,
                         void *user);

// Clip API
int clip_add(AslanClipPage *page, const char *path);
bool clip_delete(AslanClipPage *page, int index);
bool clip_move(AslanClipPage *page, int from, int to);
bool clip_split(AslanClipPage *page, int index, double time);
bool clip_set_source(AslanClipPage *page, int index, const char *path);
bool clip_set_range(AslanClipPage *page, int index, double start, double end);
bool clip_set_comment(AslanClipPage *page, int index, const char *comment);

// Id maintenance (post-replay)
void doc_recompute_next_ids(Document *doc);

// --- AOF Protocol Helpers ---
typedef struct {
  char *argv[32];
  int argc;
  char _raw[1024]; // Storage for token strings
} AofCommand;

static AofCommand aof_tokenize(const char *line) {
  AofCommand cmd;
  memset(&cmd, 0, sizeof(cmd));
  if (!line || !*line)
    return cmd;

  strncpy(cmd._raw, line, sizeof(cmd._raw) - 1);
  char *p = cmd._raw;
  bool in_quotes = false;
  char *token_start = NULL;

  while (*p) {
    if (*p == '"') {
      if (in_quotes) {
        // End of quote
        *p = '\0';
        if (token_start && cmd.argc < 32) {
          cmd.argv[cmd.argc++] = token_start;
        }
        token_start = NULL;
        in_quotes = false;
      } else {
        // Start of quote
        in_quotes = true;
        token_start = p + 1;
      }
    } else if (!in_quotes && (*p == ' ' || *p == '\t')) {
      if (token_start) {
        *p = '\0';
        if (cmd.argc < 32) {
          cmd.argv[cmd.argc++] = token_start;
        }
        token_start = NULL;
      }
    } else {
      if (!token_start) {
        token_start = p;
      }
    }
    p++;
  }

  if (token_start && cmd.argc < 32) {
    cmd.argv[cmd.argc++] = token_start;
  }

  return cmd;
}

static void engine_emit_command(Engine *engine, const char *line) {
  if (!engine || !engine->history_file || !line)
    return;

  // Noisy commands pruning (like set_rect)
  if (strncmp(line, "set_rect ", 9) == 0 || strncmp(line, "move ", 5) == 0) {
    // For now, these are handled by prune_last_session_set_rect_and_append
    // which is called by the UI handlers. We should migrate that here later.
    // For today, we just append to maintain current behavior but centralized.
  }

  fprintf(engine->history_file, "%s\n", line);
  fflush(engine->history_file);
}

// Pages API
uint32_t doc_page_create(Document *doc, const char *name);
uint32_t doc_page_create_with_id(Document *doc, uint32_t page_id,
                                 const char *name);
bool doc_page_select(Document *doc, uint32_t page_id);
uint32_t doc_page_active_id(const Document *doc);
const char *doc_page_name(const Document *doc, uint32_t page_id);

static void rewrite_emit_global_kv(const char *key, const char *value,
                                   void *user) {
  FILE *out = (FILE *)user;
  if (!out || !key || !key[0] || !value)
    return;

  // Minimal escaping for quotes/backslashes so the line stays parseable.
  char escaped[512];
  size_t wi = 0;
  for (size_t i = 0; value[i] != '\0' && wi + 2 < sizeof(escaped); i++) {
    char c = value[i];
    if (c == '\\' || c == '"') {
      if (wi + 2 >= sizeof(escaped))
        break;
      escaped[wi++] = '\\';
      escaped[wi++] = c;
    } else {
      escaped[wi++] = c;
    }
  }
  escaped[wi] = 0;

  fprintf(out, "set_global %s \"%s\"\n", key, escaped);
}

void engine_serialize_note(const AslanNoteDoc *doc, uint32_t page_id,
                           FILE *fp) {
  if (!fp)
    return;

  // Canonical snapshot format for notes. For now: clear + re-insert all text
  // lines.
  fprintf(fp, "note_clear %u\n", (unsigned)page_id);

  if (!doc)
    return;

  for (int i = 0; i < doc->row_count; i++) {
    const AslanRow *r = &doc->rows[i];
    const char *content = r->chars ? r->chars : "";

    // Escape helper (minimal for now: quotes)
    char escaped[2048];
    int j = 0;
    for (int k = 0; content[k] && j < (int)sizeof(escaped) - 2; k++) {
      if (content[k] == '\"') {
        escaped[j++] = '\\';
      }
      escaped[j++] = content[k];
    }
    escaped[j] = '\0';

    if (r->type == ASLAN_ROW_TEXT) {
      fprintf(fp, "note_ins_ln %u %d \"%s\"\n", (unsigned)page_id, i, escaped);
    } else if (r->type == ASLAN_ROW_HEADING_1) {
      fprintf(fp, "note_ins_block %u %d h1 \"%s\"\n", (unsigned)page_id, i,
              escaped);
    } else if (r->type == ASLAN_ROW_HEADING_2) {
      fprintf(fp, "note_ins_block %u %d h2 \"%s\"\n", (unsigned)page_id, i,
              escaped);
    } else if (r->type == ASLAN_ROW_HEADING_3) {
      fprintf(fp, "note_ins_block %u %d h3 \"%s\"\n", (unsigned)page_id, i,
              escaped);
    } else if (r->type == ASLAN_ROW_SEPARATOR) {
      fprintf(fp, "note_ins_block %u %d separator \"%s\"\n", (unsigned)page_id,
              i, escaped);
    } else if (r->type == ASLAN_ROW_CALLOUT) {
      // Theme defaults to INFO if missing.
      int theme = 0;
      if (r->block) {
        const AslanCalloutBlock *cb = (const AslanCalloutBlock *)r->block;
        theme = (int)cb->theme;
      }
      fprintf(fp, "note_ins_block %u %d callout %d \"%s\"\n", (unsigned)page_id,
              i, theme, escaped);
    }
  }
}

static const char *k_history_path = "history.log";
static const char *k_redo_path = "redo.log";

static void prune_last_session_set_rect_and_append(const char *command_line) {
  if (!command_line)
    return;
  FILE *f = fopen(k_history_path, "r");
  if (!f) {
    FILE *out = fopen(k_history_path, "a");
    if (out) {
      fprintf(out, "%s\n", command_line);
      fclose(out);
    }
    return;
  }

  char **lines = malloc(4096 * sizeof(char *));
  int count = 0;
  char buf[512];
  while (fgets(buf, sizeof(buf), f) && count < 4096) {
    lines[count++] = strdup(buf);
  }
  fclose(f);

  // Find last session marker
  int last_session = -1;
  for (int i = 0; i < count; i++) {
    if (strstr(lines[i], "-- Session started --") != NULL)
      last_session = i;
  }

  // Keep only the last set_rect after the last session marker (drop earlier
  // ones), then append new.
  int last_set_rect_after_session = -1;
  for (int i = last_session + 1; i < count; i++) {
    const char *l = lines[i];
    while (*l == ' ' || *l == '\t')
      l++;
    if (strncmp(l, "set_rect ", 9) == 0)
      last_set_rect_after_session = i;
  }

  f = fopen(k_history_path, "w");
  if (f) {
    for (int i = 0; i < count; i++) {
      if (i > last_session) {
        const char *l = lines[i];
        while (*l == ' ' || *l == '\t')
          l++;
        // Drop all but the last set_rect of the session.
        if (strncmp(l, "set_rect ", 9) == 0 && i != last_set_rect_after_session)
          continue;
      }
      fputs(lines[i], f);
    }
    fprintf(f, "%s\n", command_line);
    fclose(f);
  }

  for (int i = 0; i < count; i++)
    free(lines[i]);
  free(lines);
}

static void clear_history_log(void) {
  FILE *f = fopen(k_history_path, "w");
  if (f)
    fclose(f);
}

static bool is_mutating_command_line(const char *line) {
  if (!line)
    return false;
  const char *l = line;
  while (*l == ' ' || *l == '\t')
    l++;
  if (*l == '\0' || *l == '\n' || *l == '\r' || *l == '-')
    return false;

  // Treat only "committed" geometry changes as AOF mutations.
  // High-frequency interactive commands (move/resize) are executed but not
  // logged. NOTE: `page_create` is logged in a canonical form inside the
  // command handler, so it is intentionally excluded from this predicate.
  return (
      strncmp(l, "rect ", 5) == 0 || strncmp(l, "create_rect ", 12) == 0 ||
      strncmp(l, "set_rect ", 9) == 0 || strncmp(l, "create_text ", 12) == 0 ||
      strncmp(l, "set_text ", 9) == 0 || strncmp(l, "page_type ", 10) == 0 ||
      strncmp(l, "grid_set ", 9) == 0 ||
      // CLIP commands
      strncmp(l, "clip_add ", 9) == 0 || strncmp(l, "clip_del ", 9) == 0 ||
      strncmp(l, "clip_move ", 10) == 0 || strncmp(l, "clip_split ", 11) == 0 ||
      strncmp(l, "clip_set_source ", 16) == 0 ||
      strncmp(l, "clip_set_range ", 15) == 0 ||
      strncmp(l, "clip_set_comment ", 17) == 0 ||
      // NOTE block commands
      strncmp(l, "note_insert_block ", 18) == 0 ||
      strncmp(l, "note_set_callout_theme ", 23) == 0 ||
      // NOTE: `page_meta` is treated as view/camera state for option B:
      // persisted in history/save, but NOT undoable.
      // So we intentionally exclude it from the undo/redo mutating predicate.
      strncmp(l, "doc_type ", 9) == 0 || strncmp(l, "doc_meta ", 9) == 0 ||
      strncmp(l, "set_global ", 11) == 0 ||
      strncmp(l, "unset_global ", 13) == 0 ||
      strncmp(l, "set_visible ", 12) == 0 ||
      strncmp(l, "bring_to_front ", 15) == 0 ||
      strncmp(l, "send_to_back ", 13) == 0 ||
      strncmp(l, "bring_forward ", 14) == 0 ||
      strncmp(l, "send_backward ", 14) == 0);
}

static void clear_redo_log(void) {
  FILE *f = fopen(k_redo_path, "w");
  if (f)
    fclose(f);
}

static void gather_nodes_recursive(Node *node, Node **out, int *count,
                                   int max) {
  if (!node || !out || !count)
    return;
  if (node->type != NODE_TYPE_ROOT) {
    if (*count < max)
      out[(*count)++] = node;
  }
  gather_nodes_recursive(node->first_child, out, count, max);
  gather_nodes_recursive(node->next_sibling, out, count, max);
}

static int node_is_group(const Node *n) {
  if (!n)
    return 0;
  if (n->type != NODE_TYPE_RECT)
    return 0;
  if (strncmp(n->name, "Group", 5) != 0)
    return 0;
  return 1;
}

static void translate_group_children(Node *parent, float dx, float dy) {
  if (!parent)
    return;
  Node *child = parent->first_child;
  while (child) {
    child->x += dx;
    child->y += dy;
    if (child->first_child) {
      translate_group_children(child, dx, dy);
    }
    child = child->next_sibling;
  }
}

static int cmp_node_id_ptr(const void *a, const void *b) {
  const Node *na = *(const Node *const *)a;
  const Node *nb = *(const Node *const *)b;
  if (na->id < nb->id)
    return -1;
  if (na->id > nb->id)
    return 1;
  return 0;
}

static bool engine_rewrite_aof(Engine *engine) {
  if (!engine || !engine->doc || !engine->doc->root)
    return false;

  // fermer history en cours (sinon windows/mac garde un fd ouvert)
  FILE *saved = engine->history_file;
  if (saved) {
    fclose(saved);
    engine->history_file = NULL;
  }

  // snapshot -> fichier temporaire puis rename (safe)
  const char *tmp_path = "history.tmp";
  FILE *out = fopen(tmp_path, "w");
  if (!out) {
    engine->history_file = fopen(k_history_path, "a");
    return false;
  }

  fprintf(out, "-- aslan_aof:1 --\n");

  // Document meta first (so the replay configures the doc before creating
  // items)
  if (engine->doc) {
    fprintf(out, "doc_type %s\n",
            engine->doc->doc_type[0] ? engine->doc->doc_type : "canvas2d");
    fprintf(out, "doc_meta width %d\n", engine->doc->doc_width);
    fprintf(out, "doc_meta height %d\n", engine->doc->doc_height);
  }

  // Document globals
  if (engine->doc) {
    doc_globals_iterate(engine->doc, rewrite_emit_global_kv, out);
  }

  // Pages: declare pages with stable ids and rewrite each page's tree.
  // This keeps `history.log` canonical and avoids duplicates/no-ops.
  const uint32_t active_before = doc_page_active_id(engine->doc);
  uint32_t active_emitted = 0;

  for (int pi = 0; pi < 32; pi++) {
    if (!engine->doc->pages[pi].in_use)
      continue;
    fprintf(out, "page_create %u \"%s\"\n",
            (unsigned)engine->doc->pages[pi].page_id,
            engine->doc->pages[pi].name[0] ? engine->doc->pages[pi].name
                                           : "Page");

    // Persist page type and meta
    if (engine->doc->pages[pi].type == PAGE_TYPE_PAGE) {
      // Canonical token: page
      fprintf(out, "page_type %u page\n",
              (unsigned)engine->doc->pages[pi].page_id);
      fprintf(out, "page_meta %u width %.2f\n",
              (unsigned)engine->doc->pages[pi].page_id,
              engine->doc->pages[pi].width);
      fprintf(out, "page_meta %u height %.2f\n",
              (unsigned)engine->doc->pages[pi].page_id,
              engine->doc->pages[pi].height);
    } else if (engine->doc->pages[pi].type == PAGE_TYPE_SPACE) {
      // Canonical token: space
      fprintf(out, "page_type %u space\n",
              (unsigned)engine->doc->pages[pi].page_id);
      fprintf(out, "page_meta %u viewport.x %.2f\n",
              (unsigned)engine->doc->pages[pi].page_id,
              engine->doc->pages[pi].view_x);
      fprintf(out, "page_meta %u viewport.y %.2f\n",
              (unsigned)engine->doc->pages[pi].page_id,
              engine->doc->pages[pi].view_y);
      fprintf(out, "page_meta %u viewport.zoom %.2f\n",
              (unsigned)engine->doc->pages[pi].page_id,
              engine->doc->pages[pi].view_zoom);
    } else {
      // Placeholders for now: only persist type so we don't lose intent.
      const char *t = "space";
      if (engine->doc->pages[pi].type == PAGE_TYPE_TABLE)
        t = "table";
      else if (engine->doc->pages[pi].type == PAGE_TYPE_NOTE)
        t = "note";
      else if (engine->doc->pages[pi].type == PAGE_TYPE_CLIP)
        t = "clip";
      else if (engine->doc->pages[pi].type == PAGE_TYPE_GRID)
        t = "grid";
      fprintf(out, "page_type %u %s\n",
              (unsigned)engine->doc->pages[pi].page_id, t);
    }
  }

  for (int pi = 0; pi < 32; pi++) {
    if (!engine->doc->pages[pi].in_use)
      continue;
    Node *page_root = engine->doc->pages[pi].root;
    if (!page_root)
      continue;

    if (active_emitted != engine->doc->pages[pi].page_id) {
      fprintf(out, "page_select %u\n",
              (unsigned)engine->doc->pages[pi].page_id);
      active_emitted = engine->doc->pages[pi].page_id;
    }

    // Notes snapshot (v1): emit note lines before canvas objects.
    if (engine->doc->pages[pi].type == PAGE_TYPE_NOTE) {
      engine_serialize_note(engine->doc->pages[pi].note,
                            engine->doc->pages[pi].page_id, out);
    }

    // 1) Snapshot objects deterministically by id
    Node *nodes[4096];
    int count = 0;
    gather_nodes_recursive(page_root, nodes, &count, 4096);
    qsort(nodes, (size_t)count, sizeof(Node *), cmp_node_id_ptr);

    for (int i = 0; i < count; i++) {
      Node *n = nodes[i];
      if (!n)
        continue;
      if (n->type == NODE_TYPE_RECT) {
        fprintf(out, "create_rect %u %s %f %f %f %f\n", (unsigned)n->id,
                n->name, n->x, n->y, n->width, n->height);
      } else if (n->type == NODE_TYPE_TEXT) {
        fprintf(out, "create_text %u %s %f %f %f %f \"%s\"\n", (unsigned)n->id,
                n->name, n->x, n->y, n->width, n->height, n->text);
      } else if (n->type == NODE_TYPE_IMAGE) {
        fprintf(out, "create_image %u %s %f %f %f %f \"%s\"\n", (unsigned)n->id,
                n->name, n->x, n->y, n->width, n->height, n->text);
      }
    }

    // 2) Persist visibility state (only when hidden)
    for (int i = 0; i < count; i++) {
      Node *n = nodes[i];
      if (!n)
        continue;
      if (n->visible == 0) {
        fprintf(out, "set_visible %u 0\n", (unsigned)n->id);
      }
    }

    // 3) Persist z-order (sibling order under this page root)
    Node *child = page_root->first_child;
    Node *stack[4096];
    int scount = 0;
    while (child && scount < 4096) {
      stack[scount++] = child;
      child = child->next_sibling;
    }
    for (int i = 0; i < scount; i++) {
      fprintf(out, "bring_to_front %u\n", (unsigned)stack[i]->id);
    }
  }

  // Restore active page
  if (active_before != 0 && active_before != active_emitted) {
    fprintf(out, "page_select %u\n", (unsigned)active_before);
    active_emitted = active_before;
  }

  fclose(out);

  // replace history
  remove(k_history_path);
  if (rename(tmp_path, k_history_path) != 0) {
    // fallback: keep tmp for inspection
    engine->history_file = fopen(k_history_path, "a");
    return false;
  }

  // redo invalid after rewrite
  clear_redo_log();

  if (engine->ui_host)
    engine->ui_host->selected_node = NULL;
  scripting_set_document(engine->doc);
  engine->history_file = fopen(k_history_path, "a");
  return engine->history_file != NULL;
}

static bool engine_write_snapshot_to_file(Engine *engine, FILE *out,
                                          const char *header_line) {
  if (!engine || !engine->doc || !engine->doc->root || !out)
    return false;

  if (header_line && header_line[0]) {
    fprintf(out, "%s\n", header_line);
  }

  // Document meta first (so the replay configures the doc before creating
  // items)
  fprintf(out, "doc_type %s\n",
          engine->doc->doc_type[0] ? engine->doc->doc_type : "canvas2d");
  fprintf(out, "doc_meta width %d\n", engine->doc->doc_width);
  fprintf(out, "doc_meta height %d\n", engine->doc->doc_height);

  // Document globals
  doc_globals_iterate(engine->doc, rewrite_emit_global_kv, out);

  // Pages: declare pages (with stable ids) and serialize each page's node tree.
  // Format v1 extension (back-compat):
  // - Old: page_create "Name"
  // - New: page_create <id> "Name"
  // We always emit the new form to keep ids stable across save/load.
  if (engine->doc) {
    for (int i = 0; i < 32; i++) {
      if (!engine->doc->pages[i].in_use)
        continue;
      fprintf(out, "page_create %u \"%s\"\n",
              (unsigned)engine->doc->pages[i].page_id,
              engine->doc->pages[i].name[0] ? engine->doc->pages[i].name
                                            : "Page");

      // Persist page type and meta
      if (engine->doc->pages[i].type == PAGE_TYPE_PAGE) {
        fprintf(out, "page_type %u page\n",
                (unsigned)engine->doc->pages[i].page_id);
        fprintf(out, "page_meta %u width %.2f\n",
                (unsigned)engine->doc->pages[i].page_id,
                engine->doc->pages[i].width);
        fprintf(out, "page_meta %u height %.2f\n",
                (unsigned)engine->doc->pages[i].page_id,
                engine->doc->pages[i].height);
      } else if (engine->doc->pages[i].type == PAGE_TYPE_SPACE) {
        fprintf(out, "page_type %u space\n",
                (unsigned)engine->doc->pages[i].page_id);
        fprintf(out, "page_meta %u viewport.x %.2f\n",
                (unsigned)engine->doc->pages[i].page_id,
                engine->doc->pages[i].view_x);
        fprintf(out, "page_meta %u viewport.y %.2f\n",
                (unsigned)engine->doc->pages[i].page_id,
                engine->doc->pages[i].view_y);
        fprintf(out, "page_meta %u viewport.zoom %.2f\n",
                (unsigned)engine->doc->pages[i].page_id,
                engine->doc->pages[i].view_zoom);
      } else {
        const char *t = "space";
        if (engine->doc->pages[i].type == PAGE_TYPE_TABLE)
          t = "table";
        else if (engine->doc->pages[i].type == PAGE_TYPE_NOTE)
          t = "note";
        else if (engine->doc->pages[i].type == PAGE_TYPE_CLIP)
          t = "clip";
        else if (engine->doc->pages[i].type == PAGE_TYPE_GRID)
          t = "grid";
        fprintf(out, "page_type %u %s\n",
                (unsigned)engine->doc->pages[i].page_id, t);
      }
    }

    // Serialize each page deterministically in the order of the pages array.
    for (int i = 0; i < 32; i++) {
      if (!engine->doc->pages[i].in_use)
        continue;

      Node *page_root = engine->doc->pages[i].root;
      if (!page_root)
        continue;

      fprintf(out, "page_select %u\n", (unsigned)engine->doc->pages[i].page_id);

      // Notes snapshot (v1): emit note lines before canvas objects.
      if (engine->doc->pages[i].type == PAGE_TYPE_NOTE) {
        engine_serialize_note(engine->doc->pages[i].note,
                              engine->doc->pages[i].page_id, out);
      }

      // 1) Snapshot objects deterministically by id
      Node *nodes[4096];
      int count = 0;
      gather_nodes_recursive(page_root, nodes, &count, 4096);
      qsort(nodes, (size_t)count, sizeof(Node *), cmp_node_id_ptr);

      for (int j = 0; j < count; j++) {
        Node *n = nodes[j];
        if (!n)
          continue;
        if (n->type == NODE_TYPE_RECT) {
          fprintf(out, "create_rect %u %s %f %f %f %f\n", (unsigned)n->id,
                  n->name, n->x, n->y, n->width, n->height);
        } else if (n->type == NODE_TYPE_TEXT) {
          fprintf(out, "create_text %u %s %f %f %f %f \"%s\"\n",
                  (unsigned)n->id, n->name, n->x, n->y, n->width, n->height,
                  n->text);
        } else if (n->type == NODE_TYPE_IMAGE) {
          fprintf(out, "create_image %u %s %f %f %f %f \"%s\"\n",
                  (unsigned)n->id, n->name, n->x, n->y, n->width, n->height,
                  n->text);
        }
      }

      // 2) Persist visibility state (only when hidden)
      for (int j = 0; j < count; j++) {
        Node *n = nodes[j];
        if (!n)
          continue;
        if (n->visible == 0) {
          fprintf(out, "set_visible %u 0\n", (unsigned)n->id);
        }
      }

      // 3) Persist z-order: based on sibling order under this page root.
      Node *child = page_root->first_child;
      Node *stack[4096];
      int scount = 0;
      while (child && scount < 4096) {
        stack[scount++] = child;
        child = child->next_sibling;
      }
      for (int j = 0; j < scount; j++) {
        fprintf(out, "bring_to_front %u\n", (unsigned)stack[j]->id);
      }
    }

    // Restore the user's active page at the end (so loading lands in the same
    // page).
    fprintf(out, "page_select %u\n", (unsigned)doc_page_active_id(engine->doc));
  }

  return true;
}

bool engine_save_snapshot_as_aslan(Engine *engine, const char *out_path) {
  if (!engine || !engine->doc || !engine->doc->root || !out_path ||
      !out_path[0])
    return false;

  FILE *out = fopen(out_path, "w");
  if (!out)
    return false;

  bool ok = engine_write_snapshot_to_file(engine, out, "-- aslan:1 --");
  fclose(out);
  return ok;
}

static bool is_comment_or_empty_line(const char *line) {
  if (!line)
    return true;
  const char *l = line;
  while (*l == ' ' || *l == '\t')
    l++;
  if (*l == '\0' || *l == '\n' || *l == '\r')
    return true;
  if (*l == '-' && *(l + 1) == '-')
    return true;
  return false;
}

static void strip_trailing_newlines(char *s) {
  if (!s)
    return;
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
    s[n - 1] = 0;
    n--;
  }
}

bool engine_load_snapshot_from_aslan(Engine *engine, const char *in_path) {
  if (!engine || !in_path || !in_path[0])
    return false;

  FILE *f = fopen(in_path, "r");
  if (!f)
    return false;

  // Reset to a clean doc + clear logs (we don't want to append open-file
  // commands to history.log). This leverages existing clear behavior.
  engine_execute_command(engine, "clear");

  char line[1024];
  bool saw_any_command = false;
  while (fgets(line, sizeof(line), f)) {
    strip_trailing_newlines(line);
    if (is_comment_or_empty_line(line))
      continue;

    // Accept v1 header as comment-ish. We keep it in file but don't execute it.
    if (strcmp(line, "-- aslan:1 --") == 0)
      continue;

    if (engine_execute_command(engine, line)) {
      saw_any_command = true;
    }
  }

  fclose(f);

  // After bulk replay, ensure future creations use collision-free ids.
  if (engine->doc) {
    doc_recompute_next_ids(engine->doc);
  }
  return saw_any_command;
}

Engine *engine_create(void) {
  Engine *engine = (Engine *)calloc(1, sizeof(Engine));
  if (!engine)
    return NULL;

  engine->doc = doc_create();
  engine->is_running = true;

  // Ouverture du journal en mode "append"
  engine->history_file = fopen(k_history_path, "a");
  if (engine->history_file) {
    fprintf(engine->history_file, "-- Session started --\n");
    fflush(engine->history_file);
  }

  return engine;
}

void engine_destroy(Engine *engine) {
  if (!engine)
    return;
  if (engine->history_file)
    fclose(engine->history_file);
  doc_destroy(engine->doc);
  free(engine);
}

// Parsing simple pour le prototype
// Format attendu : "action arg1 arg2 ..."
bool engine_execute_command(Engine *engine, const char *command_line) {
  if (!command_line || !*command_line)
    return false;

  AofCommand cmd = aof_tokenize(command_line);
  if (cmd.argc == 0)
    return false;

  char name[256];
  float x, y, w, h;
  int id;
  int value;
  const char *verb = cmd.argv[0];
  bool success = false;

  // Commande: exit
  if (strcmp(verb, "exit") == 0) {
    engine->is_running = false;
    success = true;
  }

  // Commande: clear
  else if (strcmp(verb, "clear") == 0) {
    // Close AOF
    if (engine->history_file) {
      fclose(engine->history_file);
      engine->history_file = NULL;
    }

    // Reset logs
    clear_history_log();
    clear_redo_log();

    // Reset document
    doc_destroy(engine->doc);
    engine->doc = doc_create();

    // Default meta already set by doc_create()

    // Reset UI selection
    if (engine->ui_host)
      engine->ui_host->selected_node = NULL;

    // Sync Lua host with the new document
    scripting_set_document(engine->doc);

    // Reopen AOF for appends
    engine->history_file = fopen(k_history_path, "a");
    if (engine->history_file) {
      fprintf(engine->history_file, "-- Session started --\n");
      fflush(engine->history_file);
    }
    printf(">> Cleared document and history.\n");
    success = true;
  }

  // Commande: doc_type <type>
  else if (strcmp(verb, "doc_type") == 0 && cmd.argc >= 2) {
    if (engine->doc) {
      memset(engine->doc->doc_type, 0, sizeof(engine->doc->doc_type));
      strncpy(engine->doc->doc_type, cmd.argv[1],
              sizeof(engine->doc->doc_type) - 1);
      success = true;
    }
  }

  // Commande: doc_meta width|height <int>
  else if (strcmp(verb, "doc_meta") == 0 && cmd.argc >= 3) {
    if (engine->doc) {
      int val = atoi(cmd.argv[2]);
      if (strcmp(cmd.argv[1], "width") == 0) {
        engine->doc->doc_width = val;
        success = true;
      } else if (strcmp(cmd.argv[1], "height") == 0) {
        engine->doc->doc_height = val;
        success = true;
      }
      if (success)
        engine_emit_command(engine, command_line);
    }
  }

  // Commande: page_create [<id>] <name>
  else if (strcmp(verb, "page_create") == 0 && cmd.argc >= 2) {
    uint32_t forced_id = 0;
    const char *page_name = NULL;
    if (cmd.argc >= 3) {
      forced_id = (uint32_t)atoi(cmd.argv[1]);
      page_name = cmd.argv[2];
    } else {
      page_name = cmd.argv[1];
    }

    if (engine->doc) {
      uint32_t pid = 0;
      if (forced_id > 0) {
        // Idempotent: if the page already exists, treat as success but do NOT
        // log.
        const char *existing_name = doc_page_name(engine->doc, forced_id);
        if (existing_name) {
          success = true;
          return success;
        }
        pid = doc_page_create_with_id(engine->doc, forced_id, page_name);
      } else {
        pid = doc_page_create(engine->doc, page_name);
      }

      if (pid != 0) {
        // Log command (canonical form).
        if (engine->history_file) {
          char canon[256];
          snprintf(canon, sizeof(canon), "page_create %u \"%s\"", (unsigned)pid,
                   page_name);
          engine_emit_command(engine, canon);
        }
        if (engine->ui_host)
          engine->ui_host->selected_node = NULL;
        success = true;
        return success;
      }
    }
  }

  // Commande: page_select <page_id>
  else if (strcmp(verb, "page_select") == 0 && cmd.argc >= 2) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    if (engine->doc) {
      if (pid == doc_page_active_id(engine->doc)) {
        success = true;
      } else if (doc_page_select(engine->doc, pid)) {
        // page_select is historically logged as-is in engine_execute_command
        // but we can also log it here if needed. For now, let's keep the
        // end-of-function logging for simplicity but flag it.
        if (engine->ui_host)
          engine->ui_host->selected_node = NULL;
        success = true;
      }
    }
  }

  // Commande: set_global <key> <value>
  else if (strcmp(verb, "set_global") == 0 && cmd.argc >= 3) {
    success = doc_set_global(engine->doc, cmd.argv[1], cmd.argv[2]);
    if (success) {
      char canon[1024];
      snprintf(canon, sizeof(canon), "set_global %s \"%s\"", cmd.argv[1],
               cmd.argv[2]);
      engine_emit_command(engine, canon);
    }
  }

  // Commande: unset_global <key>
  else if (strcmp(verb, "unset_global") == 0 && cmd.argc >= 2) {
    success = doc_unset_global(engine->doc, cmd.argv[1]);
    if (success)
      engine_emit_command(engine, command_line);
  }

  // Commande: page_type <page_id> <type>
  else if (strcmp(verb, "page_type") == 0 && cmd.argc >= 3) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    const char *type_str = cmd.argv[2];
    PageType pt = PAGE_TYPE_SPACE;
    if (strcmp(type_str, "page") == 0 || strcmp(type_str, "print") == 0)
      pt = PAGE_TYPE_PAGE;
    else if (strcmp(type_str, "space") == 0 || strcmp(type_str, "board") == 0)
      pt = PAGE_TYPE_SPACE;
    else if (strcmp(type_str, "table") == 0)
      pt = PAGE_TYPE_TABLE;
    else if (strcmp(type_str, "note") == 0)
      pt = PAGE_TYPE_NOTE;
    else if (strcmp(type_str, "clip") == 0)
      pt = PAGE_TYPE_CLIP;
    else if (strcmp(type_str, "grid") == 0)
      pt = PAGE_TYPE_GRID;

    success = doc_page_set_type(engine->doc, pid, pt);
    if (success)
      engine_emit_command(engine, command_line);
  }

  // Commande: page_meta <page_id> <key> <value>
  else if (strcmp(verb, "page_meta") == 0 && cmd.argc >= 4) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    success = doc_page_set_meta(engine->doc, pid, cmd.argv[2],
                                (float)atof(cmd.argv[3]));
    if (success)
      engine_emit_command(engine, command_line);
  }

  // Commande: note_clear <page_id>
  else if (strcmp(verb, "note_clear") == 0 && cmd.argc >= 2) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    if (engine->doc) {
      for (int i = 0; i < 32; i++) {
        if (!engine->doc->pages[i].in_use ||
            engine->doc->pages[i].page_id != pid)
          continue;

        if (engine->doc->pages[i].note == NULL) {
          (void)doc_page_set_type(engine->doc, pid, PAGE_TYPE_NOTE);
        }

        AslanNoteDoc *nd = engine->doc->pages[i].note;
        if (nd) {
          for (int r = 0; r < nd->row_count; r++) {
            free(nd->rows[r].chars);
            if (nd->rows[r].block) {
              AslanCalloutBlock *cb = (AslanCalloutBlock *)nd->rows[r].block;
              free(cb->chars);
              free(cb);
            }
          }
          free(nd->rows);
          nd->rows = NULL;
          nd->row_count = 0;
          nd->row_cap = 0;
          nd->cursor_row = 0;
          nd->cursor_col = 0;
        }
        success = true;
        break;
      }
    }
  }

  // Commande: note_ins_ln <page_id> <row> <text>
  else if (strcmp(verb, "note_ins_ln") == 0 && cmd.argc >= 4) {
    uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
    int row_idx = atoi(cmd.argv[2]);
    const char *text = cmd.argv[3];
    if (engine->doc) {
      for (int i = 0; i < 32; i++) {
        if (!engine->doc->pages[i].in_use ||
            engine->doc->pages[i].page_id != page_id)
          continue;

        if (engine->doc->pages[i].note == NULL) {
          (void)doc_page_set_type(engine->doc, page_id, PAGE_TYPE_NOTE);
        }
        AslanNoteDoc *nd = engine->doc->pages[i].note;
        if (nd) {
          note_insert_line(nd, row_idx, text);
          success = true;
        }
        break;
      }
    }
  }

  // Commande: note_ins_block <page_id> <row> <kind> [theme] <text>
  else if (strcmp(verb, "note_ins_block") == 0 && cmd.argc >= 5) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    int row_idx = atoi(cmd.argv[2]);
    const char *kind_str = cmd.argv[3];
    int theme = 0;
    const char *text = NULL;

    if (cmd.argc >= 6 && strcmp(kind_str, "callout") == 0) {
      theme = atoi(cmd.argv[4]);
      text = cmd.argv[5];
    } else {
      text = cmd.argv[4];
    }

    if (engine->doc) {
      for (int i = 0; i < 32; i++) {
        if (!engine->doc->pages[i].in_use ||
            engine->doc->pages[i].page_id != pid)
          continue;

        if (engine->doc->pages[i].note == NULL) {
          (void)doc_page_set_type(engine->doc, pid, PAGE_TYPE_NOTE);
        }
        AslanNoteDoc *nd = engine->doc->pages[i].note;
        if (!nd) {
          success = true;
          break;
        }

        if (row_idx < 0)
          row_idx = 0;
        if (row_idx > nd->row_count)
          row_idx = nd->row_count;

        // Grow array if needed
        if (nd->row_count + 1 > nd->row_cap) {
          int nc = nd->row_cap ? nd->row_cap * 2 : 16;
          nd->rows = (AslanRow *)realloc(nd->rows, nc * sizeof(AslanRow));
          nd->row_cap = nc;
        }

        if (row_idx < nd->row_count) {
          memmove(&nd->rows[row_idx + 1], &nd->rows[row_idx],
                  (size_t)(nd->row_count - row_idx) * sizeof(AslanRow));
        }

        AslanRow *r = &nd->rows[row_idx];
        memset(r, 0, sizeof(*r));
        r->chars = strdup(text ? text : "");
        r->len = strlen(r->chars);
        r->cap = r->len + 1;

        if (strcmp(kind_str, "h1") == 0)
          r->type = ASLAN_ROW_HEADING_1;
        else if (strcmp(kind_str, "h2") == 0)
          r->type = ASLAN_ROW_HEADING_2;
        else if (strcmp(kind_str, "h3") == 0)
          r->type = ASLAN_ROW_HEADING_3;
        else if (strcmp(kind_str, "separator") == 0)
          r->type = ASLAN_ROW_SEPARATOR;
        else if (strcmp(kind_str, "callout") == 0) {
          r->type = ASLAN_ROW_CALLOUT;
          AslanCalloutBlock *cb =
              (AslanCalloutBlock *)calloc(1, sizeof(AslanCalloutBlock));
          if (cb) {
            cb->theme = (AslanCalloutTheme)theme;
            cb->chars = strdup(text ? text : "");
            cb->len = strlen(cb->chars);
            cb->cap = cb->len + 1;
            r->block = cb;
          }
        } else {
          r->type = ASLAN_ROW_TEXT;
        }

        nd->row_count++;
        success = true;
        break;
      }
    }
  }

  // Commande: note_set_ln <page_id> <row> <text>
  else if (strcmp(verb, "note_set_ln") == 0 && cmd.argc >= 4) {
    uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
    int row_idx = atoi(cmd.argv[2]);
    const char *text = cmd.argv[3];

    if (engine->doc) {
      for (int i = 0; i < 32; i++) {
        if (!engine->doc->pages[i].in_use ||
            engine->doc->pages[i].page_id != page_id)
          continue;

        if (engine->doc->pages[i].note == NULL) {
          (void)doc_page_set_type(engine->doc, page_id, PAGE_TYPE_NOTE);
        }
        AslanNoteDoc *nd = engine->doc->pages[i].note;
        if (!nd) {
          success = true;
          break;
        }

        if (nd->row_count == 0) {
          note_insert_line(nd, 0, "");
        }

        if (row_idx < 0)
          row_idx = 0;
        if (row_idx >= nd->row_count) {
          while (nd->row_count <= row_idx) {
            note_insert_line(nd, nd->row_count, "");
          }
        }

        AslanRow *r = &nd->rows[row_idx];
        size_t n = strlen(text);
        if (n + 1 > (size_t)r->cap) {
          r->chars = (char *)realloc(r->chars, n + 1);
          r->cap = (int)n + 1;
        }
        memcpy(r->chars, text, n);
        r->chars[n] = '\0';
        r->len = (int)n;

        // Mirror to callout if needed
        if (r->type == ASLAN_ROW_CALLOUT && r->block) {
          AslanCalloutBlock *cb = (AslanCalloutBlock *)r->block;
          if (n + 1 > (size_t)cb->cap) {
            cb->chars = (char *)realloc(cb->chars, n + 1);
            cb->cap = (int)n + 1;
          }
          memcpy(cb->chars, text, n);
          cb->chars[n] = '\0';
          cb->len = (int)n;
        }

        success = true;
        break;
      }
    }
  }

  // Commande: note_insert_block <block_type>
  // Inserts a specialized block (h1, h2, h3, separator) at the cursor position
  // in the active NOTE page.
  else if (strncmp(command_line, "note_insert_block ", 18) == 0) {
    const char *block_type_str = command_line + 18;
    AslanRowType block_type = ASLAN_ROW_TEXT;

    if (strcmp(block_type_str, "h1") == 0)
      block_type = ASLAN_ROW_HEADING_1;
    else if (strcmp(block_type_str, "h2") == 0)
      block_type = ASLAN_ROW_HEADING_2;
    else if (strcmp(block_type_str, "h3") == 0)
      block_type = ASLAN_ROW_HEADING_3;
    else if (strcmp(block_type_str, "separator") == 0)
      block_type = ASLAN_ROW_SEPARATOR;
    else if (strcmp(block_type_str, "callout") == 0)
      block_type = ASLAN_ROW_CALLOUT;
    else if (strcmp(block_type_str, "checklist") == 0)
      block_type = ASLAN_ROW_CHECKLIST;

    if (engine->doc) {
      uint32_t pid = doc_page_active_id(engine->doc);
      for (int i = 0; i < 32; i++) {
        if (!engine->doc->pages[i].in_use)
          continue;
        if (engine->doc->pages[i].page_id != pid)
          continue;
        if (engine->doc->pages[i].type != PAGE_TYPE_NOTE)
          break;

        AslanNoteDoc *nd = engine->doc->pages[i].note;
        if (!nd)
          break;

        // Insert a new row at cursor position
        int insert_pos = nd->cursor_row;
        if (insert_pos < 0)
          insert_pos = 0;
        if (insert_pos > nd->row_count)
          insert_pos = nd->row_count;

        // Grow array if needed
        if (nd->row_count >= nd->row_cap) {
          int new_cap = nd->row_cap ? nd->row_cap * 2 : 8;
          AslanRow *new_rows =
              (AslanRow *)realloc(nd->rows, (size_t)new_cap * sizeof(AslanRow));
          if (!new_rows)
            break;
          nd->rows = new_rows;
          nd->row_cap = new_cap;
        }

        // Shift rows down
        for (int j = nd->row_count; j > insert_pos; j--) {
          nd->rows[j] = nd->rows[j - 1];
        }

        // Initialize new block
        AslanRow *new_row = &nd->rows[insert_pos];
        new_row->type = block_type;
        new_row->chars = NULL;
        new_row->len = 0;
        new_row->cap = 0;
        new_row->block = NULL;

        // For specialized blocks, allocate a placeholder text
        const char *placeholder = "";
        if (block_type == ASLAN_ROW_HEADING_1)
          placeholder = "Heading 1";
        else if (block_type == ASLAN_ROW_HEADING_2)
          placeholder = "Heading 2";
        else if (block_type == ASLAN_ROW_HEADING_3)
          placeholder = "Heading 3";
        else if (block_type == ASLAN_ROW_SEPARATOR)
          placeholder = "---";
        else if (block_type == ASLAN_ROW_CALLOUT)
          placeholder = "Callout";
        else if (block_type == ASLAN_ROW_CHECKLIST)
          placeholder = "";

        size_t n = strlen(placeholder);
        new_row->chars = (char *)malloc(n + 1);
        if (new_row->chars) {
          memcpy(new_row->chars, placeholder, n);
          new_row->chars[n] = '\0';
          new_row->len = n;
          new_row->cap = n + 1;
        }

        // Callout payload
        if (block_type == ASLAN_ROW_CALLOUT) {
          AslanCalloutBlock *cb =
              (AslanCalloutBlock *)calloc(1, sizeof(AslanCalloutBlock));
          if (cb) {
            cb->theme = ASLAN_CALLOUT_INFO;
            cb->chars = (char *)malloc(n + 1);
            if (cb->chars) {
              memcpy(cb->chars, placeholder, n);
              cb->chars[n] = '\0';
              cb->len = n;
              cb->cap = n + 1;
            }
            new_row->block = cb;
          }
        }

        // Checklist payload
        if (block_type == ASLAN_ROW_CHECKLIST) {
          AslanChecklistBlock *cl =
              (AslanChecklistBlock *)calloc(1, sizeof(AslanChecklistBlock));
          if (cl) {
            cl->checked = 0;
            new_row->block = cl;
          }
        }

        nd->row_count++;
        nd->cursor_row = insert_pos;
        nd->cursor_col = 0;

        success = true;
        break;
      }
    }
  }

  // Commande: note_set_callout_theme <page_id> <row> <theme>
  // theme: 0..4 (Info, Star, Alert, Check, Idea)
  else if (strncmp(command_line, "note_set_callout_theme ", 23) == 0) {
    int page_id = 0;
    int row_idx = 0;
    int theme = 0;
    if (sscanf(command_line, "note_set_callout_theme %d %d %d", &page_id,
               &row_idx, &theme) == 3) {
      if (engine->doc) {
        for (int i = 0; i < 32; i++) {
          if (!engine->doc->pages[i].in_use)
            continue;
          if (engine->doc->pages[i].page_id != (uint32_t)page_id)
            continue;
          if (engine->doc->pages[i].type != PAGE_TYPE_NOTE)
            break;
          AslanNoteDoc *nd = engine->doc->pages[i].note;
          if (!nd)
            break;
          if (row_idx < 0 || row_idx >= nd->row_count) {
            success = true;
            break;
          }
          AslanRow *r = &nd->rows[row_idx];
          if (r->type != ASLAN_ROW_CALLOUT) {
            success = true;
            break;
          }
          AslanCalloutBlock *cb = (AslanCalloutBlock *)r->block;
          if (!cb) {
            cb = (AslanCalloutBlock *)calloc(1, sizeof(AslanCalloutBlock));
            if (cb) {
              cb->theme = ASLAN_CALLOUT_INFO;
              r->block = cb;
            }
          }
          if (cb) {
            if (theme < 0)
              theme = 0;
            if (theme > 4)
              theme = 4;
            cb->theme = (AslanCalloutTheme)theme;
          }
          success = true;
          break;
        }
      }
    }
  }

  // Commande: grid_set <page_id> <row> <col> <text>
  else if (strcmp(verb, "grid_set") == 0 && cmd.argc >= 5) {
    uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
    int r = atoi(cmd.argv[2]);
    int c = atoi(cmd.argv[3]);
    const char *text = cmd.argv[4];

    if (engine->doc) {
      for (int i = 0; i < 32; i++) {
        if (!engine->doc->pages[i].in_use ||
            engine->doc->pages[i].page_id != page_id)
          continue;

        if (engine->doc->pages[i].grid == NULL) {
          (void)doc_page_set_type(engine->doc, page_id, PAGE_TYPE_GRID);
        }
        AslanGrid *g = engine->doc->pages[i].grid;
        if (g) {
          grid_set_cell(g, r, c, text);
          success = true;
        }
        break;
      }
    }
  }

  // =========================
  // CLIP Commands (Video Sequence Editor)
  // =========================

  // Commande: clip_add <page_id> <path>
  else if (strcmp(verb, "clip_add") == 0 && cmd.argc >= 3) {
    uint32_t page_id = (uint32_t)atoi(cmd.argv[1]);
    const char *path = cmd.argv[2];

    if (engine->doc) {
      for (int i = 0; i < 32; i++) {
        if (!engine->doc->pages[i].in_use ||
            engine->doc->pages[i].page_id != page_id)
          continue;

        if (engine->doc->pages[i].clip == NULL) {
          (void)doc_page_set_type(engine->doc, page_id, PAGE_TYPE_CLIP);
        }
        AslanClipPage *cp = engine->doc->pages[i].clip;
        if (cp) {
          clip_add(cp, path);
          success = true;
        }
        break;
      }
    }
  }

  // Commande: clip_del <page_id> <index>
  else if (strncmp(command_line, "clip_del ", 9) == 0) {
    int page_id = 0;
    int index = 0;

    if (sscanf(command_line, "clip_del %d %d", &page_id, &index) == 2) {
      if (engine->doc) {
        for (int i = 0; i < 32; i++) {
          if (!engine->doc->pages[i].in_use)
            continue;
          if (engine->doc->pages[i].page_id != (uint32_t)page_id)
            continue;

          AslanClipPage *cp = engine->doc->pages[i].clip;
          if (cp) {
            clip_delete(cp, index);
          }
          success = true;
          break;
        }
      }
    }
  }

  // Commande: clip_move <page_id> <from> <to>
  else if (strncmp(command_line, "clip_move ", 10) == 0) {
    int page_id = 0;
    int from = 0;
    int to = 0;

    if (sscanf(command_line, "clip_move %d %d %d", &page_id, &from, &to) == 3) {
      if (engine->doc) {
        for (int i = 0; i < 32; i++) {
          if (!engine->doc->pages[i].in_use)
            continue;
          if (engine->doc->pages[i].page_id != (uint32_t)page_id)
            continue;

          AslanClipPage *cp = engine->doc->pages[i].clip;
          if (cp) {
            clip_move(cp, from, to);
          }
          success = true;
          break;
        }
      }
    }
  }

  // Commande: clip_split <page_id> <index> <time>
  else if (strncmp(command_line, "clip_split ", 11) == 0) {
    int page_id = 0;
    int index = 0;
    double split_time = 0.0;

    if (sscanf(command_line, "clip_split %d %d %lf", &page_id, &index,
               &split_time) == 3) {
      if (engine->doc) {
        for (int i = 0; i < 32; i++) {
          if (!engine->doc->pages[i].in_use)
            continue;
          if (engine->doc->pages[i].page_id != (uint32_t)page_id)
            continue;

          AslanClipPage *cp = engine->doc->pages[i].clip;
          if (cp) {
            clip_split(cp, index, split_time);
          }
          success = true;
          break;
        }
      }
    }
  }

  // Commande: clip_set_source <page_id> <index> "path"
  else if (strncmp(command_line, "clip_set_source ", 16) == 0) {
    int page_id = 0;
    int index = 0;
    char path[256];
    path[0] = 0;

    if (sscanf(command_line, "clip_set_source %d %d \"%255[^\"]\"", &page_id,
               &index, path) >= 2) {
      if (engine->doc) {
        for (int i = 0; i < 32; i++) {
          if (!engine->doc->pages[i].in_use)
            continue;
          if (engine->doc->pages[i].page_id != (uint32_t)page_id)
            continue;

          AslanClipPage *cp = engine->doc->pages[i].clip;
          if (cp) {
            clip_set_source(cp, index, path);
          }
          success = true;
          break;
        }
      }
    }
  }

  // Commande: clip_set_range <page_id> <index> <start> <end>
  else if (strncmp(command_line, "clip_set_range ", 15) == 0) {
    int page_id = 0;
    int index = 0;
    double start_time = 0.0;
    double end_time = 0.0;

    if (sscanf(command_line, "clip_set_range %d %d %lf %lf", &page_id, &index,
               &start_time, &end_time) == 4) {
      if (engine->doc) {
        for (int i = 0; i < 32; i++) {
          if (!engine->doc->pages[i].in_use)
            continue;
          if (engine->doc->pages[i].page_id != (uint32_t)page_id)
            continue;

          AslanClipPage *cp = engine->doc->pages[i].clip;
          if (cp) {
            clip_set_range(cp, index, start_time, end_time);
          }
          success = true;
          break;
        }
      }
    }
  }

  // Commande: clip_set_comment <page_id> <index> "comment"
  else if (strncmp(command_line, "clip_set_comment ", 17) == 0) {
    int page_id = 0;
    int index = 0;
    char comment[256];
    comment[0] = 0;

    if (sscanf(command_line, "clip_set_comment %d %d \"%255[^\"]\"", &page_id,
               &index, comment) >= 2) {
      if (engine->doc) {
        for (int i = 0; i < 32; i++) {
          if (!engine->doc->pages[i].in_use)
            continue;
          if (engine->doc->pages[i].page_id != (uint32_t)page_id)
            continue;

          AslanClipPage *cp = engine->doc->pages[i].clip;
          if (cp) {
            clip_set_comment(cp, index, comment);
          }
          success = true;
          break;
        }
      }
    }
  }

  else if (strncmp(command_line, "frame", 5) == 0) {
    float nx = 100.0f, ny = 100.0f, nw = 120.0f, nh = 120.0f;
    float ax, ay, aw, ah;
    if (sscanf(command_line, "frame %f %f %f %f", &ax, &ay, &aw, &ah) == 4) {
      nx = ax;
      ny = ay;
      nw = aw;
      nh = ah;
    }

    uint32_t new_id = engine->doc ? engine->doc->next_node_id : 1;
    snprintf(name, sizeof(name), "Frame%u", (unsigned)new_id);

    Node *node = doc_create_node(engine->doc, NODE_TYPE_RECT, name);
    if (node) {
      node->x = nx;
      node->y = ny;
      node->width = nw;
      node->height = nh;
      doc_add_child(engine->doc->root, node);
      snprintf(node->name, sizeof(node->name), "Frame%u", (unsigned)node->id);

      if (engine->ui_host)
        engine->ui_host->selected_node = node;
      printf(">> New frame created: %s (ID: %u)\n", node->name,
             (unsigned)node->id);
      success = true;

      if (engine->history_file) {
        fprintf(engine->history_file, "create_rect %u %s %f %f %f %f\n",
                (unsigned)node->id, node->name, node->x, node->y, node->width,
                node->height);
        fflush(engine->history_file);
      }

      clear_redo_log();
    }

    return success;
  }

  else if (strcmp(verb, "new_rect") == 0) {
    float nx = 100.0f, ny = 100.0f, nw = 120.0f, nh = 120.0f;
    if (cmd.argc >= 5) {
      nx = (float)atof(cmd.argv[1]);
      ny = (float)atof(cmd.argv[2]);
      nw = (float)atof(cmd.argv[3]);
      nh = (float)atof(cmd.argv[4]);
    }

    uint32_t new_id = engine->doc ? engine->doc->next_node_id : 1;
    char node_name[64];
    snprintf(node_name, sizeof(node_name), "Rect%u", (unsigned)new_id);

    Node *node = doc_create_node(engine->doc, NODE_TYPE_RECT, node_name);
    if (node) {
      node->x = nx;
      node->y = ny;
      node->width = nw;
      node->height = nh;
      doc_add_child(engine->doc->root, node);
      snprintf(node->name, sizeof(node->name), "Rect%u", (unsigned)node->id);

      if (engine->ui_host)
        engine->ui_host->selected_node = node;
      printf(">> New rect created: %s (ID: %u)\n", node->name,
             (unsigned)node->id);
      success = true;

      if (engine->history_file) {
        char canon[256];
        snprintf(canon, sizeof(canon), "create_rect %u \"%s\" %f %f %f %f",
                 (unsigned)node->id, node->name, node->x, node->y, node->width,
                 node->height);
        engine_emit_command(engine, canon);
      }
      clear_redo_log();
    }
    return success;
  }

  else if (strcmp(verb, "new_text") == 0) {
    float nx = 120.0f, ny = 120.0f, nw = 220.0f, nh = 60.0f;
    const char *content = "Text";
    if (cmd.argc >= 5) {
      nx = (float)atof(cmd.argv[1]);
      ny = (float)atof(cmd.argv[2]);
      nw = (float)atof(cmd.argv[3]);
      nh = (float)atof(cmd.argv[4]);
      if (cmd.argc >= 6)
        content = cmd.argv[5];
    }

    uint32_t new_id = engine->doc ? engine->doc->next_node_id : 1;
    char node_name[64];
    snprintf(node_name, sizeof(node_name), "Text%u", (unsigned)new_id);

    Node *node = doc_create_node(engine->doc, NODE_TYPE_TEXT, node_name);
    if (node) {
      node->x = nx;
      node->y = ny;
      node->width = nw;
      node->height = nh;
      strncpy(node->text, content, sizeof(node->text) - 1);
      doc_add_child(engine->doc->root, node);
      snprintf(node->name, sizeof(node->name), "Text%u", (unsigned)node->id);

      if (engine->ui_host)
        engine->ui_host->selected_node = node;
      success = true;

      if (engine->history_file) {
        char canon[512];
        snprintf(canon, sizeof(canon),
                 "create_text %u \"%s\" %f %f %f %f \"%s\"", (unsigned)node->id,
                 node->name, node->x, node->y, node->width, node->height,
                 node->text);
        engine_emit_command(engine, canon);
      }
      clear_redo_log();
    }
    return success;
  }

  // Commande: new_image x y w h "path"
  // Creates an IMAGE node and logs a single create_image line.
  else if (strncmp(command_line, "new_image", 9) == 0) {
    float nx = 100.0f, ny = 100.0f, nw = 160.0f, nh = 120.0f;
    char path[256];
    path[0] = 0;

    float ax, ay, aw, ah;
    int matched = sscanf(command_line, "new_image %f %f %f %f \"%255[^\"]\"",
                         &ax, &ay, &aw, &ah, path);
    if (matched >= 4) {
      nx = ax;
      ny = ay;
      nw = aw;
      nh = ah;
    }

    uint32_t new_id = engine->doc ? engine->doc->next_node_id : 1;
    snprintf(name, sizeof(name), "Image%u", (unsigned)new_id);

    Node *node = doc_create_node(engine->doc, NODE_TYPE_IMAGE, name);
    if (node) {
      node->x = nx;
      node->y = ny;
      node->width = nw;
      node->height = nh;
      if (path[0]) {
        strncpy(node->text, path, sizeof(node->text) - 1);
        node->text[sizeof(node->text) - 1] = 0;
      } else {
        node->text[0] = 0;
      }

      snprintf(node->name, sizeof(node->name), "Image%u", (unsigned)node->id);
      doc_add_child(engine->doc->root, node);

      if (engine->ui_host)
        engine->ui_host->selected_node = node;
      success = true;

      if (engine->history_file) {
        fprintf(engine->history_file, "create_image %u %s %f %f %f %f \"%s\"\n",
                (unsigned)node->id, node->name, node->x, node->y, node->width,
                node->height, node->text);
        fflush(engine->history_file);
      }
      clear_redo_log();
    }
    return success;
  }

  // Commande: rect <name> <x> <y> <w> <h>
  else if (sscanf(command_line, "rect %63s %f %f %f %f", name, &x, &y, &w,
                  &h) == 5) {
    Node *node = doc_create_node(engine->doc, NODE_TYPE_RECT, name);
    node->x = x;
    node->y = y;
    node->width = w;
    node->height = h;
    doc_add_child(engine->doc->root, node);
    printf(">> Created RECT '%s' (ID: %d)\n", name, node->id);
    success = true;
  }

  else if (sscanf(command_line, "create_frame %d %63s %f %f %f %f", &id, name,
                  &x, &y, &w, &h) == 6) {
    if (id > 0 && !doc_find_node_by_id(engine->doc, (uint32_t)id)) {
      Node *node = doc_create_node(engine->doc, NODE_TYPE_RECT, name);
      node->id = (uint32_t)id;

      if (engine->doc->next_node_id <= (uint32_t)id) {
        engine->doc->next_node_id = (uint32_t)id + 1;
      }

      node->x = x;
      node->y = y;
      node->width = w;
      node->height = h;
      doc_add_child(engine->doc->root, node);
      success = true;
    }
  }

  // Commande: create_rect <id> <name> <x> <y> <w> <h>
  else if (strcmp(verb, "create_rect") == 0 && cmd.argc >= 7) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    const char *node_name = cmd.argv[2];
    float nx = (float)atof(cmd.argv[3]);
    float ny = (float)atof(cmd.argv[4]);
    float nw = (float)atof(cmd.argv[5]);
    float nh = (float)atof(cmd.argv[6]);

    if (pid > 0 && !doc_find_node_by_id(engine->doc, pid)) {
      Node *node = doc_create_node(engine->doc, NODE_TYPE_RECT, node_name);
      node->id = pid;
      if (engine->doc->next_node_id <= pid)
        engine->doc->next_node_id = pid + 1;
      node->x = nx;
      node->y = ny;
      node->width = nw;
      node->height = nh;
      doc_add_child(engine->doc->root, node);
      success = true;
    }
  }

  else if (sscanf(command_line, "create_group %d %63s %f %f %f %f", &id, name,
                  &x, &y, &w, &h) == 6) {
    if (id > 0 && !doc_find_node_by_id(engine->doc, (uint32_t)id)) {
      Node *node = doc_create_node(engine->doc, NODE_TYPE_RECT, name);
      node->id = (uint32_t)id;

      if (engine->doc->next_node_id <= (uint32_t)id) {
        engine->doc->next_node_id = (uint32_t)id + 1;
      }

      node->x = x;
      node->y = y;
      node->width = w;
      node->height = h;
      doc_add_child(engine->doc->root, node);
      success = true;
    }
  }

  // Commande: create_text <id> <name> <x> <y> <w> <h> <content>
  else if (strcmp(verb, "create_text") == 0 && cmd.argc >= 7) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    const char *node_name = cmd.argv[2];
    float nx = (float)atof(cmd.argv[3]);
    float ny = (float)atof(cmd.argv[4]);
    float nw = (float)atof(cmd.argv[5]);
    float nh = (float)atof(cmd.argv[6]);
    const char *content = (cmd.argc >= 8) ? cmd.argv[7] : "";

    if (pid > 0 && !doc_find_node_by_id(engine->doc, pid)) {
      Node *node = doc_create_node(engine->doc, NODE_TYPE_TEXT, node_name);
      node->id = pid;
      if (engine->doc->next_node_id <= pid)
        engine->doc->next_node_id = pid + 1;
      node->x = nx;
      node->y = ny;
      node->width = nw;
      node->height = nh;
      strncpy(node->text, content, sizeof(node->text) - 1);
      doc_add_child(engine->doc->root, node);
      success = true;
    }
  }

  // Commande: create_image <id> <name> <x> <y> <w> <h> <path>
  else if (strcmp(verb, "create_image") == 0 && cmd.argc >= 7) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    const char *node_name = cmd.argv[2];
    float nx = (float)atof(cmd.argv[3]);
    float ny = (float)atof(cmd.argv[4]);
    float nw = (float)atof(cmd.argv[5]);
    float nh = (float)atof(cmd.argv[6]);
    const char *path = (cmd.argc >= 8) ? cmd.argv[7] : "";

    if (pid > 0 && !doc_find_node_by_id(engine->doc, pid)) {
      Node *node = doc_create_node(engine->doc, NODE_TYPE_IMAGE, node_name);
      node->id = pid;
      if (engine->doc->next_node_id <= pid)
        engine->doc->next_node_id = pid + 1;
      node->x = nx;
      node->y = ny;
      node->width = nw;
      node->height = nh;
      strncpy(node->text, path, sizeof(node->text) - 1);
      doc_add_child(engine->doc->root, node);
      success = true;
    }
  }

  // Commande: set_visible <id> <0|1>
  else if (sscanf(command_line, "set_visible %d %d", &id, &value) == 2) {
    if (id > 0) {
      success = doc_set_visible(engine->doc, (uint32_t)id, value);
    }
  }

  // Commande: bring_to_front <id>
  else if (strcmp(verb, "bring_to_front") == 0 && cmd.argc >= 2) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    success = doc_bring_to_front(engine->doc, pid);
    if (success)
      engine_emit_command(engine, command_line);
  }

  // Commande: send_to_back <id>
  else if (strcmp(verb, "send_to_back") == 0 && cmd.argc >= 2) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    success = doc_send_to_back(engine->doc, pid);
    if (success)
      engine_emit_command(engine, command_line);
  }

  // Commande: bring_forward <id>
  else if (strcmp(verb, "bring_forward") == 0 && cmd.argc >= 2) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    success = doc_bring_forward(engine->doc, pid);
    if (success)
      engine_emit_command(engine, command_line);
  }

  // Commande: send_backward <id>
  else if (strcmp(verb, "send_backward") == 0 && cmd.argc >= 2) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    success = doc_send_backward(engine->doc, pid);
    if (success)
      engine_emit_command(engine, command_line);
  }

  else if (sscanf(command_line, "group_attach %d %d", &id, &value) == 2) {
    if (id > 0 && value > 0) {
      Node *parent = doc_find_node_by_id(engine->doc, (uint32_t)id);
      Node *child = doc_find_node_by_id(engine->doc, (uint32_t)value);
      if (parent && child && parent != child && node_is_group(parent)) {
        success = doc_reparent_node(engine->doc, (uint32_t)value, (uint32_t)id);
      }
    }
  }

  // Commande: set_rect <id> <x> <y> <w> <h>
  else if (strcmp(verb, "set_rect") == 0 && cmd.argc >= 6) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    Node *node = doc_find_node_by_id(engine->doc, pid);
    if (node) {
      float ox = node->x, oy = node->y, ow = node->width, oh = node->height;
      node->x = (float)atof(cmd.argv[2]);
      node->y = (float)atof(cmd.argv[3]);
      node->width = (float)atof(cmd.argv[4]);
      node->height = (float)atof(cmd.argv[5]);
      doc_node_sync_spatial_index(engine->doc, node, ox, oy, ow, oh);
      success = true;

      if (engine->history_file) {
        char canon[256];
        snprintf(canon, sizeof(canon), "set_rect %u %f %f %f %f",
                 (unsigned)node->id, node->x, node->y, node->width,
                 node->height);
        prune_last_session_set_rect_and_append(canon);
      }
    }
  }

  // Commande: move <id> <x> <y>
  else if (strcmp(verb, "move") == 0 && cmd.argc >= 4) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    float nx = (float)atof(cmd.argv[2]);
    float ny = (float)atof(cmd.argv[3]);
    Node *node = doc_find_node_by_id(engine->doc, pid);
    if (node) {
      float dx = nx - node->x;
      float dy = ny - node->y;
      float ox = node->x, oy = node->y;
      node->x = nx;
      node->y = ny;
      doc_node_sync_spatial_index(engine->doc, node, ox, oy, node->width,
                                  node->height);
      if ((dx != 0.0f || dy != 0.0f) && node_is_group(node)) {
        translate_group_children(node, dx, dy);
      }
      success = true;
      if (success) {
        char canon[128];
        snprintf(canon, sizeof(canon), "move %u %f %f", (unsigned)node->id,
                 node->x, node->y);
        engine_emit_command(engine, canon);
      }
    }
  }

  // Commande: set_text <id> <content>
  else if (strcmp(verb, "set_text") == 0 && cmd.argc >= 3) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    Node *node = doc_find_node_by_id(engine->doc, pid);
    if (node && node->type == NODE_TYPE_TEXT) {
      strncpy(node->text, cmd.argv[2], sizeof(node->text) - 1);
      success = true;

      // Log to history
      if (engine->history_file) {
        char canon[1100];
        snprintf(canon, sizeof(canon), "set_text %u \"%s\"", (unsigned)node->id,
                 node->text);
        engine_emit_command(engine, canon);
      }
    }
  }

  // Commande: resize <id> <w> <h>
  else if (strcmp(verb, "resize") == 0 && cmd.argc >= 4) {
    uint32_t pid = (uint32_t)atoi(cmd.argv[1]);
    float nw = (float)atof(cmd.argv[2]);
    float nh = (float)atof(cmd.argv[3]);
    Node *node = doc_find_node_by_id(engine->doc, pid);
    if (node) {
      float ow = node->width, oh = node->height;
      node->width = nw;
      node->height = nh;
      doc_node_sync_spatial_index(engine->doc, node, node->x, node->y, ow, oh);
      success = true;
      if (success) {
        char canon[128];
        snprintf(canon, sizeof(canon), "resize %u %f %f", (unsigned)node->id,
                 node->width, node->height);
        engine_emit_command(engine, canon);
      }
    }
  }

  // Commande: select <id>
  else if (sscanf(command_line, "select %d", &id) == 1) {
    Node *node = doc_find_node_by_id(engine->doc, (uint32_t)id);
    if (node && engine->ui_host) {
      engine->ui_host->selected_node = node;
    }
    printf(">> Selected node ID: %d\n", id);
    success = true;
  }

  // Commande: list
  else if (strncmp(command_line, "list", 4) == 0) {
    doc_print_tree(engine->doc);
    success = true;
  }

  // Commande: help
  else if (strncmp(command_line, "help", 4) == 0) {
    printf("Available commands:\n");
    printf("  rect <name> <x> <y> <w> <h>  : Create a rectangle\n");
    printf("  new_rect [x y w h]           : Create a new rectangle (AOF: "
           "create_rect)\n");
    printf("  new_text [x y w h \"txt\"]    : Create a new text box (AOF: "
           "create_text)\n");
    printf("  set_rect <id> <x> <y> <w> <h>: Set full rect geometry\n");
    printf("  rewrite_aof                  : Compact history.log (snapshot)\n");
    printf("  move <id> <x> <y>            : Move a node (interactive; not "
           "logged)\n");
    printf("  resize <id> <w> <h>          : Resize a node (interactive; not "
           "logged)\n");
    printf("  select <id>                  : Select a node\n");
    printf("  undo                         : Undo last command\n");
    printf("  redo                         : Redo last undone command\n");
    printf("  clear                        : New empty document (clears "
           "history/redo)\n");
    printf("  list                         : Show document tree\n");
    printf("  exit                         : Quit application\n");
    success = true;
  }

  // Commande: rewrite_aof
  else if (strncmp(command_line, "rewrite_aof", 11) == 0) {
    success = engine_rewrite_aof(engine);
    if (success) {
      printf(">> AOF rewritten to '%s'\n", k_history_path);
    }
    return true; // pas de log
  }

  // Commande: redo
  else if (strncmp(command_line, "redo", 4) == 0) {
    // On prend la dernière commande mutante dans redo.log, on la remet dans
    // history.log, puis on replay.
    if (engine->history_file) {
      fclose(engine->history_file);
      engine->history_file = NULL;
    }

    FILE *rf = fopen(k_redo_path, "r");
    if (!rf)
      return true;

    char **lines = malloc(1000 * sizeof(char *));
    int count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), rf) && count < 1000) {
      lines[count++] = strdup(buffer);
    }
    fclose(rf);

    int last_mutating = -1;
    for (int i = 0; i < count; i++) {
      if (is_mutating_command_line(lines[i]))
        last_mutating = i;
    }

    if (last_mutating >= 0) {
      // Append to history
      FILE *hf = fopen(k_history_path, "a");
      if (hf) {
        // ensure single newline
        fprintf(hf, "%s", lines[last_mutating]);
        if (lines[last_mutating][strlen(lines[last_mutating]) - 1] != '\n') {
          fprintf(hf, "\n");
        }
        fclose(hf);
      }

      // Rewrite redo without that line
      rf = fopen(k_redo_path, "w");
      if (rf) {
        for (int i = 0; i < count; i++) {
          if (i == last_mutating)
            continue;
          fprintf(rf, "%s", lines[i]);
        }
        fclose(rf);
      }

      // Reload document from history
      doc_destroy(engine->doc);
      engine->doc = doc_create();
      engine_load_history(engine);

      if (engine->ui_host)
        engine->ui_host->selected_node = NULL;
      scripting_set_document(engine->doc);
    }

    for (int i = 0; i < count; i++)
      free(lines[i]);
    free(lines);

    engine->history_file = fopen(k_history_path, "a");
    return true;
  }

  // Commande: undo
  else if (strncmp(command_line, "undo", 4) == 0) {
    // L'undo dans un AOF est simple : on ferme le fichier, on le tronque d'une
    // ligne, et on recharge. Version améliorée: on retire la dernière commande
    // *mutante* (ignore commentaires / select / help / list ...)
    if (engine->history_file) {
      fclose(engine->history_file);
      engine->history_file = NULL;

      // Lecture de toutes les lignes (history)
      FILE *f = fopen(k_history_path, "r");
      if (f) {
        char **lines = malloc(1000 * sizeof(char *));
        int count = 0;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), f) && count < 1000) {
          lines[count++] = strdup(buffer);
        }
        fclose(f);

        // On cherche la dernière commande mutante
        int last_mutating = -1;
        for (int i = 0; i < count; i++) {
          if (is_mutating_command_line(lines[i]))
            last_mutating = i;
        }

        // Si on a trouvé une ligne mutante, on la pousse dans redo.log
        if (last_mutating >= 0) {
          FILE *redo = fopen(k_redo_path, "a");
          if (redo) {
            fprintf(redo, "%s", lines[last_mutating]);
            if (lines[last_mutating][strlen(lines[last_mutating]) - 1] !=
                '\n') {
              fprintf(redo, "\n");
            }
            fclose(redo);
          }
        }

        // Réécriture de toutes les lignes sauf la dernière commande mutante
        f = fopen(k_history_path, "w");
        if (f) {
          for (int i = 0; i < count; i++) {
            if (i == last_mutating)
              continue;
            fprintf(f, "%s", lines[i]);
          }
          fclose(f);
        }

        for (int i = 0; i < count; i++)
          free(lines[i]);
        free(lines);
      }

      // Réinitialiser le document et rejouer
      doc_destroy(engine->doc);
      engine->doc = doc_create();
      engine_load_history(engine);

      if (engine->ui_host)
        engine->ui_host->selected_node = NULL;
      scripting_set_document(engine->doc);

      // Réouvrir le log en append
      engine->history_file = fopen(k_history_path, "a");
    }
    return true; // On ne log pas l'undo lui-même
  }

  // Si c'est une commande mutante, toute nouvelle action invalide la pile redo
  if (success && is_mutating_command_line(command_line)) {
    clear_redo_log();
  }

  // Si la commande a réussi et n'est pas une commande de lecture (list/help),
  // on l'enregistre
  return success;
}

void engine_load_history(Engine *engine) {
  FILE *f = fopen(k_history_path, "r");
  if (!f)
    return;

  printf(">> Replaying history...\n");
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    // Enlever le saut de ligne
    line[strcspn(line, "\r\n")] = 0;

    // Ignorer les commentaires ou lignes vides
    if (line[0] == '-' || line[0] == '\0')
      continue;

    // Exécuter sans ré-enregistrer (on passe NULL pour history_file
    // temporairement ou on gère dans execute) Pour faire simple ici, on appelle
    // engine_execute_command mais on désactive le log
    FILE *temp = engine->history_file;
    engine->history_file = NULL;
    engine_execute_command(engine, line);
    engine->history_file = temp;
  }
  fclose(f);
  printf(">> History replayed.\n");
}
