/**
 * Project: openRender
 *
 * File: geometryFrame.cpp
 *
 * Description:
 *   The installed CGeometryFrame (spec 031, FR-002): one pointer, read lock-free by any thread.
 *
 *   The pointer is installed by the renderer core after the frame's hiders exist and cleared last when the
 *   frame ends, so no thread reads it while it changes. A debug build of the renderer (OPENRENDER_GEOMETRY_FRAME_REPORT,
 *   defined for the renderer library only) reports once a read with no frame installed; libRIBVector, which
 *   never installs one, does not.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "geometryFrame.h"

#include <atomic>
#include <cassert>

#include "common/diagnostics.h"

namespace {

    std::atomic<const CGeometryFrame *> installedFrame{nullptr};

    const CGeometryFrame zeroFrame = CGeometryFrame();

#if defined(OPENRENDER_GEOMETRY_FRAME_REPORT) && (!defined(NDEBUG) || defined(OPENRENDER_GEOMETRY_FRAME_REPORT_ALWAYS))
    std::atomic<bool> reported{false};

    void reportNoFrame() {
        if (!reported.exchange(true))
            error(CODE_BUG, "Geometry read the frame limits with no CGeometryFrame installed\n");
    }
#else
    inline void reportNoFrame() {}
#endif

} // namespace

void installGeometryFrame(const CGeometryFrame *frame) {
    assert(frame == nullptr || (frame->numThreads >= 1 && frame->maxGridSize >= 1));
    installedFrame.store(frame, std::memory_order_release);
}

const CGeometryFrame &geometryFrame() {
    const CGeometryFrame *frame = installedFrame.load(std::memory_order_acquire);
    if (frame != nullptr)
        return *frame;
    reportNoFrame();
    return zeroFrame;
}

unsigned int geometryFrameRaytracingFlags() {
    const CGeometryFrame *frame = installedFrame.load(std::memory_order_acquire);
    return frame != nullptr ? frame->raytracingFlags : 0u;
}
