#include <cstdio>
#include <cstring>
#include <cmath>

// Integration test: load camera-dof.rib through ribpreview_load
// and verify basic invariants.
#include "ribpreview_api.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

int main(int argc, char *argv[]) {
    // Locate the test RIB — prefer argv[1], fall back to relative path
    const char *ribPath = "examples/rib/camera-dof.rib";
    if (argc > 1) ribPath = argv[1];

    PreviewSceneC *scene = ribpreview_load(ribPath);
    if (!scene) {
        fprintf(stderr, "ribpreview_load returned NULL for %s\n", ribPath);
        fprintf(stderr, "test_integration: 0 pass, 1 fail\n");
        return 1;
    }

    CHECK(scene->vertexCount > 0);
    CHECK(scene->camera.nearPlane > 0.0f);
    CHECK(scene->camera.farPlane > scene->camera.nearPlane);

    // All vertices must be finite (no NaN/Inf)
    bool allFinite = true;
    for (int i = 0; i < scene->vertexCount * 3; ++i) {
        float v = scene->vertices[i];
        if (!std::isfinite(v)) { allFinite = false; break; }
    }
    CHECK(allFinite);

    ribpreview_free(scene);

    printf("test_integration: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
