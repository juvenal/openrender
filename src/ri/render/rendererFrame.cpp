/**
 * Project: openRender
 *
 * File: rendererFrame.cpp
 *
 * Description:
 *   See rendererFrame.h. beginFrameParams copies every field from the CRenderer static of the same
 *   meaning and computes nothing, so each field equals today's value bit for bit; the shutter
 *   scale is the stored reciprocal.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "rendererFrame.h"

#include "frameParams.h"
#include "renderer.h"

namespace {

    openrender::FrameParams frameValue;               // the installed value
    openrender::CFrameCamera *frameCameraObject = NULL; // the frame's camera

} // namespace

void beginFrameParams(const openrender::sampling::CSamplingConfig &samplingConfig) {
    openrender::FrameParams &f = frameValue;

    f.sampling = samplingConfig;

    f.shutterOpen = CRenderer::shutterOpen;
    f.shutterClose = CRenderer::shutterClose;
    f.shutterTime = CRenderer::shutterTime;
    f.invShutterTime = CRenderer::invShutterTime;

    f.projection = (int)CRenderer::projection;
    f.imagePlane = CRenderer::imagePlane;
    f.invImagePlane = CRenderer::invImagePlane;
    f.dxdPixel = CRenderer::dxdPixel;
    f.dydPixel = CRenderer::dydPixel;

    f.aperture = CRenderer::aperture;
    f.focaldistance = CRenderer::focaldistance;
    f.invFocaldistance = CRenderer::invFocaldistance;
    f.focallength = CRenderer::focallength;
    f.fstop = CRenderer::fstop;

    movmm(f.camera.fromWorld, CRenderer::fromWorld);
    movmm(f.camera.toWorld, CRenderer::toWorld);
    movmm(f.camera.fromWorld1, CRenderer::fromWorld1);
    movmm(f.camera.toWorld1, CRenderer::toWorld1);
    movqq(f.camera.relRotQ, CRenderer::relRotQ);
    movvv(f.camera.relTrans, CRenderer::relTrans);
    f.camera.hasMotion = CRenderer::cameraHasMotion;
    f.camera.hasRotation = CRenderer::cameraHasRotation;
    f.camera.rotationOnly = CRenderer::cameraRotationOnly;

    openrender::installFrameParams(&frameValue);

    delete frameCameraObject;
    frameCameraObject = new openrender::CFrameCamera(frameValue);
}

void endFrameParams() {
    openrender::installFrameParams(NULL);

    delete frameCameraObject;
    frameCameraObject = NULL;
}

const openrender::ICamera &frameCamera() {
    assert(frameCameraObject != NULL);
    return *frameCameraObject;
}
