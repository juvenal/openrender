/**
 * Project: openRender
 *
 * File: serviceLookup.h
 *
 * Description:
 *   The shading library's lookups of things the renderer core owns: DSO
 *   shadeops, texture and step filters, light-category IDs and variables.
 *   Each forwards to the default renderer services
 *   (CShadingContext::defaultServices()), so the library never names the
 *   renderer core. They exist for call sites that run outside a shading
 *   context (the .rslo loader, shader instantiation, the gather lookup).
 *
 *   With no services installed, each returns the not-found value: nullptr,
 *   RiGaussianFilter, RiGaussianStepFilter, 0 and nullptr.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef SERVICELOOKUP_H
#define SERVICELOOKUP_H

#include "common/riVocabulary.h" // RtFilterFunc, RtStepFilterFunc

class CDSO;
class CVariable;

namespace libshader {

    // The shadeop implementing `name` with the RSL prototype, or nullptr
    CDSO *lookupDSO(const char *name, const char *prototype);

    // The named filter (RiGaussianFilter when the name is unknown or there are no services)
    RtFilterFunc lookupFilter(const char *name);

    // The named step filter (RiGaussianStepFilter likewise)
    RtStepFilterFunc lookupStepFilter(const char *name);

    // The ID of a light category, 0 with no services
    int lookupGlobalID(const char *name);

    // The declared variable, or nullptr
    CVariable *lookupVariable(const char *name);

} // namespace libshader

#endif // SERVICELOOKUP_H
