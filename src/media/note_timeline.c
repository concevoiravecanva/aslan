#include "note_timeline.h"
#include <stdio.h>
#include <stdlib.h>

// =============================================================================
// NOTE: All note timeline functionality disabled
// This file contains stubs for the old note-based timeline system which is
// incompatible with the new block-based document model.
// =============================================================================

TimelineSync *timeline_sync_create(Document *doc, uint32_t note_id) {
  (void)doc;
  (void)note_id;
  fprintf(stderr, "[NOTE_TIMELINE] Feature disabled - incompatible with new "
                  "document model\\n");
  return NULL;
}

void timeline_sync_destroy(TimelineSync *sync) { (void)sync; }

void timeline_sync_refresh(TimelineSync *sync, Document *doc) {
  (void)sync;
  (void)doc;
}

bool timeline_sync_link_clip(TimelineSync *sync, int section_idx,
                             uint32_t clip_id) {
  (void)sync;
  (void)section_idx;
  (void)clip_id;
  return false;
}

bool timeline_sync_unlink_clip(TimelineSync *sync, int section_idx) {
  (void)sync;
  (void)section_idx;
  return false;
}

uint32_t timeline_sync_get_clip(const TimelineSync *sync, int section_idx) {
  (void)sync;
  (void)section_idx;
  return 0;
}

int timeline_sync_find_section_for_clip(const TimelineSync *sync,
                                        uint32_t clip_id) {
  (void)sync;
  (void)clip_id;
  return -1;
}

void timeline_sync_auto_sequence(TimelineSync *sync) { (void)sync; }

bool timeline_sync_move_section(TimelineSync *sync, int from_idx, int to_idx) {
  (void)sync;
  (void)from_idx;
  (void)to_idx;
  return false;
}

double timeline_sync_calculate_duration(const TimelineSync *sync,
                                        Document *doc) {
  (void)sync;
  (void)doc;
  return 0.0;
}

EdlData timeline_sync_export_edl(const TimelineSync *sync, Document *doc,
                                 const char *project_name, double fps) {
  EdlData edl = {NULL, 0};
  (void)sync;
  (void)doc;
  (void)project_name;
  (void)fps;
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
  (void)sync;
  (void)doc;
  return NULL;
}

int timeline_sync_hit_test(const TimelineSync *sync, Document *doc, float x,
                           float y) {
  (void)sync;
  (void)doc;
  (void)x;
  (void)y;
  return -1;
}

bool timeline_sync_handle_drop(TimelineSync *sync, Document *doc,
                               uint32_t clip_id, float x, float y) {
  (void)sync;
  (void)doc;
  (void)clip_id;
  (void)x;
  (void)y;
  return false;
}

SectionDisplayInfo timeline_sync_get_section_info(const TimelineSync *sync,
                                                  Document *doc,
                                                  int section_idx) {
  SectionDisplayInfo info = {0};
  (void)sync;
  (void)doc;
  (void)section_idx;
  return info;
}
