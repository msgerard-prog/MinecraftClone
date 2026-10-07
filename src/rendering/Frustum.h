#pragma once

#include <glm/glm.hpp>

namespace mc::gfx {

// View frustum planes extracted from a view-projection matrix (Gribb/Hartmann).
// GL-free; used to skip sections outside the view.
struct Frustum {
    glm::vec4 planes[6]; // ax + by + cz + d >= 0 inside

    static Frustum fromMatrix(const glm::mat4& m) {
        Frustum f;
        const glm::vec4 r0(m[0][0], m[1][0], m[2][0], m[3][0]);
        const glm::vec4 r1(m[0][1], m[1][1], m[2][1], m[3][1]);
        const glm::vec4 r2(m[0][2], m[1][2], m[2][2], m[3][2]);
        const glm::vec4 r3(m[0][3], m[1][3], m[2][3], m[3][3]);
        f.planes[0] = r3 + r0; // left
        f.planes[1] = r3 - r0; // right
        f.planes[2] = r3 + r1; // bottom
        f.planes[3] = r3 - r1; // top
        f.planes[4] = r3 + r2; // near (GL clip z in -w..w)
        f.planes[5] = r3 - r2; // far
        return f;
    }

    // True if the axis-aligned box touches the frustum (conservative).
    bool intersectsBox(const glm::vec3& min, const glm::vec3& max) const {
        for (const glm::vec4& p : planes) {
            // The box corner furthest along the plane normal.
            const glm::vec3 v(p.x >= 0 ? max.x : min.x, p.y >= 0 ? max.y : min.y,
                              p.z >= 0 ? max.z : min.z);
            if (p.x * v.x + p.y * v.y + p.z * v.z + p.w < 0) return false;
        }
        return true;
    }
};

} // namespace mc::gfx
