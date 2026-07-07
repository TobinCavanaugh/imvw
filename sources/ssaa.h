// SSAA (Super-Sample Anti-Aliasing) via offscreen render target.
//
// Renders everything to an N× offscreen texture (default 1.1×), then blits it
// down to the backbuffer with bilinear filtering at the end of each frame.
// This gives cheap, effective anti-aliasing for the entire frame (image, text,
// UI, etc.) without needing MSAA support from tr_raylib.
//
// The scale factor is set once at init and kept constant afterwards so that
// runtime changes don't cause coordinate-space mismatches with the viewport
// or destroy/recreate D3D11 resources in the middle of a frame.

#ifndef SSAA_H
#define SSAA_H

#include "dialect.h"
#include <trlib.h>

// ---- Public API -----------------------------------------------------------

// Call once after InitWindow to create the offscreen resources.
// `scale` is the supersampling factor (e.g. 1.10f) — must be > 0.
// Must be called with the D3D11 device from tr_get_device().
void ssaa_init(ID3D11Device *device, float scale);

// Call on window resize to recreate the offscreen target at the new size.
void ssaa_resize(int window_w, int window_h, ID3D11Device *device);

// Call at the START of each frame (just after BeginDrawing).
// Sets the offscreen RTV as active and clears it to `clear_color`.
// Returns 1 on success.
int  ssaa_begin_frame(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *backbuffer_rtv,
                       Color clear_color);

// Call at the END of each frame (just before EndDrawing).
// Restores the backbuffer RTV, then draws the offscreen texture as a
// fullscreen quad with bilinear filtering (downscaling N× → 1.0×).
void ssaa_end_frame(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *backbuffer_rtv,
                    int window_w, int window_h);

// Returns the SSAA scale factor set at init (never changes at runtime).
float ssaa_get_scale(void);

// Call once at shutdown to release all D3D11 resources.
void ssaa_cleanup();

#endif //SSAA_H
