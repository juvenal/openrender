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

#include "common/formatVersion.h"
#include "common/global.h"
#include "sloMetadata.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Forward declarations for LLVM types to keep headers clean
namespace llvm {
    class LLVMContext;
    class Module;
    namespace orc {
        class LLJIT;
    }
} // namespace llvm

/**
 * @brief Signature for the JIT-compiled shader entry point.
 *
 * @param numVertices Number of vertices in the chunk to process.
 * @param stuff Pointer to the access arrays (constants, globals, locals).
 * @param tags Pointer to the activity tags array.
 */
typedef void (*TShaderJitEntry)(int numVertices, void ***stuff, int *tags);

/**
 * @brief CLLVMJitEngine manages the LLVM JIT infrastructure in the renderer.
 */
class CLLVMJitEngine {
    public:
        CLLVMJitEngine();
        ~CLLVMJitEngine();

        /**
         * @brief Loads an .slo bitcode file, extracts its named metadata, and JIT-compiles it.
         *
         * On success, the SLOShaderInfo extracted from the module is stored in the metadata
         * cache and can be retrieved with getCachedMetadata().
         *
         * @param filename   Path to the .slo file.
         * @param shaderName Name of the shader entry point.
         * @return TShaderJitEntry Function pointer to the compiled shader, or nullptr on failure.
         */
        TShaderJitEntry compileShader(const std::string &filename, const std::string &shaderName);

        /**
         * @brief Returns the metadata cached by the most recent compileShader() call for
         *        the given shader name.  Returns false if no metadata was found in the module.
         */
        bool getCachedMetadata(const std::string &shaderName, SLOShaderInfo &info) const;

        /**
         * @brief Read-only metadata probe: parses the .slo bitcode and extracts named metadata
         *        without initializing the JIT or adding anything to the ORC engine.
         *        Suitable for tools (e.g., sloinfo) that only inspect .slo files.
         *
         * @param filename Path to the .slo file.
         * @param info     Output: populated on success.
         * @return true if the metadata was found and parsed, false otherwise.
         */
        static bool extractMetadataFromFile(const std::string &filename, SLOShaderInfo &info);

        /**
         * @brief Look up an already-compiled init entry for the given shader name.
         *
         * Returns the init entry function pointer if the shader has a non-trivial
         * #!Init section (shadername_init symbol), or nullptr if it does not.
         * Must be called after compileShader() for the same shader.
         */
        TShaderJitEntry lookupInitEntry(const std::string &shaderName);

        /**
         * @brief Binds the DSO shadeop operands a compiled module calls (spec 038).
         *
         * A module that calls shadeop operands carries the slot list `openrender.shader.shadeops`
         * (name and prototype per slot) and the slot array `@"<shader>.shadeops"`, which compileShader()
         * records. This writes each slot in order from resolve(name, prototype), and stops at the first
         * nullptr. The slots belong to the renderer session that resolved them, so the loader calls this
         * on every load, not only when the module is compiled. A module without slots binds nothing.
         *
         * @return -1 when every slot is bound, otherwise the index of the slot that could not be
         *         resolved (its name and prototype are written to the two optional outputs).
         */
        int bindShadeops(const std::string &shaderName,
                         const std::function<void *(const std::string &name, const std::string &prototype)> &resolve,
                         std::string *failedName = nullptr, std::string *failedPrototype = nullptr);

        /**
         * @brief Singleton access for the global JIT engine.
         */
        static CLLVMJitEngine &getInstance();

        /**
         * @brief Register a process-level symbol with LLVM's dynamic library table.
         *
         * Called by jitSymbolRetain.cpp to register op_* and rsl_* function addresses
         * so the JIT can resolve them via DynamicLibrarySearchGenerator.  The name
         * should be the C function name WITHOUT the platform ABI underscore prefix.
         */
        static void addProcessSymbol(const char *name, void *addr);

    private:
        std::unique_ptr<llvm::orc::LLJIT> jit;
        std::mutex compileMutex_;
        std::unordered_map<std::string, TShaderJitEntry> cache_;
        std::unordered_map<std::string, TShaderJitEntry> initCache_;
        std::unordered_map<std::string, SLOShaderInfo> metaCache_;

        // The DSO operands a compiled module calls (spec 038): per shader name the slots read from
        // `openrender.shader.shadeops` and the address of the module's slot array.
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> shadeopSlots_;
        std::unordered_map<std::string, void **> shadeopArrays_;

        // Errors the JIT session reports on its own (the unresolved-symbol list of a
        // module that cannot bind, for one) while a compile is in flight; compileShader()
        // adds them to its diagnostic.
        std::mutex sessionErrorMutex_;
        std::string sessionErrors_;

        // Read the module's `openrender.slo.format` record (sloFormat.h). False when the
        // module has none, or one that is not three integer operands.
        static bool readFormat(const llvm::Module &mod, SFormatVersion &version);

        // Extract named metadata from an already-loaded module.
        static bool extractMetadataFromModule(const llvm::Module &mod, SLOShaderInfo &info);
};

#endif // RI_LLVMJIT_H
