/**
 * Project: openRender
 *
 * File: rsloInit.cpp
 *
 * Description:
 *   The interpreter backend's bind-time init (spec 033): moved from src/ri/render/init.cpp, where it was
 *   CRendererContext::init(). Only its hooks changed: a named coordinate system is asked of the services
 *   (libshader::bindCoordinateSystem, getMatrix with phase == Bind), a global parameter's variable through
 *   libshader::lookupVariable, and the scratch memory is globalArena.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	rsloInit.cpp
//  Classes				:	-
//  Description			:	This file implements the shading language interpreter that runs the init code
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

// The init block runs once, at shader-bind time, outside any shading context: a DSO call in it uses the data of
// thread index 0 (scriptFunctions.h, before its first inclusion)
#define DSO_THREAD_INDEX 0

#include "common/colorSpace.h"
#include "common/diagnostics.h"
#include "common/memory.h"
#include "noise.h"
#include "common/random.h"
#include "common/sampling.h"
#include "ri_config.h"
#include "serviceLookup.h"
#include "dsoHandle.h" // CDSO
#include "rslo_code.h"
#include "shader.h"
#include "shaderPayload.h"
#include "shading.h"
#include "specularTerm.h"

////////////////////////////////////////////////////////////////////////////////////////////////
//	Prototypes for shading language functions
//	These functions are defined in interpreter.cpp
////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
// Shading language functions ... (implemented in execute.cpp)
#define saveLighting(a) scripterror("Invalid environment function call during init\n")
#define clearLighting() scripterror("Invalid environment function call during init\n")
#define enterLightingConditional() scripterror("Invalid environment function call during init\n")
#define exitLightingConditional() scripterror("Invalid environment function call during init\n")
#define enterFastLightingConditional() scripterror("Invalid environment function call during init\n")
#define exitFastLightingConditional() scripterror("Invalid environment function call during init\n")
#define rendererInfo(a, b) scripterror("Invalid environment function call during init\n")
#define emission(a, b) scripterror("Invalid environment function call during init\n")
#define debugFunction(a)
#define illuminateBegin(a, b, c, d, e, f)
// The named coordinate systems of the parameter defaults: the services answer, as a bind-time request (GitHub #134)
#define findCoordinateSystem(name, from, to, kind) libshader::bindCoordinateSystem((name), currentShaderInstance, (from), (to), (kind))
// The shared opcode/builtin bodies included below allocate from threadMemory,
// a CShadingContext member when they run while shading. Shader init code runs
// outside any shading context, so here they allocate from the process-wide
// scratch arena instead (spec 023: globalArena, formerly the renderer core's global memory).
#define threadMemory globalArena.top

// random()/urandom() in init code (spec 039, research R3.10): no shading context and
// no shading point, so the draws come from a local keyed stream of (shader, call site,
// the number of draws this init run made so far) instead of a process-wide generator.
// The same shader instance therefore draws the same values in every run and thread.
static void initDraw(CProgrammableShaderInstance *instance, uint32_t drawNumber, unsigned callSite, float *res, int n) {
    uint64_t nameKey = 0x1417ull;
    for (const char *c = instance->getName(); c != nullptr && *c != '\0'; ++c)
        nameKey = openrender::sampling::detail::mix(nameKey, (uint64_t)(unsigned char)*c);
    openrender::sampling::CKeyedStream stream(openrender::sampling::detail::mix(nameKey, callSite, drawNumber));
    for (int k = 0; k < n; k++)
        res[k] = stream.next1D();
}
#define drawCounters(__callSite) ((uint32_t *)nullptr)
#define drawRandom(__vertex, __counts, __callSite, __res, __n) \
    ((void)(__vertex), (void)(__counts), initDraw(currentShaderInstance, initDrawCounter++, (__callSite), (__res), (__n)))

///////////////////////////////////////////////////////////////////////
// Function				:	initialiseRsloInstance
// Description			:	Execute the current shader's init code (the interpreter's)
// Return Value			:	-
// Comments				:	Run by libshader::initialiseInstance() inside arenaBegin/arenaEnd(globalArena)
void libshader::initialiseRsloInstance(CProgrammableShaderInstance &instance) {
    CProgrammableShaderInstance *currentShaderInstance = &instance;
    CRsloPayload *payload = rsloPayloadOf(*instance.parent); // The interpreter's bytecode and frame layout
    uint32_t initDrawCounter = 0; // The number of random() draws this init run has made
    // At this point, the shader sends us the arrays for parameters/constants/variables/uniforms for the shader

#define scripterror(mes)                                    \
    {                                                       \
        error(CODE_SCRIPT, "\"%s\", (nullified)\n", mes);   \
        payload->codeEntryPoint = -1;                       \
        payload->initEntryPoint = -1;                       \
        goto execEnd;                                       \
    }
//	Keep a copy of the string for the life of the instance: the result lands in a parameter default, which outlives the
//	arena region bind() runs this init in (GitHub #192)
#define savestring(r, n) r = (char *)currentShaderInstance->persistString(n)

//	Begin a conditional block execution
#define beginConditional()                \
    if (conditionals == NULL) {           \
        conditionals = new CConditional;  \
        conditionals->next = NULL;        \
        conditionals->prev = NULL;        \
    }                                     \
                                          \
    conditionals->prev = lastConditional; \
    lastConditional = conditionals;       \
    conditionals = lastConditional->next;

//	End a conditional block execution
#define endConditional()                  \
    lastConditional->next = conditionals; \
    conditionals = lastConditional;       \
    lastConditional = lastConditional->prev;

//	Retrieve a pointer to an operand and obtain it's size
#define operand(i, n, t)                          \
    {                                             \
        const TArgument ref = code->arguments[i]; \
        n = (t)stuff[ref.accessor][ref.index];    \
    }

    //	Retrieve an operand's size
#define operandSize(i, n, s, t)                   \
    {                                             \
        const TArgument ref = code->arguments[i]; \
        n = (t)stuff[ref.accessor][ref.index];    \
        s = ref.numItems;                         \
    }

#define operandNumItems(i) code->arguments[i].numItems

#define operandBytesPerItem(i) code->arguments[i].bytesPerItem

#define operandVaryingStep(i) code->arguments[i].varyingStep

//	Retrieve an integer operand (label references are integer)
#define argument(i) code->arguments[i].index

//	Retrieve the number of arguments
#define argumentcount(n) n = code->numArguments

//	Control transfer
#define jmp(n)                              \
    {                                       \
        code = payload->codeArea + n;       \
        goto execStart;                     \
    }

//	Run the light source shaders for the lP
#define runAmbientLights() scripterror("Light source exec during init code\n");

//	Run the light source shaders for the lP
#define runLights(lP, lN, lT) scripterror("Light source exec during init code\n");
#define runCategoryLights(lP, lN, lT, lC) scripterror("Light source exec during init code\n");

// The misc macros
#define DEFLINKOPCODE(name, text, nargs) case OPCODE_##name:
#define DEFLINKFUNC(name, text, prototype, par) case FUNCTION_##name:

// Break the shader execution
#define BREAK goto execEnd;

    //	The	shading variables and junk
    void **stuff[3];               // Where we keep pointers to the variables
    CConditional *lastConditional; // The last conditional
    int numActive;
    int numPassive;
    int *tags;
    int *tagStart;
    CShader *currentShader = currentShaderInstance->parent;
    const TCode *code;
    int tmpTags;
    int numVertices;
    float **varying;
    CShadedLight **lights;
    CShadedLight **alights;
    CShadedLight **currentLight;
    CConditional *conditionals = NULL;
    CConditional *cConditional;
    int i;
    CVariable *cParameter;

    code = payload->codeArea + payload->initEntryPoint;

    numVertices = 1;
    tagStart = &tmpTags;
    tmpTags = 0;

    // Setup local variables
    stuff[SL_VARYING_OPERAND] = (void **)ralloc(payload->numVariables * sizeof(void *), threadMemory); // Shader varying variables
    for (i = 0; i < payload->numVariables; i++) {                                                      // Allocate memory for every varying variable
        int size = payload->varyingSizes[i];

        if (size != 0) {
            if (size < 0)
                size = -size;
            stuff[SL_VARYING_OPERAND][i] = (void *)ralloc(size, threadMemory);
        }
    }

    // Now go thru the parameters and replace the corresponding variables with their default values
    for (cParameter = currentShaderInstance->parameters; cParameter != NULL; cParameter = cParameter->next) {
        assert(cParameter->defaultValue != NULL);

        stuff[SL_VARYING_OPERAND][cParameter->entry] = (TCode *)cParameter->defaultValue;

        // Relink the entry point to the global, we already verified the match
        if (cParameter->storage == STORAGE_GLOBAL) {
            CVariable *cVar = libshader::lookupVariable(cParameter->name);
            cParameter->entry = cVar->entry;
        }
    }

    varying = NULL;
    lights = NULL;
    alights = NULL;
    currentLight = NULL;

    // Set the access arrays
    stuff[SL_IMMEDIATE_OPERAND] = payload->constantEntries; // Immediate operands
    stuff[SL_GLOBAL_OPERAND] = (void **)varying;                  // Varying globals

    numActive = numVertices;
    numPassive = 0;
    lastConditional = NULL; // The last conditional block

    // Execute
execStart:
    const TRSLObjectCode opcode = (TRSLObjectCode)code->opcode;

    tags = tagStart;

#define INIT_SHADING
#define DEFOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, param) \
    case OPCODE_##name:                                                             \
    {                                                                               \
        expr_pre;                                                                   \
        expr;                                                                       \
        expr_post                                                                   \
            code++;                                                                 \
        goto execStart;                                                             \
    } break;

#define DEFSHORTOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, param) \
    case OPCODE_##name:                                                                  \
    {                                                                                    \
        expr_pre;                                                                        \
        expr;                                                                            \
        expr_post                                                                        \
            code++;                                                                      \
        goto execStart;                                                                  \
    } break;

#define DEFFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) \
    case FUNCTION_##name:                                                           \
    {                                                                               \
        expr_pre;                                                                   \
        expr;                                                                       \
        expr_post                                                                   \
            code++;                                                                 \
        goto execStart;                                                             \
    } break;

// DEFPRINTFUNC (GitHub #13): this file only ever executes the Init: block
// (parameter-default setup), which runs once regardless -- identical to
// DEFFUNC's expansion here. execute.cpp's per-vertex Code: block dispatch
// is the only place DEFPRINTFUNC's uniform-fast-path removal actually
// matters.
#define DEFPRINTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) \
    case FUNCTION_##name:                                                                \
    {                                                                                    \
        expr_pre;                                                                        \
        expr;                                                                            \
        expr_post                                                                        \
            code++;                                                                      \
        goto execStart;                                                                  \
    } break;

#define DEFLIGHTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) \
    case FUNCTION_##name:                                                                \
    {                                                                                    \
        expr_pre;                                                                        \
        expr;                                                                            \
        expr_post                                                                        \
            code++;                                                                      \
        goto execStart;                                                                  \
    } break;

#define DEFSHORTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) \
    case FUNCTION_##name:                                                                \
    {                                                                                    \
        expr_pre;                                                                        \
        expr;                                                                            \
        expr_post                                                                        \
            code++;                                                                      \
        goto execStart;                                                                  \
    } break;

    switch (opcode) {

#include "scriptOpcodes.h"

#include "scriptFunctions.h"

        default:
            goto execEnd;
    }

    code++;
    goto execStart;
#undef DEFOPCODE
#undef DEFFUNC
#undef DEFPRINTFUNC
#undef DEFLIGHTFUNC
#undef INIT_SHADING

    goto execStart;
execEnd:

    // Delete the conditionals
    while ((cConditional = conditionals) != NULL) {
        conditionals = cConditional->next;
        delete cConditional;
    }

    assert(numActive == numVertices);
    assert(numPassive == 0);

// Undefine junk
#undef savestring
#undef allocbuffer
#undef freebuffer
#undef beginConditional
#undef endConditional
#undef operand
#undef argument
#undef argumentcount
#undef jmp
#undef runAmbientLights
#undef runCategoryLights
#undef runLights
#undef firstLight
#undef nextLight
#undef allLights
#undef currentLight
#undef saveLight
#undef uniformGlobal
#undef varyingGlobal
#undef DEFOPCODE
#undef DEFFUNC
#undef DEFPRINTFUNC
#undef DEFLINKOPCODE
#undef DEFLINKFUNC
#undef BREAK
}
