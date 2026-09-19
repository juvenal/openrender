/**
 * Project: openRender
 *
 * File: imager.cpp
 *
 * Description:
 *   CImagerExecutor implementation.
 *   Executes an imager shader over a pixel tile before display dispatch.
 *   Uses the shading context's CShadingState to drive the shader VM, copying
 *   pixel data in and out of the state's varying arrays (same data flow as
 *   surface/atmosphere shaders in shade()).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <algorithm>
#include <cstring>

#include "imager.h"
#include "memory.h"
#include "rendererc.h"
#include "renderer.h"
#include "shader.h"
#include "shading.h"

// Execute one chunk (≤ maxGridSize pixels) through the shader VM.
static void executeChunk(CShaderInstance &shader,
                         CShadingContext   *ctx,
                         int                left, int top,
                         int                width,
                         int                chunkStart, int chunkLen,
                         float             *pixels,
                         int                sampleStride) noexcept {

    CShadingState *state = ctx->newState();
    CShadingState *savedState = ctx->currentShadingState;

    const float shutterOpen = CRenderer::shutterOpen;
    const float dtime       = CRenderer::shutterClose - shutterOpen;

    // Copy pixel data into state's varying arrays.
    // Ci/Oi/P use [i*3+channel]; alpha/time use [i]; ncomps/dtime are uniform at [0].
    for (int idx = 0; idx < chunkLen; ++idx) {
        const int pidx  = chunkStart + idx;
        const float *px = pixels + pidx * sampleStride;

        state->varying[VARIABLE_CI][idx*3]   = px[0];
        state->varying[VARIABLE_CI][idx*3+1] = px[1];
        state->varying[VARIABLE_CI][idx*3+2] = px[2];
        // Synthesize Oi from scalar alpha (pixel buffer stores scalar, not 3-channel Oi)
        state->varying[VARIABLE_OI][idx*3]   =
        state->varying[VARIABLE_OI][idx*3+1] =
        state->varying[VARIABLE_OI][idx*3+2] = px[3];
        state->varying[VARIABLE_ALPHA][idx]  = px[3];

        state->varying[VARIABLE_P][idx*3]   = float(left + (pidx % width)) + 0.5f;
        state->varying[VARIABLE_P][idx*3+1] = float(top  + (pidx / width)) + 0.5f;
        state->varying[VARIABLE_P][idx*3+2] = 0.0f;

        state->varying[VARIABLE_TIME][idx] = shutterOpen;
    }
    // Uniform variables: one value covers the whole chunk
    state->varying[VARIABLE_NCOMPS][0] = 3.0f;
    state->varying[VARIABLE_DTIME][0]  = dtime;

    state->numVertices  = chunkLen;
    state->numActive    = chunkLen;
    state->numPassive   = 0;
    state->lights       = nullptr;
    state->alights      = nullptr;
    state->currentLight = nullptr;
    state->freeLights   = nullptr;
    state->currentObject = nullptr;
    state->postShader   = nullptr;
    std::memset(state->tags, 0, chunkLen * 3 * sizeof(int));

    ctx->currentShadingState = state;

    memBegin(ctx->threadMemory)
    float **locals = shader.prepare(ctx->threadMemory, state->varying, chunkLen);
    shader.execute(ctx, locals);
    memEnd(ctx->threadMemory)

    // Write results back; fold Oi to scalar alpha via channel average
    for (int idx = 0; idx < chunkLen; ++idx) {
        const int pidx = chunkStart + idx;
        float *px = pixels + pidx * sampleStride;

        px[0] = state->varying[VARIABLE_CI][idx*3];
        px[1] = state->varying[VARIABLE_CI][idx*3+1];
        px[2] = state->varying[VARIABLE_CI][idx*3+2];

        const float oiMean = (state->varying[VARIABLE_OI][idx*3]   +
                              state->varying[VARIABLE_OI][idx*3+1] +
                              state->varying[VARIABLE_OI][idx*3+2]) * (1.0f / 3.0f);
        // Per spec: if shader modified Oi independently of alpha, use oiMean; else use alpha
        const float newAlpha = state->varying[VARIABLE_ALPHA][idx];
        px[3] = (oiMean != newAlpha) ? oiMean : newAlpha;
    }

    ctx->currentShadingState = savedState;
    ctx->deleteState(state);
}

void CImagerExecutor::execute(CShaderInstance &shader,
                              int left, int top,
                              int width, int height,
                              float *pixels,
                              int sampleStride) noexcept {
    const int numPixels = width * height;
    if (numPixels == 0) return;

    log_debug("imager execute: tile ({},{}) {}x{} ({} pixels)", left, top, width, height, numPixels);

    // Use the calling thread's context; fall back to thread 0 for single-threaded tests.
    CShadingContext *ctx = CRenderer::activeContext;
    if (ctx == nullptr) {
        if (CRenderer::contexts == nullptr || CRenderer::contexts[0] == nullptr) {
            log_warn("imager execute: shading context unavailable, skipping");
            return;
        }
        ctx = CRenderer::contexts[0];
    }
    const int chunkSize = CRenderer::maxGridSize;

    // Process in chunks of at most maxGridSize pixels
    for (int start = 0; start < numPixels; start += chunkSize) {
        const int len = std::min(chunkSize, numPixels - start);
        executeChunk(shader, ctx, left, top, width, start, len, pixels, sampleStride);
    }

    log_debug("imager execute: tile done");
}
