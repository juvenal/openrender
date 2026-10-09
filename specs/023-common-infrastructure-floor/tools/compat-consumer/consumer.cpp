// Spec 023 SC-006 compatibility consumer.
//
// Uses only the installed public header ri.h: RI tokens, the RI error-handler
// API, RiDeclare and a little geometry. It is built against a pre-feature
// install and must keep running, unchanged, after the new libraries are
// installed in their place.
//
// Usage: consumer <render|rib> <output-file>
//   render: RiBegin(RI_NULL) and render a 64x64 image through the "file"
//           display driver (libRIBRender). One thread and one pixel sample
//           keep the output reproducible run to run.
//   rib:    RiBegin(<output-file>) (libRIBVector / libRIBOut, where
//           RiBegin(RI_NULL) is unsupported).

#include <cstdio>
#include <cstring>

#include "ri.h"

int main(int argc, char **argv) {
    if (argc != 3 || (strcmp(argv[1], "render") != 0 && strcmp(argv[1], "rib") != 0)) {
        fprintf(stderr, "usage: %s <render|rib> <output-file>\n", argv[0]);
        return 2;
    }
    const bool render = strcmp(argv[1], "render") == 0;
    const char *output = argv[2];

    RiBegin(render ? RI_NULL : output);
    RiErrorHandler(RiErrorPrint);
    RiDeclare("foo", "uniform float");

    if (render) {
        RtInt one = 1;
        RiOption(RI_LIMITS, RI_NUMTHREADS, &one, RI_NULL);
        RiPixelSamples(1, 1);
        RiFormat(64, 64, 1);
        RiDisplay(output, RI_FILE, RI_RGBA, RI_NULL);
    }
    RiProjection(RI_PERSPECTIVE, RI_NULL);
    RiTranslate(0, 0, 4);

    RiWorldBegin();
    RtFloat foo = 0.5f;
    RiSphere(1, -1, 1, 360, "foo", &foo, RI_NULL);
    RtPoint quad[4] = {{-1, -1, 1.5f}, {1, -1, 1.5f}, {1, 1, 1.5f}, {-1, 1, 1.5f}};
    RtColor colors[4] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
    RiPolygon(4, RI_P, (RtPointer)quad, RI_CS, (RtPointer)colors, RI_NULL);
    RiWorldEnd();

    RiEnd();
    return 0;
}
