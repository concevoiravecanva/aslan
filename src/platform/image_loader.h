// =============================================================================
// ASLAN IMAGE LOADER - Cross-Platform Image Loading Abstraction
// =============================================================================
// Uses CoreGraphics on macOS for optimal performance
// Falls back to stb_image on Linux/Windows for portability

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

// Load image as RGBA pixel data
// Returns pointer to pixel data (caller must free with image_free_data)
// Sets *w and *h to image dimensions
// Returns NULL on failure
void *image_load_rgba(const char *path, int *w, int *h);

// Get image dimensions without loading full pixel data
// Returns 1 on success, 0 on failure
int image_get_size(const char *path, int *w, int *h);

// Free pixel data returned by image_load_rgba
void image_free_data(void *data);

#ifdef __cplusplus
}
#endif

#endif // IMAGE_LOADER_H
