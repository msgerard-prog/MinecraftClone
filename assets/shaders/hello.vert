#version 460 core
// Hello triangle: positions come from gl_VertexID, no vertex buffer needed.
// Red apex at the top, green bottom-left, blue bottom-right (orientation check).
const vec2 kPositions[3] = vec2[](vec2(0.0, 0.6), vec2(-0.6, -0.6), vec2(0.6, -0.6));
const vec3 kColors[3] = vec3[](vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1));

out vec3 vColor;

void main() {
    gl_Position = vec4(kPositions[gl_VertexID], 0.0, 1.0);
    vColor = kColors[gl_VertexID];
}
