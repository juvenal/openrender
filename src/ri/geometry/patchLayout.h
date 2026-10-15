/**
 * Project: openRender
 *
 * File: patchLayout.h
 *
 * Description:
 *   The split layout of a diced surface, decided once before any of its grids
 *   is drawn, so that every grid knows the dicing of the grids around it and
 *   can close the T-junctions between grids diced at different rates
 *   (GitHub #51).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef PATCHLAYOUT_H
#define PATCHLAYOUT_H

#include <atomic>
#include <vector>

#include "common/algebra.h"
#include "common/global.h"

///////////////////////////////////////////////////////////////////////
// Function				:	gridParam
// Description			:	The parameter of vertex i of a grid diced into div
//							segments over [min, max]
// Return Value			:	-
// Comments				:	The one formula for a grid's vertex parameters: a
//							grid and its neighbours evaluate a shared vertex at
//							bit-identical (u, v) only if both compute it this
//							way. Vertex div lands exactly on max.
inline float gridParam(float min, float max, int div, int i) {
    const double start = min;
    const double step = (max - start) / (double)div;
    return (float)(start + i * step);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CGridSnap
// Description			:	Boundary vertices of one grid that are drawn at a
//							point of a neighbouring grid's boundary
// Comments				:	The grid still shades at its own parameters; only
//							the projected position of each listed vertex is
//							replaced, by a weighted sum of projected surface
//							points: one point is a vertex of the neighbour,
//							two weighted ones a point on the neighbour's
//							chord between two of its vertices
struct CGridSnap {
        std::vector<int> index;     // Vertex index in the grid (row-major, udiv + 1 per row)
        std::vector<int> termStart; // First term of each vertex; one more entry ends the last
        std::vector<float> terms;   // (u, v, weight) per term
};

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchLayout
// Description			:	The tree of split decisions of one surface
// Comments				:	Built single-threaded by the first dice of the
//							surface, read-only afterwards. Every split is at a
//							midpoint, so node edges are dyadic and match with ==.
//
//							Neighbouring leaves diced at different rates
//							approximate their shared boundary by different
//							polylines. Each grid snaps its boundary vertices
//							onto the polyline of the side that owns the
//							boundary: the leaf with the longer edge, or with
//							equal edges the one with fewer segments. One
//							snapped vertex lands on each of the owner's
//							vertices, evaluated at the same parameters, so
//							shared vertices are bit-identical; the others keep
//							their parameter and slide onto the owner's chords,
//							which moves them by no more than the chord's sag.
class CPatchLayout {
    public:
        enum ENodeState { NODE_CULLED, NODE_SPLIT, NODE_LEAF };

        struct CNode {
                float umin, umax, vmin, vmax; // Parametric extent
                vector bmin, bmax;            // Camera-space bound (not for culled nodes)
                int udiv, vdiv;               // Divisions; 0 = spans the eye plane (split in 4)
                int state;                    // ENodeState
                int split;                    // CPatch::splitToChildren direction (split nodes)
                int firstChild, numChildren;  // Children (split nodes)
        };

        CPatchLayout(int wrapU, int wrapV);

        void attach() { refCount++; }
        void detach() {
            if (--refCount == 0)
                delete this;
        }

        int addNode(float umin, float umax, float vmin, float vmax);
        void finalize(int maxGridSize); // Raise leaf divisions so neighbouring vertices nest

        void snaps(int node, CGridSnap &snap) const; // The snaps of a leaf's grid

        std::vector<CNode> nodes; // Node 0 is the root

    private:
        int wrapU, wrapV;            // The surface closes on itself across u = 0/1, v = 0/1
        std::atomic<int> refCount;

        void neighbours(int node, int side, std::vector<int> &out) const;
        int owner(int node, int side, int &neighbour) const;
        int containing(float u, float v, int &side) const;
        void cornerTarget(float u, float v, float weight, std::vector<float> &terms, int depth) const;
        void vertexTarget(int node, int side, int k, float weight, std::vector<float> &terms, int depth) const;
        void chordTarget(int node, int side, int j, double f, float weight, std::vector<float> &terms, int depth) const;
        int nest(int node, int side, int nb, int maxGridSize);
        int raise(int node, int alongV, int div, int maxGridSize);
};

#endif
