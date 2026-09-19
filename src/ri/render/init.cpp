/**
 * Project: openRender
 *
 * File: init.cpp
 *
 * Description:
 *   This file implements the functionality for init.
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
//  File				:	init.cpp
//  Classes				:	-
//  Description			:	This file implements the shading language interpreter that runs the init code
//
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include "common/colorSpace.h"
#include "error.h"
#include "memory.h"
#include "noise.h"
#include "random.h"
#include "renderer.h"
#include "rendererContext.h"
#include "ri_config.h"
#include "shader.h"
#include "shading.h"
#include "rslo_code.h"

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
#define findCoordinateSystem CRenderer::findCoordinateSystem
#define threadMemory CRenderer::globalMemory
#define urand() _urand()

///////////////////////////////////////////////////////////////////////
// Function				:	sfInit
// Description			:	Execute the current shader's init code
// Return Value			:	-
// Comments				:
void CRendererContext::init(CProgrammableShaderInstance *currentShaderInstance) {
    // At this point, the shader sends us the arrays for parameters/constants/variables/uniforms for the shader

#define scripterror(mes)                                    \
    {                                                       \
        error(CODE_SCRIPT, "\"%s\", (nullified)\n", mes);   \
        currentShaderInstance->parent->codeEntryPoint = -1; \
        currentShaderInstance->parent->initEntryPoint = -1; \
        goto execEnd;                                       \
    }
//	Allocate temporary memory for the string and save it
#define savestring(r, n)                                      \
    {                                                         \
        int strLen = (int)strlen(n) + 1;                      \
        int strSize = (strLen & ~3) + 4;                      \
        char *strmem = (char *)ralloc(strSize, threadMemory); \
        strcpy(strmem, n);                                    \
        r = strmem;                                           \
    }

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
        code = currentShader->codeArea + n; \
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

#ifdef OPENRENDER_HAVE_LLVM
    // .slo-only (JIT) shaders have no interpreter bytecode.
    // Run the JIT init section here, at shader-bind time, so that op_pfrom("shader",...)
    // uses context->getXform() which correctly reflects the shader's bind-time transform.
    if (currentShader->codeArea == nullptr) {
        if (currentShaderInstance->jitInitEntry != nullptr) {
            const int nVars = currentShader->numVariables;
            // locals: param slots point at persistent defaultValue storage so the
            // init results (e.g. transformed light position) persist across frames.
            // Temporary slots get transient scratch buffers (3 floats each, uniform).
            void **jitLocals = (void **)alloca(nVars * sizeof(void *));
            float *scratch    = (float *)alloca(nVars * 3 * sizeof(float));
            memset(scratch, 0, nVars * 3 * sizeof(float));
            for (int vi = 0; vi < nVars; vi++)
                jitLocals[vi] = scratch + vi * 3;
            for (CVariable *v = currentShaderInstance->parameters; v; v = v->next)
                if (v->entry < nVars && v->defaultValue != nullptr)
                    jitLocals[v->entry] = v->defaultValue;

            void **stuffInit[3];
            stuffInit[SL_IMMEDIATE_OPERAND] = currentShader->constantEntries;
            stuffInit[SL_GLOBAL_OPERAND]    = nullptr; // init section never uses globals
            stuffInit[SL_VARYING_OPERAND]   = jitLocals;

            int initTag = 0;
            // Supply the shader-space xform so op_pfrom("shader",...) in the init
            // section can convert space-qualified parameter defaults correctly.
            // activeContext() is null at bind time; jitSetInitXform provides the fallback.
            extern void jitSetInitXform(const float*, const float*);
            if (currentShaderInstance->xform) {
                jitSetInitXform(currentShaderInstance->xform->from,
                                currentShaderInstance->xform->to);
            }
            currentShaderInstance->jitInitEntry(1, (void ***)stuffInit, &initTag);
            jitSetInitXform(nullptr, nullptr);
        }
        return;
    }
#endif
    code = currentShader->codeArea + currentShader->initEntryPoint;

    numVertices = 1;
    tagStart = &tmpTags;
    tmpTags = 0;

    // Setup local variables
    stuff[SL_VARYING_OPERAND] = (void **)ralloc(currentShader->numVariables * sizeof(void *), threadMemory); // Shader varying variables
    for (i = 0; i < currentShader->numVariables; i++) {                                                      // Allocate memory for every varying variable
        int size = currentShader->varyingSizes[i];

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
            CVariable *cVar = CRenderer::retrieveVariable(cParameter->name);
            cParameter->entry = cVar->entry;
        }
    }

    varying = NULL;
    lights = NULL;
    alights = NULL;
    currentLight = NULL;

    // Set the access arrays
    stuff[SL_IMMEDIATE_OPERAND] = currentShader->constantEntries; // Immediate operands
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
    case OPCODE_##name: {                                                           \
        expr_pre;                                                                   \
        expr;                                                                       \
        expr_post                                                                   \
            code++;                                                                 \
        goto execStart;                                                             \
    } break;

#define DEFSHORTOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, param) \
    case OPCODE_##name: {                                                                \
        expr_pre;                                                                        \
        expr;                                                                            \
        expr_post                                                                        \
            code++;                                                                      \
        goto execStart;                                                                  \
    } break;

#define DEFFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) \
    case FUNCTION_##name: {                                                         \
        expr_pre;                                                                   \
        expr;                                                                       \
        expr_post                                                                   \
            code++;                                                                 \
        goto execStart;                                                             \
    } break;

#define DEFLIGHTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) \
    case FUNCTION_##name: {                                                              \
        expr_pre;                                                                        \
        expr;                                                                            \
        expr_post                                                                        \
            code++;                                                                      \
        goto execStart;                                                                  \
    } break;

#define DEFSHORTFUNC(name, text, prototype, expr_pre, expr, expr_update, expr_post, par) \
    case FUNCTION_##name: {                                                              \
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
#undef DEFLINKOPCODE
#undef DEFLINKFUNC
#undef BREAK
}
