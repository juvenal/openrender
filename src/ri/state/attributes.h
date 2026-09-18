/**
 * Project: openRender
 *
 * File: attributes.h
 *
 * Description:
 *   This file defines the interface for attributes.
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
//  File				:	attributes.h
//  Classes				:	CAttributes
//  Description			:	Holds the attributes attached to an object
//
////////////////////////////////////////////////////////////////////////
#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

#include "common/algebra.h"    // Matrix - vector stuff
#include "common/containers.h" // Misc data structures
#include "common/global.h"     // The global header file
#include "shader.h"            // Shader stuff
#include "userAttributes.h"    // Attribute dictionary stuff
#include "xform.h"             // Transformations

class CPhotonMap;

// Constant definitions for the flag field of the attributes
const unsigned int ATTRIBUTES_FLAGS_INSIDE = 1;                      // Flip the orientation
const unsigned int ATTRIBUTES_FLAGS_CUSTOM_ST = 1 << 1;              // Explicit surface st
const unsigned int ATTRIBUTES_FLAGS_MATTE = 1 << 2;                  // The object is matte
const unsigned int ATTRIBUTES_FLAGS_CUSTOM_BOUND = 1 << 3;           // The bound of the object is explicitly set
const unsigned int ATTRIBUTES_FLAGS_BINARY_DICE = 1 << 4;            // Use binary dicing on the surface
const unsigned int ATTRIBUTES_FLAGS_PRIMARY_VISIBLE = 1 << 6;        // The primitive is visible to the primary rays
const unsigned int ATTRIBUTES_FLAGS_PHOTON_VISIBLE = 1 << 7;         // The primitive is visible to the photon rays
const unsigned int ATTRIBUTES_FLAGS_SPECULAR_VISIBLE = 1 << 9;       // The primitive is visible to the gather/trace/environment rays
const unsigned int ATTRIBUTES_FLAGS_DIFFUSE_VISIBLE = 1 << 10;       // The primitive is visible to the gather/occlusion/diffuse rays
const unsigned int ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE = 1 << 11;  // The primitive is visible to the transmission/shadow rays
const unsigned int ATTRIBUTES_FLAGS_DISPLACEMENTS = 1 << 14;         // The primitive is visible to the photon rays
const unsigned int ATTRIBUTES_FLAGS_ILLUMINATE_FRONT_ONLY = 1 << 17; // During the photon tracing, only photons that hit the front will be traced
const unsigned int ATTRIBUTES_FLAGS_LOD = 1 << 18;                   // A detail range has been specified
const unsigned int ATTRIBUTES_FLAGS_DISCARD_GEOMETRY = 1 << 19;      // Discard geometry calls
const unsigned int ATTRIBUTES_FLAGS_DISCARD_ALL = 1 << 20;           // Discard all calls
const unsigned int ATTRIBUTES_FLAGS_NONRASTERORIENT_DICE = 1 << 21;  // Perform non raster-oriented dicing
const unsigned int ATTRIBUTES_FLAGS_SHADE_HIDDEN = 1 << 22;          // Shade even if occluded
const unsigned int ATTRIBUTES_FLAGS_SHADE_BACKFACE = 1 << 23;        // Shade even if backfacing
const unsigned int ATTRIBUTES_FLAGS_DOUBLE_SIDED = 1 << 24;          // The surface is double sided
const unsigned int ATTRIBUTES_FLAGS_SAMPLEMOTION = 1 << 25;          // Sample the time in tracing rays
const unsigned int ATTRIBUTES_FLAGS_SOLID_FRAGMENT = 1 << 26;        // Attribute clone belongs to a resolved CSG Boundary Fragment (spec 013)

// The minimum shading rate
const float ATTRIBUTES_MIN_SHADINGRATE = C_EPSILON;

///////////////////////////////////////////////////////////////////////
// Class				:	CActiveLight
// Description			:	Holds an active light source instance
// Comments				:
class CActiveLight {
    public:
        CProgrammableShaderInstance *light;
        CActiveLight *next;
};

// The shading model
typedef enum {
    SM_MATTE,
    SM_TRANSLUCENT,
    SM_CHROME,
    SM_GLASS,
    SM_WATER,
    SM_DIELECTRIC,
    SM_TRANSPARENT
} EShadingModel;

// The "trimcurve"/"sense" attribute: which side of the trim loops is discarded
typedef enum {
    TS_INSIDE, // Region enclosed by trim loops is discarded (default)
    TS_OUTSIDE // Region enclosed by trim loops is kept; everything outside is discarded
} ETrimSense;

///////////////////////////////////////////////////////////////////////
// Class				:	CTrimLoop
// Description			:	One or more homogeneous rational B-spline curves
//							in (u,v,w) parameter space, connected head-to-tail
//							into a single closed boundary (RiTrimCurve).
// Comments				:
class CTrimLoop {
    public:
        int curveCount;   // Number of curves composing this loop (ncurves[i])
        int *order;       // B-spline order per curve, order[curveCount]
        double *knot;     // Concatenated knot vectors, one run per curve
        double *min, *max; // Parameter-range clamp per curve, min[curveCount]/max[curveCount]
        int *n;           // Control-point count per curve, n[curveCount]
        double *u, *v, *w; // Concatenated homogeneous control points (u,v,w) across all curves
};

///////////////////////////////////////////////////////////////////////
// Class				:	CAttributes
// Description			:	This class encapsulates the attributes attached
//							to a surface. Surfaces that have the same set of
//							attributes share a common clone to avoid unnecessary
//							memory allocation.
// Comments				:
class CAttributes : public CRefCounter {
    public:
        CAttributes();
        CAttributes(const CAttributes *);
        virtual ~CAttributes();

        void addLight(CShaderInstance *); // Add or remove a lightsource from the environment
        void removeLight(CShaderInstance *);
        void checkParameters();                 // Re-compute the required shader parameters
        CVariable *findParameter(const char *); // Find a shader parameter
        void restore(const CAttributes *other, int shading, int geometrymodification, int geometrydefinition, int /*hiding*/);
        int find(const char *name, const char *category, EVariableType &type, const void *&value, int &intValue, float &floatValue) const;

        CAttributes *next;           // points to the next attribute if there's motion blur

        CShaderInstance *surface;    // Shaders attached to the primitive
        CShaderInstance *displacement;
        CShaderInstance *atmosphere;
        CShaderInstance *interior;
        CShaderInstance *exterior;
        unsigned int usedParameters; // The set of used parameters that the shaders need

        vector surfaceColor;         // Default surface color and opacity
        vector surfaceOpacity;

        float s[4], t[4];            // The texture coordinates

        vector bmin, bmax;           // The custom bounding box if given
        float bexpand;               // Bounding box expansion percentage

        matrix uBasis, vBasis;       // The basis for bicubic patches
        int uStep, vStep;            // The step sizes for bicubic patches

        unsigned int flags;          // Attribute flags

        float maxDisplacement;       // Maximum amount of displacement in camera system
        char *maxDisplacementSpace;  // The current space in which the maximum displacement is given

        CActiveLight *lightSources;  // The list of active light sources

        float shadingRate;           // Shading rate for this primitive
        float motionFactor;          // Amount to increase shading rate when motion blurring

        char *name;                  // The name of the object if any

        int numUProbes, numVProbes;  // The samples to gather when estimating the extend of a patch
        int minSplits;               // The minimum number of splits
        float rasterExpand;          // The expansion coefficient during the sampling
        float bias;                  // The bias amount expressed in the camera coordinates

        float tessellationTolerance; // Tessellation tolerance for solid (CSG) boundary resolution

        char transmissionHitMode;    // Either: 'p' = Look at the primitive   or   's' = Execute the shader
        char specularHitMode;        // Either: 'p' = Look at the primitive   or   's' = Execute the shader
        char diffuseHitMode;         // Either: 'p' = Look at the primitive   or   's' = Execute the shader
        char cameraHitMode;          // Either: 'p' = Look at the primitive   or   's' = Execute the shader

        int emit;                    // The number of photons to emit from this light source
        float relativeEmit;          // The relative emittance

        EShadingModel shadingModel;  // The surface shading model

        char *shaderFormat;            // "slo" or "rslo" shader format preference

        // Blobby implicit-surface fidelity (spec 015, FR-025). Negative
        // means "never set", in which case the cell size is derived from
        // the primitive's own field extent, so a scene that never sets it
        // still renders smoothly at typical framing. Zero is deliberately
        // *not* the unset marker: an author who writes 0 explicitly gets a
        // diagnostic, which a zero default would make impossible to tell
        // apart from silence.
        float blobbyTolerance;

        char *globalMapName;              // The name of the global photon map
        char *causticMapName;             // The name of the caustic photon map
        CPhotonMap *globalMap;            // The global photon map
        CPhotonMap *causticMap;           // The caustic photon map
        char *irradianceHandle;           // The irradiance cache
        char *irradianceHandleMode;       // The irradiance cache mode
        float irradianceMaxError;         // The error threshold for the irradiance cache
        float irradianceMaxPixelDistance; // The maximum pixel distance between the samples for trradiance caching
        int photonEstimator;              // The total number of photons to use to estimate irradiance
        float photonIor[2];               // Index of refraction range used for the dielectic shading model
        int maxDiffuseDepth;              // The maximum number of diffuse bounces before going to the photon map
        int maxSpecularDepth;             // The maximum number of specular bounces before giving up
        int shootStep;                    // The step size for shooting rays for the attached object

        float lodRange[4]; // LOD variables
        float lodSize;
        float lodImportance;

        int numPendingTrimLoops;     // Number of loops in pendingTrimLoops (0 if none)
        CTrimLoop *pendingTrimLoops; // Pending TrimCurve loops (RiTrimCurve) for the next NuPatch in scope; heap-owned, nullptr if none
        ETrimSense trimSense;        // Backing storage for Attribute "trimcurve" "sense" (default TS_INSIDE)

        CUserAttributeDictionary userAttributes; // Duh.

        static char findHitMode(const char *mode);
        static const char *findHitMode(char mode);
        static EShadingModel findShadingModel(const char *name);
        static const char *findShadingModel(EShadingModel model);
        static char *setShaderFormat(const char *format);
};

// Interior/Exterior shader selection for a primary camera-ray hit (spec
// 013-solid-csg-operations, FR-010/FR-011/FR-020). Returns NULL when the
// feature does not apply -- either the hit object is not a Boundary
// Fragment of a resolved CSG solid, or no shader was assigned for the hit
// side -- in which case ordinary surface/atmosphere shading proceeds
// untouched (FR-012). Inline: called from both the `ri` and
// `libshader_shading` link targets.
inline CShaderInstance *selectVolumeShader(const CAttributes *attr, bool isSolidFragment, bool isExterior) {
    if (!isSolidFragment)
        return NULL;

    return isExterior ? attr->exterior : attr->interior;
}

#endif
