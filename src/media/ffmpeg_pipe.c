#include "ffmpeg_pipe.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

// =============================================================================
// PHASE 3: THE CLIP ENGINE (FFmpeg via Pipes)
// =============================================================================
// Implementation following the Antirez philosophy:
// - Simple subprocess spawning via popen/pipe
// - Minimal parsing of ffprobe JSON output
// - Direct RGB frame extraction via ffmpeg

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

bool ffmpeg_is_available(void) {
  FILE *fp = popen("which ffmpeg 2>/dev/null", "r");
  if (!fp) return false;
  
  char buf[256];
  bool found = (fgets(buf, sizeof(buf), fp) != NULL);
  pclose(fp);
  return found;
}

bool ffprobe_is_available(void) {
  FILE *fp = popen("which ffprobe 2>/dev/null", "r");
  if (!fp) return false;
  
  char buf[256];
  bool found = (fgets(buf, sizeof(buf), fp) != NULL);
  pclose(fp);
  return found;
}

void format_duration(double seconds, char *out, size_t out_size) {
  if (!out || out_size < 12) return;
  
  int hours = (int)(seconds / 3600);
  int mins = (int)((seconds - hours * 3600) / 60);
  double secs = seconds - hours * 3600 - mins * 60;
  
  if (hours > 0) {
    snprintf(out, out_size, "%d:%02d:%05.2f", hours, mins, secs);
  } else if (mins > 0) {
    snprintf(out, out_size, "%d:%05.2f", mins, secs);
  } else {
    snprintf(out, out_size, "%.2f", secs);
  }
}

double parse_timestamp(const char *str) {
  if (!str) return 0.0;
  
  // Try parsing as seconds first
  char *endptr;
  double val = strtod(str, &endptr);
  if (*endptr == '\0' || *endptr == '\n') {
    return val;
  }
  
  // Try parsing as HH:MM:SS or MM:SS
  int h = 0, m = 0;
  double s = 0.0;
  
  if (sscanf(str, "%d:%d:%lf", &h, &m, &s) == 3) {
    return h * 3600.0 + m * 60.0 + s;
  }
  if (sscanf(str, "%d:%lf", &m, &s) == 2) {
    return m * 60.0 + s;
  }
  
  return 0.0;
}

// =============================================================================
// VIDEO METADATA (via ffprobe)
// =============================================================================

// Simple JSON value extractor (minimal parsing)
static bool extract_json_string(const char *json, const char *key, 
                                char *out, size_t out_size) {
  if (!json || !key || !out) return false;
  
  char search[128];
  snprintf(search, sizeof(search), "\"%s\"", key);
  
  const char *pos = strstr(json, search);
  if (!pos) return false;
  
  // Find the value after ":"
  pos = strchr(pos, ':');
  if (!pos) return false;
  pos++;
  
  // Skip whitespace
  while (*pos == ' ' || *pos == '\t') pos++;
  
  // Handle string value
  if (*pos == '"') {
    pos++;
    const char *end = strchr(pos, '"');
    if (!end) return false;
    
    size_t len = (size_t)(end - pos);
    if (len >= out_size) len = out_size - 1;
    strncpy(out, pos, len);
    out[len] = '\0';
    return true;
  }
  
  // Handle numeric value
  const char *end = pos;
  while (*end && *end != ',' && *end != '}' && *end != '\n') end++;
  
  size_t len = (size_t)(end - pos);
  if (len >= out_size) len = out_size - 1;
  strncpy(out, pos, len);
  out[len] = '\0';
  
  // Trim trailing whitespace
  while (len > 0 && (out[len-1] == ' ' || out[len-1] == '\t' || out[len-1] == '\r')) {
    out[--len] = '\0';
  }
  
  return true;
}

bool ffmpeg_get_metadata(const char *filepath, VideoMetadata *out_meta) {
  if (!filepath || !out_meta) return false;
  
  memset(out_meta, 0, sizeof(VideoMetadata));
  strncpy(out_meta->filepath, filepath, sizeof(out_meta->filepath) - 1);
  
  if (!ffprobe_is_available()) {
    fprintf(stderr, "[CLIP ENGINE] ffprobe not found in PATH\n");
    return false;
  }
  
  // Build ffprobe command
  // Using JSON output for easier parsing
  char cmd[1024];
  snprintf(cmd, sizeof(cmd),
           "ffprobe -v quiet -print_format json -show_format -show_streams "
           "-select_streams v:0 \"%s\" 2>/dev/null",
           filepath);
  
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    fprintf(stderr, "[CLIP ENGINE] Failed to run ffprobe: %s\n", strerror(errno));
    return false;
  }
  
  // Read all output
  char json[16384] = {0};
  size_t total_read = 0;
  
  while (!feof(fp) && total_read < sizeof(json) - 1) {
    size_t n = fread(json + total_read, 1, sizeof(json) - 1 - total_read, fp);
    if (n <= 0) break;
    total_read += n;
  }
  json[total_read] = '\0';
  
  int status = pclose(fp);
  if (status != 0 || total_read == 0) {
    fprintf(stderr, "[CLIP ENGINE] ffprobe failed or file not found: %s\n", filepath);
    return false;
  }
  
  // Parse JSON output (simple extraction)
  char buf[256];
  
  // Duration from format
  if (extract_json_string(json, "duration", buf, sizeof(buf))) {
    out_meta->duration = strtod(buf, NULL);
  }
  
  // Width and height from video stream
  if (extract_json_string(json, "width", buf, sizeof(buf))) {
    out_meta->width = atoi(buf);
  }
  if (extract_json_string(json, "height", buf, sizeof(buf))) {
    out_meta->height = atoi(buf);
  }
  
  // Codec name
  if (extract_json_string(json, "codec_name", buf, sizeof(buf))) {
    strncpy(out_meta->codec, buf, sizeof(out_meta->codec) - 1);
  }
  
  // Frame rate (try r_frame_rate first, then avg_frame_rate)
  if (extract_json_string(json, "r_frame_rate", buf, sizeof(buf))) {
    // Format is typically "30/1" or "30000/1001"
    int num = 0, den = 1;
    if (sscanf(buf, "%d/%d", &num, &den) == 2 && den != 0) {
      out_meta->fps = (double)num / (double)den;
    }
  }
  
  // Format name
  if (extract_json_string(json, "format_name", buf, sizeof(buf))) {
    strncpy(out_meta->format, buf, sizeof(out_meta->format) - 1);
  }
  
  // Bitrate
  if (extract_json_string(json, "bit_rate", buf, sizeof(buf))) {
    out_meta->bitrate = strtoll(buf, NULL, 10);
  }
  
  // Check for audio stream
  out_meta->has_audio = (strstr(json, "\"codec_type\": \"audio\"") != NULL ||
                         strstr(json, "\"codec_type\":\"audio\"") != NULL);
  
  out_meta->valid = (out_meta->duration > 0 && out_meta->width > 0 && out_meta->height > 0);
  
  return out_meta->valid;
}

// =============================================================================
// THUMBNAIL EXTRACTION
// =============================================================================

ThumbnailData ffmpeg_extract_thumbnail(const char *filepath,
                                       int width, int height,
                                       double timestamp) {
  ThumbnailData result = {0};
  
  if (!filepath || width <= 0 || height <= 0) {
    return result;
  }
  
  if (!ffmpeg_is_available()) {
    fprintf(stderr, "[CLIP ENGINE] ffmpeg not found in PATH\n");
    return result;
  }
  
  // Clamp timestamp to valid range
  if (timestamp < 0) timestamp = 0;
  
  // Build ffmpeg command to extract single frame as raw RGB
  // -ss before -i for fast seeking
  // -vf scale for resize with aspect ratio preservation
  // -frames:v 1 to extract single frame
  // -f rawvideo -pix_fmt rgb24 for raw RGB output
  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "ffmpeg -ss %.3f -i \"%s\" "
           "-vf \"scale=%d:%d:force_original_aspect_ratio=decrease,pad=%d:%d:(ow-iw)/2:(oh-ih)/2\" "
           "-frames:v 1 -f rawvideo -pix_fmt rgb24 - 2>/dev/null",
           timestamp, filepath, width, height, width, height);
  
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    fprintf(stderr, "[CLIP ENGINE] Failed to run ffmpeg: %s\n", strerror(errno));
    return result;
  }
  
  // Allocate buffer for RGB data
  size_t expected_size = (size_t)(width * height * 3);
  uint8_t *pixels = (uint8_t *)malloc(expected_size);
  if (!pixels) {
    pclose(fp);
    return result;
  }
  
  // Read pixel data
  size_t total_read = 0;
  while (total_read < expected_size) {
    size_t n = fread(pixels + total_read, 1, expected_size - total_read, fp);
    if (n <= 0) break;
    total_read += n;
  }
  
  pclose(fp);
  
  if (total_read != expected_size) {
    fprintf(stderr, "[CLIP ENGINE] Incomplete frame data: got %zu, expected %zu\n",
            total_read, expected_size);
    free(pixels);
    return result;
  }
  
  result.pixels = pixels;
  result.width = width;
  result.height = height;
  result.valid = true;
  
  return result;
}

void thumbnail_free(ThumbnailData *thumb) {
  if (!thumb) return;
  free(thumb->pixels);
  thumb->pixels = NULL;
  thumb->width = 0;
  thumb->height = 0;
  thumb->valid = false;
}

// =============================================================================
// FRAME EXTRACTION
// =============================================================================

FrameData ffmpeg_extract_frame(const char *filepath, double timestamp) {
  FrameData result = {0};
  
  if (!filepath) return result;
  
  // First get video metadata to know the frame size
  VideoMetadata meta;
  if (!ffmpeg_get_metadata(filepath, &meta)) {
    return result;
  }
  
  // Clamp timestamp
  if (timestamp < 0) timestamp = 0;
  if (timestamp > meta.duration) timestamp = meta.duration;
  
  // Build ffmpeg command for full-resolution frame
  char cmd[2048];
  snprintf(cmd, sizeof(cmd),
           "ffmpeg -ss %.3f -i \"%s\" "
           "-frames:v 1 -f rawvideo -pix_fmt rgb24 - 2>/dev/null",
           timestamp, filepath);
  
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    return result;
  }
  
  size_t expected_size = (size_t)(meta.width * meta.height * 3);
  uint8_t *pixels = (uint8_t *)malloc(expected_size);
  if (!pixels) {
    pclose(fp);
    return result;
  }
  
  size_t total_read = 0;
  while (total_read < expected_size) {
    size_t n = fread(pixels + total_read, 1, expected_size - total_read, fp);
    if (n <= 0) break;
    total_read += n;
  }
  
  pclose(fp);
  
  if (total_read != expected_size) {
    free(pixels);
    return result;
  }
  
  result.pixels = pixels;
  result.width = meta.width;
  result.height = meta.height;
  result.timestamp = timestamp;
  result.valid = true;
  
  return result;
}

void frame_free(FrameData *frame) {
  if (!frame) return;
  free(frame->pixels);
  frame->pixels = NULL;
  frame->width = 0;
  frame->height = 0;
  frame->valid = false;
}

// =============================================================================
// THUMBNAIL CACHE
// =============================================================================

void thumbnail_cache_init(ThumbnailCache *cache) {
  if (!cache) return;
  memset(cache, 0, sizeof(ThumbnailCache));
}

static int find_cache_entry(ThumbnailCache *cache, const char *filepath, 
                            double timestamp) {
  if (!cache || !filepath) return -1;
  
  for (int i = 0; i < THUMBNAIL_CACHE_SIZE; i++) {
    if (cache->entries[i].in_use &&
        strcmp(cache->entries[i].filepath, filepath) == 0 &&
        fabs(cache->entries[i].timestamp - timestamp) < 0.01) {
      return i;
    }
  }
  return -1;
}

static int find_free_or_lru_entry(ThumbnailCache *cache) {
  if (!cache) return 0;
  
  int oldest_idx = 0;
  uint32_t oldest_time = UINT32_MAX;
  
  for (int i = 0; i < THUMBNAIL_CACHE_SIZE; i++) {
    if (!cache->entries[i].in_use) {
      return i;
    }
    if (cache->entries[i].last_used < oldest_time) {
      oldest_time = cache->entries[i].last_used;
      oldest_idx = i;
    }
  }
  
  // Evict oldest entry
  thumbnail_free(&cache->entries[oldest_idx].data);
  cache->entries[oldest_idx].in_use = false;
  
  return oldest_idx;
}

ThumbnailData *thumbnail_cache_get(ThumbnailCache *cache,
                                   const char *filepath,
                                   int width, int height,
                                   double timestamp) {
  if (!cache || !filepath) return NULL;
  
  cache->frame_counter++;
  
  // Check if already cached
  int idx = find_cache_entry(cache, filepath, timestamp);
  if (idx >= 0) {
    cache->entries[idx].last_used = cache->frame_counter;
    return &cache->entries[idx].data;
  }
  
  // Generate new thumbnail
  ThumbnailData thumb = ffmpeg_extract_thumbnail(filepath, width, height, timestamp);
  if (!thumb.valid) {
    return NULL;
  }
  
  // Store in cache
  idx = find_free_or_lru_entry(cache);
  
  ThumbnailCacheEntry *entry = &cache->entries[idx];
  strncpy(entry->filepath, filepath, sizeof(entry->filepath) - 1);
  entry->timestamp = timestamp;
  entry->data = thumb;
  entry->last_used = cache->frame_counter;
  entry->in_use = true;
  
  return &entry->data;
}

void thumbnail_cache_clear(ThumbnailCache *cache) {
  if (!cache) return;
  
  for (int i = 0; i < THUMBNAIL_CACHE_SIZE; i++) {
    if (cache->entries[i].in_use) {
      thumbnail_free(&cache->entries[i].data);
      cache->entries[i].in_use = false;
    }
  }
}

// =============================================================================
// SDL TEXTURE INTEGRATION
// =============================================================================

SDL_Texture *thumbnail_to_texture(SDL_Renderer *renderer,
                                  const ThumbnailData *thumb) {
  if (!renderer || !thumb || !thumb->valid || !thumb->pixels) {
    return NULL;
  }
  
  // Create SDL texture
  SDL_Texture *tex = SDL_CreateTexture(renderer,
                                       SDL_PIXELFORMAT_RGB24,
                                       SDL_TEXTUREACCESS_STATIC,
                                       thumb->width, thumb->height);
  if (!tex) {
    fprintf(stderr, "[CLIP ENGINE] Failed to create texture: %s\n", SDL_GetError());
    return NULL;
  }
  
  // Upload pixel data
  if (SDL_UpdateTexture(tex, NULL, thumb->pixels, thumb->width * 3) != 0) {
    fprintf(stderr, "[CLIP ENGINE] Failed to update texture: %s\n", SDL_GetError());
    SDL_DestroyTexture(tex);
    return NULL;
  }
  
  return tex;
}

SDL_Texture *frame_to_texture(SDL_Renderer *renderer,
                              const FrameData *frame) {
  if (!renderer || !frame || !frame->valid || !frame->pixels) {
    return NULL;
  }
  
  SDL_Texture *tex = SDL_CreateTexture(renderer,
                                       SDL_PIXELFORMAT_RGB24,
                                       SDL_TEXTUREACCESS_STATIC,
                                       frame->width, frame->height);
  if (!tex) {
    return NULL;
  }
  
  if (SDL_UpdateTexture(tex, NULL, frame->pixels, frame->width * 3) != 0) {
    SDL_DestroyTexture(tex);
    return NULL;
  }
  
  return tex;
}
