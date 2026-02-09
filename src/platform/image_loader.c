// =============================================================================
// ASLAN IMAGE LOADER - Cross-Platform Implementation
// =============================================================================

#include "image_loader.h"
#include <stdlib.h>

#ifdef __APPLE__
// macOS: Use CoreGraphics via mac_image.m (linked separately)
extern void *mac_load_image_rgba(const char *path, int *w, int *h);
extern int mac_get_image_size(const char *path, int *w, int *h);

void *image_load_rgba(const char *path, int *w, int *h) {
  return mac_load_image_rgba(path, w, h);
}

int image_get_size(const char *path, int *w, int *h) {
  return mac_get_image_size(path, w, h);
}

void image_free_data(void *data) { free(data); }

#else
// Linux/Windows: Use stb_image (header-only library)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#include "../../include/stb_image.h"

void *image_load_rgba(const char *path, int *w, int *h) {
  int channels;
  // Force 4 channels (RGBA)
  unsigned char *data = stbi_load(path, w, h, &channels, 4);
  return data;
}

int image_get_size(const char *path, int *w, int *h) {
  int channels;
  int ok = stbi_info(path, w, h, &channels);
  return ok;
}

void image_free_data(void *data) { stbi_image_free(data); }

#endif
