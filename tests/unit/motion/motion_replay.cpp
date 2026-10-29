/**
 * Project: openRender
 *
 * File: motion_replay.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), tasks T006 to T013: the intersection and sample
 *   replay, a CHARACTERIZATION tool.
 *
 *   --record <file>   run every scene and write the results (build tree only; the tracked
 *                     replay file is copied from it on the starting tree, once)
 *   --check <file>    run the scenes and compare their output with <file>, text for text
 *   --family <f>      with --check, run and compare only the scenes of that family
 *
 *   Each scene is an in-process Ri session (replay_capture.h). For every leaf the tool
 *   fires a fixed grid of rays at seven times (I records) and calls sample() on a fixed
 *   (u, v) grid at the same times, with PARAMETER_BEGIN_SAMPLE and with
 *   PARAMETER_END_SAMPLE (S records). Every float is written as a C99 hex float (%a), so a
 *   check is bitwise.
 *
 *   The replay file is recorded on the starting tree (master 4eb683b), where it is green by
 *   construction; its red evidence is one mutation per family in a scratch copy (spec 025's
 *   T-P1a precedent). It uses starting-tree interfaces only (the Ri API,
 *   CObject::intersect(), CObject::sample(), CObject::materialize()) and names no 028 type.
 *
 *   Format (contracts/replay-format.md):
 *     # motion_replay v1 <platform> master-4eb683b
 *     <scene> <family> I <leaf> <ray> <time> <hit> [<t> <u> <v> <Nx> <Ny> <Nz>]
 *     <scene> <family> S <leaf> <uv> <time|BEGIN|END> <Px> <Py> <Pz> <Ngx> ... <dPdtimez>
 *   A miss (hit 0) carries no further field. Records appear in scene order (the order of
 *   kScenes), then kind (I before S), leaf, ray or (u, v) index, and time in the fixed order
 *   below.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <typeinfo>
#include <vector>

#include "object.h"
#include "renderer.h"
#include "replay_capture.h"
#include "ri.h"

#include "curves.h"
#include "points.h"

// CCurveMesh::create() is private and is reached in production only through CCurveMesh::dice(), which
// needs a Reyes bucket. The curve leaves (CCubicCurve, CLinearCurve) are what the replay samples, so the
// tool names that one member through an explicit instantiation (the access check does not apply to the
// arguments of an explicit instantiation) and calls it directly.
struct CurveMeshCreateTag {
        typedef void (CCurveMesh::*type)(CShadingContext *);
        friend type curveMeshCreate(CurveMeshCreateTag);
};
template <CurveMeshCreateTag::type M>
struct CurveMeshCreateRob {
        friend CurveMeshCreateTag::type curveMeshCreate(CurveMeshCreateTag) { return M; }
};
template struct CurveMeshCreateRob<&CCurveMesh::create>;

// The same for CPoints::prep(), which the root CPoints needs before sample() has points to sample
// (production calls it from CPoints::dice(); the root's points array is NULL until then).
struct PointsPrepTag {
        typedef void (CPoints::*type)();
        friend type pointsPrep(PointsPrepTag);
};
template <PointsPrepTag::type M>
struct PointsPrepRob {
        friend PointsPrepTag::type pointsPrep(PointsPrepTag) { return M; }
};
template struct PointsPrepRob<&CPoints::prep>;

#ifndef MOTION_REPLAY_PLATFORM
#error "MOTION_REPLAY_PLATFORM must be defined by the build (macos or linux)"
#endif

using namespace motionreplay;

namespace {

    const char *const kTree = "master-4eb683b";

    // The times every query is made at. The outer two lie outside the shutter: the evaluator
    // does not clamp (the starting tree extrapolates linearly).
    const double kTimes[7] = {-0.25, 0.0, 0.25, 0.5, 0.75, 1.0, 1.25};

    struct SceneDef {
            const char *id;
            const char *family;
            float shutterOpen, shutterClose;
            int camera;
            bool intersect; // Write I records
            bool sample;    // Write S records
            int uvGrid;     // S records: uvGrid x uvGrid samples per configuration
            // Coverage guards: what the scene must actually produce, or the tool (not the code under
            // test) is wrong and the replay would test nothing
            const char *requiredClass; // A substring of some leaf's class name
            const char *excludeClass;  // Leaves whose class name contains this are not queried (they belong to another family's replay), or NULL
            bool expectMoving;         // Some leaf is moving() (deforming)
            bool expectXformMotion;    // Some leaf's xform has a "next"
            void (*build)(CReplaySession &);
    };

    std::string hx(double v) {
        char b[64];
        snprintf(b, sizeof(b), "%a", v);
        return std::string(b);
    }

    // ------------------------------------------------------------------------------------
    // The seven quadrics, two parameter sets each. The values are not dyadic on purpose: at
    // t = 0.75 and t = 1.25 the float product of a parameter and t rounds, so a lerp that drops
    // its double evaluation changes bits.
    // ------------------------------------------------------------------------------------
    enum { Q_SPHERE,
           Q_CONE,
           Q_CYLINDER,
           Q_DISK,
           Q_PARABOLOID,
           Q_HYPERBOLOID,
           Q_TORUS,
           Q_COUNT };

    void emitQuadric(int q, int k) {
        switch (q) {
            case Q_SPHERE:
            {
                static const float p[2][4] = {{0.9f, -0.7f, 0.8f, 300.0f}, {1.35f, -0.95f, 0.85f, 340.0f}};
                RiSphere(p[k][0], p[k][1], p[k][2], p[k][3], RI_NULL);
                break;
            }
            case Q_CONE:
            {
                static const float p[2][3] = {{1.2f, 0.8f, 320.0f}, {1.7f, 1.15f, 355.0f}};
                RiCone(p[k][0], p[k][1], p[k][2], RI_NULL);
                break;
            }
            case Q_CYLINDER:
            {
                static const float p[2][4] = {{0.7f, -0.6f, 0.9f, 310.0f}, {1.05f, -0.8f, 1.25f, 350.0f}};
                RiCylinder(p[k][0], p[k][1], p[k][2], p[k][3], RI_NULL);
                break;
            }
            case Q_DISK:
            {
                static const float p[2][3] = {{0.3f, 1.0f, 300.0f}, {0.55f, 1.45f, 345.0f}};
                RiDisk(p[k][0], p[k][1], p[k][2], RI_NULL);
                break;
            }
            case Q_PARABOLOID:
            {
                static const float p[2][4] = {{0.9f, 0.2f, 0.9f, 305.0f}, {1.3f, 0.35f, 1.2f, 340.0f}};
                RiParaboloid(p[k][0], p[k][1], p[k][2], p[k][3], RI_NULL);
                break;
            }
            case Q_HYPERBOLOID:
            {
                static const float p[2][7] = {{0.5f, 0.0f, -0.6f, 0.9f, 0.3f, 0.7f, 300.0f}, {0.7f, 0.0f, -0.8f, 1.2f, 0.4f, 0.9f, 345.0f}};
                float p1[3] = {p[k][0], p[k][1], p[k][2]};
                float p2[3] = {p[k][3], p[k][4], p[k][5]};
                RiHyperboloid(p1, p2, p[k][6], RI_NULL);
                break;
            }
            case Q_TORUS:
            {
                static const float p[2][5] = {{1.0f, 0.35f, 0.0f, 330.0f, 310.0f}, {1.35f, 0.5f, 20.0f, 340.0f, 350.0f}};
                RiTorus(p[k][0], p[k][1], p[k][2], p[k][3], p[k][4], RI_NULL);
                break;
            }
            default:
                break;
        }
    }

    // ------------------------------------------------------------------------------------
    // Scenes
    // ------------------------------------------------------------------------------------

    // quadrics-deform: every quadric deforms between two parameter sets over the shutter 0 0.5
    void buildQuadricsDeform(CReplaySession &s) {
        for (int q = 0; q < Q_COUNT; ++q) {
            s.add([&] {
                RiTranslate(-4.5f + 1.5f * q, 0.1f * q, 5.0f);
                RiMotionBegin(2, 0.0f, 0.5f);
                emitQuadric(q, 0);
                emitQuadric(q, 1);
                RiMotionEnd();
            });
        }
    }

    // quadrics-xform: every quadric under a translating and rotating transform, shutter 0.25 0.75
    void buildQuadricsXform(CReplaySession &s) {
        for (int q = 0; q < Q_COUNT; ++q) {
            s.add([&] {
                const float x0 = -4.5f + 1.5f * q;
                RiMotionBegin(2, 0.25f, 0.75f);
                RiTranslate(x0, 0.0f, 5.0f);
                RiTranslate(x0 + 0.8f, 0.35f, 5.3f);
                RiMotionEnd();
                RiMotionBegin(2, 0.25f, 0.75f);
                RiRotate(0.0f, 0.3f, 1.0f, 0.2f);
                RiRotate(28.0f, 0.3f, 1.0f, 0.2f);
                RiMotionEnd();
                emitQuadric(q, 0);
            });
        }
    }

    // Static quadrics: their motion is the camera's alone (synthesized by addObject)
    void buildQuadricsStatic(CReplaySession &s) {
        s.add([&] {
            RiTranslate(-1.2f, 0.0f, 5.0f);
            emitQuadric(Q_SPHERE, 0);
        });
        s.add([&] {
            RiTranslate(1.4f, 0.2f, 5.5f);
            emitQuadric(Q_CYLINDER, 0);
        });
    }

    // GitHub #106, pinned: a quadric with its OWN transform motion under a moving camera gets no
    // camera motion blur. A second, static quadric beside it gets the camera motion.
    void buildQuadricsOwnMotionCamera(CReplaySession &s) {
        s.add([&] {
            RiMotionBegin(2, 0.0f, 1.0f);
            RiTranslate(-1.0f, 0.0f, 5.0f);
            RiTranslate(-0.4f, 0.5f, 5.3f);
            RiMotionEnd();
            emitQuadric(Q_SPHERE, 0);
        });
        s.add([&] {
            RiTranslate(1.5f, 0.0f, 5.0f);
            emitQuadric(Q_CYLINDER, 0);
        });
    }

    void buildBilinear(CReplaySession &s) {
        static const float P[2][12] = {{-1.0f, -1.0f, 5.0f, 1.0f, -0.9f, 5.2f, -1.1f, 1.0f, 5.1f, 0.9f, 1.1f, 5.6f},
                                       {-0.7f, -1.2f, 5.4f, 1.3f, -0.8f, 5.0f, -0.9f, 1.3f, 5.7f, 1.2f, 0.9f, 5.2f}};
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiPatch(RI_BILINEAR, RI_P, P[0], RI_NULL);
            RiPatch(RI_BILINEAR, RI_P, P[1], RI_NULL);
            RiMotionEnd();
        });
    }

    void bicubicPoints(float *P, int k) {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                float *p = P + (i * 4 + j) * 3;
                p[0] = (i - 1.5f) * 0.9f * (k ? 1.1f : 1.0f) + (k ? 0.2f : 0.0f);
                p[1] = (j - 1.5f) * 0.9f * (k ? 0.9f : 1.0f);
                p[2] = 5.0f + 0.25f * ((i * 3 + j * 5) % 4) + (k ? 0.15f * (i - j) : 0.0f);
            }
    }

    void buildBicubic(CReplaySession &s) {
        float P0[48], P1[48];
        bicubicPoints(P0, 0);
        bicubicPoints(P1, 1);
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiPatch(RI_BICUBIC, RI_P, P0, RI_NULL);
            RiPatch(RI_BICUBIC, RI_P, P1, RI_NULL);
            RiMotionEnd();
        });
    }

    void nurbsPoints(float *P, int k) {
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j < 5; ++j) {
                float *p = P + (i * 5 + j) * 3;
                p[0] = (i - 2.0f) * 0.8f * (k ? 1.15f : 1.0f);
                p[1] = (j - 2.0f) * 0.8f + (k ? 0.25f : 0.0f);
                p[2] = 5.0f + 0.3f * ((i * 2 + j * 3) % 3) + (k ? 0.2f * (j - i) * 0.5f : 0.0f);
            }
    }

    void buildNurbs(CReplaySession &s) {
        float P0[75], P1[75];
        nurbsPoints(P0, 0);
        nurbsPoints(P1, 1);
        float knots[9] = {0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 1.0f, 1.0f, 1.0f, 1.0f};
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiNuPatch(5, 4, knots, 0.0f, 1.0f, 5, 4, knots, 0.0f, 1.0f, RI_P, P0, RI_NULL);
            RiNuPatch(5, 4, knots, 0.0f, 1.0f, 5, 4, knots, 0.0f, 1.0f, RI_P, P1, RI_NULL);
            RiMotionEnd();
        });
    }

    // A moving triangle (vertices 0 1 2) and a moving quad (3 4 5 6)
    void buildPolygons(CReplaySession &s) {
        static const float P[2][21] = {
            {-2.0f, -1.0f, 5.0f, -0.4f, -1.0f, 5.2f, -1.2f, 0.9f, 5.1f, 0.4f, -1.0f, 5.0f, 2.1f, -0.9f, 5.3f, 2.0f, 1.0f, 5.6f, 0.5f, 0.8f, 5.2f},
            {-1.6f, -1.2f, 5.3f, 0.1f, -0.8f, 5.0f, -1.0f, 1.2f, 5.5f, 0.8f, -1.2f, 5.4f, 2.5f, -0.6f, 5.0f, 2.3f, 1.3f, 5.2f, 0.7f, 1.0f, 5.7f}};
        int nverts[2] = {3, 4};
        int verts[7] = {0, 1, 2, 3, 4, 5, 6};
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiPointsPolygons(2, nverts, verts, RI_P, P[0], RI_NULL);
            RiPointsPolygons(2, nverts, verts, RI_P, P[1], RI_NULL);
            RiMotionEnd();
        });
    }

    void buildImplicit(CReplaySession &s) {
        s.add([&] {
            const char *path = MOTION_REPLAY_IMPLICIT_PLUGIN;
            RtToken tokens[1] = {"file"};
            RtPointer params[1] = {(RtPointer)&path};
            RiGeometryV("implicit", 1, tokens, params);
        });
    }

    void buildDlo(CReplaySession &s) {
        s.add([&] {
            RiTranslate(0.0f, 0.0f, 5.0f);
            const char *path = MOTION_REPLAY_DLO_PLUGIN;
            RtToken tokens[1] = {"file"};
            RtPointer params[1] = {(RtPointer)&path};
            RiGeometryV("dlo", 1, tokens, params);
        });
    }

    void buildCurves(CReplaySession &s) {
        static const float C[2][36] = {
            {-1.5f, -1.0f, 5.0f, -0.9f, 0.4f, 5.1f, -0.3f, -0.6f, 5.3f, 0.3f, 0.7f, 5.0f, 0.9f, -0.5f, 5.2f, 1.5f, 0.6f, 5.4f,
             -1.4f, 1.0f, 5.5f, -0.8f, 1.6f, 5.0f, -0.2f, 0.9f, 5.2f, 0.4f, 1.8f, 5.3f, 1.0f, 1.1f, 5.1f, 1.6f, 1.9f, 5.6f},
            {-1.2f, -1.3f, 5.3f, -0.7f, 0.1f, 5.0f, -0.1f, -0.9f, 5.5f, 0.5f, 0.4f, 5.2f, 1.1f, -0.8f, 5.0f, 1.7f, 0.3f, 5.7f,
             -1.1f, 0.7f, 5.2f, -0.5f, 1.9f, 5.4f, 0.1f, 0.6f, 5.0f, 0.7f, 2.1f, 5.1f, 1.3f, 0.8f, 5.6f, 1.9f, 2.2f, 5.2f}};
        static const float W = 0.12f;
        int nv[2] = {6, 6};
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiCurves(RI_CUBIC, 2, nv, RI_NONPERIODIC, RI_P, C[0], RI_CONSTANTWIDTH, &W, RI_NULL);
            RiCurves(RI_CUBIC, 2, nv, RI_NONPERIODIC, RI_P, C[1], RI_CONSTANTWIDTH, &W, RI_NULL);
            RiMotionEnd();
        });
        int nl[2] = {3, 3};
        static const float L[2][18] = {
            {-1.0f, -2.0f, 5.0f, 0.0f, -1.5f, 5.3f, 1.0f, -2.1f, 5.1f, -1.0f, -2.8f, 5.2f, 0.0f, -2.4f, 5.0f, 1.0f, -2.9f, 5.4f},
            {-0.8f, -2.2f, 5.4f, 0.2f, -1.3f, 5.0f, 1.2f, -2.4f, 5.5f, -0.9f, -3.0f, 5.0f, 0.3f, -2.2f, 5.3f, 1.1f, -3.1f, 5.1f}};
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiCurves(RI_LINEAR, 2, nl, RI_NONPERIODIC, RI_P, L[0], RI_CONSTANTWIDTH, &W, RI_NULL);
            RiCurves(RI_LINEAR, 2, nl, RI_NONPERIODIC, RI_P, L[1], RI_CONSTANTWIDTH, &W, RI_NULL);
            RiMotionEnd();
        });
    }

    void buildPoints(CReplaySession &s) {
        float P[2][27];
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < 9; ++i) {
                P[k][i * 3 + 0] = (i % 3 - 1.0f) * 0.9f + (k ? 0.3f : 0.0f) + 0.05f * i;
                P[k][i * 3 + 1] = (i / 3 - 1.0f) * 0.9f + (k ? -0.2f : 0.0f);
                P[k][i * 3 + 2] = 5.0f + 0.1f * (i % 4) + (k ? 0.25f : 0.0f);
            }
        static const float W = 0.2f;
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiPoints(9, RI_P, P[0], RI_CONSTANTWIDTH, &W, RI_NULL);
            RiPoints(9, RI_P, P[1], RI_CONSTANTWIDTH, &W, RI_NULL);
            RiMotionEnd();
        });
    }

    // A Catmull-Clark mesh. kind 0: an open 4 x 4 face grid with a bump (regular interior faces and
    // boundary faces); kind 1: a closed cube (extraordinary vertices).
    void subdivGridPoints(float *P, int k) {
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j < 5; ++j) {
                float *p = P + (i * 5 + j) * 3;
                p[0] = (i - 2.0f) * 0.9f + (k ? 0.15f * j : 0.0f);
                p[1] = (j - 2.0f) * 0.9f;
                p[2] = 5.0f + 0.35f * ((i * 3 + j * 2) % 4) + (k ? 0.3f : 0.0f);
            }
    }

    void buildSubdivGrid(CReplaySession &s) {
        float P[2][75];
        subdivGridPoints(P[0], 0);
        subdivGridPoints(P[1], 1);
        int nverts[16];
        int verts[64];
        for (int f = 0; f < 16; ++f) {
            const int i = f % 4, j = f / 4;
            nverts[f] = 4;
            verts[f * 4 + 0] = j * 5 + i;
            verts[f * 4 + 1] = j * 5 + i + 1;
            verts[f * 4 + 2] = (j + 1) * 5 + i + 1;
            verts[f * 4 + 3] = (j + 1) * 5 + i;
        }
        // "interpolateboundary": without it the faces next to the open border are skipped, and only
        // the regular interior faces (bicubic patches) remain; with it the border faces become CPatchGrid
        RtToken tags[1] = {(RtToken) "interpolateboundary"};
        int nargs[2] = {0, 0};
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiSubdivisionMesh("catmull-clark", 16, nverts, verts, 1, tags, nargs, NULL, NULL, RI_P, P[0], RI_NULL);
            RiSubdivisionMesh("catmull-clark", 16, nverts, verts, 1, tags, nargs, NULL, NULL, RI_P, P[1], RI_NULL);
            RiMotionEnd();
        });
    }

    void buildSubdivCube(CReplaySession &s) {
        float P[2][24];
        for (int k = 0; k < 2; ++k)
            for (int v = 0; v < 8; ++v) {
                P[k][v * 3 + 0] = ((v & 1) ? 1.0f : -1.0f) * (k ? 1.2f : 1.0f);
                P[k][v * 3 + 1] = ((v & 2) ? 1.0f : -1.0f) * (k ? 0.9f : 1.0f) + (k ? 0.2f : 0.0f);
                P[k][v * 3 + 2] = 5.0f + ((v & 4) ? 1.0f : -1.0f) * (k ? 1.1f : 1.0f);
            }
        int nverts[6] = {4, 4, 4, 4, 4, 4};
        int verts[24] = {0, 2, 3, 1, 4, 5, 7, 6, 0, 1, 5, 4, 2, 6, 7, 3, 0, 4, 6, 2, 1, 3, 7, 5};
        // A creased edge (vertices 0 and 1) makes the faces around it "funny", so they are built from
        // B-spline patch grids beside the CSubdivision patches of the extraordinary corners
        RtToken tags[1] = {(RtToken) "crease"};
        int nargs[2] = {2, 1};
        int intargs[2] = {0, 1};
        float floatargs[1] = {2.5f};
        s.add([&] {
            RiMotionBegin(2, 0.0f, 0.5f);
            RiSubdivisionMesh("catmull-clark", 6, nverts, verts, 1, tags, nargs, intargs, floatargs, RI_P, P[0], RI_NULL);
            RiSubdivisionMesh("catmull-clark", 6, nverts, verts, 1, tags, nargs, intargs, floatargs, RI_P, P[1], RI_NULL);
            RiMotionEnd();
        });
    }

    // Shutter 0.5 0.5: invShutterTime is 1 and the motion sampling flag is cleared. One sphere
    // deforms, one has a transform motion; both samples collapse onto the shutter's single instant.
    void buildEmptyShutter(CReplaySession &s) {
        s.add([&] {
            RiTranslate(-1.2f, 0.0f, 5.0f);
            RiMotionBegin(2, 0.0f, 1.0f);
            emitQuadric(Q_SPHERE, 0);
            emitQuadric(Q_SPHERE, 1);
            RiMotionEnd();
        });
        s.add([&] {
            RiMotionBegin(2, 0.0f, 1.0f);
            RiTranslate(1.0f, 0.0f, 5.0f);
            RiTranslate(1.6f, 0.5f, 5.4f);
            RiMotionEnd();
            emitQuadric(Q_SPHERE, 0);
        });
    }

    // The scenes, in the order their records appear in the file. A family's scenes are
    // exactly the ones whose code path its Motion_Replay_<family> test guards.
    // clang-format off
    const SceneDef kScenes[] = {
        // id                          family          open  close cam               I      S      uv  class            exclude         moving xform  build
        {"quadrics-deform",            "quadrics",     0.0f, 0.5f, CAMERA_STATIC,    true,  true,  3, "CSphere", NULL,        true,  false, buildQuadricsDeform},
        {"quadrics-xform",             "quadrics",     0.25f, 0.75f, CAMERA_STATIC,  true,  true,  3, "CSphere", NULL,        false, true,  buildQuadricsXform},
        {"quadrics-camera-translate",  "raytransform", 0.0f, 1.0f, CAMERA_TRANSLATE, true,  true,  3, "CSphere", NULL,        false, true,  buildQuadricsStatic},
        {"quadrics-camera-rotate",     "raytransform", 0.0f, 1.0f, CAMERA_ROTATE,    true,  true,  3, "CSphere", NULL,        false, true,  buildQuadricsStatic},
        {"quadrics-camera-rotonly",    "raytransform", 0.0f, 1.0f, CAMERA_ROTONLY,   true,  true,  3, "CSphere", NULL,        false, true,  buildQuadricsStatic},
        {"quadrics-own-motion-camera", "raytransform", 0.0f, 1.0f, CAMERA_ROTATE,    true,  true,  3, "CSphere", NULL,        false, true,  buildQuadricsOwnMotionCamera},
        {"dlo-camera-translate",       "raytransform", 0.0f, 1.0f, CAMERA_TRANSLATE, true,  false, 0, "CDLObject", NULL,      false, true,  buildDlo},
        {"dlo-camera-rotate",          "raytransform", 0.0f, 1.0f, CAMERA_ROTATE,    true,  false, 0, "CDLObject", NULL,      false, true,  buildDlo},
        {"bilinear",                   "bilinear",     0.0f, 0.5f, CAMERA_STATIC,    true,  true,  3, "CBilinearPatch", NULL, true,  false, buildBilinear},
        {"polygons",                   "polygons",     0.0f, 0.5f, CAMERA_STATIC,    true,  true,  3, "CPolygonTriangle", NULL, true, false, buildPolygons},
        {"implicit",                   "implicit",     0.0f, 1.0f, CAMERA_STATIC,    true,  false, 0, "CImplicit", NULL,      false, false, buildImplicit},
        {"bicubic",                    "tessellation", 0.0f, 0.5f, CAMERA_STATIC,    true,  true,  3, "CBicubicPatch", NULL,  true,  false, buildBicubic},
        {"nurbs",                      "tessellation", 0.0f, 0.5f, CAMERA_STATIC,    true,  true,  3, "CNURBS", NULL,         true,  false, buildNurbs},
        {"curves",                     "gridonly",     0.0f, 0.5f, CAMERA_STATIC,    false, true,  3, "Curve", NULL,          true,  false, buildCurves},
        {"points",                     "gridonly",     0.0f, 0.5f, CAMERA_STATIC,    false, true,  3, "CPoints", NULL,        true,  false, buildPoints},
        {"subdiv-grid",                "gridonly",     0.0f, 0.5f, CAMERA_STATIC,    false, true,  2, "PatchGrid", "CBicubicPatch",      true,  false, buildSubdivGrid},
        {"subdiv-cube",                "gridonly",     0.0f, 0.5f, CAMERA_STATIC,    false, true,  2, "BSplinePatchGrid", NULL,   true,  false, buildSubdivCube},
        {"empty-shutter",              "emptyshutter", 0.5f, 0.5f, CAMERA_STATIC,    true,  true,  3, "CSphere", NULL,        true,  true,  buildEmptyShutter},
    };
    // clang-format on

    const char *const kFamilies[] = {"quadrics", "raytransform", "bilinear", "polygons", "implicit", "tessellation", "gridonly", "emptyshutter"};

    // ------------------------------------------------------------------------------------
    // Queries
    // ------------------------------------------------------------------------------------

    // Intersect `start` and everything below it, the way CShadingContext::trace() walks the
    // hierarchy: non-dummy objects are intersected, lazily built children are clustered and
    // visited, and a subtree whose box the ray no longer reaches is pruned. A CSurface with no
    // children only builds its tessellation on the first call and finds the hit in the child.
    void traceFrom(CShadingContext *ctx, CObject *start, CRay *ray) {
        std::vector<CObject *> stack;
        stack.push_back(start);
        long visits = 0;
        while (!stack.empty()) {
            CObject *o = stack.back();
            stack.pop_back();

            // A sane hierarchy under one leaf is tiny; a runaway means the tool is walking it wrongly
            if (++visits > 3000000) {
                fprintf(stderr, "motion_replay: traceFrom visited %ld objects under a %s (flags %d, %zu on the stack); giving up\n", visits, typeid(*start).name(),
                        start->flags, stack.size());
                fflush(stderr);
                abort();
            }

            const float tmin = nearestBox(o->bmin, o->bmax, ray->from, ray->invDir, ray->tmin, ray->t);
            if (!(tmin < ray->t))
                continue;

            if ((o->flags & OBJECT_DUMMY) == 0)
                o->intersect(ctx, ray);

            if ((o->flags & OBJECT_HIERARCHY_READY) == 0) {
                osLock(CRenderer::hierarchyMutex);
                if ((o->flags & OBJECT_HIERARCHY_READY) == 0) {
                    o->cluster(ctx);
                    o->flags |= OBJECT_HIERARCHY_READY;
                }
                osUnlock(CRenderer::hierarchyMutex);
            }

            std::vector<CObject *> kids;
            for (CObject *c = o->children; c != NULL; c = c->sibling)
                kids.push_back(c);
            for (size_t k = kids.size(); k > 0; --k)
                stack.push_back(kids[k - 1]);
        }
    }

    // 32 camera-space rays aimed at the leaf's bound: an 8 x 4 lattice in front of it with the
    // directions drawn toward the bound's centre (rows 0-2) or fanned past it (row 3), the
    // last two lattice slots replaced by rays that graze its sides. Plain double arithmetic,
    // so the grid does not depend on the code under test.
    const int kRaysPerLeaf = 32;

    void makeRay(const CObject *leaf, int r, double *o, double *d) {
        double c[3], e[3];
        for (int i = 0; i < 3; ++i) {
            c[i] = 0.5 * ((double)leaf->bmin[i] + (double)leaf->bmax[i]);
            e[i] = std::max((double)leaf->bmax[i] - (double)leaf->bmin[i], 1e-3);
        }
        const double pad = 0.25;
        if (r >= kRaysPerLeaf - 2) {
            const int k = r - (kRaysPerLeaf - 2);
            o[0] = (double)leaf->bmin[0] - pad * e[0] - 0.5;
            o[1] = c[1] + (k - 0.5) * 0.6 * e[1];
            o[2] = c[2] + ((k & 1) ? 0.1 : -0.1) * e[2];
            d[0] = 1.0;
            d[1] = 0.02 * (k - 0.5);
            d[2] = ((k & 1) ? 0.03 : -0.03);
        }
        else {
            const int i = r % 8, j = r / 8;
            o[0] = (double)leaf->bmin[0] - pad * e[0] + (i + 0.5) / 8.0 * (1.0 + 2.0 * pad) * e[0];
            o[1] = (double)leaf->bmin[1] - pad * e[1] + (j + 0.5) / 4.0 * (1.0 + 2.0 * pad) * e[1];
            o[2] = (double)leaf->bmin[2] - 0.5 * e[2] - 1.0;
            const double a = (j >= 3) ? 1.6 : 0.8;
            d[0] = c[0] + (o[0] - c[0]) * a - o[0];
            d[1] = c[1] + (o[1] - c[1]) * a - o[1];
            d[2] = c[2] - o[2];
        }
        const double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        for (int i = 0; i < 3; ++i)
            d[i] /= len;
    }

    void queryIntersections(CShadingContext *ctx, const SceneDef &sc, CObject *leaf, int leafIndex, std::vector<std::string> &out) {
        for (int r = 0; r < kRaysPerLeaf; ++r) {
            double o[3], d[3];
            makeRay(leaf, r, o, d);
            for (int ti = 0; ti < 7; ++ti) {
                CRay ray; // Every input field is set below; the output fields start at zero
                for (int i = 0; i < 3; ++i) {
                    ray.from[i] = (float)o[i];
                    ray.dir[i] = (float)d[i];
                    ray.invDir[i] = 1.0 / (double)ray.dir[i];
                    ray.N[i] = 0.0f;
                }
                ray.u = ray.v = 0.0f;
                ray.time = (float)kTimes[ti];
                ray.flags = ATTRIBUTES_FLAGS_PRIMARY_VISIBLE;
                ray.t = C_INFINITY;
                ray.tmin = 0.0f;
                // The ray differential selects the tessellation level of a CSurface (a footprint of zero
                // asks for the finest level, a very deep tree). An orthographic footprint, from fine to
                // wider than the primitive, cycling with the ray so that every tessellation density of
                // CTesselationPatch::intersect (1x1, 2x2 and 4x4 grids, subsampled and not, 8x8 and 16x16)
                // is reached by some ray.
                static const float kFootprint[4] = {0.05f, 0.4f, 1.6f, 6.4f};
                ray.da = 0.0f;
                ray.db = kFootprint[r % 4];
                ray.jimp = 0.0f;
                ray.object = NULL;
                ray.child = NULL;

                traceFrom(ctx, leaf, &ray);

                std::string line = std::string(sc.id) + " " + sc.family + " I " + std::to_string(leafIndex) + " " + std::to_string(r) + " " + hx(kTimes[ti]);
                if (ray.object == NULL) {
                    line += " 0";
                }
                else {
                    line += " 1 " + hx(ray.t) + " " + hx(ray.u) + " " + hx(ray.v);
                    for (int i = 0; i < 3; ++i)
                        line += " " + hx(ray.N[i]);
                }
                out.push_back(line);
            }
        }
    }

    void querySamples(const SceneDef &sc, CSurface *surf, int leafIndex, std::vector<std::string> &out) {
        const int n = sc.uvGrid;
        const int nv = n * n;
        struct Config {
                const char *label;
                double time;
                unsigned int flags;
        };
        std::vector<Config> configs;
        for (int ti = 0; ti < 7; ++ti)
            configs.push_back({NULL, kTimes[ti], 0});
        configs.push_back({"BEGIN", 0.0, PARAMETER_BEGIN_SAMPLE});
        configs.push_back({"END", 0.0, PARAMETER_END_SAMPLE});

        for (const Config &cfg : configs) {
            // Every varying slot gets zeroed storage: sample() may touch slots a given family does not
            // need, and an unwritten output must read as a defined zero
            std::vector<std::vector<float>> storage(VARIABLE_CONSTANTWIDTH + 1, std::vector<float>(4 * nv + 4, 0.0f));
            float *varying[VARIABLE_CONSTANTWIDTH + 1];
            for (unsigned int k = 0; k <= VARIABLE_CONSTANTWIDTH; ++k)
                varying[k] = storage[k].data();
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i) {
                    varying[VARIABLE_U][j * n + i] = (float)i / (float)(n - 1);
                    varying[VARIABLE_V][j * n + i] = (float)j / (float)(n - 1);
                    varying[VARIABLE_TIME][j * n + i] = (float)cfg.time;
                }

            // dPdtime is requested only with PARAMETER_BEGIN_SAMPLE. The patch-grid families
            // (patchgrid.cpp, bsplinePatchgrid.cpp) compute dPdtime as the difference of the end keys
            // and the begin keys, addressing the end keys at "vertexData + disp". That is right only
            // when vertexData is the begin key set (the BEGIN call): with PARAMETER_END_SAMPLE it
            // already points at the end keys, and in the time-interpolated path the interpolation loop
            // has advanced it past its scratch array, so both read outside the array (an existing
            // defect, GitHub #110: the values differ from run to run). A replay must be deterministic, so those
            // combinations are not recorded; the defect is reported, not fixed (FR-014).
            unsigned int up = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | cfg.flags;
            if ((cfg.flags & PARAMETER_BEGIN_SAMPLE) != 0)
                up |= PARAMETER_DPDTIME;
            // No primitive variables are requested: every "locals" slot is NULL, but the array itself
            // exists, because CVertexData::dispatch() indexes it for each non-global variable a primitive has
            std::vector<float **> locals(64, (float **)NULL);
            surf->sample(0, nv, varying, locals.data(), up);

            for (int k = 0; k < nv; ++k) {
                std::string line = std::string(sc.id) + " " + sc.family + " S " + std::to_string(leafIndex) + " " + std::to_string(k) + " " +
                                   (cfg.label != NULL ? std::string(cfg.label) : hx(cfg.time));
                const unsigned int slots[5] = {VARIABLE_P, VARIABLE_NG, VARIABLE_DPDU, VARIABLE_DPDV, VARIABLE_DPDTIME};
                for (unsigned int slot : slots)
                    for (int c = 0; c < 3; ++c)
                        line += " " + hx(varying[slot][k * 3 + c]);
                out.push_back(line);
            }
        }
    }

    // Run one scene and append its records. Returns false (with a message) if the scene did not
    // produce what it is there to cover.
    double secondsSince(std::chrono::steady_clock::time_point t0) {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }

    bool runScene(const SceneDef &sc, std::vector<std::string> &out, bool verbose) {
        const auto t0 = std::chrono::steady_clock::now();
        CReplaySession session(sc.shutterOpen, sc.shutterClose, sc.camera);
        sc.build(session);
        CShadingContext *ctx = session.ctx();
        if (ctx == NULL) {
            fprintf(stderr, "motion_replay: scene %s: no shading context\n", sc.id);
            return false;
        }

        std::vector<CObject *> leaves;
        for (CObject *top : session.capture()->captured) {
            if (CPoints *points = dynamic_cast<CPoints *>(top))
                (points->*pointsPrep(PointsPrepTag()))();
            if (CCurveMesh *mesh = dynamic_cast<CCurveMesh *>(top))
                (mesh->*curveMeshCreate(CurveMeshCreateTag()))(ctx); // The mesh builds its curves lazily and has no materialize()
            collectLeaves(ctx, top, leaves);
        }

        // Coverage guards
        std::map<std::string, int> classes;
        bool anyMoving = false, anyXform = false;
        bool haveClass = (sc.requiredClass == NULL || sc.requiredClass[0] == 0);
        for (CObject *l : leaves) {
            const std::string name = typeid(*l).name();
            classes[name]++;
            if (name.find(sc.requiredClass) != std::string::npos)
                haveClass = true;
            CSurface *s = dynamic_cast<CSurface *>(l);
            if (s != NULL && s->moving())
                anyMoving = true;
            if (l->xform != NULL && l->xform->next != NULL)
                anyXform = true;
        }
        if (verbose) {
            printf("scene %-28s family %-13s leaves %zu:", sc.id, sc.family, leaves.size());
            for (const auto &kv : classes)
                printf(" %s x%d", kv.first.c_str(), kv.second);
            printf(" | moving=%d xformMotion=%d invShutterTime=%g cameraMotion=%d rotation=%d\n", anyMoving, anyXform,
                   (double)CRenderer::invShutterTime, (int)CRenderer::cameraHasMotion, (int)CRenderer::cameraHasRotation);
            fflush(stdout);
        }
        if (leaves.empty() || !haveClass || (sc.expectMoving && !anyMoving) || (sc.expectXformMotion && !anyXform)) {
            fprintf(stderr, "motion_replay: scene %s does not cover what it is for (leaves %zu, class \"%s\" %s, moving %d/%d, xform motion %d/%d)\n", sc.id,
                    leaves.size(), sc.requiredClass, haveClass ? "present" : "ABSENT", (int)anyMoving, (int)sc.expectMoving, (int)anyXform,
                    (int)sc.expectXformMotion);
            return false;
        }

        if (verbose)
            fprintf(stderr, "motion_replay: %s built in %.1f s\n", sc.id, secondsSince(t0));
        auto excluded = [&](CObject *l) { return sc.excludeClass != NULL && std::string(typeid(*l).name()).find(sc.excludeClass) != std::string::npos; };
        for (size_t li = 0; li < leaves.size(); ++li)
            if (sc.intersect && !excluded(leaves[li]))
                queryIntersections(ctx, sc, leaves[li], (int)li, out);
        if (verbose)
            fprintf(stderr, "motion_replay: %s intersections done at %.1f s\n", sc.id, secondsSince(t0));
        if (sc.sample)
            for (size_t li = 0; li < leaves.size(); ++li) {
                if (excluded(leaves[li]))
                    continue;
                CSurface *s = dynamic_cast<CSurface *>(leaves[li]);
                if (s == NULL) {
                    fprintf(stderr, "motion_replay: scene %s: leaf %zu is not a CSurface\n", sc.id, li);
                    return false;
                }
                querySamples(sc, s, (int)li, out);
            }
        if (verbose)
            fprintf(stderr, "motion_replay: %s queries done at %.1f s\n", sc.id, secondsSince(t0));
        return true;
    }

    std::string headerLine() {
        return std::string("# motion_replay v1 ") + MOTION_REPLAY_PLATFORM + " " + kTree;
    }

    bool familyKnown(const char *f) {
        for (const char *k : kFamilies)
            if (strcmp(k, f) == 0)
                return true;
        return false;
    }

} // namespace

int main(int argc, char **argv) {
    const char *recordFile = NULL, *checkFile = NULL, *family = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--record") == 0 && i + 1 < argc)
            recordFile = argv[++i];
        else if (strcmp(argv[i], "--check") == 0 && i + 1 < argc)
            checkFile = argv[++i];
        else if (strcmp(argv[i], "--family") == 0 && i + 1 < argc)
            family = argv[++i];
        else {
            fprintf(stderr, "usage: motion_replay --record <file> | --check <file> [--family <f>]\n");
            return 2;
        }
    }
    if ((recordFile == NULL) == (checkFile == NULL) || (family != NULL && checkFile == NULL)) {
        fprintf(stderr, "usage: motion_replay --record <file> | --check <file> [--family <f>]\n");
        return 2;
    }
    if (family != NULL && !familyKnown(family)) {
        fprintf(stderr, "motion_replay: unknown family \"%s\"\n", family);
        return 2;
    }

    // Read the replay file first when checking: a missing file is a failure, never a skip
    std::vector<std::string> fileLines;
    if (checkFile != NULL) {
        FILE *f = fopen(checkFile, "r");
        if (f == NULL) {
            fprintf(stderr, "motion_replay: no replay file for %s (%s)\n", MOTION_REPLAY_PLATFORM, checkFile);
            return 1;
        }
        std::string cur;
        int ch;
        while ((ch = fgetc(f)) != EOF) {
            if (ch == '\n') {
                fileLines.push_back(cur);
                cur.clear();
            }
            else
                cur += (char)ch;
        }
        if (!cur.empty())
            fileLines.push_back(cur);
        fclose(f);
        const std::string headerPrefix = std::string("# motion_replay v1 ") + MOTION_REPLAY_PLATFORM + " ";
        if (fileLines.empty() || fileLines[0].compare(0, headerPrefix.size(), headerPrefix) != 0) {
            fprintf(stderr, "motion_replay: %s is not a %s replay file (header \"%s\", expected \"%s\")\n", checkFile, MOTION_REPLAY_PLATFORM,
                    fileLines.empty() ? "" : fileLines[0].c_str(), headerLine().c_str());
            return 1;
        }
    }

    // Run the scenes (all of them to record, one family's to check with --family)
    std::vector<std::string> lines;
    int scenesRun = 0;
    for (const SceneDef &sc : kScenes) {
        if (family != NULL && strcmp(sc.family, family) != 0)
            continue;
        if (!runScene(sc, lines, recordFile != NULL))
            return 2;
        ++scenesRun;
    }
    if (scenesRun == 0) {
        fprintf(stderr, "motion_replay: no scene to run\n");
        return 2;
    }

    if (recordFile != NULL) {
        FILE *f = fopen(recordFile, "w");
        if (f == NULL) {
            fprintf(stderr, "motion_replay: cannot write %s\n", recordFile);
            return 2;
        }
        fprintf(f, "%s\n", headerLine().c_str());
        for (const std::string &l : lines)
            fprintf(f, "%s\n", l.c_str());
        fclose(f);
        printf("motion_replay: recorded %zu records from %d scenes into %s\n", lines.size(), scenesRun, recordFile);
        return 0;
    }

    // Check: compare with the file's lines of the selected families, in order
    std::map<std::string, std::vector<std::string>> expectedBy, gotBy;
    for (size_t i = 1; i < fileLines.size(); ++i) {
        const std::string &l = fileLines[i];
        const size_t a = l.find(' ');
        const size_t b = a == std::string::npos ? a : l.find(' ', a + 1);
        if (b == std::string::npos) {
            fprintf(stderr, "motion_replay: malformed line %zu in %s\n", i + 1, checkFile);
            return 1;
        }
        const std::string fam = l.substr(a + 1, b - a - 1);
        if (family == NULL || fam == family)
            expectedBy[fam].push_back(l);
    }
    for (const std::string &l : lines) {
        const size_t a = l.find(' ');
        const size_t b = l.find(' ', a + 1);
        gotBy[l.substr(a + 1, b - a - 1)].push_back(l);
    }

    int failures = 0;
    for (const char *fam : kFamilies) {
        if (family != NULL && strcmp(fam, family) != 0)
            continue;
        const std::vector<std::string> &want = expectedBy[fam];
        const std::vector<std::string> &got = gotBy[fam];
        int shown = 0;
        size_t differing = 0;
        const size_t common = std::min(want.size(), got.size());
        for (size_t i = 0; i < common; ++i)
            if (want[i] != got[i]) {
                ++differing;
                if (shown++ < 10)
                    fprintf(stderr, "DIFF %s record %zu\n  replay: %s\n  now:    %s\n", fam, i, want[i].c_str(), got[i].c_str());
            }
        if (want.size() != got.size()) {
            fprintf(stderr, "COUNT %s: replay file has %zu records, this tree produced %zu (%s)\n", fam, want.size(), got.size(),
                    want.size() > got.size() ? "missing lines" : "extra lines");
            ++failures;
        }
        if (differing != 0) {
            fprintf(stderr, "family %s: %zu of %zu records differ\n", fam, differing, common);
            ++failures;
        }
        if (want.empty() && got.empty()) {
            fprintf(stderr, "family %s: no records in the replay file or in this tree\n", fam);
            ++failures;
        }
    }
    if (failures != 0)
        return 1;
    printf("motion_replay: %s matches %s\n", family != NULL ? family : "all families", checkFile);
    return 0;
}
