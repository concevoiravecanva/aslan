// ============================================================================
// ASLAN TEST RUNNER
// ============================================================================
// Simple test framework for Aslan
// Philosophy: Antirez-style - simple, direct, no fancy frameworks
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Import Aslan core
#include "../src/core/engine.h"
#include "../src/document/document.h"

// ============================================================================
// TEST HELPERS
// ============================================================================

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST(name) \
    printf("\n─────────────────────────────────────\n"); \
    printf("TEST: %s\n", name); \
    test_count++;

#define ASSERT(condition, message) \
    if (!(condition)) { \
        printf("  ❌ FAIL: %s\n", message); \
        printf("     Line %d: %s\n", __LINE__, #condition); \
        test_failed++; \
        return; \
    } else { \
        printf("  ✅ PASS: %s\n", message); \
    }

#define ASSERT_EQ(a, b, message) \
    if ((a) != (b)) { \
        printf("  ❌ FAIL: %s\n", message); \
        printf("     Expected: %d, Got: %d\n", (int)(b), (int)(a)); \
        test_failed++; \
        return; \
    } else { \
        printf("  ✅ PASS: %s\n", message); \
    }

#define ASSERT_STR_EQ(a, b, message) \
    if (strcmp((a), (b)) != 0) { \
        printf("  ❌ FAIL: %s\n", message); \
        printf("     Expected: \"%s\", Got: \"%s\"\n", (b), (a)); \
        test_failed++; \
        return; \
    } else { \
        printf("  ✅ PASS: %s\n", message); \
    }

static void test_passed_inc() {
    test_passed++;
}

// ============================================================================
// DOCUMENT TESTS
// ============================================================================

void test_create_document() {
    TEST("Create Document");
    
    Document *doc = doc_create();
    ASSERT(doc != NULL, "Document created");
    ASSERT(doc->root != NULL, "Document has root node");
    ASSERT(doc->next_node_id == 1, "Next node ID initialized");
    
    doc_destroy(doc);
    test_passed_inc();
}

void test_create_page() {
    TEST("Create Page");
    
    Document *doc = doc_create();
    uint32_t page_id = doc_page_create(doc, "Test Page");
    
    ASSERT(page_id != 0, "Page created with valid ID");
    ASSERT_EQ(doc->page_count, 1, "Page count is 1");
    
    const char *name = doc_page_name(doc, page_id);
    ASSERT_STR_EQ(name, "Test Page", "Page name correct");
    
    doc_destroy(doc);
    test_passed_inc();
}

// ============================================================================
// NOTE TESTS
// ============================================================================

void test_note_create() {
    TEST("Create Note Page");
    
    Document *doc = doc_create();
    uint32_t page_id = doc_page_create(doc, "Note Test");
    
    bool success = doc_page_set_type(doc, page_id, PAGE_TYPE_NOTE);
    ASSERT(success, "Page type set to NOTE");
    
    // Find the page
    AslanNoteDoc *note = NULL;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            note = doc->pages[i].note;
            break;
        }
    }
    
    ASSERT(note != NULL, "Note structure created");
    ASSERT(note->row_count > 0, "Note has at least one row (default empty line)");
    
    doc_destroy(doc);
    test_passed_inc();
}

void test_note_insert_line() {
    TEST("Insert Text Line in Note");
    
    Document *doc = doc_create();
    uint32_t page_id = doc_page_create(doc, "Note Test");
    doc_page_set_type(doc, page_id, PAGE_TYPE_NOTE);
    
    AslanNoteDoc *note = NULL;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            note = doc->pages[i].note;
            break;
        }
    }
    
    ASSERT(note != NULL, "Note exists");
    
    int initial_count = note->row_count;
    note_insert_line(note, note->row_count, "Test line");
    
    ASSERT_EQ(note->row_count, initial_count + 1, "Row count increased");
    
    const char *text = note->rows[note->row_count - 1].chars;
    ASSERT_STR_EQ(text, "Test line", "Text content correct");
    ASSERT_EQ(note->rows[note->row_count - 1].type, ASLAN_ROW_TEXT, "Row type is TEXT");
    
    doc_destroy(doc);
    test_passed_inc();
}

void test_note_heading() {
    TEST("Insert Heading in Note");
    
    Document *doc = doc_create();
    uint32_t page_id = doc_page_create(doc, "Note Test");
    doc_page_set_type(doc, page_id, PAGE_TYPE_NOTE);
    
    AslanNoteDoc *note = NULL;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            note = doc->pages[i].note;
            break;
        }
    }
    
    note_insert_row(note, 0, ASLAN_ROW_HEADING_1, "My Heading");
    
    ASSERT_EQ(note->rows[0].type, ASLAN_ROW_HEADING_1, "Row type is HEADING_1");
    ASSERT_STR_EQ(note->rows[0].chars, "My Heading", "Heading text correct");
    
    doc_destroy(doc);
    test_passed_inc();
}

void test_note_highlight() {
    TEST("Insert Highlight (Fluo) in Note");
    
    Document *doc = doc_create();
    uint32_t page_id = doc_page_create(doc, "Note Test");
    doc_page_set_type(doc, page_id, PAGE_TYPE_NOTE);
    
    AslanNoteDoc *note = NULL;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            note = doc->pages[i].note;
            break;
        }
    }
    
    note_insert_row(note, 0, ASLAN_ROW_HIGHLIGHT, "Important text");
    
    ASSERT_EQ(note->rows[0].type, ASLAN_ROW_HIGHLIGHT, "Row type is HIGHLIGHT");
    ASSERT_STR_EQ(note->rows[0].chars, "Important text", "Highlight text correct");
    
    // This tests the data structure, not the rendering
    // Rendering test would need visual verification
    
    doc_destroy(doc);
    test_passed_inc();
}

void test_note_separator() {
    TEST("Insert Separator in Note");
    
    Document *doc = doc_create();
    uint32_t page_id = doc_page_create(doc, "Note Test");
    doc_page_set_type(doc, page_id, PAGE_TYPE_NOTE);
    
    AslanNoteDoc *note = NULL;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            note = doc->pages[i].note;
            break;
        }
    }
    
    note_insert_row(note, 0, ASLAN_ROW_SEPARATOR, "---");
    
    ASSERT_EQ(note->rows[0].type, ASLAN_ROW_SEPARATOR, "Row type is SEPARATOR");
    
    doc_destroy(doc);
    test_passed_inc();
}

// ============================================================================
// TABLE EMBED TESTS
// ============================================================================

void test_table_embed() {
    TEST("Table Embed in Note");
    
    Document *doc = doc_create();
    
    // Create source table
    uint32_t table_page = doc_page_create(doc, "Source Table");
    doc_page_set_type(doc, table_page, PAGE_TYPE_GRID);
    
    // Create note with embed
    uint32_t note_page = doc_page_create(doc, "Note with Table");
    doc_page_set_type(doc, note_page, PAGE_TYPE_NOTE);
    
    AslanNoteDoc *note = NULL;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == note_page) {
            note = doc->pages[i].note;
            break;
        }
    }
    
    ASSERT(note != NULL, "Note exists");
    
    // Insert table embed
    note_insert_row(note, 0, ASLAN_ROW_GRID, "Table View");
    
    // Set up the table view block
    AslanTableViewBlock *tvb = (AslanTableViewBlock *)calloc(1, sizeof(AslanTableViewBlock));
    tvb->source_page_id = table_page;
    tvb->collapsed = 0;
    note->rows[0].block = tvb;
    
    ASSERT_EQ(note->rows[0].type, ASLAN_ROW_GRID, "Row type is GRID");
    ASSERT(note->rows[0].block != NULL, "Block data exists");
    
    AslanTableViewBlock *block = (AslanTableViewBlock *)note->rows[0].block;
    ASSERT_EQ(block->source_page_id, table_page, "Source page ID correct");
    
    doc_destroy(doc);
    test_passed_inc();
}

void test_embed_resolution() {
    TEST("Embed Resolution (find source page)");
    
    Document *doc = doc_create();
    
    // Create source table
    uint32_t table_page = doc_page_create(doc, "Source Table");
    doc_page_set_type(doc, table_page, PAGE_TYPE_GRID);
    
    // Verify we can find the source page
    const char *name = doc_page_name(doc, table_page);
    ASSERT_STR_EQ(name, "Source Table", "Can find source page by ID");
    
    // Verify grid exists
    AslanGrid *grid = NULL;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == table_page) {
            grid = doc->pages[i].grid;
            break;
        }
    }
    
    ASSERT(grid != NULL, "Grid structure exists");
    
    doc_destroy(doc);
    test_passed_inc();
}

// ============================================================================
// WEB EMBED TESTS
// ============================================================================

void test_web_embed() {
    TEST("Web Embed in Note");
    
    Document *doc = doc_create();
    uint32_t page_id = doc_page_create(doc, "Note Test");
    doc_page_set_type(doc, page_id, PAGE_TYPE_NOTE);
    
    AslanNoteDoc *note = NULL;
    for (int i = 0; i < 32; i++) {
        if (doc->pages[i].in_use && doc->pages[i].page_id == page_id) {
            note = doc->pages[i].note;
            break;
        }
    }
    
    note_insert_row(note, 0, ASLAN_ROW_EMBED, "Video");
    
    // Set up embed block
    AslanEmbedBlock *eb = (AslanEmbedBlock *)calloc(1, sizeof(AslanEmbedBlock));
    snprintf(eb->url, sizeof(eb->url), "https://www.youtube.com/watch?v=test");
    eb->height = 320.0f;
    note->rows[0].block = eb;
    
    ASSERT_EQ(note->rows[0].type, ASLAN_ROW_EMBED, "Row type is EMBED");
    ASSERT(note->rows[0].block != NULL, "Embed block exists");
    
    AslanEmbedBlock *block = (AslanEmbedBlock *)note->rows[0].block;
    ASSERT(strstr(block->url, "youtube.com") != NULL, "URL contains youtube.com");
    ASSERT_EQ((int)block->height, 320, "Height is 320px");
    
    doc_destroy(doc);
    test_passed_inc();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char **argv) {
    printf("═══════════════════════════════════════\n");
    printf("  ASLAN TEST SUITE\n");
    printf("═══════════════════════════════════════\n");
    
    // Document tests
    test_create_document();
    test_create_page();
    
    // Note tests
    test_note_create();
    test_note_insert_line();
    test_note_heading();
    test_note_highlight();
    test_note_separator();
    
    // Embed tests
    test_table_embed();
    test_embed_resolution();
    test_web_embed();
    
    // Summary
    printf("\n═══════════════════════════════════════\n");
    printf("  TEST SUMMARY\n");
    printf("═══════════════════════════════════════\n");
    printf("  Total:  %d tests\n", test_count);
    printf("  ✅ Passed: %d\n", test_passed);
    printf("  ❌ Failed: %d\n", test_failed);
    printf("═══════════════════════════════════════\n");
    
    if (test_failed == 0) {
        printf("\n🎉 ALL TESTS PASSED!\n\n");
        return 0;
    } else {
        printf("\n💥 SOME TESTS FAILED\n\n");
        return 1;
    }
}
