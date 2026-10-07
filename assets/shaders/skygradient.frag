#version 460 core
// The sky dome (M22.2; wiki: Sky, Daylight cycle): the sky colour overhead fading to
// the fog colour at the horizon, plus vanilla's sunrise/sunset fan - a glowing ellipse
// on the horizon under the sun, ~50 degrees wide and up to ~22 tall.
layout(location = 0) uniform mat4 uInvViewProj; // camera at the origin
layout(location = 1) uniform vec3 uSky;
layout(location = 2) uniform vec3 uFog;
layout(location = 3) uniform vec4 uSunrise; // rgb, alpha (0: no glow)
layout(location = 4) uniform vec2 uSunSide; // horizontal direction of the sun (x, z)

in vec2 vNdc;
out vec4 fragColor;

void main() {
    const vec4 far = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    const vec3 d = normalize(far.xyz / far.w);
    const float elevation = asin(clamp(d.y, -1.0, 1.0)); // radians
    // Overhead: the sky; the last ~25 degrees down to the horizon blend into the fog
    // (vanilla: the sky plane fogged by distance); below the horizon, fog.
    vec3 c = mix(uFog, uSky, smoothstep(0.0, 0.45, elevation));
    if (uSunrise.a > 0.0) {
        const vec2 h = normalize(d.xz + vec2(1e-6));
        const float azimuth = acos(clamp(dot(h, uSunSide), -1.0, 1.0));
        const vec2 r = vec2(azimuth / 0.876, elevation / (0.38 * max(uSunrise.a, 0.05)));
        const float glow = uSunrise.a * clamp(1.0 - length(r), 0.0, 1.0);
        c = mix(c, uSunrise.rgb, glow);
    }
    fragColor = vec4(c, 1.0);
}
