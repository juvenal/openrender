/**
 * Project: openRender
 *
 * File: ribOut.cpp
 *
 * Description:
 *   This file implements the functionality for ribOut.
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
//  File				:	ribOut.cpp
//  Classes				:	CRibOut
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>
#include <string.h>
#include <time.h>

#include "common/os.h"
#include "error.h"
#include "ri.h"
#include "ri_config.h"
#include "ribOut.h"
#include "variable.h"

// This is the size of the temporary buffer we use before going to the file
const int ribOutScratchSize = 1000;

// Options for rib
int preferCompressedRibOut = FALSE;

static const char *getFilter(float (*function)(float, float, float, float)) {
    if (function == RiGaussianFilter) {
        return RI_GAUSSIANFILTER;
    } else if (function == RiBoxFilter) {
        return RI_BOXFILTER;
    } else if (function == RiTriangleFilter) {
        return RI_TRIANGLEFILTER;
    } else if (function == RiCatmullRomFilter) {
        return RI_CATMULLROMFILTER;
    } else if (function == RiBlackmanHarrisFilter) {
        return RI_BLACKMANHARRISFILTER;
    } else if (function == RiMitchellFilter) {
        return RI_MITCHELLFILTER;
    } else if (function == RiSincFilter) {
        return RI_SINCFILTER;
    } else if (function == RiBesselFilter) {
        return RI_BESSELFILTER;
    } else if (function == RiDiskFilter) {
        return RI_DISKFILTER;
    } else {
        return RI_GAUSSIANFILTER;
    }
}

CRibOut::CRibAttributes::CRibAttributes() {
    uStep = 3;
    vStep = 3;
    next = NULL;
}

CRibOut::CRibAttributes::CRibAttributes(CRibAttributes *a) {
    this[0] = a[0];
    this->next = a;
}

CRibOut::CRibAttributes::~CRibAttributes() {
}

CRibOut::CRibOut(const char *n) : CRiInterface() {
    outName = strdup(n);
    if (*outName == '|') {
        outFile = popen(outName + 1, "w");
        outputCompressed = FALSE;
        outputIsPipe = TRUE;
    } else {

#ifdef HAVE_ZLIB

        if ((strstr(outName, ".Z") != NULL) ||
            (strstr(outName, ".zip") != NULL) ||
            (strstr(outName, ".z") != NULL) ||
            (preferCompressedRibOut == TRUE)) {
            outFile = (FILE *)gzopen(outName, "wb");
            outputCompressed = TRUE;
        } else {
            outFile = fopen(outName, "w");
            outputCompressed = FALSE;
        }
#else
        outFile = fopen(outName, "w");
        outputCompressed = FALSE;
#endif

        outputIsPipe = FALSE;
    }
    completeInit();
}

CRibOut::CRibOut(FILE *o) : CRiInterface() {
    outName = NULL;
    outFile = o;
    outputCompressed = FALSE;
    outputIsPipe = FALSE;
    completeInit();
}

void CRibOut::completeInit() {
    struct tm *newtime;
    time_t aclock;

    time(&aclock);
    newtime = localtime(&aclock);

    declaredVariables = new CTrie<CVariable *>;
    numLightSources = 1;
    numObjects = 1;
    attributes = new CRibAttributes;
    scratch = new char[ribOutScratchSize];

    out("##RenderMan RIB-Structure 1.1\n");
    out("##Creator openRender %s\n", openrender_version_string());
    out("##CreationDate %s", asctime(newtime));

    declareDefaultVariables();
}

CRibOut::~CRibOut() {

    if (outName != NULL) {
        if (outputIsPipe) {
            pclose(outFile);
        } else {

#ifdef HAVE_ZLIB
            if (outputCompressed) {
                gzclose(outFile);
            } else {
                fclose(outFile);
            }
#else
            fclose(outFile);
#endif
        }

        free((void *)outName);
    }

    assert(attributes->next == NULL);

    delete attributes;
    declaredVariables->destroy();

    delete[] scratch;
}

void CRibOut::RiDeclare(const char *name, const char *type) {
    out("Declare \"%s\" \"%s\"\n", name, type);
    declareVariable(name, type);
}

void CRibOut::RiFrameBegin(int number) {
    out("FrameBegin %d\n", number);
}

void CRibOut::RiFrameEnd(void) {
    out("FrameEnd\n");
}

void CRibOut::RiWorldBegin(void) {
    out("WorldBegin\n");
}

void CRibOut::RiWorldEnd(void) {
    out("WorldEnd\n");
}

void CRibOut::RiFormat(int xres, int yres, float aspect) {
    out("Format %d %d %g\n", xres, yres, aspect);
}

void CRibOut::RiFrameAspectRatio(float aspect) {
    out("FrameAspectRatio %g\n", aspect);
}

void CRibOut::RiScreenWindow(float left, float right, float bot, float top) {
    out("ScreenWindow %g %g %g %g\n", left, right, bot, top);
}

void CRibOut::RiCropWindow(float xmin, float xmax, float ymin, float ymax) {
    out("CropWindow %g %g %g %g\n", xmin, xmax, ymin, ymax);
}

void CRibOut::RiProjectionV(const char *name, int n, const char *tokens[], const void *params[]) {
    out("Projection \"%s\" ", name);
    writePL(n, tokens, params);
}

void CRibOut::RiClipping(float hither, float yon) {
    out("Clipping %g %g\n", hither, yon);
}

void CRibOut::RiClippingPlane(float x, float y, float z, float nx, float ny, float nz) {
    out("ClippingPlane %g %g %g %g %g %g\n", x, y, z, nx, ny, nz);
}

void CRibOut::RiDepthOfField(float fstop, float focallength, float focaldistance) {
    out("DepthOfField %g %g %g\n", fstop, focallength, focaldistance);
}

void CRibOut::RiShutter(float smin, float smax) {
    out("Shutter %g %g\n", smin, smax);
}

void CRibOut::RiPixelVariance(float variance) {
    out("PixelVariance %g\n", variance);
}

void CRibOut::RiPixelSamples(float xsamples, float ysamples) {
    out("PixelSamples %g %g\n", xsamples, ysamples);
}

void CRibOut::RiPixelFilter(float (*function)(float, float, float, float), float xwidth, float ywidth) {
    if (function == RiGaussianFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_GAUSSIANFILTER, xwidth, ywidth);
    } else if (function == RiBoxFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_BOXFILTER, xwidth, ywidth);
    } else if (function == RiTriangleFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_TRIANGLEFILTER, xwidth, ywidth);
    } else if (function == RiCatmullRomFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_CATMULLROMFILTER, xwidth, ywidth);
    } else if (function == RiBlackmanHarrisFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_BLACKMANHARRISFILTER, xwidth, ywidth);
    } else if (function == RiMitchellFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_MITCHELLFILTER, xwidth, ywidth);
    } else if (function == RiSincFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_SINCFILTER, xwidth, ywidth);
    } else if (function == RiBesselFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_BESSELFILTER, xwidth, ywidth);
    } else if (function == RiDiskFilter) {
        out("PixelFilter \"%s\" %g %g\n", RI_DISKFILTER, xwidth, ywidth);
    } else {
        errorHandler(RIE_BADHANDLE, RIE_ERROR, "Failed to write custom filter function\n");
    }
}

void CRibOut::RiExposure(float gain, float gamma) {
    out("Exposure %g %g\n", gain, gamma);
}

void CRibOut::RiImagerV(const char *name, int n, const char *tokens[], const void *params[]) {
    out("Imager \"%s\" ", name);
    writePL(n, tokens, params);
}

void CRibOut::RiQuantize(const char *type, int one, int qmin, int qmax, float ampl) {
    out("Quantize \"%s\" %d %d %d %g\n", type, one, qmin, qmax, ampl);
}

void CRibOut::RiDisplayV(const char *name, const char *type, const char *mode, int n, const char *tokens[], const void *params[]) {
    out("Display \"%s\" \"%s\" \"%s\" ", name, type, mode);
    writePL(n, tokens, params);
}

void CRibOut::RiCustomDisplayV(const char *, RtToken, RtDisplayStartFunction, RtDisplayDataFunction, RtDisplayFinishFunction, RtInt, RtToken[], RtPointer[]) {
    error(CODE_INCAPABLE, "Can not serialize custom displays.\n");
}

void CRibOut::RiDisplayChannelV(const char *channel, int n, const char *tokens[], const void *params[]) {
    out("Display \"%s\" ", channel);
    writePL(n, tokens, params);
}

void CRibOut::RiHiderV(const char *type, int n, const char *tokens[], const void *params[]) {
    out("Hider \"%s\" ", type);
    writePL(n, tokens, params);
}

void CRibOut::RiColorSamples(int N, float *nRGB, float *RGBn) {
    int i;

    out("ColorSamples [ ");

    for (i = 0; i < N * 3; i++) {
        out("%g ", nRGB[i]);
    }

    out("] [ ");

    for (i = 0; i < N * 3; i++) {
        out("%g ", RGBn[i]);
    }

    out("]\n");
}

void CRibOut::RiRelativeDetail(float relativedetail) {
    out("RelativeDetail %g\n", relativedetail);
}

void CRibOut::RiOptionV(const char *name, int n, const char *tokens[], const void *params[]) {
    int i;

    // Check the searchpath options
    if (strcmp(name, RI_SEARCHPATH) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_ARCHIVE) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Option \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_PROCEDURAL) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Option \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_TEXTURE) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Option \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_SHADER) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Option \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_DISPLAY) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Option \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_RESOURCE) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Option \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiOption(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
        // Check the limit options
    } else if (strcmp(name, RI_LIMITS) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_BUCKETSIZE) == 0) {
                const int *val = (const int *)params[i];
                int k;
                out("Option \"%s\" \"%s\" [%i", name, tokens[i], val[0]);
                for (k = 1; k < 2; k++) { out(" %i", val[k]); }
                out("]\n");
            } else if (strcmp(tokens[i], RI_METABUCKETS) == 0) {
                const int *val = (const int *)params[i];
                int k;
                out("Option \"%s\" \"%s\" [%i", name, tokens[i], val[0]);
                for (k = 1; k < 2; k++) { out(" %i", val[k]); }
                out("]\n");
            } else if (strcmp(tokens[i], RI_INHERITATTRIBUTES) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_GRIDSIZE) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_EYESPLITS) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_TEXTUREMEMORY) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_BRICKMEMORY) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiOption(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
        // Check the hider options
    } else if (strcmp(name, RI_HIDER) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_JITTER) == 0) { // GSHTODO: should be INT
                const float *val = (const float *)params[i];
                out("Option \"%s\" \"%s\" [%g]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_FALSECOLOR) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_EMIT) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_DEPTHFILTER) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Option \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiOption(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
        // Check the trace options
    } else if (strcmp(name, RI_TRACE) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_MAXDEPTH) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiOption(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
        // Check the io options
    } else if (strcmp(name, RI_STATISTICS) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_ENDOFFRAME) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_FILELOG) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Option \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_PROGRESS) == 0) {
                const int *val = (const int *)params[i];
                out("Option \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiOption(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
        // Check for rib compression / output options
    } else if (strcmp(name, RI_RIB) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_COMPRESSION) == 0) {
                const char *val = ((const char **)params[i])[0];
                if (strcmp(val, "gzip") == 0) {
                    preferCompressedRibOut = TRUE;
                } else if (strcmp(val, "none") == 0) {
                    preferCompressedRibOut = FALSE;
                } else {
                    error(CODE_BADTOKEN, "Unknown compression type \"%s\"\n", val);
                }
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiOption(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else {
        error(CODE_BADTOKEN, "Unknown option: \"%s\"\n", name);
    }
}



                        void CRibOut::RiAttributeBegin(void) {
                            out("AttributeBegin\n");

                            attributes = new CRibAttributes(attributes);
                        }

                        void CRibOut::RiAttributeEnd(void) {
                            CRibAttributes *old = attributes;

                            out("AttributeEnd\n");

                            attributes = attributes->next;
                            delete old;
                        }

                        void CRibOut::RiColor(float *Cs) {
                            out("Color [%g %g %g]\n", Cs[0], Cs[1], Cs[2]);
                        }

                        void CRibOut::RiOpacity(float *Cs) {
                            out("Opacity [%g %g %g]\n", Cs[0], Cs[1], Cs[2]);
                        }

                        void CRibOut::RiTextureCoordinates(float s1, float t1, float s2, float t2, float s3, float t3, float s4, float t4) {
                            out("TextureCoordinates [%g %g %g %g %g %g %g %g]\n", s1, t1, s2, t2, s3, t3, s4, t4);
                        }

                        void *CRibOut::RiLightSourceV(const char *name, int n, const char *tokens[], const void *params[]) {
                            out("LightSource \"%s\" %d ", name, numLightSources);
                            writePL(n, tokens, params);

                            return (void *)(uintptr_t)numLightSources++;
                        }

                        void *CRibOut::RiAreaLightSourceV(const char *name, int n, const char *tokens[], const void *params[]) {
                            out("AreaLightSource \"%s\" %d ", name, numLightSources);
                            writePL(n, tokens, params);

                            return (void *)(uintptr_t)numLightSources++;
                        }

                        void CRibOut::RiIlluminate(const void *light, int onoff) {
                            out("Illuminate %d %d\n", light, onoff);
                        }

                        void CRibOut::RiSurfaceV(const char *name, int n, const char *tokens[], const void *params[]) {
                            out("Surface \"%s\" ", name);
                            writePL(n, tokens, params);
                        }

                        void CRibOut::RiAtmosphereV(const char *name, int n, const char *tokens[], const void *params[]) {
                            out("Atmosphere \"%s\" ", name);
                            writePL(n, tokens, params);
                        }

                        void CRibOut::RiInteriorV(const char *name, int n, const char *tokens[], const void *params[]) {
                            out("Interior \"%s\" ", name);
                            writePL(n, tokens, params);
                        }

                        void CRibOut::RiExteriorV(const char *name, int n, const char *tokens[], const void *params[]) {
                            out("Exterior \"%s\" ", name);
                            writePL(n, tokens, params);
                        }

                        void CRibOut::RiShadingRate(float size) {
                            out("ShadingRate %g\n", size);
                        }

                        void CRibOut::RiShadingInterpolation(const char *type) {
                            out("ShadingInterpolation \"%s\"\n", type);
                        }

                        void CRibOut::RiMatte(int onoff) {
                            out("Matte %d\n", onoff);
                        }

                        void CRibOut::RiBound(float *bound) {
                            out("Bound [%g %g %g %g %g %g]\n", bound[0], bound[1], bound[2], bound[3], bound[4], bound[5]);
                        }

                        void CRibOut::RiDetail(float *bound) {
                            out("Detail [%g %g %g %g %g %g]\n", bound[0], bound[1], bound[2], bound[3], bound[4], bound[5]);
                        }

                        void CRibOut::RiDetailRange(float minvis, float lowtran, float uptran, float maxvis) {
                            out("DetailRange %g %g %g %g\n", minvis, lowtran, uptran, maxvis);
                        }

                        void CRibOut::RiGeometricApproximation(const char *type, float value) {
                            out("GeometricApproximation \"%s\" %g\n", type, value);
                        }

                        void CRibOut::RiGeometricRepresentation(const char *type) {
                            out("GeometricRepresentation \"%s\"\n", type);
                        }

                        void CRibOut::RiOrientation(const char *orientation) {
                            out("Orientation \"%s\"\n", orientation);
                        }

                        void CRibOut::RiReverseOrientation(void) {
                            out("ReverseOrientation\n");
                        }

                        void CRibOut::RiSides(int nsides) {
                            out("Sides %d\n", nsides);
                        }

                        void CRibOut::RiIdentity(void) {
                            out("Identity\n");
                        }

                        void CRibOut::RiTransform(float transform[][4]) {
                            out("Transform [%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g]\n", transform[0][0], transform[0][1], transform[0][2], transform[0][3], transform[1][0], transform[1][1], transform[1][2], transform[1][3], transform[2][0], transform[2][1], transform[2][2], transform[2][3], transform[3][0], transform[3][1], transform[3][2], transform[3][3]);
                        }

                        void CRibOut::RiConcatTransform(float transform[][4]) {
                            out("ConcatTransform [%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g]\n", transform[0][0], transform[0][1], transform[0][2], transform[0][3], transform[1][0], transform[1][1], transform[1][2], transform[1][3], transform[2][0], transform[2][1], transform[2][2], transform[2][3], transform[3][0], transform[3][1], transform[3][2], transform[3][3]);
                        }

                        void CRibOut::RiPerspective(float fov) {
                            out("Perspective %g\n", fov);
                        }

                        void CRibOut::RiTranslate(float dx, float dy, float dz) {
                            out("Translate %g %g %g\n", dx, dy, dz);
                        }

                        void CRibOut::RiRotate(float angle, float dx, float dy, float dz) {
                            out("Rotate %g %g %g %g\n", angle, dx, dy, dz);
                        }

                        void CRibOut::RiScale(float dx, float dy, float dz) {
                            out("Scale %g %g %g\n", dx, dy, dz);
                        }

                        void CRibOut::RiSkew(float angle, float dx1, float dy1, float dz1, float dx2, float dy2, float dz2) {
                            out("Skew %g %g %g %g %g %g %g\n", angle, dx1, dy1, dz1, dx2, dy2, dz2);
                        }

                        void CRibOut::RiDeformationV(const char *name, int n, const char *tokens[], const void *params[]) {
                            out("Deformation \"%s\" ", name);
                            writePL(n, tokens, params);
                        }

                        void CRibOut::RiDisplacementV(const char *name, int n, const char *tokens[], const void *params[]) {
                            out("Displacement \"%s\" ", name);
                            writePL(n, tokens, params);
                        }

                        void CRibOut::RiCoordinateSystem(const char *space) {
                            out("CoordinateSystem \"%s\"\n", space);
                        }

                        void CRibOut::RiCoordSysTransform(const char *space) {
                            out("CoordSysTransform \"%s\"\n", space);
                        }

                        RtPoint *CRibOut::RiTransformPoints(const char *, const char *, int, RtPoint *) {
                            errorHandler(RIE_SYSTEM, RIE_ERROR, "Failed to output TransformPoints\n");
                            return NULL;
                        }

                        void CRibOut::RiTransformBegin(void) {
                            out("TransformBegin\n");
                        }

                        void CRibOut::RiTransformEnd(void) {
                            out("TransformEnd\n");
                        }

void CRibOut::RiAttributeV(const char *name, int n, const char *tokens[], const void *params[]) {
    int i;

    if (strcmp(name, RI_DICE) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_NUMPROBES) == 0) {
                const int *val = (const int *)params[i];
                int k;
                out("Attribute \"%s\" \"%s\" [%i", name, tokens[i], val[0]);
                for (k = 1; k < 2; k++) { out(" %i", val[k]); }
                out("]\n");
            } else if (strcmp(tokens[i], RI_MINSUBDIVISION) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_MAXSUBDIVISION) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_MINSPLITS) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_BOUNDEXPAND) == 0) {
                const float *val = (const float *)params[i];
                out("Attribute \"%s\" \"%s\" [%g]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_BINARY) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_RASTERORIENT) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else if (strcmp(name, RI_BOUND) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_DISPLACEMENT) == 0) {
                const float *val = (const float *)params[i];
                out("Attribute \"%s\" \"%s\" [%g]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else if (strcmp(name, RI_DISPLACEMENTBOUND) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_SPHERE) == 0) {
                const float *val = (const float *)params[i];
                out("Attribute \"%s\" \"%s\" [%g]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_COORDINATESYSYTEM) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else if (strcmp(name, RI_TRACE) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_DISPLACEMENTS) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_BIAS) == 0) {
                const float *val = (const float *)params[i];
                out("Attribute \"%s\" \"%s\" [%g]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_MAXDIFFUSEDEPTH) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_MAXSPECULARDEPTH) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
        // Check the irradiance cache options
    } else if (strcmp(name, RI_IRRADIANCE) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_HANDLE) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_FILEMODE) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_MAXERROR) == 0) {
                const float *val = (const float *)params[i];
                out("Attribute \"%s\" \"%s\" [%g]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else if (strcmp(name, RI_PHOTON) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_GLOBALMAP) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_CAUSTICMAP) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_SHADINGMODEL) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_IOR) == 0) {
                const float *val = (const float *)params[i];
                out("Attribute \"%s\" \"%s\" [%g]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_ESTIMATOR) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_ILLUMINATEFRONT) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else if (strcmp(name, RI_VISIBILITY) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_TRANSMISSION) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_DIFFUSE) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_SPECULAR) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_CAMERA) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_TRACE) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_PHOTON) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else if (strcmp(name, RI_SHADE) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_TRANSMISSIONHITMODE) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_DIFFUSEHITMODE) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else if (strcmp(tokens[i], RI_SPECULARHITMODE) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else if (strcmp(name, RI_IDENTIFIER) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_NAME) == 0) {
                const char *val = ((const char **)params[i])[0];
                out("Attribute \"%s\" \"%s\" \"%s\"\n", name, tokens[i], val);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    } else if (strcmp(name, RI_CULL) == 0) {
        for (i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_HIDDEN) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else if (strcmp(tokens[i], RI_BACKFACING) == 0) {
                const int *val = (const int *)params[i];
                out("Attribute \"%s\" \"%s\" [%i]\n", name, tokens[i], val[0]);
            } else {
                CVariable var;
                if (parseVariable(&var, NULL, tokens[i]) == TRUE) {
                    RiAttribute(name, var.name, params[i], RI_NULL);
                } else {
                    error(CODE_BADTOKEN, "Unknown %s option: \"%s\"\n", name, tokens[i]);
                }
            }
        }
    }
}



                                                            void CRibOut::RiPolygonV(int nvertices, int n, const char *tokens[], const void *params[]) {
                                                                out("Polygon ");
                                                                writePL(nvertices, nvertices, nvertices, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiGeneralPolygonV(int nloops, int *nverts, int n, const char *tokens[], const void *params[]) {
                                                                int i;
                                                                int nvertices = 0;

                                                                out("GeneralPolygon [");
                                                                for (i = 0; i < nloops; i++) {
                                                                    nvertices += nverts[i];
                                                                    out("%d ", nverts[i]);
                                                                }
                                                                out("] ");

                                                                writePL(nvertices, nvertices, nvertices, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiPointsPolygonsV(int npolys, int *nverts, int *verts, int n, const char *tokens[], const void *params[]) {
                                                                int i;
                                                                int nvertices = 0;
                                                                int mvertex = 0;

                                                                out("PointsPolygons [");

                                                                for (i = 0; i < npolys; i++) {
                                                                    nvertices += nverts[i];
                                                                    out("%d ", nverts[i]);
                                                                }
                                                                out("] ");

                                                                out("[");
                                                                for (i = 0; i < nvertices; i++) {
                                                                    if (verts[i] > mvertex) {
                                                                        mvertex = verts[i];
                                                                    }
                                                                    out("%d ", verts[i]);
                                                                }
                                                                out("] ");
                                                                mvertex++;

                                                                writePL(mvertex, mvertex, nvertices, npolys, n, tokens, params);
                                                            }

                                                            void CRibOut::RiPointsGeneralPolygonsV(int npolys, int *nloops, int *nverts, int *verts, int n, const char *tokens[], const void *params[]) {
                                                                int i, j;
                                                                int sverts = 0;
                                                                int nvertices = 0;
                                                                int k = 0;

                                                                out("PointsGeneralPolygons [");
                                                                for (i = 0; i < npolys; i++) {
                                                                    out("%d ", nloops[i]);
                                                                    for (j = 0; j < nloops[i]; j++, k++) {
                                                                        sverts += nverts[k];
                                                                    }
                                                                }
                                                                out("] ");

                                                                out("[");
                                                                for (k = 0, i = 0; i < npolys; i++) {
                                                                    for (j = 0; j < nloops[i]; j++, k++) {
                                                                        out("%d ", nverts[k]);
                                                                    }
                                                                }
                                                                out("] ");

                                                                out("[");
                                                                for (i = 0; i < sverts; i++) {
                                                                    int newVertexCount = verts[i] + 1;
                                                                    if (newVertexCount > nvertices) {
                                                                        nvertices = newVertexCount;
                                                                    }
                                                                    out("%d ", verts[i]);
                                                                }
                                                                out("] ");

                                                                writePL(nvertices, nvertices, sverts, npolys, n, tokens, params);
                                                            }

                                                            void CRibOut::RiBasis(float ubasis[][4], int ustep, float vbasis[][4], int vstep) {
                                                                if ((ubasis == RiBezierBasis || ubasis == RiBSplineBasis || ubasis == RiCatmullRomBasis || ubasis == RiHermiteBasis || ubasis == RiPowerBasis) &&
                                                                    (vbasis == RiBezierBasis || vbasis == RiBSplineBasis || vbasis == RiCatmullRomBasis || vbasis == RiHermiteBasis || vbasis == RiPowerBasis)) {

                                                                    const char *ubasis_str;
                                                                    if (ubasis == RiBezierBasis)
                                                                        ubasis_str = "bezier";
                                                                    else if (ubasis == RiBSplineBasis)
                                                                        ubasis_str = "b-spline";
                                                                    else if (ubasis == RiCatmullRomBasis)
                                                                        ubasis_str = "catmull-rom";
                                                                    else if (ubasis == RiHermiteBasis)
                                                                        ubasis_str = "hermite";
                                                                    else if (ubasis == RiPowerBasis)
                                                                        ubasis_str = "power";
                                                                    else
                                                                        ubasis_str = "bezier";

                                                                    const char *vbasis_str;
                                                                    if (vbasis == RiBezierBasis)
                                                                        vbasis_str = "bezier";
                                                                    else if (vbasis == RiBSplineBasis)
                                                                        vbasis_str = "b-spline";
                                                                    else if (vbasis == RiCatmullRomBasis)
                                                                        vbasis_str = "catmull-rom";
                                                                    else if (vbasis == RiHermiteBasis)
                                                                        vbasis_str = "hermite";
                                                                    else if (vbasis == RiPowerBasis)
                                                                        vbasis_str = "power";
                                                                    else
                                                                        vbasis_str = "bezier";

                                                                    out("Basis \"%s\" %d \"%s\" %d\n",
                                                                        ubasis_str, ustep,
                                                                        vbasis_str, vstep);
                                                                    attributes->uStep = ustep;
                                                                    attributes->vStep = vstep;
                                                                    return;
                                                                }
                                                                out("Basis [%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g] %d [%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g] %d\n", ubasis[0][0], ubasis[0][1], ubasis[0][2], ubasis[0][3], ubasis[1][0], ubasis[1][1], ubasis[1][2], ubasis[1][3], ubasis[2][0], ubasis[2][1], ubasis[2][2], ubasis[2][3], ubasis[3][0], ubasis[3][1], ubasis[3][2], ubasis[3][3], ustep, vbasis[0][0], vbasis[0][1], vbasis[0][2], vbasis[0][3], vbasis[1][0], vbasis[1][1], vbasis[1][2], vbasis[1][3], vbasis[2][0], vbasis[2][1], vbasis[2][2], vbasis[2][3], vbasis[3][0], vbasis[3][1], vbasis[3][2], vbasis[3][3], vstep);
                                                                attributes->uStep = ustep;
                                                                attributes->vStep = vstep;
                                                            }

                                                            void CRibOut::RiPatchV(const char *type, int n, const char *tokens[], const void *params[]) {
                                                                int uver, vver;

                                                                if (strcmp(type, RI_BILINEAR) == 0) {
                                                                    uver = 2;
                                                                    vver = 2;
                                                                } else if (strcmp(type, RI_BICUBIC) == 0) {
                                                                    uver = 4;
                                                                    vver = 4;
                                                                } else {
                                                                    char tmp[512];

                                                                    snprintf(tmp, sizeof(tmp), "Unknown patch type: \"%s\"\n", type);
                                                                    errorHandler(RIE_BADTOKEN, RIE_ERROR, tmp);
                                                                    return;
                                                                }

                                                                out("Patch \"%s\" ", type);
                                                                writePL(uver * vver, 4, 4, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiPatchMeshV(const char *type, int nu, const char *uwrap, int nv, const char *vwrap, int n, const char *tokens[], const void *params[]) {
                                                                int uw, vw;
                                                                int uver, vver;
                                                                int upatches, vpatches;

                                                                if (strcmp(uwrap, RI_PERIODIC) == 0) {
                                                                    uw = TRUE;
                                                                } else if ((strcmp(uwrap, RI_NONPERIODIC) == 0) || (strcmp(uwrap, RI_NOWRAP) == 0)) {
                                                                    uw = FALSE;
                                                                } else {
                                                                    errorHandler(RIE_BADTOKEN, RIE_ERROR, "Wrapping mode unrecognized\n");
                                                                    return;
                                                                }

                                                                if (strcmp(vwrap, RI_PERIODIC) == 0) {
                                                                    vw = TRUE;
                                                                } else if ((strcmp(vwrap, RI_NONPERIODIC) == 0) || (strcmp(vwrap, RI_NOWRAP) == 0)) {
                                                                    vw = FALSE;
                                                                } else {
                                                                    errorHandler(RIE_BADTOKEN, RIE_ERROR, "Wrapping mode unrecognized\n");
                                                                    return;
                                                                }

                                                                uver = nu;
                                                                vver = nv;

                                                                if (strcmp(type, RI_BICUBIC) == 0) {
                                                                    if (uw) {
                                                                        if ((uver % attributes->uStep) != 0) {
                                                                            errorHandler(RIE_CONSISTENCY, RIE_ERROR, "Unexpected number of u vertices\n");
                                                                            return;
                                                                        }

                                                                        upatches = (uver) / attributes->uStep;
                                                                    } else {
                                                                        if (((uver - 4) % attributes->uStep) != 0) {
                                                                            errorHandler(RIE_CONSISTENCY, RIE_ERROR, "Unexpected number of u vertices\n");
                                                                            return;
                                                                        }

                                                                        upatches = ((uver - 4) / attributes->uStep) + 1;
                                                                    }

                                                                    if (vw) {
                                                                        if ((vver % attributes->vStep) != 0) {
                                                                            errorHandler(RIE_CONSISTENCY, RIE_ERROR, "Unexpected number of v vertices\n");
                                                                            return;
                                                                        }

                                                                        vpatches = (vver) / attributes->vStep;
                                                                    } else {
                                                                        if (((vver - 4) % attributes->vStep) != 0) {
                                                                            errorHandler(RIE_CONSISTENCY, RIE_ERROR, "Unexpected number of v vertices\n");
                                                                            return;
                                                                        }

                                                                        vpatches = ((vver - 4) / attributes->vStep) + 1;
                                                                    }
                                                                } else {
                                                                    if (uw)
                                                                        upatches = uver;
                                                                    else
                                                                        upatches = uver - 1;

                                                                    if (vw)
                                                                        vpatches = vver;
                                                                    else
                                                                        vpatches = vver - 1;
                                                                }

                                                                out("PatchMesh \"%s\" %i \"%s\" %i \"%s\" ", type, nu, uwrap, nv, vwrap);
                                                                writePL(uver * vver, uver * vver, uver * vver, upatches * vpatches, n, tokens, params);
                                                            }

                                                            void CRibOut::RiNuPatchV(int nu, int uorder, float *uknot, float umin, float umax, int nv, int vorder, float *vknot, float vmin, float vmax, int n, const char *tokens[], const void *params[]) {
                                                                int upatches = nu - uorder + 1;
                                                                int vpatches = nv - vorder + 1;
                                                                int i, uk, vk;

                                                                out("NuPatch ");

                                                                // Print the knot sequence
                                                                uk = nu + uorder;
                                                                vk = nv + vorder;
                                                                out("%i %i [%g", nu, uorder, uknot[0]);
                                                                for (i = 1; i < uk; i++)
                                                                    out(" %g", uknot[i]);
                                                                out("] %g %g ", umin, umax);

                                                                out("%i %i [%g", nv, vorder, vknot[0]);
                                                                for (i = 1; i < vk; i++)
                                                                    out(" %g", vknot[i]);
                                                                out("] %g %g ", vmin, vmax);

                                                                writePL(nu * nv, (nu - uorder + 2) * (nv - vorder + 2), (nu - uorder + 2) * (nv - vorder + 2), upatches * vpatches, n, tokens, params);
                                                            }

                                                            void CRibOut::RiTrimCurve(int nloops, int *ncurves, int *order, float *knot, float *amin, float *amax, int *n, float *u, float *v, float *w) {
                                                                int i, j, k, numCurves;

                                                                // Write the ncurves
                                                                out("TrimCurve [%d", ncurves[0]);
                                                                numCurves = ncurves[0];
                                                                for (i = 1; i < nloops; i++) {
                                                                    out(" %d", ncurves[i]);
                                                                    numCurves += ncurves[i];
                                                                }

                                                                // Print the order for each curve
                                                                out("] [%d", order[0]);
                                                                for (i = 1; i < numCurves; i++)
                                                                    out(" %d", order[i]);

                                                                // Print the knot vector for each curve
                                                                out("] [");
                                                                for (k = 0, i = 0; i < numCurves; i++) {

                                                                    for (j = n[i] + order[i]; j > 0; j--, k++) {
                                                                        if (k == 0) {
                                                                            out("%g", knot[k]);
                                                                        } else {
                                                                            out(" %g", knot[k]);
                                                                        }
                                                                    }
                                                                }

                                                                // Print the parametric range for each curve
                                                                out("] [%g", amin[0]);
                                                                for (i = 1; i < numCurves; i++) {
                                                                    out(" %g", amin[i]);
                                                                }

                                                                out("] [%g", amax[0]);
                                                                for (i = 1; i < numCurves; i++) {
                                                                    out(" %g", amax[i]);
                                                                }

                                                                // Print the number of vertices for each curve
                                                                out("] [%d", n[0]);
                                                                for (i = 1; i < numCurves; i++) {
                                                                    out(" %d", n[i]);
                                                                }

                                                                // Print the vertices for each curve
                                                                out("] [");
                                                                for (k = 0, i = 0; i < numCurves; i++) {

                                                                    for (j = n[i]; j > 0; j--, k++) {
                                                                        if (k == 0) {
                                                                            out("%g", u[k]);
                                                                        } else {
                                                                            out(" %g", u[k]);
                                                                        }
                                                                    }
                                                                }

                                                                out("] [");
                                                                for (k = 0, i = 0; i < numCurves; i++) {

                                                                    for (j = n[i]; j > 0; j--, k++) {
                                                                        if (k == 0) {
                                                                            out("%g", v[k]);
                                                                        } else {
                                                                            out(" %g", v[k]);
                                                                        }
                                                                    }
                                                                }

                                                                out("] [");
                                                                for (k = 0, i = 0; i < numCurves; i++) {

                                                                    for (j = n[i]; j > 0; j--, k++) {
                                                                        if (k == 0) {
                                                                            out("%g", w[k]);
                                                                        } else {
                                                                            out(" %g", w[k]);
                                                                        }
                                                                    }
                                                                }

                                                                out("]\n");
                                                            }

                                                            void CRibOut::RiSphereV(float radius, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) {
                                                                out("Sphere %g %g %g %g ", radius, zmin, zmax, thetamax);
                                                                writePL(4, 4, 4, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiConeV(float height, float radius, float thetamax, int n, const char *tokens[], const void *params[]) {
                                                                out("Cone %g %g %g ", height, radius, thetamax);
                                                                writePL(4, 4, 4, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiCylinderV(float radius, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) {
                                                                out("Cylinder %g %g %g %g ", radius, zmin, zmax, thetamax);
                                                                writePL(4, 4, 4, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiHyperboloidV(float *point1, float *point2, float thetamax, int n, const char *tokens[], const void *params[]) {
                                                                out("Hyperboloid %g %g %g %g %g %g %g ", point1[0], point1[1], point1[2], point2[0], point2[1], point2[2], thetamax);
                                                                writePL(4, 4, 4, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiParaboloidV(float rmax, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) {
                                                                out("Paraboloid %g %g %g %g ", rmax, zmin, zmax, thetamax);
                                                                writePL(4, 4, 4, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiDiskV(float height, float radius, float thetamax, int n, const char *tokens[], const void *params[]) {
                                                                out("Disk %g %g %g ", height, radius, thetamax);
                                                                writePL(4, 4, 4, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiTorusV(float majorrad, float minorrad, float phimin, float phimax, float thetamax, int n, const char *tokens[], const void *params[]) {
                                                                out("Torus %g %g %g %g %g", majorrad, minorrad, phimin, phimax, thetamax);
                                                                writePL(4, 4, 4, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiProcedural(void *, float *, void (*)(void *, float), void (*)(void *)) {
                                                                errorHandler(RIE_UNIMPLEMENT, RIE_ERROR, "Failed to output procedural geometry\n");
                                                            }

                                                            void CRibOut::RiGeometryV(const char *type, int n, const char *tokens[], const void *params[]) {
                                                                out("Geometry \"%s\" ", type);
                                                                if (n > 0)
                                                                    writePL(n, tokens, params);
                                                                else
                                                                    out("\n");
                                                            }

                                                            void CRibOut::RiCurvesV(const char *degree, int ncurves, int nverts[], const char *wrap, int n, const char *tokens[], const void *params[]) {
                                                                int i;
                                                                int nvertices = 0;
                                                                int nvaryings = 0;
                                                                int wrapadd;

                                                                if (strcmp(wrap, RI_PERIODIC) == 0) {
                                                                    wrapadd = 0;
                                                                } else {
                                                                    wrapadd = 1;
                                                                }

                                                                out("Curves \"%s\" [", degree);

                                                                if (strcmp(degree, RI_LINEAR) == 0) {
                                                                    for (i = 0; i < ncurves; i++) {
                                                                        nvertices += nverts[i];
                                                                        out("%d ", nverts[i]);
                                                                    }

                                                                    nvaryings = nvertices;
                                                                } else if (strcmp(degree, RI_CUBIC) == 0) {
                                                                    for (i = 0; i < ncurves; i++) {
                                                                        int j = (nverts[i] - 4) / attributes->vStep + 1;
                                                                        nvertices += nverts[i];
                                                                        nvaryings += j + wrapadd;
                                                                        out("%d ", nverts[i]);
                                                                    }
                                                                }

                                                                out("] \"%s\" ", wrap);

                                                                writePL(nvertices, nvaryings, nvaryings, ncurves, n, tokens, params);
                                                            }

                                                            void CRibOut::RiPointsV(int npts, int n, const char *tokens[], const void *params[]) {
                                                                out("Points ");
                                                                writePL(npts, npts, npts, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiSubdivisionMeshV(const char *scheme, int nfaces, int nvertices[], int vertices[], int ntags, const char *tags[], int nargs[], int intargs[], float floatargs[], int n, const char *tokens[], const void *params[]) {
                                                                int numVertices;
                                                                int i, j;
                                                                int numInt, numFloat;
                                                                int numFacevaryings;

                                                                for (i = 0, j = 0; i < nfaces; j += nvertices[i], i++)
                                                                    ;
                                                                numFacevaryings = j;

                                                                for (numVertices = -1, i = 0; i < j; i++) {
                                                                    if (vertices[i] > numVertices)
                                                                        numVertices = vertices[i];
                                                                }
                                                                numVertices++;

                                                                out("SubdivisionMesh \"%s\" [ ", scheme);
                                                                for (i = 0; i < nfaces; i++) {
                                                                    out("%d ", nvertices[i]);
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < j; i++) {
                                                                    out("%d ", vertices[i]);
                                                                }

                                                                out("] [");
                                                                for (i = 0; i < ntags; i++) {
                                                                    out(" \"%s\" ", tags[i]);
                                                                }

                                                                out("] [");
                                                                numInt = 0;
                                                                numFloat = 0;
                                                                for (i = 0; i < ntags; i++) {
                                                                    out(" %d %d ", nargs[0], nargs[1]);
                                                                    numInt += nargs[0];
                                                                    numFloat += nargs[1];
                                                                    nargs += 2;
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < numInt; i++) {
                                                                    out("%d ", intargs[i]);
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < numFloat; i++) {
                                                                    out("%g ", floatargs[i]);
                                                                }
                                                                out("] ");

                                                                writePL(numVertices, numVertices, numFacevaryings, nfaces, n, tokens, params);
                                                            }

                                                            void CRibOut::RiHierarchicalSubdivisionMeshV(const char *scheme, int nfaces, int nvertices[], int vertices[], int ntags, const char *tags[], int nargs[], int intargs[], float floatargs[], int noverrides, int overrideFaceIndex[], int overrideLevel[], const char *overrideTags[], float overrideValues[], int n, const char *tokens[], const void *params[]) {
                                                                int numVertices;
                                                                int i, j;
                                                                int numInt, numFloat;
                                                                int numFacevaryings;

                                                                for (i = 0, j = 0; i < nfaces; j += nvertices[i], i++)
                                                                    ;
                                                                numFacevaryings = j;

                                                                for (numVertices = -1, i = 0; i < j; i++) {
                                                                    if (vertices[i] > numVertices)
                                                                        numVertices = vertices[i];
                                                                }
                                                                numVertices++;

                                                                out("HierarchicalSubdivisionMesh \"%s\" [ ", scheme);
                                                                for (i = 0; i < nfaces; i++) {
                                                                    out("%d ", nvertices[i]);
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < j; i++) {
                                                                    out("%d ", vertices[i]);
                                                                }

                                                                out("] [");
                                                                for (i = 0; i < ntags; i++) {
                                                                    out(" \"%s\" ", tags[i]);
                                                                }

                                                                out("] [");
                                                                numInt = 0;
                                                                numFloat = 0;
                                                                for (i = 0; i < ntags; i++) {
                                                                    out(" %d %d ", nargs[0], nargs[1]);
                                                                    numInt += nargs[0];
                                                                    numFloat += nargs[1];
                                                                    nargs += 2;
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < numInt; i++) {
                                                                    out("%d ", intargs[i]);
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < numFloat; i++) {
                                                                    out("%g ", floatargs[i]);
                                                                }
                                                                out("] [ ");

                                                                for (i = 0; i < noverrides; i++) {
                                                                    out("%d ", overrideFaceIndex[i]);
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < noverrides; i++) {
                                                                    out("%d ", overrideLevel[i]);
                                                                }

                                                                out("] [");
                                                                for (i = 0; i < noverrides; i++) {
                                                                    out(" \"%s\" ", overrideTags[i]);
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < noverrides; i++) {
                                                                    out("%g ", overrideValues[i]);
                                                                }
                                                                out("] ");

                                                                writePL(numVertices, numVertices, numFacevaryings, nfaces, n, tokens, params);
                                                            }

                                                            ///////////////////////////////////////////////////////////////////////
                                                            // Class				:	CRibOut
                                                            // Method				:	RiBlobbyV
                                                            // Description			:	Re-emit a Blobby statement (FR-004)
                                                            // Comments				:	This is a correctness requirement, not a
                                                            //							convenience. Each server in a distributed
                                                            //							render re-derives its own surface from the
                                                            //							re-emitted declaration, so a stub here means
                                                            //							the primitive is silently lost across
                                                            //							servers and on any RIB round trip.
                                                            //
                                                            //							Always emits the four-array form, including
                                                            //							the strings array: it is the general one, and
                                                            //							a scene that used the three-array form reads
                                                            //							back identically from it.
                                                            ///////////////////////////////////////////////////////////////////////
                                                            void CRibOut::RiBlobbyV(int nleaf, int ncode, int code[], int nfloats, float floats[], int nstrings, const char *strings[], int n, const char *tokens[], const void *params[]) {
                                                                int i;

                                                                out("Blobby %d [ ", nleaf);
                                                                for (i = 0; i < ncode; i++) {
                                                                    out("%d ", code[i]);
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < nfloats; i++) {
                                                                    out("%g ", floats[i]);
                                                                }

                                                                out("] [ ");
                                                                for (i = 0; i < nstrings; i++) {
                                                                    out("\"%s\" ", strings[i] == NULL ? "" : strings[i]);
                                                                }
                                                                out("] ");

                                                                // Per-blob parameters are varying/vertex over the primitive
                                                                // fields, so the vertex, varying and facevarying counts are all
                                                                // the leaf count, and there is one uniform value for the whole
                                                                // primitive (contracts/rib-binding.md 1).
                                                                writePL(nleaf, nleaf, nleaf, 1, n, tokens, params);
                                                            }

                                                            void CRibOut::RiProcDelayedReadArchive(const char *, float) {
                                                            }

                                                            void CRibOut::RiProcRunProgram(const char *, float) {
                                                            }

                                                            void CRibOut::RiProcDynamicLoad(const char *, float) {
                                                            }

                                                            void CRibOut::RiProcFree(const char *) {
                                                            }

                                                            void CRibOut::RiSolidBegin(const char *type) {
                                                                out("SolidBegin \"%s\"\n", type);
                                                            }

                                                            void CRibOut::RiSolidEnd(void) {
                                                                out("SolidEnd\n");
                                                            }

                                                            void *CRibOut::RiObjectBegin(void) {
                                                                out("ObjectBegin %d\n", numObjects);

                                                                return (void *)(uintptr_t)numObjects++;
                                                            }

                                                            void CRibOut::RiObjectEnd(void) {
                                                                out("ObjectEnd\n");
                                                            }

                                                            void CRibOut::RiObjectInstance(const void *handle) {
                                                                out("ObjectInstance %d\n", handle);
                                                            }

                                                            void CRibOut::RiMotionBeginV(int N, float times[]) {
                                                                int i;

                                                                out("MotionBegin [ ");
                                                                for (i = 0; i < N; i++) {
                                                                    out(" %g ", times[i]);
                                                                }
                                                                out("]\n");
                                                            }

                                                            void CRibOut::RiMotionEnd(void) {
                                                                out("MotionEnd\n");
                                                            }

                                                            void CRibOut::RiMakeTextureV(const char *pic, const char *tex, const char *swrap, const char *twrap, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) {
                                                                out("MakeTexture \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" %g %g ", pic, tex, swrap, twrap, getFilter(filterfunc), swidth, twidth);
                                                                writePL(n, tokens, params);
                                                            }

                                                            void CRibOut::RiMakeBumpV(const char *pic, const char *tex, const char *swrap, const char *twrap, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) {
                                                                out("MakeBump \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" %g %g ", pic, tex, swrap, twrap, getFilter(filterfunc), swidth, twidth);
                                                                writePL(n, tokens, params);
                                                            }

                                                            void CRibOut::RiMakeLatLongEnvironmentV(const char *pic, const char *tex, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) {
                                                                out("MakeBump \"%s\" \"%s\" \"%s\" %g %g", pic, tex, getFilter(filterfunc), swidth, twidth);
                                                                writePL(n, tokens, params);
                                                            }

                                                            void CRibOut::RiMakeCubeFaceEnvironmentV(const char *px, const char *nx, const char *py, const char *ny, const char *pz, const char *nz, const char *tex, float fov, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) {
                                                                out("MakeCubeFaceEnvironment \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" %g \"%s\" %g %g ", px, nx, py, ny, pz, nz, tex, fov, getFilter(filterfunc), swidth, twidth);
                                                                writePL(n, tokens, params);
                                                            }

                                                            void CRibOut::RiMakeShadowV(const char *pic, const char *tex, int n, const char *tokens[], const void *params[]) {
                                                                out("MakeShadow \"%s\" \"%s\" ", pic, tex);
                                                                writePL(n, tokens, params);
                                                            }

                                                            void CRibOut::RiMakeBrickMapV(int n, const char **src, const char *dest, int numTokens, const char *tokens[], const void *params[]) {
                                                                out("MakeBrickMap [");
                                                                for (int i = 0; i < n; i++)
                                                                    out("\"%s\" ", src[i]);
                                                                out("] \"%s\" ", dest);
                                                                writePL(numTokens, tokens, params);
                                                            }

                                                            void CRibOut::RiErrorHandler(void (*handler)(int, int, const char *)) {
                                                                errorHandler = handler;
                                                            }

                                                            void CRibOut::RiArchiveRecord(const char *type, const char *format, va_list args) {
                                                                if (strcmp(type, RI_COMMENT) == 0) {
                                                                    out("#");
                                                                    vout(format, args);
                                                                    out("\n");
                                                                } else if (strcmp(type, RI_STRUCTURE) == 0) {
                                                                    out("##");
                                                                    vout(format, args);
                                                                    out("\n");
                                                                } else if (strcmp(type, RI_VERBATIM) == 0) {
                                                                    vout(format, args);
                                                                    out("\n");
                                                                } else {
                                                                    error(CODE_BADTOKEN, "Unknown record type: \"%s\"\n", type);
                                                                }
                                                            }

                                                            void CRibOut::RiReadArchiveV(const char *filename, void (*)(const char *, ...), int, const char *[], const void *[]) {
                                                                out("ReadArchive \"%s\"\n", filename);
                                                            }

                                                            void *CRibOut::RiArchiveBeginV(const char *name, int n, const char *tokens[], const void *parms[]) {
                                                                out("ArchiveBegin \"%s\" ", name);
                                                                writePL(n, tokens, parms);
                                                                return NULL;
                                                            }

                                                            void CRibOut::RiArchiveEnd(void) {
                                                                out("ArchiveEnd\n");
                                                            }

                                                            void CRibOut::RiResourceV(const char *handle, const char *type, int n, const char *tokens[], const void *parms[]) {
                                                                out("Resource \"%s\" \"%s\" ", handle, type);
                                                                writePL(n, tokens, parms);
                                                            }

                                                            void CRibOut::RiResourceBegin(void) {
                                                                out("ResourceBegin\n");
                                                            }

                                                            void CRibOut::RiResourceEnd(void) {
                                                                out("ResourceEnd\n");
                                                            }

                                                            void CRibOut::RiIfBeginV(const char *expr, int n, const char *tokens[], const void *parms[]) {
                                                                out("IfBegin \"%s\" ", expr);
                                                                writePL(n, tokens, parms);
                                                            }

                                                            void CRibOut::RiElseIfV(const char *expr, int n, const char *tokens[], const void *parms[]) {
                                                                out("ElseIf \"%s\" ", expr);
                                                                writePL(n, tokens, parms);
                                                            }

                                                            void CRibOut::RiElse(void) {
                                                                out("Else\n");
                                                            }

                                                            void CRibOut::RiIfEnd(void) {
                                                                out("IfEnd\n");
                                                            }

                                                            void CRibOut::writePL(int numParameters, const char *tokens[], const void *vals[]) {
                                                                int i, j;
                                                                const float *f;
                                                                const int *iv;
                                                                const char **s;

                                                                for (i = 0; i < numParameters; i++) {
                                                                    CVariable tmpVar;
                                                                    CVariable *variable;

                                                                    if (declaredVariables->find(tokens[i], variable) == TRUE) {
                                                                    retry:;

                                                                        out(" \"%s\" [", tokens[i]);

                                                                        switch (variable->type) {
                                                                        case TYPE_FLOAT:

                                                                            f = (float *)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, f++) {
                                                                                out("%g ", f[0]);
                                                                            }
                                                                            break;
                                                                        case TYPE_COLOR:
                                                                        case TYPE_VECTOR:
                                                                        case TYPE_NORMAL:
                                                                        case TYPE_POINT:

                                                                            f = (float *)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, f += 3) {
                                                                                out("%g %g %g ", f[0], f[1], f[2]);
                                                                            }
                                                                            break;
                                                                        case TYPE_MATRIX:

                                                                            f = (float *)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, f += 16) {
                                                                                out("%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g ", f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]);
                                                                            }
                                                                            break;
                                                                        case TYPE_MPOINT:

                                                                            f = (float *)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, f += 16) {
                                                                                out("%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g ", f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]);
                                                                            }
                                                                            break;
                                                                        case TYPE_QUAD:

                                                                            f = (float *)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, f += 4) {
                                                                                out("%g %g %g %g ", f[0], f[1], f[2], f[3]);
                                                                            }
                                                                            break;
                                                                        case TYPE_DOUBLE:

                                                                            f = (float *)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, f += 2) {
                                                                                out("%g %g ", f[0], f[1]);
                                                                            }
                                                                            break;
                                                                        case TYPE_STRING:

                                                                            s = (const char **)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, s++) {
                                                                                out("\"%s\" ", s[0]);
                                                                            }

                                                                            break;
                                                                        case TYPE_INTEGER:
                                                                            iv = (int *)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, iv++) {
                                                                                out("%d ", iv[0]);
                                                                            }
                                                                            break;
                                                                        default:
                                                                            break;
                                                                        }

                                                                        out("] ");
                                                                    } else {
                                                                        if (parseVariable(&tmpVar, NULL, tokens[i])) {
                                                                            variable = &tmpVar;
                                                                            goto retry;
                                                                        } else {
                                                                            char tmp[512];

                                                                            snprintf(tmp, sizeof(tmp), "Parameter \"%s\" not found\n", tokens[i]);
                                                                            errorHandler(RIE_BADTOKEN, RIE_ERROR, tmp);
                                                                        }
                                                                    }
                                                                }

                                                                // Print the final newline
                                                                out("\n");
                                                            }

                                                            void CRibOut::writePL(int numVertex, int numVarying, int numFaceVarying, int numUniform, int numParameters, const char *tokens[], const void *vals[]) {
                                                                int i, j;
                                                                const float *f;
                                                                const char **s;

#define numItems(__dest, __var)                            \
    switch (variable->container) {                         \
    case CONTAINER_UNIFORM:                                \
        __dest = __var->numItems * numUniform;             \
        break;                                             \
    case CONTAINER_VERTEX:                                 \
        __dest = __var->numItems * numVertex;              \
        break;                                             \
    case CONTAINER_VARYING:                                \
        __dest = __var->numItems * numVarying;             \
        break;                                             \
    case CONTAINER_FACEVARYING:                            \
        __dest = __var->numItems * numFaceVarying;         \
        break;                                             \
    case CONTAINER_CONSTANT:                               \
        __dest = __var->numItems;                          \
        break;                                             \
    default:                                               \
        error(CODE_BUG, "Unknown container in writePL\n"); \
        __dest = 1;                                        \
    }

                                                                for (i = 0; i < numParameters; i++) {
                                                                    CVariable tmpVar;
                                                                    CVariable *variable;

                                                                    if (declaredVariables->find(tokens[i], variable) == TRUE) {
                                                                    retry:;
                                                                        out(" \"%s\" [", tokens[i]);

                                                                        switch (variable->type) {
                                                                        case TYPE_FLOAT:

                                                                            f = (float *)vals[i];
                                                                            numItems(j, variable);
                                                                            for (; j > 0; j--, f++) {
                                                                                out("%g ", f[0]);
                                                                            }
                                                                            break;
                                                                        case TYPE_COLOR:
                                                                        case TYPE_VECTOR:
                                                                        case TYPE_NORMAL:
                                                                        case TYPE_POINT:

                                                                            f = (float *)vals[i];
                                                                            numItems(j, variable);
                                                                            for (; j > 0; j--, f += 3) {
                                                                                out("%g %g %g ", f[0], f[1], f[2]);
                                                                            }
                                                                            break;
                                                                        case TYPE_MATRIX:

                                                                            f = (float *)vals[i];
                                                                            numItems(j, variable);
                                                                            for (; j > 0; j--, f += 16) {
                                                                                out("%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g ", f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]);
                                                                            }
                                                                            break;
                                                                        case TYPE_MPOINT:

                                                                            f = (float *)vals[i];
                                                                            numItems(j, variable);
                                                                            for (; j > 0; j--, f += 16) {
                                                                                out("%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g ", f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]);
                                                                            }
                                                                            break;
                                                                        case TYPE_QUAD:

                                                                            f = (float *)vals[i];
                                                                            numItems(j, variable);
                                                                            for (; j > 0; j--, f += 4) {
                                                                                out("%g %g %g %g ", f[0], f[1], f[2], f[3]);
                                                                            }
                                                                            break;
                                                                        case TYPE_DOUBLE:

                                                                            f = (float *)vals[i];
                                                                            numItems(j, variable);
                                                                            for (; j > 0; j--, f += 2) {
                                                                                out("%g %g ", f[0], f[1]);
                                                                            }
                                                                            break;
                                                                        case TYPE_STRING:

                                                                            s = (const char **)vals[i];
                                                                            for (j = variable->numItems; j > 0; j--, s++) {
                                                                                out("\"%s\" ", s[0]);
                                                                            }
                                                                            break;
                                                                        case TYPE_INTEGER:
                                                                            break;
                                                                        default:
                                                                            break;
                                                                        }

                                                                        out("] ");
                                                                    } else {
                                                                        if (parseVariable(&tmpVar, NULL, tokens[i])) {
                                                                            variable = &tmpVar;
                                                                            goto retry;
                                                                        } else {
                                                                            char tmp[512];

                                                                            snprintf(tmp, sizeof(tmp), "Parameter \"%s\" not found\n", tokens[i]);
                                                                            errorHandler(RIE_BADTOKEN, RIE_ERROR, tmp);
                                                                        }
                                                                    }
                                                                }

                                                                // Print the final newline
                                                                out("\n");

#undef numItems
                                                            }

                                                            void CRibOut::declareVariable(const char *name, const char *decl) {
                                                                CVariable cVariable, *nVariable;

                                                                assert(declaredVariables != NULL);

                                                                if (parseVariable(&cVariable, name, decl) == TRUE) {
                                                                    // Parse successful, insert the variable into the dictionary
                                                                    CVariable *oVariable;

                                                                    if (declaredVariables->erase(cVariable.name, oVariable)) {
                                                                        delete oVariable;
                                                                    };

                                                                    // Add the new variable into the variables list
                                                                    nVariable = new CVariable;
                                                                    nVariable[0] = cVariable;

                                                                    // Insert the variable into the variables trie
                                                                    declaredVariables->insert(nVariable->name, nVariable);
                                                                }
                                                            }

                                                            void CRibOut::declareDefaultVariables() {
                                                                // Define the options
                                                                declareVariable(RI_ARCHIVE, "string");
                                                                declareVariable(RI_PROCEDURAL, "string");
                                                                declareVariable(RI_TEXTURE, "string");
                                                                declareVariable(RI_SHADER, "string");
                                                                declareVariable(RI_DISPLAY, "string");
                                                                declareVariable(RI_RESOURCE, "string");

                                                                declareVariable(RI_BUCKETSIZE, "int[2]");
                                                                declareVariable(RI_METABUCKETS, "int[2]");
                                                                declareVariable(RI_INHERITATTRIBUTES, "int");
                                                                declareVariable(RI_GRIDSIZE, "int");
                                                                declareVariable(RI_EYESPLITS, "int");
                                                                declareVariable(RI_TEXTUREMEMORY, "int");
                                                                declareVariable(RI_BRICKMEMORY, "int");

                                                                declareVariable(RI_RADIANCECACHE, "int");
                                                                declareVariable(RI_JITTER, "float");
                                                                declareVariable(RI_FALSECOLOR, "int");
                                                                declareVariable(RI_EMIT, "int");
                                                                declareVariable(RI_DEPTHFILTER, "string");

                                                                declareVariable(RI_MAXDEPTH, "int");

                                                                declareVariable(RI_ENDOFFRAME, "int");
                                                                declareVariable(RI_FILELOG, "string");
                                                                declareVariable(RI_PROGRESS, "int");

                                                                // Define the attributes
                                                                declareVariable(RI_NUMPROBES, "int[2]");
                                                                declareVariable(RI_MINSUBDIVISION, "int");
                                                                declareVariable(RI_MAXSUBDIVISION, "int");
                                                                declareVariable(RI_MINSPLITS, "int");
                                                                declareVariable(RI_BOUNDEXPAND, "float");
                                                                declareVariable(RI_BINARY, "int");
                                                                declareVariable(RI_RASTERORIENT, "int");

                                                                declareVariable(RI_SPHERE, "float");
                                                                declareVariable(RI_COORDINATESYSYTEM, "string");

                                                                declareVariable(RI_DISPLACEMENTS, "int");
                                                                declareVariable(RI_BIAS, "float");
                                                                declareVariable(RI_MAXDIFFUSEDEPTH, "int");
                                                                declareVariable(RI_MAXSPECULARDEPTH, "int");
                                                                declareVariable(RI_SAMPLEMOTION, "int");

                                                                declareVariable(RI_HANDLE, "string");
                                                                declareVariable(RI_FILEMODE, "string");
                                                                declareVariable(RI_MAXERROR, "float");

                                                                declareVariable(RI_GLOBALMAP, "string");
                                                                declareVariable(RI_CAUSTICMAP, "string");
                                                                declareVariable(RI_SHADINGMODEL, "string");
                                                                declareVariable(RI_ESTIMATOR, "int");
                                                                declareVariable(RI_ILLUMINATEFRONT, "int");

                                                                declareVariable(RI_TRANSMISSION, "int");
                                                                declareVariable(RI_CAMERA, "int");
                                                                declareVariable(RI_SPECULAR, "int");
                                                                declareVariable(RI_DIFFUSE, "int");
                                                                declareVariable(RI_PHOTON, "int");

                                                                declareVariable(RI_DIFFUSEHITMODE, "string");
                                                                declareVariable(RI_SPECULARHITMODE, "string");
                                                                declareVariable(RI_TRANSMISSIONHITMODE, "string");
                                                                declareVariable(RI_CAMERAHITMODE, "string");

                                                                declareVariable(RI_NAME, "string");

                                                                declareVariable(RI_HIDDEN, "int");
                                                                declareVariable(RI_BACKFACING, "backfacing");

                                                                // File display variables
                                                                declareVariable("quantize", "float[4]");
                                                                declareVariable("dither", "float");
                                                                declareVariable("gamma", "float");
                                                                declareVariable("gain", "float");
                                                                declareVariable("near", "float");
                                                                declareVariable("far", "float");
                                                                declareVariable("Software", "string");
                                                                declareVariable("compression", "string");
                                                                declareVariable("NP", "float[16]");
                                                                declareVariable("Nl", "float[16]");

                                                                // Declare the rest
                                                                declareVariable("P", "global vertex point");
                                                                declareVariable("Ps", "global vertex point");
                                                                declareVariable("N", "global varying normal");
                                                                declareVariable("Ng", "global varying normal");
                                                                declareVariable("dPdu", "global vertex vector");
                                                                declareVariable("dPdv", "global vertex vector");
                                                                declareVariable("L", "global varying vector");
                                                                declareVariable("Cs", "global varying color");
                                                                declareVariable("Os", "global varying color");
                                                                declareVariable("Cl", "global varying color");
                                                                declareVariable("Ol", "global varying color");
                                                                declareVariable("Ci", "global varying color");
                                                                declareVariable("Oi", "global varying color");
                                                                declareVariable("s", "global varying float");
                                                                declareVariable("t", "global varying float");
                                                                declareVariable("st", "varying float[2]");
                                                                declareVariable("du", "global varying float");
                                                                declareVariable("dv", "global varying float");
                                                                declareVariable("u", "global varying float");
                                                                declareVariable("v", "global varying float");
                                                                declareVariable("I", "global varying vector");
                                                                declareVariable("E", "global varying point");
                                                                declareVariable("alpha", "global varying float");
                                                                declareVariable("time", "global varying float");
                                                                declareVariable("Pw", "global vertex htpoint");
                                                                declareVariable("Pz", "vertex float");
                                                                declareVariable("width", "vertex float");
                                                                declareVariable("constantwidth", "constant float");

                                                                // Define uniform variables
                                                                declareVariable("ncomps", "global uniform float");
                                                                declareVariable("dtime", "global uniform float");
                                                                declareVariable("Np", "uniform normal");

                                                                // Misc. variables
                                                                declareVariable("fov", "float");

                                                                // Standard RI variables
                                                                declareVariable("Ka", "float");
                                                                declareVariable("Kd", "float");
                                                                declareVariable("Kr", "float");
                                                                declareVariable("Ks", "float");
                                                                declareVariable("amplitude", "float");
                                                                declareVariable("background", "color");
                                                                declareVariable("beamdistribution", "float");
                                                                declareVariable("coneangle", "float");
                                                                declareVariable("conedeltangle", "float");
                                                                declareVariable("distance", "float");
                                                                declareVariable("from", "point");
                                                                declareVariable("intensity", "float");
                                                                declareVariable("lightcolor", "color");
                                                                declareVariable("maxdistance", "float");
                                                                declareVariable("mindistance", "float");
                                                                declareVariable("roughness", "float");
                                                                declareVariable("specularcolor", "color");
                                                                declareVariable("texturename", "string");
                                                                declareVariable("to", "point");
                                                            }
