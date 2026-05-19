#pragma once
#include <cmath>
#include <algorithm>

// ─── ArcballCamera ────────────────────────────────────────────────────────────
//
// C++20 port of CInterface from src/gui/interface.h.
// All matrices are column-major float[16] to match Metal/GL conventions.
//
// worldToCamera = T(0,0,-distance) * R(orientation) * T(orbitCenter)

struct vec3 { float x, y, z; };
struct vec2 { float x, y; };

// Column-major 4×4 stored as m[col][row], flat index: col*4 + row.
struct mat4 {
    float m[16]{};

    static mat4 identity() {
        mat4 r; r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.f; return r;
    }
    // m[col*4 + row]
    float& at(int col, int row) { return m[col*4+row]; }
    float  at(int col, int row) const { return m[col*4+row]; }
};

struct quatf { float x, y, z, w; };  // w = scalar (r) component

mat4  mat4_mul(const mat4 &a, const mat4 &b);
mat4  mat4_inverse(const mat4 &m);
quatf quat_normalize(quatf q);
quatf quat_mul(quatf a, quatf b);
mat4  quat_to_mat4(quatf q);

// ─── ArcballCamera ────────────────────────────────────────────────────────────

class ArcballCamera {
public:
    ArcballCamera(const float *projMatrix16,   // column-major
                  const float *viewMatrix16,   // column-major worldToCamera
                  const float *sceneBoundsMin, // float[3]
                  const float *sceneBoundsMax, // float[3]
                  float windowW, float windowH);

    void beginOrbit(float sx, float sy);
    void orbit(float sx, float sy);

    void beginPan(float sx, float sy);
    void pan(float sx, float sy);

    void zoom(float delta);   // delta > 0 → zoom out

    void reset();

    void updateAspect(float w, float h);

    // Returns column-major MVP (proj * view).
    void viewProjectionMatrix(float *out16) const;
    // Returns column-major camera-to-world.
    void cameraToWorldMatrix(float *out16) const;

    bool isOrthographic() const;
    float fovDegrees() const;

    // State exposed for tests.
    float distance()  const { return distance_; }
    vec3  orbitCenter() const { return orbitCenter_; }
    quatf orientation() const { return orientation_; }

    float windowW() const { return windowW_; }
    float windowH() const { return windowH_; }

private:
    mat4  ribProj_, ribView_;
    mat4  projMatrix_;
    vec3  orbitCenter_;
    quatf orientation_;
    float distance_;

    vec3  initOrbitCenter_;
    quatf initOrientation_;
    float initDistance_;

    quatf savedOrientation_{0,0,0,1};
    vec3  savedOrbitCenter_{};
    vec2  panFrom_{};
    vec3  dragFromSphere_{0,0,1};

    float windowW_, windowH_, radius_;

    vec3 toSphere(float sx, float sy) const;

    static std::pair<quatf,float> decompose(const mat4 &worldToCamera,
                                            vec3 orbitCenter);
};
