import os

with open("src/platform/ui_design.c", "r") as f:
    code = f.read()

# Fix 1: last_child -> remove
code = code.replace("dd->root->last_child = NULL;", "")

# Fix 2: Unused variables dot & line -> add to parents
old_code_3 = """        // Project Timeline
        Node *line = design_create_styled_node(dd, NODE_STYLE_LINE, 50, h/2, w-100, 4, 0x444444FF);
        
        for (int i=0; i<5; i++) {
            Node *dot = design_create_styled_node(dd, NODE_STYLE_ELLIPSE, 50 + i*((w-100)/4) - 10, h/2 - 10, 20, 20, 0x4285F4FF);
            Node *text = design_create_styled_node(dd, NODE_STYLE_TEXT, 50 + i*((w-100)/4) - 50, (i%2==0) ? h/2 - 60 : h/2 + 20, 100, 30, color);
            strncpy(text->text, "Milestone", 63);
        }"""

new_code_3 = """        // Project Timeline
        Node *main_h = design_create_styled_node(dd, NODE_STYLE_RECT, 0, 0, w, h, color);
        main_h->is_container = 1; main_h->layout_type = 2; main_h->layout_padding = 20;
        
        Node *line = design_create_styled_node(dd, NODE_STYLE_LINE, 50, h/2, w-100, 4, 0x444444FF);
        line->parent = main_h; node_add_child(main_h, line);
        
        for (int i=0; i<5; i++) {
            Node *dot = design_create_styled_node(dd, NODE_STYLE_ELLIPSE, 50 + i*((w-100)/4) - 10, h/2 - 10, 20, 20, 0x4285F4FF);
            dot->parent = main_h; node_add_child(main_h, dot);
            Node *text = design_create_styled_node(dd, NODE_STYLE_TEXT, 50 + i*((w-100)/4) - 50, (i%2==0) ? h/2 - 60 : h/2 + 20, 100, 30, color);
            strncpy(text->text, "Milestone", 63);
            text->parent = main_h; node_add_child(main_h, text);
        }"""

code = code.replace(old_code_3, new_code_3)

with open("src/platform/ui_design.c", "w") as f:
    f.write(code)

print("Fixed ui_design.c")
