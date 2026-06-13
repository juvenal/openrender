/**
 * Project: openRender
 *
 * File: llvmJit.h
 *
 * Description:
 *   CLLVMJitEngine: Manages ORC JIT v2 for on-the-fly shader compilation.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RI_LLVMJIT_H
#define RI_LLVMJIT_H

#include "common/global.h"
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>

// Forward declarations for LLVM types to keep headers clean
namespace llvm {
    class Module;
    namespace orc {
        class LLJIT;
    }
}

/**
 * @brief Signature for the JIT-compiled shader entry point.
 * 
 * @param numVertices Number of vertices in the chunk to process.
 * @param stuff Pointer to the access arrays (constants, globals, locals).
 * @param tags Pointer to the activity tags array.
 */
typedef void (*TShaderJitEntry)(int numVertices, void*** stuff, int* tags);

/**
 * @brief CLLVMJitEngine manages the LLVM JIT infrastructure in the renderer.
 */
class CLLVMJitEngine {
public:
    CLLVMJitEngine();
    ~CLLVMJitEngine();

    /**
     * @brief Loads an .slo bitcode file and adds it to the JIT.
     * 
     * @param filename Path to the .slo file.
     * @param shaderName Name of the shader entry point.
     * @return TShaderJitEntry Function pointer to the compiled shader, or nullptr on failure.
     */
    TShaderJitEntry compileShader(const std::string &filename, const std::string &shaderName);

    /**
     * @brief Singleton access for the global JIT engine.
     */
    static CLLVMJitEngine& getInstance();

private:
    std::unique_ptr<llvm::orc::LLJIT> jit;
    std::mutex compileMutex_;
    std::unordered_map<std::string, TShaderJitEntry> cache_;
};

#endif // RI_LLVMJIT_H
