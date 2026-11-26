/*
 * Spec 035, Phase 3 (T054, T069): the delegating "cldbl" shader backend of the closure tests.
 *
 * A third IShaderBackend (key and extension "cldbl") that WRAPS a compiled shader of the real `rslo` or `slo` backend and reimplements no
 * shading code: its description holds the real description, its instance holds the real instance, and execute() calls the real backend's
 * execute() for the wrapped instance, so a frame shaded through it runs the real interpreter or the real JIT with every builtin and every
 * funnel of the library in it. A cldbl shader file is one line, "<wrapped shader name> <rslo|slo> [kind]", and sits beside the compiled
 * wrapped shader (the wrapper takes the real file's path from its own: <dir of the cldbl file>/<name>.<format>). The kind (T069) is what
 * the wrapper emits: none (not closure capable), "diffuse" (capable; one `diffuse` component of weight Cs and the point's N) or
 * "constdiffuse" (capable; a `diffuse` component of weight Cs and the constant N (0, 0, 1), a tree that depends on Cs alone).
 *
 * The wrapper's counters are the point-count oracle of the tests (research C2): every execute() adds the shading state's
 * numRealVertices, the number of real points of the batch (the derivative-helper vertices of a raytrace batch are not counted).
 *
 * Phase 3 later adds emitsClosures()/executeClosures() to it (T069); the harness proof (T054) delegates execute() only.
 */
#ifndef TESTS_CLOSURES_CLDBL_BACKEND_H
#define TESTS_CLOSURES_CLDBL_BACKEND_H

#include <atomic>

#include "libshader/shading/shaderBackend.h"

namespace cldbl {

    // What the wrapper saw, process-wide (the tests run one scenario at a time)
    struct SCounters {
            std::atomic<long> executes{0};     // calls of execute() the wrapper received
            std::atomic<long> realPoints{0};   // the sum of numRealVertices over those calls
            std::atomic<long> allVertices{0};  // the sum of numVertices: more than realPoints when the batch has derivative-offset vertices
            std::atomic<long> loads{0}, instances{0};
            std::atomic<long> viaRslo{0}, viaSlo{0}; // executes delegated to the interpreter's / the JIT's backend
            void reset() { executes = realPoints = allVertices = loads = instances = viaRslo = viaSlo = 0; }
    };
    SCounters &counters();

    // The wrapper's second oracle (T069): the real instance's `output` variables, summed over the real points of every execute() since
    // resetOutputs(). The test reads a mean (outputMean) to know whether a builtin returned a value or was gated to 0.
    void resetOutputs();
    double outputMean(const char *name); // the sum of the output over the real points / the number of real points (0 if there is none)

    // The wrapper's light counter: the number of illuminate() calls of a wrapped light (each is one light shader run for a batch), and the
    // number of points they ran for.
    long lightRuns();
    long lightPoints();

    const IShaderBackend &backend();
    void registerBackend();   // priority 20, after rslo (0) and slo (10); call before RiBegin
    void unregisterBackend(); // tests only

} // namespace cldbl

#endif
