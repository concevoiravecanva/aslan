#include "document.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int global_find_index(const Document* doc, const char* key) {
    if (!doc || !key || !key[0]) return -1;
    for (int i = 0; i < (int)(sizeof(doc->globals) / sizeof(doc->globals[0])); i++) {
        if (doc->globals[i].in_use && strncmp(doc->globals[i].key, key, sizeof(doc->globals[i].key)) == 0) {
            return i;
        }
    }
    return -1;
}

static int global_find_free_slot(Document* doc) {
    if (!doc) return -1;
    for (int i = 0; i < (int)(sizeof(doc->globals) / sizeof(doc->globals[0])); i++) {
        if (!doc->globals[i].in_use) return i;
    }
    return -1;
}

static Node* detach_child(Node* parent, Node* child) {
    if (!parent || !child) return NULL;
    Node* prev = NULL;
    Node* cur = parent->first_child;
    while (cur) {
        if (cur == child) {
            if (prev) prev->next_sibling = cur->next_sibling;
            else parent->first_child = cur->next_sibling;
            cur->next_sibling = NULL;
            cur->parent = parent;
            return cur;
        }
        prev = cur;
        cur = cur->next_sibling;
    }
    return NULL;
}

static void append_child(Node* parent, Node* child) {
    if (!parent || !child) return;
    child->parent = parent;
    child->next_sibling = NULL;
    if (!parent->first_child) {
        parent->first_child = child;
        return;
    }
    Node* cur = parent->first_child;
    while (cur->next_sibling) cur = cur->next_sibling;
    cur->next_sibling = child;
}

static void insert_child_after(Node* parent, Node* child, Node* after) {
    if (!parent || !child) return;
    child->parent = parent;
    if (!after) {
        child->next_sibling = parent->first_child;
        parent->first_child = child;
        return;
    }
    child->next_sibling = after->next_sibling;
    after->next_sibling = child;
}

static Node* make_root_node(void) {
    Node* r = (Node*)calloc(1, sizeof(Node));
    if (!r) return NULL;
    r->id = 0;
    r->type = NODE_TYPE_ROOT;
    strcpy(r->name, "Root");
    return r;
}

Document* doc_create(void) {
    Document* doc = (Document*)malloc(sizeof(Document));
    if (!doc) return NULL;
    
    doc->next_node_id = 1;
    memset(doc->filepath, 0, sizeof(doc->filepath));
    
    // Pages defaults
    memset(doc->pages, 0, sizeof(doc->pages));
    doc->page_count = 0;
    doc->next_page_id = 1;
    doc->active_page_id = 0;

    // Create page 0
    doc->pages[0].in_use = 1;
    doc->pages[0].page_id = doc->next_page_id++;
    strncpy(doc->pages[0].name, "Page 1", sizeof(doc->pages[0].name) - 1);
    doc->pages[0].root = make_root_node();
    doc->page_count = 1;
    doc->active_page_id = doc->pages[0].page_id;

    // Back-compat: root points to active page root
    doc->root = doc->pages[0].root;

    // Document meta defaults
    memset(doc->doc_type, 0, sizeof(doc->doc_type));
    strncpy(doc->doc_type, "canvas2d", sizeof(doc->doc_type) - 1);
    doc->doc_width = 800;
    doc->doc_height = 600;

    // Globals defaults
    memset(doc->globals, 0, sizeof(doc->globals));
    
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
    // Free all page roots
    for (int i = 0; i < (int)(sizeof(doc->pages) / sizeof(doc->pages[0])); i++) {
        if (doc->pages[i].in_use && doc->pages[i].root) {
            free_node_recursive(doc->pages[i].root);
            doc->pages[i].root = NULL;
        }
    }
    free(doc);
}

static int page_find_index_by_id(const Document* doc, uint32_t page_id) {
    if (!doc || page_id == 0) return -1;
    for (int i = 0; i < (int)(sizeof(doc->pages) / sizeof(doc->pages[0])); i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) return i;
    }
    return -1;
}

static int page_find_free_slot(Document* doc) {
    if (!doc) return -1;
    for (int i = 0; i < (int)(sizeof(doc->pages) / sizeof(doc->pages[0])); i++) {
        if (!doc->pages[i].in_use) return i;
    }
    return -1;
}

static uint32_t doc_page_create_internal(Document* doc, uint32_t forced_page_id, const char* name) {
    if (!doc) return 0;
    int slot = page_find_free_slot(doc);
    if (slot < 0) return 0;

    if (forced_page_id != 0) {
        // Ensure uniqueness
        if (page_find_index_by_id(doc, forced_page_id) >= 0) return 0;
    }

    Node* r = make_root_node();
    if (!r) return 0;

    doc->pages[slot].in_use = 1;
    doc->pages[slot].page_id = forced_page_id != 0 ? forced_page_id : doc->next_page_id++;
    memset(doc->pages[slot].name, 0, sizeof(doc->pages[slot].name));
    if (name && name[0]) strncpy(doc->pages[slot].name, name, sizeof(doc->pages[slot].name) - 1);
    else {
        // default name: Page N
        snprintf(doc->pages[slot].name, sizeof(doc->pages[slot].name), "Page %d", doc->page_count + 1);
    }
    doc->pages[slot].root = r;
    doc->page_count++;

    // Keep next_page_id monotonic even with forced ids (avoid collisions).
    if (forced_page_id != 0 && forced_page_id >= doc->next_page_id) {
        doc->next_page_id = forced_page_id + 1;
    }

    // Auto-select new page
    doc->active_page_id = doc->pages[slot].page_id;
    doc->root = doc->pages[slot].root;
    return doc->pages[slot].page_id;
}

uint32_t doc_page_create(Document* doc, const char* name) {
    return doc_page_create_internal(doc, 0, name);
}

uint32_t doc_page_create_with_id(Document* doc, uint32_t page_id, const char* name) {
    if (!doc || page_id == 0) return 0;
    return doc_page_create_internal(doc, page_id, name);
}

bool doc_page_select(Document* doc, uint32_t page_id) {
    if (!doc || page_id == 0) return false;
    int idx = page_find_index_by_id(doc, page_id);
    if (idx < 0) return false;
    doc->active_page_id = page_id;
    doc->root = doc->pages[idx].root;
    return true;
}

uint32_t doc_page_active_id(const Document* doc) {
    if (!doc) return 0;
    return doc->active_page_id;
}

const char* doc_page_name(const Document* doc, uint32_t page_id) {
    if (!doc || page_id == 0) return NULL;
    int idx = page_find_index_by_id(doc, page_id);
    if (idx < 0) return NULL;
    return doc->pages[idx].name;
}

bool doc_set_global(Document* doc, const char* key, const char* value) {
    if (!doc || !key || !key[0] || !value) return false;

    int idx = global_find_index(doc, key);
    if (idx < 0) {
        idx = global_find_free_slot(doc);
        if (idx < 0) return false;
        memset(doc->globals[idx].key, 0, sizeof(doc->globals[idx].key));
        strncpy(doc->globals[idx].key, key, sizeof(doc->globals[idx].key) - 1);
        doc->globals[idx].in_use = 1;
    }

    memset(doc->globals[idx].value, 0, sizeof(doc->globals[idx].value));
    strncpy(doc->globals[idx].value, value, sizeof(doc->globals[idx].value) - 1);
    return true;
}

bool doc_unset_global(Document* doc, const char* key) {
    if (!doc || !key || !key[0]) return false;
    int idx = global_find_index(doc, key);
    if (idx < 0) return false;
    memset(doc->globals[idx].key, 0, sizeof(doc->globals[idx].key));
    memset(doc->globals[idx].value, 0, sizeof(doc->globals[idx].value));
    doc->globals[idx].in_use = 0;
    return true;
}

const char* doc_get_global(const Document* doc, const char* key) {
    if (!doc || !key || !key[0]) return NULL;
    int idx = global_find_index(doc, key);
    if (idx < 0) return NULL;
    return doc->globals[idx].value;
}

void doc_globals_iterate(const Document* doc, doc_global_iter_fn fn, void* user) {
    if (!doc || !fn) return;
    for (int i = 0; i < (int)(sizeof(doc->globals) / sizeof(doc->globals[0])); i++) {
        if (!doc->globals[i].in_use) continue;
        fn(doc->globals[i].key, doc->globals[i].value, user);
    }
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

bool doc_set_visible(Document* doc, uint32_t id, int visible) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || node->type == NODE_TYPE_ROOT) return false;
    node->visible = visible ? 1 : 0;
    return true;
}

bool doc_bring_to_front(Document* doc, uint32_t id) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || !node->parent) return false;
    Node* parent = node->parent;
    // already last?
    Node* cur = parent->first_child;
    if (!cur) return false;
    while (cur->next_sibling) cur = cur->next_sibling;
    if (cur == node) return true;

    Node* detached = detach_child(parent, node);
    if (!detached) return false;
    append_child(parent, detached);
    return true;
}

bool doc_send_to_back(Document* doc, uint32_t id) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || !node->parent) return false;
    Node* parent = node->parent;
    if (parent->first_child == node) return true;

    Node* detached = detach_child(parent, node);
    if (!detached) return false;
    insert_child_after(parent, detached, NULL);
    return true;
}

bool doc_bring_forward(Document* doc, uint32_t id) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || !node->parent) return false;
    Node* parent = node->parent;

    Node* prev = NULL;
    Node* cur = parent->first_child;
    while (cur && cur != node) {
        prev = cur;
        cur = cur->next_sibling;
    }
    if (!cur) return false;
    Node* next = cur->next_sibling;
    if (!next) return true; // already at front

    // swap cur and next
    if (prev) prev->next_sibling = next;
    else parent->first_child = next;
    cur->next_sibling = next->next_sibling;
    next->next_sibling = cur;
    return true;
}

bool doc_send_backward(Document* doc, uint32_t id) {
    Node* node = doc_find_node_by_id(doc, id);
    if (!node || !node->parent) return false;
    Node* parent = node->parent;
    if (parent->first_child == node) return true; // already at back

    Node* prevprev = NULL;
    Node* prev = parent->first_child;
    while (prev && prev->next_sibling != node) {
        prevprev = prev;
        prev = prev->next_sibling;
    }
    if (!prev) return false;

    // swap prev and node
    if (prevprev) prevprev->next_sibling = node;
    else parent->first_child = node;
    prev->next_sibling = node->next_sibling;
    node->next_sibling = prev;
    return true;
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
