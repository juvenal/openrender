/**
 * Project: openRender
 *
 * File: shaderPayload.h
 *
 * Description:
 *   What a loaded shader carries that only its own backend understands (spec 033, FR-007, data-model.md "Shader payload").
 *   CShader (shader.h) is the backend-neutral description: name, type, parameters, used parameters, flags and data. Everything
 *   else it used to hold, the bytecode, the constant tables, the strings, the frame layout and the JIT entry points, is the
 *   payload of the backend that loaded it:
 *
 *     CShaderPayload            a virtual destructor
 *     |- CRslPayload            the frame layout every RSL backend shares (numVariables, varyingSizes, numGlobals, the strings and
 *     |                         the storage that holds them)
 *        |- CRsloPayload        the interpreter's bytecode: codeArea, constantEntries, codeEntryPoint, initEntryPoint
 *        |- CSloPayload         the JIT's two entry points (OPENRENDER_HAVE_LLVM only)
 *
 *   The accessors are static_casts, used only inside the owning backend and by CProgrammableShaderInstance::prepare().
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADERPAYLOAD_H
#define SHADERPAYLOAD_H

#include <stdlib.h>

#include <vector>

#include "common/memory.h" // free_untyped
#include "config.h"        // OPENRENDER_HAVE_LLVM
#include "shader.h"        // CShader, TCode
#ifdef OPENRENDER_HAVE_LLVM
#include "llvmJit.h" // TShaderJitEntry
#endif

class CShaderPayload {
    public:
        virtual ~CShaderPayload() = default;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRslPayload
// Description			:	The frame layout and the owned storage of a shader written in RSL, whichever backend runs it
// Comments				:	Frees the strings and the memory they and the tables live in, exactly as ~CShader did
class CRslPayload : public CShaderPayload {
    public:
        ~CRslPayload() override {
            // Delete the strings allocated for this shader
            for (int i = 0; i < numStrings; i++)
                free(strings[i]);

            // Ditch the memory baby
            if (memory != nullptr)
                free_untyped(memory);
        }

        void *memory = nullptr;      // The memory base allocated for this shader
        int *varyingSizes = nullptr; // The size of a variable (if negative, it is uniform)
        char **strings = nullptr;    // Strings used by the shader
        int numGlobals = 0;          // Number of global parameters
        int numStrings = 0;          // Number of strings
        int numVariables = 0;        // Number of variables
        std::vector<char> stringSlot; // One entry per variable: non-zero when the variable is a string (prepare() points an unset one at "")
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRsloPayload
// Description			:	The interpreter's: the bytecode of an .rslo shader
class CRsloPayload : public CRslPayload {
    public:
        TCode *codeArea = nullptr;           // The code array
        void **constantEntries = nullptr;    // The constant entries
        int codeEntryPoint = 0;              // Index into code array where the actual code starts
        int initEntryPoint = 0;              // Index into code array where the init code starts
};

#ifdef OPENRENDER_HAVE_LLVM
///////////////////////////////////////////////////////////////////////
// Class				:	CSloPayload
// Description			:	The JIT's: the entry points of a compiled .slo shader (no bytecode)
// Comments				:	jitEntry gets a non-null value only in sloLoader.cpp (parseSloShader); a null one with this payload means
//							the JIT compile failed
class CSloPayload : public CRslPayload {
    public:
        TShaderJitEntry jitEntry = nullptr;     // JIT-compiled main entry (set at load time)
        TShaderJitEntry jitInitEntry = nullptr; // JIT-compiled init entry (nullptr if no #!Init)
};
#endif

// The payload of a description, as the type its backend made it. Only the owning backend, its loader and prepare() use these.
inline CRslPayload *rslPayloadOf(CShader &shader) { return static_cast<CRslPayload *>(shader.payload); }
inline const CRslPayload *rslPayloadOf(const CShader &shader) { return static_cast<const CRslPayload *>(shader.payload); }
inline CRsloPayload *rsloPayloadOf(CShader &shader) { return static_cast<CRsloPayload *>(shader.payload); }
inline const CRsloPayload *rsloPayloadOf(const CShader &shader) { return static_cast<const CRsloPayload *>(shader.payload); }
#ifdef OPENRENDER_HAVE_LLVM
inline CSloPayload *sloPayloadOf(CShader &shader) { return static_cast<CSloPayload *>(shader.payload); }
inline const CSloPayload *sloPayloadOf(const CShader &shader) { return static_cast<const CSloPayload *>(shader.payload); }
#endif

#endif
