import os
import re

def process_file(filepath):
    print("Processing", filepath)
    with open(filepath, 'r') as f:
        lines = f.readlines()

    out_lines = []
    in_nk_block = False
    
    for i, line in enumerate(lines):
        # We need a robust way to strip nk_ lines without breaking {} brackets
        # Let's just comment out lines that contain nk_ but NOT #include or // nk_
        if re.search(r'\bnk_[a-zA-Z0-9_]+\(', line):
            # It's a function call like nk_begin(
            # We can comment it out, but what if it's an if (nk_xxx) { ?
            if 'if (nk_' in line or 'if(nk_' in line or 'if (!nk_' in line:
                out_lines.append(line.replace('if', 'if (0) //'))
            elif '=' in line and 'nk_' in line.split('=')[1]:
                # assignment like x = nk_...
                # comment the whole line
                out_lines.append('// ' + line)
            elif 'nk_' in line and ';' in line and not '{' in line:
                out_lines.append('// ' + line)
            else:
                out_lines.append(line)
        else:
            out_lines.append(line)
            
    with open(filepath, 'w') as f:
        f.writelines(out_lines)

for root, dirs, files in os.walk('src/platform'):
    for file in files:
        if file.endswith('.c') or file.endswith('.h'):
            process_file(os.path.join(root, file))
