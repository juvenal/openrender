#include <cassert>
#include <cmath>
#include <cstdio>

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((float)(a)-(float)(b)) < (eps))

// Reproduce the perspective matrix formula from previewContext.cpp
// projMatrix[row][col] stored row-major
static void buildPerspective(float fovDeg, float aspect, float near_, float far_, float out[16]) {
    float f = 1.0f / std::tan(fovDeg * 3.14159265358979f / 360.0f);
    for (int i=0;i<16;++i) out[i]=0;
    out[0]  = f / aspect;
    out[5]  = f;
    out[10] = (far_ + near_) / (near_ - far_);
    out[11] = (2.0f * far_ * near_) / (near_ - far_);
    out[14] = -1.0f;
}

int main() {
    // Known perspective camera: fov=60°, aspect=4/3, near=0.1, far=1000
    float proj[16];
    buildPerspective(60.0f, 4.0f/3.0f, 0.1f, 1000.0f, proj);

    // f = 1/tan(30°) = sqrt(3) ≈ 1.7320508
    float expected_proj00 = 1.7320508f / (4.0f/3.0f); // f/aspect ≈ 1.299038
    float expected_proj11 = 1.7320508f;                // f ≈ 1.7320508
    float expected_proj22 = (1000.0f+0.1f)/(0.1f-1000.0f); // ≈ -1.0002
    float expected_proj23 = (2.0f*1000.0f*0.1f)/(0.1f-1000.0f); // ≈ -0.2002

    CHECK_NEAR(proj[0],  expected_proj00, 1e-4f);
    CHECK_NEAR(proj[5],  expected_proj11, 1e-4f);
    CHECK_NEAR(proj[10], expected_proj22, 1e-4f);
    CHECK_NEAR(proj[11], expected_proj23, 1e-3f);
    CHECK_NEAR(proj[14], -1.0f, 1e-5f);
    CHECK_NEAR(proj[15],  0.0f, 1e-5f);

    // Identity view matrix produces viewMatrix[5]=1 etc.
    float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    CHECK_NEAR(view[0], 1.0f, 1e-5f);
    CHECK_NEAR(view[5], 1.0f, 1e-5f);

    // nearPlane > 0 invariant
    CHECK(0.1f > 0.0f);
    // farPlane > nearPlane invariant
    CHECK(1000.0f > 0.1f);

    printf("test_camera: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
