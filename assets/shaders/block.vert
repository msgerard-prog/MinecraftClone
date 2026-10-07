#version 460 core
// Opaque block pass. One vertex = uvec2 (rendering/PackedVertex.h):
//   lo: x:5 y:5 z:5 face:3 uvCorner:2 sprite:12    hi: tint:2
layout(location = 0) in uvec2 aPacked;

layout(location = 0) uniform mat4 uViewProj;  // camera at the origin (Camera::viewProjectionAtOrigin)
layout(location = 1) uniform int uAtlasColumns;
layout(location = 2) uniform vec3 uGrassColor; // biome tint (plains until M8)

// Section origin minus camera position, one per draw (ChunkRenderer).
layout(std430, binding = 0) readonly buffer SectionOffsets { vec4 offsets[]; };

// Vanilla directional shading by face (down, up, north, south, west, east).
// Must match kFaceShade in rendering/ChunkMesher.h.
const float kShade[6] = float[](0.5, 1.0, 0.8, 0.8, 0.6, 0.6);
const vec2 kCornerUv[4] = vec2[](vec2(0, 0), vec2(0, 1), vec2(1, 1), vec2(1, 0));

out vec2 vUv;
out vec3 vColor;

void main() {
    const uint lo = aPacked.x;
    const vec3 local = vec3(lo & 31u, (lo >> 5) & 31u, (lo >> 10) & 31u);
    const uint face = (lo >> 15) & 7u;
    const uint corner = (lo >> 18) & 3u;
    const uint sprite = lo >> 20;

    gl_Position = uViewProj * vec4(local + offsets[gl_BaseInstance].xyz, 1.0);

    const uint cols = uint(uAtlasColumns);
    vUv = (vec2(sprite % cols, sprite / cols) + kCornerUv[corner]) / float(cols);
    const vec3 tint = (aPacked.y & 3u) == 1u ? uGrassColor : vec3(1.0);
    vColor = tint * kShade[face];
}
