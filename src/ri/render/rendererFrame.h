/**
 * Project: openRender
 *
 * File: rendererFrame.h
 *
 * Description:
 *   The renderer core's side of the frame parameters (spec 028, FR-001 to FR-003): builds the
 *   FrameParams of a frame from the values CRenderer::beginFrame has computed, installs it in
 *   Graphics State's slot and builds the frame's camera; uninstalls both when the frame ends.
 *
 *   Called only by CRenderer: beginFrameParams right after the frame's sampling configuration is
 *   built and before the hider contexts are created, endFrameParams in endFrame after they are
 *   deleted, so no render thread exists while the slot is written. The value and the camera live
 *   in rendererFrame.cpp for the length of the frame.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef RENDERERFRAME_H
#define RENDERERFRAME_H

#include "camera.h"
#include "common/sampling.h"

// Copies the frame's shutter, raster, lens and camera-motion values from the renderer core, installs
// the FrameParams (with the given sampling configuration embedded) and builds the camera
void beginFrameParams(const openrender::sampling::CSamplingConfig &samplingConfig);

// Uninstalls the FrameParams and deletes the camera
void endFrameParams();

// The camera of the current frame (valid between beginFrameParams and endFrameParams)
const openrender::ICamera &frameCamera();

#endif
