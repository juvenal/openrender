/**
 * Project: openRender
 *
 * File: shaderRefs.h
 *
 * Description:
 *   Resolves Graphics State's opaque shader handles back to Shading's own types
 *   (spec 025, contracts/state-handles.md). Inline only: the conversion is a
 *   static_cast, so no reference count is touched and no symbol is added.
 *
 *   The renderer core and Shading use these. Graphics State, Geometry,
 *   Texturing and Scene Interface never include this header.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADERREFS_H
#define SHADERREFS_H

#include "attributes.h"
#include "shader.h"
#include "stateHandles.h"

// The shader instance a slot target holds (nullptr stays nullptr). Every class between
// CShaderInstance and CShaderGroupRefTarget uses single inheritance, so the pointer value
// is the same as before spec 025.
inline CShaderInstance *shaderInstanceOf(const CShaderGroupRefTarget *target) {
    return static_cast<CShaderInstance *>(const_cast<CShaderGroupRefTarget *>(target));
}

inline CShaderInstance *shaderInstanceOf(const ShaderGroupRef &ref) {
    return shaderInstanceOf(ref.get());
}

// The shader instance an active-light entry holds (spec 033, P-Q9: no downcast). Every member the callers use, the categories, the
// flags, prepare(), illuminate() and requiredParameters(), is on the base, whichever backend made the light.
inline CShaderInstance *lightInstanceOf(const CActiveLight &entry) {
    return shaderInstanceOf(entry.light);
}

#endif
