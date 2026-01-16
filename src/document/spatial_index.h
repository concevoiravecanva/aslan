#ifndef SPATIAL_INDEX_H
#define SPATIAL_INDEX_H

#include <stdbool.h>
#include <stdint.h>

struct Node;
struct SpatialIndex;

struct SpatialIndex *spatial_index_create(int cell_size);
void spatial_index_destroy(struct SpatialIndex *idx);

void spatial_index_insert(struct SpatialIndex *idx, struct Node *node);
void spatial_index_remove(struct SpatialIndex *idx, struct Node *node);
void spatial_index_update(struct SpatialIndex *idx, struct Node *node,
                          float old_x, float old_y, float old_w, float old_h);

// Returns the first node found at (x, y).
struct Node *spatial_index_find_at(struct SpatialIndex *idx, float x, float y);

#endif
