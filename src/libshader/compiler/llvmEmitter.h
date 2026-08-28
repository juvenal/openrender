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

/**
 * @brief Single source of truth for the opcode mnemonics emitFunction()'s
 *        dispatch recognizes. nullptr-terminated, defined in llvmEmitter.cpp.
 *
 *        External (not TU-local static) linkage is deliberate: the
 *        libshader coverage-guard ctest links against this symbol and reads
 *        it directly, so it can never drift from what the emitter actually
 *        dispatches on (spec 011-jit-opcode-parity, research.md D3).
 */
extern const char *const kHandledOpcodes[];

/**
 * @brief One (mnemonic-or-function-name, PARAMETER_* bits) row, re-expanded
 *        verbatim from the interpreter's own opcode/function tables
 *        (shaderOpcodes.h/shaderFunctions.h/giOpcodes.h/giFunctions.h via
 *        scriptOpcodes.h/scriptFunctions.h) by llvmEmitter.cpp's local
 *        DEFOPCODE/DEFFUNC-family X-macro redefinition — the same mechanism
 *        rslo_code.h:31-49 uses to build TRSLObjectCode, but capturing
 *        `params` instead of discarding it (spec 014-jit-shading-parity,
 *        data-model.md "Opcode/Function Bit Table").
 */
struct OpcodeParamEntry {
    const char *text;
    unsigned int params;
};

/**
 * @brief Single source of truth for which PARAMETER_* bits each opcode or
 *        built-in function name carries, mirroring the interpreter's tables
 *        by construction (re-#include, not hand-transcription). Terminated
 *        by a {nullptr, 0} sentinel. External linkage so the libshader
 *        table-parity ctest can read it directly (table-parity-contract.md).
 */
extern const OpcodeParamEntry kOpcodeParamTable[];

/**
 * @brief Computes the openrender.shader.usedparameters bitmask for a
 *        compiled IRModule — the single source of truth embedMetadata()
 *        embeds into the .slo, and what the libshader gating-condition
 *        ctest calls directly (in-process, no .slo round-trip) to assert
 *        on specific PARAMETER_* bits for small fixture sources compiled
 *        via CScriptContext{emitJIT=true} (spec 014-jit-shading-parity,
 *        gating-condition-contract.md).
 */
unsigned int computeUsedParameters(const IRModule &ir);

#endif // OSHADER_LLVM_EMITTER_H
