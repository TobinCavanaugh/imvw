import sys
with open('../d3d11_bench/tr_raylib.c', 'r') as f:
    content = f.read()

# Fix 1: Remove stray 'n' before the GPU upgrade comment
content = content.replace(
    'n    // GPU upgrade (after Present so the WARP frame renders fully before swap)',
    '    // GPU upgrade (after Present so the WARP frame renders fully before swap)'
)

# Fix 2: Fix the broken fprintf newline escape
old_fp = '''            fprintf(stderr, \"[%.0f] TR| GPU upgrade SWAPPED to hardware
\", tr_timer_now_ms(g_state));'''
new_fp = '''            fprintf(stderr, \"[%.0f] TR| GPU upgrade SWAPPED to hardware\\n\", tr_timer_now_ms(g_state));'''
content = content.replace(old_fp, new_fp)

with open('../d3d11_bench/tr_raylib.c', 'w') as f:
    f.write(content)
print('Done - fixed tr_raylib.c')
