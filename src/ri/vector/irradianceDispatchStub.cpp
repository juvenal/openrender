/**
 * Project: openRender
 *
 * File: irradianceDispatchStub.cpp
 *
 * Description:
 *   ribVector's counterpart to src/ri/hiders/irradianceDispatch.cpp.
 *   Provides the same two signatures with bodies that never touch
 *   CShadingContext/CTextureLookup, so a consumer that links this file
 *   instead of irradianceDispatch.cpp never needs libshader_shading
 *   resolvable for either.
 *
 *   CIrradianceCache::sample() is safe as a loud, unreachable stub
 *   because orender-wire's actual code paths never call it: the
 *   preview/wireframe path never touches CIrradianceCache at all, and
 *   the data-viewer feature (dataSink.cpp, via CDataDocument::open() in
 *   dataLoad.cpp) only ever reads an already-computed cache through
 *   lookup() -- never samples a new one. Deliberately loud (error() +
 *   assert(FALSE)) rather than a silent no-op, so a wrong assumption
 *   here fails immediately and diagnosably.
 *
 *   irradianceSampleAccept() is different: lookup() genuinely calls it,
 *   for real, every time the data-viewer walks the cache's octree -- so
 *   it can't be a loud stub. Its answer is exact rather than loud
 *   because dataLoad.cpp's CDataDocument::open() only ever opens a
 *   cache with CACHE_READ | CACHE_RDONLY, never CACHE_SAMPLE (see
 *   irradiance.cpp: smallSampleWeight is 0 unless CACHE_SAMPLE is set),
 *   and the real body's `w > context->urand() * smallSampleWeight` is
 *   `w > 0` whenever smallSampleWeight is exactly 0, regardless of
 *   urand()'s value -- so skipping the RNG call changes nothing this
 *   build can ever observe. The assert documents (and would catch) that
 *   invariant being violated, rather than silently computing the wrong
 *   answer if it ever is.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cassert>

#include "error.h"
#include "irradiance.h"

void CIrradianceCache::sample(float *, const float *, const float *, const float *, const float *, CShadingContext *) {
    error(CODE_BUG, "CIrradianceCache::sample() reached in a build with no rendering pipeline\n");
    assert(FALSE);
}

bool irradianceSampleAccept(float w, float smallSampleWeight, CShadingContext *) {
    assert(smallSampleWeight == 0.0f);
    return w > 0.0f;
}
