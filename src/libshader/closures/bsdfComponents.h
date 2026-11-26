/**
 * Project: openRender
 *
 * File: bsdfComponents.h
 *
 * Description:
 *   The operations of the five built-in BSDF closures (spec 035, contracts/closure-abi.md "BSDF table"): diffuse, oren_nayar, microfacet,
 *   reflection and refraction. Internal to Shading. Each closure's OrBsdfComponentOps works in the component's LOCAL frame, whose z axis is the
 *   component's own normal; the tree-level table (bsdf.cpp) converts the query into it. The registry fills its entries' `ops` from here.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef BSDFCOMPONENTS_H
#define BSDFCOMPONENTS_H

#include <stdint.h>

#include "closureAbi.h"

namespace openrender::shading {

    // The operations of a built-in BSDF closure (id 0 to 4), nullptr for any other id
    const OrBsdfComponentOps *bsdfComponentOps(int32_t builtinId);

    // The kind flags of a lobe of closure `id` with the parameter block `block` (OR_BSDF_REFLECTION or TRANSMISSION, and DIFFUSE, GLOSSY or
    // SPECULAR), and whether it is a delta lobe (a mirror, a refraction, a microfacet below kMicrofacetDeltaAlpha). 0 for a closure that is not a
    // BSDF built-in.
    uint32_t bsdfLobeKind(int32_t id, const void *block, bool *isDelta);

    // The floor of a lobe's selection estimate: the albedo() of a component is never below it, so a lobe that contributes is always chosen with
    // a positive probability
    inline constexpr float kBsdfAlbedoFloor = 0.01f;

} // namespace openrender::shading

#endif
