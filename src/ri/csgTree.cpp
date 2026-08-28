/**
 * Project: openRender
 *
 * File: csgTree.cpp
 *
 * Description:
 *   This file contains the implementation of csgTree.
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
//  File				:	csgTree.cpp
//  Classes				:	CSGTreeNode
//  Description			:	Build-time RiSolidBegin/RiSolidEnd capture tree
//
////////////////////////////////////////////////////////////////////////
#include "csgTree.h"
#include "csgBoolean.h"
#include "error.h"
#include "object.h"
#include "patches.h"
#include "pl.h"
#include "polygons.h"
#include "rendererContext.h"
#include "solidObject.h"
#include "subdivisionCreator.h"
#include "subdivisionLoop.h"
#include "surface.h"
#include "variable.h"
#include "xform.h"

#include <assert.h>
#include <math.h>

// Fallback tessellation tolerance for a leaf operand whose
// Attribute "solid" "tessellationtolerance" is unset (0): a fraction of
// the leaf's own bound diagonal, so differently-sized primitives each get
// a reasonable absolute tolerance without one global default.
static const float kCsgDefaultToleranceFraction = 0.001f;

///////////////////////////////////////////////////////////////////////
// Class				:	CSGTreeNode
// Method				:	CSGTreeNode
// Description			:	Ctor
// Return Value			:
// Comments				:
CSGTreeNode::CSGTreeNode(ECSGOperation operation, CSGTreeNode *parent) {
    this->operation = operation;
    this->parent    = parent;

    operands    = new CArray<CSGTreeNode *>;
    leafObjects = NULL;
    outerXform  = NULL;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSGTreeNode
// Method				:	~CSGTreeNode
// Description			:	Dtor
// Return Value			:
// Comments				:
CSGTreeNode::~CSGTreeNode() {
    delete operands;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgValidateNestedSolidBegin
// Description			:	FR-019
// Return Value			:
// Comments				:
void csgValidateNestedSolidBegin(CSGTreeNode *parent) {
    if (parent->operation == CSG_PRIMITIVE) {
        error(CODE_BADTOKEN, "SolidBegin/SolidEnd cannot be nested inside a \"primitive\" solid block\n");
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgValidateProceduralCapture
// Description			:	FR-021
// Return Value			:
// Comments				:
int csgValidateProceduralCapture(CSGTreeNode *node) {
    assert(node != NULL);

    error(CODE_BADTOKEN, "RiProcedural cannot be declared inside a SolidBegin block\n");
    return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgResolveTolerance
// Description			:	Resolves the tessellation tolerance for one CSG
//							leaf operand: the explicit Attribute "solid"
//							"tessellationtolerance" value if set (> 0),
//							otherwise a default scaled off the leaf's own
//							bound diagonal.
// Return Value			:	The tolerance to hand to tesselateQuadricAdaptive
//							/ tesselatePatchMeshAdaptive
// Comments				:
static float csgResolveTolerance(CObject *leaf) {
    if (leaf->attributes->tessellationTolerance > 0.0f)
        return leaf->attributes->tessellationTolerance;

    vector diagonal;
    subvv(diagonal, leaf->bmax, leaf->bmin);
    float size = lengthv(diagonal);

    return (size > C_EPSILON) ? size * kCsgDefaultToleranceFraction : C_EPSILON;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgMakeGridVertex
// Description			:	Builds one CCSGVertex from a tessellated grid's
//							(i,j) sample, deriving an analytic shading
//							normal from dPdu x dPdv when the grid carries
//							derivatives (T023).
// Return Value			:	The vertex
// Comments				:
static CCSGVertex csgMakeGridVertex(const CTesselatedGrid &grid, int i, int j) {
    const int n = grid.div + 1;
    const int k = i * n + j;

    CCSGVertex v;
    v.p[0]      = grid.P[k * 3 + 0];
    v.p[1]      = grid.P[k * 3 + 1];
    v.p[2]      = grid.P[k * 3 + 2];
    v.hasNormal = FALSE;

    if (grid.dPdu != NULL && grid.dPdv != NULL) {
        vector normal;
        crossvv(normal, grid.dPdu + k * 3, grid.dPdv + k * 3);
        float len = lengthv(normal);

        if (len > 0.0f) {
            v.n[0]      = normal[0] / len;
            v.n[1]      = normal[1] / len;
            v.n[2]      = normal[2] / len;
            v.hasNormal = TRUE;
        }
    }

    return v;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgTriangleIsDegenerate
// Description			:	Detects a zero-area triangle (coincident
//							corners), the same C_EPSILON-squared convention
//							normalFix() uses for degenerate-vertex detection.
// Return Value			:	TRUE if the triangle's area is ~0
// Comments				:	A grid cell adjacent to a pole/seam still has
//							its other, non-degenerate triangle emitted --
//							that surviving fan triangle closes the pole
//							correctly on its own.
static int csgTriangleIsDegenerate(const CCSGVertex &a, const CCSGVertex &b, const CCSGVertex &c) {
    vector e1, e2, n;
    subvv(e1, b.p, a.p);
    subvv(e2, c.p, a.p);
    crossvv(n, e1, e2);

    return dotvv(n, n) < C_EPSILON * C_EPSILON;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgEmitTriangle
// Description			:	Appends one triangle to `out` as a CCSGPolygon
//							tagged with `attr`, unless it's degenerate.
// Return Value			:
// Comments				:
static void csgEmitTriangle(const CCSGVertex &a, const CCSGVertex &b, const CCSGVertex &c, CAttributes *attr, CArray<CCSGPolygon *> *out) {
    if (csgTriangleIsDegenerate(a, b, c))
        return;

    CCSGPolygon *poly = new CCSGPolygon();
    poly->vertices.push(a);
    poly->vertices.push(b);
    poly->vertices.push(c);
    poly->attributes = attr;
    poly->computePlane();

    out->push(poly);
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgTriangulateGrid
// Description			:	Triangulates a (div+1)x(div+1) tessellated grid
//							into CCSGPolygons, 2 triangles per cell.
// Return Value			:
// Comments				:
static void csgTriangulateGrid(const CTesselatedGrid &grid, CAttributes *attr, CArray<CCSGPolygon *> *out) {
    for (int i = 0; i < grid.div; i++) {
        for (int j = 0; j < grid.div; j++) {
            CCSGVertex p00 = csgMakeGridVertex(grid, i, j);
            CCSGVertex p10 = csgMakeGridVertex(grid, i + 1, j);
            CCSGVertex p11 = csgMakeGridVertex(grid, i + 1, j + 1);
            CCSGVertex p01 = csgMakeGridVertex(grid, i, j + 1);

            csgEmitTriangle(p00, p10, p11, attr, out);
            csgEmitTriangle(p00, p11, p01, attr, out);
        }
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgSignedVolume
// Description			:	Divergence-theorem signed volume of a closed
//							polygon soup (fan-triangulated per polygon).
//							Formula matches csgtest::computeVolume
//							(tests/unit/csg/csgTestUtils.h) exactly, so the
//							sign convention this relies on to normalize
//							operand orientation is the one T013-T016's tests
//							are calibrated against.
// Return Value			:	Signed volume (positive for outward-facing/CCW)
// Comments				:
static double csgSignedVolume(CArray<CCSGPolygon *> *polys) {
    double volume = 0.0;

    for (int i = 0; i < polys->numItems; i++) {
        CCSGPolygon *poly = polys->array[i];
        int n = poly->vertices.numItems;

        for (int k = 1; k + 1 < n; k++) {
            const float *v0 = poly->vertices.array[0].p;
            const float *v1 = poly->vertices.array[k].p;
            const float *v2 = poly->vertices.array[k + 1].p;
            double cx = (double)v1[1] * v2[2] - (double)v1[2] * v2[1];
            double cy = (double)v1[2] * v2[0] - (double)v1[0] * v2[2];
            double cz = (double)v1[0] * v2[1] - (double)v1[1] * v2[0];

            volume += (double)v0[0] * cx + (double)v0[1] * cy + (double)v0[2] * cz;
        }
    }

    return volume / 6.0;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgFlipAll
// Description			:	Flips every polygon in `polys` in place.
// Return Value			:
// Comments				:
static void csgFlipAll(CArray<CCSGPolygon *> *polys) {
    for (int i = 0; i < polys->numItems; i++)
        polys->array[i]->flip();
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgTessellateOperand
// Description			:	Tessellates one CSG operand -- a whole sibling
//							chain of captured leaf CObjects (a "primitive"
//							node may capture more than one Ri call) -- into
//							one outward-oriented polygon soup (T022/T023).
//							Every leaf consumed is pushed onto
//							`consumedLeaves` rather than detached here, so
//							the caller can release them only once every
//							CPolygonMesh that will reference their
//							CAttributes* (via its own independent attach())
//							has already been built.
// Return Value			:	A newly allocated polygon soup (possibly
//							empty)
// Comments				:	Also dispatches CLoopSubdivMesh (Loop
//							subdivision, via buildPolygonMesh() + the same
//							CPolygonMesh path above), CSubdivMesh
//							(Catmull-Clark, via tessellateToSurfaces() into a
//							CSurface patch chain), and CNURBSPatchMesh (RiNuPatch,
//							via tesselateNURBSPatchMeshAdaptive() into a flat
//							count-sized grid array) -- all resolve entirely in
//							the geometry domain via a standalone memory pool,
//							independent of any hider/CShadingContext.
static CArray<CCSGPolygon *> *csgTessellateOperand(CObject *operandHead, CArray<CObject *> *consumedLeaves) {
    CArray<CCSGPolygon *> *soup = new CArray<CCSGPolygon *>;

    for (CObject *leaf = operandHead; leaf != NULL; leaf = leaf->sibling) {
        CSurface *surf     = dynamic_cast<CSurface *>(leaf);
        CPatchMesh *mesh   = (surf == NULL) ? dynamic_cast<CPatchMesh *>(leaf) : NULL;

        if (surf != NULL) {
            float tolerance     = csgResolveTolerance(leaf);
            CTesselatedGrid grid = tesselateQuadricAdaptive(surf, tolerance, TRUE);

            csgTriangulateGrid(grid, leaf->attributes, soup);

            delete[] grid.P;
            delete[] grid.dPdu;
            delete[] grid.dPdv;
        } else if (mesh != NULL) {
            float tolerance               = csgResolveTolerance(leaf);
            CTesselatedPatchMeshOperand op = tesselatePatchMeshAdaptive(mesh, tolerance, TRUE);

            int total = op.uPatches * op.vPatches;
            for (int k = 0; k < total; k++) {
                csgTriangulateGrid(op.grids[k], leaf->attributes, soup);
                delete[] op.grids[k].P;
                delete[] op.grids[k].dPdu;
                delete[] op.grids[k].dPdv;
            }
            delete[] op.grids;
        } else if (CNURBSPatchMesh *nurbsMesh = dynamic_cast<CNURBSPatchMesh *>(leaf)) {
            float tolerance                     = csgResolveTolerance(leaf);
            CTesselatedNURBSPatchMeshOperand op = tesselateNURBSPatchMeshAdaptive(nurbsMesh, tolerance, TRUE);

            for (int k = 0; k < op.count; k++) {
                csgTriangulateGrid(op.grids[k], leaf->attributes, soup);
                delete[] op.grids[k].P;
                delete[] op.grids[k].dPdu;
                delete[] op.grids[k].dPdv;
            }
            delete[] op.grids;
        } else if (CPolygonMesh *polyMesh = dynamic_cast<CPolygonMesh *>(leaf)) {
            float tolerance = csgResolveTolerance(leaf);

            // Each CPolygonTriangle/CPolygonQuad constructed below attach()es
            // polyMesh (it's their shared owner of ->pl) and detach()es it in
            // its own destructor. polyMesh itself starts at refCount 0 (see
            // addObject's currentSolid branch -- captured "primitive" leaves
            // are never attach()'d), so without this keep-alive attach() the
            // last `delete tri` below drives polyMesh's refcount to 0 and
            // self-deletes it (via CRefCounter::detach()) while it is still
            // `leaf` -- the outer for-loop then reads freed memory at
            // `leaf->sibling`. This attach() is intentionally left
            // unbalanced here: it is paired with the single detach() that
            // resolveCSGTree runs over consumedLeaves once soup-building is
            // fully done (csgTree.cpp, the `consumedLeaves.array[i]->detach()`
            // loop), which is what actually frees polyMesh -- the same point
            // every other leaf type's capture-time (non-)reference is
            // released.
            polyMesh->attach();

            // Triangulates polyMesh into a sibling chain of
            // CPolygonTriangle/CPolygonQuad -- both CSurface subclasses --
            // reusing polyMesh's own proven ear-clipping-with-holes
            // decomposition (see csgTessellatePolygonMeshOperand in
            // polygons.cpp). None of the returned objects are attach()'d
            // (setChildren() is never called), so each must be delete'd
            // directly, not detach()'d, once its grid has been extracted.
            CObject *tris = csgTessellatePolygonMeshOperand(polyMesh);

            CObject *tri = tris;
            while (tri != NULL) {
                CObject *next = tri->sibling;
                CSurface *triSurf = dynamic_cast<CSurface *>(tri);
                assert(triSurf != NULL);

                CTesselatedGrid grid = tesselateQuadricAdaptive(triSurf, tolerance, TRUE);

                csgTriangulateGrid(grid, leaf->attributes, soup);

                delete[] grid.P;
                delete[] grid.dPdu;
                delete[] grid.dPdv;

                delete tri;
                tri = next;
            }
        } else if (CLoopSubdivMesh *loopMesh = dynamic_cast<CLoopSubdivMesh *>(leaf)) {
            float tolerance = csgResolveTolerance(leaf);

            // buildPolygonMesh() synthesizes a brand-new CPolygonMesh each
            // call, entirely independent of `leaf`'s own create()/children
            // cache -- unlike the captured CPolygonMesh leaf case above,
            // nothing else ever attach()es or releases this object, so we
            // hold it alive ourselves across the tessellation loop below
            // (mirroring that same attach-for-the-loop / detach-when-done
            // pattern) and detach() it explicitly once done, which is what
            // actually frees it.
            CPolygonMesh *polyMesh = dynamic_cast<CPolygonMesh *>(loopMesh->buildPolygonMesh());
            assert(polyMesh != NULL);

            polyMesh->attach();

            CObject *tris = csgTessellatePolygonMeshOperand(polyMesh);

            CObject *tri = tris;
            while (tri != NULL) {
                CObject *next = tri->sibling;
                CSurface *triSurf = dynamic_cast<CSurface *>(tri);
                assert(triSurf != NULL);

                CTesselatedGrid grid = tesselateQuadricAdaptive(triSurf, tolerance, TRUE);

                csgTriangulateGrid(grid, leaf->attributes, soup);

                delete[] grid.P;
                delete[] grid.dPdu;
                delete[] grid.dPdv;

                delete tri;
                tri = next;
            }

            polyMesh->detach();
        } else if (CSubdivMesh *subdivMesh = dynamic_cast<CSubdivMesh *>(leaf)) {
            float tolerance = csgResolveTolerance(leaf);

            // tessellateToSurfaces() synthesizes a fresh sibling chain of
            // CSurface patches (CBicubicPatch/CPatchGrid/CBSplinePatchGrid/
            // CSubdivision) via a standalone memory pool. Each patch's
            // vertex data is ralloc'd from that pool, not copied, so the
            // pool must stay alive until every patch below has been
            // tessellated -- it is only torn down once the delete loop is
            // done. None of these patches are ever attach()'d/ref-counted
            // -- exactly like the ordinary CSurface leaf case above, each
            // is delete'd directly once its grid has been extracted.
            CMemPage *subdivMem = NULL;
            CObject *patches = subdivMesh->tessellateToSurfaces(subdivMem);

            CObject *patch = patches;
            while (patch != NULL) {
                CObject *next = patch->sibling;
                CSurface *patchSurf = dynamic_cast<CSurface *>(patch);
                assert(patchSurf != NULL);

                CTesselatedGrid grid = tesselateQuadricAdaptive(patchSurf, tolerance, TRUE);

                csgTriangulateGrid(grid, leaf->attributes, soup);

                delete[] grid.P;
                delete[] grid.dPdu;
                delete[] grid.dPdv;

                delete patch;
                patch = next;
            }

            memoryTini(subdivMem);
        } else {
            error(CODE_BADTOKEN, "Unsupported CSG operand type inside a SolidBegin/SolidEnd boolean block\n");
        }

        consumedLeaves->push(leaf);
    }

    if (csgSignedVolume(soup) < 0.0)
        csgFlipAll(soup);

    return soup;
}

///////////////////////////////////////////////////////////////////////
// Struct				:	CCSGResolved
// Description			:	Result of resolving one CSGTreeNode: either
//							unresolved passthrough geometry (FR-002/FR-017,
//							a chain of CObjects that has not gone through
//							the boolean kernel) or an already boolean-
//							resolved polygon soup. At most one is non-NULL.
struct CCSGResolved {
    CObject *passthrough;
    CArray<CCSGPolygon *> *soup;
};

///////////////////////////////////////////////////////////////////////
// Function				:	csgToSoup
// Description			:	Converts one resolved operand to a polygon
//							soup, tessellating passthrough geometry on
//							demand (already-resolved soup is returned as-is
//							-- no re-tessellation of an already-resolved
//							boundary).
// Return Value			:	The operand's polygon soup
// Comments				:
static CArray<CCSGPolygon *> *csgToSoup(CCSGResolved &r, CArray<CObject *> *consumedLeaves) {
    if (r.soup != NULL)
        return r.soup;

    return csgTessellateOperand(r.passthrough, consumedLeaves);
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgResolveNode
// Description			:	Recursively resolves `node` to either its flat
//							passthrough fragment chain (0 or 1 total
//							operand, FR-016/FR-017) or a boolean-resolved
//							polygon soup (2+ operands, T017-T019 kernel +
//							T022/T023 leaf tessellation). Nested boolean
//							blocks resolve to soup themselves and are folded
//							in directly (no lossy round trip through a
//							rebuilt CPolygonMesh for intermediate results).
// Return Value			:	The resolved operand
// Comments				:	Every leaf CObject consumed into soup is
//							pushed onto `consumedLeaves`, not detached here
//							-- see csgTessellateOperand.
static CCSGResolved csgResolveNode(CSGTreeNode *node, CArray<CObject *> *consumedLeaves) {
    if (node->operation == CSG_PRIMITIVE) {
        CCSGResolved r = { node->leafObjects, NULL };
        return r; // FR-002: raw geometry is already one opaque leaf operand
    }

    CArray<CCSGResolved> operands;

    if (node->leafObjects != NULL) {
        CCSGResolved r = { node->leafObjects, NULL };
        operands.push(r);
    }

    int i;
    for (i = 0; i < node->operands->numItems; i++) {
        CCSGResolved r = csgResolveNode(node->operands->array[i], consumedLeaves);
        if (r.passthrough != NULL || r.soup != NULL)
            operands.push(r); // Empty nested block (FR-016) contributes nothing
    }

    if (operands.numItems == 0) {
        CCSGResolved empty = { NULL, NULL };
        return empty; // FR-016: empty solid block -> no geometry
    }

    if (operands.numItems == 1)
        return operands.array[0]; // FR-017: single operand -> passthrough unchanged

    CArray<CCSGPolygon *> *combined = csgToSoup(operands.array[0], consumedLeaves);
    for (i = 1; i < operands.numItems; i++) {
        CArray<CCSGPolygon *> *next   = csgToSoup(operands.array[i], consumedLeaves);
        CArray<CCSGPolygon *> *folded = csgCombine(node->operation, combined, next);

        csgFreePolygons(combined);
        csgFreePolygons(next);
        combined = folded;
    }

    CCSGResolved result = { NULL, combined };
    return result;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgBuildMeshForAttributeGroup
// Description			:	Converts every polygon in `polygons` tagged
//							with `attr` into one CPolygonMesh (T024/T025):
//							fan-triangulates each polygon and packs
//							unwelded P/N vertex data (falling back to the
//							polygon's own flat planeNormal where no analytic
//							per-vertex normal was carried, e.g. BSP-cut
//							difference faces).
// Return Value			:	A new CPolygonMesh, or NULL if `attr` has no
//							polygons in `polygons`
// Comments				:	Built with an identity CXform: T021/T022's
//							tessellated P is already world-space
//							(CSurface::sample() unconditionally applies
//							xform->from), and csgRebaseFragments already
//							accounts for a fragment declared in a different
//							frame than the enclosing solid's outer xform.
static CObject *csgBuildMeshForAttributeGroup(CArray<CCSGPolygon *> *polygons, CAttributes *attr) {
    int numTris = 0;
    for (int i = 0; i < polygons->numItems; i++) {
        CCSGPolygon *poly = polygons->array[i];
        if (poly->attributes == attr)
            numTris += poly->vertices.numItems - 2;
    }

    if (numTris <= 0)
        return NULL;

    int numVerts  = numTris * 3;
    int dataSize  = numVerts * 3 * 2; // P then N, 3 floats/vertex each

    float *data0    = new float[dataSize];
    int *nholes     = new int[numTris];
    int *nvertices  = new int[numTris];
    int *vertices   = new int[numVerts];

    int tri = 0;
    for (int i = 0; i < polygons->numItems; i++) {
        CCSGPolygon *poly = polygons->array[i];
        if (poly->attributes != attr)
            continue;

        int n = poly->vertices.numItems;
        for (int k = 1; k + 1 < n; k++) {
            const CCSGVertex *corners[3] = { &poly->vertices.array[0], &poly->vertices.array[k], &poly->vertices.array[k + 1] };

            for (int c = 0; c < 3; c++) {
                int vidx    = tri * 3 + c;
                float *pDst = data0 + vidx * 3;
                float *nDst = data0 + numVerts * 3 + vidx * 3;

                pDst[0] = corners[c]->p[0];
                pDst[1] = corners[c]->p[1];
                pDst[2] = corners[c]->p[2];

                if (corners[c]->hasNormal) {
                    nDst[0] = corners[c]->n[0];
                    nDst[1] = corners[c]->n[1];
                    nDst[2] = corners[c]->n[2];
                } else {
                    nDst[0] = poly->planeNormal[0];
                    nDst[1] = poly->planeNormal[1];
                    nDst[2] = poly->planeNormal[2];
                }

                vertices[vidx] = vidx;
            }

            nholes[tri]    = 1;
            nvertices[tri] = 3;
            tri++;
        }
    }

    CVariable *varP = CRenderer::retrieveVariable("P");
    CVariable *varN = CRenderer::retrieveVariable("N");

    CPlParameter *plParams = new CPlParameter[2];
    plParams[0].variable  = varP;
    plParams[0].numItems  = numVerts;
    plParams[0].index     = 0;
    plParams[0].container = CONTAINER_VERTEX;
    plParams[1].variable  = varN;
    plParams[1].numItems  = numVerts;
    plParams[1].index     = numVerts * 3;
    plParams[1].container = CONTAINER_VERTEX;

    CPl *pl = new CPl(dataSize, 2, plParams, data0);

    // attr is shared with whatever non-CSG objects originally referenced
    // this attribute set (CAttributes instances are shared/refcounted
    // across primitives with identical attributes) -- clone before tagging
    // it as a Boundary Fragment so the flag doesn't leak onto unrelated
    // geometry (FR-020).
    CAttributes *fragAttr = new CAttributes(attr);
    fragAttr->flags |= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;

    CXform *identity = new CXform();
    CObject *result  = new CPolygonMesh(fragAttr, identity, pl, numTris, nholes, nvertices, vertices);

    delete[] nholes;
    delete[] nvertices;
    delete[] vertices;

    return result;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgPolygonsToFragments
// Description			:	Converts an already boolean-resolved polygon
//							soup into a Boundary Fragment chain (T024): one
//							CPolygonMesh per distinct originating
//							CAttributes*, chained via CObject::sibling.
// Return Value			:	The fragment chain, or NULL if `polygons` is
//							empty
// Comments				:
static CObject *csgPolygonsToFragments(CArray<CCSGPolygon *> *polygons) {
    CArray<CAttributes *> distinctAttrs;

    int i;
    for (i = 0; i < polygons->numItems; i++) {
        CAttributes *attr = polygons->array[i]->attributes;
        int seen          = FALSE;

        int j;
        for (j = 0; j < distinctAttrs.numItems; j++) {
            if (distinctAttrs.array[j] == attr) {
                seen = TRUE;
                break;
            }
        }

        if (!seen)
            distinctAttrs.push(attr);
    }

    CObject *chain = NULL;
    for (i = 0; i < distinctAttrs.numItems; i++) {
        CObject *mesh = csgBuildMeshForAttributeGroup(polygons, distinctAttrs.array[i]);
        if (mesh == NULL)
            continue;

        mesh->sibling = chain;
        chain         = mesh;
    }

    return chain;
}

///////////////////////////////////////////////////////////////////////
// Function				:	csgRebaseFragments
// Description			:	Each captured leaf's `xform` is absolute (the
//							full local-to-world transform active when it
//							was declared). CSolidObject::intersect()/dice()
//							unconditionally re-composes every fragment's
//							xform with the container's own `outerXform` on
//							first use (processDelayedSolid), so a fragment
//							declared under a transform different from the
//							one snapshotted at SolidBegin time must first
//							be rebased into that outer frame here, or the
//							outer transform is applied twice.
// Return Value			:
// Comments				:	outerXform is shared (also held as the future
//							CSolidObject's own xform), so it is copied
//							before being inverted rather than inverted in
//							place.
static void csgRebaseFragments(CObject *fragments, CXform *outerXform) {
    if (fragments == NULL)
        return;

    CXform *outerInverse = new CXform(outerXform);
    outerInverse->invert();

    CObject *cObject;
    for (cObject = fragments; cObject != NULL; cObject = cObject->sibling) {
        CXform *rebased = new CXform(outerInverse);
        rebased->concat(cObject->xform);

        rebased->attach();
        cObject->xform->detach();
        cObject->xform = rebased;
    }

    delete outerInverse;
}

///////////////////////////////////////////////////////////////////////
// Function				:	resolveCSGTree
// Description			:	See csgTree.h
// Return Value			:
// Comments				:
void resolveCSGTree(CRendererContext *context, CSGTreeNode *node) {
    assert(node != NULL);
    assert(node->outerXform != NULL);

    CArray<CObject *> consumedLeaves;
    CCSGResolved resolved = csgResolveNode(node, &consumedLeaves);

    CObject *fragments;
    if (resolved.soup != NULL) {
        fragments = csgPolygonsToFragments(resolved.soup);
        csgFreePolygons(resolved.soup);
    } else {
        fragments = resolved.passthrough;
    }

    if (fragments != NULL) {
        csgRebaseFragments(fragments, node->outerXform);
        context->addObject(new CSolidObject(context->getAttributes(FALSE), node->outerXform, fragments));
    }

    int i;
    for (i = 0; i < consumedLeaves.numItems; i++)
        consumedLeaves.array[i]->detach();

    node->outerXform->detach();
    delete node;
}
