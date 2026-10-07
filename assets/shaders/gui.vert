#version 460 core
// GUI quads (rendering/GuiBatch): GUI-pixel positions, texel UVs, colour, texture.
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
layout(location = 3) in uint aTexture;
layout(location = 0) uniform vec2 uGuiSize; // framebuffer / GUI scale

out vec2 vUv;
out vec4 vColor;
flat out uint vTexture;

void main() {
    vUv = aUv;
    vColor = aColor;
    vTexture = aTexture;
    const vec2 ndc = aPos / uGuiSize * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
