// PixelLines grid overlay — HLSL pixel shader for D3D11
// Adapted from grid.fs (OpenGL GLSL) for use with tr_raylib + imvw

cbuffer ShaderUniforms : register(b0)
{
    float2 screenResolution;
    float2 cameraTarget;
    float2 cameraOffset;
    float  cameraZoom;
};

static const float4 gridColor = float4(0.5, 0.5, 0.5, 0.5);

float4 main_ps(float4 pos : SV_POSITION, float2 uv : TEXCOORD0, float4 col : COLOR0) : SV_TARGET
{
    // 1. Calculate world-space position of this pixel
    //    SV_POSITION.y is 0 at top, increasing downward — matches our coordinate system
    float2 screenPos = pos.xy;
    float2 worldPos = (screenPos - cameraOffset) / cameraZoom + cameraTarget;

    // 2. Distance to nearest grid line in screen pixels
    //    fwidth(worldPos) tells us how much world position changes per screen pixel
    float2 grid = abs(frac(worldPos - 0.5) - 0.5) / fwidth(worldPos);
    float  lineDist = min(grid.x, grid.y);

    // 3. 1.0 means the line is 1 screen pixel wide
    float alpha = 1.0 - smoothstep(0.0, 1.0, lineDist);

    // 4. Fade out the grid when zoom gets too low
    float zoomFade = smoothstep(1.5, 4.0, cameraZoom);

    // 5. Apply fade to pre-multiplied alpha
    float finalAlpha = gridColor.a * alpha * zoomFade;
    return float4(gridColor.rgb * finalAlpha, finalAlpha);
}
