#include <cmath>
#include <cstdio>

#include "ribpreview_api.h"

// Exercises the six RIB scenes repointed at orender-wire (T030) -- previously named
// Hider "oshow:none" and asserted a guaranteed failure through the now-deleted CShow hider.
// libribpreview has never had subdivision-tessellation coverage before this test; the
// tessellator itself (tessSubdivision.cpp) is pre-existing and unchanged by spec 016, so this
// adds missing regression coverage rather than driving new production code.

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

static const char *SCENES[] = {
    "examples/rib/tests/subdiv-hierarchical-override-wire.rib",
    "examples/rib/tests/subdiv-loop-wire.rib",
    "examples/rib/tests/subdiv-facevarying-seam-wire.rib",
    "examples/rib/tests/subdiv-crease-convergence-wire.rib",
    "examples/rib/tests/subdiv-new-tags-wire.rib",
    "examples/rib/tests/parity/motion-subdiv-translate-wire.rib",
};

int main() {
    for (const char *path : SCENES) {
        PreviewSceneC *scene = ribpreview_load(path);
        if (scene == NULL) {
            ++g_fail;
            fprintf(stderr, "FAIL %s: ribpreview_load returned NULL\n", path);
            continue;
        }

        CHECK(scene->vertexCount > 0);

        bool boundsFinite = true;
        for (int i = 0; i < 3; i++) {
            if (!std::isfinite(scene->bounds.sceneBoundsMin[i]) ||
                !std::isfinite(scene->bounds.sceneBoundsMax[i]))
                boundsFinite = false;
        }
        CHECK(boundsFinite);

        bool verticesFinite = true;
        for (int i = 0; i < scene->vertexCount * 3; i++) {
            if (!std::isfinite(scene->vertices[i]))
                verticesFinite = false;
        }
        CHECK(verticesFinite);

        ribpreview_free(scene);
    }

    printf("test_preview_subdiv: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
