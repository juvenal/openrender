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

// The programmable instance an active-light entry holds.
// DB-013: unchecked, as before spec 025. The built-in sphere and quad lights (hcshader.h) are not
// programmable instances; the consumers read only CShaderInstance members, so the cast is never
// followed through a CProgrammableShaderInstance-only member for them.
inline CProgrammableShaderInstance *lightInstanceOf(const CActiveLight &entry) {
    return static_cast<CProgrammableShaderInstance *>(shaderInstanceOf(entry.light));
}

#endif
