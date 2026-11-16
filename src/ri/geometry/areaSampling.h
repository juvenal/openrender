/**
 * Project: openRender
 *
 * File: areaSampling.h
 *
 * Description:
 *   Spec 057 (area-light geometry), FR-006 to FR-009: area sampling of a leaf, in Geometry. A leaf's CAreaTable draws a
 *   point uniformly by area as a pure function of two numbers (no state, no allocation, no random()); an emitter set
 *   (emitterRegistry.h) chooses the leaf by area and then asks its table.
 *
 *   Three samplers, all built from camera-space geometry at shutter open:
 *     - the exact triangle (the polygon mesh's triangle, PolygonTriangle map: the point's (u,v) are the triangle's own
 *       parametrization, P = v0 (1-u) + v1 u v + v2 u (1-v));
 *     - the exact planar quad (BilinearQuad map): two triangles chosen by area, the drawn point mapped back to the face's
 *       (u,v) by inverse bilinear. A quad that is not planar, or whose diagonal split is not convex, falls back to the
 *       tessellated table;
 *     - the tessellated table (Affine map): K x K cells of the surface (K = kAreaTessellationK), each two flat triangles,
 *       areas from the true surface evaluation. The drawn point is placed in a cell's triangle and its (u,v) are interpolated
 *       affinely across the cell; P and Ng are then the TRUE surface's at that (u,v) (CSurface::samplePoint), so the point lies
 *       on the surface the ray path sees. The density is 1 / (the tessellated area): it carries the O(K^-2) bias of
 *       research R3.
 *   Ng is the unit geometric normal oriented as dPdu x dPdv (Orientation is applied by the caller, as for shading).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef AREASAMPLING_H
#define AREASAMPLING_H

#include <cstddef>
#include <vector>

#include "object.h"

// Cells per side of a tessellated table (subdivision limit patches, non-planar quads, the free-form patches)
inline constexpr int kAreaTessellationK = 8;

///////////////////////////////////////////////////////////////////////
// Class				:	CAreaSample
// Description			:	One draw: a point of a leaf and the density of drawing it
// Comments				:	pdfA = 1 / (the light's total emitter area), or 0 with leaf == nullptr when the light has no area
struct CAreaSample {
        const CSurface *leaf = nullptr; // the leaf the point belongs to; (u, v) are its own parametric coordinates
        float u = 0, v = 0;
        float P[3] = {0, 0, 0};  // camera space, shutter open
        float Ng[3] = {0, 0, 0}; // unit, oriented as dPdu x dPdv
        float pdfA = 0;
};

enum class EAreaMap { Affine, PolygonTriangle, BilinearQuad };

// One flat triangle of a table
struct CAreaFace {
        double P[3][3] = {{0}};
        float uv[3][2] = {{0}}; // the corners' parametric coordinates (Affine map)
        double area = 0;
        double normal[3] = {0, 0, 0}; // unit, oriented as dPdu x dPdv; zero for a face of no area
};

///////////////////////////////////////////////////////////////////////
// Class				:	CAreaTable
// Description			:	A leaf's own area table: its flat triangles, their cumulative areas and the map from a drawn point
//							to the leaf's (u, v)
// Comments				:	Built once (resolution), read-only afterwards
class CAreaTable {
    public:
        EAreaMap map = EAreaMap::Affine;
        const CSurface *surface = nullptr; // non-null: P and Ng are evaluated on this surface at the drawn (u, v)
        double quad[4][3] = {{0}};         // BilinearQuad: the corners A (u0 v0), B (u1 v0), D (u0 v1), C (u1 v1), in that order
        std::vector<CAreaFace> faces;
        std::vector<double> cdf; // cumulative face areas, normalized, cdf.back() == 1; empty when area == 0
        double area = 0;

        // Sums the faces' areas and builds the cdf
        void finalize();

        // A point uniformly by area from (u1, u2) in [0, 1): u1 chooses the face in the cdf and is rescaled in double.
        // Fills leaf-local fields (u, v, P, Ng) of `out`; FALSE (and out untouched) when the table has no area.
        bool draw(double u1, double u2, CAreaSample &out) const;
};

// The bin of `u` in a normalized cumulative table (cdf.back() == 1), never a zero-width one, and u rescaled into that bin
// in double: u in [cdf[i-1], cdf[i]) -> [0, 1). Requires a non-empty cdf.
size_t areaPickBin(const std::vector<double> &cdf, double u, double &rescaled);

// A polygon mesh triangle (CPolygonTriangle's parametrization): corners v0, v1, v2 in camera space
CAreaTable makeTriangleTable(const float *v0, const float *v1, const float *v2);

// A polygon mesh quad (CPolygonQuad's parametrization: P = (v0 (1-u) + v1 u)(1-v) + (v2 (1-u) + v3 u) v), corners in camera space.
// Planar and convex along a diagonal: the exact sampler. Otherwise the tessellated table of `surface`.
CAreaTable makeQuadTable(const CSurface *surface, const float *v0, const float *v1, const float *v2, const float *v3);

// The tessellated table of a surface over [0, 1]^2 at shutter open, K x K cells
CAreaTable makeTessellatedTable(const CSurface *surface, int K = kAreaTessellationK);

#endif
