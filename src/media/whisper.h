#ifndef WHISPER_HOST_H
#define WHISPER_HOST_H

#include <stdint.h>

// =============================================================================
// WHISPER TRANSCRIPTION MODULE
// =============================================================================
//
// Simple integration with whisper.cpp for audio/video transcription.
// Uses whisper-cli subprocess for simplicity (antirez philosophy).
//
// Usage:
//   1. Call whisper_transcribe_file() with audio/video path
//   2. Returns array of TranscriptSegment with timestamps
//   3. Free result with whisper_free_segments()
//
// =============================================================================

// A segment of transcribed text with timestamps
typedef struct TranscriptSegment {
  int start_ms;       // Start time in milliseconds
  int end_ms;         // End time in milliseconds
  char text[512];     // Transcribed text
  int word_count;     // Number of words in this segment
  int is_deleted;     // 1 if user deleted this segment
} TranscriptSegment;

// Transcription result
typedef struct TranscriptResult {
  TranscriptSegment *segments;
  int segment_count;
  int segment_capacity;
  char language[8];   // Detected language code
  int duration_ms;    // Total audio duration
  int success;        // 1 if transcription succeeded
  char error[256];    // Error message if failed
} TranscriptResult;

// =============================================================================
// API
// =============================================================================

// Initialize whisper paths (call once at startup)
// whisper_bin: path to whisper-cli binary
// model_path: path to ggml model file
void whisper_init(const char *whisper_bin, const char *model_path);

// Transcribe an audio or video file
// Returns a TranscriptResult (caller must free with whisper_free_result)
// For video files, audio is extracted first using ffmpeg
TranscriptResult *whisper_transcribe_file(const char *filepath);

// Transcribe audio data directly (16kHz mono float32 PCM)
TranscriptResult *whisper_transcribe_audio(const float *samples, int n_samples);

// Free transcription result
void whisper_free_result(TranscriptResult *result);

// =============================================================================
// UTILITY
// =============================================================================

// Check if whisper is available (binary and model exist)
int whisper_is_available(void);

// Get total duration from non-deleted segments
int whisper_get_active_duration(const TranscriptResult *result);

// Export edited transcript as video (using ffmpeg)
// Cuts out deleted segments and re-encodes
int whisper_export_edited_video(const char *input_path,
                                 const char *output_path,
                                 const TranscriptResult *result);

#endif // WHISPER_HOST_H
