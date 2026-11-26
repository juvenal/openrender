/**
 * Project: openRender
 *
 * File: test_object_scope_preview.cpp
 *
 * Description:
 *   GitHub #235 (the preview side): ObjectBegin-ObjectEnd saves and restores the attributes (RISpec 3.2 section 4), so a colour set inside an object
 *   definition is not current after ObjectEnd. A scene sets a blue colour, defines an object under a red one, draws a polygon after ObjectEnd and then
 *   instances the object. The first polygon (the one drawn after ObjectEnd) must extract blue, and the load must write nothing to stderr.
 *   Run from the repository root; argv[1] is a scratch directory for the scene file.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include <cstdio>
#include <fstream>
#include <string>

#include "ribpreview_api.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <scratch directory>\n", argv[0]);
        return 2;
    }
    const std::string path = std::string(argv[1]) + "/object-scope.rib";
    {
        std::ofstream out(path);
        out << "Projection \"orthographic\"\n"
               "Translate 0 0 5\n"
               "WorldBegin\n"
               "Color 0 0 1\n"
               "ObjectBegin 1\n"
               "  Color 1 0 0\n"
               "  Polygon \"P\" [0 0 0  1 0 0  0 1 0]\n"
               "ObjectEnd\n"
               "Polygon \"P\" [2 0 0  3 0 0  2 1 0]\n"
               "ObjectInstance 1\n"
               "WorldEnd\n";
    }

    PreviewSceneC *scene = ribpreview_load(path.c_str());
    if (scene == nullptr || scene->vertexCount < 6) {
        std::fprintf(stderr, "FAIL: ribpreview_load returned no scene or too few vertices\n");
        return 1;
    }
    int failed = 0;
    for (int i = 0; i < 6; ++i) {
        const float *c = scene->colors + 3 * i;
        if (c[0] != 0.0f || c[1] != 0.0f || c[2] != 1.0f) {
            std::fprintf(stderr, "FAIL: vertex %d of the polygon after ObjectEnd has colour (%g %g %g), expected blue (0 0 1)\n", i, c[0], c[1], c[2]);
            ++failed;
        }
    }
    if (failed == 0)
        std::printf("Preview_ObjectScope: OK\n");
    return failed == 0 ? 0 : 1;
}
