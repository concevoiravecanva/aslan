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
    Node* root;
    uint32_t next_node_id;
    char filepath[256];

    // --- Document-level metadata (future multi-page foundation) ---
    // Antirez-style: keep it simple and AOF-driven.
    char doc_type[32];
    int doc_width;
    int doc_height;
} Document;

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

// Export du document en format SVG pour visualisation rapide
void doc_export_to_svg(const Document* doc, const char* filename);

void doc_print_tree(const Document* doc); // Pour le debug

#endif
