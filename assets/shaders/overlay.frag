#version 460 core
layout(location = 1) uniform vec4 uColor;
out vec4 fragColor;

void main() {
    fragColor = uColor;
}
