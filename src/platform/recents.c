#include "recents.h"

#include <stdio.h>
#include <string.h>

static void trim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = 0;
        n--;
    }
}

void recents_load(Recents* r, const char* file_path) {
    if (!r) return;
    r->count = 0;
    if (!file_path || !file_path[0]) return;

    FILE* f = fopen(file_path, "r");
    if (!f) return;

    char line[RECENTS_PATH_MAX];
    while (fgets(line, sizeof(line), f)) {
        trim_newline(line);
        if (!line[0]) continue;
        // reuse add() (dedup + cap)
        recents_add(r, line);
    }

    fclose(f);
}

int recents_save(const Recents* r, const char* file_path) {
    if (!r || !file_path || !file_path[0]) return 0;

    FILE* f = fopen(file_path, "w");
    if (!f) return 0;

    for (int i = 0; i < r->count; i++) {
        if (r->paths[i][0]) {
            fprintf(f, "%s\n", r->paths[i]);
        }
    }

    fclose(f);
    return 1;
}

void recents_add(Recents* r, const char* path) {
    if (!r || !path || !path[0]) return;

    // Dedup first
    int existing = -1;
    for (int i = 0; i < r->count; i++) {
        if (strncmp(r->paths[i], path, RECENTS_PATH_MAX) == 0) {
            existing = i;
            break;
        }
    }

    // Shift down to make room at top
    if (existing == 0) return;

    if (existing > 0) {
        for (int i = existing; i > 0; i--) {
            memcpy(r->paths[i], r->paths[i - 1], RECENTS_PATH_MAX);
        }
    } else {
        int limit = r->count < (RECENTS_MAX - 1) ? r->count : (RECENTS_MAX - 1);
        for (int i = limit; i > 0; i--) {
            memcpy(r->paths[i], r->paths[i - 1], RECENTS_PATH_MAX);
        }
        if (r->count < RECENTS_MAX) r->count++;
    }

    snprintf(r->paths[0], RECENTS_PATH_MAX, "%s", path);
}

void recents_remove(Recents* r, const char* path) {
    if (!r || !path || !path[0]) return;

    int idx = -1;
    for (int i = 0; i < r->count; i++) {
        if (strncmp(r->paths[i], path, RECENTS_PATH_MAX) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;

    for (int i = idx; i < r->count - 1; i++) {
        memcpy(r->paths[i], r->paths[i + 1], RECENTS_PATH_MAX);
    }
    r->paths[r->count - 1][0] = 0;
    r->count--;
    if (r->count < 0) r->count = 0;
}
