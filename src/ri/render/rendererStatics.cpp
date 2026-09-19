/**
 * Project: openRender
 *
 * File: rendererStatics.cpp
 *
 * Description:
 *   Storage for CRenderer's static data members and the coordinate/color
 *   system name constants. Split out of renderer.cpp so that a consumer
 *   with no rendering pipeline (orender-wire's ribVector, see the
 *   domain-split plan) can still link -- every file across the geometry,
 *   state and parse domains references these `extern`-style static members
 *   (CRenderer::globalMemory, CRenderer::flags, CRenderer::raytracingFlags,
 *   and dozens more), so SOME translation unit must define them regardless
 *   of whether the actual render pipeline (beginRenderer()/beginFrame()/
 *   render()/endFrame()/endRenderer()/renderFrame(), still in renderer.cpp)
 *   is ever linked in. ribVector never calls any of those pipeline
 *   functions -- confirmed by tracing ribpreview_load()'s actual call graph
 *   (RiBeginLite() -> RiInit(), a purely local token-array setup that never
 *   touches CRenderer at all) -- so renderer.cpp itself is excluded from
 *   ribVector's source list entirely, no stub needed.
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
#include <math.h>
#include <string.h>

#include "includes/logging.hpp"

#include "brickmap.h"
#include "bundles.h"
#include "curves.h"
#include "delayed.h"
#include "dlobject.h"
#include "dso.h"
#include "error.h"
#include "hcshader.h"
#include "implicitSurface.h"
#include "irradiance.h"
#include "memory.h"
#include "netFileMapping.h"
#include "noise.h"
#include "object.h"
#include "patches.h"
#include "photon.h"
#include "photonMap.h"
#include "pl.h"
#include "points.h"
#include "polygons.h"
#include "quadrics.h"
#include "random.h"
#include "ray.h"
#include "raytracer.h"
#include "remoteChannel.h"
#include "renderer.h"
#include "rendererContext.h"
#include "reyes.h"
#include "ri.h"
#include "ri_config.h"
#include "rib.h"
#include "shader.h"
#include "stats.h"
#include "stochastic.h"
#include "subdivisionCreator.h"
#include "texmake.h"
#include "texture.h"
#include "xform.h"
#include "zbuffer.h"

// Textual definitions of predefined coordinate systems
const char *coordinateCameraSystem = "camera";
const char *coordinateWorldSystem = "world";
const char *coordinateObjectSystem = "object";
const char *coordinateShaderSystem = "shader";
const char *coordinateLightSystem = "light";
const char *coordinateNDCSystem = "NDC";
const char *coordinateRasterSystem = "raster";
const char *coordinateScreenSystem = "screen";
const char *coordinateCurrentSystem = "current";

// Textual definitions of the predefined color systems
const char *colorRgbSystem = "rgb";
const char *colorHslSystem = "hsl";
const char *colorHsvSystem = "hsv";
const char *colorXyzSystem = "xyz";
const char *colorYiqSystem = "yiq";
const char *colorXyySystem = "xyy";
const char *colorCieSystem = "cie";

/////////////////////////////////////////////////////////////////////
//
// Static members of the CRenderer
//
/////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
// Global members (active between RiBegin() - RiEnd())
////////////////////////////////////////////////////////////////////
CMemPage *CRenderer::globalMemory = NULL;                                    // initialized in beginRenderer, destroyed in endRenderer
CRendererContext *CRenderer::context = NULL;                                 // initialzied in beginRenderer
CArray<CShaderInstance *> *CRenderer::allLights = NULL;                      // initialized in beginRenderer, destroyed in endRenderer
CShaderInstance *CRenderer::imagerShader = nullptr;                          // initialized in beginFrame
thread_local CShadingContext *CRenderer::activeContext = nullptr;            // set by each render thread at loop start
CTrie<CNamedCoordinateSystem *> *CRenderer::definedCoordinateSystems = NULL; // initialized in initDeclarations, destroyed in shutdownDeclarations
CTrie<CVariable *> *CRenderer::declaredVariables = NULL;                     // initialized in initDeclarations, destroyed in shutdownDeclarations
CTrie<CFileResource *> *CRenderer::globalFiles = NULL;                       // initialized in initFiles, destroyed in shutdownFiles
CTrie<CGlobalIdentifier *> *CRenderer::globalIdHash = NULL;                  // initialized in initDeclarations, destroyed in shutdownDeclarations
CTrie<CNetFileMapping *> *CRenderer::netFileMappings = NULL;                 // initialized in initNetwork, destroyed in shutdownNetwork
int CRenderer::numKnownGlobalIds = 0;                                        // initialized in initDeclarations
CVariable *CRenderer::variables = NULL;                                      // initialized in initDeclarations, destroyed in shutdownDeclarations
CArray<CVariable *> *CRenderer::globalVariables = NULL;                      // initialized in initDeclarations, destroyed in shutdownDeclarations
CTrie<CDisplayChannel *> *CRenderer::declaredChannels = NULL;                // initialized in initDeclarations, destroyed in shutdownDeclarations
CArray<CDisplayChannel *> *CRenderer::displayChannels = NULL;                // initialized in initDeclarations, destroyed in shutdownDeclarations
CDSO *CRenderer::dsos = NULL;                                                // initialized in initFiles, destroyed in shutdownFiles
SOCKET CRenderer::netClient = INVALID_SOCKET;                                // initialized in initNetwork
int CRenderer::netNumServers = 0;                                            // initialized in initNetwork
SOCKET *CRenderer::netServers = NULL;                                        // initialized in initNetwork, destroyed in shutdownNetwork
char CRenderer::temporaryPath[OS_MAX_PATH_LENGTH];                           // initialized in beginRenderer

////////////////////////////////////////////////////////////////////
// Local members (active between RiWorldBegin() - RiWorldEnd())
////////////////////////////////////////////////////////////////////
// Frame options - initialized in copyOptions()
int CRenderer::xres, CRenderer::yres;
int CRenderer::frame;
float CRenderer::pixelAR;
float CRenderer::frameAR;
float CRenderer::cropLeft, CRenderer::cropRight, CRenderer::cropTop, CRenderer::cropBottom;
float CRenderer::screenLeft, CRenderer::screenRight, CRenderer::screenTop, CRenderer::screenBottom;
float CRenderer::clipMin, CRenderer::clipMax;
float CRenderer::pixelVariance;
float CRenderer::jitter;
char *CRenderer::hider;
TSearchpath *CRenderer::archivePath;
TSearchpath *CRenderer::proceduralPath;
TSearchpath *CRenderer::texturePath;
TSearchpath *CRenderer::shaderPath;
TSearchpath *CRenderer::displayPath;
TSearchpath *CRenderer::modulePath;
TSearchpath *CRenderer::geometryPath;
int CRenderer::pixelXsamples, CRenderer::pixelYsamples;
float CRenderer::gamma, CRenderer::gain;
float CRenderer::pixelFilterWidth, CRenderer::pixelFilterHeight;
RtFilterFunc CRenderer::pixelFilter;
float CRenderer::colorQuantizer[5];
float CRenderer::depthQuantizer[5];
vector CRenderer::opacityThreshold;
vector CRenderer::zvisibilityThreshold;
COptions::CDisplay *CRenderer::displays;
COptions::CClipPlane *CRenderer::clipPlanes;
float CRenderer::relativeDetail;
EProjectionType CRenderer::projection;
float CRenderer::fov;
int CRenderer::nColorComps;
float *CRenderer::fromRGB, *CRenderer::toRGB;
float CRenderer::fstop, CRenderer::focallength, CRenderer::focaldistance;
float CRenderer::shutterOpen, CRenderer::shutterClose;
float CRenderer::shutterTime, CRenderer::invShutterTime; // initialized in beginFrame
unsigned int CRenderer::flags;

// openRender dependent options
int CRenderer::endofframe;
char *CRenderer::filelog;
int CRenderer::numThreads;
int CRenderer::maxTextureSize;
int CRenderer::maxBrickSize;
int CRenderer::maxGridSize;
int CRenderer::maxRayDepth;
int CRenderer::maxPhotonDepth;
int CRenderer::bucketWidth, CRenderer::bucketHeight;
int CRenderer::netXBuckets, CRenderer::netYBuckets;
int CRenderer::threadStride;
int CRenderer::geoCacheSize;
int CRenderer::maxEyeSplits;
float CRenderer::tsmThreshold;
char *CRenderer::causticIn, *CRenderer::causticOut;
char *CRenderer::globalIn, *CRenderer::globalOut;
int CRenderer::numEmitPhotons;
int CRenderer::shootStep;
EDepthFilter CRenderer::depthFilter;

// Frame data
TMemCheckpoint CRenderer::frameCheckpoint;                               // initialized in beginFrame
CTrie<CFileResource *> *CRenderer::frameFiles = NULL;                    // initialized in beginFrame, destroyed in endFrame
CArray<const char *> *CRenderer::frameTemporaryFiles = NULL;             // initialized in beginFrame, destroyed in endFrame
CShadingContext **CRenderer::contexts = NULL;                            // initialized in beginFrame, destroyed in endFrame
int CRenderer::numActiveThreads = 0;                                     // initialized in beginFrame
CTrie<CRemoteChannel *> *CRenderer::declaredRemoteChannels = NULL;       // initialized in beginFrame, destroyed in endFrame
CArray<CRemoteChannel *> *CRenderer::remoteChannels = NULL;              // initialized in beginFrame, destroyed in endFrame
unsigned int CRenderer::raytracingFlags = 0;                             // initialized in beginFrame
CObject *CRenderer::root = NULL;                                         // initialized in beginFrame, destroyed in endFrame
CObject *CRenderer::offendingObject = NULL;                              // initialized in beginFrame
matrix CRenderer::fromWorld, CRenderer::toWorld;                         // initialized in beginFrame
matrix CRenderer::fromWorld1, CRenderer::toWorld1;                       // initialized in beginFrame
bool CRenderer::cameraHasMotion = false;                                 // initialized in beginFrame
quaternion CRenderer::relRotQ = {0, 0, 0, 1};                            // initialized in beginFrame
vector CRenderer::relTrans = {0, 0, 0};                                  // initialized in beginFrame
bool CRenderer::cameraHasRotation = false;                               // initialized in beginFrame
bool CRenderer::cameraRotationOnly = false;                              // initialized in beginFrame
bool CRenderer::correlatedSampleTable = false;                           // initialized in beginFrame
vector CRenderer::worldBmin, CRenderer::worldBmax;                       // initialized in beginFrame
CXform *CRenderer::world = NULL;                                         // initialized in beginFrame, destroyed in endFrame
matrix CRenderer::fromNDC, CRenderer::toNDC;                             // initialized in beginFrame
matrix CRenderer::fromRaster, CRenderer::toRaster;                       // initialized in beginFrame
matrix CRenderer::fromScreen, CRenderer::toScreen;                       // initialized in beginFrame
matrix CRenderer::worldToNDC;                                            // initialized in beginFrame
unsigned int CRenderer::hiderFlags;                                      // initialized in beginFrame
int CRenderer::numSamples;                                               // initialized in beginDisplays
int CRenderer::numExtraSamples;                                          // initialized in beginDisplays
int CRenderer::xPixels, CRenderer::yPixels;                              // initialized in beginFrame
unsigned int CRenderer::additionalParameters;                            // initialized in beginDisplays
float CRenderer::pixelLeft, CRenderer::pixelRight;                       // initialized in beginFrame
float CRenderer::pixelTop, CRenderer::pixelBottom;                       // initialized in beginFrame
float CRenderer::dydPixel, CRenderer::dxdPixel;                          // initialized in beginFrame
float CRenderer::dPixeldx, CRenderer::dPixeldy;                          // initialized in beginFrame
float CRenderer::dSampledx, CRenderer::dSampledy;                        // initialized in beginFrame
int CRenderer::renderLeft, CRenderer::renderRight;                       // initialized in beginFrame
int CRenderer::renderTop, CRenderer::renderBottom;                       // initialized in beginFrame
int CRenderer::xBuckets, CRenderer::yBuckets;                            // initialized in beginFrame
int CRenderer::xBucketsMinusOne;                                         // initialized in beginFrame
int CRenderer::yBucketsMinusOne;                                         // initialized in beginFrame
float CRenderer::invBucketSampleWidth, CRenderer::invBucketSampleHeight; // initialized in beginFrame
int CRenderer::metaXBuckets, CRenderer::metaYBuckets;                    // initialized in beginFrame
float CRenderer::aperture;                                               // initialized in beginFrame
float CRenderer::imagePlane;                                             // initialized in beginFrame
float CRenderer::invImagePlane;                                          // initialized in beginFrame
float CRenderer::cocFactorPixels;                                        // initialized in beginFrame
float CRenderer::cocFactorSamples;                                       // initialized in beginFrame
float CRenderer::cocFactorScreen;                                        // initialized in beginFrame
float CRenderer::invFocaldistance;                                       // initialized in beginFrame
float CRenderer::lengthA, CRenderer::lengthB;                            // initialized in beginFrame

int CRenderer::xSampleOffset, CRenderer::ySampleOffset;      // initialized in beginFrame
float CRenderer::sampleClipRight, CRenderer::sampleClipLeft; // initialized in beginFrame
float CRenderer::sampleClipTop, CRenderer::sampleClipBottom; // initialized in beginFrame
float *CRenderer::pixelFilterKernel;                         // initialized in beginFrame
int CRenderer::pixelFilterMode;                              // initialized in beginFrame

float CRenderer::leftX, CRenderer::leftZ, CRenderer::leftD;       // initialized in beginClipping
float CRenderer::rightX, CRenderer::rightZ, CRenderer::rightD;    // initialized in beginClipping
float CRenderer::topY, CRenderer::topZ, CRenderer::topD;          // initialized in beginClipping
float CRenderer::bottomY, CRenderer::bottomZ, CRenderer::bottomD; // initialized in beginClipping
int CRenderer::numActiveDisplays;                                 // initialized in beginDisplays
int CRenderer::currentXBucket;                                    // initialized in beginFrame
int CRenderer::currentYBucket;                                    // initialized in beginFrame
int CRenderer::currentPhoton;                                     // initialized in beginFrame
int *CRenderer::jobAssignment;                                    // initialized in beginFrame
FILE *CRenderer::deepShadowFile = NULL;                           // initialized in beginDisplays
int *CRenderer::deepShadowIndex = NULL;                           // initialized in beginDisplays
int CRenderer::deepShadowIndexStart;                              // initialized in beginDisplays
char *CRenderer::deepShadowFileName = NULL;                       // initialized in beginDisplays / computeDisplayData
int CRenderer::numDisplays;                                       // initialized in beginDisplays
CRenderer::CDisplayData *CRenderer::datas;                        // initialized in beginDisplays / computeDisplayData
int *CRenderer::sampleOrder;                                      // initialized in beginDisplays / computeDisplayData
float *CRenderer::sampleDefaults;                                 // initialized in beginDisplays / computeDisplayData
int *CRenderer::compChannelOrder;                                 // initialized in beginDisplays / computeDisplayData
int CRenderer::numExtraCompChannels;                              // initialized in beginDisplays / computeDisplayData
int *CRenderer::nonCompChannelOrder;                              // initialized in beginDisplays / computeDisplayData
int CRenderer::numExtraNonCompChannels;                           // initialized in beginDisplays / computeDisplayData
int CRenderer::numExtraChannels;                                  // initialized in beginDisplays / computeDisplayData
int CRenderer::numRenderedBuckets = 0;                            // initialized in beginFrame
int **CRenderer::textureRefNumber = NULL;                         // initialized in initTextures, destroyed in shutdownTextures
CTextureBlock *CRenderer::textureUsedBlocks = NULL;               // initialized in initTextures, destroyed in shutdownTextures
int *CRenderer::textureUsedMemory = NULL;                         // initialized in initTextures, destroyed in shutdownTextures
int *CRenderer::textureMaxMemory = NULL;                          // initialized in initTextures, destroyed in shutdownTextures

const CUserAttributeDictionary *CRenderer::userOptions = NULL; // initialized in beginFrame
