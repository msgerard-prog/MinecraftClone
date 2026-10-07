#version 460 core
layout(binding = 0) uniform sampler2D uAtlas; // rendering/TextureAtlas

in vec2 vUv;
in vec4 vColor;
out vec4 fragColor;

void main() {
    vec4 texel = texture(uAtlas, vUv);
    fragColor = vec4(texel.rgb * vColor.rgb, 1.0);
}
