#version 460 core
layout(location = 2) uniform vec4 uColor;
layout(location = 3) uniform vec2 uFade; // start, end (horizontal blocks)

in float vShade;
in vec2 vHorizontal;
out vec4 fragColor;

void main() {
    const float fade = 1.0 - smoothstep(uFade.x, uFade.y, length(vHorizontal));
    if (fade <= 0.0) discard;
    fragColor = vec4(uColor.rgb * vShade, uColor.a * fade);
}
