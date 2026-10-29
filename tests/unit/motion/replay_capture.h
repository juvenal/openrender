/**
 * Project: openRender
 *
 * File: replay_capture.h
 *
 * Description:
 *   The capture context and in-process Ri session of spec 028's motion replay (task T005),
 *   modelled on tests/unit/csg/csgTestUtils.h.
 *
 *   - CCaptureContext: a CRendererContext whose addObject() hands every object a test emits
 *     to CRendererContext::addObject() FIRST, so the camera-motion synthesis of
 *     rendererContext.cpp (a private xform with a "next" and an expanded bound) runs exactly
 *     as in production, and then keeps a reference to it and records it. Capturing without
 *     calling the base class would bypass that synthesis, and the camera scenes would test
 *     nothing (plan, "Replay harness fidelity"); Motion_Replay_raytransform's mutation
 *     proves the synthesis is exercised.
 *
 *     The reference is taken BEFORE the base call: the base addObject() ends with a
 *     detach(), and CRenderer::render() only keeps the object when it is raytraced, so an
 *     object the core does not retain would otherwise be freed before it is recorded. The
 *     synthesis does not depend on the reference count, so the order does not change what
 *     it computes.
 *
 *   - CReplaySession: one fresh RiBegin/RiWorldBegin ... RiWorldEnd/RiEnd session per scene
 *     (gotcha 20: this tool never parses RIB). The shutter and an optional camera motion are
 *     set before RiWorldBegin. EVERY intersect(), sample() and materialize() call of a scene
 *     runs while the session is open, between RiWorldBegin and RiWorldEnd, never after
 *     RiWorldEnd: the frame values are installed in beginFrame and uninstalled in endFrame,
 *     so a query made after the world block would read the CRenderer statics today but the
 *     default FrameParams after spec 028's phase 5, and the replay would go red with no cause
 *     in the migrated code.
 *
 *   Starting-tree interfaces only (contracts/replay-format.md rule 1): the Ri API,
 *   CObject::intersect(), CObject::sample() and CObject::materialize(). It includes no
 *   header of the spec 028 camera or motion layer.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef MOTION_REPLAY_CAPTURE_H
#define MOTION_REPLAY_CAPTURE_H

#include <cstddef>
#include <vector>

#include "object.h"
#include "renderer.h"
#include "rendererContext.h"
#include "ri.h"
#include "riHooks.h"

namespace motionreplay {

    // How the camera moves over the shutter (set before RiWorldBegin)
    enum ECameraMotion {
        CAMERA_STATIC = 0,
        CAMERA_TRANSLATE = 1, // translation only
        CAMERA_ROTATE = 2,    // rotation about a point away from the camera: rotation and translation
        CAMERA_ROTONLY = 3    // rotation about the camera: the rotation-only fast path
    };

    class CCaptureContext : public CRendererContext {
        public:
            std::vector<CObject *> captured;
            bool capturing = false; // Inside CReplaySession::add()

            virtual void addObject(CObject *o) {
                if (!capturing) {
                    CRendererContext::addObject(o);
                    return;
                }
                o->attach();                    // Our reference, taken before the core's detach()
                captured.push_back(o);          // Capture order is the leaf order of the replay file
                CRendererContext::addObject(o); // Camera-motion synthesis and CRenderer::render(), as in production
            }
    };

    inline CCaptureContext *&captureContextSlot() {
        static CCaptureContext *slot = NULL;
        return slot;
    }

    inline CRiInterface *makeCaptureContext() {
        captureContextSlot() = new CCaptureContext();
        return captureContextSlot();
    }

    class CReplaySession {
        public:
            // The raytrace hider keeps raytraced objects in the hierarchy and never calls the
            // Reyes drawObject(); a 16 x 16 one-sample frame keeps the RiWorldEnd at the end of
            // the session cheap (it renders what the scene holds). No Display is declared, so no
            // image is written.
            CReplaySession(float shutterOpen, float shutterClose, int camera) {
                RiSetContextFactory(makeCaptureContext);
                RiBegin(RI_NULL);
                RiHider("raytrace", RI_NULL);
                RiFormat(16, 16, 1);
                RiPixelSamples(1, 1);
                RiShutter(shutterOpen, shutterClose);

                // Camera motion, as in examples/rib/tests/camera-motion-*.rib: MotionBegin
                // before WorldBegin. The motion times equal the shutter times, so the two
                // samples are the camera at shutter open and at shutter close.
                switch (camera) {
                    case CAMERA_TRANSLATE:
                        RiMotionBegin(2, shutterOpen, shutterClose);
                        RiTranslate(0.0f, 0.0f, 0.0f);
                        RiTranslate(0.7f, 0.3f, 0.4f);
                        RiMotionEnd();
                        break;
                    case CAMERA_ROTATE:
                        RiTranslate(0.0f, 0.0f, 1.5f);
                        RiMotionBegin(2, shutterOpen, shutterClose);
                        RiRotate(0.0f, 0.3f, 1.0f, 0.2f);
                        RiRotate(33.0f, 0.3f, 1.0f, 0.2f);
                        RiMotionEnd();
                        break;
                    case CAMERA_ROTONLY:
                        RiMotionBegin(2, shutterOpen, shutterClose);
                        RiRotate(0.0f, 0.2f, 1.0f, 0.1f);
                        RiRotate(27.0f, 0.2f, 1.0f, 0.1f);
                        RiMotionEnd();
                        break;
                    default:
                        break;
                }

                RiWorldBegin();
            }

            ~CReplaySession() {
                for (CObject *o : capture()->captured)
                    o->detach();
                capture()->captured.clear();
                RiWorldEnd();
                RiEnd();
                RiSetContextFactory(NULL);
                captureContextSlot() = NULL;
            }

            CReplaySession(const CReplaySession &) = delete;
            CReplaySession &operator=(const CReplaySession &) = delete;

            CCaptureContext *capture() const { return captureContextSlot(); }

            // A real shading context, for materialization
            CShadingContext *ctx() const { return CRenderer::contexts != NULL ? CRenderer::contexts[0] : NULL; }

            // Run Ri calls in their own attribute block; whatever objects they create are
            // captured. Returns how many.
            template <class F>
            size_t add(F emit) {
                const size_t before = capture()->captured.size();
                capture()->capturing = true;
                RiAttributeBegin();
                emit();
                RiAttributeEnd();
                capture()->capturing = false;
                return capture()->captured.size() - before;
            }
    };

    // Depth first: materialize the object, then each of its children, and collect every object
    // that has no children of its own (the leaves a ray or a sample query is aimed at).
    inline void collectLeaves(CShadingContext *ctx, CObject *o, std::vector<CObject *> &out) {
        o->materialize(ctx);
        if (o->children == NULL) {
            out.push_back(o);
            return;
        }
        for (CObject *c = o->children; c != NULL; c = c->sibling)
            collectLeaves(ctx, c, out);
    }

} // namespace motionreplay

#endif
