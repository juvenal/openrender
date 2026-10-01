/**
 * Project: openRender
 *
 * File: surfaceCrossingDispatchStub.cpp
 *
 * Description:
 *   ribVector's counterpart to geometry/surfaceCrossingDispatch.cpp. CSG
 *   is structurally unsupported on the preview path, so the free-form
 *   crossing core is never reached there (spec 021).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "surfaceCrossingDispatch.h"

// CSG is structurally unsupported on the preview path (CRibGeometryContext
// has no RiSolidBegin/RiSolidEnd override), so no free-form surface is ever
// classified there.
int countFreeFormCrossings(CShadingContext *, const CSurface *, const float *, const float *) {
    return 0;
}
