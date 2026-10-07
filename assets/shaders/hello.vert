#version 460 core
// M1 placeholder: a world-space triangle 3 blocks south of the origin (z = 3),
// facing north. Red apex at the top, green at +X (east), blue at -X (west).
const vec3 kPositions[3] = vec3[](vec3(0.0, 1.0, 3.0), vec3(1.0, -1.0, 3.0), vec3(-1.0, -1.0, 3.0));
const vec3 kColors[3] = vec3[](vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1));

layout(location = 0) uniform mat4 uViewProj;

out vec3 vColor;

void main() {
    gl_Position = uViewProj * vec4(kPositions[gl_VertexID], 1.0);
    vColor = kColors[gl_VertexID];
}
