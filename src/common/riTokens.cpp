/**
 * Project: openRender
 *
 * File: riTokens.cpp
 *
 * Description:
 *   The RI_* token definitions, shared by every library through libRIBCommon.
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
//  File				:	riTokens.cpp
//  Classes				:	-
//  Description			:	The RI token definitions (declared in riVocabulary.h
//						and, for the public API, in ri.h)
//
////////////////////////////////////////////////////////////////////////
#include "riVocabulary.h"

//////////////////////////////////////////////////////////////////
// Token definitions
RtToken RI_FRAMEBUFFER = "framebuffer";
RtToken RI_FILE = "file";

RtToken RI_RGB = "rgb";
RtToken RI_RGBA = "rgba";
RtToken RI_RGBZ = "rgbz";
RtToken RI_RGBAZ = "rgbaz";
RtToken RI_A = "a";
RtToken RI_Z = "z";
RtToken RI_AZ = "az";

RtToken RI_PERSPECTIVE = "perspective";
RtToken RI_ORTHOGRAPHIC = "orthographic";

RtToken RI_HIDDEN = "hidden";
RtToken RI_PAINT = "paint";

RtToken RI_CONSTANT = "constant";
RtToken RI_SMOOTH = "smooth";

RtToken RI_FLATNESS = "flatness";
RtToken RI_FOV = "fov";

RtToken RI_AMBIENTLIGHT = "ambientlight";
RtToken RI_POINTLIGHT = "pointlight";
RtToken RI_DISTANTLIGHT = "distantlight";
RtToken RI_SPOTLIGHT = "spotlight";

RtToken RI_INTENSITY = "intensity";
RtToken RI_LIGHTCOLOR = "lightcolor";
RtToken RI_FROM = "from";
RtToken RI_TO = "to";
RtToken RI_CONEANGLE = "coneangle";
RtToken RI_CONEDELTAANGLE = "conedeltaangle";
RtToken RI_BEAMDISTRIBUTION = "beamdistribution";

RtToken RI_MATTE = "matte";
RtToken RI_METAL = "metal";
RtToken RI_SHINYMETAL = "shinymetal";
RtToken RI_PLASTIC = "plastic";
RtToken RI_PAINTEDPLASTIC = "paintedplastic";

RtToken RI_KA = "Ka";
RtToken RI_KD = "Kd";
RtToken RI_KS = "Ks";
RtToken RI_ROUGHNESS = "roughness";
RtToken RI_KR = "Kr";
RtToken RI_TEXTURENAME = "texturename";
RtToken RI_SPECULARCOLOR = "specularcolor";

RtToken RI_DEPTHCUE = "depthcue";
RtToken RI_FOG = "fog";
RtToken RI_BUMPY = "bumpy";

RtToken RI_MINDISTANCE = "mindistance";
RtToken RI_BACKGROUND = "background";
RtToken RI_DISTANCE = "distance";
RtToken RI_AMPLITUDE = "amplitude";

RtToken RI_INSIDE = "inside";
RtToken RI_OUTSIDE = "outside";
RtToken RI_LH = "lh";
RtToken RI_RH = "rh";

RtToken RI_P = "P";
RtToken RI_PZ = "Pz";
RtToken RI_PW = "Pw";
RtToken RI_N = "N";
RtToken RI_NP = "Np";
RtToken RI_CS = "Cs";
RtToken RI_OS = "Os";
RtToken RI_S = "s";
RtToken RI_T = "t";
RtToken RI_ST = "st";

RtToken RI_BILINEAR = "bilinear";
RtToken RI_BICUBIC = "bicubic";

RtToken RI_PRIMITIVE = "primitive";
RtToken RI_INTERSECTION = "intersection";
RtToken RI_UNION = "union";
RtToken RI_DIFFERENCE = "difference";

RtToken RI_PERIODIC = "periodic";
RtToken RI_NOWRAP = "nowrap";
RtToken RI_NONPERIODIC = "nonperiodic";
RtToken RI_CLAMP = "clamp";
RtToken RI_BLACK = "black";

RtToken RI_IGNORE = "ignore";
RtToken RI_PRINT = "print";
RtToken RI_ABORT = "abort";
RtToken RI_HANDLER = "handler";

RtToken RI_ORIGIN = "origin";
RtToken RI_IDENTIFIER = "identifier";
RtToken RI_NAME = "name";

RtToken RI_COMMENT = "comment";
RtToken RI_STRUCTURE = "structure";
RtToken RI_VERBATIM = "verbatim";

RtToken RI_LINEAR = "linear";
RtToken RI_CUBIC = "cubic";
RtToken RI_WIDTH = "width";
RtToken RI_CONSTANTWIDTH = "constantwidth";

RtToken RI_CATMULLCLARK = "catmull-clark";
RtToken RI_LOOP = "loop";
RtToken RI_HOLE = "hole";
RtToken RI_CREASE = "crease";
RtToken RI_CORNER = "corner";
RtToken RI_INTERPOLATEBOUNDARY = "interpolateboundary";
RtToken RI_FACEVARYINGINTERPOLATEBOUNDARY = "facevaryinginterpolateboundary";
RtToken RI_FACEVARYINGPROPAGATECORNERS = "facevaryingpropagatecorners";
RtToken RI_CREASEMETHOD = "creasemethod";

RtToken RI_CURRENT = "current";
RtToken RI_WORLD = "world";
RtToken RI_OBJECT = "object";
RtToken RI_SHADER = "shader";
RtToken RI_RASTER = "raster";
RtToken RI_NDC = "ndc";
RtToken RI_SCREEN = "screen";
RtToken RI_CAMERA = "camera";
RtToken RI_EYE = "eye";

////////////////////////////////////////////////////////////////////////
//
//	Filter types
//
////////////////////////////////////////////////////////////////////////
RtToken RI_BOXFILTER = "box";
RtToken RI_TRIANGLEFILTER = "triangle";
RtToken RI_GAUSSIANFILTER = "gaussian";
RtToken RI_SINCFILTER = "sinc";
RtToken RI_CATMULLROMFILTER = "catmull-rom";
RtToken RI_BLACKMANHARRISFILTER = "blackman-harris";
RtToken RI_MITCHELLFILTER = "mitchell";
RtToken RI_BESSELFILTER = "bessel";
RtToken RI_DISKFILTER = "disk";
RtToken RI_CUSTOM = "custom";

RtToken RI_MIN = "min";
RtToken RI_MAX = "max";
RtToken RI_AVERAGE = "average";
RtToken RI_ZMIN = "zmin";
RtToken RI_ZMAX = "zmax";

////////////////////////////////////////////////////////////////////////
//
//	Non-Standard options/attributes entry points
//
////////////////////////////////////////////////////////////////////////
RtToken RI_LIMITS = "limits";
RtToken RI_SEARCHPATH = "searchpath";
RtToken RI_SHADOW = "shadow";
RtToken RI_RENDER = "render";
RtToken RI_DICE = "dice";
RtToken RI_HINT = "hint";
RtToken RI_TEXTURE = "texture";
RtToken RI_HIDER = "hider";
RtToken RI_STATISTICS = "statistics";
RtToken RI_VISIBILITY = "visibility";
RtToken RI_SHADE = "shade";
RtToken RI_SHADERFORMAT = "shaderformat";
RtToken RI_BLOBBY = "blobby";
RtToken RI_BLOBBYTOLERANCE = "tolerance";
RtToken RI_BLOBBYOPCODEORDER = "opcodeorder";
RtToken RI_BOUND = "bound";
RtToken RI_DISPLACEMENT = "displacement";
RtToken RI_DISPLACEMENTBOUND = "displacementbound";
RtToken RI_IRRADIANCE = "irradiance";
RtToken RI_CULL = "cull";
RtToken RI_COMPRESSION = "compression";
RtToken RI_RIB = "rib";
RtToken RI_SHUTTER = "shutter";
RtToken RI_USER = "user";
RtToken RI_TRIMCURVE = "trimcurve";
RtToken RI_SENSE = "sense";
RtToken RI_TEXTURECACHE = "texturecache";

////////////////////////////////////////////////////////////////////////
//
//	Non-Standard attributes
//
////////////////////////////////////////////////////////////////////////

RtToken RI_NORMALDEVIATION = "normaldeviation";
RtToken RI_POINTDEVIATION = "pointdeviation";

// Dice attributes
RtToken RI_MINSUBDIVISION = "minsubdivision";
RtToken RI_MAXSUBDIVISION = "maxsubdivision";
RtToken RI_NUMPROBES = "numprobes";
RtToken RI_MINSPLITS = "minsplits";
RtToken RI_BOUNDEXPAND = "boundexpand";
RtToken RI_BINARY = "binary";
RtToken RI_RASTERORIENT = "rasterorient";

// Displacementbound attributes
RtToken RI_SPHERE = "sphere";
RtToken RI_COORDINATESYSYTEM = "coordinatesystem";

// Solid (CSG) attributes
RtToken RI_SOLID = "solid";
RtToken RI_TESSELLATIONTOLERANCE = "tessellationtolerance";

// Visibility attributes
RtToken RI_PHOTON = "photon";
RtToken RI_ENVIRONMENT = "environment";
RtToken RI_TRACE = "trace";
RtToken RI_DIFFUSE = "diffuse";
RtToken RI_SPECULAR = "specular";
RtToken RI_TRANSMISSION = "transmission";
RtToken RI_CAUSTICS = "caustics";

RtToken RI_DIFFUSEHITMODE = "diffusehitmode";
RtToken RI_SPECULARHITMODE = "specularhitmode";
RtToken RI_TRANSMISSIONHITMODE = "transmissionhitmode";
RtToken RI_CAMERAHITMODE = "camerahitmode";

// Trace attributes
RtToken RI_BIAS = "bias";
RtToken RI_DISPLACEMENTS = "displacements";
RtToken RI_MAXDIFFUSEDEPTH = "maxdiffusedepth";
RtToken RI_MAXSPECULARDEPTH = "maxspeculardepth";
RtToken RI_SAMPLEMOTION = "samplemotion";

// Photon attributes
RtToken RI_GLOBALMAP = "globalmap";
RtToken RI_CAUSTICMAP = "causticmap";
RtToken RI_ESTIMATOR = "estimator";
RtToken RI_MAXDISTANCE = "maxdistance";
RtToken RI_SHADINGMODEL = "shadingmodel";
RtToken RI_ILLUMINATEFRONT = "illuminatefront";
RtToken RI_IOR = "ior";
RtToken RI_IORRANGE = "iorrange";

// Motionfactor attribute
RtToken RI_MOTIONFACTOR = "motionfactor";

// Cull attributes
// RtToken	RI_HIDDEN				=	"hidden";
RtToken RI_BACKFACING = "backfacing";

////////////////////////////////////////////////////////////////////////
//
//	Non-Standard options
//
////////////////////////////////////////////////////////////////////////

// Searchpath options
RtToken RI_ARCHIVE = "archive";
RtToken RI_PROCEDURAL = "procedural";
RtToken RI_RESOURCE = "resource";
RtToken RI_DISPLAY = "display";
RtToken RI_GEOMETRY = "geometry";

// Limits options
RtToken RI_BUCKETSIZE = "bucketsize";
RtToken RI_METABUCKETS = "metabuckets";
RtToken RI_GRIDSIZE = "gridsize";
RtToken RI_MAXRECURSION = "raydepth";
RtToken RI_TEXTUREMEMORY = "texturememory";
RtToken RI_BRICKMEMORY = "brickmemory";
RtToken RI_EYESPLITS = "eyesplits";
RtToken RI_NUMTHREADS = "numthreads";
RtToken RI_THREADSTRIDE = "threadstride";
RtToken RI_GEOCACHEMEMORY = "geocachememory";
RtToken RI_OTHRESHOLD = "othreshold";
RtToken RI_ZTHRESHOLD = "zthreshold";

// Trace options
RtToken RI_MAXDEPTH = "maxdepth";

// Statstics options
RtToken RI_ENDOFFRAME = "endofframe";
RtToken RI_FILELOG = "filelog";
RtToken RI_PROGRESS = "progress";

RtToken RI_TEXTURECACHEENABLE = "enable";
RtToken RI_TEXTURECACHEDIRECTORY = "directory";

// Irradiance options
RtToken RI_HANDLE = "handle";
RtToken RI_FILEMODE = "filemode";
RtToken RI_MAXERROR = "maxerror";
RtToken RI_MAXPIXELDIST = "maxpixeldist";
RtToken RI_MINSAMPLEDISTANCE = "minsampledistance";
RtToken RI_MAXSAMPLEDISTANCE = "maxsampledistance";

// Hider options
RtToken RI_JITTER = "jitter";
RtToken RI_FALSECOLOR = "falsecolor";
RtToken RI_EMIT = "emit";
RtToken RI_SAMPLESPECTRUM = "samplespectrum";
RtToken RI_DEPTHFILTER = "depthfilter";
RtToken RI_RADIANCECACHE = "radiancecache";

// IO options
RtToken RI_MASKRESOLUTION = "maskresolution";
RtToken RI_MASKPRINTF = "maskprintf";
RtToken RI_MASKLOG = "masklog";
RtToken RI_MASKPROGRESS = "maskprogress";
RtToken RI_MASKSTATS = "maskstats";
RtToken RI_INHERITATTRIBUTES = "inheritattributes";

// Shutter options
RtToken RI_OFFSET = "offset";

// PixelFilter options
RtToken RI_FILTER = "filter";
RtToken RI_PIXELFILTER = "pixelfilter";
RtToken RI_MODE = "mode";
RtToken RI_CONTINUOUS = "continuous";
RtToken RI_PRECOMPUTED = "precomputed";

// Misc junk
RtToken RI_DEFAULT = "default";
RtToken RI_DEFAULTLIGHT = "defaultlight";
RtToken RI_DEFAULTSURFACE = "defaultsurface";
