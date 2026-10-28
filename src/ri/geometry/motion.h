/**
 * Project: openRender
 *
 * File: motion.h
 *
 * Description:
 *   The motion evaluator (spec 028, FR-011 to FR-016, FR-018, FR-019, FR-021): the one place the
 *   two-key motion model, the shutter scale of dPdtime, the grid transforms at a time and the
 *   ray-side camera-motion transform live. Header-only, inline, stateless: every function is a
 *   pure function of its arguments and of frameParams().
 *
 *   Motion keys are an object's two states, shutter open (t = 0) and shutter close (t = 1). Each
 *   family keeps its own storage for them; this file does not change it. The model is linear and
 *   unclamped: t outside 0..1 extrapolates, exactly as every site did before.
 *
 *   atTime is the only definition of the model. The per-site entry points are instantiations of
 *   it, named at the call site, because the sites differ in precision and spec 028 keeps each
 *   site's own arithmetic bit for bit (FR-014): atTimeD evaluates in double and rounds to float
 *   (the quadrics, patches, polygons, tessellation), atTimeF evaluates in float (subdivision). The
 *   expression key0 * (1 - t) + key1 * t, in this operand order, is the shape of every site; a
 *   re-ordered operand can change which multiply-add the compiler fuses and so a rounding.
 *
 *   Independent of any hider (FR-012): it names no renderer-core, shading or hider header.
 *   Includes the infrastructure headers, the ray, the transform and the frame parameters only.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef MOTION_H
#define MOTION_H

#include "common/algebra.h"
#include "common/rendererc.h"
#include "frameParams.h"
#include "ray.h"
#include "xform.h"

namespace openrender::motion {

    ///////////////////////////////////////////////////////////////////////
    // The two-key model
    ///////////////////////////////////////////////////////////////////////

    // The only definition of the model. Shutter open is t = 0, shutter close t = 1; t is not clamped.
    template <class S> inline S atTime(S key0, S key1, S t) {
        return key0 * (S(1) - t) + key1 * t;
    }

    // Per-site precision: instantiations of the template.
    // Evaluated in double, rounded to float (the quadrics' parameters, the patches, polygons, tessellation)
    inline float atTimeD(float k0, float k1, float t) {
        return (float)atTime<double>(k0, k1, t);
    }
    // Evaluated in float (subdivision)
    inline float atTimeF(float k0, float k1, float t) {
        return atTime<float>(k0, k1, t);
    }
    // A point or vector: three atTimeD, which is interpolatev's arithmetic
    inline void atTimeV(float *dst, const float *k0, const float *k1, float t) {
        dst[0] = atTimeD(k0[0], k1[0], t);
        dst[1] = atTimeD(k0[1], k1[1], t);
        dst[2] = atTimeD(k0[2], k1[2], t);
    }

    // A static object has one key (k1 == nullptr): its own data comes back unchanged. A moving one
    // fills the scratch with the keys' value at t and returns it.
    inline const float *keyAtTime(float *scratch, const float *k0, const float *k1, float t) {
        if (k1 == nullptr)
            return k0;
        atTimeV(scratch, k0, k1, t);
        return scratch;
    }

    // The key a sample call selects with PARAMETER_BEGIN_SAMPLE / PARAMETER_END_SAMPLE: 1 for the
    // shutter-close key, else 0. A family keeps its own "moving" test.
    inline int sampleKey(unsigned int up) {
        return (up & PARAMETER_END_SAMPLE) ? 1 : 0;
    }

    ///////////////////////////////////////////////////////////////////////
    // The dPdtime scale
    ///////////////////////////////////////////////////////////////////////

    // The stored reciprocal of the shutter time (1 for an empty shutter), never recomputed here
    inline float shutterRate() {
        return frameParams().invShutterTime;
    }

    // Scales a dPdtime vector from "per shutter" to "per unit time"
    inline void toShutterRate(float *v) {
        mulvf(v, shutterRate());
    }

    // The time a plug-in (the implicit surface) is given: the ray's own shutter fraction, unclamped
    inline float pluginTime(const CRay &ray) {
        return ray.time;
    }

    ///////////////////////////////////////////////////////////////////////
    // Grid transforms at a time (moved unchanged from objectMisc.h)
    ///////////////////////////////////////////////////////////////////////

    // Transform bunch of points from one space to another, interpolating the two keys' results at time[i]
    inline void transformAtTime(float *src, int num, const float *from0, const float *from1, const float *time) {
        int i;
        vector tmp0, tmp1;

        for (i = 0; i < num; i++) {
            mulmp(tmp0, from0, src);
            mulmp(tmp1, from1, src);
            interpolatev(src, tmp0, tmp1, time[i]);
            src += 3;
        }
    }

    // Transform bunch of vectors from one space to another, interpolating at time[i]
    inline void vtransformAtTime(float *src, int num, const float *from0, const float *from1, const float *time) {
        vector tmp0, tmp1;
        int i;

        for (i = 0; i < num; i++) {
            mulmv(tmp0, from0, src);
            mulmv(tmp1, from1, src);
            interpolatev(src, tmp0, tmp1, time[i]);
            src += 3;
        }
    }

    // Transform bunch of normals from one space to another, interpolating at time[i]
    inline void ntransformAtTime(float *src, int num, const float *to0, const float *to1, const float *time) {
        vector tmp0, tmp1;
        int i;

        for (i = 0; i < num; i++) {
            mulmn(tmp0, to0, src);
            mulmn(tmp1, to1, src);
            interpolatev(src, tmp0, tmp1, time[i]);
            src += 3;
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // The ray transform and the camera rotation
    ///////////////////////////////////////////////////////////////////////

    // M = the rotation matrix of slerp(identity, q, t): the arc a rotating camera sweeps (a lerp of
    // the endpoints would trace the chord). The primitive Reyes' own copies also compute.
    inline void cameraRotationAt(float *M, const float *q, float t) {
        static const quaternion identQ = {0.0f, 0.0f, 0.0f, 1.0f};
        quaternion Rjt;
        slerpq(Rjt, identQ, q, t);
        qtoR(M, Rjt);
    }

    // The ray in object space at the ray's time, moved unchanged from objectMisc.h. With
    // xform->cameraMotion and a camera that rotates, the direction is rotated along the camera's arc;
    // otherwise both ends of the ray are transformed by the xform's two keys and interpolated. It
    // composes only what CRendererContext::addObject synthesizes, so an object with its own transform
    // motion (cameraMotion false) gets no camera rotation, as before (GitHub #106).
    inline void transformRay(float *oFrom, float *oDir, const CXform *xform, const CRay *ray) {
        if (xform->next != NULL) {
            vector tmp[5];
            vector to;

            addvv(to, ray->from, ray->dir);

            mulmp(tmp[0], xform->to, ray->from);
            mulmp(tmp[1], xform->to, to);

            mulmp(tmp[2], xform->next->to, ray->from);
            mulmp(tmp[3], xform->next->to, to);

            interpolatev(oFrom, tmp[0], tmp[2], ray->time);

            const FrameCameraMotion &camera = frameParams().camera;
            if (xform->cameraMotion && camera.hasRotation) {
                // For camera rotation, LERP of world-space endpoints produces non-uniform
                // angular speed (chord, not arc). Apply SLERP in camera space instead,
                // matching the stochastic hider's APPLY_CAM_ROT macro.
                //
                // relRotQ is the rotation of relMotion (cam_t0 -> cam_t1), in camera space.
                // ray->dir is in camera space.  Correct formula:
                //   oDir = toWorld * SLERP(I, relRotQ, t) * ray->dir
                static const quaternion identQ = {0.0f, 0.0f, 0.0f, 1.0f};
                quaternion Rjt;
                matrix Mjt;
                vector dir_cam_t;
                // relRotQ = cam_t0 -> cam_t1 rotation.  For rays we need the inverse:
                // toWorld_t = toWorld * SLERP(I, relRotQ^{-1}, t)
                // Unit quaternion inverse = conjugate: negate xyz, keep w.
                const quaternion relRotQ_inv = {-camera.relRotQ[0],
                                                -camera.relRotQ[1],
                                                -camera.relRotQ[2],
                                                camera.relRotQ[3]};
                slerpq(Rjt, identQ, relRotQ_inv, ray->time);
                qtoR(Mjt, Rjt);
                mulmv(dir_cam_t, Mjt, ray->dir);        // rotate camera-space direction
                mulmv(oDir, camera.toWorld, dir_cam_t); // transform to world space
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

} // namespace openrender::motion

#endif
