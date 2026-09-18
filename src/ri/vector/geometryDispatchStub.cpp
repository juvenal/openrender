/**
 * Project: openRender
 *
 * File: geometryDispatchStub.cpp
 *
 * Description:
 *   ribVector's counterpart to src/ri/render/geometryDispatch.cpp. Provides
 *   the SAME seven CObject/CSurface/CCurve/CPoints methods with alternate
 *   bodies that never touch CShadingContext/CReyes, so a consumer that links
 *   this file instead of geometryDispatch.cpp never needs libshader_shading
 *   or the Reyes hider resolvable at all.
 *
 *   This is safe because none of orender-wire's actual code path ever
 *   reaches these methods: CPreviewContext::addObject() extracts geometry
 *   via each class's own wireData() accessor and this file's own dedicated
 *   tessellators (src/preview/libribpreview/tessellators/), never by calling
 *   intersect()/dice()/shade() on the constructed object. These bodies exist
 *   purely to give every geometry class a complete, linkable vtable; they
 *   are deliberately loud (error() + assert(FALSE)) rather than silent
 *   no-ops, so that if this analysis is ever wrong and one of them *is*
 *   reached, it fails immediately and diagnosably instead of producing a
 *   quietly-wrong wireframe.
 *
 *   checkRayGuard() is the one exception: returning TRUE ("caller should
 *   return immediately") is a legitimate, correct answer for a build that
 *   never raytraces at all, not an error path -- there is nothing to log.
 *
 *   Not yet wired into any CMake target (see the vector-domain step of the
 *   codebase-refactor-fixture plan for when a ribVector target is added).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "curves.h"
#include "error.h"
#include "object.h"
#include "points.h"
#include "surface.h"

void CObject::dice(CReyes *) {
    error(CODE_BUG, "CObject::dice() reached in a build with no rendering pipeline\n");
    assert(FALSE);
}

void CObject::cluster(CShadingContext *) {
    error(CODE_BUG, "CObject::cluster() reached in a build with no rendering pipeline\n");
    assert(FALSE);
}

bool CSurface::checkRayGuard(CRay *, CShadingContext *) {
    // Never raytraced in this build -- there is nothing for the caller to do.
    return TRUE;
}

void CSurface::intersect(CShadingContext *, CRay *) {
    error(CODE_BUG, "CSurface::intersect() reached in a build with no rendering pipeline\n");
    assert(FALSE);
}

void CSurface::dice(CReyes *) {
    error(CODE_BUG, "CSurface::dice() reached in a build with no rendering pipeline\n");
    assert(FALSE);
}

void CSurface::shade(CShadingContext *, int, CRay **) {
    error(CODE_BUG, "CSurface::shade() reached in a build with no rendering pipeline\n");
    assert(FALSE);
}

void CCurve::dice(CReyes *) {
    error(CODE_BUG, "CCurve::dice() reached in a build with no rendering pipeline\n");
    assert(FALSE);
}

void CPoints::dice(CReyes *) {
    error(CODE_BUG, "CPoints::dice() reached in a build with no rendering pipeline\n");
    assert(FALSE);
}
