#version 460 core
// Block passes (opaque + translucent). One vertex = uvec2 (rendering/PackedVertex.h):
//   lo: x:5 y:5 z:5 face:3 uvCorner:2 sprite:12    hi: tint:2 fluidTop:1
layout(location = 0) in uvec2 aPacked;

layout(location = 0) uniform mat4 uViewProj;   // camera at the origin (Camera::viewProjectionAtOrigin)
layout(location = 1) uniform int uAtlasColumns;
layout(location = 2) uniform vec3 uGrassColor;  // biome tints (plains / default until M8)
layout(location = 3) uniform vec3 uWaterColor;

// Section origin minus camera position, one per draw (ChunkRenderer).
layout(std430, binding = 0) readonly buffer SectionOffsets { vec4 offsets[]; };

// Vanilla directional shading by face (down, up, north, south, west, east).
// Must match kFaceShade in rendering/ChunkMesher.h.
const float kShade[6] = float[](0.5, 1.0, 0.8, 0.8, 0.6, 0.6);
const vec2 kCornerUv[4] = vec2[](vec2(0, 0), vec2(0, 1), vec2(1, 1), vec2(1, 0));

out vec2 vUv;
out vec3 vColor;
out float vDistance; // to the camera, for fog

void main() {
    const uint lo = aPacked.x;
    vec3 local = vec3(lo & 31u, (lo >> 5) & 31u, (lo >> 10) & 31u);
    const uint face = (lo >> 15) & 7u;
    const uint corner = (lo >> 18) & 3u;
    const uint sprite = lo >> 20;
    const uint tint = aPacked.y & 3u;
    if ((aPacked.y & 4u) != 0u) local.y -= 1.0 / 9.0; // source fluid surface: 8/9 tall

    const vec3 pos = local + offsets[gl_BaseInstance].xyz;
    gl_Position = uViewProj * vec4(pos, 1.0);
    // Vanilla terrain fog is cylindrical (since 1.18): horizontal distance, with the
    // vertical distance counted separately, so the ground stays visible when flying high.
    vDistance = max(length(pos.xz), abs(pos.y));

    const uint cols = uint(uAtlasColumns);
    vUv = (vec2(sprite % cols, sprite / cols) + kCornerUv[corner]) / float(cols);
    const vec3 tintColor = tint == 1u ? uGrassColor : tint == 2u ? uWaterColor : vec3(1.0);
    vColor = tintColor * kShade[face];
}
