/**
 * tests/visual/test_dof_shading_rate.cpp
 *
 * Depth-of-field shading rate driver (GitHub #205): renders a scene with one thread and the
 * end-of-frame statistics, reads the "Vertex/Grid" line (the vertices the rasterizer shaded over the
 * grids it made) and checks the vertex count against an expected value. RISpec 3.2 section 4.2.7: a
 * renderer shades at least at ShadingRate, so a scene with depth of field that does not ask for a
 * relaxation shades as often as the same scene without it.
 *
 * Usage:
 *   test_dof_shading_rate <orender> <rib> <expected vertices>
 */

#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <orender> <rib> <expected vertices>\n", argv[0]);
        return 2;
    }
    const long expected = atol(argv[3]);
    const std::string command = std::string("\"") + argv[1] + "\" -t -x 3 -t:1 \"" + argv[2] + "\" 2>&1";
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        fprintf(stderr, "  cannot run orender\n");
        return 2;
    }
    char line[1024];
    long vertices = -1, grids = -1;
    bool unknown = false;
    while (fgets(line, sizeof(line), pipe) != nullptr) {
        float perGrid;
        long v, g;
        if (sscanf(line, " Vertex/Grid: %f (%ld/%ld)", &perGrid, &v, &g) == 3) {
            vertices = v;
            grids = g;
        }
        if (std::string(line).find("Unknown geometric approximation") != std::string::npos)
            unknown = true;
    }
    pclose(pipe);
    if (vertices < 0) {
        fprintf(stderr, "  no Vertex/Grid line in the statistics\n");
        return 2;
    }
    printf("  shaded vertices %ld over %ld grids, expected %ld\n", vertices, grids, expected);
    if (unknown) {
        printf("  the scene's GeometricApproximation was refused  FAIL\n");
        return 1;
    }
    if (vertices != expected) {
        printf("  FAIL\n");
        return 1;
    }
    return 0;
}
