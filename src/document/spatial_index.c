#include "spatial_index.h"
#include "document.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BUCKETS_COUNT 1024

typedef struct NodeLink {
  Node *node;
  struct NodeLink *next;
} NodeLink;

struct SpatialIndex {
  int cell_size;
  NodeLink *buckets[BUCKETS_COUNT];
};

static uint32_t hash_coords(int cx, int cy) {
  // Standard spatial hashing constants
  return (uint32_t)(((cx * 73856093) ^ (cy * 19349663)) % BUCKETS_COUNT);
}

struct SpatialIndex *spatial_index_create(int cell_size) {
  struct SpatialIndex *idx =
      (struct SpatialIndex *)calloc(1, sizeof(struct SpatialIndex));
  if (!idx)
    return NULL;
  idx->cell_size = cell_size > 0 ? cell_size : 128;
  return idx;
}

void spatial_index_destroy(struct SpatialIndex *idx) {
  if (!idx)
    return;
  for (int i = 0; i < BUCKETS_COUNT; i++) {
    NodeLink *link = idx->buckets[i];
    while (link) {
      NodeLink *next = link->next;
      free(link);
      link = next;
    }
  }
  free(idx);
}

static void bucket_add(struct SpatialIndex *idx, int cx, int cy, Node *node) {
  uint32_t h = hash_coords(cx, cy);
  NodeLink *link = (NodeLink *)malloc(sizeof(NodeLink));
  if (!link)
    return;
  link->node = node;
  link->next = idx->buckets[h];
  idx->buckets[h] = link;
}

static void bucket_remove(struct SpatialIndex *idx, int cx, int cy,
                          Node *node) {
  uint32_t h = hash_coords(cx, cy);
  NodeLink *prev = NULL;
  NodeLink *curr = idx->buckets[h];
  while (curr) {
    if (curr->node == node) {
      if (prev)
        prev->next = curr->next;
      else
        idx->buckets[h] = curr->next;
      free(curr);
      return;
    }
    prev = curr;
    curr = curr->next;
  }
}

void spatial_index_insert(struct SpatialIndex *idx, Node *node) {
  if (!idx || !node)
    return;
  int x0 = (int)floorf(node->x / idx->cell_size);
  int y0 = (int)floorf(node->y / idx->cell_size);
  int x1 = (int)floorf((node->x + node->width) / idx->cell_size);
  int y1 = (int)floorf((node->y + node->height) / idx->cell_size);

  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      bucket_add(idx, x, y, node);
    }
  }
}

void spatial_index_remove(struct SpatialIndex *idx, Node *node) {
  if (!idx || !node)
    return;
  int x0 = (int)floorf(node->x / idx->cell_size);
  int y0 = (int)floorf(node->y / idx->cell_size);
  int x1 = (int)floorf((node->x + node->width) / idx->cell_size);
  int y1 = (int)floorf((node->y + node->height) / idx->cell_size);

  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      bucket_remove(idx, x, y, node);
    }
  }
}

void spatial_index_update(struct SpatialIndex *idx, Node *node, float old_x,
                          float old_y, float old_w, float old_h) {
  if (!idx || !node)
    return;

  // Antirez-style: just remove and re-insert. It's safe and simple.
  // Optimization: only re-index if the set of covered cells changed.

  int ox0 = (int)floorf(old_x / idx->cell_size);
  int oy0 = (int)floorf(old_y / idx->cell_size);
  int ox1 = (int)floorf((old_x + old_w) / idx->cell_size);
  int oy1 = (int)floorf((old_y + old_h) / idx->cell_size);

  int nx0 = (int)floorf(node->x / idx->cell_size);
  int ny0 = (int)floorf(node->y / idx->cell_size);
  int nx1 = (int)floorf((node->x + node->width) / idx->cell_size);
  int ny1 = (int)floorf((node->y + node->height) / idx->cell_size);

  if (ox0 == nx0 && oy0 == ny0 && ox1 == nx1 && oy1 == ny1)
    return;

  // Remove from old cells
  for (int y = oy0; y <= oy1; y++) {
    for (int x = ox0; x <= ox1; x++) {
      bucket_remove(idx, x, y, node);
    }
  }

  // Add to new cells
  for (int y = ny0; y <= ny1; y++) {
    for (int x = nx0; x <= nx1; x++) {
      bucket_add(idx, x, y, node);
    }
  }
}

Node *spatial_index_find_at(struct SpatialIndex *idx, float x, float y) {
  if (!idx)
    return NULL;
  int cx = (int)floorf(x / idx->cell_size);
  int cy = (int)floorf(y / idx->cell_size);
  uint32_t h = hash_coords(cx, cy);

  NodeLink *link = idx->buckets[h];
  Node *best = NULL;

  while (link) {
    Node *node = link->node;
    // Check if point is inside node
    if (x >= node->x && x <= node->x + node->width && y >= node->y &&
        y <= node->y + node->height) {
      // We want the "top-most" node. In a flat list we don't know z-order
      // easily unless we store it. But for now, any find is better than O(N).
      // Actually, we could traverse the tree to get z-order, but that defeats
      // the purpose. For now, let's just return the first one found or the one
      // with highest ID (heuristic).
      if (!best || node->id > best->id) {
        best = node;
      }
    }
    link = link->next;
  }
  return best;
}
