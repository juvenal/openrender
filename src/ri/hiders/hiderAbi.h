/**
 * Project: openRender
 *
 * File: hiderAbi.h
 *
 * Description:
 *   The hider registration interface (spec 030, FR-001 to FR-008, contracts/hider-registration.md). Hiders domain; strict C17
 *   and C++20, includes only orplugin.h and transport/capability.h (the first Hiders -> Transport include, allowed by
 *   constitution 1.4.0). A table says WHAT a hider kind is (its name and aliases, the capabilities it provides, which
 *   dispatcher drives it) and HOW to build one (create, destroy); the hider's runtime (CHider, renderingLoop, the tracer and
 *   producer wiring) stays internal C++ and is not external until spec 051. Not installed (not in OPENRENDER_PUBLIC_HEADERS).
 *
 *   Ownership follows orplugin.h: the table, its strings and arrays are static and outlive every call; create allocates and
 *   destroy frees. `create` returns NULL on failure, which aborts the frame.
 *
 *   Version history:
 *
 *   | Version | Change |
 *   |---|---|
 *   | 1 | None: hiders were chosen by strcmp chains in the renderer core; no table, unversioned. |
 *   | 2.0.0 | Spec 030 (starting version confirmed by the maintainer, 2026-11-07): the table (name, aliases, provided capabilities, dispatch kind, optional preDisplaySetup, create/destroy), the reserved pair field of an alias. Until spec 055 MAJOR stays 2 and 2.x is unstable: a breaking change steps MINOR and says so here. |
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef HIDERABI_H
#define HIDERABI_H

#include "orplugin.h"
#include "transport/capability.h"

#define OR_HIDER_ABI_VERSION OR_ABI_VERSION(2, 0, 0)

OR_DECLARE_HANDLE(OrHiderInstance); /* opaque: the host never dereferences it */
OR_DECLARE_HANDLE(OrHiderFrame);    /* opaque: host data valid for the create call */

typedef struct OrHiderAlias {
        const char *name;               /* non-empty, unique across all names and aliases */
        const char *deprecationWarning; /* NULL: plain alias; else the exact warning text, reported once per statement */
        const char *integrator;         /* reserved: must be NULL in 2.0.0 (a non-NULL value is warned about and ignored) */
} OrHiderAlias;

typedef enum OrHiderDispatch {
    OR_HIDER_DISPATCH_BUCKETS = 0, /* the core's bucket dispatcher */
    OR_HIDER_DISPATCH_PHOTONS = 1  /* the core's photon-bundle dispatcher */
} OrHiderDispatch;

typedef struct OrHiderCreateInfo {
        uint32_t structSize;
        int thread; /* 0 .. numThreads-1 */
        int numThreads;
        const OrHiderFrame *frame; /* valid until create returns */
} OrHiderCreateInfo;

typedef struct OrHiderTable {
        OrTableHeader header;        /* structSize = sizeof(OrHiderTable), abiVersion = OR_HIDER_ABI_VERSION */
        const char *name;            /* canonical name, non-empty */
        const OrHiderAlias *aliases; /* may be NULL when numAliases == 0 */
        int numAliases;
        const OrCapability *provides; /* the capabilities this hider kind provides */
        int numProvides;
        OrHiderDispatch dispatch;
        OrStatus (*preDisplaySetup)(void);                         /* optional (NULL): run once per frame before displays are set up */
        OrHiderInstance *(*create)(const OrHiderCreateInfo *info); /* one per render thread; NULL on failure */
        void (*destroy)(OrHiderInstance *instance);                /* called by the core in endFrame, single-threaded */
} OrHiderTable;

#endif
