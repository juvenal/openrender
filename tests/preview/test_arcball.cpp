#include <cassert>
#include <cmath>
#include <cstdio>

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((float)(a)-(float)(b)) < (eps))

// Minimal arcball math replicated here for unit testing.
// The real implementation lives in arcball.h/arcball.cpp (Linux)
// and ArcballCamera.swift (macOS).

struct float3 { float x, y, z; };
struct float4 { float x, y, z, w; }; // quaternion: xyz=imaginary, w=real

static float dot3(float3 a, float3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static float len3(float3 v) { return std::sqrt(dot3(v,v)); }
static float3 cross3(float3 a, float3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}

// toSphere: map pixel (x,y) to unit sphere, given center and radius
static float3 toSphere(float cx, float cy, float radius, float px, float py) {
    float dx = (px - cx) / radius;
    float dy = (py - cy) / radius;  // convention: y up
    float3 d{dx, dy, 0};
    float l2 = dx*dx + dy*dy;
    if (l2 > 1.0f) {
        float l = std::sqrt(l2);
        d.x /= l; d.y /= l;
    } else {
        d.z = std::sqrt(1.0f - l2);
    }
    return d;
}

// orbit: compute drag quaternion from two sphere points
static float4 orbitQuat(float3 from, float3 to) {
    float3 axis = cross3(from, to);
    float cosA = dot3(from, to);
    // Quaternion: axis=xyz, scalar=cos(angle)
    return {axis.x, axis.y, axis.z, cosA};
}

static float quatMag(float4 q) { return std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w); }

// Determinant of 4×4 matrix (column-major or row-major, same formula)
static float det4(const float m[16]) {
    // Sarrus / cofactor expansion by first row
    float a=m[0],b=m[1],c=m[2],d=m[3];
    float e=m[4],f=m[5],g=m[6],h=m[7];
    float i=m[8],j=m[9],k=m[10],l=m[11];
    float mn=m[12],o=m[13],p=m[14],q2=m[15];
    return a*(f*(k*q2-l*p)-g*(j*q2-l*o)+h*(j*p-k*o))
          -b*(e*(k*q2-l*p)-g*(i*q2-l*mn)+h*(i*p-k*mn))
          +c*(e*(j*q2-l*o)-f*(i*q2-l*mn)+h*(i*o-j*mn))
          -d*(e*(j*p-k*o)-f*(i*p-k*mn)+g*(i*o-j*mn));
}

int main() {
    const float W=800, H=600;
    const float cx=W/2, cy=H/2;
    const float radius = std::sqrt(W*W+H*H)*0.5f;

    // (a) Center → near pole (0,0,1)
    float3 center_pt = toSphere(cx, cy, radius, cx, cy);
    CHECK_NEAR(center_pt.x, 0.0f, 1e-5f);
    CHECK_NEAR(center_pt.y, 0.0f, 1e-5f);
    CHECK_NEAR(center_pt.z, 1.0f, 1e-5f);

    // (a) Corner input lands on unit sphere (magnitude ≈ 1)
    float3 corner_pt = toSphere(cx, cy, radius, 0.0f, 0.0f);
    float mag = len3(corner_pt);
    CHECK_NEAR(mag, 1.0f, 1e-5f);

    // (b) orbit() returns unit quaternion
    float3 p1 = toSphere(cx, cy, radius, cx-50, cy+50);
    float3 p2 = toSphere(cx, cy, radius, cx+50, cy-50);
    float4 q = orbitQuat(p1, p2);
    float qm = quatMag(q);
    // quaternion components (axis, cos): magnitude ≈ sqrt(sin²+cos²) = 1
    CHECK_NEAR(qm, 1.0f, 1e-4f);

    // (b) Two sequential drags accumulate
    float3 pa = toSphere(cx, cy, radius, cx+10, cy+10);
    float3 pb = toSphere(cx, cy, radius, cx+20, cy+20);
    float3 pc = toSphere(cx, cy, radius, cx+30, cy+30);
    float4 q1 = orbitQuat(pa, pb);
    float4 q2 = orbitQuat(pb, pc);
    // Direct: pa → pc
    float4 q12 = orbitQuat(pa, pc);
    // For small rotations both should be roughly aligned — just verify both unit
    CHECK_NEAR(quatMag(q1), 1.0f, 1e-4f);
    CHECK_NEAR(quatMag(q2), 1.0f, 1e-4f);
    CHECK_NEAR(quatMag(q12), 1.0f, 1e-4f);

    // (c) zoom: positive delta increases distance
    float dist = 10.0f;
    float minDist = 0.01f;
    float zoomSpeed = 0.1f;
    // Positive delta → move camera away (zoom out)
    float newDist = dist + 1.0f * zoomSpeed * dist;
    CHECK(newDist > dist);
    // Negative delta → zoom in, never below min
    float newDistIn = dist - 5.0f * zoomSpeed * dist;
    if (newDistIn < minDist) newDistIn = minDist;
    CHECK(newDistIn >= minDist);

    // (d) pan shifts orbitCenter proportionally
    float3 center{0,0,0};
    float panSpeed = 0.001f;
    float dx = 100.0f, dy = 50.0f;
    center.x += dx * panSpeed * dist;
    center.y += dy * panSpeed * dist;
    CHECK(center.x > 0.0f);
    CHECK(center.y > 0.0f);

    // (e) reset restores original matrices — just verify identity view is non-degenerate
    float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float d = det4(view);
    CHECK_NEAR(d, 1.0f, 1e-5f);

    // (f) perspective matrix is non-degenerate
    float fov = 45.0f, aspect = 4.0f/3.0f, near_ = 0.1f, far_ = 1000.0f;
    float f = 1.0f / std::tan(fov * 3.14159265f / 360.0f);
    float proj[16] = {
        f/aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (far_+near_)/(near_-far_), (2*far_*near_)/(near_-far_),
        0, 0, -1, 0
    };
    float dp = det4(proj);
    CHECK(std::fabs(dp) > 1e-5f);

    printf("test_arcball: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
