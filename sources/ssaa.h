// SSAA (Super-Sample Anti-Aliasing) via offscreen render target.
//
// Renders everything to a 1.1× offscreen texture, then blits it down to the
// backbuffer with bilinear filtering at the end of each frame.  This gives
// cheap, effective anti-aliasing for the entire frame (image, text, UI, etc.)
// without needing MSAA support from tr_raylib.

#ifndef SSAA_H
#define SSAA_H

#include "dialect.h"
#include <trlib.h>

// ---- Public API -----------------------------------------------------------

// Call once after InitWindow to create the offscreen resources.
// Must be called with the D3D11 device from tr_get_device().
void ssaa_init(ID3D11Device *device);

// Call on window resize to recreate the offscreen target at the new size.
void ssaa_resize(int window_w, int window_h, ID3D11Device *device);

// Call at the START of each frame (just after BeginDrawing).
// Sets the offscreen RTV as active and clears it to `clear_color`.
// Returns 1 on success.
int  ssaa_begin_frame(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *backbuffer_rtv,
                       Color clear_color);

// Call at the END of each frame (just before EndDrawing).
// Restores the backbuffer RTV, then draws the offscreen texture as a
// fullscreen quad with bilinear filtering (downscaling 1.1× → 1.0×).
void ssaa_end_frame(ID3D11DeviceContext *ctx, ID3D11RenderTargetView *backbuffer_rtv,
                    int window_w, int window_h);

// Returns the SSAA scale factor (e.g. 1.10f).  Custom shader uniforms
// that depend on screen position (cameraOffset, screenResolution) must
// be multiplied by this to match the larger offscreen viewport.
float ssaa_get_scale(void);

// Call once at shutdown to release all D3D11 resources.
void ssaa_cleanup();

#endif //SSAA_H
