/**
 * Project: openRender
 *
 * File: blobby.cpp
 *
 * Description:
 *   This file implements the functionality for blobby.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	blobby.cpp
//  Classes				:	-
//  Description			:	RiBlobby -> CPolygonMesh
//
////////////////////////////////////////////////////////////////////////
#include "blobby.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "atomic.h"
#include "blobbyField.h"
#include "blobbyPolygonize.h"
#include "error.h"
#include "pl.h"
#include "polygons.h"
#include "renderer.h"
#include "stats.h"
#include "xform.h"

///////////////////////////////////////////////////////////////////////
// Default fidelity (FR-025).
//
// A scene that never sets Attribute "blobby" "float tolerance" still has to
// render smoothly at typical framing, so the default is derived from the
// primitive's own geometry rather than being a fixed number that would be
// wrong at every scale but one.
//
// Two figures, because one is not enough. The overall extent sets the
// coarse scale, but a bounding box says nothing about how *thin* the
// surface inside it is: AppNote #31's 480-segment spiral spans some 14
// units while its tube is well under one, and sizing cells off the box
// alone would give three or four cells across the tube. So the default is
// also bounded by the smallest primitive field's own size, which is what
// actually sets the finest feature the surface can have.
///////////////////////////////////////////////////////////////////////
#define BLOBBY_CELLS_ACROSS_EXTENT 48
#define BLOBBY_CELLS_ACROSS_SMALLEST_FIELD 6

// Safety floor, so an absurd tolerance cannot ask for a lattice that
// exhausts memory before the polygonizer's own cell ceiling catches it.
#define BLOBBY_FINEST_CELLS_ACROSS_EXTENT 1500

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyExtentSpan
// Description			:	Largest dimension of the field extent
///////////////////////////////////////////////////////////////////////
static float blobbyExtentSpan(const CBlobbyProgram *program) {
    vector bmin, bmax;
    float span = 0;

    program->getExtent(bmin, bmax);

    for (int i = 0; i < 3; i++) {
        const float extent = bmax[i] - bmin[i];

        if (extent > span)
            span = extent;
    }

    return span;
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyDefaultCellSize
// Description			:	Cell size derived from the field extent
///////////////////////////////////////////////////////////////////////
float blobbyDefaultCellSize(const CBlobbyProgram *program) {
    if (program == NULL || !program->hasBoundedExtent())
        return 0;

    const float span = blobbyExtentSpan(program);

    if (!(span > 0))
        return 0;

    float cellSize = span / BLOBBY_CELLS_ACROSS_EXTENT;
    const float smallest = program->getSmallestFieldSize();

    if (smallest > 0) {
        const float finer = smallest / BLOBBY_CELLS_ACROSS_SMALLEST_FIELD;

        if (finer < cellSize)
            cellSize = finer;
    }

    const float floorSize = span / BLOBBY_FINEST_CELLS_ACROSS_EXTENT;

    if (cellSize < floorSize)
        cellSize = floorSize;

    return cellSize;
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyCellSizeFromTolerance
// Description			:	Validate an author-supplied tolerance and turn
//							it into a cell size.
// Comments				:	`tolerance` is the edge length of the
//							extraction lattice, in the primitive's own
//							object space. The mesh's deviation from the
//							true level set falls off roughly as its square,
//							so halving it is a real improvement in
//							fidelity and a fourfold cost in cells.
//
//							A negative value means the attribute was never
//							set, which is why CAttributes initialises it to
//							-1 rather than 0: an author who writes 0
//							explicitly deserves the diagnostic, and a
//							zero-initialised default would be
//							indistinguishable from that.
///////////////////////////////////////////////////////////////////////
float blobbyCellSizeFromTolerance(const CBlobbyProgram *program, float tolerance) {
    const float fallback = blobbyDefaultCellSize(program);

    if (tolerance < 0)
        return fallback;

    if (!(tolerance > 0)) {
        error(CODE_RANGE, "Blobby: tolerance must be greater than zero (got %g); using %g\n", tolerance, fallback);
        return fallback;
    }

    if (program == NULL || !program->hasBoundedExtent())
        return tolerance;

    const float span = blobbyExtentSpan(program);

    if (tolerance > span) {
        error(CODE_RANGE, "Blobby: tolerance %g is larger than the primitive's whole extent (%g); using %g\n", tolerance, span, fallback);
        return fallback;
    }

    const float finest = span / BLOBBY_FINEST_CELLS_ACROSS_EXTENT;

    if (tolerance < finest) {
        error(CODE_RANGE, "Blobby: tolerance %g would need more than %d cells across the primitive; using %g\n", tolerance, BLOBBY_FINEST_CELLS_ACROSS_EXTENT, finest);
        return finest;
    }

    return tolerance;
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyIsPerLeaf
// Description			:	TRUE for the two storage classes RISpec gives
//							one value per primitive field.
// Comments				:	facevarying has no meaning on a blobby -- there
//							are no faces in the declaration -- but a scene
//							can still declare it, so it is treated as
//							varying rather than rejected.
///////////////////////////////////////////////////////////////////////
static int blobbyIsPerLeaf(EVariableClass container) {
    return (container == CONTAINER_VERTEX) ||
           (container == CONTAINER_VARYING) ||
           (container == CONTAINER_FACEVARYING);
}

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyCreate
// Description			:	Extract the surface and wrap it in a
//							CPolygonMesh.
// Comments				:	Packing follows csgBuildMeshForAttributeGroup
//							(csgTree.cpp), with one deliberate difference:
//							the mesh keeps the blobby's own CXform rather
//							than an identity one, and its vertices stay in
//							object space. CSG needed identity transforms
//							because it merges operands declared in
//							different frames; a blobby has exactly one
//							frame, and keeping it is what makes an
//							instanced blobby -- the same declaration
//							inside ObjectBegin/ObjectEnd, used twice --
//							resolve at each instance's own transform.
//
//							Nothing here declares u, v, s or t. RISpec says
//							blobbies have no global parameterisation, and
//							the values shaders read come from the same
//							places they do for every other tessellated
//							primitive: u and v from the dicer, and s and t
//							defaulting to u and v in the shading engine's
//							complete(). Declaring our own would replace a
//							renderer-wide convention with an invented one,
//							which is exactly what FR-021 rules out.
///////////////////////////////////////////////////////////////////////
CObject *blobbyCreate(CAttributes *attributes, CXform *xform, const CBlobbyProgram *program, const CBlobbyProgram *programClose, int numParameters, const char **tokens, const void **parameters) {
    if (program == NULL || !program->isValid())
        return NULL;

    (void)programClose;

    atomicIncrement(&stats.numBlobbies);

    const int numLeaves = program->getNumLeaves();

    for (int i = 0; i < numLeaves; i++)
        atomicIncrement(&stats.numBlobbyLeaves);

    const float cellSize = blobbyCellSizeFromTolerance(program, attributes != NULL ? attributes->blobbyTolerance : -1);

    if (!(cellSize > 0))
        return NULL;

    // Per-blob parameters carry one value per primitive field, so a
    // declaration that disagrees with the code array -- as Pixar's own hand
    // example does, declaring 21 against 22 -- means the author's arrays are
    // shorter than the leaf count. Read the shorter of the two, so a
    // mismatch is a diagnostic and a slightly duller blend rather than a
    // read past the end (FR-017).
    int declaredLeaves = program->getDeclaredLeaves();

    if (declaredLeaves < 0)
        declaredLeaves = 0;

    const int availableLeaves = (declaredLeaves < numLeaves) ? declaredLeaves : numLeaves;

    CPl *source = NULL;

    if (numParameters > 0 && attributes != NULL) {
        source = parseParameterList(1, availableLeaves, availableLeaves, availableLeaves,
                                    numParameters, tokens, parameters, NULL, 0, attributes);
    }

    // Weights are only worth their cost if something actually consumes them.
    int wantWeights = FALSE;

    if (source != NULL && availableLeaves > 0) {
        for (int i = 0; i < source->numParameters; i++) {
            if (blobbyIsPerLeaf(source->parameters[i].container))
                wantWeights = TRUE;
        }
    }

    CBlobbyMesh *extracted = blobbyPolygonize(program, cellSize, wantWeights);

    if (extracted == NULL) {
        if (source != NULL)
            delete source;
        return NULL;
    }

    if (extracted->numVertices == 0 || extracted->numTriangles == 0) {
        delete extracted;
        if (source != NULL)
            delete source;
        return NULL;
    }

    const int numVertices = extracted->numVertices;
    const int numTriangles = extracted->numTriangles;

    // Size the output: P, then N, then one entry per author parameter.
    // Per-leaf parameters become one value per *vertex*; constant and
    // uniform ones keep their single value for the whole primitive
    // (FR-018).
    int numOutParameters = 2;
    int dataSize = numVertices * 3 * 2;

    if (source != NULL) {
        for (int i = 0; i < source->numParameters; i++) {
            const CPlParameter *sourceParameter = source->parameters + i;
            const CVariable *variable = sourceParameter->variable;

            if (blobbyIsPerLeaf(sourceParameter->container)) {
                if (variable->type == TYPE_STRING) {
                    // A per-blob string cannot be blended -- there is no
                    // meaningful mixture of two names -- so it is dropped
                    // with a diagnostic rather than silently mangled.
                    error(CODE_BADTOKEN, "Blobby: per-blob parameter \"%s\" is a string and cannot be blended between blobs; ignoring it\n", variable->name);
                    continue;
                }

                if (extracted->weights == NULL)
                    continue;

                dataSize += numVertices * variable->numFloats;
            }
            else {
                if (variable->type == TYPE_STRING)
                    dataSize += variable->numFloats * (int)(sizeof(char *) / sizeof(float));
                else
                    dataSize += variable->numFloats;
            }

            dataSize += dataSize & 1;
            numOutParameters++;
        }
    }

    // P must live at index 0: CPolygonMesh derives its object-space bound by
    // reading the first three floats of every vertex straight out of
    // pl->data0.
    float *data0 = new float[dataSize];

    memcpy(data0, extracted->P, sizeof(float) * numVertices * 3);
    memcpy(data0 + numVertices * 3, extracted->N, sizeof(float) * numVertices * 3);

    CVariable *variableP = CRenderer::retrieveVariable("P");
    CVariable *variableN = CRenderer::retrieveVariable("N");

    CPlParameter *plParameters = new CPlParameter[numOutParameters];

    plParameters[0].variable = variableP;
    plParameters[0].numItems = numVertices;
    plParameters[0].index = 0;
    plParameters[0].container = CONTAINER_VERTEX;
    plParameters[1].variable = variableN;
    plParameters[1].numItems = numVertices;
    plParameters[1].index = numVertices * 3;
    plParameters[1].container = CONTAINER_VERTEX;

    int outIndex = 2;
    int cursor = numVertices * 3 * 2;

    if (source != NULL) {
        for (int i = 0; i < source->numParameters; i++) {
            const CPlParameter *sourceParameter = source->parameters + i;
            const CVariable *variable = sourceParameter->variable;
            const int numFloats = variable->numFloats;

            if (blobbyIsPerLeaf(sourceParameter->container)) {
                if (variable->type == TYPE_STRING || extracted->weights == NULL)
                    continue;

                const float *values = source->data0 + sourceParameter->index;
                const int leaves = sourceParameter->numItems;

                for (int v = 0; v < numVertices; v++) {
                    const float *weights = extracted->weights + v * extracted->numLeaves;
                    float *destination = data0 + cursor + v * numFloats;
                    float total = 0;

                    for (int c = 0; c < numFloats; c++)
                        destination[c] = 0;

                    for (int leaf = 0; leaf < leaves; leaf++) {
                        const float weight = weights[leaf];

                        if (weight == 0)
                            continue;

                        total += weight;

                        for (int c = 0; c < numFloats; c++)
                            destination[c] += weight * values[leaf * numFloats + c];
                    }

                    // Renormalize over the leaves that actually had values.
                    // Without this an nleaf mismatch would darken the blend
                    // towards zero near the leaves whose values are missing,
                    // rather than simply ignoring them.
                    if (total > 0 && total < 1) {
                        const float inverse = 1 / total;

                        for (int c = 0; c < numFloats; c++)
                            destination[c] *= inverse;
                    }
                    else if (total == 0 && leaves > 0) {
                        for (int c = 0; c < numFloats; c++)
                            destination[c] = values[c];
                    }
                }

                plParameters[outIndex].variable = (CVariable *)variable;
                plParameters[outIndex].numItems = numVertices;
                plParameters[outIndex].index = cursor;
                plParameters[outIndex].container = CONTAINER_VERTEX;

                cursor += numVertices * numFloats;
            }
            else {
                // Constant and uniform values apply to the whole primitive
                // with no per-blob variation, so they pass straight through
                // (FR-018, US4 scenario 3).
                const int width = (variable->type == TYPE_STRING)
                                      ? numFloats * (int)(sizeof(char *) / sizeof(float))
                                      : numFloats;

                memcpy(data0 + cursor, source->data0 + sourceParameter->index, sizeof(float) * width);

                plParameters[outIndex].variable = (CVariable *)variable;
                plParameters[outIndex].numItems = 1;
                plParameters[outIndex].index = cursor;
                plParameters[outIndex].container = sourceParameter->container;

                cursor += width;
            }

            cursor += cursor & 1;
            outIndex++;
        }

        delete source;
    }

    CPl *pl = new CPl(dataSize, outIndex, plParameters, data0);

    int *nholes = new int[numTriangles];
    int *nvertices = new int[numTriangles];
    int *vertices = new int[numTriangles * 3];

    for (int i = 0; i < numTriangles; i++) {
        nholes[i] = 1;
        nvertices[i] = 3;
    }

    memcpy(vertices, extracted->triangles, sizeof(int) * numTriangles * 3);

    CObject *result = new CPolygonMesh(attributes, xform, pl, numTriangles, nholes, nvertices, vertices);

    delete[] nholes;
    delete[] nvertices;
    delete[] vertices;
    delete extracted;

    return result;
}
