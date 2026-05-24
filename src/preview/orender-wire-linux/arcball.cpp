#include "arcball.h"
#include <cstring>

// ─── Math helpers ─────────────────────────────────────────────────────────────

static float dot3(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static vec3  cross3(vec3 a, vec3 b) {
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}

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

// 4×4 matrix inverse via cofactor expansion.
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
    ribProj_    = load_mat4(projMatrix16);
    projMatrix_ = ribProj_;

    // from = to^{-1} = camera-to-world (baked space: +Z forward)
    mat4 from   = mat4_inverse(load_mat4(viewMatrix16));
    viewMatrix_     = from;
    initViewMatrix_ = from;

    vec3 bmin{sceneBoundsMin[0], sceneBoundsMin[1], sceneBoundsMin[2]};
    vec3 bmax{sceneBoundsMax[0], sceneBoundsMax[1], sceneBoundsMax[2]};
    vec3 sceneCenter{ (bmin.x+bmax.x)*0.5f,
                      (bmin.y+bmax.y)*0.5f,
                      (bmin.z+bmax.z)*0.5f };
    gridOriginWorld_ = sceneCenter;

    // Project sceneCenter into baked space: c = from * (sceneCenter, 1)
    float cx = from.at(0,0)*sceneCenter.x + from.at(1,0)*sceneCenter.y
             + from.at(2,0)*sceneCenter.z + from.at(3,0);
    float cy = from.at(0,1)*sceneCenter.x + from.at(1,1)*sceneCenter.y
             + from.at(2,1)*sceneCenter.z + from.at(3,1);
    float cz = from.at(0,2)*sceneCenter.x + from.at(1,2)*sceneCenter.y
             + from.at(2,2)*sceneCenter.z + from.at(3,2);

    if (cz > 0.01f) {
        initOrbCtrBaked_ = {cx, cy, cz};
    } else {
        float dx=bmax.x-bmin.x, dy=bmax.y-bmin.y, dz=bmax.z-bmin.z;
        float diag = std::sqrt(dx*dx + dy*dy + dz*dz);
        initOrbCtrBaked_ = {0.f, 0.f, std::max(diag, 1.0f)};
    }
    orbCtrBaked_ = initOrbCtrBaked_;

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

void ArcballCamera::viewProjectionMatrix(float *out16) const {
    mat4 vp = mat4_mul(projMatrix_, viewMatrix_);
    std::memcpy(out16, vp.m, 64);
}

void ArcballCamera::cameraToWorldMatrix(float *out16) const {
    std::memcpy(out16, viewMatrix_.m, 64);
}

// Perspective has w-from-z coefficient = 1.0 at flat index 11 = at(col=2,row=3).
bool ArcballCamera::isOrthographic() const {
    return projMatrix_.at(2,3) != 1.0f;
}

float ArcballCamera::fovDegrees() const {
    float fv = projMatrix_.at(1,1);
    if (fv <= 0.f) return 45.f;
    return 2.f * std::atan(1.f / fv) * (180.f / static_cast<float>(M_PI));
}

void ArcballCamera::beginOrbit(float sx, float sy) {
    savedViewMatrix_ = viewMatrix_;
    dragFromSphere_  = toSphere(sx, sy);
}

void ArcballCamera::orbit(float sx, float sy) {
    vec3  to   = toSphere(sx, sy);
    vec3  from = dragFromSphere_;
    vec3  axis = cross3(from, to);
    float cosA = dot3(from, to);
    quatf drag = quat_normalize({axis.x, axis.y, axis.z, cosA});
    mat4  Q    = quat_to_mat4(drag);
    mat4  Tc   = translate_mat4( orbCtrBaked_.x,  orbCtrBaked_.y,  orbCtrBaked_.z);
    mat4  Tn   = translate_mat4(-orbCtrBaked_.x, -orbCtrBaked_.y, -orbCtrBaked_.z);
    viewMatrix_ = mat4_mul(mat4_mul(Tc, mat4_mul(Q, Tn)), savedViewMatrix_);
}

void ArcballCamera::beginPan(float sx, float sy) {
    savedViewMatrix_  = viewMatrix_;
    savedOrbCtrBaked_ = orbCtrBaked_;
    panFrom_ = {sx, sy};
}

void ArcballCamera::pan(float sx, float sy) {
    float dx    = sx - panFrom_.x;
    float dy    = sy - panFrom_.y;
    float speed = std::max(0.001f * savedOrbCtrBaked_.z, 0.005f);
    float dtx   = -dx * speed;
    float dty   =  dy * speed;
    orbCtrBaked_.x = savedOrbCtrBaked_.x + dtx;
    orbCtrBaked_.y = savedOrbCtrBaked_.y + dty;
    viewMatrix_ = mat4_mul(translate_mat4(dtx, dty, 0.f), savedViewMatrix_);
}

void ArcballCamera::zoom(float delta) {
    float oldZ = orbCtrBaked_.z;
    float newZ = std::max(oldZ + oldZ * 0.1f * delta, 0.01f);
    orbCtrBaked_.z = newZ;
    viewMatrix_ = mat4_mul(translate_mat4(0.f, 0.f, newZ - oldZ), viewMatrix_);
}

void ArcballCamera::reset() {
    viewMatrix_  = initViewMatrix_;
    orbCtrBaked_ = initOrbCtrBaked_;
    projMatrix_  = ribProj_;
}

void ArcballCamera::updateAspect(float w, float h) {
    windowW_ = w;
    windowH_ = h;
    radius_  = std::sqrt(w*w + h*h) * 0.5f;
    if (projMatrix_.at(2,3) == 1.0f) {   // perspective
        float fv = projMatrix_.at(1,1);
        projMatrix_.at(0,0) = fv / (w / h);
    }
}
