/**
 * Project: openRender
 *
 * File: rendererTransport.h
 *
 * Description:
 *   The renderer core's per-frame transport slot (spec 030, contracts/frame-transport.md). The core resolves the frame's
 *   hider and its integrator through the registries in beginFrame(), keeps the result here, and everything
 *   that used to read a hider's name or the hiderFlags description bits reads a declared capability trait instead.
 *
 *   The slot is written only by the core, before any render thread exists (beginFrameTransport, setFramePeers) and after the
 *   threads are joined (finishFrameTransport, endFrameTransport); it is read-only in between, so a reader needs no lock
 *   (FR-022). It is not part of Graphics State's FrameParams (research R14). Outside a frame frameTransport() is a default
 *   value: every trait false, nothing resolved.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef RENDERERTRANSPORT_H
#define RENDERERTRANSPORT_H

#include "hiderAbi.h"
#include "hiderPeers.h"
#include "transport/integratorAbi.h"

class CHider;
class COptions;

struct SHiderTraits {
        bool rasterization = false;    // the resolved hider provides "rasterization"
        bool displayOutput = false;    // ... "display-output"
        bool displayChannels = false;  // ... "display-channels"
        bool illuminationHook = false; // ... "illumination-hook"
        bool deepShadowOutput = false; // the NAMED hider provides "deep-shadow-output" (false for an unknown name)
};

struct SFrameTransport {
        const OrHiderTable *named = nullptr;                // the registration of CRenderer::hider as written; nullptr if unknown
        const OrHiderTable *hider = nullptr;                // resolved: named, or the reyes fallback
        const OrIntegratorTable *integratorNamed = nullptr; // the registration of the integrator name as written; nullptr if unknown
        const OrIntegratorTable *integrator = nullptr;      // resolved: named, or classic
        OrIntegratorInstance *integratorInstance = nullptr; // created and configured before the displays; destroyed by finishFrameTransport or abortFrameTransport
        bool prepared = false;                              // prepare returned OK: finish is due
        bool refused = false;                               // detected before the displays (an unsupported pair, a configure that failed): nothing built, no output file
        bool aborted = false;                               // detected after the displays (create returned NULL, prepare failed): no render thread runs
        SHiderTraits traits;                                // all false on a refused frame
        CHiderPeers peers;                                  // { contexts, numThreads }: owned by the slot, never a local; valid until endFrameTransport()
};

// Read-only; a default value outside a frame.
const SFrameTransport &frameTransport();

// Core-only (called by CRenderer).
void beginFrameTransport(const COptions *o);           // resolve the hider and the integrator (unknown: error, fallback), check the pair, create and configure; traits
void setFramePeers(CHider **contexts, int numThreads); // stores the peer group in the slot, before the construction loop
void prepareFrameTransport();                          // the integrator's prepare (the peers were stored by setFramePeers); a failure aborts the frame
void abortFrameTransport();                            // a frame that fails after its displays began: aborted, traits cleared, the integrator destroyed without finish
void finishFrameTransport();                           // the integrator's finish (when prepare returned OK) and destroy; start of endFrame
void endFrameTransport();                              // clears the slot, end of endFrame, after endDisplays

// Test-only: sets the slot's illuminationHook trait outside a frame and returns the previous value, so a test that drives
// the illumination hook with no renderer (the shading_parity probes) can switch it on and off and restore it. Writes the
// slot only where no render thread exists. Not part of any installed header (this file is not in OPENRENDER_PUBLIC_HEADERS).
bool setFrameIlluminationHookForTest(bool on);

#endif
