import re

# ── Fix trlib_d3d11.c: replace the broken create_swapchain_comp ──
with open('../d3d11_bench/trlib_d3d11.c', 'r') as f:
    content = f.read()

# Fix 1: Replace the entire create_swapchain_comp function
old_func = '''static int create_swapchain_comp(IDXGISwapChain1** sc,
                                 ID3D11Device* dev, int w, int h)
{
    IDXGIFactory2* factory = NULL;
    HRESULT hr = CreateDXGIFactory1(&IID_IDXGIFactory2, (void**)&factory);
    if (FAILED(hr)) { fprintf(stderr, "TR| swapchain: CreateDXGIFactory1 failed (0x%08lx)
", (unsigned long)hr); return 0; }

    DXGI_SWAP_CHAIN_DESC1 scd = {0};
    scd.Width       = w;
    scd.Height      = h;
    scd.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.Scaling     = DXGI_SCALING_STRETCH;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED;

    hr = factory->lpVtbl->CreateSwapChainForComposition(
        factory, (IUnknown*)dev, &scd, NULL, sc);
    factory->lpVtbl->Release(factory);
    if (SUCCEEDED(hr)) {
        fprintf(stderr, "TR| swapchain: created OK (%%p, %dx%d)
", (void*)*sc, w, h);
        return 1;
    }
    fprintf(stderr, "TR| swapchain: CreateSwapChainForComposition failed (0x%08lx)
", (unsigned long)hr);
    return 0;
}'''

new_func = '''static int create_swapchain_comp(IDXGISwapChain1** sc,
                                 ID3D11Device* dev, int w, int h)
{
    IDXGIFactory2* factory = NULL;
    HRESULT hr = CreateDXGIFactory1(&IID_IDXGIFactory2, (void**)&factory);
    if (FAILED(hr)) { fprintf(stderr, "TR| swapchain: create factory failed\\n"); return 0; }

    DXGI_SWAP_CHAIN_DESC1 scd = {0};
    scd.Width       = w;
    scd.Height      = h;
    scd.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.Scaling     = DXGI_SCALING_STRETCH;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED;

    hr = factory->lpVtbl->CreateSwapChainForComposition(
        factory, (IUnknown*)dev, &scd, NULL, sc);
    factory->lpVtbl->Release(factory);
    if (SUCCEEDED(hr)) {
        fprintf(stderr, "TR| sc created OK %dx%d\\n", w, h);
        return 1;
    }
    fprintf(stderr, "TR| sc CreateSwapChainForComposition failed\\n");
    return 0;
}'''

# Replace by matching the function signature and up to the closing brace
# Using the signature line as anchor
if old_func in content:
    content = content.replace(old_func, new_func)
    print("Fixed create_swapchain_comp")
else:
    print("WARN: create_swapchain_comp replacement pattern not found")
    # Try to find what's actually there
    import sys
    # Find the function
    idx = content.find("static int create_swapchain_comp")
    if idx >= 0:
        print(f"Found at position {idx}")
        print(content[idx:idx+800])

# Fix 2: Fix the hot-swap logging line (if it has newline issues)
# Find and fix the fprintf line after hot-swap
old_hotswap = '''        fprintf(stderr, "TR| hot-swap complete: device=%p ctx=%p swapchain=%p rtv=%p
",
                (void*)s->device, (void*)s->ctx, (void*)s->swapchain, (void*)s->rtv);'''

new_hotswap = '''        fprintf(stderr, "TR| hot-swap OK dev=%p ctx=%p sc=%p rtv=%p\\n",
                (void*)s->device, (void*)s->ctx, (void*)s->swapchain, (void*)s->rtv);'''

if old_hotswap in content:
    content = content.replace(old_hotswap, new_hotswap)
    print("Fixed hot-swap logging")
else:
    print("WARN: hot-swap logging pattern not found")

with open('../d3d11_bench/trlib_d3d11.c', 'w') as f:
    f.write(content)
print("trlib_d3d11.c saved")

# ── Fix trlib_render.c: fix the tr_draw_end logging ──
with open('../d3d11_bench/trlib_render.c', 'r') as f:
    render_content = f.read()

old_render = '''    if (!s->swapchain) {
        fprintf(stderr, "TRLIB| tr_draw_end: s->swapchain is NULL!
");
        return;
    }
    fprintf(stderr, "TRLIB| Present on swapchain %p (s=%p)
", (void*)s->swapchain, (void*)s);'''

new_render = '''    if (!s->swapchain) {
        fprintf(stderr, "TRLIB| sc NULL\\n");
        return;
    }
    fprintf(stderr, "TRLIB| Present sc=%p\\n", (void*)s->swapchain);'''

if old_render in render_content:
    render_content = render_content.replace(old_render, new_render)
    print("Fixed tr_draw_end logging")
else:
    print("WARN: tr_draw_end logging pattern not found")

with open('../d3d11_bench/trlib_render.c', 'w') as f:
    f.write(render_content)
print("trlib_render.c saved")
