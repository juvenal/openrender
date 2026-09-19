/**
 * Project: openRender
 *
 * File: imager.h
 *
 * Description:
 *   CImagerExecutor — executes an imager shader over a pixel tile.
 *   Called from CRenderer::dispatch() before display driver dispatch.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#pragma once

#include "includes/logging.hpp"

class CShaderInstance;

class CImagerExecutor {
  public:
    // Execute the imager shader over one pixel tile.
    // pixels: flat buffer, sampleStride floats per pixel.
    // Base layout: [Ci.r, Ci.g, Ci.b, alpha, Z, ...AOV...]
    void execute(CShaderInstance &shader,
                 int left, int top,
                 int width, int height,
                 float *pixels,
                 int sampleStride) noexcept;
};
