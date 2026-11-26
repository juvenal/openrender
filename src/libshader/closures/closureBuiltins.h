/**
 * Project: openRender
 *
 * File: closureBuiltins.h
 *
 * Description:
 *   The eight built-in closures of spec 035 (contracts/closure-abi.md "Registry", data-model.md "Parameter blocks"): their fixed ids and
 *   their parameter blocks as C++ structs. Internal to Shading (not installed). The registry fills its entries from these structs
 *   (sizeof and offsetof), the BSDF components of Phase 2 read their blocks through them, and the tests include this header instead of
 *   keeping copies, so the layout is defined once.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSUREBUILTINS_H
#define CLOSUREBUILTINS_H

#include <stddef.h>
#include <stdint.h>

namespace openrender::shading {

    // The fixed ids of the built-ins (the registry assigns the next id to a later registration)
    enum ClosureBuiltinId : int32_t {
        kClosureDiffuse = 0,
        kClosureOrenNayar = 1,
        kClosureMicrofacet = 2,
        kClosureReflection = 3,
        kClosureRefraction = 4,
        kClosureTransparent = 5,
        kClosureHoldout = 6,
        kClosureUniformEdf = 7,
        kNumClosureBuiltins = 8
    };

    // The value of microfacet's `refract` member: 0 reflection only, 1 transmission through the surface, 2 both (to be verified against stdosl.h)
    enum ClosureMicrofacetRefract : int32_t { kMicrofacetReflect = 0,
                                              kMicrofacetRefract = 1,
                                              kMicrofacetBoth = 2 };

    // The parameter blocks. `N` is the component's own normal (research R7); `eta` is the index of refraction, 0 meaning "no Fresnel term"
    // for a microfacet reflection. transparent and holdout have no block.
    struct ClosureDiffuseBlock {
            float N[3];
    };
    struct ClosureOrenNayarBlock {
            float N[3];
            float sigma;
    };
    struct ClosureMicrofacetBlock {
            const char *distribution; // a registry-interned string: "ggx" or "beckmann"
            float N[3];
            float alpha;
            float eta;
            int32_t refract;
    };
    struct ClosureReflectionBlock {
            float N[3];
            float eta;
    };
    struct ClosureRefractionBlock {
            float N[3];
            float eta;
    };
    struct ClosureUniformEdfBlock {
            float emittance[3];
    };

    static_assert(sizeof(ClosureDiffuseBlock) == 12 && alignof(ClosureDiffuseBlock) == 4, "diffuse: 12, 4");
    static_assert(sizeof(ClosureOrenNayarBlock) == 16 && alignof(ClosureOrenNayarBlock) == 4, "oren_nayar: 16, 4");
    static_assert(sizeof(ClosureReflectionBlock) == 16 && sizeof(ClosureRefractionBlock) == 16, "reflection, refraction: 16");
    static_assert(sizeof(ClosureUniformEdfBlock) == 12 && alignof(ClosureUniformEdfBlock) == 4, "uniform_edf: 12, 4");
#if defined(__LP64__) || defined(_LP64)
    static_assert(sizeof(ClosureMicrofacetBlock) == 32 && alignof(ClosureMicrofacetBlock) == 8, "microfacet: 32, 8 (LP64)");
    static_assert(offsetof(ClosureMicrofacetBlock, N) == 8 && offsetof(ClosureMicrofacetBlock, alpha) == 20 && offsetof(ClosureMicrofacetBlock, eta) == 24 &&
                      offsetof(ClosureMicrofacetBlock, refract) == 28,
                  "microfacet offsets (LP64)");
#endif

} // namespace openrender::shading

#endif
