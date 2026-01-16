#include "note_timeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// PHASE 3: NOTE-TO-TIMELINE SYNC
// =============================================================================

// =============================================================================
// CREATION & DESTRUCTION
// =============================================================================

TimelineSync *timeline_sync_create(Document *doc, uint32_t note_id) {
  if (!doc)
    return NULL;

  // Verify node exists and is a Note
  Node *node = doc_find_node_by_id(doc, note_id);
  if (!node || node->type != NODE_TYPE_NOTE)
    return NULL;

  TimelineSync *sync = (TimelineSync *)calloc(1, sizeof(TimelineSync));
  if (!sync)
    return NULL;

  sync->note_id = note_id;
  sync->section_cap = 32;
  sync->sections = (ScriptSection *)calloc((size_t)sync->section_cap,
                                           sizeof(ScriptSection));
  if (!sync->sections) {
    free(sync);
    return NULL;
  }

  sync->section_count = 0;
  sync->selected_section = -1;
  sync->is_active = true;
  sync->auto_sequence = true;
  sync->default_clip_duration = 5.0;
  sync->transition_duration = 0.5;

  // Initialize from Note content
  timeline_sync_refresh(sync, doc);

  return sync;
}

void timeline_sync_destroy(TimelineSync *sync) {
  if (!sync)
    return;
  free(sync->sections);
  free(sync);
}

// =============================================================================
// SYNC WITH NOTE CONTENT
// =============================================================================

void timeline_sync_refresh(TimelineSync *sync, Document *doc) {
  if (!sync || !doc)
    return;

  Node *note_node = doc_find_node_by_id(doc, sync->note_id);
  if (!note_node || note_node->type != NODE_TYPE_NOTE)
    return;

  NotePayload *note = node_get_note_payload(note_node);
  if (!note)
    return;

  // Save existing clip links
  uint32_t *saved_clips = NULL;
  int *saved_rows = NULL;
  int saved_count = sync->section_count;

  if (saved_count > 0) {
    saved_clips = (uint32_t *)calloc((size_t)saved_count, sizeof(uint32_t));
    saved_rows = (int *)calloc((size_t)saved_count, sizeof(int));
    for (int i = 0; i < saved_count; i++) {
      saved_clips[i] = sync->sections[i].linked_clip_id;
      saved_rows[i] = sync->sections[i].row_index;
    }
  }

  // Clear existing sections
  sync->section_count = 0;

  // Create sections from Note rows
  // Each row that isn't empty becomes a potential section
  for (int r = 0; r < note->row_count; r++) {
    AslanRow *row = &note->rows[r];
    if (!row->chars || row->len == 0)
      continue;

    // Skip separator rows
    if (row->type == ASLAN_ROW_SEPARATOR)
      continue;

    // Ensure capacity
    if (sync->section_count >= sync->section_cap) {
      sync->section_cap *= 2;
      sync->sections = (ScriptSection *)realloc(
          sync->sections, (size_t)sync->section_cap * sizeof(ScriptSection));
    }

    ScriptSection *section = &sync->sections[sync->section_count];
    memset(section, 0, sizeof(ScriptSection));

    section->row_index = r;
    section->linked_clip_id = 0;
    section->target_duration = sync->default_clip_duration;
    section->sequence_order = sync->auto_sequence ? sync->section_count : -1;

    // Try to restore previous clip link if row index matches
    if (saved_clips && saved_rows) {
      for (int i = 0; i < saved_count; i++) {
        if (saved_rows[i] == r && saved_clips[i] != 0) {
          section->linked_clip_id = saved_clips[i];
          break;
        }
      }
    }

    sync->section_count++;
  }

  free(saved_clips);
  free(saved_rows);

  // Recalculate duration
  sync->total_duration = timeline_sync_calculate_duration(sync, doc);
}

// =============================================================================
// CLIP LINKING
// =============================================================================

bool timeline_sync_link_clip(TimelineSync *sync, int section_idx,
                             uint32_t clip_id) {
  if (!sync)
    return false;
  if (section_idx < 0 || section_idx >= sync->section_count)
    return false;

  // Check if clip is already linked elsewhere
  int existing = timeline_sync_find_section_for_clip(sync, clip_id);
  if (existing >= 0 && existing != section_idx) {
    // Unlink from previous section
    sync->sections[existing].linked_clip_id = 0;
  }

  sync->sections[section_idx].linked_clip_id = clip_id;
  sync->clip_count = 0;

  // Recount clips
  for (int i = 0; i < sync->section_count; i++) {
    if (sync->sections[i].linked_clip_id != 0) {
      sync->clip_count++;
    }
  }

  return true;
}

bool timeline_sync_unlink_clip(TimelineSync *sync, int section_idx) {
  if (!sync)
    return false;
  if (section_idx < 0 || section_idx >= sync->section_count)
    return false;

  if (sync->sections[section_idx].linked_clip_id != 0) {
    sync->sections[section_idx].linked_clip_id = 0;
    sync->clip_count--;
  }

  return true;
}

uint32_t timeline_sync_get_clip(const TimelineSync *sync, int section_idx) {
  if (!sync)
    return 0;
  if (section_idx < 0 || section_idx >= sync->section_count)
    return 0;
  return sync->sections[section_idx].linked_clip_id;
}

int timeline_sync_find_section_for_clip(const TimelineSync *sync,
                                        uint32_t clip_id) {
  if (!sync || clip_id == 0)
    return -1;

  for (int i = 0; i < sync->section_count; i++) {
    if (sync->sections[i].linked_clip_id == clip_id) {
      return i;
    }
  }
  return -1;
}

// =============================================================================
// SEQUENCE MANAGEMENT
// =============================================================================

void timeline_sync_auto_sequence(TimelineSync *sync) {
  if (!sync)
    return;

  int seq = 0;
  for (int i = 0; i < sync->section_count; i++) {
    if (sync->sections[i].linked_clip_id != 0) {
      sync->sections[i].sequence_order = seq++;
    } else {
      sync->sections[i].sequence_order = -1;
    }
  }
}

bool timeline_sync_move_section(TimelineSync *sync, int from_idx, int to_idx) {
  if (!sync)
    return false;
  if (from_idx < 0 || from_idx >= sync->section_count)
    return false;
  if (to_idx < 0 || to_idx >= sync->section_count)
    return false;
  if (from_idx == to_idx)
    return true;

  int from_seq = sync->sections[from_idx].sequence_order;
  int to_seq = sync->sections[to_idx].sequence_order;

  if (from_seq < 0 || to_seq < 0)
    return false;

  // Swap sequence orders
  sync->sections[from_idx].sequence_order = to_seq;
  sync->sections[to_idx].sequence_order = from_seq;

  return true;
}

double timeline_sync_calculate_duration(const TimelineSync *sync,
                                        Document *doc) {
  if (!sync || !doc)
    return 0.0;

  double total = 0.0;
  int clips_with_duration = 0;

  for (int i = 0; i < sync->section_count; i++) {
    if (sync->sections[i].sequence_order < 0)
      continue;

    uint32_t clip_id = sync->sections[i].linked_clip_id;
    if (clip_id == 0) {
      // Use target duration for sections without clips
      total += sync->sections[i].target_duration;
    } else {
      // Get actual clip duration
      Node *clip_node = doc_find_node_by_id(doc, clip_id);
      if (clip_node && clip_node->type == NODE_TYPE_CLIP) {
        ClipPayload *clip = node_get_clip_payload(clip_node);
        if (clip && clip->clip_count > 0) {
          total += clip_page_total_duration(clip);
          clips_with_duration++;
        } else {
          total += sync->sections[i].target_duration;
        }
      } else {
        total += sync->sections[i].target_duration;
      }
    }

    // Add transition time between clips
    if (clips_with_duration > 0) {
      total += sync->transition_duration;
    }
  }

  return total;
}

// =============================================================================
// EDL EXPORT
// =============================================================================

// Format timecode from seconds (for EDL)
static void format_timecode(double seconds, double fps, char *out,
                            size_t out_size) {
  int frames_total = (int)(seconds * fps);
  int f = frames_total % (int)fps;
  int s = (frames_total / (int)fps) % 60;
  int m = (frames_total / (int)fps / 60) % 60;
  int h = frames_total / (int)fps / 3600;

  snprintf(out, out_size, "%02d:%02d:%02d:%02d", h, m, s, f);
}

EdlData timeline_sync_export_edl(const TimelineSync *sync, Document *doc,
                                 const char *project_name, double fps) {
  EdlData edl = {NULL, 0};

  if (!sync || !doc)
    return edl;

  // Build sorted sequence
  typedef struct {
    int section_idx;
    int seq_order;
  } SeqEntry;

  SeqEntry *sorted = (SeqEntry *)calloc((size_t)sync->section_count,
                                        sizeof(SeqEntry));
  int sorted_count = 0;

  for (int i = 0; i < sync->section_count; i++) {
    if (sync->sections[i].sequence_order >= 0) {
      sorted[sorted_count].section_idx = i;
      sorted[sorted_count].seq_order = sync->sections[i].sequence_order;
      sorted_count++;
    }
  }

  // Sort by sequence order
  for (int i = 0; i < sorted_count - 1; i++) {
    for (int j = i + 1; j < sorted_count; j++) {
      if (sorted[j].seq_order < sorted[i].seq_order) {
        SeqEntry tmp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = tmp;
      }
    }
  }

  // Allocate buffer
  size_t buf_size = 16384;
  char *buf = (char *)malloc(buf_size);
  if (!buf) {
    free(sorted);
    return edl;
  }

  size_t pos = 0;

  // EDL Header
  pos += (size_t)snprintf(buf + pos, buf_size - pos,
                          "TITLE: %s\n", project_name ? project_name : "Aslan Export");
  pos += (size_t)snprintf(buf + pos, buf_size - pos, "FCM: NON-DROP FRAME\n\n");

  // Generate edits
  double record_time = 0.0;
  int edit_num = 1;

  for (int i = 0; i < sorted_count && pos < buf_size - 512; i++) {
    int sec_idx = sorted[i].section_idx;
    ScriptSection *section = &sync->sections[sec_idx];

    double duration = section->target_duration;
    const char *source_name = "BL"; // Black/placeholder

    // Get clip info if linked
    if (section->linked_clip_id != 0) {
      Node *clip_node = doc_find_node_by_id(doc, section->linked_clip_id);
      if (clip_node && clip_node->type == NODE_TYPE_CLIP) {
        ClipPayload *clip = node_get_clip_payload(clip_node);
        if (clip && clip->clip_count > 0) {
          duration = clip_page_total_duration(clip);
          source_name = clip_node->name[0] ? clip_node->name : "CLIP";
        }
      }
    }

    // Format timecodes
    char src_in[16], src_out[16], rec_in[16], rec_out[16];
    format_timecode(0.0, fps, src_in, sizeof(src_in));
    format_timecode(duration, fps, src_out, sizeof(src_out));
    format_timecode(record_time, fps, rec_in, sizeof(rec_in));
    format_timecode(record_time + duration, fps, rec_out, sizeof(rec_out));

    // Write EDL edit line
    // Format: EDIT# REEL TRACK TRANS SRC_IN SRC_OUT REC_IN REC_OUT
    pos += (size_t)snprintf(buf + pos, buf_size - pos,
                            "%03d  %-8s V     C        %s %s %s %s\n",
                            edit_num, source_name, src_in, src_out, rec_in, rec_out);

    // Add comment with section text
    Node *note_node = doc_find_node_by_id(doc, sync->note_id);
    if (note_node) {
      NotePayload *note = node_get_note_payload(note_node);
      if (note && section->row_index < note->row_count) {
        AslanRow *row = &note->rows[section->row_index];
        if (row->chars && row->len > 0) {
          // Truncate for comment
          char comment[64];
          size_t len = row->len < 60 ? row->len : 60;
          strncpy(comment, row->chars, len);
          comment[len] = '\0';
          pos += (size_t)snprintf(buf + pos, buf_size - pos,
                                  "* FROM CLIP NAME: %s\n", comment);
        }
      }
    }

    pos += (size_t)snprintf(buf + pos, buf_size - pos, "\n");

    record_time += duration;
    edit_num++;
  }

  free(sorted);

  edl.content = buf;
  edl.length = pos;

  return edl;
}

void edl_data_free(EdlData *edl) {
  if (!edl)
    return;
  free(edl->content);
  edl->content = NULL;
  edl->length = 0;
}

char *timeline_sync_export_text(const TimelineSync *sync, Document *doc) {
  if (!sync || !doc)
    return NULL;

  size_t buf_size = 8192;
  char *buf = (char *)malloc(buf_size);
  if (!buf)
    return NULL;

  size_t pos = 0;

  pos += (size_t)snprintf(buf + pos, buf_size - pos,
                          "=== TIMELINE EXPORT ===\n\n");

  Node *note_node = doc_find_node_by_id(doc, sync->note_id);
  NotePayload *note = note_node ? node_get_note_payload(note_node) : NULL;

  double time = 0.0;

  for (int i = 0; i < sync->section_count && pos < buf_size - 256; i++) {
    ScriptSection *section = &sync->sections[i];

    if (section->sequence_order < 0)
      continue;

    // Get section text
    const char *text = "";
    if (note && section->row_index < note->row_count) {
      AslanRow *row = &note->rows[section->row_index];
      if (row->chars)
        text = row->chars;
    }

    // Get clip info
    const char *clip_name = "(no clip)";
    double duration = section->target_duration;

    if (section->linked_clip_id != 0) {
      Node *clip_node = doc_find_node_by_id(doc, section->linked_clip_id);
      if (clip_node) {
        clip_name = clip_node->name;
        ClipPayload *clip = node_get_clip_payload(clip_node);
        if (clip && clip->clip_count > 0) {
          duration = clip_page_total_duration(clip);
        }
      }
    }

    // Format time
    int mins = (int)(time / 60);
    double secs = time - mins * 60;

    pos += (size_t)snprintf(buf + pos, buf_size - pos,
                            "[%02d:%05.2f] %.50s\n", mins, secs, text);
    pos += (size_t)snprintf(buf + pos, buf_size - pos,
                            "           Clip: %s (%.1fs)\n\n",
                            clip_name, duration);

    time += duration;
  }

  pos += (size_t)snprintf(buf + pos, buf_size - pos,
                          "\nTotal Duration: %.1f seconds\n", time);

  return buf;
}

// =============================================================================
// DRAG-DROP SUPPORT
// =============================================================================

int timeline_sync_hit_test(const TimelineSync *sync, Document *doc,
                           float x, float y) {
  if (!sync || !doc)
    return -1;

  Node *note_node = doc_find_node_by_id(doc, sync->note_id);
  if (!note_node)
    return -1;

  // Check if point is within Note bounds
  if (x < note_node->x || x > note_node->x + note_node->width)
    return -1;
  if (y < note_node->y || y > note_node->y + note_node->height)
    return -1;

  // Calculate which section based on Y position
  // Assuming equal row heights for simplicity
  float row_height = 24.0f;
  float header_height = 28.0f;

  float rel_y = y - note_node->y - header_height;
  if (rel_y < 0)
    return -1;

  int row = (int)(rel_y / row_height);

  // Find section for this row
  for (int i = 0; i < sync->section_count; i++) {
    if (sync->sections[i].row_index == row) {
      return i;
    }
  }

  return -1;
}

bool timeline_sync_handle_drop(TimelineSync *sync, Document *doc,
                               uint32_t clip_id, float x, float y) {
  if (!sync || !doc || clip_id == 0)
    return false;

  // Verify it's a Clip node
  Node *clip_node = doc_find_node_by_id(doc, clip_id);
  if (!clip_node || clip_node->type != NODE_TYPE_CLIP)
    return false;

  int section_idx = timeline_sync_hit_test(sync, doc, x, y);
  if (section_idx < 0)
    return false;

  return timeline_sync_link_clip(sync, section_idx, clip_id);
}

// =============================================================================
// UI HELPERS
// =============================================================================

SectionDisplayInfo timeline_sync_get_section_info(const TimelineSync *sync,
                                                  Document *doc,
                                                  int section_idx) {
  SectionDisplayInfo info = {0};

  if (!sync || !doc)
    return info;
  if (section_idx < 0 || section_idx >= sync->section_count)
    return info;

  ScriptSection *section = &sync->sections[section_idx];

  // Get text from Note
  Node *note_node = doc_find_node_by_id(doc, sync->note_id);
  if (note_node) {
    NotePayload *note = node_get_note_payload(note_node);
    if (note && section->row_index < note->row_count) {
      AslanRow *row = &note->rows[section->row_index];
      info.text = row->chars;
      info.is_heading = (row->type == ASLAN_ROW_HEADING_1 ||
                         row->type == ASLAN_ROW_HEADING_2 ||
                         row->type == ASLAN_ROW_HEADING_3);
    }
  }

  info.has_clip = (section->linked_clip_id != 0);
  info.duration = section->target_duration;
  info.sequence_pos = section->sequence_order;

  // Get actual clip duration if linked
  if (info.has_clip) {
    Node *clip_node = doc_find_node_by_id(doc, section->linked_clip_id);
    if (clip_node) {
      ClipPayload *clip = node_get_clip_payload(clip_node);
      if (clip && clip->clip_count > 0) {
        info.duration = clip_page_total_duration(clip);
      }
    }
  }

  return info;
}
