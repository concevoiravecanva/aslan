#ifndef RECENTS_H
#define RECENTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECENTS_MAX 16
#define RECENTS_PATH_MAX 512

typedef struct Recents {
    char paths[RECENTS_MAX][RECENTS_PATH_MAX];
    int count;
} Recents;

// Loads recents from a small text file (one path per line). Missing file is OK.
void recents_load(Recents* r, const char* file_path);

// Saves recents to disk (one path per line). Returns 1 on success, 0 on fail.
int recents_save(const Recents* r, const char* file_path);

// Adds a path to the top (dedup). Keeps at most RECENTS_MAX.
void recents_add(Recents* r, const char* path);

// Removes a path (if present).
void recents_remove(Recents* r, const char* path);

#ifdef __cplusplus
}
#endif

#endif
