/**
 * Project: openRender
 *
 * File: test_occlusion_culler.cpp
 *
 * Description:
 *   COcclusionCuller::touchNode() keeps every node's depth at the maximum of its children's, so
 *   the maximum opaque depth it reports is the farthest opaque sample in the bucket (GitHub #63).
 *   The reyes and zbuffer hiders cull every object whose nearest depth lies beyond it, so a value
 *   below a live sample drops objects that are visible there.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

// The standard headers first: common/algebra.h (through the renderer headers) defines macros
// that <cmath> must not see.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "common/memory.h"
#include "occlusion.h"

namespace {

    int failures = 0;

    void check(bool ok, const char *what, float got, float want) {
        if (!ok) {
            if (failures < 10)
                std::printf("FAIL %s: got %.9g, want %.9g\n", what, got, want);
            ++failures;
        }
    }

    // Exposes the culler's protected interface; one leaf per sample, as in the hiders
    class CTestCuller : public COcclusionCuller {
        public:
            CTestCuller(int w, float start) : size(w), maxDepth(start) {
                initCuller(w, &maxDepth);
                for (int y = 0; y < size; y++)
                    for (int x = 0; x < size; x++)
                        getNode(x, y)->zmax = start;
                resetHierarchy();
            }

            // A sample's opaque depth moved nearer, as the rasterizers report it
            void touch(int x, int y, float z) { touchNode(getNode(x, y), z); }

            float leaf(int x, int y) { return getNode(x, y)->zmax; }

            // Every inner node above a leaf holds the maximum of its children
            bool consistent() {
                for (int y = 0; y < size; y++)
                    for (int x = 0; x < size; x++)
                        for (COcclusionNode *n = getNode(x, y)->parent; n != NULL; n = n->parent) {
                            float m = n->children[0]->zmax;
                            for (int c = 1; c < 4; c++)
                                if (n->children[c]->zmax > m)
                                    m = n->children[c]->zmax;
                            if (n->zmax != m)
                                return false;
                        }
                return true;
            }

            float farthestLeaf() {
                float m = leaf(0, 0);
                for (int y = 0; y < size; y++)
                    for (int x = 0; x < size; x++)
                        if (leaf(x, y) > m)
                            m = leaf(x, y);
                return m;
            }

            int size;
            float maxDepth;
    };

} // namespace

int main() {
    globalArena.init();

    // The case of GitHub #63: the last leaf of a group to move nearer is not the group's farthest.
    // Its parent must take the group's maximum (4), not the touched leaf's depth (1).
    {
        CTestCuller c(4, 10);
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                if (x >= 2 || y >= 2)
                    c.touch(x, y, 1);
        c.touch(0, 0, 4);
        c.touch(1, 0, 2);
        c.touch(1, 1, 3);
        c.touch(0, 1, 1);
        check(c.maxDepth == 4, "maximum opaque depth after a group's nearest leaf moves last", c.maxDepth, 4);
        check(c.consistent(), "every node holds its children's maximum", c.maxDepth, 4);
    }

    // Random nearer-only updates, as the rasterizers make them: after every one the reported
    // maximum is the farthest leaf, and the hierarchy is consistent
    {
        uint64_t s = 0x9E3779B97F4A7C15ull;
        auto next = [&]() {
            s = s * 6364136223846793005ull + 1442695040888963407ull;
            return (uint32_t)(s >> 33);
        };
        for (int trial = 0; trial < 20; trial++) {
            CTestCuller c(16, 1000);
            bool reported = false;
            for (int k = 0; k < 4000; k++) {
                const int x = (int)(next() % 16), y = (int)(next() % 16);
                const float old = c.leaf(x, y);
                const float z = old * (float)(next() % 1000) / 1000.0f;
                if (!(z < old))
                    continue;
                c.touch(x, y, z);
                const float want = c.farthestLeaf();
                if (!reported && (c.maxDepth != want || !c.consistent())) {
                    check(false, "maximum opaque depth after random nearer updates", c.maxDepth, want);
                    reported = true;
                }
            }
        }
    }

    globalArena.tini();

    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("OK\n");
    return 0;
}
