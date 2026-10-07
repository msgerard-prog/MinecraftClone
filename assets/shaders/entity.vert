#version 460 core
// Entities (dropped items) and the breaking crack: camera-relative positions,
// atlas texel UVs, per-vertex colour (light x shade).
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
layout(location = 0) uniform mat4 uViewProj;

out vec2 vUv;
out vec4 vColor;

void main() {
    vUv = aUv;
    vColor = aColor;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
