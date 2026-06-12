/**
 * Project: openRender — libshader
 *
 * File: RSLShading.cpp
 *
 * Description:
 *   RSLShading public API implementation — Phase B Step B5.
 *
 *   RSLShading::shade() is the library's single entry point for executing
 *   a compiled RSL shader. In Phase B it delegates to CShadingContext::execute()
 *   (the existing interpreter) using the shading context already set up by
 *   the renderer. The RSLShadingState parameter carries the varying/local
 *   storage and will carry renderer-service callbacks in Phase B+.
 *
 *   Phase B JIT hook (B6, future):
 *   When RSLShadingState.jitEnabled is set and a LLVM-compiled form exists,
 *   RSLShading::shade() will call the JIT path instead of the interpreter.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "libshader/include/openrender/RSLShading.h"
#include "shader.h"   // CProgrammableShaderInstance
#include "shading.h"  // CShadingContext

///////////////////////////////////////////////////////////////////////
// RSLShading::shade
// Description: Execute a compiled shader via the interpreter.
//              ctx->execute() runs the bytecode dispatch loop in execute.cpp.
///////////////////////////////////////////////////////////////////////
void RSLShading::shade(CProgrammableShaderInstance *shader, RSLShadingState &state) {
    if (!shader || !state.ctx)
        return;

    state.ctx->execute(shader, state.locals);
}
