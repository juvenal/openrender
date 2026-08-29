/**
 * Project: openRender
 *
 * File: blobbyPolygonize.cpp
 *
 * Description:
 *   This file implements the functionality for blobbyPolygonize.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	blobbyPolygonize.cpp
//  Classes				:	CBlobbyMesh
//  Description			:	Seeded continuation marching tetrahedra
//
////////////////////////////////////////////////////////////////////////
#include "blobbyPolygonize.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <deque>
#include <map>
#include <set>
#include <vector>

#include "atomic.h"
#include "error.h"
#include "stats.h"

///////////////////////////////////////////////////////////////////////
// A hard ceiling on the walk, so a pathological field cannot exhaust
// memory (FR-029). Reaching it is always a diagnostic: a healthy
// continuation walk visits cells proportional to surface *area*, and this
// is far above what any plausible scene needs.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_MAX_CELLS 2000000

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyLattice
// Description			:	Integer coordinates of one lattice point.
// Comments				:	Ordered by (i,j,k) so every container keyed by
//							it iterates in a defined order. This is not a
//							style preference: an unordered container would
//							pass every single-machine test and fail only as
//							a seam between two servers of a distributed
//							render, each of which derives its own copy of
//							the surface (FR-023a, research Decision 3).
///////////////////////////////////////////////////////////////////////
class CBlobbyLattice {
    public:
        int i, j, k;

        CBlobbyLattice() : i(0), j(0), k(0) {}
        CBlobbyLattice(int a, int b, int c) : i(a), j(b), k(c) {}

        bool operator<(const CBlobbyLattice &other) const {
            if (i != other.i)
                return i < other.i;
            if (j != other.j)
                return j < other.j;
            return k < other.k;
        }

        bool operator==(const CBlobbyLattice &other) const {
            return (i == other.i) && (j == other.j) && (k == other.k);
        }
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Description			:	Transient state of one seeded continuation walk.
// Comments				:	Destroyed when extraction completes; nothing
//							here survives into rendering.
///////////////////////////////////////////////////////////////////////
class CBlobbyWalk {
    public:
        CBlobbyWalk(const CBlobbyProgram *p, float size, int weights);

        CBlobbyMesh *run();

    private:
        void position(const CBlobbyLattice &lattice, float *P) const;
        float cornerValue(const CBlobbyLattice &lattice);
        int vertexOnEdge(const CBlobbyLattice &a, const CBlobbyLattice &b);
        void emitTriangle(int a, int b, int c);
        void marchTetrahedron(const CBlobbyLattice *corners, const float *values, const int *tet);
        int seedFrom(const float *P, int direction, CBlobbyLattice &cell);
        void pushCell(const CBlobbyLattice &cell);
        int scanForSeeds();
        int fieldHasNoBoundary() const;

        const CBlobbyProgram *program;
        float cellSize;
        int wantWeights;
        int numLeaves;

        vector origin;
        CBlobbyLattice lowerBound;
        CBlobbyLattice upperBound;

        std::map<CBlobbyLattice, float> cornerCache;
        std::set<CBlobbyLattice> visited;
        std::deque<CBlobbyLattice> frontier;
        std::map<std::pair<CBlobbyLattice, CBlobbyLattice>, int> edgeCache;

        std::vector<float> P;
        std::vector<float> N;
        std::vector<float> weights;
        std::vector<int> triangles;

        int overflowed;
};

///////////////////////////////////////////////////////////////////////
// The 6-tetrahedron decomposition of a cube (Kuhn's), by corner index
// dx + 2*dy + 4*dz. Every tetrahedron is a path from corner 0 to corner 7
// that flips one bit at a time, one per permutation of the three axes.
//
// This decomposition is what watertightness rests on. Applied with the
// same corner indexing in every cell, adjacent cells split their shared
// face along the *same* diagonal: cell A's +x face (corners 1,3,5,7, split
// along 1-7) is cell B's -x face (corners 0,2,4,6, split along 0-6), and
// A's 1 and 7 are B's 0 and 6. The same holds on the y and z faces. So no
// cell pair can disagree about the connectivity of the surface between
// them, which is precisely the failure marching cubes' ambiguous faces
// can produce (research Decision 2).
///////////////////////////////////////////////////////////////////////
static const int blobbyTetrahedra[6][4] = {
    {0, 1, 3, 7},
    {0, 1, 5, 7},
    {0, 2, 3, 7},
    {0, 2, 6, 7},
    {0, 4, 5, 7},
    {0, 4, 6, 7},
};

// Corner offsets, indexed the same way.
static const int blobbyCornerOffset[8][3] = {
    {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
    {0, 0, 1}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1},
};

// The four corners of each of the six faces, and the neighbour direction
// that face leads to.
static const int blobbyFaceCorners[6][4] = {
    {0, 2, 4, 6}, // -x
    {1, 3, 5, 7}, // +x
    {0, 1, 4, 5}, // -y
    {2, 3, 6, 7}, // +y
    {0, 1, 2, 3}, // -z
    {4, 5, 6, 7}, // +z
};

static const int blobbyFaceDirection[6][3] = {
    {-1, 0, 0}, {1, 0, 0},
    {0, -1, 0}, {0, 1, 0},
    {0, 0, -1}, {0, 0, 1},
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyMesh
// Method				:	CBlobbyMesh
// Description			:	Ctor
///////////////////////////////////////////////////////////////////////
CBlobbyMesh::CBlobbyMesh() {
    numVertices = 0;
    numTriangles = 0;
    P = NULL;
    N = NULL;
    weights = NULL;
    triangles = NULL;
    P1 = NULL;
    N1 = NULL;
    numLeaves = 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyMesh
// Method				:	~CBlobbyMesh
// Description			:	Dtor
///////////////////////////////////////////////////////////////////////
CBlobbyMesh::~CBlobbyMesh() {
    if (P != NULL)
        delete[] P;
    if (N != NULL)
        delete[] N;
    if (weights != NULL)
        delete[] weights;
    if (triangles != NULL)
        delete[] triangles;
    if (P1 != NULL)
        delete[] P1;
    if (N1 != NULL)
        delete[] N1;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	CBlobbyWalk
// Description			:	Ctor
///////////////////////////////////////////////////////////////////////
CBlobbyWalk::CBlobbyWalk(const CBlobbyProgram *p, float size, int weightsWanted) {
    program = p;
    cellSize = size;
    wantWeights = weightsWanted;
    numLeaves = p->getNumLeaves();
    overflowed = FALSE;

    vector bmin, bmax;

    program->getExtent(bmin, bmax);

    // The lattice origin is a pure function of the declaration, so two
    // servers deriving the same blobby lay their cells in the same places.
    movvv(origin, bmin);

    // A margin of two cells around the extent, so a surface that touches
    // the extent has somewhere to close. A program containing a repeller
    // has no bounded support across its ground plane, so give it a
    // proportionally larger box to walk in -- the walk still follows the
    // surface, this only bounds how far it may follow it.
    const float margin = program->hasUnboundedField() ? 0.5f : 0.0f;

    for (int i = 0; i < 3; i++) {
        const float span = bmax[i] - bmin[i];

        origin[i] -= margin * span;
    }

    lowerBound = CBlobbyLattice(-2, -2, -2);

    int extentCells[3];

    for (int i = 0; i < 3; i++) {
        const float span = (bmax[i] - bmin[i]) * (1 + 2 * margin);

        extentCells[i] = (int)ceilf(span / cellSize) + 2;
    }

    upperBound = CBlobbyLattice(extentCells[0], extentCells[1], extentCells[2]);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	position
// Description			:	Object-space position of a lattice point
///////////////////////////////////////////////////////////////////////
void CBlobbyWalk::position(const CBlobbyLattice &lattice, float *out) const {
    initv(out,
          origin[0] + cellSize * lattice.i,
          origin[1] + cellSize * lattice.j,
          origin[2] + cellSize * lattice.k);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	cornerValue
// Description			:	Field value at a lattice point, memoized
// Comments				:	Adjacent cells share four corners each, so the
//							cache saves roughly three quarters of the
//							evaluations the walk would otherwise make.
///////////////////////////////////////////////////////////////////////
float CBlobbyWalk::cornerValue(const CBlobbyLattice &lattice) {
    std::map<CBlobbyLattice, float>::const_iterator found = cornerCache.find(lattice);

    if (found != cornerCache.end())
        return found->second;

    vector p;

    position(lattice, p);

    const float value = program->evaluate(p, NULL);

    cornerCache[lattice] = value;

    return value;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	vertexOnEdge
// Description			:	Index of the vertex where the surface crosses
//							the edge between two lattice points, creating
//							it on first use.
// Return Value			:	Vertex index
// Comments				:	The edge is canonicalized before *both* the
//							cache lookup and the interpolation. Caching
//							under a canonical key while interpolating in
//							whichever direction the caller happened to
//							supply would still be wrong: a + t(b-a) and
//							b + t'(a-b) are not bit-identical, so two
//							tetrahedra meeting on this edge would place
//							their vertices a few ulps apart and the mesh
//							would leak along that seam -- looking like a
//							topology bug rather than a rounding one.
///////////////////////////////////////////////////////////////////////
int CBlobbyWalk::vertexOnEdge(const CBlobbyLattice &a, const CBlobbyLattice &b) {
    const CBlobbyLattice &low = (a < b) ? a : b;
    const CBlobbyLattice &high = (a < b) ? b : a;
    const std::pair<CBlobbyLattice, CBlobbyLattice> key(low, high);

    std::map<std::pair<CBlobbyLattice, CBlobbyLattice>, int>::const_iterator found = edgeCache.find(key);

    if (found != edgeCache.end())
        return found->second;

    const float valueLow = cornerValue(low);
    const float valueHigh = cornerValue(high);
    vector pLow, pHigh, p;

    position(low, pLow);
    position(high, pHigh);

    const float denominator = valueHigh - valueLow;
    float t = 0.5f;

    if (denominator > C_EPSILON || denominator < -C_EPSILON)
        t = (BLOBBY_THRESHOLD - valueLow) / denominator;

    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;

    for (int i = 0; i < 3; i++)
        p[i] = pLow[i] + t * (pHigh[i] - pLow[i]);

    // Per-vertex normals are the normalized analytic gradient evaluated at
    // the vertex, never differenced from neighbouring facets (FR-024). The
    // field decreases outward, so the outward normal is the negated
    // gradient.
    vector gradient;
    float *leafWeights = NULL;
    const int index = (int)(P.size() / 3);

    if (wantWeights && numLeaves > 0) {
        weights.resize(weights.size() + numLeaves);
        leafWeights = &weights[weights.size() - numLeaves];

        // The expensive entry point, called only here -- once per emitted
        // vertex -- and never on the traversal path. Weights cost an
        // O(numLeaves) write per call, and the walk makes orders of
        // magnitude more field evaluations than it emits vertices, so
        // folding the two entry points together would directly undermine
        // SC-012 on the 500-field spiral (data-model.md 2).
        program->evaluateWeights(p, gradient, leafWeights);
    }
    else {
        program->evaluate(p, gradient);
    }

    const float length = sqrtf(dotvv(gradient, gradient));
    vector normal;

    if (length > C_EPSILON) {
        const float inverse = -1 / length;

        initv(normal, gradient[0] * inverse, gradient[1] * inverse, gradient[2] * inverse);
    }
    else {
        // The gradient legitimately vanishes at a lone blob's centre and
        // at its rim, and can vanish where operands cancel. Normalizing
        // there would produce a NaN normal that silently blackens the
        // shading, so fall back to the edge's own direction, oriented from
        // the inside corner to the outside one. Deterministic, non-zero,
        // and locally correct to first order.
        vector direction;

        subvv(direction, pHigh, pLow);

        if (valueLow < valueHigh) {
            direction[0] = -direction[0];
            direction[1] = -direction[1];
            direction[2] = -direction[2];
        }

        const float directionLength = sqrtf(dotvv(direction, direction));

        if (directionLength > C_EPSILON) {
            const float inverse = 1 / directionLength;

            initv(normal, direction[0] * inverse, direction[1] * inverse, direction[2] * inverse);
        }
        else {
            initv(normal, 0, 0, 1);
        }
    }

    for (int i = 0; i < 3; i++)
        P.push_back(p[i]);
    for (int i = 0; i < 3; i++)
        N.push_back(normal[i]);

    edgeCache[key] = index;

    return index;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	emitTriangle
// Description			:	Append one triangle, wound so its geometric
//							normal agrees with its vertices' analytic ones.
// Comments				:	Marching tetrahedra's case table fixes the
//							vertices but not the orientation, and the
//							renderer's culling and two-sidedness depend on
//							it. Deriving the winding from the normals the
//							field already gives us keeps the two consistent
//							without a second case table to get wrong.
///////////////////////////////////////////////////////////////////////
void CBlobbyWalk::emitTriangle(int a, int b, int c) {
    if (a == b || b == c || a == c)
        return;

    vector edge1, edge2, facet, average;

    subvv(edge1, &P[b * 3], &P[a * 3]);
    subvv(edge2, &P[c * 3], &P[a * 3]);
    crossvv(facet, edge1, edge2);

    initv(average,
          N[a * 3 + 0] + N[b * 3 + 0] + N[c * 3 + 0],
          N[a * 3 + 1] + N[b * 3 + 1] + N[c * 3 + 1],
          N[a * 3 + 2] + N[b * 3 + 2] + N[c * 3 + 2]);

    if (dotvv(facet, average) < 0) {
        const int swap = b;

        b = c;
        c = swap;
    }

    triangles.push_back(a);
    triangles.push_back(b);
    triangles.push_back(c);

    atomicIncrement(&stats.numBlobbyTriangles);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	marchTetrahedron
// Description			:	Triangulate one tetrahedron's sign configuration
// Comments				:	Every configuration has exactly one
//							triangulation -- one triangle when a single
//							corner is on its own side, two when the split
//							is two against two -- which is why there is no
//							ambiguity to disambiguate here.
///////////////////////////////////////////////////////////////////////
void CBlobbyWalk::marchTetrahedron(const CBlobbyLattice *corners, const float *values, const int *tet) {
    int inside[4], outside[4];
    int numInside = 0, numOutside = 0;

    for (int i = 0; i < 4; i++) {
        if (values[tet[i]] >= BLOBBY_THRESHOLD)
            inside[numInside++] = tet[i];
        else
            outside[numOutside++] = tet[i];
    }

    if (numInside == 0 || numInside == 4)
        return;

    if (numInside == 1) {
        emitTriangle(vertexOnEdge(corners[inside[0]], corners[outside[0]]),
                     vertexOnEdge(corners[inside[0]], corners[outside[1]]),
                     vertexOnEdge(corners[inside[0]], corners[outside[2]]));
    }
    else if (numInside == 3) {
        emitTriangle(vertexOnEdge(corners[outside[0]], corners[inside[0]]),
                     vertexOnEdge(corners[outside[0]], corners[inside[1]]),
                     vertexOnEdge(corners[outside[0]], corners[inside[2]]));
    }
    else {
        const int v00 = vertexOnEdge(corners[inside[0]], corners[outside[0]]);
        const int v01 = vertexOnEdge(corners[inside[0]], corners[outside[1]]);
        const int v11 = vertexOnEdge(corners[inside[1]], corners[outside[1]]);
        const int v10 = vertexOnEdge(corners[inside[1]], corners[outside[0]]);

        emitTriangle(v00, v01, v11);
        emitTriangle(v00, v11, v10);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	pushCell
// Description			:	Add a cell to the frontier if it is new
///////////////////////////////////////////////////////////////////////
void CBlobbyWalk::pushCell(const CBlobbyLattice &cell) {
    if (cell.i < lowerBound.i || cell.i > upperBound.i)
        return;
    if (cell.j < lowerBound.j || cell.j > upperBound.j)
        return;
    if (cell.k < lowerBound.k || cell.k > upperBound.k)
        return;

    if (visited.find(cell) == visited.end()) {
        visited.insert(cell);
        frontier.push_back(cell);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	seedFrom
// Description			:	Find a cell the surface crosses, walking outward
//							from a primitive field's own centre along one
//							axis direction.
// Return Value			:	TRUE if one was found
// Comments				:	The centre is usually deep inside the solid, so
//							the surface has to be found by walking out to
//							it. All six axis directions are tried, in a
//							fixed order, and every crossing found is
//							seeded.
//
//							Searching only one direction is not enough, and
//							the case that shows it is AppNote #31's own
//							dent figure: a unit blob with a long thin
//							ellipsoid subtracted through it along x. Both
//							fields are centred at the origin, and along the
//							whole +x axis the difference stays below the
//							threshold -- the rod is subtracting exactly
//							there -- while the surface is very much present
//							off-axis. A single-direction search finds
//							nothing and the primitive silently disappears.
//
//							Bounded by the lattice, so a field whose
//							surface does not exist contributes no seed
//							rather than searching forever.
///////////////////////////////////////////////////////////////////////
int CBlobbyWalk::seedFrom(const float *p, int direction, CBlobbyLattice &cell) {
    CBlobbyLattice start((int)floorf((p[0] - origin[0]) / cellSize),
                         (int)floorf((p[1] - origin[1]) / cellSize),
                         (int)floorf((p[2] - origin[2]) / cellSize));

    const int *step = blobbyFaceDirection[direction];
    float previous = cornerValue(start);

    for (int n = 1;; n++) {
        const CBlobbyLattice probe(start.i + step[0] * n,
                                   start.j + step[1] * n,
                                   start.k + step[2] * n);

        if (probe.i < lowerBound.i || probe.i > upperBound.i)
            break;
        if (probe.j < lowerBound.j || probe.j > upperBound.j)
            break;
        if (probe.k < lowerBound.k || probe.k > upperBound.k)
            break;

        const float value = cornerValue(probe);

        if ((previous >= BLOBBY_THRESHOLD) != (value >= BLOBBY_THRESHOLD)) {
            // The cell between the two probes, named by its lowest corner.
            cell = CBlobbyLattice(start.i + step[0] * n - (step[0] > 0 ? 1 : 0),
                                  start.j + step[1] * n - (step[1] > 0 ? 1 : 0),
                                  start.k + step[2] * n - (step[2] > 0 ? 1 : 0));
            return TRUE;
        }

        previous = value;
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	scanForSeeds
// Description			:	Last-resort coarse scan of the extent for a cell
//							the surface crosses.
// Return Value			:	Number of seeds found
// Comments				:	Only reached when no primitive field's own
//							centre led to the surface -- which happens when
//							every field is cancelled at its own centre, as
//							a blob with something subtracted right through
//							it can be. Deliberately strided: this is a
//							volumetric scan, the very thing SC-012 exists
//							to keep off the normal path, so it costs
//							1/512th of the lattice and runs only when the
//							alternative is losing the primitive entirely.
//							In (i,j,k) order, so it is as deterministic as
//							the rest of the walk.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_SCAN_STRIDE 8

int CBlobbyWalk::scanForSeeds() {
    int found = 0;

    for (int i = lowerBound.i; i <= upperBound.i; i += BLOBBY_SCAN_STRIDE) {
        for (int j = lowerBound.j; j <= upperBound.j; j += BLOBBY_SCAN_STRIDE) {
            float previous = cornerValue(CBlobbyLattice(i, j, lowerBound.k));

            for (int k = lowerBound.k + 1; k <= upperBound.k; k++) {
                const float value = cornerValue(CBlobbyLattice(i, j, k));

                if ((previous >= BLOBBY_THRESHOLD) != (value >= BLOBBY_THRESHOLD)) {
                    pushCell(CBlobbyLattice(i, j, k - 1));
                    found++;
                }

                previous = value;
            }
        }
    }

    return found;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	fieldHasNoBoundary
// Description			:	TRUE when the field is at or above the
//							threshold everywhere the walk could reach.
// Comments				:	The dual of "the field never reaches the
//							threshold". That case yields no geometry and no
//							error (FR-030); this one has no boundary to
//							find at all, so a walk looking for one would
//							expand outward indefinitely. It must terminate
//							promptly and say why (Edge Case 10).
///////////////////////////////////////////////////////////////////////
int CBlobbyWalk::fieldHasNoBoundary() const {
    if (!program->hasBoundedExtent()) {
        // Nothing bounds the field spatially -- a lone constant, say. If it
        // is above the threshold at all, it is above it everywhere.
        const vector probe = {0, 0, 0};

        return program->evaluate(probe, NULL) >= BLOBBY_THRESHOLD;
    }

    vector bmin, bmax;

    program->getExtent(bmin, bmax);

    // Sample the corners of the extent, expanded outward. A field with a
    // real surface has fallen below the threshold well before here.
    for (int corner = 0; corner < 8; corner++) {
        vector p;

        for (int i = 0; i < 3; i++) {
            const float span = bmax[i] - bmin[i];

            p[i] = (corner & (1 << i)) ? bmax[i] + 0.25f * span + cellSize : bmin[i] - 0.25f * span - cellSize;
        }

        if (program->evaluate(p, NULL) < BLOBBY_THRESHOLD)
            return FALSE;
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyWalk
// Method				:	run
// Description			:	Seeded continuation walk
// Return Value			:	The mesh, or NULL when there is no surface
///////////////////////////////////////////////////////////////////////
CBlobbyMesh *CBlobbyWalk::run() {
    if (program->getNumInstructions() == 0)
        return NULL;

    if (fieldHasNoBoundary()) {
        error(CODE_RANGE, "Blobby: the combined field is at or above the surface threshold everywhere, so it has no surface; no geometry emitted\n");
        return NULL;
    }

    // Seeds come from the primitive fields, in code-array order, six axis
    // directions each. Fields with no natural centre -- a constant, a
    // repeller -- contribute no seed; their part of the surface is reached
    // by continuation from a neighbouring field's seed instead.
    for (int leaf = 0; leaf < numLeaves; leaf++) {
        vector seedPoint;

        if (!program->getLeafSeed(leaf, seedPoint))
            continue;

        for (int direction = 0; direction < 6; direction++) {
            CBlobbyLattice cell;

            if (seedFrom(seedPoint, direction, cell))
                pushCell(cell);
        }
    }

    if (frontier.empty())
        scanForSeeds();

    // FIFO, not LIFO: the traversal order has to be reproducible, and a
    // stack's order depends on the seeding order in a way a queue's does
    // not.
    while (!frontier.empty()) {
        const CBlobbyLattice cell = frontier.front();

        frontier.pop_front();

        if ((int)visited.size() > BLOBBY_MAX_CELLS) {
            if (!overflowed) {
                error(CODE_LIMIT, "Blobby: extraction exceeded %d cells; the tolerance is too fine for this primitive's extent\n", BLOBBY_MAX_CELLS);
                overflowed = TRUE;
            }
            break;
        }

        atomicIncrement(&stats.numBlobbyCellsVisited);

        CBlobbyLattice corners[8];
        float values[8];
        int numInside = 0;

        for (int c = 0; c < 8; c++) {
            corners[c] = CBlobbyLattice(cell.i + blobbyCornerOffset[c][0],
                                        cell.j + blobbyCornerOffset[c][1],
                                        cell.k + blobbyCornerOffset[c][2]);
            values[c] = cornerValue(corners[c]);

            if (values[c] >= BLOBBY_THRESHOLD)
                numInside++;
        }

        if (numInside == 0 || numInside == 8)
            continue;

        atomicIncrement(&stats.numBlobbySurfaceCells);

        for (int t = 0; t < 6; t++)
            marchTetrahedron(corners, values, blobbyTetrahedra[t]);

        // Continue only across faces the surface actually crosses. That is
        // what makes the cost track the surface rather than the bounding
        // volume (SC-012): a closed surface leaves a cell through one of
        // its faces, and such a face necessarily has corners on both sides.
        //
        // A consequence worth stating, because it looks like a bug the
        // first time it is seen: where the solid is *thinner than a cell*,
        // the sampled level set pinches off into a separate closed piece,
        // so a shape that is connected in the field can extract as several
        // components. AppNote #31's own unblended cluster does this at the
        // tips of the six caps that poke into its central void. The mesh
        // stays watertight either way -- which is what FR-027 depends on --
        // and a tighter tolerance resolves the connection. This is inherent
        // to sampling an implicit surface, not particular to continuation.
        for (int f = 0; f < 6; f++) {
            int faceInside = 0;

            for (int c = 0; c < 4; c++) {
                if (values[blobbyFaceCorners[f][c]] >= BLOBBY_THRESHOLD)
                    faceInside++;
            }

            if (faceInside == 0 || faceInside == 4)
                continue;

            pushCell(CBlobbyLattice(cell.i + blobbyFaceDirection[f][0],
                                    cell.j + blobbyFaceDirection[f][1],
                                    cell.k + blobbyFaceDirection[f][2]));
        }
    }

    if (triangles.empty())
        return NULL;

    CBlobbyMesh *mesh = new CBlobbyMesh;

    mesh->numVertices = (int)(P.size() / 3);
    mesh->numTriangles = (int)(triangles.size() / 3);
    mesh->numLeaves = numLeaves;

    mesh->P = new float[P.size()];
    memcpy(mesh->P, &P[0], sizeof(float) * P.size());

    mesh->N = new float[N.size()];
    memcpy(mesh->N, &N[0], sizeof(float) * N.size());

    mesh->triangles = new int[triangles.size()];
    memcpy(mesh->triangles, &triangles[0], sizeof(int) * triangles.size());

    if (wantWeights && numLeaves > 0 && !weights.empty()) {
        mesh->weights = new float[weights.size()];
        memcpy(mesh->weights, &weights[0], sizeof(float) * weights.size());
    }

    return mesh;
}

///////////////////////////////////////////////////////////////////////
// Motion: advecting the shutter-open surface onto the shutter-close one
//
// A *fixed* number of Newton steps, never "until converged". This is the
// determinism trap the whole motion path turns on: a convergence test makes
// the step count a floating-point predicate, so the same input can take a
// different number of steps under different compiler flags or FMA
// contraction and land the vertex somewhere else. Each render server
// derives its own copy of the surface from the same declaration, so that
// divergence is a seam where two servers' geometry meets -- worst exactly
// at the vertices near a topology change, where convergence is most
// marginal (research Decision 8).
//
// The only branch is a guard against a degenerate gradient, which is a
// property of the field at that point rather than of how close the
// iteration has got. Where the surface has ceased to exist there is no
// correct destination, so a vertex simply stops after its allotted steps:
// bounded and locally unblurred rather than wild (US8 scenario 4).
//
// There is a second, quieter limit worth naming. A gradient step can only
// follow a field it can feel, and a blobby field is *exactly* zero outside
// its own support. So a vertex that starts outside every shutter-close
// field -- which happens once the motion within the shutter exceeds a
// blob's own radius -- has a zero gradient and stays where it is. That is
// the same "bounded but not faithful" outcome FR-026 allows for topology
// change, reached by a different route, and it is a property of gradient
// advection rather than of this implementation of it.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_ADVECT_STEPS 12

// No single step may move a vertex further than this many cells. A Newton
// step on a nearly-flat field is arbitrarily long, and one such step would
// throw a vertex clear across the primitive.
#define BLOBBY_ADVECT_MAX_CELLS 4.0f

static void blobbyAdvect(const CBlobbyProgram *closeProgram, CBlobbyMesh *mesh, float cellSize) {
    const float limit = BLOBBY_ADVECT_MAX_CELLS * cellSize;

    mesh->P1 = new float[mesh->numVertices * 3];
    mesh->N1 = new float[mesh->numVertices * 3];

    for (int i = 0; i < mesh->numVertices; i++) {
        vector p, gradient;

        movvv(p, mesh->P + i * 3);

        for (int step = 0; step < BLOBBY_ADVECT_STEPS; step++) {
            const float value = closeProgram->evaluate(p, gradient);
            const float denominator = dotvv(gradient, gradient);
            float scale = 0;

            // Newton on F(p) = T:  p <- p - (F - T) * grad(F) / |grad(F)|^2.
            // The sign matters and is easy to get backwards: the field
            // *increases* along its gradient, so a point with too much
            // field has to move against it.
            if (denominator > C_EPSILON)
                scale = -(value - BLOBBY_THRESHOLD) / denominator;

            vector offset;

            initv(offset, scale * gradient[0], scale * gradient[1], scale * gradient[2]);

            const float length = sqrtf(dotvv(offset, offset));

            if (length > limit) {
                const float clamped = limit / length;

                offset[0] *= clamped;
                offset[1] *= clamped;
                offset[2] *= clamped;
            }

            addvv(p, offset);
        }

        movvv(mesh->P1 + i * 3, p);

        // The normal at the second sample comes from the close field at
        // the advected position -- the same analytic gradient the first
        // sample's does, evaluated where the vertex actually ended up.
        closeProgram->evaluate(p, gradient);

        const float length = sqrtf(dotvv(gradient, gradient));
        float *destination = mesh->N1 + i * 3;

        if (length > C_EPSILON) {
            const float inverse = -1 / length;

            initv(destination, gradient[0] * inverse, gradient[1] * inverse, gradient[2] * inverse);
        }
        else {
            // Nothing better to say than what the first sample said.
            movvv(destination, mesh->N + i * 3);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyPolygonize
// Description			:	Extract the threshold level set
///////////////////////////////////////////////////////////////////////
CBlobbyMesh *blobbyPolygonize(const CBlobbyProgram *program, float cellSize, int wantWeights, const CBlobbyProgram *closeProgram) {
    if (program == NULL || !program->isValid())
        return NULL;

    if (!(cellSize > 0))
        return NULL;

    CBlobbyWalk walk(program, cellSize, wantWeights);
    CBlobbyMesh *mesh = walk.run();

    if (mesh != NULL && closeProgram != NULL && closeProgram->isValid() && mesh->numVertices > 0)
        blobbyAdvect(closeProgram, mesh, cellSize);

    return mesh;
}
