import os

with open("src/platform/ui_design.c", "r") as f:
    lines = f.readlines()

new_func = """
// =============================================================================
// DESIGN SMART LAYOUT GENERATOR
// =============================================================================
static void design_generate_smart_layout(DesignData *dd, int layout_id, AslanTable *linked_tbl) {
    if (!dd || !dd->root) return;

    // Optional: Clear existing nodes first?
    // For now we just safely append. We can clear children of root:
    dd->root->first_child = NULL;
    dd->root->last_child = NULL;

    float w = dd->width > 0 ? dd->width : 800.0f;
    float h = dd->height > 0 ? dd->height : 600.0f;
    uint32_t color = 0xCCCCCCFF;

    if (layout_id == 0) {
        // Executive Scorecard
        Node *main_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
        main_v->is_container = 1; main_v->layout_type = 1; main_v->layout_padding = 20;

        Node *top_h = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, 150, color);
        top_h->is_container = 1; top_h->layout_type = 2; top_h->layout_padding = 20;
        top_h->parent = main_v; node_add_child(main_v, top_h);

        for (int i=0; i<4; i++) {
            Node *card = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, (w-60)/4, 150, 0xE0E0E0FF);
            card->parent = top_h; node_add_child(top_h, card);
        }

        Node *bot_text = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, w, h-170, 0xFFFFFFFF);
        bot_text->parent = main_v; node_add_child(main_v, bot_text);
        if (linked_tbl && linked_tbl->column_count > 0) {
            strncpy(bot_text->linked_col_name, linked_tbl->columns[0].name, 63);
        }

    } else if (layout_id == 1) {
        // CRM Card Grid (Approximated as V of H)
        Node *main_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
        main_v->is_container = 1; main_v->layout_type = 1; main_v->layout_padding = 20;

        for (int r=0; r<3; r++) {
            Node *row_h = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, (h-40)/3, color);
            row_h->is_container = 1; row_h->layout_type = 2; row_h->layout_padding = 20;
            row_h->parent = main_v; node_add_child(main_v, row_h);

            for (int c=0; c<3; c++) {
                Node *card = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, (w-40)/3, (h-40)/3, 0xFFFFFFFF);
                card->has_shadow = 1; card->shadow_blur = 10; card->shadow_offset_y = 5;
                card->parent = row_h; node_add_child(row_h, card);
            }
        }
    } else if (layout_id == 2) {
        // Comparison Matrix
        Node *main_h = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
        main_h->is_container = 1; main_h->layout_type = 2; main_h->layout_padding = 40;

        Node *left_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, (w-40)/2, h, 0xFFFFFFFF);
        left_v->is_container = 1; left_v->layout_type = 1; left_v->layout_padding = 20;
        left_v->has_shadow = 1; left_v->parent = main_h; node_add_child(main_h, left_v);

        Node *right_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, (w-40)/2, h, 0xFFFFFFFF);
        right_v->is_container = 1; right_v->layout_type = 1; right_v->layout_padding = 20;
        right_v->has_shadow = 1; right_v->parent = main_h; node_add_child(main_h, right_v);
    } else if (layout_id == 3) {
        // Project Timeline
        Node *line = design_create_styled_node(dd, NODE_STYLE_LINE, 50, h/2, w-100, 4, 0x444444FF);
        
        for (int i=0; i<5; i++) {
            Node *dot = design_create_styled_node(dd, NODE_STYLE_ELLIPSE, 50 + i*((w-100)/4) - 10, h/2 - 10, 20, 20, 0x4285F4FF);
            Node *text = design_create_styled_node(dd, NODE_STYLE_TEXT, 50 + i*((w-100)/4) - 50, (i%2==0) ? h/2 - 60 : h/2 + 20, 100, 30, color);
            strncpy(text->text, "Milestone", 63);
        }
    } else if (layout_id == 4) {
        // Financial Split
        Node *main_h = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
        main_h->is_container = 1; main_h->layout_type = 2; main_h->layout_padding = 40;

        Node *pie = design_create_styled_node(dd, NODE_STYLE_ELLIPSE, 0, 0, (w-40)/2, (w-40)/2, 0x4285F4FF);
        pie->parent = main_h; node_add_child(main_h, pie);

        Node *right_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, (w-40)/2, h, 0xFFFFFFFF);
        right_v->is_container = 1; right_v->layout_type = 1; right_v->layout_padding = 20;
        right_v->has_shadow = 1; right_v->parent = main_h; node_add_child(main_h, right_v);

        for (int i=0; i<5; i++) {
             Node *row = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, (w-40)/2, 40, 0xEEEEEEFF);
             row->parent = right_v; node_add_child(right_v, row);
        }
    } else if (layout_id == 5) {
        // Technical Spec Sheet
        Node *main_h = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
        main_h->is_container = 1; main_h->layout_type = 2; main_h->layout_padding = 40;

        Node *img = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w*0.4, h, 0xD0D0D0FF);
        img->parent = main_h; node_add_child(main_h, img);

        Node *right_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w*0.6-40, h, color);
        right_v->is_container = 1; right_v->layout_type = 1; right_v->layout_padding = 20;
        right_v->parent = main_h; node_add_child(main_h, right_v);

        Node *title = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, w*0.6-40, 60, color);
        strncpy(title->text, "Product Title", 63);
        title->parent = right_v; node_add_child(right_v, title);

        Node *desc = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, w*0.6-40, h-80, color);
        strncpy(desc->text, "Product Specifications...", 63);
        desc->parent = right_v; node_add_child(right_v, desc);
    } else if (layout_id == 6) {
        // Kanban Light
        Node *main_h = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
        main_h->is_container = 1; main_h->layout_type = 2; main_h->layout_padding = 20;

        for (int c=0; c<3; c++) {
            Node *col_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, (w-40)/3, h, 0xF5F5F5FF);
            col_v->is_container = 1; col_v->layout_type = 1; col_v->layout_padding = 10;
            col_v->parent = main_h; node_add_child(main_h, col_v);

            Node *title = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, (w-40)/3, 30, color);
            strncpy(title->text, c==0 ? "To Do" : (c==1 ? "Doing" : "Done"), 63);
            title->parent = col_v; node_add_child(col_v, title);

            for(int r=0; r<3; r++) {
               Node *card = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, (w-40)/3, 80, 0xFFFFFFFF);
               card->has_shadow = 1; card->parent = col_v; node_add_child(col_v, card);
            }
        }
    } else if (layout_id == 7) {
        // Feedback Feed
        Node *main_v = design_create_styled_node(dd, NODE_STYLE_RECT, w*0.2, 0, w*0.6, h, color);
        main_v->is_container = 1; main_v->layout_type = 1; main_v->layout_padding = 20;

        for (int r=0; r<5; r++) {
            Node *card = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w*0.6, 100, 0xFFFFFFFF);
            card->has_shadow = 1; card->parent = main_v; node_add_child(main_v, card);
        }
    } else if (layout_id == 8) {
        // Sales Funnel
        Node *main_v = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
        main_v->is_container = 1; main_v->layout_type = 1; main_v->layout_padding = 10;
        
        for (int r=0; r<4; r++) {
            float rw = w - (r * w*0.2);
            Node *step = design_create_styled_node(dd, NODE_STYLE_RECT, (w-rw)/2, 0, rw, (h-30)/4, 0x4285F4FF);
            step->parent = main_v; node_add_child(main_v, step);
        }
    } else if (layout_id == 9) {
        // Clean Page (Export Ready)
        Node *main_v = design_create_styled_node(dd, NODE_STYLE_RECT, 40, 40, w-80, h-80, color);
        main_v->is_container = 1; main_v->layout_type = 1; main_v->layout_padding = 20;

        Node *header = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, w-80, 60, color);
        strncpy(header->text, "Official Report Header", 63);
        header->parent = main_v; node_add_child(main_v, header);

        Node *content = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w-80, h-200, 0xFFFFFFFF);
        content->parent = main_v; node_add_child(main_v, content);

        Node *footer = design_create_styled_node(dd, NODE_STYLE_TEXT, 0, 0, w-80, 40, color);
        strncpy(footer->text, "Confidential - Aslan Export", 63);
        footer->parent = main_v; node_add_child(main_v, footer);
    }
}
"""

insert_idx = -1
for i, l in enumerate(lines):
    if "static void design_render_sidebar" in l:
        insert_idx = i - 1
        break

lines.insert(insert_idx, new_func)

with open("src/platform/ui_design.c", "w") as f:
    f.writelines(lines)

print("Injected Generator Function!")
