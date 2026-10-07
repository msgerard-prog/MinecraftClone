#version 460 core
// Sky objects (sun, moon, stars): positions around the camera, rotation-only transform.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 0) uniform mat4 uTransform;
layout(location = 2) uniform vec4 uUvRect; // offset xy, scale zw (moon phase cell)

out vec2 vUv;

void main() {
    vUv = uUvRect.xy + aUv * uUvRect.zw;
    gl_Position = uTransform * vec4(aPos, 1.0);
}
