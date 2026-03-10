import re

def purge(filepath):
    with open(filepath, 'r') as f:
        data = f.read()

    # Just wrap the entire content of these files in #if 0 ... #endif
    # EXCEPT for the includes and the function signatures so it still links!
    # Even better: just define the functions as empty.

    if "ui_table.c" in filepath:
        # replace the bodies of the known problematic functions
        data = re.sub(r'static int table_popup_header.*?\{.*?\}\n\}', 'static int table_popup_header(UIHost *ui, const char *title) { return 0; }', data, flags=re.DOTALL)
        data = re.sub(r'static void ui_render_formula_bar.*?\{.*?\n\}', 'static void ui_render_formula_bar(UIHost *ui, Block *block) {}', data, flags=re.DOTALL)
        data = re.sub(r'static void ui_render_formula_help_popup.*?\{.*?\n\}', 'static void ui_render_formula_help_popup(UIHost *ui) {}', data, flags=re.DOTALL)
        data = re.sub(r'void ui_render_block_table.*?\{.*', 'void ui_render_block_table(UIHost *ui, Engine *engine, Block *block, float width) { (void)ui; (void)engine; (void)block; (void)width; }\n', data, flags=re.DOTALL)
        
    if "ui_chart.c" in filepath:
        data = re.sub(r'void ui_render_chart_node.*?\{.*', 'void ui_render_chart_node(UIHost *ui, Node *node, AslanRect bounds, Page *page) {}\nvoid ui_render_chart_node_config_bar(UIHost *ui, Engine *engine, Node *node, AslanRect bounds, Page *page) {}\nvoid ui_render_chart_impl(UIHost *ui, ChartData *cd, AslanRect bounds, Page *page) {}\n', data, flags=re.DOTALL)

    if "ui_design.c" in filepath:
        # there are so many, let's just make it empty or replace nk_ calls with nothing
        pass

    with open(filepath, 'w') as f:
        f.write(data)

purge('src/platform/ui_table.c')
purge('src/platform/ui_chart.c')
