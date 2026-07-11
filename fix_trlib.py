import re

with open('../d3d11_bench/trlib_d3d11.c', 'r') as f:
    lines = f.readlines()

# ── Edit 1: Add g_upgrade_cb static + tr_set_upgrade_callback after g_pending_driver_mode ──
# Find the line with g_pending_driver_mode
insert_after_line = None
for i, line in enumerate(lines):
    if 'g_pending_driver_mode' in line and 'static' in line:
        insert_after_line = i + 1  # insert AFTER this line
        break

if insert_after_line is None:
    print("ERROR: Could not find g_pending_driver_mode line")
else:
    new_lines = [
        '// Upgrade callback fired after a successful WARP-first hot-swap\n',
        'static TRUpgradeCallback g_upgrade_cb = NULL;\n',
        '\n',
        'void tr_set_upgrade_callback(TRUpgradeCallback cb)\n',
        '{\n',
        '    g_upgrade_cb = cb;\n',
        '}\n',
    ]
    for j, nl in enumerate(new_lines):
        lines.insert(insert_after_line + j, nl)
    print(f"Added g_upgrade_cb and tr_set_upgrade_callback after line {insert_after_line}")

# ── Edit 2: Remove swapchain+RTV creation from upgrade_thread_proc ──
# Find the lines in upgrade_thread_proc that create swapchain+rtv
# Pattern: after compile_shaders_on_device, look for create_swapchain_comp or create_rtv or CreateSwapChainForComposition
# Better approach: find lines between "if (s->abort_bg) goto fail;" and "if (!compile_shaders_on_device"
in_upgrade_thread = False
lines_to_remove = []
for i, line in enumerate(lines):
    if 'DWORD WINAPI upgrade_thread_proc' in line:
        in_upgrade_thread = True
    if in_upgrade_thread:
        if 'create_swapchain_comp' in line or 'create_rtv' in line or 'CreateSwapChain' in line or 'hw->swapchain' in line or 'hw->rtv' in line:
            if 'compile_shaders' not in line:  # don't remove compile_shaders lines
                lines_to_remove.append(i)

if lines_to_remove:
    # Remove from end to start to preserve indices
    for idx in reversed(lines_to_remove):
        print(f"Removing line {idx+1}: {lines[idx].rstrip()}")
        del lines[idx]
else:
    print("No swapchain/RTV lines found in upgrade_thread_proc (might already be clean)")

# ── Edit 3: In tr_try_upgrade_driver, add swapchain+RTV creation BEFORE releasing old WARP ──
# and add callback call after the hot-swap
# Find the "// Destroy current WARP graphics objects" block
destroy_start = None
destroy_end = None
for i, line in enumerate(lines):
    if 'Destroy current WARP graphics' in line:
        destroy_start = i
    if destroy_start is not None and i > destroy_start:
        # Look for when the destroy block ends (the line before the hot-swap assignment block)
        if 's->device    = hw->device' in line or 's->device = hw->device' in line:
            destroy_end = i
            break

if destroy_start is not None and destroy_end is not None:
    # The destroy block is lines destroy_start to destroy_end (exclusive)
    # We need to:
    # 1. CREATE swapchain+RTV BEFORE this block
    # 2. Keep the destroy block as-is
    # 3. After the hot-swap, add callback call
    
    # Remove the old create_swapchain_comp and create_rtv that might be inside the destroy block
    # and add them BEFORE the destroy block instead
    
    # Actually, let me look at what's currently in the destroy block
    print(f"Destroy block: lines {destroy_start+1} to {destroy_end}")
    for j in range(destroy_start, destroy_end):
        print(f"  {j+1}: {lines[j].rstrip()}")
    
    # Check if there are create_swapchain_comp/create_rtv calls in the destroy block
    has_swapchain_create = False
    has_rtv_create = False
    for j in range(destroy_start, destroy_end):
        if 'create_swapchain_comp' in lines[j]:
            has_swapchain_create = True
        if 'create_rtv' in lines[j] and 'destroy' not in lines[j].lower() and 'Release' not in lines[j]:
            has_rtv_create = True
    
    if not has_swapchain_create:
        # Add swapchain+RTV creation before the destroy block
        swapchain_code = [
            '        // Create swapchain + RTV on main thread BEFORE destroying WARP resources\n',
            '        // (if creation fails we can fall back to WARP)\n',
            '        if (!create_swapchain_comp(&hw->swapchain, hw->device, hw->target_width, hw->target_height)) {\n',
            '            fprintf(stderr, "TR| failed to create HW swapchain, keeping WARP\\n");\n',
            '            hw->ok = 0;\n',
            '            goto upgrade_fail;\n',
            '        }\n',
            '        if (!create_rtv(&hw->rtv, hw->device, hw->swapchain)) {\n',
            '            fprintf(stderr, "TR| failed to create HW RTV, keeping WARP\\n");\n',
            '            if (hw->swapchain) { hw->swapchain->lpVtbl->Release(hw->swapchain); hw->swapchain = NULL; }\n',
            '            hw->ok = 0;\n',
            '            goto upgrade_fail;\n',
            '        }\n',
            '\n',
        ]
        for j, nl in enumerate(swapchain_code):
            lines.insert(destroy_start + j, nl)
        print(f"Added swapchain+RTV creation before destroy block (line {destroy_start+1})")
        # Update destroy_end since we inserted lines
        destroy_end += len(swapchain_code)
else:
    print("WARNING: Could not find destroy block boundaries")

# ── Edit 4: Add the callback call after the hot-swap assignment ──
# Find the line with rs assignment (last line of hot-swap block)
rs_assignment = None
for i, line in enumerate(lines):
    if 's->rs' in line and 'hw->rs' in line and 'NULL' in line:
        rs_assignment = i
        # Check for callback call already present
        if any('g_upgrade_cb' in lines[j] for j in range(i, min(i+10, len(lines)))):
            rs_assignment = None  # Already has callback, skip

if rs_assignment is not None:
    callback_code = [
        '\n',
        '        // Fire upgrade callback so external modules can re-create D3D11 resources\n',
        '        if (g_upgrade_cb) g_upgrade_cb(s);\n',
    ]
    for j, nl in enumerate(callback_code):
        lines.insert(rs_assignment + 1 + j, nl)
    print(f"Added callback call after hot-swap (line {rs_assignment+1})")

# ── Edit 5: Add upgrade_fail label before the cleanup ──
# Find "int ok = hw->ok;" 
ok_line = None
for i, line in enumerate(lines):
    if 'int ok = hw->ok' in line and ok_line is None:
        ok_line = i
        break

if ok_line is not None:
    # Check if upgrade_fail label already exists before this line
    if ok_line > 0 and 'upgrade_fail' in lines[ok_line - 1]:
        print("upgrade_fail label already exists")
    else:
        lines.insert(ok_line, 'upgrade_fail:\n')
        print(f"Added upgrade_fail label at line {ok_line+1}")
else:
    print("WARNING: Could not find 'int ok = hw->ok'")

# ── Write the result ──
with open('../d3d11_bench/trlib_d3d11.c', 'w') as f:
    f.writelines(lines)

print("\nDone! trlib_d3d11.c updated successfully.")
