#version 460 core
// Cloud boxes: positions relative to the camera's cloud cell, shifted by uOffset.
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aShade;
layout(location = 0) uniform mat4 uViewProj;
layout(location = 1) uniform vec3 uOffset;

out float vShade;
out vec2 vHorizontal;

void main() {
    const vec3 p = aPos + uOffset;
    vShade = aShade;
    vHorizontal = p.xz;
    gl_Position = uViewProj * vec4(p, 1.0);
}
