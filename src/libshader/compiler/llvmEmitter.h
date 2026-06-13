/**
 * Project: openRender
 *
 * File: llvmEmitter.h
 *
 * Description:
 *   CLLVMEmitter: translates a compiled IRModule to LLVM bitcode (.slo file).
 *
 *   The emitter generates an LLVM IR function for each shader that:
 *   1. Receives (int numVertices, void*** stuff, int* tags) — the JIT entry signature.
 *   2. Loads variable pointers from stuff[slot][idx].
 *   3. For each IR instruction, calls the corresponding op_* / rsl_* batch function.
 *   4. Embeds named metadata (openrender.shader.*) in the module so the .slo is
 *      self-contained (no companion .rslo required).
 *
 *   External function symbols (op_normalize, op_diffuse_batch, etc.) are resolved
 *   at JIT load time from the libshader_shading shared library.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr.
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#ifndef OSHADER_LLVM_EMITTER_H
#define OSHADER_LLVM_EMITTER_H

#include "ir.h"
#include <string>

/**
 * @brief Translates an IRModule to a standalone .slo LLVM bitcode file.
 *
 * @param mod       The compiled shader IR.
 * @param outPath   Destination .slo path.
 * @param shaderName  Shader name (also the LLVM function name).
 * @return true on success, false on error.
 */
bool emitLLVMBitcode(const IRModule &mod,
                     const std::string &outPath,
                     const std::string &shaderName);

#endif // OSHADER_LLVM_EMITTER_H
