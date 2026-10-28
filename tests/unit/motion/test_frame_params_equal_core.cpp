/**
 * Project: openRender
 *
 * File: test_frame_params_equal_core.cpp
 *
 * Description:
 *   Spec 028 (camera-and-motion-layer), FR-001 / FR-002 / decision 5: Frame_ParamsEqualCore.
 *   A Consumer (it links ribRender and openrendercommon and drives in-process Ri sessions).
 *   The FrameParams the renderer core installs for a frame holds, for every field, exactly
 *   the value of the CRenderer static it is copied from, bit for bit, read from INSIDE the
 *   frame (here: from the capture context's addObject(), which runs between RiWorldBegin
 *   and RiWorldEnd). Covered: perspective and orthographic projection, depth of field on and
 *   off, a crop window, camera translation, rotation and rotation-only, and the shutters
 *   0 1, 0 0.5, 0.25 0.75 and 0.5 0.5. invShutterTime is the stored reciprocal (1 for the
 *   empty shutter, decision 5), no CSamplingConfig field is repeated outside `sampling`
 *   (FR-002), and `sampling` equals the CSamplingConfig of the shading contexts. Before
 *   RiWorldBegin and after RiWorldEnd frameParams() is the default value again, and a
 *   second RiBegin/RiEnd session installs a fresh value (gotcha 20: no RIB parsing here).
 *
 *   Red on the starting tree by build failure: frameParams.h does not exist. The
 *   executable is EXCLUDE_FROM_ALL and the test DISABLED until the phase that provides the
 *   API (task T045).
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
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "frameParams.h"
#include "options.h"
#include "renderer.h"
#include "rendererContext.h"
#include "ri.h"
#include "riHooks.h"
#include "shading.h"

using openrender::FrameParams;
using openrender::frameParams;

namespace {

    int g_failures = 0;
    int g_checks = 0;

    void fail(const char *config, const char *what) {
        ++g_failures;
        fprintf(stderr, "FAIL [%s] %s\n", config, what);
    }

    template <class T>
    void bits(const char *config, const char *what, const T &a, const T &b) {
        ++g_checks;
        if (memcmp(&a, &b, sizeof(T)) != 0)
            fail(config, what);
    }

    // FR-002: none of CSamplingConfig's fields is repeated at the top level of FrameParams
    template <class T>
    constexpr bool repeatsSampling = requires(T t) { t.nx; } || requires(T t) { t.ny; } || requires(T t) { t.jitter; } || requires(T t) { t.seed; } ||
                                     requires(T t) { t.frame; } || requires(T t) { t.backend; } || requires(T t) { t.originX; } ||
                                     requires(T t) { t.originY; } || requires(T t) { t.screenLeft; } || requires(T t) { t.screenTop; };
    static_assert(!repeatsSampling<FrameParams>, "FrameParams repeats no CSamplingConfig field outside `sampling`");

    enum { STATIC_CAMERA,
           TRANSLATE,
           ROTATE,
           ROTONLY };

    struct SConfig {
            const char *name;
            bool orthographic;
            bool dof;
            bool crop;
            int camera;
            float shutterOpen, shutterClose;
    };

    // Compares every field of the installed value with the static it is copied from
    void compareInsideFrame(const SConfig &c, const FrameParams *dflt) {
        const FrameParams &fp = frameParams();
        ++g_checks;
        if (&fp == dflt)
            fail(c.name, "frameParams() is the default value inside the frame");

        bits(c.name, "shutterOpen", fp.shutterOpen, CRenderer::shutterOpen);
        bits(c.name, "shutterClose", fp.shutterClose, CRenderer::shutterClose);
        bits(c.name, "shutterTime", fp.shutterTime, CRenderer::shutterTime);
        bits(c.name, "invShutterTime", fp.invShutterTime, CRenderer::invShutterTime);
        // decision 5: the stored reciprocal; 1 for an empty shutter
        const float want = (CRenderer::shutterClose - CRenderer::shutterOpen) <= 0 ? 1.0f : 1.0f / (CRenderer::shutterClose - CRenderer::shutterOpen);
        bits(c.name, "invShutterTime is the reciprocal of the shutter time (1 when empty)", fp.invShutterTime, want);

        const int projection = (int)CRenderer::projection;
        bits(c.name, "projection", fp.projection, projection);
        bits(c.name, "imagePlane", fp.imagePlane, CRenderer::imagePlane);
        bits(c.name, "invImagePlane", fp.invImagePlane, CRenderer::invImagePlane);
        bits(c.name, "dxdPixel", fp.dxdPixel, CRenderer::dxdPixel);
        bits(c.name, "dydPixel", fp.dydPixel, CRenderer::dydPixel);
        bits(c.name, "aperture", fp.aperture, CRenderer::aperture);
        bits(c.name, "focaldistance", fp.focaldistance, CRenderer::focaldistance);
        bits(c.name, "invFocaldistance", fp.invFocaldistance, CRenderer::invFocaldistance);
        bits(c.name, "focallength", fp.focallength, CRenderer::focallength);
        bits(c.name, "fstop", fp.fstop, CRenderer::fstop);

        bits(c.name, "camera.fromWorld", fp.camera.fromWorld, CRenderer::fromWorld);
        bits(c.name, "camera.toWorld", fp.camera.toWorld, CRenderer::toWorld);
        bits(c.name, "camera.fromWorld1", fp.camera.fromWorld1, CRenderer::fromWorld1);
        bits(c.name, "camera.toWorld1", fp.camera.toWorld1, CRenderer::toWorld1);
        bits(c.name, "camera.relRotQ", fp.camera.relRotQ, CRenderer::relRotQ);
        bits(c.name, "camera.relTrans", fp.camera.relTrans, CRenderer::relTrans);
        ++g_checks;
        if (fp.camera.hasMotion != CRenderer::cameraHasMotion)
            fail(c.name, "camera.hasMotion");
        ++g_checks;
        if (fp.camera.hasRotation != CRenderer::cameraHasRotation)
            fail(c.name, "camera.hasRotation");
        ++g_checks;
        if (fp.camera.rotationOnly != CRenderer::cameraRotationOnly)
            fail(c.name, "camera.rotationOnly");

        // `sampling` is the CSamplingConfig the shading contexts were given
        const openrender::sampling::CSamplingConfig &cfg = CRenderer::contexts[0]->samplingConfig();
        ++g_checks;
        if (fp.sampling.backend != cfg.backend || fp.sampling.seed != cfg.seed || fp.sampling.frame != cfg.frame || fp.sampling.nx != cfg.nx || fp.sampling.ny != cfg.ny ||
            memcmp(&fp.sampling.jitter, &cfg.jitter, sizeof(float)) != 0 || fp.sampling.originX != cfg.originX || fp.sampling.originY != cfg.originY ||
            memcmp(&fp.sampling.screenLeft, &cfg.screenLeft, sizeof(float)) != 0 || memcmp(&fp.sampling.screenTop, &cfg.screenTop, sizeof(float)) != 0)
            fail(c.name, "sampling differs from the CSamplingConfig of the shading contexts");

        // The configuration took effect (the test would otherwise compare defaults with defaults)
        ++g_checks;
        if (c.dof && !c.orthographic && fp.aperture == 0.0f)
            fail(c.name, "depth of field produced no aperture");
        if (c.orthographic && fp.projection != OPTIONS_PROJECTION_ORTHOGRAPHIC)
            fail(c.name, "orthographic projection not installed");
        if (c.camera != STATIC_CAMERA && !fp.camera.hasMotion)
            fail(c.name, "camera motion not installed");
        // With an empty shutter the two camera samples collapse onto the same instant, so there is no
        // relative rotation to install; the equality checks above still cover the (identity) values
        const bool emptyShutter = c.shutterClose == c.shutterOpen;
        if ((c.camera == ROTATE || c.camera == ROTONLY) && !emptyShutter && !fp.camera.hasRotation)
            fail(c.name, "camera rotation not installed");
        if (c.camera == ROTONLY && !emptyShutter && !fp.camera.rotationOnly)
            fail(c.name, "rotation-only not installed");
        if (c.camera == TRANSLATE && fp.camera.hasRotation)
            fail(c.name, "translation produced a rotation");
        if (c.shutterClose == c.shutterOpen && fp.invShutterTime != 1.0f)
            fail(c.name, "empty shutter: invShutterTime is not 1");
        if (c.crop && fp.sampling.originX == 0 && fp.sampling.originY == 0)
            fail(c.name, "crop window produced no crop origin");
    }

    class CCheckContext : public CRendererContext {
        public:
            const SConfig *config = nullptr;
            const FrameParams *dflt = nullptr;
            int objects = 0;

            virtual void addObject(CObject *o) {
                if (config != nullptr) {
                    compareInsideFrame(*config, dflt);
                    ++objects;
                }
                CRendererContext::addObject(o);
            }
    };

    CCheckContext *g_context = nullptr;
    CRiInterface *makeContext() {
        g_context = new CCheckContext();
        return g_context;
    }

    void session(const SConfig &c) {
        const FrameParams *dflt = &frameParams();

        RiSetContextFactory(makeContext);
        RiBegin(RI_NULL);
        RiHider("raytrace", RI_NULL);
        RiFormat(24, 16, 1);
        RiPixelSamples(2, 2);
        if (c.crop)
            RiCropWindow(0.25f, 0.8f, 0.1f, 0.9f);
        if (c.orthographic)
            RiProjection(RI_ORTHOGRAPHIC, RI_NULL);
        else {
            float fov = 50.0f;
            RiProjection(RI_PERSPECTIVE, "fov", &fov, RI_NULL);
        }
        if (c.dof)
            RiDepthOfField(4.0f, 0.1f, 5.0f);
        RiShutter(c.shutterOpen, c.shutterClose);
        switch (c.camera) {
            case TRANSLATE:
                RiMotionBegin(2, c.shutterOpen, c.shutterClose);
                RiTranslate(0.0f, 0.0f, 0.0f);
                RiTranslate(0.7f, 0.3f, 0.4f);
                RiMotionEnd();
                break;
            case ROTATE:
                RiTranslate(0.0f, 0.0f, 1.5f);
                RiMotionBegin(2, c.shutterOpen, c.shutterClose);
                RiRotate(0.0f, 0.3f, 1.0f, 0.2f);
                RiRotate(33.0f, 0.3f, 1.0f, 0.2f);
                RiMotionEnd();
                break;
            case ROTONLY:
                RiMotionBegin(2, c.shutterOpen, c.shutterClose);
                RiRotate(0.0f, 0.2f, 1.0f, 0.1f);
                RiRotate(27.0f, 0.2f, 1.0f, 0.1f);
                RiMotionEnd();
                break;
            default:
                break;
        }

        // Before the world block the frame has not begun: the default value
        ++g_checks;
        if (&frameParams() != dflt)
            fail(c.name, "a FrameParams is installed before RiWorldBegin");

        RiWorldBegin();
        g_context->config = &c;
        g_context->dflt = dflt;
        RiAttributeBegin();
        RiTranslate(0.0f, 0.0f, 5.0f);
        RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL);
        RiAttributeEnd();
        g_context->config = nullptr;
        ++g_checks;
        if (g_context->objects != 1)
            fail(c.name, "the capture context did not see the sphere");
        RiWorldEnd();
        RiEnd();
        RiSetContextFactory(NULL);
        g_context = nullptr;

        // After the frame: the default value again, and the same slot as before
        ++g_checks;
        if (&frameParams() != dflt)
            fail(c.name, "frameParams() is not the default value after RiWorldEnd");
        ++g_checks;
        if (frameParams().invShutterTime != 1.0f || frameParams().aperture != 0.0f || frameParams().camera.hasMotion)
            fail(c.name, "the default value is not restored after RiWorldEnd");
    }

} // namespace

int main() {
    const float shutters[][2] = {{0.0f, 1.0f}, {0.0f, 0.5f}, {0.25f, 0.75f}, {0.5f, 0.5f}};
    int n = 0;
    for (const auto &s : shutters) {
        const SConfig configs[] = {
            {"perspective pinhole", false, false, false, STATIC_CAMERA, s[0], s[1]},
            {"perspective depth of field + crop", false, true, true, STATIC_CAMERA, s[0], s[1]},
            {"orthographic + camera translation", true, false, false, TRANSLATE, s[0], s[1]},
            {"orthographic with depth of field requested", true, true, false, STATIC_CAMERA, s[0], s[1]},
            {"perspective + camera rotation", false, false, false, ROTATE, s[0], s[1]},
            {"perspective dof + camera rotation only + crop", false, true, true, ROTONLY, s[0], s[1]},
        };
        for (const SConfig &c : configs) {
            char name[160];
            snprintf(name, sizeof(name), "%s, shutter %g %g", c.name, s[0], s[1]);
            SConfig named = c;
            named.name = name;
            session(named);
            ++n;
        }
    }
    printf("%d sessions, %d checks, %d failures\n", n, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
