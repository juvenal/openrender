/**
 * Project: openRender
 *
 * File: closureInterim.h
 *
 * Description:
 *   The interim rules of the surface step (spec 035, contracts/closuremode.md "Interim rules"). Each is one function, reports once per
 *   frame (the keys live in the ClosureFrame's shared set, so "once" holds across the render threads), and is owned by the spec named
 *   beside it, which replaces it. The ids appear in code, tests and the guide, not in the message text (Q14).
 *
 *     IR-017  interimSurfaceNotClosureCapable()  a Surface whose backend cannot emit closures under `both` or `closures` renders as
 *                                                color; one warning per (shader name, mode) per frame            (spec 036; spec 042, D6)
 *     IR-018  interimClosuresWithoutConsumer()   `closures` with no consumer installed: the gate is closed, Ci = 0, the shader's own Oi;
 *                                                one warning per frame                                           (spec 042)
 *     IR-026  interimBxdfWithoutConsumer()       a Bxdf governs and no consumer is installed: the object renders with the substitute surface
 *             interimSurfaceFor()                (the defaultsurface, else the no-shader path); one warning per Bxdf name per frame
 *                                                                                                              (spec 042)
 *   Precedence when more than one applies: IR-026, then IR-017, then IR-018 (a Bxdf governing ignores the mode and the Surface slot).
 *
 *   The reports are warnings (CODE_INCAPABLE): they reach an RI error handler as RIE_WARNING (CODE_LOG would be swallowed).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSUREINTERIM_H
#define CLOSUREINTERIM_H

#include "closureFrame.h"
#include "closureMode.h"

class CShaderInstance;

namespace openrender::shading {

    class CBxdfInstance;

    void interimSurfaceNotClosureCapable(ClosureFrame &frame, const char *shaderName, ClosureMode mode);
    void interimClosuresWithoutConsumer(ClosureFrame &frame);

    // IR-026: a Bxdf governs and no consumer is installed; one warning per Bxdf NAME per frame
    void interimBxdfWithoutConsumer(ClosureFrame &frame, const char *bxdfName);

    // IR-026's substitute (A', R16): the defaultsurface RiWorldBegin bound for the Bxdf, or nullptr for the built-in no-shader path. Never a
    // user-bound Surface.
    CShaderInstance *interimSurfaceFor(const CBxdfInstance &bxdf);

} // namespace openrender::shading

#endif
