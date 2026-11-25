/**
 * Project: openRender
 *
 * File: closureFrame.cpp
 *
 * Description:
 *   See closureFrame.h. Also defines resolveClosureMode() (closureMode.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "closureFrame.h"

#include "common/diagnostics.h"

namespace openrender::shading {

    namespace {

        // The builder's reports go to the common diagnostics, once per kind per frame (ClosureFrameDiagnostics counts the rest)
        void reportDiagnostic(void *, ClosureDiagKind, int code, const char *message) {
            warning(static_cast<EErrorCode>(code), "%s\n", message);
        }

    } // namespace

    ClosureFrame::ClosureFrame(ClosureMode sceneMode) : sceneMode_(sceneMode), diagnostics_(reportDiagnostic, nullptr) {}

    bool ClosureFrame::firstInFrame(const std::string &key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return seen_.insert(key).second;
    }

    ClosureFrame &ClosureFrame::fallback() {
        static ClosureFrame frame(ClosureMode::Unset);
        return frame;
    }

    ClosureMode resolveClosureMode(ClosureMode attribute, const ClosureFrame *frame) {
        if (attribute != ClosureMode::Unset)
            return attribute;
        if (frame != nullptr && frame->sceneMode() != ClosureMode::Unset)
            return frame->sceneMode();
        return ClosureMode::Color;
    }

} // namespace openrender::shading
