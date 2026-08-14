/**
 * Project: openRender
 *
 * File: subdivisionLoop.cpp
 *
 * Description:
 *   This file implements the interface for the Loop subdivision scheme.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	subdivisionLoop.cpp
//  Classes				:	CLoopSubdivMesh
//  Description			:	Loop subdivision (all-triangle meshes), implemented as
//							iterative/uniform mask-based refinement (research.md R7 -
//							no eigenbasis / extraordinary-vertex limit evaluation).
//							The refined triangle mesh is wrapped in a plain
//							CPolygonMesh child so every hider reaches it through the
//							same generic dispatch as any other polygon mesh.
//
////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "common/os.h"
#include "error.h"
#include "polygons.h"
#include "reyes.h"
#include "riInterface.h"
#include "stats.h"
#include "subdivisionLoop.h"

// Fixed number of uniform refinement passes. Per research.md R7, Loop only
// needs to reach Catmull-Clark's existing integration depth (REYES dicing +
// generic raytrace tessellation fallback), not exact extraordinary-vertex
// limit-surface evaluation, so a small fixed refinement depth is sufficient.
static const int kLoopSubdivisionLevels = 2;

namespace {

struct LoopVertexParam {
    CVariable *variable;
    int numFloats;
    int oldIndex;
};

struct LoopEdgeInfo {
    int v0, v1;      // canonicalized endpoint vertex indices (v0 < v1)
    int face0, face1; // incident triangle indices (-1 if none)
    int opp0, opp1;   // third vertex of face0/face1, opposite this edge (-1 if none)
    bool nonManifold; // true once a third incident face is seen
};

} // namespace

static int64_t loopEdgeKey(int a, int b) {
    if (a > b) {
        int t = a;
        a = b;
        b = t;
    }

    return (static_cast<int64_t>(a) << 32) | static_cast<uint32_t>(b);
}

// Builds the edge list / adjacency for the current triangulation.
static void loopBuildEdges(const std::vector<int> &triVerts, std::vector<LoopEdgeInfo> &edges, std::unordered_map<int64_t, int> &edgeIndex) {
    const int numTris = (int)(triVerts.size() / 3);

    edges.clear();
    edgeIndex.clear();
    edgeIndex.reserve(static_cast<size_t>(numTris) * 2);

    for (int t = 0; t < numTris; t++) {
        const int a = triVerts[t * 3 + 0];
        const int b = triVerts[t * 3 + 1];
        const int c = triVerts[t * 3 + 2];
        const int tv[3][3] = {{a, b, c}, {b, c, a}, {c, a, b}};

        for (int i = 0; i < 3; i++) {
            const int v0 = tv[i][0];
            const int v1 = tv[i][1];
            const int opp = tv[i][2];
            const int64_t key = loopEdgeKey(v0, v1);
            std::unordered_map<int64_t, int>::iterator it = edgeIndex.find(key);

            if (it == edgeIndex.end()) {
                LoopEdgeInfo e;
                e.v0 = (v0 < v1) ? v0 : v1;
                e.v1 = (v0 < v1) ? v1 : v0;
                e.face0 = t;
                e.opp0 = opp;
                e.face1 = -1;
                e.opp1 = -1;
                e.nonManifold = false;
                edgeIndex[key] = (int)edges.size();
                edges.push_back(e);
            } else {
                LoopEdgeInfo &e = edges[it->second];

                if (e.face1 == -1) {
                    e.face1 = t;
                    e.opp1 = opp;
                } else {
                    e.nonManifold = true;
                }
            }
        }
    }
}

// Applies the Loop smoothing masks to a single vertex-class parameter buffer,
// returning the refined buffer (old, smoothed vertices followed by the new
// edge-midpoint vertices). The same topology (edges/neighbors/boundary data)
// is shared across every parameter and across both motion samples, since it
// depends only on the mesh connectivity, not on the parameter values.
static std::vector<float> loopSmoothParamBuffer(int numV, int nf, const std::vector<LoopEdgeInfo> &edges, const std::vector<std::vector<int>> &neighbors, const std::vector<std::vector<int>> &boundaryNeighbors, const std::vector<bool> &vertexNonManifold, const std::vector<float> &oldBuf) {
    const int numEdges = (int)edges.size();
    std::vector<float> newBuf(static_cast<size_t>(numV + numEdges) * nf);

    // Smoothed positions of the pre-existing vertices.
    for (int v = 0; v < numV; v++) {
        const float *oldV = &oldBuf[static_cast<size_t>(v) * nf];
        float *dst = &newBuf[static_cast<size_t>(v) * nf];

        if (vertexNonManifold[v] || neighbors[v].empty()) {
            for (int c = 0; c < nf; c++)
                dst[c] = oldV[c];
            continue;
        }

        if (!boundaryNeighbors[v].empty()) {
            if (boundaryNeighbors[v].size() != 2) {
                for (int c = 0; c < nf; c++)
                    dst[c] = oldV[c];
                continue;
            }

            const float *b0 = &oldBuf[static_cast<size_t>(boundaryNeighbors[v][0]) * nf];
            const float *b1 = &oldBuf[static_cast<size_t>(boundaryNeighbors[v][1]) * nf];

            for (int c = 0; c < nf; c++)
                dst[c] = 0.75f * oldV[c] + 0.125f * (b0[c] + b1[c]);
            continue;
        }

        const int n = (int)neighbors[v].size();
        const float beta = (n == 3) ? (3.0f / 16.0f) : (3.0f / (8.0f * n));

        for (int c = 0; c < nf; c++)
            dst[c] = (1.0f - n * beta) * oldV[c];

        for (int k = 0; k < n; k++) {
            const float *nb = &oldBuf[static_cast<size_t>(neighbors[v][k]) * nf];

            for (int c = 0; c < nf; c++)
                dst[c] += beta * nb[c];
        }
    }

    // Edge midpoints become the new vertices, appended after the old ones.
    for (int e = 0; e < numEdges; e++) {
        const LoopEdgeInfo &edge = edges[e];
        float *dst = &newBuf[static_cast<size_t>(numV + e) * nf];
        const float *v0 = &oldBuf[static_cast<size_t>(edge.v0) * nf];
        const float *v1 = &oldBuf[static_cast<size_t>(edge.v1) * nf];

        if (edge.nonManifold || edge.face1 == -1) {
            for (int c = 0; c < nf; c++)
                dst[c] = 0.5f * (v0[c] + v1[c]);
        } else {
            const float *o0 = &oldBuf[static_cast<size_t>(edge.opp0) * nf];
            const float *o1 = &oldBuf[static_cast<size_t>(edge.opp1) * nf];

            for (int c = 0; c < nf; c++)
                dst[c] = 0.375f * (v0[c] + v1[c]) + 0.125f * (o0[c] + o1[c]);
        }
    }

    return newBuf;
}

// Runs a single Loop refinement pass: smooths every vertex-class parameter
// buffer (for both data0 and, if moving, data1) and 4-splits every triangle
// using the new edge-midpoint vertices. Returns the new vertex count.
static int loopRefineOnce(int numV, std::vector<int> &triVerts, const std::vector<LoopVertexParam> &vertexParams, std::vector<std::vector<float>> &data0, std::vector<std::vector<float>> *data1) {
    std::vector<LoopEdgeInfo> edges;
    std::unordered_map<int64_t, int> edgeIndex;

    loopBuildEdges(triVerts, edges, edgeIndex);

    const int numEdges = (int)edges.size();

    std::vector<std::vector<int>> neighbors(numV);
    std::vector<std::vector<int>> boundaryNeighbors(numV);
    std::vector<bool> vertexNonManifold(numV, false);

    for (int e = 0; e < numEdges; e++) {
        const LoopEdgeInfo &edge = edges[e];

        neighbors[edge.v0].push_back(edge.v1);
        neighbors[edge.v1].push_back(edge.v0);

        if (edge.nonManifold) {
            vertexNonManifold[edge.v0] = true;
            vertexNonManifold[edge.v1] = true;
        } else if (edge.face1 == -1) {
            boundaryNeighbors[edge.v0].push_back(edge.v1);
            boundaryNeighbors[edge.v1].push_back(edge.v0);
        }
    }

    for (size_t p = 0; p < vertexParams.size(); p++) {
        const int nf = vertexParams[p].numFloats;

        data0[p] = loopSmoothParamBuffer(numV, nf, edges, neighbors, boundaryNeighbors, vertexNonManifold, data0[p]);

        if (data1 != NULL)
            (*data1)[p] = loopSmoothParamBuffer(numV, nf, edges, neighbors, boundaryNeighbors, vertexNonManifold, (*data1)[p]);
    }

    const int numTris = (int)(triVerts.size() / 3);
    std::vector<int> newTriVerts;
    newTriVerts.reserve(static_cast<size_t>(numTris) * 4 * 3);

    for (int t = 0; t < numTris; t++) {
        const int a = triVerts[t * 3 + 0];
        const int b = triVerts[t * 3 + 1];
        const int c = triVerts[t * 3 + 2];
        const int mab = numV + edgeIndex[loopEdgeKey(a, b)];
        const int mbc = numV + edgeIndex[loopEdgeKey(b, c)];
        const int mca = numV + edgeIndex[loopEdgeKey(c, a)];
        const int quad[4][3] = {{a, mab, mca}, {b, mbc, mab}, {c, mca, mbc}, {mab, mbc, mca}};

        for (int q = 0; q < 4; q++)
            for (int i = 0; i < 3; i++)
                newTriVerts.push_back(quad[q][i]);
    }

    triVerts = std::move(newTriVerts);

    return numV + numEdges;
}

// Builds a trivial, empty CPolygonMesh used as a safe fallback when the base
// mesh cannot be Loop-subdivided (mixed topology, or no vertex data at all).
static CObject *loopMakeEmptyFallback(CAttributes *attributes, CXform *xform, CPl *sourcePl) {
    static int dummy = 0;
    CPl *emptyPl = sourcePl->clone(attributes);

    return new CPolygonMesh(attributes, xform, emptyPl, 0, &dummy, &dummy, &dummy);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CLoopSubdivMesh
// Method				:	CLoopSubdivMesh
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CLoopSubdivMesh::CLoopSubdivMesh(CAttributes *a, CXform *x, CPl *c, int nf, int *numVerticesPerFace, int *vertexIndices) : CObject(a, x) {
    atomicIncrement(&stats.numGprims);

    pl = c;
    numFaces = nf;

    int total = 0;
    for (int i = 0; i < nf; i++)
        total += numVerticesPerFace[i];

    this->numVerticesPerFace = new int[nf];
    memcpy(this->numVerticesPerFace, numVerticesPerFace, nf * sizeof(int));

    this->vertexIndices = new int[total];
    memcpy(this->vertexIndices, vertexIndices, total * sizeof(int));

    numVertices = -1;
    for (int i = 0; i < total; i++) {
        if (vertexIndices[i] > numVertices)
            numVertices = vertexIndices[i];
    }
    numVertices++;

    initv(bmin, C_INFINITY);
    initv(bmax, -C_INFINITY);

    float *P = pl->data0;
    for (int i = 0; i < numVertices; i++, P += 3)
        addBox(bmin, bmax, P);

    if (pl->data1 != NULL) {
        P = pl->data1;
        for (int i = 0; i < numVertices; i++, P += 3)
            addBox(bmin, bmax, P);
    }

    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);

    osCreateMutex(mutex);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CLoopSubdivMesh
// Method				:	~CLoopSubdivMesh
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CLoopSubdivMesh::~CLoopSubdivMesh() {
    atomicDecrement(&stats.numGprims);

    delete pl;
    delete[] numVerticesPerFace;
    delete[] vertexIndices;

    osDeleteMutex(mutex);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CLoopSubdivMesh
// Method				:	intersect
// Description			:	Intersect a ray with the primitive
// Return Value			:	-
// Comments				:
void CLoopSubdivMesh::intersect(CShadingContext *rasterizer, CRay *) {

    if (children == NULL)
        create(rasterizer);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CLoopSubdivMesh
// Method				:	dice
// Description			:	Dice the primitive
// Return Value			:	-
// Comments				:
void CLoopSubdivMesh::dice(CReyes *rasterizer) {

    if (children == NULL)
        create(rasterizer);

    CObject::dice(rasterizer);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CLoopSubdivMesh
// Method				:	instantiate
// Description			:	Instantiate the primitive
// Return Value			:	-
// Comments				:
void CLoopSubdivMesh::instantiate(CAttributes *a, CXform *x, CRiInterface *c) const {
    CXform *nx = new CXform(x);
    nx->concat(xform);

    if (a == NULL)
        a = attributes;

    c->addObject(new CLoopSubdivMesh(a, nx, pl->clone(a), numFaces, numVerticesPerFace, vertexIndices));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CLoopSubdivMesh
// Method				:	create
// Description			:	Create the refined triangle mesh, wrapped in a
//							plain CPolygonMesh child
// Return Value			:	-
// Comments				:
void CLoopSubdivMesh::create(CShadingContext *context) {

    osLock(mutex);
    if (children != NULL) {
        osUnlock(mutex);
        return;
    }

    for (int i = 0; i < numFaces; i++) {
        if (numVerticesPerFace[i] != 3) {
            warning(CODE_RANGE, "Loop subdivision requires an all-triangle mesh; face %d has %d vertices\n", i, numVerticesPerFace[i]);
            setChildren(context, loopMakeEmptyFallback(attributes, xform, pl));
            osUnlock(mutex);
            return;
        }
    }

    std::vector<LoopVertexParam> vertexParams;
    for (int i = 0; i < pl->numParameters; i++) {
        if (pl->parameters[i].container == CONTAINER_VERTEX) {
            LoopVertexParam vp;
            vp.variable = pl->parameters[i].variable;
            vp.numFloats = vp.variable->numFloats;
            vp.oldIndex = pl->parameters[i].index;
            vertexParams.push_back(vp);
        }
    }

    if (vertexParams.empty()) {
        warning(CODE_RANGE, "Loop subdivision mesh has no vertex-class parameters\n");
        setChildren(context, loopMakeEmptyFallback(attributes, xform, pl));
        osUnlock(mutex);
        return;
    }

    const bool moving = (pl->data1 != NULL);
    int numV = numVertices;

    std::vector<std::vector<float>> data0(vertexParams.size());
    std::vector<std::vector<float>> data1(moving ? vertexParams.size() : 0);

    for (size_t p = 0; p < vertexParams.size(); p++) {
        const int nf = vertexParams[p].numFloats;
        const int idx = vertexParams[p].oldIndex;

        data0[p].assign(pl->data0 + idx, pl->data0 + idx + static_cast<size_t>(numV) * nf);

        if (moving)
            data1[p].assign(pl->data1 + idx, pl->data1 + idx + static_cast<size_t>(numV) * nf);
    }

    std::vector<int> triVerts(vertexIndices, vertexIndices + static_cast<size_t>(numFaces) * 3);

    for (int level = 0; level < kLoopSubdivisionLevels; level++)
        numV = loopRefineOnce(numV, triVerts, vertexParams, data0, moving ? &data1 : NULL);

    const int finalNumFaces = (int)(triVerts.size() / 3);

    int dataSize = 0;
    std::vector<int> paramIndex(vertexParams.size());

    for (size_t p = 0; p < vertexParams.size(); p++) {
        paramIndex[p] = dataSize;
        dataSize += vertexParams[p].numFloats * numV;
    }

    float *newData0 = new float[dataSize];
    float *newData1 = moving ? new float[dataSize] : NULL;

    for (size_t p = 0; p < vertexParams.size(); p++) {
        memcpy(newData0 + paramIndex[p], data0[p].data(), sizeof(float) * data0[p].size());

        if (moving)
            memcpy(newData1 + paramIndex[p], data1[p].data(), sizeof(float) * data1[p].size());
    }

    CPlParameter *newParameters = new CPlParameter[vertexParams.size()];

    for (size_t p = 0; p < vertexParams.size(); p++) {
        newParameters[p].variable = vertexParams[p].variable;
        newParameters[p].numItems = numV;
        newParameters[p].index = paramIndex[p];
        newParameters[p].container = CONTAINER_VERTEX;
    }

    CPl *newPl = new CPl(dataSize, (int)vertexParams.size(), newParameters, newData0, newData1);

    std::vector<int> nholes(finalNumFaces, 1);
    std::vector<int> nvertices(finalNumFaces, 3);

    CObject *poly = new CPolygonMesh(attributes, xform, newPl, finalNumFaces, nholes.data(), nvertices.data(), triVerts.data());

    setChildren(context, poly);

    osUnlock(mutex);
}
