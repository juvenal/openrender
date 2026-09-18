/**
 * Project: openRender
 *
 * File: csgTesselationDispatchStub.cpp
 *
 * Description:
 *   ribVector's counterpart to src/ri/csgTesselationDispatch.cpp. Provides
 *   the same tesselatePatchMeshAdaptive()/tesselateNURBSPatchMeshAdaptive()
 *   signatures with bodies that never touch surface.cpp's
 *   tesselateSurfaceGrid()/tesselateQuadricAdaptive(), so a consumer that
 *   links this file instead never needs libshader_shading resolvable for
 *   either.
 *
 *   This is safe because both functions' only caller repo-wide is
 *   csgTree.cpp's csgToSoup(), which is itself excluded from ribVector:
 *   CRibGeometryContext has no RiSolidBegin/RiSolidEnd override at all, so
 *   RIB `Solid`/CSG is structurally unsupported on the preview path.
 *   Deliberately loud (error() + assert(FALSE)) rather than a silent
 *   no-op, matching every other provably-unreachable method in this split.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cassert>

#include "error.h"
#include "patches.h"

CTesselatedPatchMeshOperand tesselatePatchMeshAdaptive(CPatchMesh *, float, int) {
    error(CODE_BUG, "tesselatePatchMeshAdaptive() reached in a build with no rendering pipeline\n");
    assert(FALSE);
    return CTesselatedPatchMeshOperand();
}

CTesselatedNURBSPatchMeshOperand tesselateNURBSPatchMeshAdaptive(CNURBSPatchMesh *, float, int) {
    error(CODE_BUG, "tesselateNURBSPatchMeshAdaptive() reached in a build with no rendering pipeline\n");
    assert(FALSE);
    return CTesselatedNURBSPatchMeshOperand();
}
