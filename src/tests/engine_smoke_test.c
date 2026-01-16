#include <stdio.h>
#include <string.h>
#include "../core/engine.h"
#include "../document/document.h"
#include "../platform/platform.h"

static int check_int(const char* label, int expected, int actual) {
    if (expected != actual) {
        printf("check failed: %s expected=%d actual=%d\n", label, expected, actual);
        return 1;
    }
    return 0;
}

static int check_bool(const char* label, int ok) {
    if (!ok) {
        printf("check failed: %s\n", label);
        return 1;
    }
    return 0;
}

static int check_str(const char* label, const char* expected, const char* actual) {
    if (!expected && !actual) return 0;
    if (!expected || !actual || strcmp(expected, actual) != 0) {
        printf("check failed: %s expected=\"%s\" actual=\"%s\"\n",
               label,
               expected ? expected : "(null)",
               actual ? actual : "(null)");
        return 1;
    }
    return 0;
}

static int test_engine_basic(void) {
    int failures = 0;
    platform_init();
    Engine* engine = engine_create();
    if (!engine) {
        printf("engine_create returned NULL\n");
        platform_shutdown();
        return 1;
    }
    if (!engine->doc) {
        printf("engine->doc is NULL\n");
        engine_destroy(engine);
        platform_shutdown();
        return 1;
    }

    failures += check_int("initial_page_count", 0, engine->doc->page_count);
    failures += check_int("initial_active_page", 0, (int)doc_page_active_id(engine->doc));

    int ok = engine_execute_command(engine, "page_create 1 \"TestPage\"");
    failures += check_bool("page_create command", ok);
    failures += check_int("page_count_after_create", 1, engine->doc->page_count);
    failures += check_int("active_page_after_create", 1, (int)doc_page_active_id(engine->doc));

    const char* name = doc_page_name(engine->doc, 1);
    if (!name || strcmp(name, "TestPage") != 0) {
        printf("check failed: page_name expected=TestPage actual=%s\n", name ? name : "(null)");
        failures++;
    }

    ok = engine_save_snapshot_as_aslan(engine, "_test_tmp.aslan");
    failures += check_bool("save_snapshot", ok);

    if (ok) {
        FILE* f = fopen("_test_tmp.aslan", "r");
        if (!f) {
            printf("check failed: could not reopen _test_tmp.aslan\n");
            failures++;
        } else {
            char line[256];
            if (!fgets(line, sizeof(line), f)) {
                printf("check failed: empty _test_tmp.aslan\n");
                failures++;
            } else {
                line[strcspn(line, "\r\n")] = 0;
                failures += check_str("aslan_header", "-- aslan:1 --", line);
            }
            fclose(f);
        }
    }

    ok = engine_load_snapshot_from_aslan(engine, "_test_tmp.aslan");
    failures += check_bool("load_snapshot", ok);

    remove("_test_tmp.aslan");

    engine_destroy(engine);
    platform_shutdown();
    return failures;
}

static int test_page_and_meta(void) {
    int failures = 0;
    platform_init();
    Engine* engine = engine_create();
    if (!engine || !engine->doc) {
        printf("engine_create/doc_create failed in test_page_and_meta\n");
        if (engine) engine_destroy(engine);
        platform_shutdown();
        return 1;
    }

    int ok = engine_execute_command(engine, "page_create 2 \"MetaPage\"");
    failures += check_bool("page_create_2", ok);
    failures += check_int("page_count_after_create_2", 1, engine->doc->page_count);
    failures += check_int("active_page_after_create_2", 2, (int)doc_page_active_id(engine->doc));

    ok = engine_execute_command(engine, "page_type 2 page");
    failures += check_bool("page_type_page", ok);
    failures += check_int("page_type_get", (int)PAGE_TYPE_PAGE, (int)doc_page_get_type(engine->doc, 2));

    ok = engine_execute_command(engine, "page_meta 2 width 210.0");
    failures += check_bool("page_meta_width", ok);
    ok = engine_execute_command(engine, "page_meta 2 height 297.0");
    failures += check_bool("page_meta_height", ok);

    float w = doc_page_get_meta(engine->doc, 2, "width");
    float h = doc_page_get_meta(engine->doc, 2, "height");
    failures += check_int("page_width_approx_210", (int)(210.0f), (int)(w));
    failures += check_int("page_height_approx_297", (int)(297.0f), (int)(h));

    engine_destroy(engine);
    platform_shutdown();
    return failures;
}

static int test_globals_and_clear(void) {
    int failures = 0;
    platform_init();
    Engine* engine = engine_create();
    if (!engine || !engine->doc) {
        printf("engine_create/doc_create failed in test_globals_and_clear\n");
        if (engine) engine_destroy(engine);
        platform_shutdown();
        return 1;
    }

    int ok = engine_execute_command(engine, "set_global theme \"dark\"");
    failures += check_bool("set_global_theme", ok);
    const char* theme = doc_get_global(engine->doc, "theme");
    failures += check_str("get_global_theme", "dark", theme);

    ok = engine_execute_command(engine, "unset_global theme");
    failures += check_bool("unset_global_theme", ok);
    theme = doc_get_global(engine->doc, "theme");
    failures += check_str("get_global_theme_after_unset", NULL, theme);

    ok = engine_execute_command(engine, "page_create 3 \"ClearMe\"");
    failures += check_bool("page_create_before_clear", ok);
    failures += check_int("page_count_before_clear", 1, engine->doc->page_count);

    ok = engine_execute_command(engine, "clear");
    failures += check_bool("clear_command", ok);
    if (!engine->doc) {
        printf("engine->doc is NULL after clear\n");
        failures++;
    } else {
        failures += check_int("page_count_after_clear", 0, engine->doc->page_count);
        failures += check_int("active_page_after_clear", 0, (int)doc_page_active_id(engine->doc));
    }

    engine_destroy(engine);
    platform_shutdown();
    return failures;
}

static int test_frame_commands(void) {
    int failures = 0;
    platform_init();
    Engine* engine = engine_create();
    if (!engine || !engine->doc) {
        printf("engine_create/doc_create failed in test_frame_commands\n");
        if (engine) engine_destroy(engine);
        platform_shutdown();
        return 1;
    }

    int ok = engine_execute_command(engine, "page_create 10 \"FramePage\"");
    failures += check_bool("frame_page_create", ok);

    ok = engine_execute_command(engine, "frame 10 20 30 40");
    failures += check_bool("frame_command", ok);

    Node* n1 = doc_find_node_by_id(engine->doc, 1);
    if (!n1) {
        printf("check failed: frame_node_id_1_missing\n");
        failures++;
    } else {
        failures += check_int("frame_x", 10, (int)n1->x);
        failures += check_int("frame_y", 20, (int)n1->y);
        failures += check_int("frame_w", 30, (int)n1->width);
        failures += check_int("frame_h", 40, (int)n1->height);
    }

    ok = engine_execute_command(engine, "create_frame 42 Frame42 1 2 3 4");
    failures += check_bool("create_frame_command", ok);

    Node* n2 = doc_find_node_by_id(engine->doc, 42);
    if (!n2) {
        printf("check failed: create_frame_node_id_42_missing\n");
        failures++;
    } else {
        failures += check_int("create_frame_x", 1, (int)n2->x);
        failures += check_int("create_frame_y", 2, (int)n2->y);
        failures += check_int("create_frame_w", 3, (int)n2->width);
        failures += check_int("create_frame_h", 4, (int)n2->height);
    }

    engine_destroy(engine);
    platform_shutdown();
    return failures;
}

static int test_image_commands(void) {
    int failures = 0;
    platform_init();
    Engine* engine = engine_create();
    if (!engine || !engine->doc) {
        printf("engine_create/doc_create failed in test_image_commands\n");
        if (engine) engine_destroy(engine);
        platform_shutdown();
        return 1;
    }

    int ok = engine_execute_command(engine, "page_create 20 \"ImagePage\"");
    failures += check_bool("image_page_create", ok);

    ok = engine_execute_command(engine, "new_image 5 6 70 80 \"test.bmp\"");
    failures += check_bool("new_image_command", ok);

    Node* n1 = doc_find_node_by_id(engine->doc, 1);
    if (!n1) {
        printf("check failed: image_node_id_1_missing\n");
        failures++;
    } else {
        failures += check_int("image_type", NODE_TYPE_IMAGE, (int)n1->type);
        failures += check_int("image_x", 5, (int)n1->x);
        failures += check_int("image_y", 6, (int)n1->y);
        failures += check_int("image_w", 70, (int)n1->width);
        failures += check_int("image_h", 80, (int)n1->height);
    }

    ok = engine_execute_command(engine, "create_image 42 Img42 1 2 30 40 \"foo.bmp\"");
    failures += check_bool("create_image_command", ok);

    Node* n2 = doc_find_node_by_id(engine->doc, 42);
    if (!n2) {
        printf("check failed: create_image_node_id_42_missing\n");
        failures++;
    } else {
        failures += check_int("create_image_type", NODE_TYPE_IMAGE, (int)n2->type);
        failures += check_int("create_image_x", 1, (int)n2->x);
        failures += check_int("create_image_y", 2, (int)n2->y);
        failures += check_int("create_image_w", 30, (int)n2->width);
        failures += check_int("create_image_h", 40, (int)n2->height);
    }

    engine_destroy(engine);
    platform_shutdown();
    return failures;
}

static int test_group_alias_command(void) {
    int failures = 0;
    platform_init();
    Engine* engine = engine_create();
    if (!engine || !engine->doc) {
        printf("engine_create/doc_create failed in test_group_alias_command\n");
        if (engine) engine_destroy(engine);
        platform_shutdown();
        return 1;
    }

    int ok = engine_execute_command(engine, "page_create 30 \"GroupPage\"");
    failures += check_bool("group_page_create", ok);

    ok = engine_execute_command(engine, "create_group 7 Group7 10 20 90 100");
    failures += check_bool("create_group_command", ok);

    Node* n = doc_find_node_by_id(engine->doc, 7);
    if (!n) {
        printf("check failed: group_node_id_7_missing\n");
        failures++;
    } else {
        failures += check_int("group_type_is_rect", NODE_TYPE_RECT, (int)n->type);
        failures += check_int("group_x", 10, (int)n->x);
        failures += check_int("group_y", 20, (int)n->y);
        failures += check_int("group_w", 90, (int)n->width);
        failures += check_int("group_h", 100, (int)n->height);
    }

    engine_destroy(engine);
    platform_shutdown();
    return failures;
}

static int test_group_parenting_move_children(void) {
    int failures = 0;
    platform_init();
    Engine* engine = engine_create();
    if (!engine || !engine->doc) {
        printf("engine_create/doc_create failed in test_group_parenting_move_children\n");
        if (engine) engine_destroy(engine);
        platform_shutdown();
        return 1;
    }

    int ok = engine_execute_command(engine, "page_create 40 \"GroupParentPage\"");
    failures += check_bool("group_parent_page_create", ok);

    ok = engine_execute_command(engine, "create_group 1 Group1 0 0 100 100");
    failures += check_bool("group_parent_create_group", ok);

    ok = engine_execute_command(engine, "create_rect 2 Child 10 20 30 40");
    failures += check_bool("group_parent_create_child_rect", ok);

    ok = engine_execute_command(engine, "group_attach 1 2");
    failures += check_bool("group_attach_command", ok);

    Node* g = doc_find_node_by_id(engine->doc, 1);
    Node* c = doc_find_node_by_id(engine->doc, 2);
    if (!g || !c) {
        printf("check failed: group_or_child_missing\n");
        failures++;
    } else {
        if (c->parent != g) {
            printf("check failed: child_parent_not_group\n");
            failures++;
        }
    }

    ok = engine_execute_command(engine, "set_rect 1 50 60 100 100");
    failures += check_bool("group_parent_move_with_set_rect", ok);

    c = doc_find_node_by_id(engine->doc, 2);
    if (!c) {
        printf("check failed: child_missing_after_move\n");
        failures++;
    } else {
        failures += check_int("group_child_x_after_move", 60, (int)c->x);
        failures += check_int("group_child_y_after_move", 80, (int)c->y);
    }

    engine_destroy(engine);
    platform_shutdown();
    return failures;
}

int main(void) {
    int failures = 0;
    failures += test_engine_basic();
    failures += test_page_and_meta();
    failures += test_globals_and_clear();
    failures += test_frame_commands();
    failures += test_image_commands();
    failures += test_group_alias_command();
    failures += test_group_parenting_move_children();
    if (failures != 0) {
        printf("Aslan engine tests failed: %d\n", failures);
        return 1;
    }
    printf("Aslan engine tests passed\n");
    return 0;
}
