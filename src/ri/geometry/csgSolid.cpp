/**
 * Project: openRender
 *
 * File: csgSolid.cpp
 *
 * Description:
 *   The renderable CSG solid: one placement or instance of a shared
 *   definition, resolved lazily on first use (spec 021).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "csgSolid.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "atomic.h"
#include "csgCaps.h"
#include "delayed.h"
#include "common/diagnostics.h"
#include "patches.h"
#include "polygons.h"
#include "riInterface.h"
#include "stats.h"
#include "subdivisionCreator.h"
#include "subdivisionLoop.h"
#include "xform.h"

// The two-sided offset is this fraction of the solid's bound diagonal, but
// never below a few float steps of its coordinates (a distant solid's offset
// would otherwise round away)
static const float kEpsilonFraction = C_EPSILON;
static const float kEpsilonUlps = 8.0f;

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGInstanceState
// Method				:	CCSGInstanceState / ~CCSGInstanceState
// Description			:	Ctor / dtor
// Return Value			:	-
// Comments				:
CCSGInstanceState::CCSGInstanceState() : evaluator(NULL), definition(NULL), passthrough(-1) {}

CCSGInstanceState::~CCSGInstanceState() {
    frameEnded();
    for (CCSGRegion *r : regions)
        delete r;
    for (CRegionPredicate *r : capRegions)
        delete r;
    delete evaluator;
    for (CCSGResolvedOperand *op : operands)
        delete op;
    if (definition != NULL)
        definition->detach();
}

void CCSGInstanceState::hold(CObject *o) {
    o->attach();
    held.push_back(o);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGInstanceState
// Method				:	frameEnded
// Description			:	Rendering is over: release the members and surfaces
// Return Value			:	-
// Comments				:	Called by the host before the scene hierarchy is
//							freed. Objects the hiders consumed go now; objects
//							in the raytracing hierarchy keep its reference.
void CCSGInstanceState::frameEnded() {
    for (CObject *o : held)
        o->detach();
    held.clear();
}

// Can this member take part in a solid (FR-018)?
static int supportedMember(const CObject *o) {
    if (!o->honorsRegion())
        return FALSE;
    return dynamic_cast<const CSurface *>(o) != NULL || dynamic_cast<const CPolygonMesh *>(o) != NULL ||
           dynamic_cast<const CPatchMesh *>(o) != NULL || dynamic_cast<const CNURBSPatchMesh *>(o) != NULL ||
           dynamic_cast<const CSubdivMesh *>(o) != NULL || dynamic_cast<const CLoopSubdivMesh *>(o) != NULL ||
           dynamic_cast<const CDelayedObject *>(o) != NULL;
}

// Materialize o and everything below it, depth first; collect the leaf surfaces
static void materializeSurfaces(CShadingContext *context, CObject *o, std::vector<CSurface *> &surfaces) {
    o->materialize(context);
    if (o->children == NULL) {
        CSurface *s = dynamic_cast<CSurface *>(o);
        if (s != NULL)
            surfaces.push_back(s);
        return;
    }
    for (CObject *c = o->children; c != NULL; c = c->sibling)
        materializeSurfaces(context, c, surfaces);
}

static void clearRegion(CObject *o) {
    o->region = NULL;
    for (CObject *c = o->children; c != NULL; c = c->sibling)
        clearRegion(c);
}

// Does a subtree yield nothing? (FR-025a: emptiness propagates upwards)
static int subtreeEmpty(const CCSGDefinition *def, const std::vector<CCSGResolvedOperand *> &ops, int node) {
    const CCSGDefinition::Node &n = def->nodes[node];
    if (n.operand >= 0)
        return ops[n.operand]->kind == CSG_OPERAND_EMPTY || ops[n.operand]->kind == CSG_OPERAND_OPEN;
    if (n.children.empty())
        return TRUE;
    switch (n.operation) {
    case CSG_UNION:
        for (int c : n.children)
            if (!subtreeEmpty(def, ops, c))
                return FALSE;
        return TRUE;
    case CSG_INTERSECTION:
        for (int c : n.children)
            if (subtreeEmpty(def, ops, c))
                return TRUE;
        return FALSE;
    case CSG_DIFFERENCE:
        return subtreeEmpty(def, ops, n.children[0]);
    default:
        return subtreeEmpty(def, ops, n.children[0]);
    }
}

// The operands that still contribute once empty subtrees are removed
static void contributingOperands(const CCSGDefinition *def, const std::vector<CCSGResolvedOperand *> &ops, int node,
                                 std::vector<int> &out) {
    if (subtreeEmpty(def, ops, node))
        return;
    const CCSGDefinition::Node &n = def->nodes[node];
    if (n.operand >= 0) {
        out.push_back(n.operand);
        return;
    }
    for (int c : n.children)
        contributingOperands(def, ops, c, out);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGSolid
// Method				:	CCSGSolid / ~CCSGSolid
// Description			:	Ctor / dtor
// Return Value			:	-
// Comments				:	Bounded by the definition's template bound placed
//							by this solid's own transform
CCSGSolid::CCSGSolid(CAttributes *a, CXform *x, CCSGDefinition *d) : CObject(a, x), definition(d), state(NULL) {
    definition->attach();
    movvv(bmin, definition->bmin);
    movvv(bmax, definition->bmax);
    if (bmin[0] <= bmax[0] && bmin[1] <= bmax[1] && bmin[2] <= bmax[2])
        xform->transformBound(bmin, bmax);
}

CCSGSolid::~CCSGSolid() {
    CCSGInstanceState *s = state.load();
    if (s != NULL)
        s->detach();
    definition->detach();
}

void CCSGSolid::intersect(CShadingContext *context, CRay *) {
    resolve(context);
}

// CCSGSolid::dice() lives in render/geometryDispatch.cpp with the other
// CReyes-facing geometry entry points, so this unit names no hider type

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGSolid
// Method				:	instantiate
// Description			:	A new solid over the same definition, placed by the
//							composed transform (ObjectInstance)
// Return Value			:	-
// Comments				:
void CCSGSolid::instantiate(CAttributes *a, CXform *x, CRiInterface *cx) const {
    CXform *nx = new CXform(x);
    nx->concat(xform);
    cx->addObject(new CCSGSolid(a != NULL ? a : attributes, nx, definition));
    stats.csgFrameHasSolid = TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGSolid
// Method				:	resolve
// Description			:	Build the instance state and children once
// Return Value			:	-
// Comments				:
void CCSGSolid::resolve(CShadingContext *context) {
    if (state.load(std::memory_order_acquire) != NULL)
        return;
    std::lock_guard<std::mutex> lock(resolveMutex);
    if (state.load(std::memory_order_relaxed) != NULL)
        return;

    CCSGInstanceState *s = build(context);
    s->attach();
    CGeometryHost *host = geometryHost();
    if (host != NULL)
        host->releaseAtFrameEnd(s);
    state.store(s, std::memory_order_release);
    atomicIncrement(stats.numCsgInstancesResolved);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGSolid
// Method				:	build
// Description			:	The per-instance resolution (research.md Decision
//							9): drop unsupported members, capture flagged
//							instances of the template leaves through the
//							geometry host, attach the regions, materialize,
//							collect surfaces, derive the operand kinds, and
//							keep only the operands that contribute
// Return Value			:	The new state
// Comments				:	Runs under resolveMutex. Never calls the renderer
//							core: the host does (constitution Principle VIII).
CCSGInstanceState *CCSGSolid::build(CShadingContext *context) {
    CCSGInstanceState *s = new CCSGInstanceState();
    s->definition = definition;
    definition->attach();
    CGeometryHost *host = geometryHost();
    const int numOperands = definition->numOperands();
    for (int op = 0; op < numOperands; op++)
        s->operands.push_back(new CCSGResolvedOperand());

    // The boundary rule's two-sided offset
    double diag = 0, extent = 0;
    for (int k = 0; k < 3; k++) {
        diag += (double)(bmax[k] - bmin[k]) * (bmax[k] - bmin[k]);
        extent = std::max(extent, (double)std::max(std::fabs(bmin[k]), std::fabs(bmax[k])));
    }
    const float epsilon = (float)std::max(kEpsilonFraction * std::sqrt(diag), kEpsilonUlps * FLT_EPSILON * extent);
    s->evaluator = new CCSGEvaluator(definition, s->operands, epsilon);
    for (int op = 0; op < numOperands; op++)
        s->regions.push_back(new CCSGRegion(s->evaluator, op));

    for (int op = 0; op < numOperands; op++) {
        CCSGResolvedOperand &operand = *s->operands[op];
        int unsupported = 0;

        // Objects from the host, each with one reference of ours
        std::vector<CObject *> captured;
        auto take = [&](CObject *chain) {
            CObject *next;
            for (CObject *o = chain; o != NULL; o = next) {
                next = o->sibling;
                o->sibling = NULL;
                captured.push_back(o);
            }
        };

        for (CObject *templ : definition->operandMembers[op]) {
            // 0. Members that cannot take part in a solid are dropped
            if (!supportedMember(templ)) {
                unsupported++;
                continue;
            }
            if (host == NULL)
                continue;

            // 1-2. A flagged clone of the leaf's attributes, captured through the host
            CAttributes *clone = new CAttributes(templ->attributes);
            clone->flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;
            if (definition->operandFlip(op))
                clone->flags ^= ATTRIBUTES_FLAGS_INSIDE;
            clone->attach();
            take(host->capture(templ, clone, xform));
            clone->detach();
        }

        // 2b. Procedural members expand into the operand, under their own
        // flagged attributes, until none remains (nested ones are appended and
        // expanded in turn) (FR-024, FR-025)
        for (size_t k = 0; k < captured.size(); k++) {
            CDelayedObject *procedural = dynamic_cast<CDelayedObject *>(captured[k]);
            if (procedural == NULL)
                continue;
            take(host->expand(procedural, procedural->attributes));
            atomicIncrement(stats.numCsgProceduralExpansions);
            procedural->detach();
            captured[k] = NULL;
        }

        // 0 again, over what the expansions produced; the rest join the operand
        for (CObject *o : captured) {
            if (o == NULL)
                continue;
            if (!supportedMember(o))
                unsupported++;
            else {
                operand.addMember(o);
                s->hold(o);
            }
            o->detach(); // the host's reference: the state's hold() keeps a member
        }
        if (unsupported > 0 && definition->firstWarning(CCSGDefinition::WARNING_UNSUPPORTED, op, stats.csgFrame))
            warning(CODE_BADTOKEN, "A primitive in a SolidBegin block is not supported as a solid operand and is ignored "
                                   "(implicit surfaces, DSO geometry, points, curves and object instances cannot take part in a solid)\n");

        // 3. Every member carries its operand's region (mesh children inherit it)
        for (CObject *m : operand.members)
            m->region = s->regions[op];

        // 4-5. Materialize depth first; collect the classification surfaces
        for (size_t m = 0; m < operand.members.size(); m++) {
            std::vector<CSurface *> surfaces;
            materializeSurfaces(context, operand.members[m], surfaces);
            for (CSurface *surface : surfaces) {
                operand.addSurface(surface, (int)m);
                if (surface != operand.members[m])
                    s->hold(surface);
            }
        }

        // 6. The operand's kind (FR-021)
        deriveOperandKind(operand);
        if (operand.kind == CSG_OPERAND_OPEN && definition->firstWarning(CCSGDefinition::WARNING_NOT_CLOSED, op, stats.csgFrame))
            warning(CODE_BADTOKEN, "A solid operand is not closed and is ignored: a lone open surface or mesh encloses no volume\n");
    }

    // 7. Only operands that contribute render; a lone survivor is a passthrough
    std::vector<int> contributing;
    if (definition->root >= 0)
        contributingOperands(definition, s->operands, definition->root, contributing);
    if (contributing.size() == 1) {
        s->passthrough = contributing[0];
        for (CObject *m : s->operands[s->passthrough]->members)
            clearRegion(m);
    }

    CObject *chain = NULL, *tail = NULL;
    auto append = [&](CObject *o) {
        o->sibling = NULL;
        if (tail == NULL)
            chain = o;
        else
            tail->sibling = o;
        tail = o;
    };
    for (int op : contributing) {
        CCSGResolvedOperand &operand = *s->operands[op];
        for (CObject *m : operand.members)
            append(m);

        // 8. A lone open quadric is closed by its implied caps (FR-021); a
        // passthrough renders as authored, uncapped
        if (operand.kind != CSG_OPERAND_SINGLE_QUADRIC || op == s->passthrough)
            continue;
        const CSurface *quadric = operand.surfaces[0];
        std::vector<CCSGCap> caps;
        buildImpliedCaps(quadric, caps);
        for (CCSGCap &cap : caps) {
            // Oriented with the operand, as its members are
            if (definition->operandFlip(op)) {
                CAttributes *flipped = new CAttributes(cap.surface->attributes);
                flipped->flags ^= ATTRIBUTES_FLAGS_INSIDE;
                flipped->attach();
                cap.surface->attributes->detach();
                cap.surface->attributes = flipped;
            }
            CCSGCapRegion *region = new CCSGCapRegion(s->evaluator, op, quadric, cap);
            s->capRegions.push_back(region);
            cap.surface->region = region;
            s->hold(cap.surface);
            cap.surface->detach(); // buildImpliedCaps's reference: the state's hold() keeps it
            append(cap.surface);
        }
    }
    setChildren(context, chain);
    return s;
}
