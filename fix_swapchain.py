import re

with open('../d3d11_bench/trlib_d3d11.c', 'r') as f:
    content = f.read()

# The target: find the start of the hot-swap block inside if (hw->ok)
# We need to insert swapchain+RTV creation right before the old resources are released.
# The pattern: after "if (hw->ok) {" and before "// Destroy current WARP graphics objects"

# Find: "// Destroy current WARP graphics objects"
old = '''        // Destroy current WARP graphics objects
        if (s->rtv)       { s->rtv->lpVtbl->Release(s->rtv);       s->rtv       = NULL; }
        if (s->dcomp)     { tr_dcomp_destroy(s->dcomp);            s->dcomp     = NULL; }
        if (s->swapchain) { s->swapchain->lpVtbl->Release(s->swapchain); s->swapchain = NULL; }
        if (s->ctx)       { s->ctx->lpVtbl->Release(s->ctx);       s->ctx       = NULL; }
        if (s->ps)        { s->ps->lpVtbl->Release(s->ps);         s->ps        = NULL; }
        if (s->vs)        { s->vs->lpVtbl->Release(s->vs);         s->vs        = NULL; }
        if (s->rs)        { s->rs->lpVtbl->Release(s->rs);         s->rs        = NULL; }
        if (s->device)    { s->device->lpVtbl->Release(s->device); s->device    = NULL; }'''

new = '''        // Destroy current WARP graphics objects
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

content = content.replace(old, new)

with open('../d3d11_bench/trlib_d3d11.c', 'w') as f:
    f.write(content)

print("Done! Swapchain+RTV creation moved to main thread.")
