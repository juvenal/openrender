/**
 * Project: openRender
 *
 * File: test_motion_transform_ray.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-019 / FR-021: Motion_TransformRayEquivalence. A
 *   State-only program. motion::transformRay must produce, bit for bit, what the starting
 *   tree's ray transform produces (objectMisc.h:178-232, 4eb683b). The test compiles a
 *   VERBATIM COPY of that function in its own translation unit, with the three CRenderer
 *   statics it reads (cameraHasRotation, relRotQ, toWorld) replaced by the fields of the
 *   installed FrameParams' camera, and compares oFrom and oDir with memcmp, over rays at
 *   times -0.25 .. 1.25, for:
 *     - object motion (xform->next set, cameraMotion false);
 *     - camera translation (cameraMotion true, no rotation);
 *     - camera rotation, and rotation-only (cameraMotion true, hasRotation true);
 *     - a static object (no next).
 *   An object with its OWN transform motion under a camera that has rotation gets NO camera
 *   rotation (GitHub #106, preserved: the evaluator composes only what addObject synthesizes),
 *   and the test pins that too.
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

#include "frame_test_utils.h"
#include "motion.h"
#include "ray.h"
#include "xform.h"

namespace m = openrender::motion;
using openrender::FrameCameraMotion;
using openrender::FrameParams;

namespace {

    // ------------------------------------------------------------------------------------
    // Verbatim copy of transform(float *oFrom, float *oDir, const CXform *xform, CRay *ray),
    // objectMisc.h:178-232 (4eb683b). CRenderer::cameraHasRotation, CRenderer::relRotQ and
    // CRenderer::toWorld are the fields of cam.
    // ------------------------------------------------------------------------------------
    inline void referenceTransform(const FrameCameraMotion &cam, float *oFrom, float *oDir, const CXform *xform, CRay *ray) {
        if (xform->next != NULL) {
            vector tmp[5];
            vector to;

            addvv(to, ray->from, ray->dir);

            mulmp(tmp[0], xform->to, ray->from);
            mulmp(tmp[1], xform->to, to);

            mulmp(tmp[2], xform->next->to, ray->from);
            mulmp(tmp[3], xform->next->to, to);

            interpolatev(oFrom, tmp[0], tmp[2], ray->time);

            if (xform->cameraMotion && cam.hasRotation) {
                static const quaternion identQ = {0.0f, 0.0f, 0.0f, 1.0f};
                quaternion Rjt;
                matrix Mjt;
                vector dir_cam_t;
                const quaternion relRotQ_inv = {-cam.relRotQ[0],
                                                -cam.relRotQ[1],
                                                -cam.relRotQ[2],
                                                cam.relRotQ[3]};
                slerpq(Rjt, identQ, relRotQ_inv, ray->time);
                qtoR(Mjt, Rjt);
                mulmv(dir_cam_t, Mjt, ray->dir);     // rotate camera-space direction
                mulmv(oDir, cam.toWorld, dir_cam_t); // transform to world space
            }
            else {
                interpolatev(tmp[4], tmp[1], tmp[3], ray->time);
                subvv(oDir, tmp[4], oFrom);
            }
        }
        else {
            vector to, tmp;
            addvv(to, ray->from, ray->dir);

            mulmp(oFrom, xform->to, ray->from);
            mulmp(tmp, xform->to, to);
            subvv(oDir, tmp, oFrom);
        }
    }

    const double kDeg = 3.14159265358979323846 / 180.0;

    // A camera for the cases: camera-to-world at shutter open, and the relative rotation to shutter close
    FrameParams makeCamera(bool hasRotation, double angleDeg) {
        FrameParams fp = frametest::makeFrame();
        CXform cam;
        cam.translate(0.5f, -1.0f, 3.0f);
        cam.rotate(20.0f, 0.0f, 1.0f, 0.0f);
        memcpy(fp.camera.fromWorld, cam.from, sizeof(matrix));
        memcpy(fp.camera.toWorld, cam.to, sizeof(matrix));
        fp.camera.hasMotion = true;
        fp.camera.hasRotation = hasRotation;
        fp.camera.rotationOnly = false;
        const double h = 0.5 * angleDeg * kDeg;
        const double ax = 0.3, ay = 1.0, az = 0.2, len = std::sqrt(ax * ax + ay * ay + az * az);
        fp.camera.relRotQ[0] = (float)(ax / len * std::sin(h));
        fp.camera.relRotQ[1] = (float)(ay / len * std::sin(h));
        fp.camera.relRotQ[2] = (float)(az / len * std::sin(h));
        fp.camera.relRotQ[3] = (float)std::cos(h);
        normalizeq(fp.camera.relRotQ);
        return fp;
    }

    // Run both on a set of rays and times and compare bit for bit
    void compare(const FrameParams &fp, const CXform &xf, const char *label) {
        openrender::installFrameParams(&fp);
        const float froms[][3] = {{0.0f, 0.0f, 0.0f}, {0.5f, -1.0f, 0.25f}, {-2.0f, 1.5f, 3.0f}};
        const float dirs[][3] = {{0.0f, 0.0f, 1.0f}, {0.6f, 0.0f, 0.8f}, {-0.3f, 0.4f, 0.8660254f}};
        const float times[] = {-0.25f, 0.0f, 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.25f};
        for (const auto &f : froms)
            for (const auto &d : dirs)
                for (float t : times) {
                    CRay ray;
                    movvv(ray.from, f);
                    movvv(ray.dir, d);
                    ray.time = t;
                    float wantFrom[3], wantDir[3], gotFrom[3], gotDir[3];
                    referenceTransform(fp.camera, wantFrom, wantDir, &xf, &ray);
                    m::transformRay(gotFrom, gotDir, &xf, &ray);
                    if (!frametest::sameBits(gotFrom, wantFrom, 3) || !frametest::sameBits(gotDir, wantDir, 3)) {
                        frametest::fail(__FILE__, __LINE__, label);
                        fprintf(stderr, "  at t=%g\n", t);
                        openrender::installFrameParams(nullptr);
                        return;
                    }
                }
        openrender::installFrameParams(nullptr);
    }

    // The xform of an object that moves from one placement to another
    void movingXform(CXform &xf, bool cameraMotion) {
        xf.translate(1.0f, 2.0f, 3.0f);
        xf.rotate(15.0f, 1.0f, 0.0f, 0.0f);
        xf.next = new CXform(); // owned and freed by xf
        xf.next->translate(-1.0f, 0.5f, 4.0f);
        xf.next->rotate(40.0f, 0.0f, 1.0f, 1.0f);
        xf.cameraMotion = cameraMotion;
    }

} // namespace

int main() {
    // Object motion: no camera motion involved
    {
        CXform xf;
        movingXform(xf, false);
        compare(makeCamera(false, 0.0), xf, "object motion");
    }
    // Camera translation: the synthesized next, no rotation
    {
        CXform xf;
        movingXform(xf, true);
        compare(makeCamera(false, 0.0), xf, "camera translation");
    }
    // Camera rotation
    {
        CXform xf;
        movingXform(xf, true);
        compare(makeCamera(true, 33.0), xf, "camera rotation");
        compare(makeCamera(true, 90.0), xf, "camera rotation, 90 degrees");
    }
    // Rotation only
    {
        CXform xf;
        movingXform(xf, true);
        FrameParams fp = makeCamera(true, 27.0);
        fp.camera.rotationOnly = true;
        compare(fp, xf, "rotation only");
    }
    // A static object
    {
        CXform xf;
        xf.translate(1.0f, 2.0f, 3.0f);
        compare(makeCamera(true, 33.0), xf, "static object");
    }

    // GitHub #106, pinned: an object with its own transform motion under a camera that has
    // rotation gets no camera rotation. It takes the plain interpolation path, which is also what
    // the same object gets under a camera without rotation.
    {
        CXform xf;
        movingXform(xf, false);
        const FrameParams withRotation = makeCamera(true, 60.0);
        const FrameParams noRotation = makeCamera(false, 0.0);
        CRay ray;
        ray.from[0] = 0.5f;
        ray.from[1] = -1.0f;
        ray.from[2] = 0.25f;
        ray.dir[0] = 0.6f;
        ray.dir[1] = 0.0f;
        ray.dir[2] = 0.8f;
        ray.time = 0.75f;
        float a[3], b[3], c[3], d[3];
        openrender::installFrameParams(&withRotation);
        m::transformRay(a, b, &xf, &ray);
        openrender::installFrameParams(&noRotation);
        m::transformRay(c, d, &xf, &ray);
        openrender::installFrameParams(nullptr);
        EXPECT_BITS(a, c, 3);
        EXPECT_BITS(b, d, 3);
        compare(withRotation, xf, "own motion under a rotating camera (#106)");
    }
    return TEST_RESULT();
}
