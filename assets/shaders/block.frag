#version 460 core
layout(binding = 0) uniform sampler2D uAtlas; // rendering/TextureAtlas

in vec2 vUv;
in vec3 vColor;
out vec4 fragColor;

void main() {
    fragColor = vec4(texture(uAtlas, vUv).rgb * vColor, 1.0);
}
