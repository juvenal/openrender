/**
 * Project: openRender
 *
 * File: riVocabulary.h
 *
 * Description:
 *   The RenderMan Interface vocabulary every domain may use: the Rt* types,
 *   the RI_* tokens, the standard basis matrices and filter functions, and
 *   the RIE_* error codes.
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
//  File				:	riVocabulary.h
//  Classes				:	-
//  Description			:	The RI vocabulary, without the Ri* procedures
//
//  The public ri.h stays the installed API and is not changed; its types,
//  macros and token declarations are repeated here character for character,
//  so a translation unit may include both. The tokens are defined once, in
//  riTokens.cpp; the basis matrices and filters in rslConstants.cpp.
//  Common_VocabularyConsistency keeps the token lists in step.
//
////////////////////////////////////////////////////////////////////////
#ifndef RI_VOCABULARY_H
#define RI_VOCABULARY_H

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////
//
//	Type definitions
//
////////////////////////////////////////////////////////////////////////
typedef short RtBoolean;
typedef int RtInt;
typedef float RtFloat;
typedef const char *RtToken;
typedef void RtVoid;
typedef const char *RtString;
typedef const void *RtPointer;
typedef RtFloat RtColor[3];
typedef RtFloat RtPoint[3];
typedef RtFloat RtMatrix[4][4];
typedef RtFloat RtBasis[4][4];
typedef RtFloat RtBound[6];
typedef RtPointer RtObjectHandle;
typedef RtPointer RtLightHandle;
typedef RtPointer RtContextHandle;
typedef RtString RtArchiveHandle;
typedef RtString RtShaderHandle;
typedef RtFloat (*RtFilterFunc)(RtFloat, RtFloat, RtFloat, RtFloat);
typedef RtFloat (*RtStepFilterFunc)(RtFloat, RtFloat, RtFloat);
typedef RtVoid (*RtErrorHandler)(RtInt code, RtInt severity, const char *msg);
typedef RtVoid (*RtFunc)(const char *);
typedef RtVoid (*RtArchiveCallback)(const char *, ...);

typedef void *(*RtDisplayParameterFunction)(const char *, int, int);
typedef void *(*RtDisplayStartFunction)(const char *, int, int, int, const char *, RtDisplayParameterFunction);
typedef int (*RtDisplayDataFunction)(void *, int, int, int, int, float *);
typedef int (*RtDisplayRawDataFunction)(void *, int, int, int, int, void *);
typedef void (*RtDisplayFinishFunction)(void *);

#define RI_FALSE 0
#define RI_TRUE 1
#define RI_INFINITY (RtFloat)1.0e38
#define RI_EPSILON (RtFloat)1.0e-10

#define RI_NULL ((RtToken)0)

////////////////////////////////////////////////////////////////////////
//
//	Standard tokens
//
////////////////////////////////////////////////////////////////////////
extern RtToken RI_FRAMEBUFFER,
    RI_FILE;

extern RtToken RI_RGB,
    RI_RGBA,
    RI_RGBZ,
    RI_RGBAZ,
    RI_A,
    RI_Z,
    RI_AZ;

extern RtToken RI_PERSPECTIVE,
    RI_ORTHOGRAPHIC;

extern RtToken RI_HIDDEN,
    RI_PAINT;

extern RtToken RI_CONSTANT,
    RI_SMOOTH;

extern RtToken RI_FLATNESS,
    RI_FOV;

extern RtToken RI_AMBIENTLIGHT,
    RI_POINTLIGHT,
    RI_DISTANTLIGHT,
    RI_SPOTLIGHT;

extern RtToken RI_INTENSITY,
    RI_LIGHTCOLOR,
    RI_FROM,
    RI_TO,
    RI_CONEANGLE,
    RI_CONEDELTAANGLE,
    RI_BEAMDISTRIBUTION;

extern RtToken RI_MATTE,
    RI_METAL,
    RI_SHINYMETAL,
    RI_PLASTIC,
    RI_PAINTEDPLASTIC;

extern RtToken RI_KA,
    RI_KD,
    RI_KS,
    RI_ROUGHNESS,
    RI_KR,
    RI_TEXTURENAME,
    RI_SPECULARCOLOR;

extern RtToken RI_DEPTHCUE,
    RI_FOG,
    RI_BUMPY;

extern RtToken RI_MINDISTANCE,
    RI_BACKGROUND,
    RI_DISTANCE,
    RI_AMPLITUDE;

extern RtToken RI_RASTER,
    RI_SCREEN,
    RI_CAMERA,
    RI_WORLD,
    RI_OBJECT;

extern RtToken RI_INSIDE,
    RI_OUTSIDE,
    RI_LH,
    RI_RH;

extern RtToken RI_P,
    RI_PZ,
    RI_PW,
    RI_N,
    RI_NP,
    RI_CS,
    RI_OS,
    RI_S,
    RI_T,
    RI_ST;

extern RtToken RI_BILINEAR,
    RI_BICUBIC;

extern RtToken RI_PRIMITIVE,
    RI_INTERSECTION,
    RI_UNION,
    RI_DIFFERENCE;

extern RtToken RI_PERIODIC,
    RI_NOWRAP,
    RI_NONPERIODIC,
    RI_CLAMP,
    RI_BLACK;

extern RtToken RI_IGNORE,
    RI_PRINT,
    RI_ABORT,
    RI_HANDLER;

extern RtToken RI_ORIGIN,
    RI_IDENTIFIER,
    RI_NAME;

extern RtToken RI_COMMENT,
    RI_STRUCTURE,
    RI_VERBATIM;

extern RtToken RI_LINEAR,
    RI_CUBIC,
    RI_WIDTH,
    RI_CONSTANTWIDTH;

extern RtToken RI_CATMULLCLARK,
    RI_LOOP,
    RI_HOLE,
    RI_CREASE,
    RI_CORNER,
    RI_INTERPOLATEBOUNDARY,
    RI_FACEVARYINGINTERPOLATEBOUNDARY,
    RI_FACEVARYINGPROPAGATECORNERS,
    RI_CREASEMETHOD;

extern RtToken RI_CURRENT,
    RI_SHADER;

extern RtToken RI_NDC,
    RI_EYE;

////////////////////////////////////////////////////////////////////////
//
//	Recognized filter types
//
////////////////////////////////////////////////////////////////////////
extern RtToken RI_BOXFILTER,
    RI_TRIANGLEFILTER,
    RI_GAUSSIANFILTER,
    RI_SINCFILTER,
    RI_CATMULLROMFILTER,
    RI_BLACKMANHARRISFILTER,
    RI_MITCHELLFILTER,
    RI_BESSELFILTER,
    RI_DISKFILTER,
    RI_CUSTOM;

extern RtToken RI_MIN,
    RI_MAX,
    RI_AVERAGE,
    RI_ZMIN,
    RI_ZMAX;

////////////////////////////////////////////////////////////////////////
//
//	Non-Standard options/attributes entry points
//
////////////////////////////////////////////////////////////////////////
extern RtToken RI_SEARCHPATH,
    RI_LIMITS,
    RI_SHADOW,
    RI_RENDER,
    RI_HIDER,
    RI_STATISTICS,
    RI_COMPRESSION,
    RI_RIB,
    RI_SHUTTER,
    RI_USER,
    RI_TRIMCURVE,
    RI_SENSE,
    RI_TEXTURECACHE;

extern RtToken RI_DICE,
    RI_HINT,
    RI_TEXTURE,
    RI_VISIBILITY,
    RI_SHADE,
    RI_DISPLACEMENT,
    RI_DISPLACEMENTBOUND,
    RI_IRRADIANCE,
    RI_CULL;

////////////////////////////////////////////////////////////////////////
//
//	Non-Standard attributes
//
////////////////////////////////////////////////////////////////////////

// The geometric approximation types
extern RtToken RI_NORMALDEVIATION,
    RI_POINTDEVIATION;

// Dice attributes
extern RtToken RI_MINSUBDIVISION,
    RI_MAXSUBDIVISION,
    RI_NUMPROBES,
    RI_MINSPLITS,
    RI_BOUNDEXPAND,
    RI_BINARY,
    RI_RASTERORIENT;

// Displacementbound attributes
extern RtToken RI_BOUND,
    RI_SPHERE,
    RI_COORDINATESYSYTEM;

// Solid (CSG) attributes
extern RtToken RI_SOLID,
    RI_TESSELLATIONTOLERANCE;

// Visibility attributes
extern RtToken RI_PHOTON,
    RI_ENVIRONMENT,
    RI_DIFFUSE,
    RI_SPECULAR,
    RI_TRACE,
    RI_TRANSMISSION,
    RI_CAUSTICS;

// Blobby implicit surface attribute and option (spec 015)
extern RtToken RI_BLOBBY,
    RI_BLOBBYTOLERANCE,
    RI_BLOBBYOPCODEORDER;

// Shade attributes
extern RtToken RI_SHADERFORMAT,
    RI_DIFFUSEHITMODE,
    RI_SPECULARHITMODE,
    RI_TRANSMISSIONHITMODE,
    RI_CAMERAHITMODE;

// Photon map attributes
extern RtToken RI_GLOBALMAP,
    RI_CAUSTICMAP,
    RI_ESTIMATOR,
    RI_MAXDISTANCE,
    RI_SHADINGMODEL,
    RI_ILLUMINATEFRONT,
    RI_IOR,
    RI_IORRANGE;

// Trace attributes
extern RtToken RI_BIAS,
    RI_DISPLACEMENTS,
    RI_MAXDIFFUSEDEPTH,
    RI_MAXSPECULARDEPTH,
    RI_SAMPLEMOTION;

// Motionfactor attribute
extern RtToken RI_MOTIONFACTOR;

// Cull attributes
extern RtToken RI_BACKFACING;

////////////////////////////////////////////////////////////////////////
//
//	Non-Standard options
//
////////////////////////////////////////////////////////////////////////

// Searchpath options
extern RtToken RI_ARCHIVE,
    RI_PROCEDURAL,
    RI_RESOURCE,
    RI_DISPLAY,
    RI_GEOMETRY;

// Limits options
extern RtToken RI_BUCKETSIZE,
    RI_METABUCKETS,
    RI_GRIDSIZE,
    RI_MAXRECURSION,
    RI_TEXTUREMEMORY,
    RI_BRICKMEMORY,
    RI_EYESPLITS,
    RI_NUMTHREADS,
    RI_THREADSTRIDE,
    RI_GEOCACHEMEMORY,
    RI_OTHRESHOLD,
    RI_ZTHRESHOLD;

// Trace options
extern RtToken RI_MAXDEPTH;

// Statistics options
extern RtToken RI_ENDOFFRAME,
    RI_FILELOG,
    RI_PROGRESS;

// Texturecache options (spec 020-runtime-tile-paging)
extern RtToken RI_TEXTURECACHEENABLE,
    RI_TEXTURECACHEDIRECTORY;

// Irradiance options
extern RtToken RI_HANDLE,
    RI_FILEMODE,
    RI_MAXERROR,
    RI_MAXPIXELDIST,
    RI_MINSAMPLEDISTANCE,
    RI_MAXSAMPLEDISTANCE;

// Hider options
extern RtToken RI_JITTER,
    RI_FALSECOLOR,
    RI_EMIT,
    RI_SAMPLESPECTRUM,
    RI_DEPTHFILTER,
    RI_RADIANCECACHE;

// IO options
extern RtToken RI_MASKRESOLUTION,
    RI_MASKPRINTF,
    RI_MASKLOG,
    RI_MASKPROGRESS,
    RI_MASKSTATS,
    RI_INHERITATTRIBUTES;

// Shutter options
extern RtToken RI_OFFSET;

// PixelFilter options
extern RtToken RI_FILTER,
    RI_PIXELFILTER,
    RI_MODE,
    RI_CONTINUOUS,
    RI_PRECOMPUTED;

// Misc junk
extern RtToken RI_DEFAULT,
    RI_DEFAULTLIGHT,
    RI_DEFAULTSURFACE;

extern RtBasis RiBezierBasis,
    RiBSplineBasis,
    RiCatmullRomBasis,
    RiHermiteBasis,
    RiPowerBasis,
    RiLinearBasis;

#define RI_BEZIERSTEP ((RtInt)3)
#define RI_BSPLINESTEP ((RtInt)1)
#define RI_CATMULLROMSTEP ((RtInt)1)
#define RI_HERMITESTEP ((RtInt)2)
#define RI_POWERSTEP ((RtInt)4)

// The standard filters defined in rslConstants.cpp
extern RtFloat RiGaussianFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth),
    RiBoxFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth),
    RiTriangleFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth),
    RiMitchellFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth),
    RiCatmullRomFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth),
    RiBlackmanHarrisFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth),
    RiSincFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth),
    RiBesselFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth);

extern RtFloat RiGaussianStepFilter(RtFloat t, RtFloat edge, RtFloat width),
    RiBoxStepFilter(RtFloat t, RtFloat edge, RtFloat width),
    RiTriangleStepFilter(RtFloat t, RtFloat edge, RtFloat width),
    RiMitchellStepFilter(RtFloat t, RtFloat edge, RtFloat width),
    RiCatmullRomStepFilter(RtFloat t, RtFloat edge, RtFloat width);

/*
    Error codes

     1 - 10     System and File errors
    11 - 20     Program Limitations
    21 - 40     State Errors
    41 - 60     Parameter and Protocol Errors
    61 - 80     Execution Errors
*/

#define RIE_NOERROR 0

#define RIE_NOMEM 1   /* Out of memory */
#define RIE_SYSTEM 2  /* Miscellaneous system error */
#define RIE_NOFILE 3  /* File nonexistant */
#define RIE_BADFILE 4 /* Bad file format */
#define RIE_VERSION 5 /* File version mismatch */

#define RIE_INCAPABLE 11   /* Optional RI feature */
#define RIE_OPTIONAL 11    /* Optional RI feature */
#define RIE_UNIMPLEMENT 12 /* Unimplemented feature */
#define RIE_LIMIT 13       /* Arbitrary program limit */
#define RIE_BUG 14         /* Probably a bug in renderer */

#define RIE_NOTSTARTED 23 /* RiBegin not called */
#define RIE_NESTING 24    /* Bad begin-end nesting */
#define RIE_NOTOPTIONS 25 /* Invalid state for options */
#define RIE_NOTATTRIBS 26 /* Invalid state for attributes */
#define RIE_NOTPRIMS 27   /* Invalid state for primitives */
#define RIE_ILLSTATE 28   /* Other invalid state */
#define RIE_BADMOTION 29  /* Badly formed motion block */
#define RIE_BADSOLID 30   /* Badly formed solid block */

#define RIE_BADTOKEN 41    /* Invalid token for request */
#define RIE_RANGE 42       /* Parameter out of range */
#define RIE_CONSISTENCY 43 /* Parameters inconsistent */
#define RIE_BADHANDLE 44   /* Bad object/light handle */
#define RIE_NOSHADER 45    /* Can't load requested shader */
#define RIE_MISSINGDATA 46 /* Required parameters not provided */
#define RIE_SYNTAX 47      /* Declare type syntax error */

#define RIE_MATH 61 /* Zerodivide, noninvert matrix, etc. */

#define RIE_LOG 62
#define RIE_PROGRESS 63
#define RIE_PRINTF 64
#define RIE_SCRIPT 65
#define RIE_BEGIN 66
#define RIE_END 67
#define RIE_STATS 68

/* Error severity levels */
#define RIE_INFO 0    /* Rendering stats & other info */
#define RIE_WARNING 1 /* Something seems wrong, maybe okay */
#define RIE_ERROR 2   /* Problem.  Results may be wrong */
#define RIE_SEVERE 3  /* So bad you should probably abort */

#ifdef __cplusplus
}
#endif

#endif // RI_VOCABULARY_H
