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

namespace openrender::shading {

    void interimSurfaceNotClosureCapable(ClosureFrame &frame, const char *shaderName, ClosureMode mode);
    void interimClosuresWithoutConsumer(ClosureFrame &frame);

} // namespace openrender::shading

#endif
