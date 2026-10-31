/**
 * tests/shading_contract/load_stub.cpp
 *
 * The load-time stub of the standalone contract program (spec 026, FR-024; contracts/test-doubles.md).
 *
 * libshader_shading still names a few Hiders symbols (research R10): the bundle classes
 * `trace()`/`transmission()`/`gather()` construct, the photon map lookup, and the two trace members
 * of CShadingContext. On macOS the chained fixups bind every one of them when the library loads, so
 * a process without libRIBRender cannot start unless something defines them. This file defines
 * them, and every body traps: a test that reaches a stub fails at once and never hangs (a message
 * to stderr, then std::abort()).
 *
 * The table below is compared with tools/libshader-imports-base-{macos,linux}.txt by tasks T047 and
 * T076:
 *
 *   vtable for CTraceBundle          CTraceBundle::postTraceAction() (key function), postShade x2, post()
 *   vtable for CTransmissionBundle   the same four members
 *   CGatherBundle::CGatherBundle()   the constructor, ~CGatherBundle() and its four virtuals
 *   CPhotonMap::lookup(float*, const float*, int)
 *   CShadingContext::trace(CRay*)           TEMPORARY (QC): deleted in Phase 8 (6 symbols -> 4)
 *   CShadingContext::traceEx(CRayBundle*)   TEMPORARY (QC): deleted in Phase 8
 *
 * Compiled into the standalone executable only. Deleted by spec 029, which removes the imports.
 */
// The standard headers come first: common/algebra.h #defines atan2f and friends, which breaks any
// libc++ header included after it.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "ri/hiders/bundles.h"
#include "ri/hiders/photonMap.h"

[[noreturn]] static void stubTrap(const char *what) {
    fprintf(stderr, "load_stub: %s reached; this path needs ribRender (spec 026, FR-024)\n", what);
    abort();
}

// ---- CTraceBundle -----------------------------------------------------------------------------------
int CTraceBundle::postTraceAction() { stubTrap("CTraceBundle::postTraceAction"); }
void CTraceBundle::postShade(int, CRay **, float **) { stubTrap("CTraceBundle::postShade(int, CRay **, float **)"); }
void CTraceBundle::postShade(int, CRay **) { stubTrap("CTraceBundle::postShade(int, CRay **)"); }
void CTraceBundle::post() { stubTrap("CTraceBundle::post"); }

// ---- CTransmissionBundle ----------------------------------------------------------------------------
int CTransmissionBundle::postTraceAction() { stubTrap("CTransmissionBundle::postTraceAction"); }
void CTransmissionBundle::postShade(int, CRay **, float **) { stubTrap("CTransmissionBundle::postShade(int, CRay **, float **)"); }
void CTransmissionBundle::postShade(int, CRay **) { stubTrap("CTransmissionBundle::postShade(int, CRay **)"); }
void CTransmissionBundle::post() { stubTrap("CTransmissionBundle::post"); }

// ---- CGatherBundle ----------------------------------------------------------------------------------
CGatherBundle::CGatherBundle() { stubTrap("CGatherBundle::CGatherBundle"); }
CGatherBundle::~CGatherBundle() { stubTrap("CGatherBundle::~CGatherBundle"); }
int CGatherBundle::postTraceAction() { stubTrap("CGatherBundle::postTraceAction"); }
void CGatherBundle::postShade(int, CRay **, float **) { stubTrap("CGatherBundle::postShade(int, CRay **, float **)"); }
void CGatherBundle::postShade(int, CRay **) { stubTrap("CGatherBundle::postShade(int, CRay **)"); }
void CGatherBundle::post() { stubTrap("CGatherBundle::post"); }

// ---- CPhotonMap -------------------------------------------------------------------------------------
void CPhotonMap::lookup(float *, const float *, int) { stubTrap("CPhotonMap::lookup(float *, const float *, int)"); }

// ======================================================================================================
// TEMPORARY (spec 026, QC): deleted in Phase 8, when the trace calls go through ITracer.
// The starting tree still imports these two (6 imports, not the 4 of contracts/test-doubles.md).
// ======================================================================================================
void CShadingContext::trace(CRay *) { stubTrap("CShadingContext::trace(CRay *)"); }
void CShadingContext::traceEx(CRayBundle *) { stubTrap("CShadingContext::traceEx(CRayBundle *)"); }
// ======================================================================================================
// end of the TEMPORARY block
// ======================================================================================================
