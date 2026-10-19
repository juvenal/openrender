/**
 * Project: openRender
 *
 * File: rasterTie.h
 *
 * Description:
 *   Deterministic tie-break between two opaque fragments of exactly equal depth, shared by the
 *   reyes (stochastic) and zbuffer hiders (spec 039, T084c).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RASTERTIE_H
#define RASTERTIE_H

#include <stdint.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CTieKey
// Description			:	The identity of the fragment that currently owns a sample
// Comments				:	Two opaque fragments of exactly equal depth used to be
//							resolved by arrival order: `z < pixel->z` kept the first one
//							to reach the sample, and the order in which two grids reach a
//							bucket depends on how the threads were scheduled. Exact ties
//							are not rare: a degenerate micropolygon (a triangle corner)
//							takes the depth of its apex, so two such cells at one mesh
//							vertex tie wherever they overlap on screen (GitHub #62), and
//							coincident surfaces tie everywhere.
//
//							The tie is resolved by this key instead: the smaller key wins.
//							The key is the object's stable identity, then the index of the
//							micropolygon (the index of its first vertex in the grid, or of
//							the point). Both are the same on every run, thread count and
//							process, so the winner no longer depends on arrival order.
//							Equal keys (two grids of one object that repeat an index) keep
//							the first arrival, as before.
struct CTieKey {
        uint64_t id;    // The object's stableId
        uint32_t index; // The micropolygon (or point) index in the grid
};

inline bool tieKeyLess(const CTieKey &a, const CTieKey &b) {
    return a.id < b.id || (a.id == b.id && a.index < b.index);
}

// The zbuffer keeps its samples as plain floats: the key lives in three of them
inline CTieKey loadTieKey(const float *s) {
    CTieKey k;
    memcpy(&k.id, s, sizeof(k.id));
    memcpy(&k.index, s + 2, sizeof(k.index));
    return k;
}

inline void storeTieKey(float *s, const CTieKey &k) {
    memcpy(s, &k.id, sizeof(k.id));
    memcpy(s + 2, &k.index, sizeof(k.index));
}

// The key of the micropolygon whose first vertex is `__vertex`, in the grid being drawn. Only
// used where a function has `grid`, `numVertexSamples` and the first-vertex pointer in scope.
#define RASTER_TIE_KEY(__vertex) \
    CTieKey { (uint64_t)((const CObject *)grid->object)->stableId, (uint32_t)(((__vertex) - grid->vertices) / numVertexSamples) }

#endif
