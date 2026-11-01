/**
 * Project: openRender
 *
 * File: dynload_fixture.c
 *
 * Description:
 *   Spec 037 test fixture: a RenderMan DynamicLoad generator
 *   (Procedural "DynamicLoad" [<module> <arguments>]).
 *
 *   ConvertParameters, Subdivide and Free log their calls through pluginLog.h; the unload
 *   of the module image is logged by a destructor function. Variants:
 *     plain    no version symbol (TEST_NO_VERSION; accepted silently, FR-009); Subdivide emits one RiSphere
 *     versioned  declares 1.0.0
 *     v2       declares 2.0.0 (rejected)
 *     nested   (no version) Subdivide emits an RiProcedural whose subdivide and free functions live in
 *              this module (FR-034: the module must outlive the geometry that calls them);
 *              those two log too, and emit one RiSphere
 *
 *   The module calls back into the host (RiSphere, RiProcedural). It links nothing: the
 *   symbols resolve against the process that loads it (-undefined dynamic_lookup on macOS),
 *   so there is only the one copy of the renderer's process-wide state. C, no thread-local
 *   storage, no static object with a destructor.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "dlo.h" /* OR_DYNAMICLOAD_DECLARE_VERSION lives in dlo.h (research Q3) */
#include "pluginLog.h"
#include "ri.h"

#if defined(TEST_ABI_MAJOR)
/* A variant that declares another version (TEST_ABI_MAJOR / TEST_ABI_MINOR): v2. */
OR_DECLARE_ABI_VERSION_SYMBOL(orDynamicLoadAbiVersion, OR_ABI_VERSION(TEST_ABI_MAJOR, TEST_ABI_MINOR, 0))
#elif defined(OR_DYNAMICLOAD_DECLARE_VERSION) && !defined(TEST_NO_VERSION)
OR_DYNAMICLOAD_DECLARE_VERSION()
#endif

RtPointer ConvertParameters(char *parameters) {
    (void)parameters;
    OR_TEST_LOG("ConvertParameters");
    return (RtPointer)0;
}

#ifdef TEST_NESTED

static RtVoid nestedSubdivide(void *data, RtFloat detail) {
    (void)data;
    (void)detail;
    OR_TEST_LOG("nestedSubdivide");
    RiSphere(0.5f, -0.5f, 0.5f, 360.0f, RI_NULL);
}

static RtVoid nestedFree(void *data) {
    (void)data;
    OR_TEST_LOG("nestedFree");
}

void Subdivide(RtPointer data, RtFloat detail) {
    static RtBound bound = {-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f};
    (void)data;
    (void)detail;
    OR_TEST_LOG("Subdivide");
    RiProcedural((void *)0, bound, nestedSubdivide, nestedFree);
}

#else

void Subdivide(RtPointer data, RtFloat detail) {
    (void)data;
    (void)detail;
    OR_TEST_LOG("Subdivide");
    RiSphere(0.5f, -0.5f, 0.5f, 360.0f, RI_NULL);
}

#endif

void Free(RtPointer data) {
    (void)data;
    OR_TEST_LOG("Free");
}

OR_TEST_LOG_UNLOAD()
