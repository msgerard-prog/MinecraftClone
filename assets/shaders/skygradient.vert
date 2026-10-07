#version 460 core
// Full-screen triangle (no vertex buffer): the sky behind everything.
out vec2 vNdc;

void main() {
    const vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    vNdc = p;
    gl_Position = vec4(p, 1.0, 1.0);
}
