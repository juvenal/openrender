/**
 * Project: openRender
 *
 * File: shader.h
 *
 * Description:
 *   This file defines the interface for shader.
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
//  File				:	shader.h
//  Classes				:	CShader
//							CShaderInstance
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef SHADER_H
#define SHADER_H

#include "common/containers.h"
#include "common/global.h"
#include "common/os.h"
#include "config.h" // OPENRENDER_HAVE_LLVM
#include "common/fileResource.h"
#include "common/refCounter.h"
#include "stateHandles.h" // CShaderGroupRefTarget: the base of CShaderInstance (Shading -> Graphics State)
#include "common/rendererc.h"
#include "common/riVocabulary.h"
#include "ri_config.h"
#include "common/variable.h"
#include "xform.h"

// Forward references
class CShader;
class CShaderInstance;
class CMemPage;
class CShadingContext;
class CAttributes;
class CDSO;
class IShaderBackend;
class CShaderPayload;

// Meanings of the accessor field of TArgument
const unsigned int SL_IMMEDIATE_OPERAND = 0; // Constants
const unsigned int SL_GLOBAL_OPERAND = 1;    // Global variable references
const unsigned int SL_VARYING_OPERAND = 2;   // Local variable references (this includes parameters)

// This structure holds an argument for a code
typedef struct {
        unsigned char numItems;     // The number of items to step for this variable (0 for constants,parameters, cardinality for variables,globals)
        unsigned char accessor;     // The type of the variable (SL_IMMEDIATE,SL_PARAMETER,SL_VARIABLE,SL_GLOBAL)
        unsigned char bytesPerItem; // The number of bytes per item
        unsigned char varyingStep;  // Used to store whether operand is uniform (step 0) or varying (step) ... important PADDING also
        unsigned int index;         // The index of the variable in the corresponding entry array
} TArgument;

// This structure holds a code
// In the future, we may want to make this a class
//    and have the shading language opcodes subclass from it
typedef struct {
        int opcode;                 // The opcode
        unsigned char uniform;      // TRUE if all the arguments are uniform
        unsigned char numArguments; // The number of stack arguments
        unsigned short callSite;    // The call-site ordinal of a stochastic instruction (spec 039), 0 otherwise
        TArgument *arguments;       // The array of arguments
        CDSO *dso;                  // If this is a DSO, points to the DSO function
} TCode;

// Shader types
const unsigned int SL_SURFACE = 0;
const unsigned int SL_LIGHTSOURCE = 1;
const unsigned int SL_DISPLACEMENT = 2;
const unsigned int SL_ATMOSPHERE = 3;
const unsigned int SL_IMAGER = 4;

// Shader flags
const unsigned int SHADERFLAGS_NONAMBIENT = 1;
const unsigned int SHADERFLAGS_NONDIFFUSE = 2;
const unsigned int SHADERFLAGS_NONSPECULAR = 4;

///////////////////////////////////////////////////////////////////////
// Class				:	CShaderData
// Description			:	This class records extra data for a shader instance
// Comments				:
class CShaderData {
    public:
        CShaderData() {}
        virtual ~CShaderData() {}
};

///////////////////////////////////////////////////////////////////////
// Class				:	CLightShaderData
// Description			:	This class records data for a light
// Comments				:
class CLightShaderData : public CShaderData {
    public:
        int nonDiffuseIndex;
        int nonDiffuseStep;
        int nonSpecularIndex;
        int nonSpecularStep;

        CLightShaderData() {
            nonDiffuseIndex = -1;
            nonSpecularIndex = -1;
            nonDiffuseStep = 0;
            nonSpecularStep = 0;
        }
};

///////////////////////////////////////////////////////////////////////
// Class				:	CShader
// Description			:	The backend-neutral description of a loaded shader (spec 033, FR-007)
// Comments				:	What a shader of any format has: its name (CFileResource), type, parameters, used parameters, flags and
//							light data. The backend that loaded it and the payload only that backend understands (the bytecode, the
//							constant tables, the strings and the frame layout, or the JIT entry points: shaderPayload.h) are the two
//							last members; the payload is owned and deleted with the description.
class CShader : public CFileResource {
    public:
        CShader(const char *);
        ~CShader();

        int type; // Type of the shader

        CVariable *parameters; // List of parameters

        int usedParameters;

        unsigned int flags; // shadows of parent data (to support hcShaders)
        CShaderData *data;  // Additional data (owned by CShader)

        const IShaderBackend *backend; // The backend that loaded this shader, and runs its instances
        CShaderPayload *payload;       // What only that backend understands (owned)

        // Work out the light flags and the CLightShaderData indices from the parameters. A load-time helper of the RSL loaders: numGlobals
        // is the frame layout's count of global parameters (CRslPayload::numGlobals)
        void analyse(int numGlobals);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CShaderInstance
// Description			:	This class encapsulates an instance of a shader
// Comments				:
class CShaderInstance : public CShaderGroupRefTarget {
    public:
        CShaderInstance(CAttributes *, CXform *);
        virtual ~CShaderInstance();

        virtual void illuminate(CShadingContext *, float **) = 0;
        virtual void setParameters(int, const char **, const void **) = 0;
        virtual int getParameter(const char *, void *, CVariable **, int *) = 0;
        // The stored type of a parameter and the number of floats it holds in all (array items included), without
        // writing anything: the services decide a TypeMismatch (spec 026, FR-026) from it before getParameter() writes
        // the destination. The default is "unknown, no check" (the hard-coded lights); the programmable instance
        // answers from its parameters, with the loop getParameter() uses.
        virtual bool getParameterShape(const char *name, EVariableType &type, int &numFloats) const {
            (void)name;
            (void)type;
            (void)numFloats;
            return false;
        }
        virtual void execute(CShadingContext *, float **) = 0;
        virtual unsigned int requiredParameters() = 0;
        virtual const char *getName() = 0;
        virtual float **prepare(CMemPage *&, float **, int) = 0;
        // The description this instance was made from: the dispatcher's way to the backend that runs it
        virtual const CShader *shaderDescription() const = 0;

        void createCategories();

        CVariable *parameters; // The list of parameter (cloned from the parent)
        CXform *xform;
        int *categories; // Categories for light shaders
        unsigned int flags;
        CShaderData *data; // Extra non-instance data
                           // simply referenced here (not owned)

        // The instance's own identity for the sampler (spec 039): the RIB light handle when the
        // instance has one, otherwise its number in the order the scene created shader instances.
        // It is part of the shader key of its draws, so two instances of one shader draw from
        // streams of their own and neither depends on the other's draws or on whether the
        // other exists. 0 until the renderer core sets it.
        uint64_t instanceKey;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CProgrammableShaderInstance
// Description			:	This class encapsulates an instance of a programmable shader
// Comments				:
class CProgrammableShaderInstance : public CShaderInstance {

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CAllocatedString
        // Description			:	We use this class to keep track of the allocated strings for parameters
        // Comments				:
        class CAllocatedString {
            public:
                char *string;
                CAllocatedString *next;
        };

    public:
        CProgrammableShaderInstance(CShader *, CAttributes *, CXform *);
        virtual ~CProgrammableShaderInstance();

        void illuminate(CShadingContext *, float **);
        void setParameters(int, const char **, const void **);
        int getParameter(const char *, void *, CVariable **, int *); // Get the value of a parameter
        bool getParameterShape(const char *, EVariableType &, int &) const;
        void execute(CShadingContext *, float **);                   // Execute the shader
        unsigned int requiredParameters();
        const char *getName();
        float **prepare(CMemPage *&, float **, int);
        const CShader *shaderDescription() const { return parent; }

        CAllocatedString *strings; // The strings we allocated for parameters
        CShader *parent;           // The parent shader
    private:
        int setParameter(const char *, const void *);
};


namespace libshader {

    // The bind-time init of a shader instance's parameter defaults (spec 033, #134). ShadingSystem::bind() runs the backend's init
    // itself, inside arenaBegin/arenaEnd(globalArena); this is the same call, for a caller that holds a bare instance (the
    // ShadingSystem_BindUnknownSpace test). Runs the init of the backend that loaded the shader (its description's backend).
    void initialiseInstance(CProgrammableShaderInstance &instance);

    // The interpreter's body (rsloInit.cpp); the JIT's is the .slo backend's (sloBackend.cpp)
    void initialiseRsloInstance(CProgrammableShaderInstance &instance);

} // namespace libshader

#endif
