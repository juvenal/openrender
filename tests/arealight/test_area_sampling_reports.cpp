/**
 * Project: openRender
 *
 * File: tests/arealight/test_area_sampling_reports.cpp
 *
 * Description:
 *   Spec 057 (area-light geometry), AreaSampling_Reports (contracts/tests.md "Added at the tasks stage"; decision D-4,
 *   A7; FR-009). tasks.md T045. What resolution tells the author, through the diagnostic sink, once per light:
 *
 *     A  unsupported kinds: a light whose scope holds a point set and a curve mesh (and a quad and a blobby, which do emit:
 *        blobbyCreate() polygonizes a blobby into a CPolygonMesh when it is created, so it is an ordinary mesh emitter)
 *        counts the two (numUnsupported == 2), the quad and the blobby's mesh emit (area 1 plus the mesh's own area), and ONE
 *        report names the light and says the primitives "cannot emit"; resolving again reports nothing more (resolution
 *        runs once);
 *     B  an empty set: a light with no geometry at all warns once, `area light "<name>" has no emitting geometry` (A7), its
 *        total area is 0 and sample() returns pdfA == 0 and no leaf;
 *     C  a CSG leaf (D-4): a light whose scope holds a union of two cubes counts two CSG leaves, samples each from the
 *        whole leaf (area 2 * 24, without the solid's region) and reports once that it does ("CSG", "whole leaf").
 *   Every report is a warning that reaches the sink with a code other than CODE_LOG (which swallows warnings): this test
 *   fails if a report goes through CODE_LOG.
 *
 *   Red on the starting tree: does not compile (no areaSampling.h, no CEmitterSet::resolve/sample/numUnsupported).
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */
#include "area_sampling_checks.h"

#include <string>

#include "blobby.h"
#include "blobbyField.h"
#include "common/diagnostics.h"
#include "csgSolid.h"
#include "csgTestUtils.h"
#include "curves.h"
#include "points.h"
#include "polygons.h"
#include "riServices.h"

using namespace alsampling;

namespace {

    class CaptureSink : public CDiagnosticSink {
        public:
            struct Entry {
                    EErrorCode code;
                    EDiagnosticSeverity severity;
                    std::string text;
            };
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override { entries.push_back({code, severity, message}); }
            // The reports mentioning `light` (its name) and `keyword`
            int count(const char *light, const char *keyword) const {
                int n = 0;
                for (const Entry &e : entries)
                    if (e.text.find(light) != std::string::npos && e.text.find(keyword) != std::string::npos)
                        ++n;
                return n;
            }
            // Every report about `light` is a real warning (never CODE_LOG, which swallows warnings)
            bool allAreWarnings(const char *light) const {
                for (const Entry &e : entries)
                    if (e.text.find(light) != std::string::npos && (e.code == CODE_LOG || e.severity != DIAGNOSTIC_WARNING))
                        return false;
                return true;
            }
            std::vector<Entry> entries;
    };

    // The area of a polygon mesh from its raw data (object space; a translation does not change areas), polygons fanned from
    // their first vertex: the independent reference for a blobby's polygonized surface
    double meshArea(const CPolygonMesh &mesh) {
        const float *P;
        int npoly;
        const int *nholes, *nvertices, *vertices;
        mesh.wireData(P, npoly, nholes, nvertices, vertices);
        double area = 0.0;
        for (int f = 0; f < npoly; f++) {
            const int n = nvertices[f]; // one loop per polygon (nholes[f] == 1): a blobby's mesh has no holes
            for (int k = 1; k + 1 < n; k++) {
                const float *a = P + 3 * vertices[0], *b = P + 3 * vertices[k], *c = P + 3 * vertices[k + 1];
                const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
                const double x = e1[1] * e2[2] - e1[2] * e2[1], y = e1[2] * e2[0] - e1[0] * e2[2], z = e1[0] * e2[1] - e1[1] * e2[0];
                area += 0.5 * std::sqrt(x * x + y * y + z * z);
            }
            vertices += n;
        }
        return area;
    }

    CObject *curveMesh(const contract::Placement &pl) {
        float *v = new float[6]{0, 0, 0, 1, 0, 0};
        int nverts[1] = {2};
        return new CCurveMesh(pl.attributes, pl.xform, contract::positionList(2, v), 1, 2, 1, nverts, 0);
    }

    // One ellipsoid blobby of unit radius at the origin of the placement
    CObject *blobby(const contract::Placement &pl) {
        static const int code[] = {1001, 0, 0, 1, 0};
        static const float floats[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        CBlobbyProgram *program = new CBlobbyProgram(1, 5, code, 16, floats, 0, nullptr);
        CHECK(program->isValid());
        return blobbyCreate(pl.attributes, pl.xform, program, nullptr, 0, nullptr, nullptr);
    }

    void testUnsupportedKinds(CaptureSink &sink) {
        contract::Placement pl(5.0f);
        Rig rig("al-reports-a");
        rig.add(polygon(pl, {{0, 0}, {1, 0}, {1, 1}, {0, 1}}));
        rig.add(contract::points(pl, 4));
        rig.add(curveMesh(pl));
        CObject *b = blobby(pl);
        CHECK(b != nullptr);
        double blobbyMesh = 0.0;
        if (b != nullptr) {
            const CPolygonMesh *bm = dynamic_cast<const CPolygonMesh *>(b);
            CHECK(bm != nullptr); // blobbyCreate() wraps the polygonized surface in a CPolygonMesh
            if (bm != nullptr)
                blobbyMesh = meshArea(*bm);
            rig.add(b);
        }
        const CEmitterSet &set = rig.set();
        sink.entries.clear();
        set.resolve(rig.context);
        CHECK(set.numUnsupported == 2); // points and curves; the blobby is a polygon mesh from the moment it is created
        // the quad (1) plus the blobby's own polygonized mesh, summed independently from its raw triangles
        CHECK(blobbyMesh > 0.0);
        CHECK(nearRel(set.totalArea(), 1.0 + blobbyMesh, kAreaRelTol));
        CHECK(sink.count("al-reports-a", "cannot emit") == 1);
        CHECK(sink.allAreWarnings("al-reports-a"));
        const size_t reports = sink.entries.size();
        set.resolve(rig.context); // resolution runs once: nothing more to say
        set.resolve(rig.context);
        CHECK(sink.entries.size() == reports);
    }

    void testEmptySet(CaptureSink &sink) {
        Rig rig("al-reports-b");
        const CEmitterSet &set = rig.set();
        sink.entries.clear();
        set.resolve(rig.context);
        CHECK(sink.count("al-reports-b", "no emitting geometry") == 1);
        CHECK(sink.allAreWarnings("al-reports-b"));
        CHECK(set.totalArea() == 0.0);
        CAreaSample s;
        set.sample(0.5, 0.5, s);
        CHECK(s.pdfA == 0.0f && s.leaf == nullptr);
        set.resolve(rig.context);
        CHECK(sink.count("al-reports-b", "no emitting geometry") == 1);
    }

    // Two overlapping closed cubes (side 2) as a union solid, in an in-process Ri session with a real context
    void testCsgLeaves(CaptureSink &sink) {
        csgtest::CCsgSession session;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        const float P[24] = {-1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1};
        int nverts[6] = {4, 4, 4, 4, 4, 4};
        int verts[24] = {0, 3, 2, 1, 4, 5, 6, 7, 0, 1, 5, 4, 2, 3, 7, 6, 1, 2, 6, 5, 0, 4, 7, 3};
        CObject *solid = session.one([&] {
            RiTranslate(0.0f, 0.0f, 5.0f);
            RiSolidBegin(RI_UNION);
            RiSolidBegin(RI_PRIMITIVE);
            RiPointsPolygons(6, nverts, verts, RI_P, const_cast<float *>(P), RI_NULL);
            RiSolidEnd();
            RiSolidBegin(RI_PRIMITIVE);
            RiTranslate(0.5f, 0.0f, 0.0f);
            RiPointsPolygons(6, nverts, verts, RI_P, const_cast<float *>(P), RI_NULL);
            RiSolidEnd();
            RiSolidEnd();
        });
        CHECK(solid != nullptr);
        if (solid != nullptr) {
            Rig rig("al-reports-c");
            rig.add(solid);
            const CEmitterSet &set = rig.set();
            sink.entries.clear();
            set.resolve(*session.ctx());
            CHECK(set.numCsgLeaves == 2);
            CHECK(nearRel(set.totalArea(), 48.0, kAreaRelTol)); // each cube whole: 6 faces of area 4
            CHECK(sink.count("al-reports-c", "CSG") == 1);
            CHECK(sink.count("al-reports-c", "whole leaf") == 1);
            CHECK(sink.allAreWarnings("al-reports-c"));
        }
        installDiagnosticSink(previous);
    }

} // namespace

int main() {
    (void)tests_passed; // csgTestUtils.h's counters, unused here (a header-level static, -Werror)
    (void)tests_failed;
    CRiStandardSession session;
    installFrame();
    CaptureSink sink;
    CDiagnosticSink *previous = installDiagnosticSink(&sink);
    testUnsupportedKinds(sink);
    testEmptySet(sink);
    installDiagnosticSink(previous);
    testCsgLeaves(sink);
    installGeometryFrame(nullptr);
    return CONTRACT_RESULT("AreaSampling_Reports");
}
