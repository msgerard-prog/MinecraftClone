#version 460 core
layout(binding = 0) uniform sampler2D uAtlas; // rendering/TextureAtlas
layout(location = 4) uniform vec2 uFog;       // start, end (blocks)
layout(location = 5) uniform vec3 uFogColor;  // sky colour
layout(location = 6) uniform float uAlphaCutoff; // opaque pass: cutout (torches, glass)

in vec2 vUv;
flat in vec4 vSprite;
in vec3 vColor;
in float vDistance;
out vec4 fragColor;

void main() {
    // Stay half a texel (of the mip level sampled) inside this face's sprite: at a
    // distance the filtering otherwise blends in the next sprite of the atlas along
    // every block edge - thin dark lines across far water (and seams on terrain).
    const float lod = max(textureQueryLod(uAtlas, vUv).x, 0.0);
    const vec2 inset = exp2(lod) * 0.5 / vec2(textureSize(uAtlas, 0));
    const vec2 uv = clamp(vUv, vSprite.xy + inset, vSprite.zw - inset);
    const vec4 texel = textureGrad(uAtlas, uv, dFdx(vUv), dFdy(vUv));
    if (texel.a < uAlphaCutoff) discard;
    vec3 color = texel.rgb * vColor;
    const float fog = clamp((vDistance - uFog.x) / (uFog.y - uFog.x), 0.0, 1.0); // linear
    fragColor = vec4(mix(color, uFogColor, fog), texel.a); // alpha used by the blended pass
}
