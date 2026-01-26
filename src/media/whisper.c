#include "whisper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

// =============================================================================
// WHISPER TRANSCRIPTION MODULE
// =============================================================================

static char g_whisper_bin[512] = "";
static char g_model_path[512] = "";

// Async transcription state
typedef struct {
  char filepath[512];
  TranscriptResult *result;
  int is_running;
  int is_done;
  pthread_t thread;
} AsyncTranscription;

static AsyncTranscription g_async = {0};

void whisper_init(const char *whisper_bin, const char *model_path) {
  if (whisper_bin)
    strncpy(g_whisper_bin, whisper_bin, sizeof(g_whisper_bin) - 1);
  if (model_path)
    strncpy(g_model_path, model_path, sizeof(g_model_path) - 1);
}

int whisper_is_available(void) {
  struct stat st;
  if (stat(g_whisper_bin, &st) != 0)
    return 0;
  if (stat(g_model_path, &st) != 0)
    return 0;
  return 1;
}

// Simple JSON string extraction (antirez-style minimal parser)
static int json_extract_string(const char *json, const char *key, char *out,
                               int out_size) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);

  const char *p = strstr(json, pattern);
  if (!p)
    return 0;

  p += strlen(pattern);
  while (*p && (*p == ' ' || *p == ':' || *p == '\t'))
    p++;

  if (*p != '"')
    return 0;
  p++; // skip opening quote

  int i = 0;
  while (*p && *p != '"' && i < out_size - 1) {
    if (*p == '\\' && *(p + 1)) {
      p++; // skip escape
    }
    out[i++] = *p++;
  }
  out[i] = '\0';
  return 1;
}

// Extract integer from JSON
static int json_extract_int(const char *json, const char *key) {
  char pattern[128];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);

  const char *p = strstr(json, pattern);
  if (!p)
    return 0;

  p += strlen(pattern);
  while (*p && (*p == ' ' || *p == ':' || *p == '\t'))
    p++;

  return atoi(p);
}

// Parse timestamp "HH:MM:SS,mmm" to milliseconds
static int parse_timestamp(const char *ts) {
  int h = 0, m = 0, s = 0, ms = 0;
  if (sscanf(ts, "%d:%d:%d,%d", &h, &m, &s, &ms) == 4) {
    return h * 3600000 + m * 60000 + s * 1000 + ms;
  }
  return 0;
}

// Check if file is video (needs audio extraction)
static int is_video_file(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return 0;
  ext++;
  return (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "mov") == 0 ||
          strcasecmp(ext, "avi") == 0 || strcasecmp(ext, "mkv") == 0 ||
          strcasecmp(ext, "webm") == 0);
}

// Extract audio from video using ffmpeg
static int extract_audio(const char *video_path, char *wav_path,
                         int wav_path_size) {
  // Create temp wav file
  snprintf(wav_path, wav_path_size, "/tmp/aslan_whisper_%d.wav", getpid());

  // Extract full audio track to 16kHz mono WAV
  // -vn: no video
  // -acodec pcm_s16le: PCM 16-bit (compatible with whisper)
  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "ffmpeg -y -i \"%s\" -vn -ar 16000 -ac 1 -acodec pcm_s16le \"%s\" 2>&1",
           video_path, wav_path);

  int ret = system(cmd);

  // Check if file was created and has content
  struct stat st;
  if (ret == 0 && stat(wav_path, &st) == 0) {
    fprintf(stderr, "[whisper] Extracted audio: %ld bytes (%.1f seconds)\n",
            st.st_size, (float)st.st_size / (16000 * 2)); // 16kHz * 2 bytes per sample
    return 1;
  }
  return 0;
}

// Parse whisper JSON output
static TranscriptResult *parse_whisper_json(const char *json_path) {
  FILE *f = fopen(json_path, "r");
  if (!f)
    return NULL;

  // Read entire file
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *json = (char *)malloc(size + 1);
  if (!json) {
    fclose(f);
    return NULL;
  }
  fread(json, 1, size, f);
  json[size] = '\0';
  fclose(f);

  // Allocate result
  TranscriptResult *result =
      (TranscriptResult *)calloc(1, sizeof(TranscriptResult));
  if (!result) {
    free(json);
    return NULL;
  }

  result->segment_capacity = 64;
  result->segments = (TranscriptSegment *)calloc(result->segment_capacity,
                                                  sizeof(TranscriptSegment));

  // Extract language
  json_extract_string(json, "language", result->language,
                      sizeof(result->language));

  // Find transcription array
  const char *transcription = strstr(json, "\"transcription\"");
  if (!transcription) {
    free(json);
    result->success = 0;
    strncpy(result->error, "No transcription found", sizeof(result->error) - 1);
    return result;
  }

  // Parse each segment - look for "timestamps" objects which mark each segment
  // JSON structure: { "timestamps": { "from": "...", "to": "..." }, "offsets": { "from": ms, "to": ms }, "text": "..." }
  const char *p = transcription;
  while ((p = strstr(p, "\"timestamps\"")) != NULL) {
    // Grow array if needed
    if (result->segment_count >= result->segment_capacity) {
      result->segment_capacity *= 2;
      result->segments = (TranscriptSegment *)realloc(
          result->segments,
          result->segment_capacity * sizeof(TranscriptSegment));
    }

    TranscriptSegment *seg = &result->segments[result->segment_count];
    memset(seg, 0, sizeof(TranscriptSegment));

    // Find the offsets object (contains milliseconds)
    const char *offsets = strstr(p, "\"offsets\"");
    if (offsets && offsets < p + 500) { // Must be nearby
      seg->start_ms = json_extract_int(offsets, "from");
      seg->end_ms = json_extract_int(offsets, "to");
    } else {
      // Fallback to timestamp parsing from "timestamps" object
      char ts_from[32], ts_to[32];
      if (json_extract_string(p, "from", ts_from, sizeof(ts_from))) {
        seg->start_ms = parse_timestamp(ts_from);
      }
      if (json_extract_string(p, "to", ts_to, sizeof(ts_to))) {
        seg->end_ms = parse_timestamp(ts_to);
      }
    }

    // Extract text (should be after offsets)
    const char *text_start = strstr(p, "\"text\"");
    if (text_start && text_start < p + 600) {
      json_extract_string(text_start, "text", seg->text, sizeof(seg->text));

      // Count words
      const char *w = seg->text;
      while (*w) {
        while (*w && *w == ' ')
          w++;
        if (*w) {
          seg->word_count++;
          while (*w && *w != ' ')
            w++;
        }
      }
    }

    if (seg->end_ms > result->duration_ms) {
      result->duration_ms = seg->end_ms;
    }

    // Only add segment if it has text
    if (seg->text[0] != '\0') {
      result->segment_count++;
    }

    p += 12; // Move past "timestamps" to avoid re-finding it
  }

  free(json);
  result->success = (result->segment_count > 0);
  return result;
}

TranscriptResult *whisper_transcribe_file(const char *filepath) {
  if (!whisper_is_available()) {
    TranscriptResult *result =
        (TranscriptResult *)calloc(1, sizeof(TranscriptResult));
    strncpy(result->error, "Whisper not available", sizeof(result->error) - 1);
    return result;
  }

  char wav_path[512] = "";
  const char *audio_path = filepath;
  int cleanup_wav = 0;

  // Extract audio from video if needed
  if (is_video_file(filepath)) {
    if (!extract_audio(filepath, wav_path, sizeof(wav_path))) {
      TranscriptResult *result =
          (TranscriptResult *)calloc(1, sizeof(TranscriptResult));
      strncpy(result->error, "Failed to extract audio",
              sizeof(result->error) - 1);
      return result;
    }
    audio_path = wav_path;
    cleanup_wav = 1;
  }

  // Run whisper-cli with JSON output
  // Options:
  // -l auto: auto-detect language
  // -oj: output JSON
  // -t 4: use 4 threads for speed
  // -pp: print progress (helpful for debugging)
  // -mc -1: unlimited context (process full audio)
  char json_path[512];
  snprintf(json_path, sizeof(json_path), "%s.json", audio_path);

  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "\"%s\" -m \"%s\" -f \"%s\" -l auto -t 4 -mc -1 -oj 2>&1",
           g_whisper_bin, g_model_path, audio_path);

  fprintf(stderr, "[whisper] Running: %s\n", cmd);
  int ret = system(cmd);
  fprintf(stderr, "[whisper] Exit code: %d\n", ret);

  // Parse JSON output
  TranscriptResult *result = NULL;
  if (ret == 0) {
    result = parse_whisper_json(json_path);
    unlink(json_path); // Cleanup JSON file
  } else {
    result = (TranscriptResult *)calloc(1, sizeof(TranscriptResult));
    strncpy(result->error, "Whisper transcription failed",
            sizeof(result->error) - 1);
  }

  // Cleanup temp wav
  if (cleanup_wav && wav_path[0]) {
    unlink(wav_path);
  }

  return result;
}

void whisper_free_result(TranscriptResult *result) {
  if (!result)
    return;
  if (result->segments)
    free(result->segments);
  free(result);
}

// =============================================================================
// ASYNC TRANSCRIPTION (non-blocking)
// =============================================================================

static void *transcription_thread(void *arg) {
  (void)arg;
  g_async.result = whisper_transcribe_file(g_async.filepath);
  g_async.is_running = 0;
  g_async.is_done = 1;
  return NULL;
}

int whisper_transcribe_async(const char *filepath) {
  // Don't start if already running
  if (g_async.is_running) {
    return 0;
  }

  // Clear previous state
  if (g_async.result) {
    whisper_free_result(g_async.result);
    g_async.result = NULL;
  }

  strncpy(g_async.filepath, filepath, sizeof(g_async.filepath) - 1);
  g_async.is_running = 1;
  g_async.is_done = 0;

  if (pthread_create(&g_async.thread, NULL, transcription_thread, NULL) != 0) {
    g_async.is_running = 0;
    return 0;
  }

  pthread_detach(g_async.thread);
  return 1;
}

int whisper_is_transcribing(void) {
  return g_async.is_running;
}

TranscriptResult *whisper_get_async_result(void) {
  if (!g_async.is_done) {
    return NULL;
  }
  TranscriptResult *result = g_async.result;
  g_async.result = NULL;
  g_async.is_done = 0;
  return result;
}

int whisper_get_active_duration(const TranscriptResult *result) {
  if (!result)
    return 0;

  int duration = 0;
  for (int i = 0; i < result->segment_count; i++) {
    if (!result->segments[i].is_deleted) {
      duration += (result->segments[i].end_ms - result->segments[i].start_ms);
    }
  }
  return duration;
}

int whisper_export_edited_video(const char *input_path, const char *output_path,
                                const TranscriptResult *result) {
  if (!result || result->segment_count == 0)
    return 0;

  // Build ffmpeg filter for kept segments
  // We create a complex filter that selects only non-deleted segments

  FILE *f = fopen("/tmp/aslan_segments.txt", "w");
  if (!f)
    return 0;

  // Write ffmpeg concat demuxer file format
  for (int i = 0; i < result->segment_count; i++) {
    if (!result->segments[i].is_deleted) {
      fprintf(f, "file '%s'\n", input_path);
      fprintf(f, "inpoint %f\n", result->segments[i].start_ms / 1000.0);
      fprintf(f, "outpoint %f\n", result->segments[i].end_ms / 1000.0);
    }
  }
  fclose(f);

  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "ffmpeg -y -f concat -safe 0 -i /tmp/aslan_segments.txt "
           "-c copy \"%s\" 2>/dev/null",
           output_path);

  int ret = system(cmd);
  unlink("/tmp/aslan_segments.txt");

  return (ret == 0) ? 1 : 0;
}

TranscriptResult *whisper_transcribe_audio(const float *samples,
                                            int n_samples) {
  // Write samples to temp WAV file
  char wav_path[512];
  snprintf(wav_path, sizeof(wav_path), "/tmp/aslan_audio_%d.wav", getpid());

  FILE *f = fopen(wav_path, "wb");
  if (!f)
    return NULL;

  // Write WAV header (16kHz mono 32-bit float)
  int sample_rate = 16000;
  int byte_rate = sample_rate * 4;
  int data_size = n_samples * 4;

  fwrite("RIFF", 1, 4, f);
  int file_size = 36 + data_size;
  fwrite(&file_size, 4, 1, f);
  fwrite("WAVE", 1, 4, f);
  fwrite("fmt ", 1, 4, f);
  int fmt_size = 16;
  fwrite(&fmt_size, 4, 1, f);
  short audio_format = 3; // IEEE float
  fwrite(&audio_format, 2, 1, f);
  short num_channels = 1;
  fwrite(&num_channels, 2, 1, f);
  fwrite(&sample_rate, 4, 1, f);
  fwrite(&byte_rate, 4, 1, f);
  short block_align = 4;
  fwrite(&block_align, 2, 1, f);
  short bits_per_sample = 32;
  fwrite(&bits_per_sample, 2, 1, f);
  fwrite("data", 1, 4, f);
  fwrite(&data_size, 4, 1, f);

  // Write samples
  fwrite(samples, 4, n_samples, f);
  fclose(f);

  // Transcribe the WAV file
  TranscriptResult *result = whisper_transcribe_file(wav_path);

  // Cleanup
  unlink(wav_path);

  return result;
}
