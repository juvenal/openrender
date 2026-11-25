/*
 * Spec 035, Phase 3 (T054, R-5): Closure_Harness_InProcessRender (label closures).
 *
 * The proof the mode tests stand on: an in-process render through the delegating cldbl backend (cldbl_backend.h). A session registers
 * cldbl, binds a cldbl shader (a wrapper of cl_probe_surface, compiled to .rslo and .slo) through a Surface statement, renders an 8 x 8
 * frame (raytrace hider, PixelSamples 1 1, box filter, one thread) twice in this process, once through the interpreter and once through
 * the JIT, and reads the pixels back through a custom display. Asserts the colour of every pixel (the object's Cs), that the wrapper ran the
 * real backend of the format asked for, and that the wrapper's real-point counter (the sum of numRealVertices over its execute calls, the
 * oracle of research C2) equals the number of pixels. No 035 production code is needed for it.
 */
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "cldbl_backend.h"
#include "config.h"
#include "ri/parse/ri.h"
#include "test_support.h"

namespace {

    constexpr int kSize = 8;

    struct SImage {
            std::vector<float> rgb = std::vector<float>(kSize * kSize * 3, -1.0f);
            int dataCalls = 0, finishCalls = 0, samples = 0;
    } g_image;
    int g_handle;

    void *displayStart(const char *, int width, int height, int numSamples, const char *, RtDisplayParameterFunction) {
        g_image.samples = numSamples;
        if (width != kSize || height != kSize)
            return nullptr;
        return &g_handle;
    }

    int displayData(void *handle, int x, int y, int w, int h, float *data) {
        if (handle != &g_handle)
            return 0;
        ++g_image.dataCalls;
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i)
                for (int k = 0; k < 3; ++k)
                    g_image.rgb[((y + j) * kSize + (x + i)) * 3 + k] = data[(j * w + i) * g_image.samples + k];
        return 1;
    }

    void displayFinish(void *handle) {
        if (handle == &g_handle)
            ++g_image.finishCalls;
    }

    // One session: an 8 x 8 frame of a polygon that covers it, shaded by the cldbl shader `shader`, under Option "shaderformat" "default" `format`
    void renderFrame(const char *shader, const char *format, const char *hider) {
        g_image = SImage();
        RtInt threads = 1;
        const char *formatName = format;
        RtColor colour = {0.25f, 0.5f, 0.75f};
        RtColor opaque = {1.0f, 1.0f, 1.0f};
        RtPoint quad[4] = {{-4, -4, 1}, {4, -4, 1}, {4, 4, 1}, {-4, 4, 1}};
        RiBegin(RI_NULL);
        RiOption(RI_LIMITS, RI_NUMTHREADS, &threads, RI_NULL);
        RiOption(RI_SHADERFORMAT, "default", &formatName, RI_NULL);
        RiHider(hider, RI_NULL);
        RiFormat(kSize, kSize, 1);
        RiPixelSamples(1, 1);
        RiPixelFilter(RiBoxFilter, 1, 1);
        RiCustomDisplay("cl-harness", RI_RGB, displayStart, displayData, displayFinish, RI_NULL);
        RiProjection("orthographic", RI_NULL);
        RiWorldBegin();
        RiColor(colour);
        RiOpacity(opaque);
        RiSurface(shader, RI_NULL);
        RiPolygon(4, RI_P, (RtPointer)quad, RI_NULL);
        RiWorldEnd();
        RiEnd();
    }

    void check(const char *shader, const char *format, bool jit, const char *hider) {
        cldbl::counters().reset();
        renderFrame(shader, format, hider);
        const bool raytrace = std::string(hider) == "raytrace";
        std::printf("  %s, %s hider (Option shaderformat %s): %ld executes, %ld real points, via rslo %ld, via slo %ld, %d data call(s)\n", shader, hider, format, cldbl::counters().executes.load(),
                    cldbl::counters().realPoints.load(), cldbl::counters().viaRslo.load(), cldbl::counters().viaSlo.load(), g_image.dataCalls);
        CL_CHECK(g_image.dataCalls > 0 && g_image.finishCalls == 1);
        CL_CHECK(g_image.samples == 3);
        int wrong = 0;
        for (int p = 0; p < kSize * kSize; ++p) {
            const float want[3] = {0.25f, 0.5f, 0.75f};
            for (int k = 0; k < 3; ++k)
                if (std::fabs(g_image.rgb[p * 3 + k] - want[k]) > 1e-3f)
                    ++wrong;
        }
        CL_CHECK(wrong == 0); // every pixel is the object's Cs
        CL_CHECK(cldbl::counters().loads == 1 && cldbl::counters().instances >= 1);
        CL_CHECK(cldbl::counters().executes > 0);
        if (raytrace)
            CL_CHECK(cldbl::counters().realPoints == kSize * kSize); // one primary ray per pixel, no derivative-helper vertex counted
        else
            CL_CHECK(cldbl::counters().realPoints >= kSize * kSize); // grid points: more than the pixels (the oracle of the mode tests is the count of the color render itself)
        // the wrapper ran the real backend of the format asked for, and no other
        CL_CHECK(jit ? (cldbl::counters().viaSlo == cldbl::counters().executes && cldbl::counters().viaRslo == 0)
                     : (cldbl::counters().viaRslo == cldbl::counters().executes && cldbl::counters().viaSlo == 0));
    }

} // namespace

int main() {
    cldbl::registerBackend(); // before the first RiBegin: the shading system takes the registry's entries when a session begins
    for (const char *hider : {"raytrace", "zbuffer", "reyes"}) {
        std::printf("harness: interpreter\n");
        check("cl_probe_rslo", "rslo", false, hider);
#ifdef OPENRENDER_HAVE_LLVM
        std::printf("harness: JIT, another session in the same process\n");
        check("cl_probe_slo", "slo", true, hider);
#endif
    }
    cldbl::unregisterBackend();
    return CL_RESULT("Closure_Harness_InProcessRender");
}
