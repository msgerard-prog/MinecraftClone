#version 460 core
// Additive sky objects: black texels add nothing.
layout(binding = 0) uniform sampler2D uTexture;
layout(location = 1) uniform vec4 uColor;

in vec2 vUv;
out vec4 fragColor;

void main() {
    fragColor = texture(uTexture, vUv) * uColor;
}
