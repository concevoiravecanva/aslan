#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stdint.h>
#include <stdbool.h>

// Types de nœuds graphiques supportés
typedef enum {
    NODE_TYPE_ROOT,
    NODE_TYPE_RECT,
    NODE_TYPE_TEXT,
    NODE_TYPE_IMAGE
} NodeType;

// Structure générique pour un objet graphique
// Conçue pour être simple et extensible (POD - Plain Old Data)
typedef struct Node {
    uint32_t id;
    NodeType type;
    char name[64];
    
    // Propriétés spatiales de base
    float x, y;
    float width, height;
    
    // Payload for certain node types
    // - TEXT: string content
    char text[256];
    
    // Visibility (for layers)
    int visible;
    
    // Hiérarchie
    struct Node* parent;
    struct Node* first_child;
    struct Node* next_sibling;
} Node;

// Le Document est le conteneur principal
typedef struct {
    // Back-compat: points to the active page root.
    Node* root;
    uint32_t next_node_id;
    char filepath[256];

    // --- Document-level metadata (future multi-page foundation) ---
    // Antirez-style: keep it simple and AOF-driven.
    char doc_type[32];
    int doc_width;
    int doc_height;

    // --- Document globals (future multi-page foundation) ---
    // Simple string key/value store, driven by AOF.
    // Note: values are stored untyped for now; higher-level systems can parse.
    struct {
        char key[64];
        char value[256];
        int in_use;
    } globals[64];

    // --- Pages (MVP) ---
    // Page 0 always exists.
    // For now pages are all canvas2d and share the same globals store.
    // Each page has its own Node tree root.
    struct {
        int in_use;
        uint32_t page_id;
        char name[64];
        Node* root;
    } pages[32];
    int page_count;
    uint32_t next_page_id;
    uint32_t active_page_id;
} Document;

// --- Pages API ---
uint32_t doc_page_create(Document* doc, const char* name);
bool doc_page_select(Document* doc, uint32_t page_id);
uint32_t doc_page_active_id(const Document* doc);
const char* doc_page_name(const Document* doc, uint32_t page_id);

// API C pour la manipulation du document
Document* doc_create(void);
void doc_destroy(Document* doc);

Node* doc_create_node(Document* doc, NodeType type, const char* name);
void doc_add_child(Node* parent, Node* child);
Node* doc_find_node_by_id(Document* doc, uint32_t id);

// --- Layers / z-order helpers ---
// Z-order is defined by sibling order under the same parent.
// The last sibling is drawn last and is therefore on top.
bool doc_set_visible(Document* doc, uint32_t id, int visible);
bool doc_bring_to_front(Document* doc, uint32_t id);
bool doc_send_to_back(Document* doc, uint32_t id);
bool doc_bring_forward(Document* doc, uint32_t id);
bool doc_send_backward(Document* doc, uint32_t id);

// Trouve le nœud le plus profond à une position donnée (pour le clic souris)
Node* doc_find_node_at(Document* doc, float x, float y);

// --- Globals API ---
// Returns true if set/unset succeeded.
bool doc_set_global(Document* doc, const char* key, const char* value);
bool doc_unset_global(Document* doc, const char* key);
// Returns NULL if not present.
const char* doc_get_global(const Document* doc, const char* key);

typedef void (*doc_global_iter_fn)(const char* key, const char* value, void* user);
void doc_globals_iterate(const Document* doc, doc_global_iter_fn fn, void* user);

// Export du document en format SVG pour visualisation rapide
void doc_export_to_svg(const Document* doc, const char* filename);

void doc_print_tree(const Document* doc); // Pour le debug

#endif
