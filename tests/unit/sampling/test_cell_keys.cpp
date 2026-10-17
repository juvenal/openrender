// Sampling_CellKeyUniqueness (spec 039, Phase 4, T042; research R3.3 and V2): the
// key of a grid-shaded point is cellKey(stableId, u, v, elementId, frame, seed),
// so within a frame
//
//   - two shading points that differ in (object, u, v) have different keys, and
//   - a point shaded twice at the same (object, u, v), as the edge shared by two
//     grids is, has the same key both times.
//
// The test renders a small scene of one primitive family under "sobol" and reads
// the keys the shading engine reports through CShadingContext::pointKeyObserver
// (see scene_probe.h for why this is not an image-based probe). Families: patch,
// polygon mesh, quadric, NURBS, subdivision surface, points and curves.
//
// Points are the critical family. CPoints::sample() writes no u and v, so every
// point of the object has the same (u, v); the key must then come from the
// per-vertex elementId (the point's index). For points the check is therefore
// stronger: every point shaded has its own key.
//
// The checks are on keys, not pixels. At the default shading rate one
// micropolygon covers several pixels, so equal pixels would prove nothing.
#include <cstring>
#include <map>
#include <set>
#include <tuple>

#include "scene_probe.h"

using namespace probe;

static int failures = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

static uint32_t bitsOf(float f) {
    uint32_t b;
    std::memcpy(&b, &f, sizeof b);
    return b;
}

// --- the families ------------------------------------------------------------

static void patch() {
    float P[16 * 3];
    gridPoints(P, 4, 4, 1.5f);
    RiPatch(RI_BICUBIC, RI_P, P, RI_NULL);
}

static void mesh() {
    int nverts[4] = {4, 4, 4, 4};
    int verts[16] = {0, 1, 4, 3, 1, 2, 5, 4, 3, 4, 7, 6, 4, 5, 8, 7};
    float P[9 * 3];
    gridPoints(P, 3, 3, 1.5f);
    RiPointsPolygons(4, nverts, verts, RI_P, P, RI_NULL);
}

static void quadric() {
    RiSphere(1.2f, -1.2f, 1.2f, 360.0f, RI_NULL);
}

static void nurbs() {
    float knots[8] = {0, 0, 0, 0, 1, 1, 1, 1};
    float P[16 * 3];
    gridPoints(P, 4, 4, 1.5f);
    RiNuPatch(4, 4, knots, 0.0f, 1.0f, 4, 4, knots, 0.0f, 1.0f, RI_P, P, RI_NULL);
}

static void subdivision() {
    float P[8 * 3];
    for (int i = 0; i < 8; ++i) {
        P[3 * i + 0] = (i & 1) ? 0.9f : -0.9f;
        P[3 * i + 1] = (i & 2) ? 0.9f : -0.9f;
        P[3 * i + 2] = (i & 4) ? 0.9f : -0.9f;
    }
    int nverts[6] = {4, 4, 4, 4, 4, 4};
    int verts[24] = {0, 1, 3, 2, 4, 6, 7, 5, 0, 4, 5, 1, 2, 3, 7, 6, 0, 2, 6, 4, 1, 5, 7, 3};
    RiSubdivisionMesh("catmull-clark", 6, nverts, verts, 0, nullptr, nullptr, nullptr, nullptr, RI_P, P, RI_NULL);
}

static const int kNumPoints = 40;

static void points() {
    float P[kNumPoints * 3];
    float width[kNumPoints];
    for (int i = 0; i < kNumPoints; ++i) {
        P[3 * i + 0] = -1.4f + 2.8f * (i % 8) / 7.0f;
        P[3 * i + 1] = -0.9f + 1.8f * (i / 8) / 4.0f;
        P[3 * i + 2] = 0.0f;
        width[i] = 0.18f;
    }
    RiPoints(kNumPoints, RI_P, P, RI_WIDTH, width, RI_NULL);
}

static void curves() {
    int nverts[3] = {4, 4, 4};
    float P[12 * 3];
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < 4; ++i) {
            P[3 * (4 * c + i) + 0] = -1.2f + 0.8f * c + 0.2f * i;
            P[3 * (4 * c + i) + 1] = -1.0f + 0.65f * i;
            P[3 * (4 * c + i) + 2] = 0.0f;
        }
    float width = 0.15f;
    RiCurves(RI_CUBIC, 3, nverts, RI_NONPERIODIC, RI_P, P, RI_CONSTANTWIDTH, &width, RI_NULL);
}

// --- the check -----------------------------------------------------------------

static void run(void (*family)(), bool isPoints) {
    Session s(1, "sobol", 96, 72);
    family();
    s.render();

    size_t shaded = 0;
    std::map<uint64_t, std::tuple<uint64_t, uint32_t, uint32_t>> keyOwner; // key -> (object, u, v)
    std::map<std::tuple<uint64_t, uint32_t, uint32_t>, uint64_t> tupleKey; // (object, u, v) -> key
    std::set<uint64_t> keys;
    std::set<uint64_t> objects;
    int collisions = 0, unstable = 0;

    for (const Report &r : reports()) {
        objects.insert(r.stableId);
        for (size_t i = 0; i < r.keys.size(); ++i) {
            ++shaded;
            const uint64_t key = r.keys[i].full;
            keys.insert(key);
            if (isPoints)
                continue;
            const auto tuple = std::make_tuple(r.stableId, bitsOf(r.u[i]), bitsOf(r.v[i]));
            auto owner = keyOwner.find(key);
            if (owner != keyOwner.end() && owner->second != tuple)
                ++collisions; // one key for two different points
            keyOwner[key] = tuple;
            auto seen = tupleKey.find(tuple);
            if (seen != tupleKey.end() && seen->second != key)
                ++unstable; // one point with two keys
            tupleKey[tuple] = key;
        }
    }
    std::fprintf(stderr, "  %zu shaded point(s) in %zu report(s), %zu object(s), %zu distinct key(s)\n", shaded,
                 reports().size(), objects.size(), keys.size());

    CHECK(shaded > 0); // the hook fired: keys are filled under a non-legacy sampler
    if (isPoints) {
        // Every point has its own key.
        CHECK(shaded == (size_t)kNumPoints);
        CHECK(keys.size() == shaded);
    }
    else {
        CHECK(collisions == 0);
        CHECK(unstable == 0);
        CHECK(keys.size() == tupleKey.size()); // one key per distinct (object, u, v)
    }
    s.close();
}

int main(int argc, char **argv) {
    const std::string which = argc >= 2 ? argv[1] : "";
    if (which == "patch")
        run(patch, false);
    else if (which == "mesh")
        run(mesh, false);
    else if (which == "quadric")
        run(quadric, false);
    else if (which == "nurbs")
        run(nurbs, false);
    else if (which == "subdivision")
        run(subdivision, false);
    else if (which == "points")
        run(points, true);
    else if (which == "curves")
        run(curves, false);
    else {
        std::fprintf(stderr, "usage: %s patch|mesh|quadric|nurbs|subdivision|points|curves\n", argv[0]);
        return 2;
    }
    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    return 0;
}
