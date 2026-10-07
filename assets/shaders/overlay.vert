#version 460 core
// Overlay geometry (block outline, crosshair): plain positions, one transform.
layout(location = 0) in vec3 aPos;
layout(location = 0) uniform mat4 uTransform;

void main() {
    gl_Position = uTransform * vec4(aPos, 1.0);
}
