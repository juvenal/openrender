// Sampling_DisplacePassConsistency (spec 039, Phase 6 follow-up).
//
// The dicer runs a surface's displacement shader in displacement-only passes (the bounding
// and grid-size probes, `displaceOnly` != 0) before the full shading pass that produces the
// final geometry. Under a non-Legacy sampler a displacement shader's random() draws from the
// sub-stream of the shading point's key, a function of (object, u, v, frame, seed) only, so
// a point seen by both kinds of pass must be displaced identically. The displaced-position
// hook (CShadingContext::pointDisplacedObserver) reports every pass; this test pairs up the
// points that occur in both kinds (the same object and the same (u, v), bit for bit) and
// requires their displaced positions to be bit-identical.
//
// The shader is sampling_displace_probe.sl: P += 0.05 * random() * N.
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <tuple>

#include "scene_probe.h"

using namespace probe;

namespace {

int failures = 0;
#define CHECK(cond)                                                                     \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                 \
        }                                                                               \
    } while (0)

uint32_t bitsOf(float f) {
    uint32_t b;
    std::memcpy(&b, &f, sizeof b);
    return b;
}

struct PointPass {
    std::array<uint32_t, 3> P;
};
// (object identity, u bits, v bits) -> the displaced positions seen by probe passes and by full passes
using PointId = std::tuple<uint64_t, uint32_t, uint32_t>;
std::mutex displacedMutex;
std::map<PointId, std::set<std::array<uint32_t, 3>>> probePasses, fullPasses;
size_t numProbeReports = 0, numFullReports = 0;

void observeDisplaced(const CPointDisplacedReport &r) {
    std::lock_guard<std::mutex> lock(displacedMutex);
    (r.displaceOnly != 0 ? numProbeReports : numFullReports)++;
    auto &dest = r.displaceOnly != 0 ? probePasses : fullPasses;
    for (int i = 0; i < r.numVertices; ++i)
        dest[{r.surface->stableId, bitsOf(r.u[i]), bitsOf(r.v[i])}].insert({bitsOf(r.P[3 * i]), bitsOf(r.P[3 * i + 1]), bitsOf(r.P[3 * i + 2])});
}

void sphere() {
    Session s(1, "sobol", 160, 120);
    CShadingContext::pointDisplacedObserver = &observeDisplaced;
    s.emit([&] {
        float bound = 0.2f;
        RiAttribute((RtToken) "displacementbound", (RtToken) "sphere", &bound, RI_NULL);
        RiDisplacement((RtToken) "sampling_displace_probe", RI_NULL);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
    });
    s.render();
    CShadingContext::pointDisplacedObserver = nullptr;

    size_t shared = 0, mismatched = 0, inconsistentProbe = 0, inconsistentFull = 0;
    for (const auto &kv : probePasses) {
        if (kv.second.size() > 1) ++inconsistentProbe;
        const auto full = fullPasses.find(kv.first);
        if (full == fullPasses.end()) continue;
        ++shared;
        if (full->second != kv.second) ++mismatched;
    }
    for (const auto &kv : fullPasses)
        if (kv.second.size() > 1) ++inconsistentFull;

    std::fprintf(stderr, "  %zu displacement-only report(s), %zu full report(s); %zu points in both kinds of pass, %zu of them displaced differently; "
                         "points displaced differently within one kind: %zu in displacement-only passes, %zu in full passes\n",
                 numProbeReports, numFullReports, shared, mismatched, inconsistentProbe, inconsistentFull);
    CHECK(numProbeReports > 0); // the dicer does run displacement-only passes
    CHECK(numFullReports > 0);
    CHECK(shared > 0);          // and they share points with the final shading pass
    CHECK(mismatched == 0);     // a point is displaced the same way in both
    CHECK(inconsistentProbe == 0); // nor are two sightings of a point within the probe passes
    CHECK(inconsistentFull == 0);  // nor within the full passes (grids that share an edge)
    s.close();
}

} // namespace

int main(int argc, char **argv) {
    const std::string which = argc >= 2 ? argv[1] : "";
    if (which == "sphere")
        sphere();
    else {
        std::fprintf(stderr, "usage: %s sphere\n", argv[0]);
        return 2;
    }
    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    return 0;
}
