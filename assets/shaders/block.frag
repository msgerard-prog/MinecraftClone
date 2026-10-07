#version 460 core
layout(binding = 0) uniform sampler2D uAtlas; // rendering/TextureAtlas
layout(location = 4) uniform vec2 uFog;       // start, end (blocks)
layout(location = 5) uniform vec3 uFogColor;  // sky colour
layout(location = 6) uniform float uAlphaCutoff; // opaque pass: cutout (torches, glass)

in vec2 vUv;
in vec3 vColor;
in float vDistance;
out vec4 fragColor;

void main() {
    const vec4 texel = texture(uAtlas, vUv);
    if (texel.a < uAlphaCutoff) discard;
    vec3 color = texel.rgb * vColor;
    const float fog = clamp((vDistance - uFog.x) / (uFog.y - uFog.x), 0.0, 1.0); // linear
    fragColor = vec4(mix(color, uFogColor, fog), texel.a); // alpha used by the blended pass
}
