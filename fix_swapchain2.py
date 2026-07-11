import re

with open('../d3d11_bench/trlib_d3d11.c', 'r') as f:
    content = f.read()

# The current code (after first fix) has:
# 1. Destroy old WARP dcomp/ctx/ps/vs/rs/device
# 2. Create swapchain+RTV
# 3. Release old WARP rtv/swapchain
# 4. Hot-swap
#
# We need:
# 1. Create swapchain+RTV FIRST (using hw device, before releasing old WARP)
# 2. If fail: cleanup hw, keep WARP running, return
# 3. Release ALL old WARP resources
# 4. Hot-swap

# Find the exact block to replace
old = '''        // Destroy current WARP graphics objects
        if (s->dcomp)     { tr_dcomp_destroy(s->dcomp);            s->dcomp     = NULL; }
        if (s->ctx)       { s->ctx->lpVtbl->Release(s->ctx);       s->ctx       = NULL; }
        if (s->ps)        { s->ps->lpVtbl->Release(s->ps);         s->ps        = NULL; }
        if (s->vs)        { s->vs->lpVtbl->Release(s->vs);         s->vs        = NULL; }
        if (s->rs)        { s->rs->lpVtbl->Release(s->rs);         s->rs        = NULL; }
        if (s->device)    { s->device->lpVtbl->Release(s->device); s->device    = NULL; }

        // Create swapchain + RTV on main thread (dxgi objects require the calling thread)
        if (!create_swapchain_comp(&hw->swapchain, hw->device, hw->target_width, hw->target_height)) {
            fprintf(stderr, "TR| failed to create HW swapchain\\n");
            goto fail_swapchain;
        }
        if (!create_rtv(&hw->rtv, hw->device, hw->swapchain)) {
            fprintf(stderr, "TR| failed to create HW RTV\\n");
            goto fail_swapchain;
        }

        if (s->rtv)       { s->rtv->lpVtbl->Release(s->rtv);       s->rtv       = NULL; }
        if (s->swapchain) { s->swapchain->lpVtbl->Release(s->swapchain); s->swapchain = NULL; }'''

new = '''        // Create swapchain + RTV on main thread BEFORE destroying WARP
        // (so if creation fails we can fall back to WARP)
        if (!create_swapchain_comp(&hw->swapchain, hw->device, hw->target_width, hw->target_height)) {
            fprintf(stderr, "TR| failed to create HW swapchain, keeping WARP\\n");
            hw->ok = 0;
            goto upgrade_fail;
        }
        if (!create_rtv(&hw->rtv, hw->device, hw->swapchain)) {
            fprintf(stderr, "TR| failed to create HW RTV, keeping WARP\\n");
            if (hw->swapchain) { hw->swapchain->lpVtbl->Release(hw->swapchain); hw->swapchain = NULL; }
            hw->ok = 0;
            goto upgrade_fail;
        }

        // Destroy all old WARP graphics objects (swapchain+RTV already created on HW)
        if (s->rtv)       { s->rtv->lpVtbl->Release(s->rtv);       s->rtv       = NULL; }
        if (s->swapchain) { s->swapchain->lpVtbl->Release(s->swapchain); s->swapchain = NULL; }
        if (s->dcomp)     { tr_dcomp_destroy(s->dcomp);            s->dcomp     = NULL; }
        if (s->ctx)       { s->ctx->lpVtbl->Release(s->ctx);       s->ctx       = NULL; }
        if (s->ps)        { s->ps->lpVtbl->Release(s->ps);         s->ps        = NULL; }
        if (s->vs)        { s->vs->lpVtbl->Release(s->vs);         s->vs        = NULL; }
        if (s->rs)        { s->rs->lpVtbl->Release(s->rs);         s->rs        = NULL; }
        if (s->device)    { s->device->lpVtbl->Release(s->device); s->device    = NULL; }'''

content = content.replace(old, new)

# Now add `upgrade_fail:` label before the existing cleanup code
# Find the existing: "    int ok = hw->ok;" and add the label before it
old2 = '''\n    int ok = hw->ok;\n\n    // Clean up pending struct'''
new2 = '''\nupgrade_fail:
    int ok = hw->ok;\n\n    // Clean up pending struct'''

content = content.replace(old2, new2)

with open('../d3d11_bench/trlib_d3d11.c', 'w') as f:
    f.write(content)

print("Done! Swapchain created before WARP release, fail_swapchain label added.")
