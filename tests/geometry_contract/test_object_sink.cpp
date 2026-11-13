/**
 * Project: openRender
 *
 * File: test_object_sink.cpp
 *
 * Description:
 *   Geometry_ObjectSink: instantiate() of a sphere, a polygon mesh and a CSG solid instance reports exactly the created objects to a recording IObjectSink (renderer build).
 *
 *   Written first for spec 031 (tests/geometry_contract/, label geometry_contract); red on the starting tree
 *   because the interface it exercises does not exist yet (contracts/tests.md). It was built only while the
 *   temporary option OPENRENDER_031_RED_TESTS was ON, until its phase enabled it; the option is gone and the test is always built.
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <algorithm>
#include <vector>

#include "contract_test.h"
#include "csgTestUtils.h" // CCsgSession: RiBegin(RI_NULL) + RiWorldBegin whose context captures primitives
#include "object.h"
#include "objectSink.h"

namespace {

    // Records what instantiate() reports.
    class RecordingSink : public IObjectSink {
        public:
            void addObject(CObject *object) override { objects.push_back(object); }
            std::vector<CObject *> objects;
    };

    void checkReportsExactlyTheCreatedObjects(CObject *original) {
        CHECK(original != NULL);
        if (original == NULL)
            return;
        RecordingSink sink;
        original->instantiate(original->attributes, original->xform, &sink);
        // exactly what was created: at least one object, none of them the original, none twice
        CHECK(!sink.objects.empty());
        for (CObject *created : sink.objects)
            CHECK(created != original);
        std::vector<CObject *> sorted = sink.objects;
        std::sort(sorted.begin(), sorted.end());
        CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
        for (CObject *created : sink.objects)
            created->detach();
    }

} // namespace

int main() {
    (void)tests_passed; // csgTestUtils.h's counters, unused here
    (void)tests_failed;
    {
        csgtest::CCsgSession session;

        CObject *sphere = session.one([&] { RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); });
        checkReportsExactlyTheCreatedObjects(sphere);

        float P[12] = {-1, -1, 5, 1, -1, 5, 1, 1, 5, -1, 1, 5};
        RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
        CObject *mesh = session.one([&] { RiPointsPolygons(1, nverts, verts, RI_P, (RtPointer)P, RI_NULL); });
        checkReportsExactlyTheCreatedObjects(mesh);

        CObject *solid = session.one([&] {
            RiSolidBegin(RI_UNION);
            RiSolidBegin(RI_PRIMITIVE);
            RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
            RiSolidEnd();
            RiSolidBegin(RI_PRIMITIVE);
            RiTranslate(0.5f, 0, 0);
            RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
            RiSolidEnd();
            RiSolidEnd();
        });
        checkReportsExactlyTheCreatedObjects(solid);
    }
    return CONTRACT_RESULT("Geometry_ObjectSink");
}
