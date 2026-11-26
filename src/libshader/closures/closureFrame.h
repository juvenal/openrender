/**
 * Project: openRender
 *
 * File: closureFrame.h
 *
 * Description:
 *   The per-frame closure object (spec 035, contracts/consumer-and-backend.md "ClosureFrame"): the scene mode of the frame, the set of keys
 *   the once-per-frame diagnostics (IR-017, IR-018) dedup on, and the frame's closure-tree diagnostics (each kind reported once per frame).
 *   The renderer core creates it in beginFrame() before the hider-creation loop, sets it on every context and deletes it in endFrame(); a
 *   context with none answers scene mode Unset and dedups through a process-wide fallback frame.
 *
 *   Thread safe: every render thread of the frame shares it.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSUREFRAME_H
#define CLOSUREFRAME_H

#include <mutex>
#include <set>
#include <string>

#include "closureMode.h"
#include "closureTree.h"

namespace openrender::shading {

    class ClosureFrame {
        public:
            explicit ClosureFrame(ClosureMode sceneMode);

            ClosureMode sceneMode() const { return sceneMode_; }

            // true the first time `key` is seen in this frame (thread safe); the caller then reports
            bool firstInFrame(const std::string &key) const;

            // The closure-tree diagnostics of the frame: each kind is reported once (the builders of every thread share it)
            IClosureDiagnostics &diagnostics() { return diagnostics_; }

            // The frame a context with none falls back on: scene mode Unset, one per process
            static ClosureFrame &fallback();

        private:
            ClosureMode sceneMode_;
            mutable std::mutex mutex_;
            mutable std::set<std::string> seen_;
            ClosureFrameDiagnostics diagnostics_;
    };

} // namespace openrender::shading

#endif
