/**
 * Project: openRender
 *
 * File: test_motion_camera_slerp.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-021(a) / research R8: Motion_CameraSlerpEquivalence.
 *   A State-only program. The camera-rotation SLERP exists in two places for one spec: the
 *   evaluator (motion::cameraRotationAt, used by the ray transform) and Reyes' own inline
 *   copies, which 028 does not touch. This test pins them to the same results, BITWISE:
 *     - (1) Reyes' sequence "slerpq(Rjt, identQ, q, t); qtoR(M, Rjt)" with q = relRotQ,
 *       cited from reyes.cpp:1433, 1604, 1643, stochasticQuad.h:520, 884 and
 *       rendererContext.cpp:740;
 *     - (2) the conjugate forms of the pure-rotation fast path (stochastic.cpp:166-221,
 *       invRotQ) and of the ray transform (objectMisc.h:209-214, relRotQ_inv), q = conj(relRotQ).
 *   The Reyes copies are inline code that a test cannot call, and 028 edits no Reyes file
 *   (task T050 checks it), so a verbatim copy cited by file and line stays exact. A change to
 *   slerpq's argument order, or a missing conjugate in the evaluator's caller path, turns
 *   this red.
 *
 *   Relative rotations: identity, 1e-4 rad, 5, 45, 90 degrees about each axis and an oblique
 *   axis, and 179 degrees; times: 0, the strata 1/16 .. 15/16, 0.25, 0.5, 0.75, 1.
 *
 *   Red on the starting tree by build failure: motion.h does not exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cmath>
#include <vector>

#include "frame_test_utils.h"
#include "motion.h"

namespace m = openrender::motion;

namespace {

    // The rotation of angle (radians) about the axis, as a unit quaternion (x, y, z, scalar), the
    // layout of qfromR/qtoR
    void quaternionOf(float *q, double angle, double ax, double ay, double az) {
        const double len = std::sqrt(ax * ax + ay * ay + az * az);
        const double s = std::sin(0.5 * angle) / len;
        q[0] = (float)(ax * s);
        q[1] = (float)(ay * s);
        q[2] = (float)(az * s);
        q[3] = (float)std::cos(0.5 * angle);
        normalizeq(q);
    }

    // Reyes' sequence, verbatim (reyes.cpp:1433): slerpq(Rjt, identQ, CRenderer::relRotQ, jt); qtoR(Mjt, Rjt)
    void reyesSequence(float *M, const float *q, float jt) {
        const float identQ[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        quaternion Rjt;
        slerpq(Rjt, identQ, q, jt);
        qtoR(M, Rjt);
    }

    // The ray transform's sequence, verbatim (objectMisc.h:209-214): the conjugate, then the same two calls
    void rayTransformSequence(float *M, const float *relRotQ, float time) {
        static const quaternion identQ = {0.0f, 0.0f, 0.0f, 1.0f};
        quaternion Rjt;
        const quaternion relRotQ_inv = {-relRotQ[0], -relRotQ[1], -relRotQ[2], relRotQ[3]};
        slerpq(Rjt, identQ, relRotQ_inv, time);
        qtoR(M, Rjt);
    }

} // namespace

int main() {
    const double deg = 3.14159265358979323846 / 180.0;
    const double angles[] = {0.0, 1e-4, 5 * deg, 45 * deg, 90 * deg, 179 * deg};
    const double axes[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 2, 3}, {-0.3, 1, 0.2}};

    std::vector<float> times = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (int k = 1; k < 16; ++k)
        times.push_back(k / 16.0f);

    int cases = 0;
    for (double a : angles)
        for (const auto &ax : axes) {
            float q[4], conj[4];
            quaternionOf(q, a, ax[0], ax[1], ax[2]);
            conj[0] = -q[0];
            conj[1] = -q[1];
            conj[2] = -q[2];
            conj[3] = q[3];
            for (float t : times) {
                matrix got, want, gotConj, wantRay;
                m::cameraRotationAt(got, q, t);
                reyesSequence(want, q, t);
                EXPECT_BITS(got, want, 16);

                // The conjugate forms: the evaluator with conj(q) equals the ray transform's sequence with q
                m::cameraRotationAt(gotConj, conj, t);
                rayTransformSequence(wantRay, q, t);
                EXPECT_BITS(gotConj, wantRay, 16);
                ++cases;
            }
        }
    CHECK(cases == 6 * 5 * 20);

    // Anchors that do not depend on the copies: t = 0 is the identity, t = 1 reaches the rotation
    {
        float q[4];
        quaternionOf(q, 45 * deg, 1, 2, 3);
        matrix M0, M1, R;
        m::cameraRotationAt(M0, q, 0.0f);
        m::cameraRotationAt(M1, q, 1.0f);
        qtoR(R, q);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) {
                CHECK(std::fabs(M0[r + 4 * c] - (r == c ? 1.0f : 0.0f)) < 1e-6f);
                CHECK(std::fabs(M1[r + 4 * c] - R[r + 4 * c]) < 1e-5f);
            }
    }
    return TEST_RESULT();
}
