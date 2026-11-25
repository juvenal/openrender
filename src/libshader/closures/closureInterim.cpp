/**
 * Project: openRender
 *
 * File: closureInterim.cpp
 *
 * Description:
 *   See closureInterim.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "closureInterim.h"

#include <string>

#include "common/diagnostics.h"

namespace openrender::shading {

    void interimSurfaceNotClosureCapable(ClosureFrame &frame, const char *shaderName, ClosureMode mode) {
        const char *spelling = closureModeSpelling(mode);
        const std::string key = std::string("IR-017|") + (shaderName != nullptr ? shaderName : "") + "|" + spelling;
        if (frame.firstInFrame(key))
            warning(CODE_INCAPABLE, "Surface \"%s\" does not emit closures: it renders as color under closuremode \"%s\"\n", shaderName != nullptr ? shaderName : "", spelling);
    }

    void interimClosuresWithoutConsumer(ClosureFrame &frame) {
        if (frame.firstInFrame("IR-018"))
            warning(CODE_INCAPABLE, "closuremode \"closures\" with no closure consumer installed: the lights are not run and the colour is 0; only the opacity is kept\n");
    }

} // namespace openrender::shading
