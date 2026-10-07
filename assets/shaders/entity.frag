#version 460 core
layout(binding = 0) uniform sampler2D uAtlas;
layout(location = 1) uniform float uAlphaCutoff;

in vec2 vUv;
in vec4 vColor;
out vec4 fragColor;

void main() {
    const vec4 t = textureLod(uAtlas, vUv / vec2(textureSize(uAtlas, 0)), 0.0);
    if (t.a < uAlphaCutoff) discard;
    fragColor = t * vColor;
}
