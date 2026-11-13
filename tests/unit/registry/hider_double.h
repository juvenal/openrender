/**
 * Project: openRender
 *
 * File: hider_double.h
 *
 * Description:
 *   Spec 030 (hider and integrator registry): the test double hiders of tests/unit/registry. They live in a test
 *   file, outside the renderer core and outside src/ri/hiders, and register themselves with OR_REGISTER_HIDER:
 *   that is the contributor scenario of US2 (FR-003, SC-002). The counters let a test observe how the core
 *   built, dispatched and destroyed them.
 *
 *     "testdouble"             alias "tdouble" (plain), "tdouble-old" (deprecated, with a warning text); provides
 *                              display-output; dispatches buckets; a pre-display setup that counts its calls;
 *                              renders nothing (it only takes the bucket jobs the core hands out).
 *     "testdouble-nullcreate"  as above, but its create returns NULL (the frame-abort test, contracts/frame-transport.md
 *                              "Late failures").
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef HIDER_DOUBLE_H
#define HIDER_DOUBLE_H

#include <atomic>

struct SHiderDoubleCounts {
        std::atomic<int> created{0};      // CDoubleHider objects constructed by create
        std::atomic<int> destroyed{0};    // ... destroyed by destroy
        std::atomic<int> buckets{0};      // BUCKET jobs taken in renderingLoop
        std::atomic<int> preDisplay{0};   // preDisplaySetup calls
        std::atomic<int> nullCreates{0};  // create calls of "testdouble-nullcreate"
        std::atomic<int> threadMask{0};   // bit i set: create saw thread i
        std::atomic<int> numThreadsSeen{0}; // OrHiderCreateInfo::numThreads of the last create
        std::atomic<int> peersSeen{0};    // create saw frame.peers->count == numThreads
        void reset() {
            created = 0;
            destroyed = 0;
            buckets = 0;
            preDisplay = 0;
            nullCreates = 0;
            threadMask = 0;
            numThreadsSeen = 0;
            peersSeen = 0;
        }
};

SHiderDoubleCounts &hiderDoubleCounts();

#define HIDER_DOUBLE_NAME "testdouble"
#define HIDER_DOUBLE_ALIAS "tdouble"
#define HIDER_DOUBLE_DEPRECATED_ALIAS "tdouble-old"
#define HIDER_DOUBLE_DEPRECATION_WARNING "Deprecated Hider \"tdouble-old\"; use \"testdouble\" instead"
#define HIDER_DOUBLE_NULLCREATE_NAME "testdouble-nullcreate"

#endif
