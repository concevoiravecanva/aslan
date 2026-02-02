#ifndef MAC_IMAGE_H
#define MAC_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

void *mac_load_image_rgba(const char *path, int *w, int *h);
int mac_get_image_size(const char *path, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif
