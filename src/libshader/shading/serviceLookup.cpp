/**
 * Project: openRender
 *
 * File: serviceLookup.cpp
 *
 * Description:
 *   See serviceLookup.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "serviceLookup.h"

#include "rendererServices.h"
#include "shading.h" // CShadingContext::defaultServices()

namespace libshader {

    CDSO *lookupDSO(const char *name, const char *prototype) {
        CRendererServices *svc = CShadingContext::defaultServices();
        return svc ? svc->getDSO(name, prototype) : nullptr;
    }

    RtFilterFunc lookupFilter(const char *name) {
        CRendererServices *svc = CShadingContext::defaultServices();
        return svc ? svc->getFilter(name) : RiGaussianFilter;
    }

    RtStepFilterFunc lookupStepFilter(const char *name) {
        CRendererServices *svc = CShadingContext::defaultServices();
        return svc ? svc->getStepFilter(name) : RiGaussianStepFilter;
    }

    int lookupGlobalID(const char *name) {
        CRendererServices *svc = CShadingContext::defaultServices();
        return svc ? svc->getGlobalID(name) : 0;
    }

    CVariable *lookupVariable(const char *name) {
        CRendererServices *svc = CShadingContext::defaultServices();
        return svc ? svc->retrieveVariable(name) : nullptr;
    }

} // namespace libshader
