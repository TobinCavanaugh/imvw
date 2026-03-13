// grid.fs - Fragment Shader
#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform vec2 screenResolution;
uniform vec2 cameraTarget;
uniform vec2 cameraOffset;
uniform float cameraZoom;
vec4 gridColor = vec4(0.5,0.5,0.5,0.5);

void main() {
    // 1. Calculate the world-space position of this pixel
    vec2 screenPos = gl_FragCoord.xy;
    // Note: gl_FragCoord is bottom-up, Raylib is top-down.
    screenPos.y = screenResolution.y - screenPos.y;

    vec2 worldPos = (screenPos - cameraOffset) / cameraZoom + cameraTarget;

    // 2. Determine the distance to the nearest grid line in screen pixels
    // fwidth(worldPos) tells us how much the world position changes per screen pixel
    vec2 grid = abs(fract(worldPos - 0.5) - 0.5) / fwidth(worldPos);
    float line = min(grid.x, grid.y);

    // 3. 1.0 means the line is 1 screen pixel wide
    float alpha = 1.0 - smoothstep(0.0, 1.0, line);

    // 4. Fade out the grid when zoom gets too low.
    // smoothstep(min, max, value) returns 0 below min, 1 above max, and smoothly interpolates between.
    // Tweak these two numbers (1.5 and 4.0) to change when the fade starts and ends!
    float zoomFade = smoothstep(1.5, 4.0, cameraZoom);

    // 5. Apply the fade to your pre-multiplied alpha
    float finalAlpha = gridColor.a * alpha * zoomFade;
    finalColor = vec4(gridColor.rgb * finalAlpha, finalAlpha);

//     finalColor = vec4(1,0,0,1);
}