import os

with open('src/platform/ui_design.c', 'r') as f:
    data = f.read()

start_idx = data.find('static void design_render_sidebar_nk(')
if start_idx != -1:
    brace_idx = data.find('{', start_idx)
    count = 1
    end_idx = brace_idx + 1
    while count > 0 and end_idx < len(data):
        if data[end_idx] == '{': count += 1
        elif data[end_idx] == '}': count -= 1
        end_idx += 1
    
    # wrap in #if 0
    data = data[:start_idx] + '#if 0\n' + data[start_idx:end_idx] + '\n#endif\n' + data[end_idx:]

with open('src/platform/ui_design.c', 'w') as f:
    f.write(data)

print("Wrapped design_render_sidebar_nk in #if 0")
