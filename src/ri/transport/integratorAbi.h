/**
 * Project: openRender
 *
 * File: integratorAbi.h
 *
 * Description:
 *   The integrator interface (spec 030, FR-010 to FR-016, contracts/integrator-interface.md). Transport domain; strict C17
 *   and C++20, includes only orplugin.h and capability.h. An integrator is a transport strategy a frame is rendered
 *   with; `classic` is the built-in one that leaves the Surface shader to integrate itself. The table here is what a
 *   built-in or a static registrar registers (OR_REGISTER_INTEGRATOR, integratorAbi.hpp); dlopened integrator modules
 *   come with spec 051. Not installed (not in OPENRENDER_PUBLIC_HEADERS).
 *
 *   Lifecycle (host side, per frame; all three on the thread that runs RiWorldBegin/RiWorldEnd, never concurrently):
 *     create -> configure -> ... displays and hiders are built ... -> prepare -> (render) -> finish -> destroy.
 *   configure sets acceptedParameters[i] = 1 for every parameter it defines and accepts, 0 otherwise; the host reports
 *   every 0. A status other than OR_STATUS_OK from configure refuses the frame (the instance is destroyed; prepare and
 *   finish are never called). A status other than OR_STATUS_OK from prepare aborts the frame (the instance is destroyed
 *   without finish). finish is called once for every frame whose prepare returned OK.
 *
 *   Ownership follows orplugin.h: tables, names and capability arrays are static; the settings, host and frame views
 *   are host memory, valid for the call.
 *
 *   Version history (this table also versions the capability vocabulary of capability.h):
 *
 *   | Version | Change |
 *   |---|---|
 *   | 1 | None: no integrator existed; the Surface shader integrated itself. |
 *   | 2.0.0 | Spec 030 (starting version confirmed by the maintainer, 2026-11-07): the table (name, aliases, required and optional capabilities, create/destroy, configure/prepare/finish), the settings, host and frame structs, the capability vocabulary rasterization, display-output, display-channels, deep-shadow-output and illumination-hook at 1.0.0, and the reserved names. Until spec 055 MAJOR stays 2 and 2.x is unstable: a breaking change steps MINOR and says so here. Spec 042 is expected to append `render` and the host services as a MINOR step. |
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef TRANSPORT_INTEGRATORABI_H
#define TRANSPORT_INTEGRATORABI_H

#include "orplugin.h"
#include "transport/capability.h"

#define OR_INTEGRATOR_ABI_VERSION OR_ABI_VERSION(2, 0, 0)

OR_DECLARE_HANDLE(OrIntegratorInstance);

/* 2.0.0: structSize only; spec 042 appends the services facade. */
typedef struct OrTransportHost {
        uint32_t structSize;
} OrTransportHost;

/* 2.0.0: structSize only; spec 042 appends the frame view. */
typedef struct OrIntegratorFrame {
        uint32_t structSize;
} OrIntegratorFrame;

typedef struct OrIntegratorAlias {
        const char *name;
        const char *deprecationWarning; /* NULL: a plain alias; else printed once when the alias is used */
} OrIntegratorAlias;

typedef struct OrIntegratorParameter {
        const char *name;
        int type; /* TYPE_FLOAT, TYPE_INTEGER, TYPE_STRING, TYPE_COLOR, TYPE_POINT, TYPE_VECTOR, TYPE_NORMAL, TYPE_MATRIX */
        int numItems;
        const void *data; /* normalised at statement time: float[numItems] (TYPE_FLOAT), float[numItems*3] (color, point, vector,
                             normal), float[numItems*16] (matrix), int[numItems] (TYPE_INTEGER), const char *[numItems]
                             (TYPE_STRING); host memory, valid during configure */
} OrIntegratorParameter;

typedef struct OrIntegratorSettings {
        uint32_t structSize;
        const char *hiderName;             /* the resolved hider */
        const OrCapability *hiderProvides; /* what that hider provides */
        int numHiderProvides;
        const OrIntegratorParameter *parameters; /* the statement's list, without "handle" */
        int numParameters;
} OrIntegratorSettings;

typedef struct OrIntegratorTable {
        OrTableHeader header;
        const char *name;
        const OrIntegratorAlias *aliases;
        int numAliases;
        const OrCapability *required; /* not "requires": a C++20 keyword, and this header is also compiled as C++20 */
        int numRequired;
        const OrCapability *optional;
        int numOptional;
        OrIntegratorInstance *(*create)(void);
        void (*destroy)(OrIntegratorInstance *);
        /* Lifecycle. Signatures pinned at 2.0.0; later versions add members to the structs they take and append slots to
           this table (structSize-checked), never change these. */
        OrStatus (*configure)(OrIntegratorInstance *, const OrIntegratorSettings *settings, int *acceptedParameters);
        OrStatus (*prepare)(OrIntegratorInstance *, const OrTransportHost *host, const OrIntegratorFrame *frame);
        void (*finish)(OrIntegratorInstance *, const OrTransportHost *host);
} OrIntegratorTable;

#endif
