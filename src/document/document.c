#include "document.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Document* doc_create(void) {
    Document* doc = (Document*)malloc(sizeof(Document));
    if (!doc) return NULL;
    
    doc->next_node_id = 1;
    memset(doc->filepath, 0, sizeof(doc->filepath));
    
    // Création automatique de la racine
    doc->root = (Node*)calloc(1, sizeof(Node));
    doc->root->id = 0;
    doc->root->type = NODE_TYPE_ROOT;
    strcpy(doc->root->name, "Root");
    
    return doc;
}

static void free_node_recursive(Node* node) {
    if (!node) return;
    free_node_recursive(node->first_child);
    free_node_recursive(node->next_sibling);
    free(node);
}

void doc_destroy(Document* doc) {
    if (!doc) return;
    free_node_recursive(doc->root);
    free(doc);
}

Node* doc_create_node(Document* doc, NodeType type, const char* name) {
    Node* node = (Node*)calloc(1, sizeof(Node));
    if (!node) return NULL;
    
    node->id = doc->next_node_id++;
    node->type = type;
    node->visible = 1;
    if (name) {
        strncpy(node->name, name, 63);
        node->name[63] = 0;
    }
    
    return node;
}

void doc_add_child(Node* parent, Node* child) {
    if (!parent || !child) return;
    
    child->parent = parent;
    child->next_sibling = NULL; // Sécurité
    
    if (!parent->first_child) {
        parent->first_child = child;
    } else {
        // Ajout en fin de liste
        Node* current = parent->first_child;
        while (current->next_sibling) {
            current = current->next_sibling;
        }
        current->next_sibling = child;
    }
}

static Node* find_node_recursive(Node* current, uint32_t id) {
    if (!current) return NULL;
    if (current->id == id) return current;
    
    Node* found = find_node_recursive(current->first_child, id);
    if (found) return found;
    
    return find_node_recursive(current->next_sibling, id);
}

Node* doc_find_node_by_id(Document* doc, uint32_t id) {
    if (!doc || !doc->root) return NULL;
    return find_node_recursive(doc->root, id);
}

static Node* find_node_at_recursive(Node* node, float x, float y) {
    if (!node) return NULL;

    // On cherche d'abord dans les enfants (pour avoir le nœud le plus "au-dessus")
    Node* found = find_node_at_recursive(node->next_sibling, x, y);
    if (found) return found;

    found = find_node_at_recursive(node->first_child, x, y);
    if (found) return found;

    // Vérification si le point est dans le rectangle du nœud actuel
    if (node->type != NODE_TYPE_ROOT) {
        if (x >= node->x && x <= node->x + node->width &&
            y >= node->y && y <= node->y + node->height) {
            return node;
        }
    }

    return NULL;
}

Node* doc_find_node_at(Document* doc, float x, float y) {
    if (!doc || !doc->root) return NULL;
    // On commence par les enfants de la racine
    return find_node_at_recursive(doc->root->first_child, x, y);
}

static void print_node_recursive(Node* node, int depth) {
    if (!node) return;
    
    for (int i = 0; i < depth; i++) printf("  ");
    printf("- [%d] %s (Type: %d) at (%.1f, %.1f)\n", 
           node->id, node->name, node->type, node->x, node->y);
           
    print_node_recursive(node->first_child, depth + 1);
    print_node_recursive(node->next_sibling, depth);
}

void doc_print_tree(const Document* doc) {
    printf("=== Document Tree ===\n");
    if (doc && doc->root) {
        print_node_recursive(doc->root, 0);
    } else {
        printf("(Empty)\n");
    }
    printf("=====================\n");
}

// --- SVG Export ---

static void write_svg_node_recursive(FILE* f, Node* node) {
    if (!node) return;

    // On dessine seulement si c'est un RECT pour l'instant
    if (node->type == NODE_TYPE_RECT) {
        // Couleur aléatoire basée sur l'ID pour distinguer les blocs
        int r = (node->id * 50) % 255;
        int g = (node->id * 80) % 255;
        int b = (node->id * 110) % 255;
        
        fprintf(f, "  <g>\n");
        fprintf(f, "    <rect x='%.2f' y='%.2f' width='%.2f' height='%.2f' "
                   "fill='rgb(%d,%d,%d)' stroke='black' stroke-width='2' />\n",
                   node->x, node->y, node->width, node->height, r, g, b);
        
        // Ajout du texte pour le nom
        fprintf(f, "    <text x='%.2f' y='%.2f' font-family='Arial' font-size='14' fill='white'>%s</text>\n",
                   node->x + 5, node->y + 20, node->name);
        fprintf(f, "  </g>\n");
    }

    write_svg_node_recursive(f, node->first_child);
    write_svg_node_recursive(f, node->next_sibling);
}

void doc_export_to_svg(const Document* doc, const char* filename) {
    if (!doc || !doc->root) return;

    FILE* f = fopen(filename, "w");
    if (!f) {
        printf("Error: Could not create SVG file %s\n", filename);
        return;
    }

    // En-tête SVG standard
    fprintf(f, "<?xml version='1.0' encoding='UTF-8' standalone='no'?>\n");
    fprintf(f, "<svg width='800' height='600' xmlns='http://www.w3.org/2000/svg'>\n");
    fprintf(f, "  <rect width='100%%' height='100%%' fill='#f0f0f0' />\n"); // Fond gris clair

    write_svg_node_recursive(f, doc->root);

    fprintf(f, "</svg>\n");
    fclose(f);
    printf("Exported document to %s\n", filename);
}
