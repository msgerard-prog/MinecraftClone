#version 460 core
// Texture units = GuiTexture: 0 white, 1 font, 2 hotbar, 3 selection, 4 block atlas.
layout(binding = 0) uniform sampler2D uWhite;
layout(binding = 1) uniform sampler2D uFont;
layout(binding = 2) uniform sampler2D uHotbar;
layout(binding = 3) uniform sampler2D uSelection;
layout(binding = 4) uniform sampler2D uAtlas;
layout(binding = 5) uniform sampler2D uIcons; // survival HUD icons strip
layout(binding = 6) uniform sampler2D uMap;   // the held map (M28.2b)

in vec2 vUv;
in vec4 vColor;
flat in uint vTexture;
out vec4 fragColor;

vec4 sampleTexel(sampler2D s) {
    return textureLod(s, vUv / vec2(textureSize(s, 0)), 0.0); // UVs are in texels
}

void main() {
    vec4 t;
    if (vTexture == 1u) t = sampleTexel(uFont);
    else if (vTexture == 2u) t = sampleTexel(uHotbar);
    else if (vTexture == 3u) t = sampleTexel(uSelection);
    else if (vTexture == 4u) t = sampleTexel(uAtlas);
    else if (vTexture == 5u) t = sampleTexel(uIcons);
    else if (vTexture == 6u) t = sampleTexel(uMap);
    else t = vec4(1.0);
    const vec4 c = t * vColor;
    if (c.a < 0.004) discard;
    fragColor = c;
}
