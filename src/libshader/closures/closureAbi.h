/**
 * Project: openRender
 *
 * File: closureAbi.h
 *
 * Description:
 *   The closure ABI (spec 035, contracts/closure-abi.md): the closure tree's three node structs, the closure registry entry, and
 *   the BSDF table. Strict C17 and C++20: it includes only <stdint.h>, <stddef.h> and orplugin.h. It is the interface a closure
 *   component or a Bxdf plug-in is written against, and it is NOT installed (not in OPENRENDER_PUBLIC_HEADERS): it stays internal until
 *   specs 051/052. The C++ views are closureTree.h, closureRegistry.h and bsdf.hpp.
 *
 *   Version history (the single source of the numbers; DEVNOTES_DETAILS/CLOSURES.md and the site quote it):
 *
 *   | Version | Change |
 *   |---|---|
 *   | 1.0.0 | Spec 035. The node layout (component, multiply, add), the registry entry shape, the eight built-in closures and their parameter blocks,
 *   |       | and the OrBsdfTable conventions. New in the path-tracing milestone; it starts at 1.0.0 (maintainer, 2026-11-23). The MAJOR-2 rule of
 *   |       | hiderAbi.h and kRendererServicesVersion covers interfaces that predate the milestone, not this one. 1.x is unstable until the milestone
 *   |       | closes: until then a breaking change steps MINOR and says so here; after it, only a real breaking change steps MAJOR, to 2.0.0. |
 *
 *   This version moves independently of the RSL language version and of kRendererServicesVersion (D22). It versions the node structs, the
 *   registry entry and the built-in parameter blocks, and the BSDF table's conventions: a change to any of them is a step of this version.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSUREABI_H
#define CLOSUREABI_H

#include <stddef.h>
#include <stdint.h>

#include "orplugin.h"

#define OR_CLOSURE_ABI_VERSION OR_ABI_VERSION(1, 0, 0)

/* A 16-byte alignment on a struct's first member: C17 does not allow an alignment specifier on a struct tag */
#ifdef __cplusplus
#define OR_ALIGNAS16 alignas(16)
#else
#define OR_ALIGNAS16 _Alignas(16)
#endif

/* ---- the nodes (pinned on LP64 by the assertions at the end) ---- */

enum { OR_CLOSURE_MUL = -1,
       OR_CLOSURE_ADD = -2 }; /* id >= 0: a registered component */

typedef struct OrClosureNode {
        int32_t id;
} OrClosureNode;

typedef struct OrClosureComponent {
        OR_ALIGNAS16 int32_t id; /* registered closure id; the member alignment makes the struct 16-aligned */
        float w[3];              /* weight, three channels (D17, D18) */
                                 /* the parameter block follows: (const char *)(c + 1) */
} OrClosureComponent;

typedef struct OrClosureMul {
        int32_t id;
        float weight[3];
        const OrClosureNode *closure;
} OrClosureMul;

typedef struct OrClosureAdd {
        int32_t id;
        const OrClosureNode *closureA;
        const OrClosureNode *closureB;
} OrClosureAdd;

/* ---- the registry entry ---- */

struct OrBsdfComponentOps;

typedef enum OrClosureCategory { OR_CLOSURE_BSDF = 0,
                                 OR_CLOSURE_EDF = 1,
                                 OR_CLOSURE_VDF = 2,
                                 OR_CLOSURE_SIGNAL = 3 } OrClosureCategory;

typedef enum OrClosureParamType { OR_CPARAM_FLOAT,
                                  OR_CPARAM_INT,
                                  OR_CPARAM_COLOR,
                                  OR_CPARAM_NORMAL,
                                  OR_CPARAM_VECTOR,
                                  OR_CPARAM_STRING } OrClosureParamType;

typedef struct OrClosureParam {
        const char *name;
        OrClosureParamType type;
        uint32_t offset;
} OrClosureParam;

typedef struct OrClosureEntry {
        uint32_t structSize;
        int32_t id; /* assigned by the registry; ignored on registration */
        const char *name;
        OrClosureCategory category;
        const OrClosureParam *params;
        int numParams;
        uint32_t blockSize, blockAlign;
        const struct OrBsdfComponentOps *ops; /* BSDF: required; others: NULL */
} OrClosureEntry;

/* ---- the BSDF table (declared now so that Phase 2 only adds code) ---- */

typedef enum OrTransportMode { OR_TRANSPORT_RADIANCE = 0,
                               OR_TRANSPORT_IMPORTANCE = 1 } OrTransportMode;

enum {
    OR_BSDF_REFLECTION = 1u << 0,
    OR_BSDF_TRANSMISSION = 1u << 1,
    OR_BSDF_DIFFUSE = 1u << 2,
    OR_BSDF_GLOSSY = 1u << 3,
    OR_BSDF_SPECULAR = 1u << 4,
    OR_BSDF_DELTA = 1u << 5,
    OR_BSDF_HAS_DELTA = 1u << 6,
    OR_BSDF_NOT_SCATTERER = 1u << 7,
    OR_BSDF_FAILED = 1u << 8,
    OR_BSDF_ALL = 0x1Fu
};

#define OR_SPECTRUM_MAX 4 /* storage bound of the C answer; numChannels says how many are used (3 now) */

typedef struct OrBsdfQuery {
        uint32_t structSize;
        float wo[3], wi[3];
        float Ng[3];
        OrTransportMode mode;
        uint32_t requested;
} OrBsdfQuery;

typedef struct OrBsdfEval {
        float f[OR_SPECTRUM_MAX];
        int32_t numChannels;
        float pdfForward, pdfReverse;
        uint32_t flags;
} OrBsdfEval;

typedef struct OrBsdfSample {
        float f[OR_SPECTRUM_MAX];
        int32_t numChannels;
        float pdfForward, pdfReverse;
        uint32_t flags;
        float wi[3];
        float eta;
} OrBsdfSample;

OR_DECLARE_HANDLE(OrBsdf);

typedef struct OrClosureAllocator {
        void *context;
        void *(*allocate)(void *context, size_t size, size_t align);
} OrClosureAllocator;

typedef struct OrShadingFrame {
        float N[3];
        float dPdu[3];
        float I[3];
} OrShadingFrame;

typedef struct OrBsdfTable {
        OrTableHeader header; /* structSize = sizeof(OrBsdfTable), abiVersion = OR_CLOSURE_ABI_VERSION */
        OrStatus (*prepare)(const OrClosureNode *tree, const OrShadingFrame *frame, const OrClosureAllocator *alloc, OrBsdf **out);
        OrStatus (*evaluate)(const OrBsdf *bsdf, const OrBsdfQuery *q, OrBsdfEval *out);
        OrStatus (*sample)(const OrBsdf *bsdf, const OrBsdfQuery *q, float u1, float u2, float uLobe, OrBsdfSample *out);
        OrStatus (*pdf)(const OrBsdf *bsdf, const OrBsdfQuery *q, OrBsdfEval *out); /* f left 0 */
} OrBsdfTable;

typedef struct OrBsdfComponentOps { /* one per BSDF closure; block = the component's parameter block */
        void (*evaluate)(const void *block, const float wo[3], const float wi[3], OrTransportMode, OrBsdfEval *out);
        void (*sample)(const void *block, const float wo[3], float u1, float u2, OrTransportMode, OrBsdfSample *out);
        void (*pdf)(const void *block, const float wo[3], const float wi[3], OrTransportMode, OrBsdfEval *out);
        float (*albedo)(const void *block, const float wo[3]); /* lobe-selection estimate in [0, 1] */
} OrBsdfComponentOps;

OR_EXTERN_C_BEGIN
const OrBsdfTable *orBsdfTable(void); /* the host's table (defined in Phase 2) */
OR_EXTERN_C_END

/* ---- the layout, pinned on LP64 (Closure_Layout prints the numbers for spec 049's test; the compatibility figure is the block offset 16) ---- */
#if defined(__LP64__) || defined(_LP64)
#ifdef __cplusplus
static_assert(sizeof(OrClosureComponent) == 16 && alignof(OrClosureComponent) == 16, "OrClosureComponent: 16 bytes, 16-aligned");
static_assert(offsetof(OrClosureComponent, id) == 0 && offsetof(OrClosureComponent, w) == 4, "OrClosureComponent: id 0, w 4");
static_assert(sizeof(OrClosureMul) == 24 && alignof(OrClosureMul) == 8, "OrClosureMul: 24 bytes, 8-aligned");
static_assert(offsetof(OrClosureMul, weight) == 4 && offsetof(OrClosureMul, closure) == 16, "OrClosureMul: weight 4, closure 16");
static_assert(sizeof(OrClosureAdd) == 24 && alignof(OrClosureAdd) == 8, "OrClosureAdd: 24 bytes, 8-aligned");
static_assert(offsetof(OrClosureAdd, closureA) == 8 && offsetof(OrClosureAdd, closureB) == 16, "OrClosureAdd: closureA 8, closureB 16");
#else
_Static_assert(sizeof(OrClosureComponent) == 16 && _Alignof(OrClosureComponent) == 16, "OrClosureComponent: 16 bytes, 16-aligned");
_Static_assert(offsetof(OrClosureComponent, id) == 0 && offsetof(OrClosureComponent, w) == 4, "OrClosureComponent: id 0, w 4");
_Static_assert(sizeof(OrClosureMul) == 24 && _Alignof(OrClosureMul) == 8, "OrClosureMul: 24 bytes, 8-aligned");
_Static_assert(offsetof(OrClosureMul, weight) == 4 && offsetof(OrClosureMul, closure) == 16, "OrClosureMul: weight 4, closure 16");
_Static_assert(sizeof(OrClosureAdd) == 24 && _Alignof(OrClosureAdd) == 8, "OrClosureAdd: 24 bytes, 8-aligned");
_Static_assert(offsetof(OrClosureAdd, closureA) == 8 && offsetof(OrClosureAdd, closureB) == 16, "OrClosureAdd: closureA 8, closureB 16");
#endif
#endif

#endif
