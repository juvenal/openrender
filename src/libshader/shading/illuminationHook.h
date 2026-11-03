/**
 * Project: openRender
 *
 * File: illuminationHook.h
 *
 * Description:
 *   IIlluminationHook: the photon pass's hooks into illuminate() and solar() (spec 024's illumination hook, spec
 *   029's interface, contracts/illumination-hook.md). Owned by Shading; implemented by the hider that wants them
 *   (the photon hider) and installed on the context it owns with CShadingContext::setIlluminationHook(). Called
 *   only while rendererIlluminationHook() is true, at the places the interpreter's macros and the JIT's jit*Begin/End
 *   functions have always called the former virtuals, with the operands hookOperand() picks (GitHub #68: a varying
 *   operand's first active point). With no hook installed the calls are no-ops.
 *
 *   Introduced in 2.1.0. Version: kRendererServicesVersion (rendererServices.h).
 *   Thread-safety: per context. Re-enters Shading: the photon hider traces, it never shades.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef ILLUMINATIONHOOK_H
#define ILLUMINATIONHOOK_H

namespace openrender::shading {

    class IIlluminationHook {
        public:
            virtual ~IIlluminationHook() = default; // inline: the vtable and the typeinfo are weak, wherever they are used

            // solar(); both operands are null for solar() with no arguments
            virtual void solarBegin(const float *axis, const float *angle) = 0;
            virtual void solarEnd() = 0;
            // illuminate(); axis and angle are null for the one-argument form
            virtual void illuminateBegin(const float *P, const float *axis, const float *angle) = 0;
            virtual void illuminateEnd() = 0;
    };

} // namespace openrender::shading

#endif
