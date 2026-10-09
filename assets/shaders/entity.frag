#version 460 core
layout(binding = 0) uniform sampler2D uAtlas;
layout(location = 1) uniform float uAlphaCutoff;
// (M29.2c) Glowing: when alpha > 0, the shape drawn in one flat colour (through walls).
layout(location = 2) uniform vec4 uFlat;

in vec2 vUv;
in vec4 vColor;
out vec4 fragColor;

void main() {
    const vec4 t = textureLod(uAtlas, vUv / vec2(textureSize(uAtlas, 0)), 0.0);
    if (t.a < uAlphaCutoff) discard;
    fragColor = uFlat.a > 0.0 ? uFlat : t * vColor;
}
