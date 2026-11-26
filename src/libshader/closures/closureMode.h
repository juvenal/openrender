/**
 * Project: openRender
 *
 * File: closureMode.h
 *
 * Description:
 *   The closure mode of an object (spec 035, FR-017, contracts/closuremode.md): `color` (today's behaviour, and the default), `both` (the
 *   shader's Ci and Oi as today plus a closure tree per point for a consumer) and `closures` (the trees only: no lighting, Ci = 0, the
 *   shader's own Oi). The mirror of Graphics State's EClosureMode (state/stateVocabulary.h), value for value, so that neither domain
 *   includes the other's header; State_ClosureModeVocabularyMirror compares them.
 *
 *   resolveClosureMode(): the effective mode of an object is its attribute's if set, else the frame's scene mode if set, else Color.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSUREMODE_H
#define CLOSUREMODE_H

#include <cstdint>

namespace openrender::shading {

    enum class ClosureMode : uint8_t {
        Unset = 0, // no value at this level: the next level decides
        Color = 1,
        Both = 2,
        Closures = 3
    };

    class ClosureFrame;

    // The RIB spelling of a mode ("color", "both", "closures"); "unset" for Unset (a diagnostic spelling only)
    inline const char *closureModeSpelling(ClosureMode mode) {
        switch (mode) {
            case ClosureMode::Color: return "color";
            case ClosureMode::Both: return "both";
            case ClosureMode::Closures: return "closures";
            case ClosureMode::Unset: break;
        }
        return "unset";
    }

    // attribute if not Unset, else the frame's scene mode if not Unset (a null frame has none), else Color. Defined in closureFrame.cpp.
    ClosureMode resolveClosureMode(ClosureMode attribute, const ClosureFrame *frame);

} // namespace openrender::shading

#endif
