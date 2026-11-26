/**
 * Project: openRender
 *
 * File: bxdfAbi.h
 *
 * Description:
 *   The Bxdf table (spec 035, contracts/bxdf.md): a Bxdf is a named, typed, range-checked parameter list and a declarative `build` that writes
 *   the closure tree of one shading point into the batch's arena. Strict C17 and C++20: it includes only <stdint.h>, orplugin.h and
 *   closureAbi.h. It is NOT installed (not in OPENRENDER_PUBLIC_HEADERS): it stays internal until specs 051/052, like hiderAbi.h. The built-in
 *   Bxdfs register through it, and specs 036 and 049 register a table whose `build` runs a shader.
 *
 *   Ownership follows orplugin.h: the table and its strings are static; `create` allocates and `destroy` frees; `build` allocates only through
 *   the allocator it is given (the batch arena), so a tree lives as long as the batch. `build` is a pure function of `data` and `point`
 *   (trees from equal inputs are bit-identical, FR-028) and is declarative: no ray, no light, no illuminance, gather or occlusion (FR-015).
 *
 *   Version history (the single source of the numbers; DEVNOTES_DETAILS/CLOSURES.md and the site quote it):
 *
 *   | Version | Change |
 *   |---|---|
 *   | 1.0.0 | Spec 035. The table (name, typed parameters with ranges, create/destroy, build), the point record and the allocator. Built-ins register
 *   |       | through it (D8). New in the path-tracing milestone; it starts at 1.0.0 (maintainer, 2026-11-23). 1.x is unstable until the milestone
 *   |       | closes: until then a breaking change steps MINOR and says so here; after it, only a real breaking change steps MAJOR. |
 *
 *   This version moves independently of the closure ABI (closureAbi.h), the RSL language version and kRendererServicesVersion.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef BXDFABI_H
#define BXDFABI_H

#include <stdint.h>

#include "closureAbi.h"
#include "orplugin.h"

#define OR_BXDF_ABI_VERSION OR_ABI_VERSION(1, 0, 0)

OR_DECLARE_HANDLE(OrBxdfData);

typedef enum OrBxdfParamType { OR_BXDF_FLOAT,
                               OR_BXDF_INT,
                               OR_BXDF_COLOR,
                               OR_BXDF_STRING } OrBxdfParamType;

/* What the host does with a value outside [minValue, maxValue]: clamp it (one diagnostic naming the parameter) or refuse the statement */
typedef enum OrBxdfRange { OR_BXDF_CLAMP = 0,
                           OR_BXDF_REFUSE = 1 } OrBxdfRange;

typedef struct OrBxdfParam {
        const char *name;
        OrBxdfParamType type;
        float defaultValue[3];    /* for FLOAT/INT/COLOR */
        float minValue, maxValue; /* per channel; maxValue < minValue: no upper bound */
        OrBxdfRange onOutOfRange;
        const char *doc;
} OrBxdfParam;

/* RGB-3 until the spectral spec: the fixed float[3] fields (Cs, Os, defaultValue) carry no channel count. Point inputs. Cs and Os are the
   point's colour and opacity (the primitive's, else the attributes'): RISpec 3.2 section 4.2.1. N is faceforward(normalize(N), I), in camera
   space, the shading space of every vector in a tree. */
typedef struct OrBxdfPoint {
        uint32_t structSize;
        float P[3], N[3], Ng[3], dPdu[3], dPdv[3], I[3];
        float u, v;
        float Cs[3], Os[3];
} OrBxdfPoint;

typedef struct OrBxdfDiagnostics {
        void *context;
        void (*report)(void *context, int code, const char *message);
} OrBxdfDiagnostics;

typedef struct OrBxdfTable {
        OrTableHeader header; /* structSize = sizeof(OrBxdfTable), abiVersion = OR_BXDF_ABI_VERSION */
        const char *name;
        const OrBxdfParam *params;
        int numParams;
        /* `values` has one entry per parameter, in table order: already typed, defaulted and range-handled by the host (a float, int or color
           parameter points at 1 or 3 floats; a string parameter at a `const char *`). Returns NULL to refuse. */
        OrBxdfData *(*create)(const void *const *values, const OrBxdfDiagnostics *diag);
        void (*destroy)(OrBxdfData *data);
        /* Builds the point's tree in `alloc`; NULL is the empty tree. Declarative (FR-015, D15). */
        const OrClosureNode *(*build)(const OrBxdfData *data, const OrBxdfPoint *point, const OrClosureAllocator *alloc);
} OrBxdfTable;

#endif
