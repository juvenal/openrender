/**
 * Project: openRender
 *
 * File: sloLoader.cpp
 *
 * Description:
 *   parseSloShader(): the loader of a standalone .slo LLVM bitcode file, moved here verbatim from the
 *   renderer core (src/ri/render/rendererFiles.cpp) by spec 026 (Q5). It references no CRenderer
 *   member, so it belongs to Shading beside parseShader() (the .rslo loader, rslo.y); the core keeps
 *   choosing the backend (getShader(), DB-007 is unchanged). With the JIT off (OPENRENDER_HAVE_LLVM
 *   not defined) this file compiles to nothing.
 *
 *   Extracts named metadata (C3 format) to populate shader type, parameters, and varyingSizes without
 *   requiring a companion .rslo file. Returns nullptr if the .slo has no embedded openrender metadata
 *   (pre-C3 .slo).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "sloLoader.h"

#ifdef OPENRENDER_HAVE_LLVM
#include "config.h"
#include "libshader/shading/llvmJit.h"
#include "libshader/shading/sloMetadata.h"
#include "rslo_code.h" // SL_VARYING_OPERAND
#include "shader.h"    // CShader, SL_SURFACE, SL_LIGHTSOURCE, …
#include "common/variable.h"  // CVariable, EVariableType, EVariableStorage
#include "common/global.h"    // allocate_untyped
#include "common/os.h"        // osProcessEscapes
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Map SLOShaderInfo typeName to SL_* constant.
//
// "volume" (spec 017-jit-builtin-function-coverage, US5): confirmed missing
// here while testing incident()/opposite() -- rslo.y's grammar accepts the
// `volume NAME(...)` shader-type keyword (SL_VOLUME token) and the emitter
// faithfully embeds typeName="volume" into the .slo's metadata (confirmed
// via sloinfo), but this function never checked for it, silently falling
// through to the SL_SURFACE default. There is no distinct SL_VOLUME runtime
// constant (shader.h only defines SURFACE/LIGHTSOURCE/DISPLACEMENT/
// ATMOSPHERE/IMAGER) -- RiAtmosphereV/RiInteriorV/RiExteriorV all bind
// against SL_ATMOSPHERE (rendererContext.cpp), so "volume" maps there too,
// same as the already-handled "atmosphere" typeName string. Every other
// .slo shader type happened to already have a matching branch here; a
// volume-class .slo shader was the one combination nothing had exercised
// before (no shipped .sl file uses the `volume` keyword) -- .rslo loading
// never went through this function at all, so the interpreter path was
// silently unaffected the whole time.
static unsigned int sloShaderType(const std::string &n) {
    if (n == "light")
        return SL_LIGHTSOURCE;
    if (n == "displacement")
        return SL_DISPLACEMENT;
    if (n == "atmosphere" || n == "volume")
        return SL_ATMOSPHERE;
    if (n == "imager")
        return SL_IMAGER;
    return SL_SURFACE;
}

// Float count per type element (for CVariable::numFloats — a component count,
// not a byte size; string components are still counted as 1 "slot" here).
static int sloFloatCount(const std::string &t) {
    if (t == "vector" || t == "normal" || t == "point" || t == "color")
        return 3;
    if (t == "matrix")
        return 16;
    return 1; // float, string
}

// Per-element byte width for varyingSizes[] slot allocation. Strings store a
// char* per element (matches rslo.y's TYPE_STRING branch, which sizes with
// sizeof(char*) instead of sizeof(float)); everything else is float-sized.
static size_t sloElemByteSize(const std::string &t) {
    return (t == "string") ? sizeof(char *) : sizeof(float);
}

// Map SLOParamInfo typeName → EVariableType.
static EVariableType sloVarType(const std::string &t) {
    if (t == "color")
        return TYPE_COLOR;
    if (t == "vector")
        return TYPE_VECTOR;
    if (t == "normal")
        return TYPE_NORMAL;
    if (t == "point")
        return TYPE_POINT;
    if (t == "matrix")
        return TYPE_MATRIX;
    if (t == "string")
        return TYPE_STRING;
    return TYPE_FLOAT;
}

// Sets a 4x4 matrix to diag(d, d, d, 1): what rslo.y gives "matrix m = d" (and,
// with d = 1, a matrix parameter with no constant default).
static void setSloMatrixDiagonal(float *m, float d) {
    for (int j = 0; j < 16; j++)
        m[j] = 0.0f;
    m[0] = m[5] = m[10] = d;
    m[15] = 1.0f;
}

// Parses a string parameter's default from the .slo metadata into `slots`
// (GitHub #66). The text is what the .rslo file carries: one quoted string
// ("hello", "") or a bracketed list ([ "a" "b c" ]). RSL strings cannot hold a
// double quote, so each value runs to the next quote; it is decoded with
// osProcessEscapes() exactly as rslo.l decodes SCRL_TEXT_VALUE, then strdup'd
// and recorded in `owned`. An empty `text` (no constant default; the init
// section computes the value) and slots past the parsed values stay NULL.
static void parseSloStringDefaults(const std::string &text, char **slots, int numSlots, std::vector<char *> &owned) {
    const char *cp = text.c_str();
    for (int j = 0; j < numSlots; j++) {
        cp = strchr(cp, '"');
        if (cp == nullptr)
            break;
        const char *end = strchr(cp + 1, '"');
        if (end == nullptr)
            break;
        std::string value(cp + 1, end);
        char *s = strdup(value.c_str());
        osProcessEscapes(s);
        slots[j] = s;
        owned.push_back(s);
        cp = end + 1;
    }
}

CShader *parseSloShader(const char *shaderName, const char *sloPath) {
    SLOShaderInfo info;
    if (!CLLVMJitEngine::extractMetadataFromFile(sloPath, info) || !info.valid())
        return nullptr; // no embedded metadata → fall back to .rslo

    // Construct with the logical shader name, matching parseShader()'s own
    // `new CShader(shaderName)` (rslo.y) for the .rslo path -- NOT the full
    // .slo file path. CFileResource::name backs CShader::getName(), which is
    // the return value of RSL's shadername() builtin; it must be the
    // logical name ("wood", not "/path/to/wood.slo"). It also backs
    // getShader()'s post-load `globalFiles->insert(cShader->name, cShader)`
    // cache key below, which must match getShader()'s own by-logical-name
    // `globalFiles->find(name, file)` lookup for the cache to ever hit.
    //
    // GitHub issue #3 (spec 017-jit-builtin-function-coverage, US5): this
    // used to construct `new CShader(sloPath)` instead, so shadername()'s
    // no-arg overload returned the .slo file's full path under --jit
    // (confirmed via jitShaderName -> shaderName() -> getName() ->
    // parent->name) instead of the shader's logical name -- silently wrong,
    // no error. Found only after fixing an unrelated masking bug (GitHub
    // issue #8) in shadername_probe.sl's own test pattern; the probe's
    // previous "pass" carried no information about this codepath at all.
    // Nothing else in this function (or its caller) reads `sh->name`/
    // `parent->name` expecting a path -- the JIT-compile call just below
    // already takes `sloPath` as its own explicit argument.
    CShader *sh = new CShader(shaderName);
    sh->type = sloShaderType(info.typeName);

    int numParams = (int)info.params.size();
    int numVars = (int)info.vars.size();
    int numTotal = numParams + numVars;

    // --- varyingSizes[] covers ALL slots (params [0..numParams-1] then vars).
    // Mirrors rslo.y: -(numItems*numComp*sizeof(float)) for uniform, positive for varying.
    sh->numVariables = numTotal;
    if (numTotal > 0) {
        sh->varyingSizes = (int *)malloc(numTotal * sizeof(int));
        int idx = 0;
        auto fillSize = [&](const SLOParamInfo &p) {
            int sz = p.arraySize * sloFloatCount(p.typeName) * (int)sloElemByteSize(p.typeName);
            sh->varyingSizes[idx++] = (p.storage == "uniform") ? -sz : sz;
        };
        for (const auto &p : info.params)
            fillSize(p);
        for (const auto &v : info.vars)
            fillSize(v);
    }

    // --- parameters linked list: ONLY actual shader parameters (Ka, Kd, …), NOT locals.
    // Local temporaries are accessed exclusively by slot index through locals[] at execute time.
    // The list is built in reverse source order (matching rslo.y's prepend convention),
    // so the final order mirrors what the interpreter expects.
    // defaultValue MUST be allocated with new[] as the type ~CShader() frees it with:
    // char *[] for a string, float[] otherwise.
    CVariable *paramHead = nullptr;
    std::vector<char *> defaultStrings; // strdup'd string defaults, handed to sh->strings below
    for (int i = numParams - 1; i >= 0; --i) {
        const SLOParamInfo &p = info.params[i];
        int totalFloats = p.arraySize * sloFloatCount(p.typeName);

        CVariable *v = new CVariable;
        strncpy(v->name, p.name.c_str(), VARIABLE_NAME_LENGTH - 1);
        v->name[VARIABLE_NAME_LENGTH - 1] = '\0';
        v->type = sloVarType(p.typeName);
        v->container = (p.storage == "varying") ? CONTAINER_VARYING : CONTAINER_UNIFORM;
        v->storage = STORAGE_PARAMETER;
        v->numItems = p.arraySize;
        v->numFloats = totalFloats;
        v->entry = i; // slot i in varyingSizes[] and locals[]
        v->accessor = SL_VARYING_OPERAND;
        v->usageMarker = 0;

        // A string default holds one char * per slot (GitHub #66): the instance
        // constructor, setParameter() and prepare() all read it as char **, and
        // a float-sized buffer made the constructor copy 8 bytes out of 4. The
        // strings are owned by the shader (sh->strings, freed by ~CShader).
        if (v->type == TYPE_STRING) {
            const int numSlots = totalFloats > 0 ? totalFloats : 1;
            char **slots = new char *[numSlots];
            for (int j = 0; j < numSlots; j++)
                slots[j] = nullptr;
            parseSloStringDefaults(p.defaultStr, slots, totalFloats, defaultStrings);
            v->defaultValue = slots;
            v->next = paramHead;
            paramHead = v;
            continue;
        }

        // Allocate and populate default value using new[] (matched by delete[] in CShader dtor).
        v->defaultValue = new float[totalFloats > 0 ? totalFloats : 1];
        float *buf = (float *)v->defaultValue;
        for (int j = 0; j < totalFloats; j++)
            buf[j] = 0.0f;
        // A scalar matrix with no constant default is the identity, as rslo.y makes it.
        const bool scalarMatrix = (v->type == TYPE_MATRIX && p.arraySize == 1);
        if (scalarMatrix && p.defaultStr.empty())
            setSloMatrixDiagonal(buf, 1.0f);
        if (!p.defaultStr.empty()) {
            // Reuse parseDefaultStr's logic inline to avoid malloc/new mismatch.
            const char *cp = p.defaultStr.c_str();
            while (*cp == ' ' || *cp == '[')
                ++cp;
            int numParsed = 0;
            for (int j = 0; j < totalFloats; j++) {
                char *end = nullptr;
                buf[j] = strtof(cp, &end);
                if (!end || end == cp)
                    break;
                ++numParsed;
                cp = end;
                while (*cp == ' ' || *cp == ']')
                    ++cp;
            }
            // RSL scalar-to-tuple broadcast: "color = 1" means {1,1,1}, "vector = 0" means {0,0,0}.
            // A matrix is the exception: "matrix = v" means diag(v, v, v, 1), as in rslo.y.
            if (numParsed == 1 && scalarMatrix) {
                setSloMatrixDiagonal(buf, buf[0]);
            }
            else if (numParsed == 1 && totalFloats > 1) {
                for (int j = 1; j < totalFloats; j++)
                    buf[j] = buf[0];
            }
        }

        v->next = paramHead;
        paramHead = v;
    }
    sh->parameters = paramHead;
    sh->numGlobals = 0;

    // Interpreter fields null — JIT is the only execution path for .slo-based shaders
    // (codeArea == nullptr is what marks one). The default strings live in sh->strings,
    // as in rslo.y: ~CShader frees each string and, through `memory`, the array.
    sh->memory = nullptr;
    sh->codeArea = nullptr;
    sh->constantEntries = nullptr;
    sh->strings = nullptr;
    sh->numStrings = (int)defaultStrings.size();
    if (sh->numStrings > 0) {
        sh->memory = (char *)allocate_untyped(sh->numStrings * sizeof(char *));
        sh->strings = (char **)sh->memory;
        for (int j = 0; j < sh->numStrings; j++)
            sh->strings[j] = defaultStrings[j];
    }
    sh->codeEntryPoint = 0;
    sh->initEntryPoint = 0;
    sh->usedParameters = info.usedParameters;
    sh->flags = 0;
    sh->data = nullptr;

    sh->analyse(); // populates flags / CLightShaderData for light shaders

    // Eagerly compile the JIT so that jitInitEntry is available when init() is
    // called at shader-bind time (before the first prepare() is ever called).
    {
        const char *filename = sloPath;
        const char *sep = strrchr(filename, OS_DIR_SEPERATOR);
        const char *base = sep ? sep + 1 : filename;
        const char *ext = strrchr(base, '.');
        std::string logicalName = ext ? std::string(base, ext) : std::string(base);
        sh->jitEntry = CLLVMJitEngine::getInstance().compileShader(sloPath, logicalName);
        sh->jitInitEntry = CLLVMJitEngine::getInstance().lookupInitEntry(logicalName);
    }

    return sh;
}
#endif // OPENRENDER_HAVE_LLVM
