#include "arcball.h"
#include <cstring>
#include <cassert>

// ─── Math helpers ─────────────────────────────────────────────────────────────

static float dot3(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static vec3  cross3(vec3 a, vec3 b) {
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}
static float len3(vec3 v) { return std::sqrt(dot3(v,v)); }
static vec3  norm3(vec3 v) { float l = len3(v); return {v.x/l, v.y/l, v.z/l}; }

// Column-major multiply: result[col] = sum of a.col[k] * b.row[k]
mat4 mat4_mul(const mat4 &a, const mat4 &b) {
    mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.at(k,row) * b.at(c,k);
            r.at(c,row) = s;
        }
    return r;
}

// 4×4 matrix inverse via cofactor expansion (no SIMD dependency needed).
mat4 mat4_inverse(const mat4 &src) {
    const float *m = src.m;
    float inv[16];
    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
             + m[9]*m[7]*m[14]  + m[13]*m[6]*m[11]  - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14]  + m[8]*m[6]*m[15]
             - m[8]*m[7]*m[14]  - m[12]*m[6]*m[11]  + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13]  - m[8]*m[5]*m[15]
             + m[8]*m[7]*m[13]  + m[12]*m[5]*m[11]  - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13]  + m[8]*m[5]*m[14]
             - m[8]*m[6]*m[13]  - m[12]*m[5]*m[10]  + m[12]*m[6]*m[9];
    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14]  + m[9]*m[2]*m[15]
             - m[9]*m[3]*m[14]  - m[13]*m[2]*m[11]  + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14]  - m[8]*m[2]*m[15]
             + m[8]*m[3]*m[14]  + m[12]*m[2]*m[11]  - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13]  + m[8]*m[1]*m[15]
             - m[8]*m[3]*m[13]  - m[12]*m[1]*m[11]  + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13]  - m[8]*m[1]*m[14]
             + m[8]*m[2]*m[13]  + m[12]*m[1]*m[10]  - m[12]*m[2]*m[9];
    inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]   - m[5]*m[2]*m[15]
             + m[5]*m[3]*m[14]  + m[13]*m[2]*m[7]   - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]   + m[4]*m[2]*m[15]
             - m[4]*m[3]*m[14]  - m[12]*m[2]*m[7]   + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]   - m[4]*m[1]*m[15]
             + m[4]*m[3]*m[13]  + m[12]*m[1]*m[7]   - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]   + m[4]*m[1]*m[14]
             - m[4]*m[2]*m[13]  - m[12]*m[1]*m[6]   + m[12]*m[2]*m[5];
    inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]   + m[5]*m[2]*m[11]
             - m[5]*m[3]*m[10]  - m[9]*m[2]*m[7]    + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]   - m[4]*m[2]*m[11]
             + m[4]*m[3]*m[10]  + m[8]*m[2]*m[7]    - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]    + m[4]*m[1]*m[11]
             - m[4]*m[3]*m[9]   - m[8]*m[1]*m[7]    + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]    - m[4]*m[1]*m[10]
             + m[4]*m[2]*m[9]   + m[8]*m[1]*m[6]    - m[8]*m[2]*m[5];

    float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (det == 0.f) return mat4::identity();
    float id = 1.f / det;
    mat4 out;
    for (int i = 0; i < 16; i++) out.m[i] = inv[i] * id;
    return out;
}

quatf quat_normalize(quatf q) {
    float l = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (l < 1e-12f) return {0,0,0,1};
    return {q.x/l, q.y/l, q.z/l, q.w/l};
}

// Hamilton product.
quatf quat_mul(quatf a, quatf b) {
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };
}

// Build a column-major rotation matrix from a unit quaternion.
mat4 quat_to_mat4(quatf q) {
    float x=q.x, y=q.y, z=q.z, w=q.w;
    mat4 r;
    r.at(0,0) = 1-2*(y*y+z*z); r.at(0,1) = 2*(x*y+z*w); r.at(0,2) = 2*(x*z-y*w); r.at(0,3)=0;
    r.at(1,0) = 2*(x*y-z*w);   r.at(1,1) = 1-2*(x*x+z*z); r.at(1,2) = 2*(y*z+x*w); r.at(1,3)=0;
    r.at(2,0) = 2*(x*z+y*w);   r.at(2,1) = 2*(y*z-x*w); r.at(2,2) = 1-2*(x*x+y*y); r.at(2,3)=0;
    r.at(3,0) = 0; r.at(3,1) = 0; r.at(3,2) = 0; r.at(3,3) = 1;
    return r;
}

// ─── ArcballCamera ────────────────────────────────────────────────────────────

static mat4 translate_mat4(float x, float y, float z) {
    mat4 m = mat4::identity();
    m.at(3,0) = x; m.at(3,1) = y; m.at(3,2) = z;
    return m;
}

static mat4 load_mat4(const float *p) {
    mat4 m; std::memcpy(m.m, p, 64); return m;
}

ArcballCamera::ArcballCamera(const float *projMatrix16,
                             const float *viewMatrix16,
                             const float *sceneBoundsMin,
                             const float *sceneBoundsMax,
                             float windowW, float windowH)
{
    ribProj_   = load_mat4(projMatrix16);
    ribView_   = load_mat4(viewMatrix16);
    projMatrix_ = ribProj_;

    vec3 bmin{sceneBoundsMin[0], sceneBoundsMin[1], sceneBoundsMin[2]};
    vec3 bmax{sceneBoundsMax[0], sceneBoundsMax[1], sceneBoundsMax[2]};
    vec3 center{ (bmin.x+bmax.x)*0.5f, (bmin.y+bmax.y)*0.5f, (bmin.z+bmax.z)*0.5f };

    auto [orient, dist] = decompose(ribView_, center);

    orbitCenter_     = center;
    orientation_     = orient;
    distance_        = dist;
    initOrbitCenter_ = center;
    initOrientation_ = orient;
    initDistance_    = dist;

    windowW_ = windowW;
    windowH_ = windowH;
    radius_  = std::sqrt(windowW*windowW + windowH*windowH) * 0.5f;
}

vec3 ArcballCamera::toSphere(float sx, float sy) const {
    float cx = windowW_ * 0.5f;
    float cy = windowH_ * 0.5f;
    float dx = (sx - cx) / radius_;
    float dy = -(sy - cy) / radius_;   // screen Y is down, sphere Y is up
    float l2 = dx*dx + dy*dy;
    if (l2 > 1.f) {
        float l = std::sqrt(l2);
        return {dx/l, dy/l, 0.f};
    }
    return {dx, dy, std::sqrt(1.f - l2)};
}

// Extract orientation quaternion and orbit distance from worldToCamera.
// worldToCamera = T(0,0,-d) * R * T(oc)
// t = -R*oc + (0,0,-d)  →  d = -(R*oc + t).z
std::pair<quatf,float> ArcballCamera::decompose(const mat4 &wc, vec3 oc) {
    // Upper-left 3×3 columns (column-major: col 0 = first 3 elements, etc.)
    float r00=wc.at(0,0), r10=wc.at(0,1), r20=wc.at(0,2);
    float r01=wc.at(1,0), r11=wc.at(1,1), r21=wc.at(1,2);
    float r02=wc.at(2,0), r12=wc.at(2,1), r22=wc.at(2,2);
    float tx=wc.at(3,0), ty=wc.at(3,1), tz=wc.at(3,2);

    // R*oc
    float rox = r00*oc.x + r01*oc.y + r02*oc.z;
    float roy = r10*oc.x + r11*oc.y + r12*oc.z;
    float roz = r20*oc.x + r21*oc.y + r22*oc.z;

    float d = -(roz + tz);
    float dist = std::max(d, 1.f);

    // Build quaternion from upper-left 3×3 rotation sub-matrix.
    // Shepperd's method.
    float trace = r00 + r11 + r22;
    quatf q;
    if (trace > 0.f) {
        float s = 0.5f / std::sqrt(trace + 1.f);
        q.w = 0.25f / s;
        q.x = (r12 - r21) * s;
        q.y = (r20 - r02) * s;
        q.z = (r01 - r10) * s;
    } else if (r00 > r11 && r00 > r22) {
        float s = 2.f * std::sqrt(1.f + r00 - r11 - r22);
        q.w = (r12 - r21) / s;
        q.x = 0.25f * s;
        q.y = (r01 + r10) / s;
        q.z = (r20 + r02) / s;
    } else if (r11 > r22) {
        float s = 2.f * std::sqrt(1.f + r11 - r00 - r22);
        q.w = (r20 - r02) / s;
        q.x = (r01 + r10) / s;
        q.y = 0.25f * s;
        q.z = (r12 + r21) / s;
    } else {
        float s = 2.f * std::sqrt(1.f + r22 - r00 - r11);
        q.w = (r01 - r10) / s;
        q.x = (r20 + r02) / s;
        q.y = (r12 + r21) / s;
        q.z = 0.25f * s;
    }
    return {quat_normalize(q), dist};
}

static mat4 currentViewMatrix(vec3 oc, quatf orient, float dist) {
    mat4 T_dist  = translate_mat4(0, 0, -dist);
    mat4 R       = quat_to_mat4(orient);
    mat4 T_center = translate_mat4(oc.x, oc.y, oc.z);
    return mat4_mul(mat4_mul(T_dist, R), T_center);
}

void ArcballCamera::viewProjectionMatrix(float *out16) const {
    mat4 vp = mat4_mul(projMatrix_, currentViewMatrix(orbitCenter_, orientation_, distance_));
    std::memcpy(out16, vp.m, 64);
}

void ArcballCamera::cameraToWorldMatrix(float *out16) const {
    mat4 view = currentViewMatrix(orbitCenter_, orientation_, distance_);
    mat4 c2w  = mat4_inverse(view);
    std::memcpy(out16, c2w.m, 64);
}

bool ArcballCamera::isOrthographic() const {
    return projMatrix_.at(3,2) != -1.f;
}

float ArcballCamera::fovDegrees() const {
    float fv = projMatrix_.at(1,1);
    if (fv <= 0.f) return 45.f;
    return 2.f * std::atan(1.f / fv) * (180.f / static_cast<float>(M_PI));
}

void ArcballCamera::beginOrbit(float sx, float sy) {
    savedOrientation_ = orientation_;
    dragFromSphere_   = toSphere(sx, sy);
}

void ArcballCamera::orbit(float sx, float sy) {
    vec3 to   = toSphere(sx, sy);
    vec3 from = dragFromSphere_;
    vec3 axis = cross3(from, to);
    float cosA = dot3(from, to);
    quatf drag{axis.x, axis.y, axis.z, cosA};
    orientation_ = quat_normalize(quat_mul(drag, savedOrientation_));
}

void ArcballCamera::beginPan(float sx, float sy) {
    savedOrbitCenter_ = orbitCenter_;
    panFrom_ = {sx, sy};
}

void ArcballCamera::pan(float sx, float sy) {
    float dx = sx - panFrom_.x;
    float dy = sy - panFrom_.y;
    mat4 view = currentViewMatrix(orbitCenter_, orientation_, distance_);
    mat4 c2w  = mat4_inverse(view);
    // Camera X and Y in world space (columns 0 and 1 of c2w).
    vec3 right{c2w.at(0,0), c2w.at(0,1), c2w.at(0,2)};
    vec3 up   {c2w.at(1,0), c2w.at(1,1), c2w.at(1,2)};
    float speed = 0.001f * distance_;
    orbitCenter_.x = savedOrbitCenter_.x - right.x*(dx*speed) + up.x*(dy*speed);
    orbitCenter_.y = savedOrbitCenter_.y - right.y*(dx*speed) + up.y*(dy*speed);
    orbitCenter_.z = savedOrbitCenter_.z - right.z*(dx*speed) + up.z*(dy*speed);
}

void ArcballCamera::zoom(float delta) {
    distance_ = std::max(distance_ * (1.f + delta * 0.1f), 0.01f);
}

void ArcballCamera::reset() {
    orbitCenter_ = initOrbitCenter_;
    orientation_ = initOrientation_;
    distance_    = initDistance_;
    projMatrix_  = ribProj_;
}

void ArcballCamera::updateAspect(float w, float h) {
    windowW_ = w;
    windowH_ = h;
    radius_  = std::sqrt(w*w + h*h) * 0.5f;

    if (projMatrix_.at(3,2) == -1.f) {   // perspective
        float fv     = projMatrix_.at(1,1);
        float aspect = w / h;
        projMatrix_.at(0,0) = fv / aspect;
    }
}
