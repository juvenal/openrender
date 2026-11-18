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
 *   RiGaussianFilter, RiGaussianStepFilter, 0 and nullptr. The first lookup of
 *   each kind reports one CODE_NOTSTARTED warning; later ones are silent.
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

#include "common/rendererc.h"      // ECoordinateSystem
#include "common/riVocabulary.h" // RtFilterFunc, RtStepFilterFunc

class CDSO;
class CShaderInstance;
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

    // A coordinate system named by a parameter default, at bind time (renderer services 2.4.0, GitHub #134): asks the services'
    // getMatrix() with phase == Bind and `instance` as the shader. Found: `from`/`to` and `kind` as the renderer answered (its raw
    // named-system lookup: nothing is substituted) and TRUE. Not found: the identity, `kind` COORDINATE_CUSTOM, no warning, FALSE
    // (both backends; the interpreter's answer was undefined before, P-Q2). No services (or a service that answers Unavailable): the
    // identity and one CODE_NOTSTARTED warning "renderer services do not provide coordinate systems". Used by the interpreter's init
    // (rsloInit.cpp) and the JIT's (sloInit.cpp).
    int bindCoordinateSystem(const char *name, const CShaderInstance *instance, const float *&from, const float *&to, ECoordinateSystem &kind);

    // Parses a "__category" string (names separated by commas, tabs or spaces) into the
    // IDs of its categories: appends each non-zero ID from lookupGlobalID() to `out`,
    // terminates the list with 0 and returns how many IDs it holds. A category whose ID
    // is 0 (no renderer services) is skipped, never stored. `out` must hold one entry
    // more than the string has names.
    int parseCategories(const char *categoryString, int *out);

} // namespace libshader

#endif // SERVICELOOKUP_H
