#version 460 core
// Block passes (opaque + translucent). One vertex = uvec3 (rendering/PackedVertex.h):
//   x: x:9 y:9 z:9 face:3         (1/16 block units inside the section)
//   y: sprite:12 u:5 v:5 tint:2 fluidTop:1 ao:2
//   z: sky:6 block:6              (sums of 4 smooth-lighting samples, 0..60)
layout(location = 0) in uvec3 aPacked;

layout(location = 0) uniform mat4 uViewProj;   // camera at the origin
layout(location = 1) uniform int uAtlasColumns;
layout(location = 2) uniform vec3 uGrassColor;  // biome tints (plains / default until M8)
layout(location = 3) uniform vec3 uWaterColor;
layout(location = 8) uniform vec3 uFoliageColor;
layout(location = 7) uniform float uSkyDarken;  // 0 (day) .. 11 (night): sky light lost

layout(std430, binding = 0) readonly buffer SectionOffsets { vec4 offsets[]; };

// Vanilla directional shading by face (down, up, north, south, west, east).
// Must match kFaceShade in rendering/ChunkMesher.h.
const float kShade[6] = float[](0.5, 1.0, 0.8, 0.8, 0.6, 0.6);
// Ambient occlusion: each opaque neighbour (of 3) darkens the corner (vanilla
// averages 4 cells where opaque ones count 0.2).
const float kAo[4] = float[](1.0, 0.8, 0.6, 0.4);

out vec2 vUv;
out vec3 vColor;
out float vDistance;

// Light curve: brightness of level l is l / (60 - 3l) (public write-up: Origins docs,
// "brightness"; each level ~80% of the one above, wiki: Light), then the Brightness
// option (default 50%) lifts the darks - our estimate: mix(x, 1 - (1-x)^4, 0.5).
// Full darkness stays ~5% visible (wiki: Light › Rendered brightness).
float brightness(float level) {
    const float f = clamp(level / 15.0, 0.0, 1.0);
    const float b = f / (4.0 - 3.0 * f);
    const float lifted = 1.0 - pow(1.0 - b, 4.0);
    return mix(0.05, 1.0, mix(b, lifted, 0.5));
}

void main() {
    const uint w0 = aPacked.x, w1 = aPacked.y, w2 = aPacked.z;
    vec3 local = vec3(w0 & 511u, (w0 >> 9) & 511u, (w0 >> 18) & 511u) / 16.0;
    const uint face = (w0 >> 27) & 7u;
    const uint sprite = w1 & 4095u;
    const vec2 texel = vec2((w1 >> 12) & 31u, (w1 >> 17) & 31u);
    const uint tint = (w1 >> 22) & 3u;
    if (((w1 >> 24) & 1u) != 0u) local.y -= 1.0 / 9.0; // source fluid surface: 8/9 tall
    const uint ao = (w1 >> 25) & 3u;
    const float sky = float(w2 & 63u) / 4.0;
    const float blockLight = float((w2 >> 6) & 63u) / 4.0;

    const vec3 pos = local + offsets[gl_BaseInstance].xyz;
    gl_Position = uViewProj * vec4(pos, 1.0);
    // Vanilla terrain fog is cylindrical (since 1.18).
    vDistance = max(length(pos.xz), abs(pos.y));

    const uint cols = uint(uAtlasColumns);
    vUv = (vec2(sprite % cols, sprite / cols) + texel / 16.0) / float(cols);

    // Light: sky light dimmed at night, block light slightly warm; added and clamped.
    const vec3 skyPart = vec3(brightness(sky - uSkyDarken));
    const vec3 blockPart = brightness(blockLight) * vec3(1.0, 0.93, 0.82);
    const vec3 light = min(skyPart + blockPart, vec3(1.0)) * kAo[ao];

    const vec3 tintColor = tint == 1u ? uGrassColor
                         : tint == 2u ? uWaterColor
                         : tint == 3u ? uFoliageColor
                                      : vec3(1.0);
    vColor = tintColor * kShade[face] * light;
}
