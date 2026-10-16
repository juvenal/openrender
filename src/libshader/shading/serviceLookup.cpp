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

#include <atomic>
#include <string>

#include "common/diagnostics.h"
#include "rendererServices.h"
#include "shading.h" // CShadingContext::defaultServices()

namespace libshader {

    // One warning per kind of lookup per process: the first lookup of a kind made with
    // no services reports it, later ones are silent
    static std::atomic<bool> s_warnedDSO{false};
    static std::atomic<bool> s_warnedFilter{false};
    static std::atomic<bool> s_warnedStepFilter{false};
    static std::atomic<bool> s_warnedGlobalID{false};
    static std::atomic<bool> s_warnedVariable{false};

    static void warnNoServices(std::atomic<bool> &warned, const char *kind, const char *name) {
        if (!warned.exchange(true))
            warning(CODE_NOTSTARTED, "renderer services are not installed: %s lookup of \"%s\" returns not found\n", kind, name ? name : "");
    }

    CDSO *lookupDSO(const char *name, const char *prototype) {
        CRendererServices *svc = CShadingContext::defaultServices();
        if (svc)
            return svc->getDSO(name, prototype);
        warnNoServices(s_warnedDSO, "DSO shadeop", name);
        return nullptr;
    }

    RtFilterFunc lookupFilter(const char *name) {
        CRendererServices *svc = CShadingContext::defaultServices();
        if (svc)
            return svc->getFilter(name);
        warnNoServices(s_warnedFilter, "filter", name);
        return RiGaussianFilter;
    }

    RtStepFilterFunc lookupStepFilter(const char *name) {
        CRendererServices *svc = CShadingContext::defaultServices();
        if (svc)
            return svc->getStepFilter(name);
        warnNoServices(s_warnedStepFilter, "step filter", name);
        return RiGaussianStepFilter;
    }

    int lookupGlobalID(const char *name) {
        CRendererServices *svc = CShadingContext::defaultServices();
        if (svc)
            return svc->getGlobalID(name);
        warnNoServices(s_warnedGlobalID, "light category", name);
        return 0;
    }

    CVariable *lookupVariable(const char *name) {
        CRendererServices *svc = CShadingContext::defaultServices();
        if (svc)
            return svc->retrieveVariable(name);
        warnNoServices(s_warnedVariable, "variable", name);
        return nullptr;
    }

    int parseCategories(const char *categoryString, int *out) {
        const std::string list = categoryString ? categoryString : "";
        const char *const delimiters = ",\t ";
        int count = 0;

        size_t pos = 0;
        while (pos < list.size()) {
            const size_t start = list.find_first_not_of(delimiters, pos);
            if (start == std::string::npos)
                break;
            size_t end = list.find_first_of(delimiters, start);
            if (end == std::string::npos)
                end = list.size();

            // A category with no ID (no services) is skipped, never stored as 0
            const int id = lookupGlobalID(list.substr(start, end - start).c_str());
            if (id != 0)
                out[count++] = id;
            pos = end;
        }

        out[count] = 0; // terminate the list
        return count;
    }

} // namespace libshader
