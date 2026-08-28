/**
 * Project: openRender
 *
 * File: patches.h
 *
 * Description:
 *   This file defines the interface for patches.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	patches.h
//  Classes				:	CPointDB
//							CBilinearPatchMesh
//							CBicubicPatchMesh
//  Description			:	Some geometry classes
//
////////////////////////////////////////////////////////////////////////
#ifndef PATCHES_H
#define PATCHES_H

#include "attributes.h" // ETrimSense
#include "common/global.h"
#include "memory.h"
#include "object.h"
#include "pl.h"
#include "surface.h" // CTesselatedGrid, used by tesselatePatchMeshAdaptive (T022)

class CTrimTest; // Forward declaration; full definition below (after CPatchMesh)

///////////////////////////////////////////////////////////////////////
// Struct				:	CTesselatedPatchMeshOperand
// Description			:	The result of tesselatePatchMeshAdaptive(): every
//							bilinear/bicubic sub-patch of a CPatchMesh,
//							tessellated to a single shared resolution (T022's
//							seam-welding requirement -- adjacent sub-patches
//							at different resolutions would leave T-junction
//							cracks along their shared edge, the same
//							watertightness failure a trimmed NURBS patch has).
// Comments				:	grids is row-major over the mesh's own
//							(vPatches x uPatches) sub-patch grid: sub-patch
//							(i,j) is grids[i * uPatches + j], matching
//							CPatchMesh::create()'s own k = i*upatches+j
//							sub-patch numbering. Caller owns grids and every
//							grid's P/dPdu/dPdv (delete[] each, then
//							delete[] grids).
struct CTesselatedPatchMeshOperand {
        int div;               // Shared grid resolution: every grid is (div+1) x (div+1)
        int uPatches, vPatches; // Sub-patch counts along u and v
        CTesselatedGrid *grids; // Row-major uPatches*vPatches grids (caller owns)
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBilinearPatch
// Description			:	Encapsulates a bilinear patch
// Comments				:
class CBilinearPatch : public CSurface {
    public:
        CBilinearPatch(CAttributes *, CXform *, CVertexData *, CParameter *, float, float, float, float, float *);
        ~CBilinearPatch();

        // CObject interface
        void intersect(CShadingContext *, CRay *);
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(FALSE); }

        // CSurface interface
        int moving() const { return variables->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

    private:
        CVertexData *variables;         // The variables for the patch
        CParameter *parameters;         // The parameters for the patch
        float *vertex;                  // The vertex data
        float uMult, vMult, uOrg, vOrg; // The parametric range of the patch

        friend class CPreviewContext;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBicubicPatch
// Description			:	Encapsulates a bicubic patch
// Comments				:
class CBicubicPatch : public CSurface {
    public:
        CBicubicPatch(CAttributes *, CXform *, CVertexData *, CParameter *, float, float, float, float, float *, const float *uBasis = NULL, const float *vBasis = NULL);
        ~CBicubicPatch();

        // CObject interface
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(FALSE); }

        // CSurface interface
        int moving() const { return variables->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

    private:
        void computeVertexData(float *, const float *, int, const float *, const float *);

        CVertexData *variables;         // Variables for the patch
        CParameter *parameters;         // Parameters for the patch
        float *vertex;                  // The vertex data
        float uOrg, vOrg, uMult, vMult; // The parametric range of the patch

        friend class CPreviewContext;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CNURBSPatch
// Description			:	Encapsulates a NURBS patch
// Comments				:
class CNURBSPatch : public CSurface {
    public:
        CNURBSPatch(CAttributes *, CXform *, CVertexData *, CParameter *, int, int, float *, float *, float *, CTrimTest * = NULL);
        ~CNURBSPatch();

        // CObject interface
        void instantiate(CAttributes *, CXform *, CRiInterface *) const { assert(FALSE); }

        // CSurface interface
        int moving() const { return variables->moving; }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;

        int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            int uOrderValue = uOrder - 1 - depth;
            if (1 > uOrderValue) {
                minDivU = 1;
            } else {
                minDivU = uOrderValue;
            }
            int vOrderValue = vOrder - 1 - depth;
            if (1 > vOrderValue) {
                minDivV = 1;
            } else {
                minDivV = vOrderValue;
            }
            return 0;
        }

        bool trimAccepts(float u, float v) const;
        bool hasTrim() const { return trimTest != NULL; }

    private:
        void precompBasisCoefficients(double *, unsigned int, unsigned int, unsigned int, const float *);
        void precomputeVertexData(double *, const double *, const double *, float *, int);

        CVertexData *variables;         // The variable data
        CParameter *parameters;         // The parameters for this patch
        double *vertex;                 // The vertex data
        int uOrder, vOrder;             // The order of the patch
        float uOrg, vOrg, uMult, vMult; // The parametric range of the patch
        CTrimTest *trimTest;            // Shared trim classification (refcounted, see CTrimTest); NULL if untrimmed

        friend class CPreviewContext;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CPatchMesh
// Description			:	Encapsulates a patch mesh
// Comments				:
class CPatchMesh : public CObject {
    public:
        CPatchMesh(CAttributes *, CXform *, CPl *, int, int, int, int, int);
        ~CPatchMesh();

        // CObject interface
        void intersect(CShadingContext *, CRay *);
        void dice(CReyes *rasterizer);
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

    private:
        void create(CShadingContext *context);

        CPl *pl;
        int degree;
        int uVertices, vVertices, uWrap, vWrap;
        TMutex mutex;

        friend class CPreviewContext;
        friend CTesselatedPatchMeshOperand tesselatePatchMeshAdaptive(CPatchMesh *mesh, float tolerance, int computeDerivatives);
};

///////////////////////////////////////////////////////////////////////
// Function				:	tesselatePatchMeshAdaptive
// Description			:	Tessellates every bilinear/bicubic sub-patch of a
//							CPatchMesh into a single seam-welded grid mosaic,
//							for use as a CSG leaf operand at RiSolidEnd time
//							(no CShadingContext exists there). Reuses
//							create()'s own sub-patch decomposition span loop
//							(gatherData + CBilinearPatch/CBicubicPatch
//							construction), but replaces create()'s
//							context->threadMemory scratch arena with a
//							standalone CMemPage owned entirely by this call --
//							create()'s use of CShadingContext is incidental
//							(a memory pool), not a real dependency, so no
//							CShadingContext is constructed here.
// Return Value			:	Every sub-patch's grid, uniformly re-tessellated
//							at the coarsest resolution that still satisfies
//							every sub-patch's own tolerance test (the max
//							div across sub-patches) so adjacent sub-patches
//							share identical edge sample counts and welding
//							cracks do not appear.
// Comments				:	Trimmed NURBS operands (CNURBSPatchMesh) are not
//							supported by this function -- see T022's "Known
//							follow-up" note in tasks.md. mesh's pl is
//							consumed (deleted and NULLed) exactly as
//							create() does; mesh must not be tessellated (via
//							this function or create()) more than once.
CTesselatedPatchMeshOperand tesselatePatchMeshAdaptive(CPatchMesh *mesh, float tolerance, int computeDerivatives);

///////////////////////////////////////////////////////////////////////
// Class				:	CTrimPolyline
// Description			:	A single trim loop, flattened once to a closed
//							polyline in (u,v) parameter space.
// Comments				:
class CTrimPolyline {
    public:
        CTrimPolyline() : numVertices(0), uv(NULL) {}
        ~CTrimPolyline() { delete[] uv; }

        int numVertices;
        float *uv; // Flattened (u,v) pairs, uv[2 * numVertices]
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTrimTest
// Description			:	Hider-agnostic (u,v) inside/outside classification
//							built once per CNURBSPatchMesh from its pending
//							RiTrimCurve state, and consulted identically by
//							every tessellation/sampling code path.
// Comments				:	CRefCounter-based: the owning mesh and every
//							per-span CNURBSPatch that references it each hold
//							an attach(), since diced spans can outlive the
//							mesh once dispatched to worker threads.
class CTrimTest : public CRefCounter {
    public:
        CTrimTest() : numLoops(0), loopPolylines(NULL), sense(TS_INSIDE) {}
        ~CTrimTest() { delete[] loopPolylines; }

        int numLoops;
        CTrimPolyline *loopPolylines; // One flattened polyline per surviving (non-rejected) trim loop
        ETrimSense sense;             // Snapshot of CAttributes::trimSense at mesh-construction time
};

// Flattens a single Trim Loop's connected curves into one closed (u,v)
// polyline, amortized once per loop (FR-018). Assumes the loop has already
// passed weight/closure validation (see CNURBSPatchMesh::create()).
void trimFlattenLoop(const CTrimLoop &loop, CTrimPolyline &polyline);

// Classifies a sampled (u,v) point against a built Shared Trim Test using an
// O(polyline edges) odd-crossing-count test per loop, composed across every
// loop and combined with the test's trim sense (FR-005, FR-006). Returns
// true if the point should be retained (kept) in the diced/tessellated
// surface; false if it should be excluded.
bool trimClassifyPoint(const CTrimTest &test, float u, float v);

///////////////////////////////////////////////////////////////////////
// Class				:	CNURBSPatchMesh
// Description			:	Encapsulates a NURBS patch mesh
// Comments				:
class CNURBSPatchMesh : public CObject {
    public:
        // The trailing CTrimTest*/eagerBuildTrim pair exists solely for
        // instantiate()'s use: it passes the template mesh's own already-
        // decided trimTest (attach()'d verbatim, even if NULL) with
        // eagerBuildTrim=FALSE, so a clone never re-derives trim state from
        // the ObjectInstance call-site's ambient CAttributes. The normal
        // RiNuPatchV definition path uses the defaults, eagerly consuming
        // whatever RiTrimCurve loops are pending on the given CAttributes.
        CNURBSPatchMesh(CAttributes *, CXform *, CPl *, int, int, int, int, float *, float *, CTrimTest * = NULL, int = TRUE);
        ~CNURBSPatchMesh();

        // CObject interface
        void intersect(CShadingContext *, CRay *);
        void dice(CReyes *rasterizer);
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

    private:
        void create(CShadingContext *context);

        CPl *pl;
        int uVertices, vVertices, uOrder, vOrder;
        float *uKnots, *vKnots;
        TMutex mutex;

        CTrimTest *trimTest; // Shared trim classification for this mesh (nullptr if no TrimCurve was set)

        friend class CPreviewContext;
};

#endif
