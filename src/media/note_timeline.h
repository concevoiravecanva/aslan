#ifndef NOTE_TIMELINE_H
#define NOTE_TIMELINE_H

// =============================================================================
// PHASE 3: NOTE-TO-TIMELINE SYNC
// =============================================================================
// A specialized mode where text in a Note node acts as the "Script",
// and Clip nodes can be dragged into specific paragraphs to set the sequence.
//
// This enables the "Moodboard to Movie" workflow where writers can:
// 1. Write a script in a Note node
// 2. Drag video clips to specific paragraphs/sections
// 3. Generate a timeline based on the script structure
// 4. Export the sequence as EDL or timeline data

#include "../document/document.h"
#include <stdbool.h>

// =============================================================================
// SCRIPT SECTION
// =============================================================================
// A script section represents a paragraph/heading in a Note that can have
// clips associated with it.

typedef struct {
  int row_index;          // Index in the Note's rows array
  uint32_t linked_clip_id; // ID of associated Clip node (0 = none)
  double target_duration;  // Suggested duration for this section
  char marker[64];         // Custom marker/label for this section
  int sequence_order;      // Order in the final timeline (-1 = not in sequence)
} ScriptSection;

// =============================================================================
// TIMELINE SYNC STATE
// =============================================================================
// Manages the synchronization between a Note and associated Clips.

typedef struct {
  uint32_t note_id;        // The Note node being used as script
  
  // Script sections (one per meaningful row in the Note)
  ScriptSection *sections;
  int section_count;
  int section_cap;
  
  // Timeline state
  double total_duration;
  int clip_count;
  bool auto_sequence;      // Auto-arrange clips in row order
  
  // UI state
  int selected_section;
  bool is_active;
  
  // Export settings
  double default_clip_duration;
  double transition_duration;
} TimelineSync;

// =============================================================================
// API
// =============================================================================

// Initialize timeline sync for a Note node
TimelineSync *timeline_sync_create(Document *doc, uint32_t note_id);

// Destroy timeline sync state
void timeline_sync_destroy(TimelineSync *sync);

// Refresh sections from Note content (call after Note is edited)
void timeline_sync_refresh(TimelineSync *sync, Document *doc);

// Link a Clip node to a specific section
bool timeline_sync_link_clip(TimelineSync *sync, int section_idx, 
                             uint32_t clip_id);

// Unlink a Clip from a section
bool timeline_sync_unlink_clip(TimelineSync *sync, int section_idx);

// Get the Clip ID linked to a section (returns 0 if none)
uint32_t timeline_sync_get_clip(const TimelineSync *sync, int section_idx);

// Find which section a clip is linked to (returns -1 if not found)
int timeline_sync_find_section_for_clip(const TimelineSync *sync, 
                                        uint32_t clip_id);

// =============================================================================
// SEQUENCE GENERATION
// =============================================================================

// Generate sequence order based on Note structure
void timeline_sync_auto_sequence(TimelineSync *sync);

// Move a section in the sequence
bool timeline_sync_move_section(TimelineSync *sync, int from_idx, int to_idx);

// Calculate total duration of the sequence
double timeline_sync_calculate_duration(const TimelineSync *sync, 
                                        Document *doc);

// =============================================================================
// EXPORT
// =============================================================================

// EDL (Edit Decision List) format for professional video editing
typedef struct {
  char *content;
  size_t length;
} EdlData;

// Generate EDL from timeline
EdlData timeline_sync_export_edl(const TimelineSync *sync, Document *doc,
                                 const char *project_name, double fps);

// Free EDL data
void edl_data_free(EdlData *edl);

// Export timeline as simple text format
char *timeline_sync_export_text(const TimelineSync *sync, Document *doc);

// =============================================================================
// DRAG-DROP SUPPORT
// =============================================================================

// Check if a point is over a drop target section
// Returns section index or -1 if not over any section
int timeline_sync_hit_test(const TimelineSync *sync, Document *doc,
                           float x, float y);

// Handle drop of a clip node onto a section
bool timeline_sync_handle_drop(TimelineSync *sync, Document *doc,
                               uint32_t clip_id, float x, float y);

// =============================================================================
// UI HELPERS
// =============================================================================

// Get display info for a section
typedef struct {
  const char *text;        // Section text (first line)
  bool has_clip;           // Whether clip is linked
  double duration;         // Clip duration or default
  int sequence_pos;        // Position in sequence (0-based, -1 if not set)
  bool is_heading;         // Whether this is a heading row
} SectionDisplayInfo;

SectionDisplayInfo timeline_sync_get_section_info(const TimelineSync *sync,
                                                  Document *doc,
                                                  int section_idx);

#endif // NOTE_TIMELINE_H
