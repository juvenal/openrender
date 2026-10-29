/**
 * Project: openRender
 *
 * File: test_motion_two_key.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-011 / decision 3 / US2 AS2-AS3: Motion_TwoKeyModel.
 *   A State-only program (narrow include path plus src/ri/geometry for motion.h). The
 *   two-key motion model has ONE definition, openrender::motion::atTime<S>, and the per-site
 *   precision entry points are instantiations of it. Over a dense set of finite keys and
 *   times, including t < 0 and t > 1 (the starting tree extrapolates, no clamp):
 *     - atTimeD(k0, k1, 0) == k0 and atTimeD(k0, k1, 1) == k1 (compared with ==: a -0.0 key
 *       may come back +0.0, as today);
 *     - atTimeF == atTime<float>, atTimeD == (float)atTime<double>, and atTimeV equals three
 *       atTimeD, all bitwise;
 *     - atTimeD and atTimeV equal verbatim copies of the starting tree's quadric expression
 *       (quadrics.cpp:200) and of interpolatev (mathSpec.h:126, the real function), atTimeF
 *       equals a copy of subdivision.cpp:186's expression, and the polygons' fused form
 *       atTime<double> equals a copy of polygons.cpp:312-313's;
 *     - keyAtTime(scratch, k0, nullptr, t) returns k0 itself (pointer identity: a static
 *       object is unchanged), and with a second key fills the scratch with atTimeV;
 *     - sampleKey(PARAMETER_BEGIN_SAMPLE) == 0, sampleKey(PARAMETER_END_SAMPLE) == 1;
 *     - toShutterRate(v) equals mulvf(v, invShutterTime) for the installed shutters 0 1, 0 0.5,
 *       0.25 0.75 and 0.5 0.5 (the empty shutter, whose reciprocal is 1).
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

#include <cstring>

#include "frame_test_utils.h"
#include "motion.h"

namespace m = openrender::motion;

namespace {

    const float kKeys[] = {-3.7f, -0.1f, 0.0f, 0.001f, 0.3f, 1.0f, 1.3333334f, 2.718281f, 123.456f, -77.25f, 65504.5f};
    const float kTimes[] = {-1.7f, -0.25f, -0.001f, 0.0f, 0.0625f, 0.1f, 0.25f, 0.3333333f, 0.5f, 0.75f, 0.9f, 1.0f, 1.0625f, 1.25f, 2.3f};

    // Verbatim: quadrics.cpp:200, the intersect side of a quadric parameter
    float quadricIntersectLerp(float p, float next, float time) {
        return (float)(p * (1.0 - time) + next * (double)time);
    }

    // Verbatim: subdivision.cpp:186-189, a float-evaluated lerp (const float ctime)
    float subdivisionLerp(float vertex0, float vertex1, float ctime) {
        return vertex0 * (1 - ctime) + vertex1 * ctime;
    }

    void scalarModel() {
        for (float k0 : kKeys)
            for (float k1 : kKeys)
                for (float t : kTimes) {
                    const float d = m::atTimeD(k0, k1, t);
                    const float f = m::atTimeF(k0, k1, t);
                    CHECK(frametest::sameBits(d, (float)m::atTime<double>((double)k0, (double)k1, (double)t)));
                    CHECK(frametest::sameBits(f, m::atTime<float>(k0, k1, t)));
                    CHECK(frametest::sameBits(d, quadricIntersectLerp(k0, k1, t)));
                    CHECK(frametest::sameBits(f, subdivisionLerp(k0, k1, t)));
                }
        // The ends are the keys (== : a -0.0 key may come back +0.0)
        for (float k0 : kKeys)
            for (float k1 : kKeys) {
                CHECK(m::atTimeD(k0, k1, 0.0f) == k0);
                CHECK(m::atTimeD(k0, k1, 1.0f) == k1);
                CHECK(m::atTimeF(k0, k1, 0.0f) == k0);
                CHECK(m::atTimeF(k0, k1, 1.0f) == k1);
            }
    }

    void vectorModel() {
        for (size_t i = 0; i + 2 < sizeof(kKeys) / sizeof(kKeys[0]); ++i) {
            const float a[3] = {kKeys[i], kKeys[i + 1], kKeys[i + 2]};
            const float b[3] = {kKeys[i + 2], kKeys[i], kKeys[i + 1]};
            for (float t : kTimes) {
                float v[3], want[3], viaInterpolatev[3];
                m::atTimeV(v, a, b, t);
                for (int c = 0; c < 3; ++c)
                    want[c] = m::atTimeD(a[c], b[c], t);
                interpolatev(viaInterpolatev, a, b, t); // the starting tree's function (mathSpec.h:126)
                EXPECT_BITS(v, want, 3);
                EXPECT_BITS(v, viaInterpolatev, 3);
            }
        }
    }

    // Verbatim: polygons.cpp:312-313, the fused spatial + temporal interpolation of a triangle's
    // vertex data; the evaluator takes the same two double operands and applies the model
    void polygonFusedModel() {
        const float sv00[1] = {1.7f}, sv01[1] = {-0.6f}, sv02[1] = {0.35f};
        const float sv10[1] = {2.3f}, sv11[1] = {0.9f}, sv12[1] = {-1.1f};
        const double us[] = {0.0, 0.25, 0.6, 1.0};
        const double vs[] = {0.0, 0.4, 0.75, 1.0};
        for (double cu : us)
            for (double cv : vs)
                for (float t : kTimes) {
                    const double ctime = t;
                    const int k = 0;
                    const float want = (float)((sv00[k] * (1.0 - cu) + sv01[k] * cu * cv + sv02[k] * cu * (1.0 - cv)) * (1.0 - ctime) +
                                               (sv10[k] * (1.0 - cu) + sv11[k] * cu * cv + sv12[k] * cu * (1.0 - cv)) * ctime);
                    const float got = (float)m::atTime<double>((sv00[k] * (1.0 - cu) + sv01[k] * cu * cv + sv02[k] * cu * (1.0 - cv)),
                                                               (sv10[k] * (1.0 - cu) + sv11[k] * cu * cv + sv12[k] * cu * (1.0 - cv)), ctime);
                    CHECK(frametest::sameBits(want, got));
                }
    }

    void keys() {
        const float k0[3] = {1.0f, 2.0f, 3.0f};
        const float k1[3] = {4.0f, -5.0f, 6.5f};
        float scratch[3] = {-1.0f, -1.0f, -1.0f};
        // A static object: the key itself, the scratch untouched
        CHECK(m::keyAtTime(scratch, k0, nullptr, 0.5f) == k0);
        CHECK(scratch[0] == -1.0f && scratch[1] == -1.0f && scratch[2] == -1.0f);
        // A moving one: the scratch, filled with atTimeV
        for (float t : kTimes) {
            const float *r = m::keyAtTime(scratch, k0, k1, t);
            float want[3];
            m::atTimeV(want, k0, k1, t);
            CHECK(r == scratch);
            EXPECT_BITS(r, want, 3);
        }
        CHECK(m::sampleKey(PARAMETER_BEGIN_SAMPLE) == 0);
        CHECK(m::sampleKey(PARAMETER_END_SAMPLE) == 1);
        CHECK(m::sampleKey(PARAMETER_END_SAMPLE | PARAMETER_P | PARAMETER_DPDTIME) == 1);
        CHECK(m::sampleKey(PARAMETER_BEGIN_SAMPLE | PARAMETER_P) == 0);
        CHECK(m::sampleKey(PARAMETER_P) == 0);
    }

    void shutterRate() {
        const float shutters[][2] = {{0.0f, 1.0f}, {0.0f, 0.5f}, {0.25f, 0.75f}, {0.5f, 0.5f}};
        for (const auto &s : shutters) {
            openrender::FrameParams fp = frametest::makeFrame();
            fp.shutterOpen = s[0];
            fp.shutterClose = s[1];
            fp.shutterTime = s[1] - s[0];
            // CRenderer::beginFrame (renderer.cpp:459-463): the stored reciprocal, 1 for an empty shutter
            fp.invShutterTime = (fp.shutterTime <= 0) ? 1.0f : 1.0f / fp.shutterTime;
            openrender::installFrameParams(&fp);

            CHECK(frametest::sameBits(m::shutterRate(), fp.invShutterTime));
            const float v0[3] = {1.25f, -3.5f, 0.1f};
            float got[3], want[3];
            memcpy(got, v0, sizeof(got));
            memcpy(want, v0, sizeof(want));
            m::toShutterRate(got);
            mulvf(want, fp.invShutterTime); // mulvf(dest, CRenderer::invShutterTime), the starting tree's line
            EXPECT_BITS(got, want, 3);
            openrender::installFrameParams(nullptr);
        }
    }

} // namespace

int main() {
    scalarModel();
    vectorModel();
    polygonFusedModel();
    keys();
    shutterRate();
    return TEST_RESULT();
}
