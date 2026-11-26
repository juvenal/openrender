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

#include "bxdfInstance.h"
#include "common/diagnostics.h"
#include "libshader/shading/shaderRefs.h"

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

    void interimBxdfWithoutConsumer(ClosureFrame &frame, const char *bxdfName) {
        const std::string name = bxdfName != nullptr ? bxdfName : "";
        if (frame.firstInFrame("IR-026|" + name))
            warning(CODE_INCAPABLE, "Bxdf \"%s\" is bound and no closure consumer is installed: the object renders with the default surface, or with no shader when there is none\n", name.c_str());
    }

    CShaderInstance *interimSurfaceFor(const CBxdfInstance &bxdf) {
        return shaderInstanceOf(bxdf.substituteSurface);
    }

} // namespace openrender::shading
