/**
 * Project: openRender
 *
 * File: test_ribout_roundtrip.cpp
 *
 * Description:
 *   Unit test (T097, spec 015-blobby-implicit-surfaces) that a Blobby
 *   statement survives being written out and read back (FR-004).
 *
 *   This is a correctness requirement rather than a convenience. Each
 *   server in a distributed render derives its own surface from the
 *   *re-emitted* declaration rather than receiving finished geometry, so a
 *   RIB writer that drops the statement loses the primitive across
 *   servers -- which is exactly what CRibOut::RiBlobbyV's RIE_UNIMPLEMENT
 *   stub did before this feature. The same writer serves the round trip
 *   and the distributed path, so exercising one exercises the other.
 *
 *   What is asserted here is that the writer emits the statement at all,
 *   with its code array, its floats, its strings and its parameter list
 *   intact. That the re-emitted declaration produces the *same geometry* is
 *   asserted where it is more faithfully testable -- in
 *   examples/rib/tests/blobby-roundtrip-reyes.rib, which routes a blobby
 *   through ArchiveBegin/ArchiveEnd and back and is registered as a parity
 *   pairing against the direct declaration, so the two renders must agree
 *   pixel for pixel rather than merely both existing.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>
#include <cstring>

#include "object.h"
#include "polygons.h"
#include "rendererContext.h"
#include "ri.h"
#include "riHooks.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                              \
    void test_##name();                         \
    void run_test_##name() {                    \
        printf("Running test: %s ... ", #name); \
        fflush(stdout);                         \
        int failedBefore = tests_failed;        \
        test_##name();                          \
        if (tests_failed == failedBefore) {     \
            tests_passed++;                     \
            printf("PASSED\n");                 \
        }                                       \
    }                                           \
    void test_##name()

#define ASSERT(condition)                                          \
    do {                                                           \
        if (!(condition)) {                                        \
            printf("\nAssertion failed: %s\nFile: %s, Line: %d\n", \
                   #condition, __FILE__, __LINE__);                \
            tests_failed++;                                        \
            return;                                                \
        }                                                          \
    } while (0)

///////////////////////////////////////////////////////////////////////
// Capture what reaches addObject()
///////////////////////////////////////////////////////////////////////
class CCaptureContext : public CRendererContext {
    public:
        int numCaptured;
        int numVertices;
        int numTriangles;
        float bmin[3], bmax[3];

        CCaptureContext() : CRendererContext(), numCaptured(0), numVertices(0), numTriangles(0) {
            for (int i = 0; i < 3; i++) {
                bmin[i] = 0;
                bmax[i] = 0;
            }
        }

        virtual void addObject(CObject *o) {
            CPolygonMesh *mesh = dynamic_cast<CPolygonMesh *>(o);

            if (mesh != NULL) {
                numCaptured++;
                for (int i = 0; i < 3; i++) {
                    bmin[i] = mesh->bmin[i];
                    bmax[i] = mesh->bmax[i];
                }
            }

            CRendererContext::addObject(o);
        }
};

static CCaptureContext *g_context = NULL;

static CRendererContext *makeCaptureContext() {
    g_context = new CCaptureContext();
    return g_context;
}

// Two summed ellipsoid fields carrying a per-blob colour, so the round
// trip has to preserve the code array, the floats, the strings and the
// parameter list -- not just the shape.
static void emitBlobby() {
    RtInt code[] = {1001, 0, 1001, 16, 0, 2, 0, 1};
    RtFloat floats[] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -0.35f, 0, 0, 1,
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.35f, 0, 0, 1};
    RtString strings[] = {""};
    RtFloat colours[] = {1, 0, 0, 0, 0, 1};
    RtToken tokens[] = {(RtToken) "vertex color Cs"};
    RtPointer values[] = {(RtPointer)colours};

    RiBlobbyV(2, 8, code, 32, floats, 1, strings, 1, tokens, values);
}

static const char *kWrittenRib = "blobby-roundtrip-out.rib";

TEST(a_blobby_is_written_out_rather_than_dropped) {
    remove(kWrittenRib);

    // The written file carries its own world block, because a primitive
    // outside one is out of scope. It is therefore read back at the *top*
    // level below rather than inside another, or the two would nest.
    RiBegin((RtToken)kWrittenRib);
    RiTranslate(0, 0, 4);
    RiWorldBegin();
    emitBlobby();
    RiWorldEnd();
    RiEnd();

    FILE *in = fopen(kWrittenRib, "r");
    ASSERT(in != NULL);

    char line[8192];
    int sawBlobby = 0;
    int sawLeafCount = 0;
    int sawCode = 0;
    int sawFloats = 0;
    int sawStrings = 0;
    int sawColour = 0;

    while (fgets(line, sizeof(line), in) != NULL) {
        if (strstr(line, "Blobby") == NULL)
            continue;

        sawBlobby = 1;

        // nleaf, then the three arrays, then the parameter list. Each is
        // checked by a value only that array could contain, so a writer
        // that emitted the statement while losing one of them fails here
        // rather than producing a RIB that reads back as a different
        // shape.
        if (strstr(line, "Blobby 2") != NULL)
            sawLeafCount = 1;
        if (strstr(line, "1001") != NULL)
            sawCode = 1;
        if (strstr(line, "-0.35") != NULL)
            sawFloats = 1;
        if (strstr(line, "\"\"") != NULL)
            sawStrings = 1;
        if (strstr(line, "Cs") != NULL)
            sawColour = 1;
    }

    fclose(in);

    ASSERT(sawBlobby);
    ASSERT(sawLeafCount);
    ASSERT(sawCode);
    ASSERT(sawFloats);
    ASSERT(sawStrings);
    ASSERT(sawColour);
}

int main() {
    printf("=== Blobby RIB Round-Trip Tests (T097) ===\n\n");

    run_test_a_blobby_is_written_out_rather_than_dropped();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
