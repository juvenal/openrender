/**
 * Project: openRender
 *
 * File: test_frame_default.cpp
 *
 * Description:
 *   Geometry_FrameDefault: with nothing installed geometryFrame() is all zero and raytraced() is false; an installed frame reads back bit for bit; the frame's camera2pixels equals renderer.h's; a frame built after a raytrace hider's constructors carries ATTRIBUTES_FLAGS_PRIMARY_VISIBLE (finding F1). Renderer build. The frame's lens is the openrender::LensModel `lens` (the lens phase replaced the old cocFactorPixels and invFocaldistance fields; tasks T115a).
 *
 *   Written first for spec 031 (tests/geometry_contract/, label geometry_contract); red on the starting tree
 *   because the interface it exercises does not exist yet (contracts/tests.md). It was built only while the
 *   temporary option OPENRENDER_031_RED_TESTS was ON, until its phase enabled it; the option is gone and the test is always built.
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#include <cstring>

#include "contract_test.h"
#include "geometryFrame.h"
#include "object.h"
#include "quadrics.h"
#include "renderer.h"
#include "ri.h"

namespace {

    template <class T> bool sameBits(T a, T b) { return std::memcmp(&a, &b, sizeof(T)) == 0; }

    CGeometryFrame filledFrame() {
        CGeometryFrame f = CGeometryFrame();
        f.numThreads = 7;
        f.maxGridSize = 289;
        f.maxEyeSplits = 13;
        f.flags = OPTIONS_FLAGS_MOTIONBLUR | OPTIONS_FLAGS_FOCALBLUR;
        f.raytracingFlags = ATTRIBUTES_FLAGS_PRIMARY_VISIBLE | ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE;
        f.projection = OPTIONS_PROJECTION_PERSPECTIVE;
        f.imagePlane = 1.3125f;
        f.dPixeldx = 0.0031f;
        f.dPixeldy = 0.0047f;
        f.pixelLeft = -3.5f;
        f.pixelTop = 2.25f;
        f.clipMin = 0.0125f;
        f.lens.projection = OPTIONS_PROJECTION_PERSPECTIVE;
        f.lens.imagePlane = 1.3125f;
        f.lens.aperture = 0.0125f;
        f.lens.focaldistance = 5.0f;
        f.lens.invFocaldistance = 0.2f;
        f.lens.focallength = 0.05f;
        f.lens.fstop = 2.0f;
        return f;
    }

    void testDefault() {
        const CGeometryFrame zero = CGeometryFrame();
        CHECK(std::memcmp(&geometryFrame(), &zero, sizeof(CGeometryFrame)) == 0); // all zero, bit for bit
        CAttributes *attributes = new CAttributes();
        attributes->flags |= ATTRIBUTES_FLAGS_PRIMARY_VISIBLE;
        CXform *xform = new CXform();
        CSphere sphere(attributes, xform, NULL, 0, 1.0f, -1.0f, 1.0f, 6.2831853f);
        CHECK(sphere.raytraced() == 0); // nothing installed: no attribute flag makes an object raytraced
    }

    void testReadBack() {
        const CGeometryFrame f = filledFrame();
        installGeometryFrame(&f);
        const CGeometryFrame &g = geometryFrame();
        CHECK(g.numThreads == f.numThreads && g.maxGridSize == f.maxGridSize && g.maxEyeSplits == f.maxEyeSplits);
        CHECK(g.flags == f.flags && g.raytracingFlags == f.raytracingFlags && g.projection == f.projection);
        CHECK(sameBits(g.imagePlane, f.imagePlane) && sameBits(g.dPixeldx, f.dPixeldx) && sameBits(g.dPixeldy, f.dPixeldy));
        CHECK(sameBits(g.pixelLeft, f.pixelLeft) && sameBits(g.pixelTop, f.pixelTop) && sameBits(g.clipMin, f.clipMin));
        CHECK(g.lens.projection == f.lens.projection && sameBits(g.lens.imagePlane, f.lens.imagePlane) && sameBits(g.lens.aperture, f.lens.aperture));
        CHECK(sameBits(g.lens.focaldistance, f.lens.focaldistance) && sameBits(g.lens.invFocaldistance, f.lens.invFocaldistance));
        CHECK(sameBits(g.lens.focallength, f.lens.focallength) && sameBits(g.lens.fstop, f.lens.fstop));

        CAttributes *attributes = new CAttributes();
        attributes->flags |= ATTRIBUTES_FLAGS_PRIMARY_VISIBLE;
        CXform *xform = new CXform();
        CSphere sphere(attributes, xform, NULL, 0, 1.0f, -1.0f, 1.0f, 6.2831853f);
        CHECK(sphere.raytraced() != 0);
        installGeometryFrame(nullptr);
        CHECK(sphere.raytraced() == 0);
    }

    // the frame's camera2pixels is renderer.h's expression over the same values
    void testCamera2Pixels() {
        const CGeometryFrame f = filledFrame();
        CRenderer::projection = (EProjectionType)f.projection;
        CRenderer::imagePlane = f.imagePlane;
        CRenderer::dPixeldx = f.dPixeldx;
        CRenderer::dPixeldy = f.dPixeldy;
        CRenderer::pixelLeft = f.pixelLeft;
        CRenderer::pixelTop = f.pixelTop;

        const int n = 4;
        const float P[n][3] = {{0.3f, -0.2f, 2.0f}, {-1.1f, 0.7f, 5.5f}, {2.0f, 2.0f, 0.9f}, {0.0f, 0.0f, 1.0f}}; // stride 3, as renderer.h
        float a[n][3], b[n][3];
        std::memcpy(a, P, sizeof(a));
        std::memcpy(b, P, sizeof(b));
        camera2pixels(n, &a[0][0]); // renderer.h
        camera2pixels(f, n, &b[0][0]); // the frame's
        CHECK(std::memcmp(a, b, sizeof(a)) == 0);

        CRenderer::projection = OPTIONS_PROJECTION_ORTHOGRAPHIC;
        CGeometryFrame o = f;
        o.projection = OPTIONS_PROJECTION_ORTHOGRAPHIC;
        std::memcpy(a, P, sizeof(a));
        std::memcpy(b, P, sizeof(b));
        camera2pixels(n, &a[0][0]);
        camera2pixels(o, n, &b[0][0]);
        CHECK(std::memcmp(a, b, sizeof(a)) == 0);
    }

    // Finding F1: the raytrace and photon hiders set ATTRIBUTES_FLAGS_PRIMARY_VISIBLE in CRenderer::raytracingFlags in
    // their constructors, which run in beginFrame() after the frame values are first available. A frame built after the
    // hiders exist must carry the bit. This case installs its own frame, so it cannot see where beginFrame() installs the
    // core's; the install point is guarded by the scratch mutation of tasks T045.
    void testFrameBuiltAfterRaytraceHider() {
        RiBegin(RI_NULL);
        RiHider("raytrace", RI_NULL);
        RiFormat(16, 16, 1);
        RiWorldBegin(); // beginFrame() constructs the hiders
        const unsigned int expected = CRenderer::raytracingFlags;
        CHECK((expected & ATTRIBUTES_FLAGS_PRIMARY_VISIBLE) != 0);

        CGeometryFrame f = filledFrame();
        f.raytracingFlags = CRenderer::raytracingFlags; // the value a frame built now copies
        installGeometryFrame(&f);
        CHECK((geometryFrame().raytracingFlags & ATTRIBUTES_FLAGS_PRIMARY_VISIBLE) != 0);
        CHECK(geometryFrame().raytracingFlags == expected);
        installGeometryFrame(nullptr);

        RiWorldEnd();
        RiEnd();
    }

} // namespace

int main() {
    testDefault();
    testReadBack();
    testCamera2Pixels();
    testFrameBuiltAfterRaytraceHider();
    return CONTRACT_RESULT("Geometry_FrameDefault");
}
