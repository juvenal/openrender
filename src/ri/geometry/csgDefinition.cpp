/**
 * Project: openRender
 *
 * File: csgDefinition.cpp
 *
 * Description:
 *   Immutable, shared CSG definition: the template tree a solid
 *   keeps for the whole frame (spec 021).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "csgDefinition.h"

#include <algorithm>
#include <cmath>

#include "object.h"
#include "quadrics.h"
#include "rendererc.h"
#include "xform.h"

// Boundary edges of one surface are compared at this many points
static const int kEdgeSamples = 5;

// Edges coincide (or collapse) within this fraction of the member's bound
static const double kEdgeTolerance = 1e-5;

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGResolvedOperand
// Method				:	CCSGResolvedOperand
// Description			:	Ctor: an empty operand with an empty bound
// Return Value			:	-
// Comments				:
CCSGResolvedOperand::CCSGResolvedOperand() {
    kind = CSG_OPERAND_EMPTY;
    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);
}

void CCSGResolvedOperand::addMember(CObject *member) {
    members.push_back(member);
}

void CCSGResolvedOperand::addSurface(CSurface *surface, int member) {
    surfaces.push_back(surface);
    surfaceMember.push_back(member);
}

// A surface evaluated at (u, v) in [0, 1]^2 through its own sample(), at
// shutter open and undisplaced
static void sampleSurface(const CSurface *surface, int n, const float *u, const float *v, float *P) {
    std::vector<float> uu(u, u + n), vv(v, v + n), time(n, 0.0f), du(3 * n), dv(3 * n), ng(3 * n), dt(3 * n), pw(4 * n);
    float *varying[VARIABLE_CONSTANTWIDTH + 1];
    for (unsigned int k = 0; k <= VARIABLE_CONSTANTWIDTH; k++)
        varying[k] = NULL;
    varying[VARIABLE_U] = uu.data();
    varying[VARIABLE_V] = vv.data();
    varying[VARIABLE_TIME] = time.data();
    varying[VARIABLE_P] = P;
    varying[VARIABLE_DPDU] = du.data();
    varying[VARIABLE_DPDV] = dv.data();
    varying[VARIABLE_NG] = ng.data();
    varying[VARIABLE_DPDTIME] = dt.data();
    varying[VARIABLE_PW] = pw.data();
    unsigned int up = PARAMETER_P | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_NG | PARAMETER_BEGIN_SAMPLE;
    surface->sample(0, n, varying, NULL, up);
}

struct CBoundaryEdge {
        float P[kEdgeSamples][3];
        float mid; // x of the middle sample, for the sweep
};

static bool edgePointsMatch(const float *a, const float *b, double tol) {
    return std::fabs(a[0] - b[0]) <= tol && std::fabs(a[1] - b[1]) <= tol && std::fabs(a[2] - b[2]) <= tol;
}

static bool edgesCoincide(const CBoundaryEdge &a, const CBoundaryEdge &b, double tol) {
    bool forward = true, backward = true;
    for (int i = 0; i < kEdgeSamples && (forward || backward); i++) {
        forward = forward && edgePointsMatch(a.P[i], b.P[i], tol);
        backward = backward && edgePointsMatch(a.P[i], b.P[kEdgeSamples - 1 - i], tol);
    }
    return forward || backward;
}

///////////////////////////////////////////////////////////////////////
// Function				:	memberIsClosed
// Description			:	FR-021 closedness of one declared non-quadric
//							primitive: every parametric boundary edge of its
//							surfaces collapses to a point or coincides with
//							another boundary edge of the same member
// Return Value			:	TRUE if closed
// Comments				:	Polygon faces sample as triangles or bilinear
//							quads, so "every edge shared by two faces" is the
//							same test
static int memberIsClosed(const std::vector<CSurface *> &surfaces) {
    if (surfaces.empty())
        return FALSE;

    vector bmin, bmax;
    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);
    std::vector<CBoundaryEdge> edges;
    edges.reserve(surfaces.size() * 4);

    float u[4 * kEdgeSamples], v[4 * kEdgeSamples], P[4 * kEdgeSamples * 3];
    for (int i = 0; i < kEdgeSamples; i++) {
        const float s = (float)i / (kEdgeSamples - 1);
        u[i] = s, v[i] = 0;                                         // v = 0
        u[kEdgeSamples + i] = s, v[kEdgeSamples + i] = 1;           // v = 1
        u[2 * kEdgeSamples + i] = 0, v[2 * kEdgeSamples + i] = s;   // u = 0
        u[3 * kEdgeSamples + i] = 1, v[3 * kEdgeSamples + i] = s;   // u = 1
    }
    for (const CSurface *surface : surfaces) {
        sampleSurface(surface, 4 * kEdgeSamples, u, v, P);
        for (int e = 0; e < 4; e++) {
            CBoundaryEdge edge;
            for (int i = 0; i < kEdgeSamples; i++)
                for (int k = 0; k < 3; k++) {
                    edge.P[i][k] = P[(e * kEdgeSamples + i) * 3 + k];
                    bmin[k] = std::min(bmin[k], edge.P[i][k]);
                    bmax[k] = std::max(bmax[k], edge.P[i][k]);
                }
            edge.mid = edge.P[kEdgeSamples / 2][0];
            edges.push_back(edge);
        }
    }

    const double diag = std::sqrt((double)(bmax[0] - bmin[0]) * (bmax[0] - bmin[0]) + (double)(bmax[1] - bmin[1]) * (bmax[1] - bmin[1]) +
                                  (double)(bmax[2] - bmin[2]) * (bmax[2] - bmin[2]));
    const double tol = std::max(kEdgeTolerance * diag, 1e-7);

    // Collapsed edges need no partner
    std::vector<CBoundaryEdge> open;
    for (const CBoundaryEdge &edge : edges) {
        bool collapsed = true;
        for (int i = 1; i < kEdgeSamples && collapsed; i++)
            collapsed = edgePointsMatch(edge.P[0], edge.P[i], tol);
        if (!collapsed)
            open.push_back(edge);
    }

    // Every remaining edge must coincide with another one: sweep by the
    // middle sample's x, which coinciding edges share in either direction
    std::sort(open.begin(), open.end(), [](const CBoundaryEdge &a, const CBoundaryEdge &b) { return a.mid < b.mid; });
    for (size_t i = 0; i < open.size(); i++) {
        bool partner = false;
        for (size_t j = i; j-- > 0 && open[i].mid - open[j].mid <= tol && !partner;)
            partner = edgesCoincide(open[i], open[j], tol);
        for (size_t j = i + 1; j < open.size() && open[j].mid - open[i].mid <= tol && !partner; j++)
            partner = edgesCoincide(open[i], open[j], tol);
        if (!partner)
            return FALSE;
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Function				:	deriveOperandKind
// Description			:	See csgDefinition.h. One member: a quadric with an
//							implied solid is a single-quadric operand, a disk
//							or a flat ring encloses nothing (empty), any other
//							member is multi-surface if closed and open
//							otherwise. Several members: multi-surface parity.
// Return Value			:	-
// Comments				:	Also sets the operand's camera-space bound
void deriveOperandKind(CCSGResolvedOperand &operand) {
    initv(operand.bmin, C_INFINITY);
    initv(operand.bmax, -C_INFINITY);
    for (const CSurface *surface : operand.surfaces) {
        addBox(operand.bmin, operand.bmax, surface->bmin);
        addBox(operand.bmin, operand.bmax, surface->bmax);
    }

    if (operand.surfaces.empty() || operand.members.empty()) {
        operand.kind = CSG_OPERAND_EMPTY;
        return;
    }
    if (operand.members.size() > 1) {
        operand.kind = CSG_OPERAND_MULTI_SURFACE;
        return;
    }

    const CSurface *quadric = dynamic_cast<const CSurface *>(operand.members[0]);
    if (quadric != NULL && operand.surfaces.size() == 1 && operand.surfaces[0] == quadric) {
        if (quadric->impliesSolid()) {
            operand.kind = CSG_OPERAND_SINGLE_QUADRIC;
            return;
        }
        if (dynamic_cast<const CDisk *>(quadric) != NULL || dynamic_cast<const CHyperboloid *>(quadric) != NULL) {
            operand.kind = CSG_OPERAND_EMPTY; // Encloses no volume
            return;
        }
    }

    operand.kind = memberIsClosed(operand.surfaces) ? CSG_OPERAND_MULTI_SURFACE : CSG_OPERAND_OPEN;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGDefinition
// Method				:	CCSGDefinition / ~CCSGDefinition
// Description			:	Ctor / dtor (members are attached while held)
// Return Value			:	-
// Comments				:
CCSGDefinition::CCSGDefinition() {
    root = -1;
    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);
}

CCSGDefinition::~CCSGDefinition() {
    for (std::vector<CObject *> &members : operandMembers)
        for (CObject *member : members)
            member->detach();
}

int CCSGDefinition::addOperand() {
    operandMembers.emplace_back();
    leafFlip.push_back(0);
    return (int)operandMembers.size() - 1;
}

void CCSGDefinition::addOperandMember(int operand, CObject *member) {
    member->attach();
    operandMembers[operand].push_back(member);
}

int CCSGDefinition::addLeaf(int operand) {
    Node n;
    n.operation = CSG_PRIMITIVE;
    n.operand = operand;
    nodes.push_back(n);
    return (int)nodes.size() - 1;
}

int CCSGDefinition::addNode(ECSGOperation operation) {
    Node n;
    n.operation = operation;
    n.operand = -1;
    nodes.push_back(n);
    return (int)nodes.size() - 1;
}

void CCSGDefinition::addChild(int node, int child) {
    nodes[node].children.push_back(child);
}

void CCSGDefinition::setRoot(int node) {
    root = node;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGDefinition
// Method				:	finalize
// Description			:	Static orientation parity per operand, member
//							rebasing into the outer frame, and the CSG-aware
//							template bound
// Return Value			:	-
// Comments				:	Captured leaves carry absolute transforms; each
//							becomes outerXform^-1 . leafXform, so that an
//							instance placed with the solid's xform reproduces
//							the leaf's original placement (formerly
//							csgRebaseFragments). The bound is expressed in the
//							same outer-local frame. outerXform may be NULL
//							(unit tests): no rebasing, camera-space bound.
void CCSGDefinition::finalize(CXform *outerXform) {
    // Flips: odd number of subtrahend positions on the path to the root
    std::fill(leafFlip.begin(), leafFlip.end(), 0);
    if (root >= 0)
        markFlips(root, 0);

    // Rebase every member's transform into the outer frame
    CXform *outerInverse = NULL;
    if (outerXform != NULL) {
        outerInverse = new CXform(outerXform);
        outerInverse->invert();
        for (std::vector<CObject *> &members : operandMembers)
            for (CObject *member : members) {
                CXform *rebased = new CXform(outerInverse);
                rebased->concat(member->xform);
                rebased->attach();
                member->xform->detach();
                member->xform = rebased;
            }
    }

    // Per-operand template bounds, in the outer frame
    operandBmin.assign(operandMembers.size() * 3, C_INFINITY);
    operandBmax.assign(operandMembers.size() * 3, -C_INFINITY);
    for (size_t op = 0; op < operandMembers.size(); op++)
        for (CObject *member : operandMembers[op]) {
            vector lo, hi;
            movvv(lo, member->bmin);
            movvv(hi, member->bmax);
            if (outerXform != NULL)
                outerXform->invTransformBound(lo, hi);
            addBox(&operandBmin[op * 3], &operandBmax[op * 3], lo);
            addBox(&operandBmin[op * 3], &operandBmax[op * 3], hi);
        }

    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);
    if (root >= 0)
        nodeBound(root, bmin, bmax);

    if (outerInverse != NULL)
        delete outerInverse;
}

void CCSGDefinition::markFlips(int node, int parity) {
    const Node &n = nodes[node];
    if (n.operand >= 0) {
        leafFlip[n.operand] = parity & 1;
        return;
    }
    for (size_t i = 0; i < n.children.size(); i++)
        markFlips(n.children[i], parity + ((n.operation == CSG_DIFFERENCE && i > 0) ? 1 : 0));
}

// CSG-aware bound of a subtree: union = union of boxes, intersection = their
// overlap, difference = its first operand's box. An empty box is lo > hi.
void CCSGDefinition::nodeBound(int node, float *lo, float *hi) const {
    const Node &n = nodes[node];
    initv(lo, C_INFINITY);
    initv(hi, -C_INFINITY);
    if (n.operand >= 0) {
        movvv(lo, &operandBmin[n.operand * 3]);
        movvv(hi, &operandBmax[n.operand * 3]);
        return;
    }
    const int count = (int)n.children.size();
    for (int i = 0; i < count; i++) {
        vector clo, chi;
        nodeBound(n.children[i], clo, chi);
        if (n.operation == CSG_INTERSECTION) {
            if (i == 0) {
                movvv(lo, clo);
                movvv(hi, chi);
            }
            else
                for (int k = 0; k < 3; k++) {
                    lo[k] = std::max(lo[k], clo[k]);
                    hi[k] = std::min(hi[k], chi[k]);
                }
        }
        else if (n.operation == CSG_DIFFERENCE) {
            if (i == 0) {
                movvv(lo, clo);
                movvv(hi, chi);
            }
        }
        else if (clo[0] <= chi[0] && clo[1] <= chi[1] && clo[2] <= chi[2]) {
            addBox(lo, hi, clo);
            addBox(lo, hi, chi);
        }
    }
}

int CCSGDefinition::operandFlip(int operand) const {
    return leafFlip[operand];
}

// Does a subtree hold any leaf at all?
int CCSGDefinition::hasLeaves(int node) const {
    const Node &n = nodes[node];
    if (n.operand >= 0)
        return TRUE;
    for (int child : n.children)
        if (hasLeaves(child))
            return TRUE;
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGDefinition
// Method				:	isEmpty
// Description			:	The tree holds no operand at all, so the solid
//							yields nothing (spec 013 FR-016)
// Return Value			:	-
// Comments				:	Structural: emptiness of an operand's members is
//							only known per instance, after expansion
int CCSGDefinition::isEmpty() const {
    return root < 0 || !hasLeaves(root);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGDefinition
// Method				:	passthroughOperand
// Description			:	The operand a tree collapses to when every node on
//							its path has a single non-empty child (spec 013
//							FR-017), or -1
// Return Value			:	-
// Comments				:
int CCSGDefinition::passthroughOperand() const {
    if (root < 0)
        return -1;
    int node = root;
    for (;;) {
        const Node &n = nodes[node];
        if (n.operand >= 0)
            return n.operand;
        int only = -1, count = 0;
        for (int child : n.children)
            if (hasLeaves(child)) {
                only = child;
                count++;
            }
        if (count != 1)
            return -1;
        node = only;
    }
}
