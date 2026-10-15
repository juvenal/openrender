/**
 * Project: openRender
 *
 * File: patchLayout.cpp
 *
 * Description:
 *   The split layout of a diced surface and the boundary snaps that close the
 *   T-junctions between its grids (GitHub #51).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <math.h>

#include <algorithm>
#include <climits>
#include <deque>

#include "common/global.h"
#include "patchLayout.h"

// Sides of a node: 0 = u at umin, 1 = u at umax (both run along v), 2 = v at
// vmin, 3 = v at vmax (both run along u)
static inline int opposite(int side) { return side ^ 1; }

// Longest chain of corners that slide onto sides ending in such corners again
static const int kMaxSnapChain = 4;

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	CPatchLayout
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CPatchLayout::CPatchLayout(int wrapU, int wrapV) : wrapU(wrapU), wrapV(wrapV), refCount(0) {}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	addNode
// Description			:	Add an undecided node
// Return Value			:	Its index
// Comments				:
int CPatchLayout::addNode(float umin, float umax, float vmin, float vmax) {
    CNode node;
    node.umin = umin;
    node.umax = umax;
    node.vmin = vmin;
    node.vmax = vmax;
    initv(node.bmin, 0);
    initv(node.bmax, 0);
    node.udiv = node.vdiv = 0;
    node.state = NODE_CULLED;
    node.split = -1;
    node.firstChild = -1;
    node.numChildren = 0;
    nodes.push_back(node);
    return (int)nodes.size() - 1;
}

// The extent of a node's side along the side, and its segment count
static inline void sideRange(const CPatchLayout::CNode &n, int side, float &lo, float &hi, int &div) {
    if (side < 2) {
        lo = n.vmin;
        hi = n.vmax;
        div = n.vdiv;
    }
    else {
        lo = n.umin;
        hi = n.umax;
        div = n.udiv;
    }
}

// The parameters of vertex k along a node's side
static inline void sideVertex(const CPatchLayout::CNode &n, int side, int k, float &u, float &v) {
    switch (side) {
        case 0:
            u = n.umin;
            v = gridParam(n.vmin, n.vmax, n.vdiv, k);
            break;
        case 1:
            u = n.umax;
            v = gridParam(n.vmin, n.vmax, n.vdiv, k);
            break;
        case 2:
            u = gridParam(n.umin, n.umax, n.udiv, k);
            v = n.vmin;
            break;
        default:
            u = gridParam(n.umin, n.umax, n.udiv, k);
            v = n.vmax;
            break;
    }
}

// The grid index of vertex k along a node's side
static inline int sideIndex(const CPatchLayout::CNode &n, int side, int k) {
    const int nu = n.udiv + 1;
    switch (side) {
        case 0:
            return k * nu;
        case 1:
            return k * nu + n.udiv;
        case 2:
            return k;
        default:
            return n.vdiv * nu + k;
    }
}

// The vertex of a side nearest to the parameter x along it; halfway goes down
static inline int nearestVertex(float lo, float hi, int div, float x) {
    const double t = (x - (double)lo) / ((double)hi - lo) * div;
    int k = (int)ceil(t - 0.5);
    if (k < 0)
        k = 0;
    if (k > div)
        k = div;
    return k;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	neighbours
// Description			:	The undivided nodes across a node's side
// Return Value			:	-
// Comments				:	Across u = 0 of a surface that wraps in u lies u = 1,
//							and the same in v
void CPatchLayout::neighbours(int node, int side, std::vector<int> &out) const {
    const CNode &n = nodes[node];
    out.clear();

    // The line the neighbours' opposite side lies on
    const int alongV = side < 2;
    float line;
    switch (side) {
        case 0:
            line = (n.umin == 0) ? (wrapU ? 1.0f : -1.0f) : n.umin;
            break;
        case 1:
            line = (n.umax == 1) ? (wrapU ? 0.0f : -1.0f) : n.umax;
            break;
        case 2:
            line = (n.vmin == 0) ? (wrapV ? 1.0f : -1.0f) : n.vmin;
            break;
        default:
            line = (n.vmax == 1) ? (wrapV ? 0.0f : -1.0f) : n.vmax;
            break;
    }
    if (line < 0)
        return;

    const float lo = alongV ? n.vmin : n.umin;
    const float hi = alongV ? n.vmax : n.umax;
    const int upper = side & 1; // The neighbours lie above the line (their min is on it)

    std::vector<int> stack(1, 0);
    while (!stack.empty()) {
        const CNode &c = nodes[stack.back()];
        const int index = stack.back();
        stack.pop_back();

        const float cMin = alongV ? c.umin : c.vmin;
        const float cMax = alongV ? c.umax : c.vmax;
        const float cLo = alongV ? c.vmin : c.umin;
        const float cHi = alongV ? c.vmax : c.umax;
        if (!(cLo < hi && cHi > lo))
            continue;

        if (c.state == NODE_SPLIT) {
            if (upper ? (cMin <= line && cMax > line) : (cMin < line && cMax >= line))
                for (int i = 0; i < c.numChildren; i++)
                    stack.push_back(c.firstChild + i);
        }
        else if (upper ? (cMin == line) : (cMax == line)) {
            out.push_back(index);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	owner
// Description			:	Does a leaf snap its side onto its neighbour's?
// Return Value			:	TRUE if so; neighbour receives the neighbour
// Comments				:	The side with the longer edge owns the boundary;
//							with equal edges, the one with fewer segments, and
//							across a wrap with equal segments the side at 1.
//							Edges are dyadic, so they nest: across a side there
//							is one longer or equal neighbour, or only shorter
//							ones (which snap onto this leaf).
int CPatchLayout::owner(int node, int side, int &neighbour) const {
    std::vector<int> across;
    neighbours(node, side, across);
    if (across.size() != 1 || nodes[across[0]].state != NODE_LEAF)
        return FALSE;

    neighbour = across[0];
    float a, b, c, d;
    int div, nDiv;
    sideRange(nodes[node], side, a, b, div);
    sideRange(nodes[neighbour], opposite(side), c, d, nDiv);

    // The neighbour's edge is longer
    if (c <= a && d >= b && (c < a || d > b))
        return TRUE;

    // Equal edges: the finer side snaps
    if (c == a && d == b) {
        if (div != nDiv)
            return div > nDiv;

        // Equal dicing: the vertices coincide, except across a wrap, where
        // u = 0 and u = 1 evaluate to points a rounding step apart
        const CNode &n = nodes[node];
        return (side == 0 && wrapU && n.umin == 0) || (side == 2 && wrapV && n.vmin == 0);
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	containing
// Description			:	The leaf with (u, v) inside one of its sides
// Return Value			:	The leaf, or -1; side receives the side
// Comments				:	At most one leaf has a given point strictly inside
//							a side: a T-junction. Across a wrap, u = 0 is u = 1.
int CPatchLayout::containing(float u, float v, int &side) const {
    float us[2] = {u, u}, vs[2] = {v, v};
    int numU = 1, numV = 1;
    if (wrapU && (u == 0 || u == 1))
        us[numU++] = 1 - u;
    if (wrapV && (v == 0 || v == 1))
        vs[numV++] = 1 - v;

    for (int i = 0; i < numU; i++) {
        for (int j = 0; j < numV; j++) {
            const float pu = us[i];
            const float pv = vs[j];

            std::vector<int> stack(1, 0);
            while (!stack.empty()) {
                const int index = stack.back();
                const CNode &c = nodes[index];
                stack.pop_back();

                if (pu < c.umin || pu > c.umax || pv < c.vmin || pv > c.vmax)
                    continue;

                if (c.state == NODE_SPLIT) {
                    for (int k = 0; k < c.numChildren; k++)
                        stack.push_back(c.firstChild + k);
                }
                else if (c.state == NODE_LEAF) {
                    if (pv > c.vmin && pv < c.vmax) {
                        if (pu == c.umin) {
                            side = 0;
                            return index;
                        }
                        if (pu == c.umax) {
                            side = 1;
                            return index;
                        }
                    }
                    if (pu > c.umin && pu < c.umax) {
                        if (pv == c.vmin) {
                            side = 2;
                            return index;
                        }
                        if (pv == c.vmax) {
                            side = 3;
                            return index;
                        }
                    }
                }
            }
        }
    }

    return -1;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	cornerTarget
// Description			:	Where a leaf corner at (u, v) is drawn, appended to
//							terms as (u, v, weight) points
// Return Value			:	-
// Comments				:	A corner inside another leaf's side (a T-junction)
//							slides onto that side's chord; the chord's ends may
//							be corners inside a longer side again. Any other
//							corner stays, at u = 1 rather than u = 0 across a
//							wrap, so every grid sharing it agrees. Every grid
//							sharing a corner builds the same terms.
void CPatchLayout::cornerTarget(float u, float v, float weight, std::vector<float> &terms, int depth) const {
    int side;
    const int leaf = (depth < kMaxSnapChain) ? containing(u, v, side) : -1;
    if (leaf >= 0) {
        float lo, hi;
        int div;
        sideRange(nodes[leaf], side, lo, hi, div);
        const double t = ((side < 2 ? v : u) - (double)lo) / ((double)hi - lo) * div;
        int j = (int)floor(t);
        if (j < 0)
            j = 0;
        if (j > div - 1)
            j = div - 1;
        chordTarget(leaf, side, j, t - j, weight, terms, depth + 1);
        return;
    }

    terms.push_back((wrapU && u == 0) ? 1.0f : u);
    terms.push_back((wrapV && v == 0) ? 1.0f : v);
    terms.push_back(weight);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	vertexTarget
// Description			:	Where vertex k of a leaf's side is drawn, the leaf
//							owning that side
// Return Value			:	-
// Comments				:
void CPatchLayout::vertexTarget(int node, int side, int k, float weight, std::vector<float> &terms, int depth) const {
    float lo, hi, u, v;
    int div;
    sideRange(nodes[node], side, lo, hi, div);
    sideVertex(nodes[node], side, k, u, v);
    if (k == 0 || k == div) {
        cornerTarget(u, v, weight, terms, depth);
    }
    else {
        terms.push_back(u);
        terms.push_back(v);
        terms.push_back(weight);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	chordTarget
// Description			:	The point at fraction f along segment j of a leaf's
//							side, the leaf owning that side
// Return Value			:	-
// Comments				:
void CPatchLayout::chordTarget(int node, int side, int j, double f, float weight, std::vector<float> &terms, int depth) const {
    if (f <= 0) {
        vertexTarget(node, side, j, weight, terms, depth);
    }
    else if (f >= 1) {
        vertexTarget(node, side, j + 1, weight, terms, depth);
    }
    else {
        vertexTarget(node, side, j, (float)(weight * (1 - f)), terms, depth);
        vertexTarget(node, side, j + 1, (float)(weight * f), terms, depth);
    }
}

// Do counts n1 over a side and n2 over a side scale (2^k) times as long nest:
// is every vertex of the longer side within the shorter one of its vertices?
static inline int nested(int n1, int n2, int scale) { return (scale * n1) % n2 == 0; }

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	raise
// Description			:	Set a leaf's divisions along one direction
// Return Value			:	TRUE if the leaf changed
// Comments				:	Not past the grid size: the boundary then slides
//							(snaps()) instead of nesting
int CPatchLayout::raise(int node, int alongV, int div, int maxGridSize) {
    CNode &n = nodes[node];
    const int other = alongV ? n.udiv : n.vdiv;
    if ((long long)(other + 1) * (div + 1) > maxGridSize)
        return FALSE;
    if (alongV)
        n.vdiv = div;
    else
        n.udiv = div;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	nest
// Description			:	Raise one of two neighbouring leaves' divisions so
//							that their vertices along the shared boundary nest
// Return Value			:	The leaf raised, or -1
// Comments				:	nb is the one leaf across the side. Across a side
//							2^k times as long (k >= 0), every vertex of the
//							longer side inside the shorter one must be one of
//							its vertices. Either the shorter side rises to the
//							next count that nests, or the longer one does;
//							whichever adds fewer vertices. Nested sides need no
//							snap but the corners'; the others slide (snaps()).
int CPatchLayout::nest(int node, int side, int nb, int maxGridSize) {
    float a, b, c, d;
    int ns, nl;
    sideRange(nodes[node], side, a, b, ns);
    sideRange(nodes[nb], opposite(side), c, d, nl);
    if (!(c <= a && d >= b))
        return -1; // The neighbour's side is shorter: nested from its side

    const int scale = (int)lround((d - (double)c) / ((double)b - a)); // 2^k
    if (nested(ns, nl, scale) || (scale == 1 && nested(nl, ns, 1)))
        return -1;

    // The next counts that nest, on either side, no larger than a grid; the
    // longer side only nests at counts dividing 2^k * ns
    int shortDiv = ns + 1, longDiv = nl + 1;
    while (shortDiv < maxGridSize && !nested(shortDiv, nl, scale) && !(scale == 1 && nested(nl, shortDiv, 1)))
        shortDiv++;
    while (longDiv < maxGridSize && !nested(ns, longDiv, scale) && !(scale == 1 && nested(longDiv, ns, 1)))
        longDiv++;
    const int shortOk = shortDiv < maxGridSize;
    const int longOk = longDiv < maxGridSize;
    if (!shortOk && !longOk)
        return -1;

    const int alongV = side < 2;
    const long long shortCost = shortOk ? (long long)(shortDiv - ns) * ((alongV ? nodes[node].udiv : nodes[node].vdiv) + 1) : LLONG_MAX;
    const long long longCost = longOk ? (long long)(longDiv - nl) * ((alongV ? nodes[nb].udiv : nodes[nb].vdiv) + 1) : LLONG_MAX;
    if (shortCost <= longCost) {
        if (raise(node, alongV, shortDiv, maxGridSize))
            return node;
        return (longOk && raise(nb, alongV, longDiv, maxGridSize)) ? nb : -1;
    }
    if (raise(nb, alongV, longDiv, maxGridSize))
        return nb;
    return (shortOk && raise(node, alongV, shortDiv, maxGridSize)) ? node : -1;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	finalize
// Description			:	Settle the leaves' divisions
// Return Value			:	-
// Comments				:	Nests every boundary until none changes. A raise
//							can unnest the boundaries along the raised
//							direction, so those are checked again; a side's
//							count does not depend on the other direction's.
//							Divisions only grow, up to the grid size, so this
//							reaches the fixed point and stops. The leaves
//							across each side are found once: only divisions
//							change here, never the tree.
void CPatchLayout::finalize(int maxGridSize) {
    const int numNodes = (int)nodes.size();

    // The undivided nodes across each side of each leaf, entries
    // start[4 * node + side] to start[4 * node + side + 1] of across
    std::vector<int> start(4 * (size_t)numNodes + 1, 0);
    std::vector<int> across, found;
    for (int i = 0; i < numNodes; i++) {
        for (int side = 0; side < 4; side++) {
            start[4 * i + side] = (int)across.size();
            if (nodes[i].state == NODE_LEAF) {
                neighbours(i, side, found);
                across.insert(across.end(), found.begin(), found.end());
            }
        }
    }
    start[4 * (size_t)numNodes] = (int)across.size();

    // Every side of every leaf, in order, then each side a raise may unnest
    std::deque<int> queue;
    std::vector<char> queued(4 * (size_t)numNodes, FALSE);
    const auto enqueue = [&](int item) {
        if (!queued[item]) {
            queued[item] = TRUE;
            queue.push_back(item);
        }
    };
    for (int i = 0; i < numNodes; i++)
        if (nodes[i].state == NODE_LEAF)
            for (int side = 0; side < 4; side++)
                enqueue(4 * i + side);

    while (!queue.empty()) {
        const int item = queue.front();
        queue.pop_front();
        queued[item] = FALSE;

        // One leaf across the side, or nothing to nest with
        if (start[item + 1] - start[item] != 1 || nodes[across[start[item]]].state != NODE_LEAF)
            continue;

        const int side = item & 3;
        const int raised = nest(item >> 2, side, across[start[item]], maxGridSize);
        if (raised < 0)
            continue;

        // The raised leaf's sides along the raised direction, from both ends
        const int first = (side < 2) ? 0 : 2;
        for (int s = first; s < first + 2; s++) {
            const int raisedItem = 4 * raised + s;
            enqueue(raisedItem);
            for (int k = start[raisedItem]; k < start[raisedItem + 1]; k++)
                if (nodes[across[k]].state == NODE_LEAF)
                    enqueue(4 * across[k] + opposite(s));
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Method				:	snaps
// Description			:	The boundary vertices of a leaf's grid that are
//							drawn on a neighbour's boundary, and where
// Return Value			:	-
// Comments				:	Corners follow cornerTarget(). On a side owned by
//							the neighbour, each of the neighbour's vertices
//							inside the side takes the nearest free vertex of
//							this grid, in order; the other vertices keep their
//							parameter and slide onto the neighbour's chord
//							between the vertices around them.
void CPatchLayout::snaps(int node, CGridSnap &snap) const {
    const CNode &n = nodes[node];
    std::vector<float> terms;
    snap.index.clear();
    snap.termStart.clear();
    snap.terms.clear();

    // Record a vertex unless it stays at its own parameters
    const auto add = [&](int index, float u, float v) {
        if (terms.size() == 3 && terms[0] == u && terms[1] == v && terms[2] == 1)
            return;
        snap.index.push_back(index);
        snap.termStart.push_back((int)snap.terms.size() / 3);
        snap.terms.insert(snap.terms.end(), terms.begin(), terms.end());
    };

    for (int corner = 0; corner < 4; corner++) {
        const int i = (corner & 1) ? n.udiv : 0;
        const int j = (corner & 2) ? n.vdiv : 0;
        const float u = (corner & 1) ? n.umax : n.umin;
        const float v = (corner & 2) ? n.vmax : n.vmin;
        terms.clear();
        cornerTarget(u, v, 1, terms, 0);
        add(j * (n.udiv + 1) + i, u, v);
    }

    std::vector<int> anchor; // Per vertex of the side: the neighbour vertex it lands on, or -1
    for (int side = 0; side < 4; side++) {
        int nb;
        if (!owner(node, side, nb))
            continue;

        float a, b, c, d;
        int div, nDiv;
        sideRange(n, side, a, b, div);
        sideRange(nodes[nb], opposite(side), c, d, nDiv);
        const int nSide = opposite(side);

        // The neighbour's vertices inside the side, each on its own vertex
        anchor.assign(div + 1, -1);
        int kLo = nDiv, kHi = 0;
        for (int k = 1; k < nDiv; k++) {
            const float x = gridParam(c, d, nDiv, k);
            if (x > a && x < b) {
                kLo = std::min(kLo, k);
                kHi = std::max(kHi, k);
            }
        }
        for (int k = kLo, prev = 0; k <= kHi; k++) {
            const int last = div - 1 - (kHi - k); // Leave a vertex for each one after
            if (prev + 1 > last)
                break; // Too few vertices (the grid size capped the increase)
            int i = nearestVertex(a, b, div, gridParam(c, d, nDiv, k));
            i = std::max(prev + 1, std::min(last, i));
            anchor[i] = k;
            prev = i;
        }

        for (int k = 1; k < div; k++) {
            float u, v;
            sideVertex(n, side, k, u, v);
            terms.clear();
            if (anchor[k] >= 0) {
                vertexTarget(nb, nSide, anchor[k], 1, terms, 0);
            }
            else {
                // The chord between the neighbour vertices around this one
                int j = -1;
                for (int i = k - 1; i > 0 && j < 0; i--)
                    if (anchor[i] >= 0)
                        j = anchor[i];
                if (j < 0) {
                    for (int i = k + 1; i < div && j < 0; i++)
                        if (anchor[i] >= 0)
                            j = anchor[i] - 1;
                }
                const float x = side < 2 ? v : u;
                if (j < 0)
                    j = (int)floor((x - (double)c) / ((double)d - c) * nDiv);
                j = std::max(0, std::min(nDiv - 1, j));

                const double x0 = gridParam(c, d, nDiv, j);
                const double x1 = gridParam(c, d, nDiv, j + 1);
                chordTarget(nb, nSide, j, std::max(0.0, std::min(1.0, (x - x0) / (x1 - x0))), 1, terms, 0);
            }
            add(sideIndex(n, side, k), u, v);
        }
    }

    snap.termStart.push_back((int)snap.terms.size() / 3);
}
