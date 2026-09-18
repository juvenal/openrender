/**
 * Project: openRender
 *
 * File: csgTesselationDispatch.cpp
 *
 * Description:
 *   Real (rendering-time) bodies of tesselatePatchMeshAdaptive()/
 *   tesselateNURBSPatchMeshAdaptive(), the two patches.cpp free functions
 *   that call surface.cpp's tesselateSurfaceGrid()/tesselateQuadricAdaptive()
 *   -- found via an `nm -u` sweep over ribVector_obj's built object files.
 *   Both exist purely to CSG-tessellate a patch mesh's sub-patches at
 *   RiSolidEnd time; their only caller repo-wide is csgTree.cpp's
 *   csgToSoup(), confirmed by grepping every call site directly. Since
 *   CRibGeometryContext has no RiSolidBegin/RiSolidEnd override at all (RIB
 *   `Solid`/CSG is structurally unsupported on the preview path -- see
 *   src/ri/CMakeLists.txt's csgTree.cpp/solidObject.cpp exclusion comment),
 *   csgTree.cpp is already excluded from ribVector, which makes these two
 *   functions dead code there too -- but they still live in patches.cpp
 *   (needed for its many other, genuinely-required CPatchMesh/
 *   CNURBSPatchMesh methods), so they still get compiled and still need
 *   tesselateSurfaceGrid()/tesselateQuadricAdaptive() resolvable.
 *
 *   A consumer that must not link libshader_shading (orender-wire's
 *   ribVector, see the domain-split plan) needs alternate, non-shading
 *   bodies for both -- see vector/csgTesselationDispatchStub.cpp, whose
 *   stubs are loud (error() + assert(FALSE)) rather than silent no-ops,
 *   matching every other provably-unreachable method in this split.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <math.h>
#include <vector>

#include "common/polynomial.h"
#include "error.h"
#include "memory.h"
#include "patchUtils.h"
#include "patches.h"

///////////////////////////////////////////////////////////////////////
// Macro				:	gatherData
// Description			:	Get the data to create the primitive
// Return Value			:	-
// Comments				:	Copied verbatim from patches.cpp -- both
//							functions below use it exactly as
//							CPatchMesh::create()/CNURBSPatchMesh::create()
//							do, against the same local vertexSize/vertices/
//							uvertices/vvertices/uvaryings/vvaryings/
//							parameterList variables each function declares.
#define gatherData(__context, __u, __v, __nu, __nv, __uv, __vv, __un, __vertex, __parameters)                  \
    {                                                                                                          \
        int __i, __j;                                                                                          \
        float *__dest;                                                                                         \
                                                                                                               \
        assert(vertexSize > 0);                                                                                \
                                                                                                               \
        if (__vertex == NULL)                                                                                  \
            __vertex = (float *)ralloc(vertexSize * (__nu) * (__nv) * sizeof(float), __context->threadMemory); \
                                                                                                               \
        __dest = __vertex;                                                                                     \
                                                                                                               \
        for (__j = 0; __j < (__nv); __j++) {                                                                   \
            const int __vVertex = ((__v) + __j) % vvertices;                                                   \
            for (__i = 0; __i < (__nu); __i++) {                                                               \
                const int __uVertex = ((__u) + __i) % uvertices;                                               \
                const int __index = __vVertex * uvertices + __uVertex;                                         \
                const float *__src = vertices + __index * vertexSize;                                          \
                int __k;                                                                                       \
                                                                                                               \
                for (__k = vertexSize; __k > 0; __k--)                                                         \
                    *__dest++ = *__src++;                                                                      \
            }                                                                                                  \
        }                                                                                                      \
                                                                                                               \
        __parameters = parameterList->uniform(__un, NULL);                                                     \
                                                                                                               \
        const int __v0 = (__vv) * uvaryings + (__uv);                                                          \
        const int __v1 = (__vv) * uvaryings + (((__uv) + 1) % uvaryings);                                      \
        const int __v2 = (((__vv) + 1) % vvaryings) * uvaryings + (__uv);                                      \
        const int __v3 = (((__vv) + 1) % vvaryings) * uvaryings + (((__uv) + 1) % uvaryings);                  \
        __parameters = parameterList->varying(__v0, __v1, __v2, __v3, __parameters);                           \
    }

///////////////////////////////////////////////////////////////////////
// Function				:	tesselatePatchMeshAdaptive
// Description			:	See patches.h. Mirrors CPatchMesh::create()'s
//							own sub-patch decomposition span loop, but with
//							two differences: (1) the transient per-sub-patch
//							scratch buffer comes from a standalone CMemPage
//							owned by this call (create()'s context->
//							threadMemory is a memory pool, not a real
//							CShadingContext dependency -- there is none at
//							RiSolidEnd time), and (2) each sub-patch is
//							tessellated immediately via
//							tesselateQuadricAdaptive (CBilinearPatch and
//							CBicubicPatch are CSurface subclasses, so T021's
//							driver applies to them unchanged) instead of
//							being linked into a lazy children list.
// Comments				:	A local struct standing in for CShadingContext
//							satisfies gatherData's __context->threadMemory
//							access without pulling in any real shading
//							state.
CTesselatedPatchMeshOperand tesselatePatchMeshAdaptive(CPatchMesh *mesh, float tolerance, int computeDerivatives) {
    struct CMemPageContext {
            CMemPage *threadMemory;
    };

    assert(mesh->pl != NULL);

    int i, j, k;
    int uvaryings, vvaryings;
    int uvertices, vvertices;
    CPl *parameterList;
    CVertexData *vertexData;
    float *vertices;
    int vertexSize;

    int upatches, vpatches;
    CSurface **subPatches;
    CTesselatedGrid *grids;
    int maxDiv = 0;

    CMemPage *localMemory = NULL;
    memoryInit(localMemory);

    memBegin(localMemory);

    CMemPageContext memCtx;
    memCtx.threadMemory = localMemory;
    CMemPageContext *memCtxPtr = &memCtx;

    uvertices = mesh->uVertices;
    vvertices = mesh->vVertices;

    vertices = NULL;
    mesh->pl->transform(mesh->xform);
    mesh->pl->collect(vertexSize, vertices, CONTAINER_VERTEX, localMemory);
    parameterList = mesh->pl;
    vertexData = mesh->pl->vertexData();
    vertexData->attach();

    if (mesh->degree == 1) {
        float uMult;
        float vMult;
        float *vertex = NULL;
        CParameter *parameters;

        if (mesh->uWrap)
            upatches = uvertices;
        else
            upatches = uvertices - 1;

        if (mesh->vWrap)
            vpatches = vvertices;
        else
            vpatches = vvertices - 1;

        uMult = 1 / (float)upatches;
        vMult = 1 / (float)vpatches;

        uvaryings = uvertices;
        vvaryings = vvertices;

        subPatches = new CSurface *[upatches * vpatches];
        grids      = new CTesselatedGrid[upatches * vpatches];

        for (k = 0, i = 0; i < vpatches; i++) {
            for (j = 0; j < upatches; j++, k++) {
                float uOrg = j * uMult;
                float vOrg = i * vMult;

                gatherData(memCtxPtr, j, i, 2, 2, j, i, k, vertex, parameters);

                CBilinearPatch *sub = new CBilinearPatch(mesh->attributes, mesh->xform, vertexData, parameters, uOrg, vOrg, uMult, vMult, vertex);
                sub->attach();
                subPatches[k] = sub;

                grids[k] = tesselateQuadricAdaptive(sub, tolerance, computeDerivatives);
                if (grids[k].div > maxDiv) maxDiv = grids[k].div;
            }
        }
    } else {
        float uMult;
        float vMult;
        float *vertex = NULL;
        CParameter *parameters;
        const int us = mesh->attributes->uStep;
        const int vs = mesh->attributes->vStep;

        assert(mesh->degree == 3);

        if (mesh->uWrap)
            upatches = (uvertices) / us;
        else
            upatches = ((uvertices - 4) / us) + 1;

        if (mesh->vWrap)
            vpatches = (vvertices) / vs;
        else
            vpatches = ((vvertices - 4) / vs) + 1;

        uMult = 1 / (float)upatches;
        vMult = 1 / (float)vpatches;

        uvaryings = (upatches + 1 - mesh->uWrap);
        vvaryings = (vpatches + 1 - mesh->vWrap);

        subPatches = new CSurface *[upatches * vpatches];
        grids      = new CTesselatedGrid[upatches * vpatches];

        for (k = 0, i = 0; i < vpatches; i++) {
            for (j = 0; j < upatches; j++, k++) {
                float uOrg = j * uMult;
                float vOrg = i * vMult;

                gatherData(memCtxPtr, j * us, i * vs, 4, 4, j, i, k, vertex, parameters);

                CBicubicPatch *sub = new CBicubicPatch(mesh->attributes, mesh->xform, vertexData, parameters, uOrg, vOrg, uMult, vMult, vertex);
                sub->attach();
                subPatches[k] = sub;

                grids[k] = tesselateQuadricAdaptive(sub, tolerance, computeDerivatives);
                if (grids[k].div > maxDiv) maxDiv = grids[k].div;
            }
        }
    }

    vertexData->detach();

    memEnd(localMemory);
    memoryTini(localMemory);

    // Weld seams: re-sample any sub-patch whose own adaptive resolution
    // landed below the mesh-wide max so every sub-patch shares one
    // resolution -- otherwise adjacent sub-patches diced at different
    // resolutions leave T-junction cracks along their shared edge.
    const int total = upatches * vpatches;
    for (k = 0; k < total; k++) {
        if (grids[k].div != maxDiv) {
            delete[] grids[k].P;
            delete[] grids[k].dPdu;
            delete[] grids[k].dPdv;
            grids[k] = tesselateSurfaceGrid(subPatches[k], maxDiv, computeDerivatives);
        }
        subPatches[k]->detach();
    }
    delete[] subPatches;

    // We're done with the parameter list
    delete mesh->pl;
    mesh->pl = NULL;

    CTesselatedPatchMeshOperand result;
    result.div      = maxDiv;
    result.uPatches = upatches;
    result.vPatches = vpatches;
    result.grids    = grids;
    return result;
}

///////////////////////////////////////////////////////////////////////
// Function				:	tesselateNURBSPatchMeshAdaptive
// Description			:	See patches.h. Mirrors CNURBSPatchMesh::create()'s
//							own sub-patch decomposition span loop (including
//							its degenerate-knot-span skip and trimTest
//							pass-through), but with the same two differences
//							tesselatePatchMeshAdaptive applies for CPatchMesh:
//							(1) the transient per-sub-patch scratch buffer
//							comes from a standalone CMemPage owned by this
//							call instead of a real CShadingContext (there is
//							none at RiSolidEnd time), and (2) each sub-patch
//							is tessellated immediately via
//							tesselateQuadricAdaptive (CNURBSPatch is a
//							CSurface subclass, so T021's driver applies to it
//							unchanged) instead of being linked into a lazy
//							children list.
CTesselatedNURBSPatchMeshOperand tesselateNURBSPatchMeshAdaptive(CNURBSPatchMesh *mesh, float tolerance, int computeDerivatives) {
    struct CMemPageContext {
            CMemPage *threadMemory;
    };

    assert(mesh->pl != NULL);

    const int uPatches = mesh->uVertices - mesh->uOrder + 1;
    const int vPatches = mesh->vVertices - mesh->vOrder + 1;
    int i, j, k;
    float *vertex = NULL;
    CParameter *parameters;
    CPl *parameterList;
    CVertexData *vertexData;
    float *vertices;
    int vertexSize;

    const int uvertices = mesh->uVertices;
    const int vvertices = mesh->vVertices;
    const int uvaryings  = mesh->uVertices - mesh->uOrder + 2;
    const int vvaryings  = mesh->vVertices - mesh->vOrder + 2;

    CSurface **subPatches = new CSurface *[uPatches * vPatches];
    CTesselatedGrid *grids = new CTesselatedGrid[uPatches * vPatches];
    int maxDiv = 0;
    int count  = 0;

    CMemPage *localMemory = NULL;
    memoryInit(localMemory);

    memBegin(localMemory);

    CMemPageContext memCtx;
    memCtx.threadMemory = localMemory;
    CMemPageContext *memCtxPtr = &memCtx;

    // Transform the core into the camera coordinate system
    vertices = NULL;
    mesh->pl->transform(mesh->xform);
    mesh->pl->collect(vertexSize, vertices, CONTAINER_VERTEX, localMemory);
    parameterList = mesh->pl;
    vertexData = mesh->pl->vertexData();
    vertexData->attach();

    for (k = 0, j = 0; j < vPatches; j++) {
        for (i = 0; i < uPatches; i++, k++) {
            float umin = mesh->uKnots[i + mesh->uOrder - 1];
            float umax = mesh->uKnots[i + mesh->uOrder];
            float vmin = mesh->vKnots[j + mesh->vOrder - 1];
            float vmax = mesh->vKnots[j + mesh->vOrder];
            float uint = umax - umin;
            float vint = vmax - vmin;

            if ((uint == 0) || (vint == 0)) {
                // The patch does not have a valid parametric space, so just skip it
            } else {
                gatherData(memCtxPtr, i, j, mesh->uOrder, mesh->vOrder, i, j, k, vertex, parameters);

                CNURBSPatch *sub = new CNURBSPatch(mesh->attributes, mesh->xform, vertexData, parameters, mesh->uOrder, mesh->vOrder, mesh->uKnots + i, mesh->vKnots + j, vertex, mesh->trimTest);
                sub->attach();
                subPatches[count] = sub;

                grids[count] = tesselateQuadricAdaptive(sub, tolerance, computeDerivatives);
                if (grids[count].div > maxDiv) maxDiv = grids[count].div;

                count++;
            }
        }
    }

    vertexData->detach();

    memEnd(localMemory);
    memoryTini(localMemory);

    // Weld seams: re-sample any sub-patch whose own adaptive resolution
    // landed below the mesh-wide max so every sub-patch shares one
    // resolution -- otherwise adjacent sub-patches diced at different
    // resolutions leave T-junction cracks along their shared edge.
    for (k = 0; k < count; k++) {
        if (grids[k].div != maxDiv) {
            delete[] grids[k].P;
            delete[] grids[k].dPdu;
            delete[] grids[k].dPdv;
            grids[k] = tesselateSurfaceGrid(subPatches[k], maxDiv, computeDerivatives);
        }
        subPatches[k]->detach();
    }
    delete[] subPatches;

    // We're done with the parameter list
    delete mesh->pl;
    mesh->pl = NULL;

    CTesselatedNURBSPatchMeshOperand result;
    result.count = count;
    result.div   = maxDiv;
    result.grids = grids;
    return result;
}
