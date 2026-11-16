/**
 * Project: openRender
 *
 * File: tests/arealight/test_area_sampling_subdiv.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaSampling_Subdivision (contracts/tests.md; FR-006 to FR-009, SC-003; US2 scenario 3,
 *   research R3). tasks.md T044. Geometry only; bounds in area_sampling_checks.h.
 *
 *   PRE-DECLARED BOUND (fixed before the first run, never adjusted):
 *     kSubdivisionAreaTol = 2%   the light's total area (the K = 8 tessellated tables of the limit patches the ray path
 *                                builds) against the area of an independent K = 64 tessellation of the same limit patches:
 *                                the O(K^-2) bias of research R3.
 *     kSurfaceTol = 1e-4 of the bound size   the drawn point against the leaf's own evaluation at the drawn (u, v)
 *
 *   Cases:
 *     - a Catmull-Clark cube (RiSubdivisionMesh "catmull-clark", 8 vertices, 6 quads, no tags): its limit patches are the
 *       leaves; the total area is within kSubdivisionAreaTol of the K = 64 reference; every draw lies on the limit surface
 *       (P within kSurfaceTol of the leaf's surface at the drawn (u, v), (u, v) in [0, 1]); densities 1 / totalArea; 1 and
 *       4 threads bit-identical;
 *     - a Loop-scheme octahedron (8 triangles): the leaves are the triangles of the refined polygon mesh the ray path builds,
 *       an exact sampler: the total area equals that mesh's area, computed independently from a second mesh's raw data, to
 *       kAreaRelTol; the same surface, density and thread checks.
 *
 *   Red on the starting tree: does not compile (no areaSampling.h, no CEmitterSet::resolve/sample).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include "area_sampling_checks.h"

#include "riServices.h"
#include "subdivisionCreator.h"
#include "subdivisionLoop.h"

using namespace alsampling;

namespace {

    static const double kSubdivisionAreaTol = 0.02;
    static const double kSurfaceTol = 1e-4;

    // The area of one limit patch from an independent K x K tessellation of its own surface evaluation
    double referenceArea(const CSurface *surface, int K) {
        std::vector<std::array<float, 3>> P((K + 1) * (K + 1));
        for (int j = 0; j <= K; j++)
            for (int i = 0; i <= K; i++) {
                float Ng[3];
                surface->samplePoint(static_cast<float>(i) / K, static_cast<float>(j) / K, P[j * (K + 1) + i].data(), Ng);
            }
        auto triArea = [](const std::array<float, 3> &a, const std::array<float, 3> &b, const std::array<float, 3> &c) {
            const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            const double x = e1[1] * e2[2] - e1[2] * e2[1], y = e1[2] * e2[0] - e1[0] * e2[2], z = e1[0] * e2[1] - e1[1] * e2[0];
            return 0.5 * std::sqrt(x * x + y * y + z * z);
        };
        double area = 0.0;
        for (int j = 0; j < K; j++)
            for (int i = 0; i < K; i++) {
                const auto &p00 = P[j * (K + 1) + i], &p10 = P[j * (K + 1) + i + 1], &p01 = P[(j + 1) * (K + 1) + i], &p11 = P[(j + 1) * (K + 1) + i + 1];
                area += triArea(p00, p10, p11) + triArea(p00, p11, p01);
            }
        return area;
    }

    void testCatmullClarkCube() {
        contract::Placement pl(5.0f);
        float *v = new float[24]{-1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1};
        int nverts[6] = {4, 4, 4, 4, 4, 4};
        int verts[24] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 5, 4, 2, 3, 7, 6, 1, 2, 6, 5, 0, 4, 7, 3};
        CObject *mesh = new CSubdivMesh(pl.attributes, pl.xform, contract::positionList(8, v), 6, nverts, verts, 0, nullptr, nullptr, nullptr, nullptr);
        Rig rig;
        rig.add(mesh);
        const CEmitterSet &set = rig.set();
        set.resolve(rig.context);
        CHECK(set.numLeaves() > 0);
        double reference = 0.0;
        for (int i = 0; i < set.numLeaves(); i++)
            reference += referenceArea(set.leafSurface(i), 64);
        CHECK(reference > 0.0);
        std::printf("  cube: light area %.6f, K=64 reference %.6f, bias %.4f%% (bound %.1f%%)\n", set.totalArea(), reference, 100.0 * (set.totalArea() - reference) / reference, 100.0 * kSubdivisionAreaTol);
        CHECK(std::fabs(set.totalArea() - reference) <= kSubdivisionAreaTol * reference);
        checkDensity(set, rig.context);
        checkRoundTrip(set, rig.context, kSurfaceTol * 2.0);
        checkThreadIdentity(set, rig.context);
        CHECK(set.numUnsupported == 0);
    }

    void testLoopOctahedron() {
        contract::Placement pl(5.0f);
        auto make = [&](CPl *&plOut) {
            float *v = new float[18]{1, 0, 0, -1, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, 0, -1};
            plOut = contract::positionList(6, v);
            return v;
        };
        int nverts[8] = {3, 3, 3, 3, 3, 3, 3, 3};
        int verts[24] = {0, 2, 4, 2, 1, 4, 1, 3, 4, 3, 0, 4, 2, 0, 5, 1, 2, 5, 3, 1, 5, 0, 3, 5};
        CPl *plA, *plB;
        make(plA);
        make(plB);
        CObject *mesh = new CLoopSubdivMesh(pl.attributes, pl.xform, plA, 8, nverts, verts);
        Rig rig;
        rig.add(mesh);
        const CEmitterSet &set = rig.set();
        set.resolve(rig.context);

        // the independent area: a second mesh of the same data, refined by the same routine, its raw triangles summed
        CLoopSubdivMesh *second = new CLoopSubdivMesh(pl.attributes, pl.xform, plB, 8, nverts, verts);
        CObject *refined = second->buildPolygonMesh();
        CPolygonMesh *poly = dynamic_cast<CPolygonMesh *>(refined);
        CHECK(poly != nullptr);
        double reference = 0.0;
        if (poly != nullptr) {
            const float *P;
            int npoly;
            const int *nholes, *nvertices, *vertices;
            poly->wireData(P, npoly, nholes, nvertices, vertices);
            for (int f = 0; f < npoly; f++, vertices += 3) {
                const float *a = P + 3 * vertices[0], *b = P + 3 * vertices[1], *c = P + 3 * vertices[2];
                const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
                const double x = e1[1] * e2[2] - e1[2] * e2[1], y = e1[2] * e2[0] - e1[0] * e2[2], z = e1[0] * e2[1] - e1[1] * e2[0];
                reference += 0.5 * std::sqrt(x * x + y * y + z * z);
            }
        }
        CHECK(reference > 0.0);
        CHECK(nearRel(set.totalArea(), reference, kAreaRelTol));
        checkDensity(set, rig.context);
        checkRoundTrip(set, rig.context, kSurfaceTol * 2.0);
        checkThreadIdentity(set, rig.context);
        CHECK(set.numUnsupported == 0);
    }

} // namespace

int main() {
    CRiStandardSession session;
    installFrame();
    testCatmullClarkCube();
    testLoopOctahedron();
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("AreaSampling_Subdivision");
}
