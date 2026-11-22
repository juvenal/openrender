/**
 * Project: openRender
 *
 * File: areaLightControls.h
 *
 * Description:
 *   Spec 057 (area-light geometry), FR-018: how the three levels of the "arealight" controls combine for one receiver and one
 *   bound light. One pure function, header-only, shared by CShadingContext::iterateLights and the precedence-table test.
 *
 *   Levels, most specific first: the receiver's own attributes (object), the light's (set by the attributes in force at its
 *   AreaLightSource statement), the scene (Option, always set by the hider from the registry). `samples` and `mode` resolve
 *   independently, each from the first level that sets it (samples: > 0; mode: not Unset). Off at ANY level wins over everything.
 *   Fast turns a count n into max(1, ceil(n / 4)). A chain that sets no sample count at all gives 0, which iterateLights treats as
 *   "skip" (the hider fills the scene level, so a real frame never sees it).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef AREALIGHTCONTROLS_H
#define AREALIGHTCONTROLS_H

#include "libshader/shading/areaLightSampler.h"

namespace openrender::shading {

    struct AreaLightControls {
            int samples;          // the sample count to run (after Fast); 0 when none is set at any level
            AreaLightMode mode;   // Full, Fast or Off (Unset never comes out)
    };

    inline AreaLightControls resolveAreaLightControls(int receiverSamples, AreaLightMode receiverMode, const AreaLightInfo &info) {
        AreaLightControls r;
        r.samples = receiverSamples > 0 ? receiverSamples : (info.lightSamples > 0 ? info.lightSamples : (info.sceneSamples > 0 ? info.sceneSamples : 0));

        if (receiverMode == AreaLightMode::Off || info.lightMode == AreaLightMode::Off || info.sceneMode == AreaLightMode::Off)
            r.mode = AreaLightMode::Off;
        else if (receiverMode != AreaLightMode::Unset)
            r.mode = receiverMode;
        else if (info.lightMode != AreaLightMode::Unset)
            r.mode = info.lightMode;
        else if (info.sceneMode != AreaLightMode::Unset)
            r.mode = info.sceneMode;
        else
            r.mode = AreaLightMode::Full;

        if (r.mode == AreaLightMode::Fast && r.samples > 0)
            r.samples = (r.samples + 3) / 4; // max(1, ceil(n / 4)): n >= 1 here
        return r;
    }

} // namespace openrender::shading

#endif
