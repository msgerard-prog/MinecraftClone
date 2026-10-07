#version 460 core
// Opaque block pass. Vertex layout: rendering/Mesh.cpp (BlockVertex).
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor; // face shade x biome tint

layout(location = 0) uniform mat4 uViewProj;

out vec2 vUv;
out vec4 vColor;

void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    vUv = aUv;
    vColor = aColor;
}
