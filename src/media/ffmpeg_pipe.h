#ifndef FFMPEG_PIPE_H
#define FFMPEG_PIPE_H

// =============================================================================
// PHASE 3: THE CLIP ENGINE (FFmpeg via Pipes)
// =============================================================================
// A minimalist video decoder interface using ffmpeg subprocess.
// Following the Antirez philosophy: no heavy libraries, just simple pipes.
//
// Features:
// - Thumbnail extraction at specific timestamps
// - Video metadata retrieval (duration, resolution, codec)
// - Frame extraction for viewport preview
//
// Requirements: ffmpeg and ffprobe must be installed and in PATH.

#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// VIDEO METADATA
// =============================================================================

typedef struct {
  char filepath[512];
  double duration;        // Total duration in seconds
  int width;              // Video width in pixels
  int height;             // Video height in pixels
  double fps;             // Frames per second
  char codec[64];         // Video codec name
  char format[64];        // Container format
  int64_t bitrate;        // Bitrate in bits/s
  bool has_audio;         // Whether file has audio stream
  bool valid;             // Whether metadata was successfully retrieved
} VideoMetadata;

// Retrieve video metadata using ffprobe
// Returns true on success, false if file doesn't exist or ffprobe failed
bool ffmpeg_get_metadata(const char *filepath, VideoMetadata *out_meta);

// =============================================================================
// THUMBNAIL GENERATION
// =============================================================================

// Raw RGB pixel data for thumbnails
typedef struct {
  uint8_t *pixels;        // RGB24 pixel data (3 bytes per pixel)
  int width;
  int height;
  bool valid;
} ThumbnailData;

// Extract a thumbnail at the specified timestamp
// width/height: desired thumbnail size (will maintain aspect ratio)
// timestamp: time in seconds to extract frame from
// Returns allocated ThumbnailData (caller must call thumbnail_free)
ThumbnailData ffmpeg_extract_thumbnail(const char *filepath, 
                                       int width, int height,
                                       double timestamp);

// Free thumbnail data
void thumbnail_free(ThumbnailData *thumb);

// =============================================================================
// FRAME EXTRACTION (for preview/scrubbing)
// =============================================================================

// Frame data structure
typedef struct {
  uint8_t *pixels;        // RGB24 pixel data
  int width;
  int height;
  double timestamp;       // Actual timestamp of extracted frame
  bool valid;
} FrameData;

// Extract a single frame at the specified timestamp
FrameData ffmpeg_extract_frame(const char *filepath, double timestamp);

// Free frame data
void frame_free(FrameData *frame);

// =============================================================================
// THUMBNAIL CACHE
// =============================================================================
// Simple in-memory cache for generated thumbnails.

#define THUMBNAIL_CACHE_SIZE 64

typedef struct {
  char filepath[512];
  double timestamp;
  ThumbnailData data;
  uint32_t last_used;     // Frame counter for LRU eviction
  bool in_use;
} ThumbnailCacheEntry;

typedef struct {
  ThumbnailCacheEntry entries[THUMBNAIL_CACHE_SIZE];
  uint32_t frame_counter;
} ThumbnailCache;

// Initialize the thumbnail cache
void thumbnail_cache_init(ThumbnailCache *cache);

// Get or generate a thumbnail (uses cache)
ThumbnailData *thumbnail_cache_get(ThumbnailCache *cache,
                                   const char *filepath,
                                   int width, int height,
                                   double timestamp);

// Clear all cached thumbnails
void thumbnail_cache_clear(ThumbnailCache *cache);

// =============================================================================
// SDL TEXTURE INTEGRATION
// =============================================================================
// Helper to convert thumbnail to SDL texture for rendering.

#include <SDL.h>

// Create SDL texture from thumbnail data
// Returns NULL on failure
SDL_Texture *thumbnail_to_texture(SDL_Renderer *renderer, 
                                  const ThumbnailData *thumb);

// Create SDL texture from frame data
SDL_Texture *frame_to_texture(SDL_Renderer *renderer, 
                              const FrameData *frame);

// =============================================================================
// UTILITY
// =============================================================================

// Check if ffmpeg is available in PATH
bool ffmpeg_is_available(void);

// Check if ffprobe is available in PATH
bool ffprobe_is_available(void);

// Format duration as "HH:MM:SS.mmm" string
void format_duration(double seconds, char *out, size_t out_size);

// Parse timestamp string to seconds (supports "HH:MM:SS" and seconds)
double parse_timestamp(const char *str);

#endif // FFMPEG_PIPE_H
