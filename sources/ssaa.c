// SSAA implementation — offscreen render target at 1.1× with bilinear downscale.
#include "ssaa.h"
#include <d3dcompiler.h>
#include <stdio.h>
#include <float.h>

// ---- Constants ------------------------------------------------------------

#define SSAA_SCALE 1.10f

// ---- Embedded HLSL shaders for the blit pass ------------------------------

static const char *BLIT_VS_SRC =
    "struct VS_In { float2 pos : POSITION; float2 uv : TEXCOORD; };\n"
    "struct VS_Out { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };\n"
    "VS_Out main(VS_In vin) {\n"
    "    VS_Out vout;\n"
    "    vout.pos = float4(vin.pos, 0.0f, 1.0f);\n"
    "    vout.uv  = vin.uv;\n"
    "    return vout;\n"
    "}\n";

static const char *BLIT_PS_SRC =
    "Texture2D    tex : register(t0);\n"
    "SamplerState sam : register(s0);\n"
    "float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_TARGET {\n"
    "    return tex.Sample(sam, uv);\n"
    "}\n";

// ---- Internal state -------------------------------------------------------

static struct {
    ID3D11Device           *device;
    ID3D11Texture2D        *offscreen_tex;
    ID3D11RenderTargetView *offscreen_rtv;
    ID3D11ShaderResourceView *offscreen_srv;

    ID3D11VertexShader   *blit_vs;
    ID3D11PixelShader    *blit_ps;
    ID3D11InputLayout    *blit_il;
    ID3D11Buffer         *blit_vb;
    ID3D11SamplerState   *blit_sampler;

    int win_w, win_h;       // window size (logical)
    int tex_w, tex_h;       // offscreen tex size (physical, ~1.1×)
    int valid;              // resources are ready
} ss = {0};

// ---- Helpers --------------------------------------------------------------

static void compile_shader(const char *src, const char *target,
                           ID3DBlob **blob) {
    ID3DBlob *err = NULL;
    HRESULT hr = D3DCompile(src, strlen(src), NULL, NULL, NULL,
                            "main", target, D3DCOMPILE_OPTIMIZATION_LEVEL3,
                            0, blob, &err);
    if (FAILED(hr)) {
        const char *msg = err ? (const char*)err->lpVtbl->GetBufferPointer(err) : "?";
        fprintf(stderr, "SSAA| shader compile error (%s): %s\n", target, msg);
        if (err) err->lpVtbl->Release(err);
        *blob = NULL;
    } else {
        if (err) err->lpVtbl->Release(err);
    }
}

// ---- Public API -----------------------------------------------------------

void ssaa_init(ID3D11Device *device) {
    ss.device = device;
    ss.valid  = 0;

    // Compile vertex shader
    ID3DBlob *vs_blob = NULL, *ps_blob = NULL;
    compile_shader(BLIT_VS_SRC, "vs_4_0", &vs_blob);
    compile_shader(BLIT_PS_SRC, "ps_4_0", &ps_blob);
    if (!vs_blob || !ps_blob) {
        if (vs_blob) vs_blob->lpVtbl->Release(vs_blob);
        if (ps_blob) ps_blob->lpVtbl->Release(ps_blob);
        fprintf(stderr, "SSAA| shader compilation failed\n");
        return;
    }

    device->lpVtbl->CreateVertexShader(device,
        vs_blob->lpVtbl->GetBufferPointer(vs_blob),
        vs_blob->lpVtbl->GetBufferSize(vs_blob), NULL, &ss.blit_vs);
    device->lpVtbl->CreatePixelShader(device,
        ps_blob->lpVtbl->GetBufferPointer(ps_blob),
        ps_blob->lpVtbl->GetBufferSize(ps_blob), NULL, &ss.blit_ps);

    // Input layout: position (float2) + texcoord (float2)
    D3D11_INPUT_ELEMENT_DESC layout_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    device->lpVtbl->CreateInputLayout(device, layout_desc, 2,
        vs_blob->lpVtbl->GetBufferPointer(vs_blob),
        vs_blob->lpVtbl->GetBufferSize(vs_blob), &ss.blit_il);
    vs_blob->lpVtbl->Release(vs_blob);
    ps_blob->lpVtbl->Release(ps_blob);

    // Fullscreen quad vertex buffer (two triangles)
    // Position (xy) + Texcoord (uv)
    float verts[] = {
        -1.0f, -1.0f,  0.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
    };
    D3D11_BUFFER_DESC bd = {0};
    bd.ByteWidth      = sizeof(verts);
    bd.Usage          = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA srd = {verts};
    device->lpVtbl->CreateBuffer(device, &bd, &srd, &ss.blit_vb);

    // Bilinear sampler
    D3D11_SAMPLER_DESC sd = {0};
    sd.Filter         = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD         = FLT_MAX;
    device->lpVtbl->CreateSamplerState(device, &sd, &ss.blit_sampler);

    printf("SSAA| init OK\n");
    ss.valid = 1;
}

void ssaa_resize(int window_w, int window_h, ID3D11Device *device) {
    ss.win_w = window_w;
    ss.win_h = window_h;

    // Release old resources
    if (ss.offscreen_srv) { ss.offscreen_srv->lpVtbl->Release(ss.offscreen_srv); ss.offscreen_srv = NULL; }
    if (ss.offscreen_rtv) { ss.offscreen_rtv->lpVtbl->Release(ss.offscreen_rtv); ss.offscreen_rtv = NULL; }
    if (ss.offscreen_tex) { ss.offscreen_tex->lpVtbl->Release(ss.offscreen_tex); ss.offscreen_tex = NULL; }

    if (window_w <= 0 || window_h <= 0) return;

    int tw = (int)(window_w * SSAA_SCALE + 0.5f);
    int th = (int)(window_h * SSAA_SCALE + 0.5f);
    ss.tex_w = tw;
    ss.tex_h = th;

    D3D11_TEXTURE2D_DESC td = {0};
    td.Width              = tw;
    td.Height             = th;
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count   = 1;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->lpVtbl->CreateTexture2D(device, &td, NULL, &ss.offscreen_tex);
    if (FAILED(hr) || !ss.offscreen_tex) {
        fprintf(stderr, "SSAA| CreateTexture2D(%dx%d) failed\n", tw, th);
        return;
    }

    device->lpVtbl->CreateRenderTargetView(device, ss.offscreen_tex, NULL, &ss.offscreen_rtv);
    device->lpVtbl->CreateShaderResourceView(device, ss.offscreen_tex, NULL, &ss.offscreen_srv);

    printf("SSAA| offscreen %dx%d  (window %dx%d, scale %.2f)\n",
           tw, th, window_w, window_h, SSAA_SCALE);
}

int ssaa_begin_frame(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *backbuffer_rtv,
                      Color clear_color) {
    if (!ss.valid || !ss.offscreen_rtv) return 0;
    (void)backbuffer_rtv;

    // Set offscreen as the active render target
    ctx->lpVtbl->OMSetRenderTargets(ctx, 1, &ss.offscreen_rtv, NULL);

    // Set viewport to offscreen size
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width    = (float)ss.tex_w;
    vp.Height   = (float)ss.tex_h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->lpVtbl->RSSetViewports(ctx, 1, &vp);

    // Clear the entire offscreen target to the background colour so the
    // overscan area (right/bottom edges at 1.1×) has the correct colour
    // rather than stale data which would bleed in during bilinear downscale.
    FLOAT rgba[4] = {
        clear_color.r / 255.0f,
        clear_color.g / 255.0f,
        clear_color.b / 255.0f,
        clear_color.a / 255.0f
    };
    ctx->lpVtbl->ClearRenderTargetView(ctx, ss.offscreen_rtv, rgba);

    return 1;
}

void ssaa_end_frame(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *backbuffer_rtv,
                    int window_w, int window_h) {
    if (!ss.valid || !ss.offscreen_srv) return;

    // Restore backbuffer RTV
    ctx->lpVtbl->OMSetRenderTargets(ctx, 1, &backbuffer_rtv, NULL);

    // Set viewport back to window size
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width    = (float)window_w;
    vp.Height   = (float)window_h;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->lpVtbl->RSSetViewports(ctx, 1, &vp);

    // Draw fullscreen quad with blit shader
    UINT stride = 4 * sizeof(float); // 2 floats pos + 2 floats uv
    UINT offset = 0;
    ctx->lpVtbl->IASetVertexBuffers(ctx, 0, 1, &ss.blit_vb, &stride, &offset);
    ctx->lpVtbl->IASetInputLayout(ctx, ss.blit_il);
    ctx->lpVtbl->IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->lpVtbl->VSSetShader(ctx, ss.blit_vs, NULL, 0);
    ctx->lpVtbl->PSSetShader(ctx, ss.blit_ps, NULL, 0);
    ctx->lpVtbl->PSSetShaderResources(ctx, 0, 1, &ss.offscreen_srv);
    ctx->lpVtbl->PSSetSamplers(ctx, 0, 1, &ss.blit_sampler);

    ctx->lpVtbl->Draw(ctx, 6, 0);

    // Unbind SRV to avoid warning on next frame's OMSetRenderTargets
    ID3D11ShaderResourceView *null_srv = NULL;
    ctx->lpVtbl->PSSetShaderResources(ctx, 0, 1, &null_srv);
}

float ssaa_get_scale(void) { return SSAA_SCALE; }

void ssaa_cleanup() {
    if (ss.blit_sampler) { ss.blit_sampler->lpVtbl->Release(ss.blit_sampler); ss.blit_sampler = NULL; }
    if (ss.blit_vb)      { ss.blit_vb->lpVtbl->Release(ss.blit_vb);       ss.blit_vb      = NULL; }
    if (ss.blit_il)      { ss.blit_il->lpVtbl->Release(ss.blit_il);       ss.blit_il      = NULL; }
    if (ss.blit_ps)      { ss.blit_ps->lpVtbl->Release(ss.blit_ps);       ss.blit_ps      = NULL; }
    if (ss.blit_vs)      { ss.blit_vs->lpVtbl->Release(ss.blit_vs);       ss.blit_vs      = NULL; }
    if (ss.offscreen_srv){ ss.offscreen_srv->lpVtbl->Release(ss.offscreen_srv); ss.offscreen_srv = NULL; }
    if (ss.offscreen_rtv){ ss.offscreen_rtv->lpVtbl->Release(ss.offscreen_rtv); ss.offscreen_rtv = NULL; }
    if (ss.offscreen_tex){ ss.offscreen_tex->lpVtbl->Release(ss.offscreen_tex); ss.offscreen_tex = NULL; }
    ss.valid = 0;
    printf("SSAA| cleanup\n");
}
