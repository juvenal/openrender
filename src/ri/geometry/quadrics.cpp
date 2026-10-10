/**
 * Project: openRender
 *
 * File: quadrics.cpp
 *
 * Description:
 *   This file implements the functionality for quadrics.
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
//  File				:	quadrics.cpp
//  Classes				:	CSphere
//							CDisk
//							CCone ...
//  Description			:	Implementation
//
//	FIXME: The quadrics implementation has not been heavily optimized
//	FIXME: Re-optimize
////////////////////////////////////////////////////////////////////////
#include <math.h>

#include <algorithm>

#include "common/os.h"
#include "common/polynomial.h"
#include "common/memory.h"
#include "object.h"
#include "objectMisc.h"
#include "patchUtils.h"
#include "quadrics.h"
#include "renderer.h"
#include "rendererContext.h"
#include "shading.h"
#include "common/stats.h"
#include "surface.h"

#define transformPoints()                                                                                                                \
    if ((xform->next != NULL) && (!(up & (PARAMETER_BEGIN_SAMPLE | PARAMETER_END_SAMPLE)))) {                                            \
                                                                                                                                         \
        transform(&varying[VARIABLE_P][start * 3], numVertices, xform->from, xform->next->from, &varying[VARIABLE_TIME][start]);         \
                                                                                                                                         \
        if (up & PARAMETER_DPDU) {                                                                                                       \
            vtransform(&varying[VARIABLE_DPDU][start * 3], numVertices, xform->from, xform->next->from, &varying[VARIABLE_TIME][start]); \
        }                                                                                                                                \
                                                                                                                                         \
        if (up & PARAMETER_DPDV) {                                                                                                       \
            vtransform(&varying[VARIABLE_DPDV][start * 3], numVertices, xform->from, xform->next->from, &varying[VARIABLE_TIME][start]); \
        }                                                                                                                                \
                                                                                                                                         \
        if (up & PARAMETER_NG) {                                                                                                         \
            ntransform(&varying[VARIABLE_NG][start * 3], numVertices, xform->to, xform->next->to, &varying[VARIABLE_TIME][start]);       \
        }                                                                                                                                \
    }                                                                                                                                    \
    else {                                                                                                                               \
        float *from = xform->from;                                                                                                       \
        float *to = xform->to;                                                                                                           \
                                                                                                                                         \
        if ((up & PARAMETER_END_SAMPLE) && (xform->next != NULL)) {                                                                      \
            from = xform->next->from;                                                                                                    \
            to = xform->next->to;                                                                                                        \
        }                                                                                                                                \
                                                                                                                                         \
        transform(&varying[VARIABLE_P][start * 3], numVertices, from);                                                                   \
                                                                                                                                         \
        if (up & PARAMETER_DPDU) {                                                                                                       \
            vtransform(&varying[VARIABLE_DPDU][start * 3], numVertices, from);                                                           \
        }                                                                                                                                \
                                                                                                                                         \
        if (up & PARAMETER_DPDV) {                                                                                                       \
            vtransform(&varying[VARIABLE_DPDV][start * 3], numVertices, from);                                                           \
        }                                                                                                                                \
                                                                                                                                         \
        if (up & PARAMETER_NG) {                                                                                                         \
            ntransform(&varying[VARIABLE_NG][start * 3], numVertices, to);                                                               \
        }                                                                                                                                \
    }

///////////////////////////////////////////////////////////////////////
// Function				:	regionKeepsHit
// Description			:	Does the surface's region (a spatial trim, e.g. a
//							CSG solid's boundary rule) keep this ray hit?
// Return Value			:	TRUE when there is no region or it accepts the
//							camera-space hit point
// Comments				:	The root loops run in object space: the point is
//							taken on the camera-space ray, the normal moved to
//							camera space the way an accepted hit's is (spec 021)
static int regionKeepsHit(const CSurface *surface, CShadingContext *context, const CRay *rv, double t, const float *objectNormal) {
    if (surface->region == NULL)
        return TRUE;
    vector P, N;
    for (int k = 0; k < 3; k++)
        P[k] = rv->from[k] + rv->dir[k] * (float)t;
    mulmn(N, surface->xform->to, objectNormal);
    return surface->region->accepts(context, P, N);
}

//////////////////////////////////////////// S P H E R E ///////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Method				:	CSphere
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CSphere::CSphere(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float ra, float vmina, float vmaxa, float anglea) : CSurface(a, x) {
    atomicIncrement(stats.numGprims);

    r = ra;
    vmin = vmina;
    vmax = vmaxa;
    umax = anglea;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = NULL;

    computeObjectBound(bmin, bmax, r, vmin, vmax, umax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Method				:	CSphere
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CSphere::CSphere(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float r0, float vmin0, float vmax0, float angle0, float r1, float vmin1, float vmax1, float angle1) : CSurface(a, x) {
    vector tbmin, tbmax;

    atomicIncrement(stats.numGprims);

    r = r0;
    vmin = vmin0;
    vmax = vmax0;
    umax = angle0;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = new float[4];
    nextData[0] = r1;
    nextData[1] = vmin1;
    nextData[2] = vmax1;
    nextData[3] = angle1;

    computeObjectBound(bmin, bmax, r, vmin, vmax, umax);
    computeObjectBound(tbmin, tbmax, nextData[0], nextData[1], nextData[2], nextData[3]);
    addBox(bmin, bmax, tbmin);
    addBox(bmin, bmax, tbmax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Method				:	~CSphere
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CSphere::~CSphere() {
    atomicDecrement(stats.numGprims);

    if (parameters != NULL)
        delete parameters;
    if (nextData != NULL)
        delete[] nextData;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Method				:	intersect
// Description			:	Compute the intersection with a ray
// Return Value			:	TRUE if intersects
// Comments				:
void CSphere::intersect(CShadingContext *context, CRay *rv) {
    unsigned int ns, i;
    double s[2];
    double r, umax, vmin, vmax;

    if (checkRayGuard(rv, context))
        return;

    vector oFrom, oDir;
    transform(oFrom, oDir, xform, rv);

    if (nextData != NULL) {
        r = (float)(this->r * (1.0 - rv->time) + nextData[0] * (double)rv->time);
        vmin = (float)(this->vmin * (1.0 - rv->time) + nextData[1] * (double)rv->time);
        vmax = (float)(this->vmax * (1.0 - rv->time) + nextData[2] * (double)rv->time);
        umax = (float)(this->umax * (1.0 - rv->time) + nextData[3] * (double)rv->time);
    }
    else {
        r = this->r;
        vmin = this->vmin;
        vmax = this->vmax;
        umax = this->umax;
    }

    const double a = dotvv(oDir, oDir);
    const double b = 2 * dotvv(oDir, oFrom);
    const double c = dotvv(oFrom, oFrom) - r * r;

    if ((ns = solveQuadric<double>(a, b, c, s)) == 0)
        return;

    for (i = 0; i < ns; i++) {
        double P[3];
        float t;
        double u, v;
        vector tmp;

        t = (float)s[i];

        if (t <= rv->tmin)
            continue;
        if (t >= rv->t)
            return;

        P[0] = oDir[0] * t + oFrom[0];
        P[1] = oDir[1] * t + oFrom[1];
        P[2] = oDir[2] * t + oFrom[2];

        if (r > 0) {
            u = atan2(P[COMP_Y], P[COMP_X]);
            if (P[COMP_Z] < -r)
                P[COMP_Z] = -r;
            if (P[COMP_Z] > r)
                P[COMP_Z] = r;
        }
        else {
            u = atan2(-P[COMP_Y], -P[COMP_X]);
            if (P[COMP_Z] < r)
                P[COMP_Z] = r;
            if (P[COMP_Z] > -r)
                P[COMP_Z] = -r;
        }

        if (u < 0)
            u = 2 * C_PI + u;

        if (umax < 0) {
            u = u - 2 * C_PI;
            if (u < umax)
                continue;
        }
        else {
            if (u > umax)
                continue;
        }

        v = asin(P[COMP_Z] / r);
        v = (v - vmin) / (vmax - vmin);
        if (v < 0)
            continue;
        if (v > 1)
            continue;

        const double f = r * umax * (vmax - vmin);
        P[0] *= f;
        P[1] *= f;
        P[2] *= f;

        if ((attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ xform->flip) {
            P[0] = -P[0];
            P[1] = -P[1];
            P[2] = -P[2];
        }

        if (!(attributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED)) {
            if ((oDir[0] * P[0] + oDir[1] * P[1] + oDir[2] * P[2]) > 0)
                continue;
        }

        tmp[0] = (float)P[0];
        tmp[1] = (float)P[1];
        tmp[2] = (float)P[2];

        if (!regionKeepsHit(this, context, rv, t, tmp))
            continue;

        rv->object = this;
        rv->u = (float)(u / umax);
        rv->v = (float)v;
        rv->t = (float)t;
        mulmn(rv->N, xform->to, tmp);
        return;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Method				:	sample
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CSphere::sample(int start, int numVertices, float **varying, float ***, unsigned int &up) const {
    const float *u = varying[VARIABLE_U] + start;
    const float *v = varying[VARIABLE_V] + start;
    float *sinu = (float *)alloca(numVertices * 4 * sizeof(float));
    float *cosu = sinu + numVertices;
    float *sinv = cosu + numVertices;
    float *cosv = sinv + numVertices;

    if ((nextData != NULL) && (!(up & (PARAMETER_BEGIN_SAMPLE | PARAMETER_END_SAMPLE)))) {
        float *r = (float *)alloca(numVertices * 4 * sizeof(float));
        float *umax = r + numVertices;
        float *vmax = umax + numVertices;
        float *vmin = vmax + numVertices;
        const float *time = varying[VARIABLE_TIME] + start;
        float *dest = varying[VARIABLE_P] + start * 3;

        // Precompute sinu cosu sinv and cosv
        for (int currentVertex = 0; currentVertex < numVertices; ++currentVertex) {
            const double ctime = *time++;
            const float cumax = umax[currentVertex] = (float)(this->umax * (1.0 - ctime) + nextData[3] * ctime);
            const float cvmax = vmax[currentVertex] = (float)(this->vmax * (1.0 - ctime) + nextData[2] * ctime);
            const float cvmin = vmin[currentVertex] = (float)(this->vmin * (1.0 - ctime) + nextData[1] * ctime);
            r[currentVertex] = (float)(this->r * (1.0 - ctime) + nextData[0] * (double)ctime);

            // Precompute sinu cosu sinv and cosv
            const double cu = u[currentVertex];
            const double cv = v[currentVertex];
            const float uAngle = (float)(cumax * cu);
            const float vAngle = (float)(cvmax * cv + cvmin * (1.0 - cv));
            sinu[currentVertex] = sinf(uAngle);
            cosu[currentVertex] = cosf(uAngle);
            sinv[currentVertex] = sinf(vAngle);
            cosv[currentVertex] = cosf(vAngle);

            const float tmp = r[currentVertex] * cosv[currentVertex];
            dest[COMP_Z] = r[currentVertex] * sinv[currentVertex];
            dest[COMP_X] = tmp * cosu[currentVertex];
            dest[COMP_Y] = tmp * sinu[currentVertex];
            dest += 3;
        }

        // Set dPdu
        if (up & PARAMETER_DPDU) {
            const float *src = varying[VARIABLE_P] + start * 3;
            dest = varying[VARIABLE_DPDU] + start * 3;

            for (int currentVertex = 0; currentVertex < numVertices; ++currentVertex) {
                dest[COMP_X] = -umax[currentVertex] * src[COMP_Y];
                dest[COMP_Y] = umax[currentVertex] * src[COMP_X];
                dest[COMP_Z] = 0;
                dest += 3;
                src += 3;
            }
        }

        // Set dPdv
        if (up & PARAMETER_DPDV) {
            dest = varying[VARIABLE_DPDV] + start * 3;

            for (int currentVertex = 0; currentVertex < numVertices; ++currentVertex) {
                dest[COMP_X] = -r[currentVertex] * cosu[currentVertex] * sinv[currentVertex] * (vmax[currentVertex] - vmin[currentVertex]);
                dest[COMP_Y] = -r[currentVertex] * sinu[currentVertex] * sinv[currentVertex] * (vmax[currentVertex] - vmin[currentVertex]);
                dest[COMP_Z] = r[currentVertex] * cosv[currentVertex] * (vmax[currentVertex] - vmin[currentVertex]);
                dest += 3;
            }
        }
    }
    else {
        float r = this->r;
        float umax = this->umax;
        float vmin = this->vmin;
        float vmax = this->vmax;

        if ((up & PARAMETER_END_SAMPLE) && (nextData != NULL)) {
            r = nextData[0];
            vmin = nextData[1];
            vmax = nextData[2];
            umax = nextData[3];
        }

        // Precompute sinu cosu sinv and cosv
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int currentVertex = 0; currentVertex < numVertices; ++currentVertex) {
            const float uAngle = (float)(umax * (double)u[currentVertex]);
            const float vAngle = (float)(vmax * (double)v[currentVertex] + vmin * (1.0 - v[currentVertex]));
            sinu[currentVertex] = sinf(uAngle);
            cosu[currentVertex] = cosf(uAngle);
            sinv[currentVertex] = sinf(vAngle);
            cosv[currentVertex] = cosf(vAngle);

            const float tmp = r * cosv[currentVertex];
            dest[COMP_Z] = r * sinv[currentVertex];
            dest[COMP_X] = tmp * cosu[currentVertex];
            dest[COMP_Y] = tmp * sinu[currentVertex];
            dest += 3;
        }

        // Set dPdu
        if (up & PARAMETER_DPDU) {
            const float *src = varying[VARIABLE_P] + start * 3;
            dest = varying[VARIABLE_DPDU] + start * 3;

            for (int currentVertex = 0; currentVertex < numVertices; ++currentVertex) {
                dest[COMP_X] = -umax * src[COMP_Y];
                dest[COMP_Y] = umax * src[COMP_X];
                dest[COMP_Z] = 0;
                dest += 3;
                src += 3;
            }
        }

        // Set dPdv
        if (up & PARAMETER_DPDV) {
            dest = varying[VARIABLE_DPDV] + start * 3;

            for (int currentVertex = 0; currentVertex < numVertices; ++currentVertex) {
                dest[COMP_X] = -r * cosu[currentVertex] * sinv[currentVertex] * (vmax - vmin);
                dest[COMP_Y] = -r * sinu[currentVertex] * sinv[currentVertex] * (vmax - vmin);
                dest[COMP_Z] = r * cosv[currentVertex] * (vmax - vmin);
                dest += 3;
            }
        }
    }

    // Set Ng
    if (up & PARAMETER_NG) {
        float *N = varying[VARIABLE_NG] + start * 3;
        const float *P = varying[VARIABLE_P] + start * 3;
        float f = r * umax * (vmax - vmin);

        if (xform->flip)
            f *= -1;

        for (int i = numVertices; i > 0; i--, N += 3, P += 3) {
            mulvf(N, P, f);
        }
    }

    // Set dPdtime
    if (up & PARAMETER_DPDTIME) {
        // This is where we'll store the result
        float *dest = varying[VARIABLE_DPDTIME] + start * 3;

        // Are we moving?
        if (nextData != NULL) {
            // Get the xform matrices to move to the camera system
            const float *fromStart = xform->from;
            const float *fromEnd = (xform->next != NULL ? xform->next->from : xform->from);

            // Get the u and v
            assert(u == (varying[VARIABLE_U] + start));
            assert(v == (varying[VARIABLE_V] + start));

            // Compute the xforms
            for (int currentVertex = 0; currentVertex < numVertices; ++currentVertex, dest += 3) {
                const float uAngleStart = (float)(umax * (double)u[currentVertex]);
                const float vAngleStart = (float)(vmax * (double)v[currentVertex] + vmin * (1.0 - v[currentVertex]));
                const float rStart = r;
                const float uAngleEnd = (float)(nextData[3] * (double)u[currentVertex]);
                const float vAngleEnd = (float)(nextData[2] * (double)v[currentVertex] + nextData[1] * (1.0 - v[currentVertex]));
                const float rEnd = nextData[0];
                vector Pstart, Pend;

                // Compute the position at the beginning
                Pstart[COMP_Z] = rStart * sinf(vAngleStart);
                Pstart[COMP_X] = rStart * cosf(vAngleStart) * cosf(uAngleStart);
                Pstart[COMP_Y] = rStart * cosf(vAngleStart) * sinf(uAngleStart);
                mulmp(Pstart, fromStart, Pstart);

                // Compute the position at the end
                Pend[COMP_Z] = rEnd * sinf(vAngleEnd);
                Pend[COMP_X] = rEnd * cosf(vAngleEnd) * cosf(uAngleEnd);
                Pend[COMP_Y] = rEnd * cosf(vAngleEnd) * sinf(uAngleEnd);
                mulmp(Pend, fromEnd, Pend);

                // Compute the dPdtime
                subvv(dest, Pend, Pstart);
                mulvf(dest, CRenderer::invShutterTime);
            }
        }
        else {
            // Get the xform matrices
            const float *fromStart = xform->from + element(0, 3);
            const float *fromEnd = (xform->next != NULL ? xform->next->from + element(0, 3) : xform->from + element(0, 3));
            vector D;
            subvv(D, fromEnd, fromStart);
            mulvf(D, CRenderer::invShutterTime);

            // dPdtime is zero
            for (int i = 0; i < numVertices; ++i, dest += 3)
                movvv(dest, D);
        }
    }

    // Transform the points
    transformPoints();

    // Turn off the parameters we computed
    up &= ~parametersF;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Method				:	interpolate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CSphere::interpolate(int numVertices, float **varying, float ***locals) const {
    if (parameters != NULL)
        parameters->dispatch(numVertices, varying, locals);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CSphere::instantiate(CAttributes *a, CXform *x, CRiInterface *cx) const {
    CXform *nx = new CXform(x);

    nx->concat(xform); // Concetenate the local xform

    if (a == NULL)
        a = attributes;

    if (nextData == NULL)
        cx->addObject(new CSphere(a, nx, (parameters == NULL ? NULL : parameters->clone(a)), parametersF, r, vmin, vmax, umax));
    else
        cx->addObject(new CSphere(a, nx, (parameters == NULL ? NULL : parameters->clone(a)), parametersF, r, vmin, vmax, umax, nextData[0], nextData[1], nextData[2], nextData[3]));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Method				:	computeObjectBound
// Description			:	Compute the bounding box in the object space
// Return Value			:	-
// Comments				:
void CSphere::computeObjectBound(float *bmin, float *bmax, float r, float vmin, float vmax, float) {
    float maxRadius;
    float vmi;
    if (vmax < vmin) {
        vmi = vmax;
    }
    else {
        vmi = vmin;
    }
    float vma;
    if (vmax > vmin) {
        vma = vmax;
    }
    else {
        vma = vmin;
    }
    float zmin;
    float zmax;

    r = absf(r);

    zmin = r * sinf(vmin);
    zmax = r * sinf(vmax);

    if (vmi > 0) {
        maxRadius = (float)cos(vmi) * r;
    }
    else if (vma < 0) {
        maxRadius = (float)cos(vma) * r;
    }
    else {
        maxRadius = r;
    }

    float minZ;
    if (zmax < zmin) {
        minZ = zmax;
    }
    else {
        minZ = zmin;
    }
    float maxZ;
    if (zmax > zmin) {
        maxZ = zmax;
    }
    else {
        maxZ = zmin;
    }
    initv(bmin, -maxRadius, -maxRadius, minZ);
    initv(bmax, maxRadius, maxRadius, maxZ);
}

//////////////////////////////////////////// D I S K ///////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Method				:	CDisk
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDisk::CDisk(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float ra, float za, float anglea) : CSurface(a, x) {
    atomicIncrement(stats.numGprims);

    r = ra;
    z = za;
    umax = anglea;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = NULL;

    computeObjectBound(bmin, bmax, r, z, umax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Method				:	CDisk
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CDisk::CDisk(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float r0, float z0, float angle0, float r1, float z1, float angle1) : CSurface(a, x) {
    vector tbmin, tbmax;

    atomicIncrement(stats.numGprims);

    r = r0;
    z = z0;
    umax = angle0;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = new float[3];
    nextData[0] = r1;
    nextData[1] = z1;
    nextData[2] = angle1;

    computeObjectBound(bmin, bmax, r, z, umax);
    computeObjectBound(tbmin, tbmax, nextData[0], nextData[1], nextData[2]);
    addBox(bmin, bmax, tbmin);
    addBox(bmin, bmax, tbmax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Method				:	~CDisk
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CDisk::~CDisk() {
    atomicDecrement(stats.numGprims);

    if (parameters != NULL)
        delete parameters;
    if (nextData != NULL)
        delete[] nextData;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Method				:	intersect
// Description			:	Compute the intersection with a ray
// Return Value			:	TRUE if intersects
// Comments				:
void CDisk::intersect(CShadingContext *context, CRay *rv) {
    float t;
    double u;
    double x, y;
    double r, z, umax;
    vector Nt;

    if (checkRayGuard(rv, context))
        return;

    vector oFrom, oDir;
    transform(oFrom, oDir, xform, rv);

    // Convert to the object coordinate system
    if (nextData != NULL) {
        r = (float)(this->r * (1.0 - (double)rv->time) + nextData[0] * (double)rv->time);
        z = (float)(this->z * (1.0 - (double)rv->time) + nextData[1] * (double)rv->time);
        umax = (float)(this->umax * (1.0 - (double)rv->time) + nextData[2] * (double)rv->time);
    }
    else {
        r = this->r;
        z = this->z;
        umax = this->umax;
    }

    t = (float)(z - oFrom[COMP_Z]) / oDir[COMP_Z];

    if ((t <= rv->tmin) || (t >= rv->t))
        return;

    x = oFrom[COMP_X] + oDir[COMP_X] * t;
    y = oFrom[COMP_Y] + oDir[COMP_Y] * t;
    if ((x * x + y * y) > r * r)
        return;

    if (r < 0)
        u = atan2(-y, -x);
    else
        u = atan2(y, x);

    if (u < 0)
        u = 2 * C_PI + u;

    if (umax < 0) {
        u = u - 2 * C_PI;
        if (u < umax)
            return;
    }
    else {
        if (u > umax)
            return;
    }

    initv(Nt, 0, 0, (float)umax);

    if ((attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ xform->flip) {
        mulvf(Nt, -1);
    }

    if (!(attributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED)) {
        if (dotvv(oDir, Nt) > 0) {
            return;
        }
    }

    if (!regionKeepsHit(this, context, rv, t, Nt))
        return;

    rv->object = this;
    rv->u = (float)(u / umax);
    if (r > 0) {
        rv->v = (float)(((r - sqrt(x * x + y * y))) / r);
    }
    else {
        rv->v = (float)(((r + sqrt(x * x + y * y))) / r);
    }
    rv->t = (float)t;
    mulmn(rv->N, xform->to, Nt);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Method				:	sample
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CDisk::sample(int start, int numVertices, float **varying, float ***, unsigned int &up) const {
    const float *u = varying[VARIABLE_U] + start;
    const float *v = varying[VARIABLE_V] + start;

    // Allocate memory for precomputing sin(u) , cos(u)
    float *sinu = (float *)alloca(numVertices * 2 * sizeof(float));
    float *cosu = sinu + numVertices;

    if ((nextData != NULL) && (!(up & (PARAMETER_BEGIN_SAMPLE | PARAMETER_END_SAMPLE)))) {
        float *r = (float *)alloca(numVertices * 3 * sizeof(float));
        float *z = r + numVertices;
        float *umax = z + numVertices;
        const float *time = varying[VARIABLE_TIME] + start;
        float *dest = varying[VARIABLE_P] + start * 3;

        // Precompute sinu cosu sinv and cosv
        for (int i = 0; i < numVertices; ++i, dest += 3) {
            const double ctime = *time++;

            r[i] = (float)(this->r * (1.0 - ctime) + nextData[0] * ctime);
            z[i] = (float)(this->z * (1.0 - ctime) + nextData[1] * ctime);
            umax[i] = (float)(this->umax * (1.0 - ctime) + nextData[2] * ctime);
            cosu[i] = cosf(u[i] * umax[i]);
            sinu[i] = sinf(u[i] * umax[i]);

            dest[COMP_X] = (float)(r[i] * (1.0 - (double)v[i]) * cosu[i]);
            dest[COMP_Y] = (float)(r[i] * (1.0 - (double)v[i]) * sinu[i]);
            dest[COMP_Z] = z[i];
        }

        // Set dPdu/dPdv if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                dPdu[COMP_X] = (float)(-umax[i] * r[i] * (1.0 - (double)v[i]) * sinu[i]);
                dPdu[COMP_Y] = (float)(umax[i] * r[i] * (1.0 - (double)v[i]) * cosu[i]);
                dPdu[COMP_Z] = 0;
                dPdv[COMP_X] = -r[i] * cosu[i];
                dPdv[COMP_Y] = -r[i] * sinu[i];
                dPdv[COMP_Z] = 0;
                dPdu += 3;
                dPdv += 3;
            }
        }
    }
    else {
        float r = this->r;
        float z = this->z;
        float umax = this->umax;

        if ((up & PARAMETER_END_SAMPLE) && (nextData != NULL)) {
            r = nextData[0];
            z = nextData[1];
            umax = nextData[2];
        }

        // Precompute the sin/cos for u points
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int i = 0; i < numVertices; ++i, dest += 3) {
            cosu[i] = cosf(u[i] * umax);
            sinu[i] = sinf(u[i] * umax);
            dest[COMP_X] = (float)(r * (1.0 - (double)v[i]) * cosu[i]);
            dest[COMP_Y] = (float)(r * (1.0 - (double)v[i]) * sinu[i]);
            dest[COMP_Z] = z;
        }

        // Set dPdu/dPdv if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                dPdu[COMP_X] = (float)(-umax * r * (1.0 - (double)v[i]) * sinu[i]);
                dPdu[COMP_Y] = (float)(umax * r * (1.0 - (double)v[i]) * cosu[i]);
                dPdu[COMP_Z] = 0;
                dPdv[COMP_X] = -r * cosu[i];
                dPdv[COMP_Y] = -r * sinu[i];
                dPdv[COMP_Z] = 0;
                dPdu += 3;
                dPdv += 3;
            }
        }
    }

    // Set Ng if needed
    if (up & PARAMETER_NG) {
        float *Ng = varying[VARIABLE_NG] + start * 3;
        float f = umax;

        if (xform->flip)
            f = -umax;

        for (int i = numVertices; i > 0; i--, Ng += 3) {
            initv(Ng, 0, 0, f);
        }
    }

    // Set dPdtime
    if (up & PARAMETER_DPDTIME) {
        // This is where we'll store the result
        float *dest = varying[VARIABLE_DPDTIME] + start * 3;

        // Are we moving?
        if (nextData != NULL) {
            // Get the xform matrices to move to the camera system
            const float *fromStart = xform->from;
            const float *fromEnd = (xform->next != NULL ? xform->next->from : xform->from);

            // Obtain the u/v
            assert(u == (varying[VARIABLE_U] + start));
            assert(v == (varying[VARIABLE_V] + start));

            // Compute the xforms
            for (int i = 0; i < numVertices; ++i, dest += 3) {
                const float uAngleStart = umax;
                const float zStart = z;
                const float rStart = r;
                const float uAngleEnd = nextData[2];
                const float zEnd = nextData[1];
                const float rEnd = nextData[0];
                vector Pstart, Pend;

                // Compute the position at the beginning
                Pstart[COMP_X] = (float)(rStart * (1.0 - (double)v[i]) * cosf(u[i] * uAngleStart));
                Pstart[COMP_Y] = (float)(rStart * (1.0 - (double)v[i]) * sinf(u[i] * uAngleStart));
                Pstart[COMP_Z] = zStart;
                mulmp(Pstart, fromStart, Pstart);

                // Compute the position at the end
                Pend[COMP_X] = (float)(rEnd * (1.0 - (double)v[i]) * cosf(u[i] * uAngleEnd));
                Pend[COMP_Y] = (float)(rEnd * (1.0 - (double)v[i]) * sinf(u[i] * uAngleEnd));
                Pend[COMP_Z] = zEnd;
                mulmp(Pend, fromEnd, Pend);

                // Compute the dPdtime
                subvv(dest, Pend, Pstart);
                mulvf(dest, CRenderer::invShutterTime);
            }
        }
        else {
            // Get the xform matrices
            const float *fromStart = xform->from + element(0, 3);
            const float *fromEnd = (xform->next != NULL ? xform->next->from + element(0, 3) : xform->from + element(0, 3));
            vector D;
            subvv(D, fromEnd, fromStart);
            mulvf(D, CRenderer::invShutterTime);

            // dPdtime is zero
            for (int i = 0; i < numVertices; ++i, dest += 3)
                movvv(dest, D);
        }
    }

    // Transform the points
    transformPoints();

    // Turn off the parameters we computed
    up &= ~parametersF;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Method				:	interpolate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CDisk::interpolate(int numVertices, float **varying, float ***locals) const {
    if (parameters != NULL)
        parameters->dispatch(numVertices, varying, locals);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CDisk::instantiate(CAttributes *a, CXform *x, CRiInterface *cx) const {
    CXform *nx = new CXform(x);

    nx->concat(xform); // Concetenate the local xform

    if (a == NULL)
        a = attributes;

    if (nextData == NULL)
        cx->addObject(new CDisk(a, nx, parameters, parametersF, r, z, umax));
    else
        cx->addObject(new CDisk(a, nx, parameters, parametersF, r, z, umax, nextData[0], nextData[1], nextData[2]));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Method				:	computeObjectBound
// Description			:	Compute the bounding box in the object space
// Return Value			:	-
// Comments				:
void CDisk::computeObjectBound(float *bmin, float *bmax, float r, float z, float) {
    r = absf(r);

    initv(bmin, -r, -r, z);
    initv(bmax, r, r, z);
}

//////////////////////////////////////////// C O N E ///////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Method				:	CCone
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CCone::CCone(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float ra, float heighta, float anglea) : CSurface(a, x) {
    atomicIncrement(stats.numGprims);

    r = ra;
    height = heighta;
    umax = anglea;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = NULL;

    computeObjectBound(bmin, bmax, r, height, umax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Method				:	CCone
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CCone::CCone(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float r0, float height0, float angle0, float r1, float height1, float angle1) : CSurface(a, x) {
    vector tbmin, tbmax;

    atomicIncrement(stats.numGprims);

    r = r0;
    height = height0;
    umax = angle0;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = new float[3];
    nextData[0] = r1;
    nextData[1] = height1;
    nextData[2] = angle1;

    computeObjectBound(bmin, bmax, r, height, umax);
    computeObjectBound(tbmin, tbmax, nextData[0], nextData[1], nextData[2]);
    addBox(bmin, bmax, tbmin);
    addBox(bmin, bmax, tbmax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Method				:	~CCone
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CCone::~CCone() {
    atomicDecrement(stats.numGprims);

    if (parameters != NULL)
        delete parameters;
    if (nextData != NULL)
        delete[] nextData;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Method				:	intersect
// Description			:	Compute the intersection with a ray
// Return Value			:	TRUE if intersects
// Comments				:
void CCone::intersect(CShadingContext *context, CRay *rv) {
    float *from;
    float *dir;
    double s[2];
    unsigned int ns, i;
    double height, r, umax;
    vector Nt;

    if (checkRayGuard(rv, context))
        return;

    vector oFrom, oDir;
    transform(oFrom, oDir, xform, rv);

    from = oFrom;
    dir = oDir;

    if (nextData != NULL) {
        r = (float)(this->r * (1.0 - (double)rv->time) + nextData[0] * (double)rv->time);
        height = (float)(this->height * (1.0 - (double)rv->time) + nextData[1] * (double)rv->time);
        umax = (float)(this->umax * (1.0 - (double)rv->time) + nextData[2] * (double)rv->time);
    }
    else {
        r = this->r;
        height = this->height;
        umax = this->umax;
    }

    if (height * height < C_EPSILON) {
        // We're a disc
        ns = 1;
        s[0] = -from[COMP_Z] / dir[COMP_Z];
    }
    else {
        const double a = height * height * (dir[COMP_X] * dir[COMP_X] + dir[COMP_Y] * dir[COMP_Y]) - r * r * dir[COMP_Z] * dir[COMP_Z];
        const double b = height * height * 2 * (from[COMP_X] * dir[COMP_X] + from[COMP_Y] * dir[COMP_Y]) - r * r * 2 * (from[COMP_Z] - height) * dir[COMP_Z];
        const double c = height * height * (from[COMP_X] * from[COMP_X] + from[COMP_Y] * from[COMP_Y]) - r * r * (from[COMP_Z] - height) * (from[COMP_Z] - height);

        if ((ns = solveQuadric<double>(a, b, c, s)) == 0)
            return;
    }

    for (i = 0; i < ns; i++) {
        float t = (float)s[i];
        double u, v;
        double P[3];

        if (t <= rv->tmin)
            continue;
        if (t >= rv->t)
            return;

        P[COMP_Z] = oDir[COMP_Z] * t + oFrom[COMP_Z];

        if (height < 0) {
            if (P[COMP_Z] < height)
                continue;
            if (P[COMP_Z] > 0)
                continue;
        }
        else if (height > 0) {
            if (P[COMP_Z] > height)
                continue;
            if (P[COMP_Z] < 0)
                continue;
        }

        P[COMP_X] = oDir[COMP_X] * t + oFrom[COMP_X];
        P[COMP_Y] = oDir[COMP_Y] * t + oFrom[COMP_Y];

        if (r > 0)
            u = atan2(P[COMP_Y], P[COMP_X]);
        else
            u = atan2(-P[COMP_Y], -P[COMP_X]);

        if (u < 0)
            u = 2 * C_PI + u;
        if (umax < 0) {
            u = u - 2 * C_PI;
            if (u < umax) {
                continue;
            }
        }
        else {
            if (u > umax) {
                continue;
            }
        }

        if (r * r > height * height)
            v = 1.0 - sqrt((P[COMP_X] * P[COMP_X] + P[COMP_Y] * P[COMP_Y]) / (r * r));
        else
            v = P[COMP_Z] / height;

        if (v < 0) {
            continue;
        }

        Nt[COMP_X] = (float)(P[COMP_X] * height * umax);
        Nt[COMP_Y] = (float)(P[COMP_Y] * height * umax);
        Nt[COMP_Z] = (float)(r * r * umax * (1.0 - v));
        if ((attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ xform->flip) {
            mulvf(Nt, -1);
        }

        if (!(attributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED)) {
            if (dotvv(oDir, Nt) > 0) {
                continue;
            }
        }

        if (!regionKeepsHit(this, context, rv, t, Nt))
            continue;

        rv->object = this;
        rv->u = (float)(u / umax);
        rv->v = (float)v;
        rv->t = (float)t;
        mulmn(rv->N, xform->to, Nt);
        return;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Method				:	sample
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CCone::sample(int start, int numVertices, float **varying, float ***, unsigned int &up) const {
    const float *u = varying[VARIABLE_U] + start;
    const float *v = varying[VARIABLE_V] + start;

    // Allocate memory for precomputing sin(u) , cos(u)
    float *sinu = (float *)alloca(numVertices * 2 * sizeof(float));
    float *cosu = sinu + numVertices;

    // Are we raytracing this primitive?
    if ((nextData != NULL) && (!(up & (PARAMETER_BEGIN_SAMPLE | PARAMETER_END_SAMPLE)))) {
        float *r = (float *)alloca(numVertices * 3 * sizeof(float));
        float *height = r + numVertices;
        float *umax = height + numVertices;
        const float *time = varying[VARIABLE_TIME] + start;
        float *dest = varying[VARIABLE_P] + start * 3;

        // Compute the position
        for (int i = 0; i < numVertices; ++i, dest += 3) {
            r[i] = (float)(this->r * (1.0 - (double)time[i]) + nextData[0] * (double)time[i]);
            height[i] = (float)(this->height * (1.0 - (double)time[i]) + nextData[1] * (double)time[i]);
            umax[i] = (float)(this->umax * (1.0 - (double)time[i]) + nextData[2] * (double)time[i]);
            cosu[i] = cosf(u[i] * umax[i]);
            sinu[i] = sinf(u[i] * umax[i]);
            dest[COMP_X] = (float)(r[i] * (1.0 - (double)v[i]) * cosu[i]);
            dest[COMP_Y] = (float)(r[i] * (1.0 - (double)v[i]) * sinu[i]);
            dest[COMP_Z] = v[i] * height[i];
        }

        // Set Ng if needed
        if (up & PARAMETER_NG) {
            float *Ng = varying[VARIABLE_NG] + start * 3;
            const float *P = varying[VARIABLE_P] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                Ng[COMP_X] = P[COMP_X] * umax[i] * height[i];
                Ng[COMP_Y] = P[COMP_Y] * umax[i] * height[i];
                Ng[COMP_Z] = (float)(r[i] * r[i] * umax[i] * (1.0 - (double)v[i]));
                Ng += 3;
                P += 3;
            }
        }

        // Set dPdu/dPdv if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                dPdu[COMP_X] = (float)(-umax[i] * r[i] * (1.0 - (double)v[i]) * sinu[i]);
                dPdu[COMP_Y] = (float)(umax[i] * r[i] * (1.0 - (double)v[i]) * cosu[i]);
                dPdu[COMP_Z] = 0;
                dPdv[COMP_X] = -r[i] * cosu[i];
                dPdv[COMP_Y] = -r[i] * sinu[i];
                dPdv[COMP_Z] = height[i];
                dPdu += 3;
                dPdv += 3;
            }
        }
    }
    else {
        float r = this->r;
        float height = this->height;
        float umax = this->umax;

        if ((up & PARAMETER_END_SAMPLE) && (nextData != NULL)) {
            r = nextData[0];
            height = nextData[1];
            umax = nextData[2];
        }

        // Set u/v/P
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int i = 0; i < numVertices; ++i) {
            cosu[i] = cosf(u[i] * umax);
            sinu[i] = sinf(u[i] * umax);
            dest[COMP_X] = (float)(r * (1.0 - (double)v[i]) * cosu[i]);
            dest[COMP_Y] = (float)(r * (1.0 - (double)v[i]) * sinu[i]);
            dest[COMP_Z] = v[i] * height;
            dest += 3;
        }

        // Set Ng if needed
        if (up & PARAMETER_NG) {
            float *Ng = varying[VARIABLE_NG] + start * 3;
            const float *P = varying[VARIABLE_P] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                Ng[COMP_X] = P[COMP_X] * height * umax;
                Ng[COMP_Y] = P[COMP_Y] * height * umax;
                Ng[COMP_Z] = (float)(r * r * umax * (1.0 - (double)v[i]));
                Ng += 3;
                P += 3;
            }
        }

        // Set dPdu/dPdv if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                dPdu[COMP_X] = (float)(-umax * r * (1.0 - (double)v[i]) * sinu[i]);
                dPdu[COMP_Y] = (float)(umax * r * (1.0 - (double)v[i]) * cosu[i]);
                dPdu[COMP_Z] = 0;
                dPdv[COMP_X] = -r * cosu[i];
                dPdv[COMP_Y] = -r * sinu[i];
                dPdv[COMP_Z] = height;
                dPdu += 3;
                dPdv += 3;
            }
        }
    }

    if (xform->flip == 1) {
        float *N = varying[VARIABLE_NG] + start * 3;

        for (int i = numVertices; i > 0; --i, N += 3) {
            mulvf(N, -1);
        }
    }

    // Set dPdtime
    if (up & PARAMETER_DPDTIME) {
        // This is where we'll store the result
        float *dest = varying[VARIABLE_DPDTIME] + start * 3;

        // Are we moving?
        if (nextData != NULL) {
            // Get the xform matrices to move to the camera system
            const float *fromStart = xform->from;
            const float *fromEnd = (xform->next != NULL ? xform->next->from : xform->from);

            // Compute the xforms
            for (int i = 0; i < numVertices; ++i, dest += 3) {
                const float rStart = r;
                const float heightStart = height;
                const float umaxStart = umax;
                const float rEnd = nextData[0];
                const float heightEnd = nextData[1];
                const float umaxEnd = nextData[2];
                vector Pstart, Pend;

                // Compute the position at the start
                Pstart[COMP_X] = (rStart * (1.0f - v[i]) * cosf(u[i] * umaxStart));
                Pstart[COMP_Y] = (rStart * (1.0f - v[i]) * sinf(u[i] * umaxStart));
                Pstart[COMP_Z] = v[i] * heightStart;
                mulmp(Pstart, fromStart, Pstart);

                // Compute the position at the end
                Pend[COMP_X] = (rEnd * (1.0f - v[i]) * cosf(u[i] * umaxEnd));
                Pend[COMP_Y] = (rEnd * (1.0f - v[i]) * sinf(u[i] * umaxEnd));
                Pend[COMP_Z] = v[i] * heightEnd;
                mulmp(Pend, fromEnd, Pend);

                // Compute the dPdtime
                subvv(dest, Pend, Pstart);
                mulvf(dest, CRenderer::invShutterTime);
            }
        }
        else {
            // Get the xform matrices
            const float *fromStart = xform->from + element(0, 3);
            const float *fromEnd = (xform->next != NULL ? xform->next->from + element(0, 3) : xform->from + element(0, 3));
            vector D;
            subvv(D, fromEnd, fromStart);
            mulvf(D, CRenderer::invShutterTime);

            // dPdtime is zero
            for (int i = 0; i < numVertices; ++i, dest += 3)
                movvv(dest, D);
        }
    }

    // Transform the points
    transformPoints();

    // Turn off the parameters we computed
    up &= ~parametersF;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Method				:	interpolate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CCone::interpolate(int numVertices, float **varying, float ***locals) const {
    if (parameters != NULL)
        parameters->dispatch(numVertices, varying, locals);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CCone::instantiate(CAttributes *a, CXform *x, CRiInterface *cx) const {
    CXform *nx = new CXform(x);

    nx->concat(xform); // Concetenate the local xform

    if (a == NULL)
        a = attributes;

    if (nextData == NULL)
        cx->addObject(new CCone(a, nx, parameters, parametersF, r, height, umax));
    else
        cx->addObject(new CCone(a, nx, parameters, parametersF, r, height, umax, nextData[0], nextData[1], nextData[2]));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Method				:	computeObjectBound
// Description			:	Compute the bounding box in the object space
// Return Value			:	-
// Comments				:
void CCone::computeObjectBound(float *bmin, float *bmax, float r, float height, float) {

    r = absf(r);

    float minHeight;
    if (0 < height) {
        minHeight = 0;
    }
    else {
        minHeight = height;
    }
    float maxHeight;
    if (0 > height) {
        maxHeight = 0;
    }
    else {
        maxHeight = height;
    }
    initv(bmin, -r, -r, minHeight);
    initv(bmax, r, r, maxHeight);
}

//////////////////////////////////////////// P A R A B O L O I D ///////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Method				:	CParaboloid
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CParaboloid::CParaboloid(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float ra, float zmina, float zmaxa, float anglea) : CSurface(a, x) {
    atomicIncrement(stats.numGprims);

    r = ra;
    zmin = zmina;
    zmax = zmaxa;
    umax = anglea;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = NULL;

    computeObjectBound(bmin, bmax, r, zmin, zmax, umax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Method				:	CParaboloid
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CParaboloid::CParaboloid(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float r0, float zmin0, float zmax0, float angle0, float r1, float zmin1, float zmax1, float angle1) : CSurface(a, x) {
    vector tbmin, tbmax;

    atomicIncrement(stats.numGprims);

    r = r0;
    zmin = zmin0;
    zmax = zmax0;
    umax = angle0;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = new float[4];
    nextData[0] = r1;
    nextData[1] = zmin1;
    nextData[2] = zmax1;
    nextData[3] = angle1;

    computeObjectBound(bmin, bmax, r, zmin, zmax, umax);
    computeObjectBound(tbmin, tbmax, nextData[0], nextData[1], nextData[2], nextData[3]);
    addBox(bmin, bmax, tbmin);
    addBox(bmin, bmax, tbmax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Method				:	~CParaboloid
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CParaboloid::~CParaboloid() {
    atomicDecrement(stats.numGprims);

    if (parameters != NULL)
        delete parameters;
    if (nextData != NULL)
        delete[] nextData;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Method				:	intersect
// Description			:	Compute the intersection with a ray
// Return Value			:	TRUE if intersects
// Comments				:
void CParaboloid::intersect(CShadingContext *context, CRay *rv) {
    vector Nt;

    if (checkRayGuard(rv, context))
        return;

    vector oFrom, oDir;
    transform(oFrom, oDir, xform, rv);

    float *from = oFrom;
    float *dir = oDir;
    double a, b, c, s[2];
    unsigned int ns, i;
    float zmi, zma;
    float zmin, zmax;
    float r, umax;

    if (nextData != NULL) {
        zmin = (float)(this->zmin * (1.0 - (double)rv->time) + nextData[1] * (double)rv->time);
        zmax = (float)(this->zmax * (1.0 - (double)rv->time) + nextData[2] * (double)rv->time);
        r = (float)(this->r * (1.0 - (double)rv->time) + nextData[0] * (double)rv->time);
        umax = (float)(this->umax * (1.0 - (double)rv->time) + nextData[3] * (double)rv->time);
    }
    else {
        zmin = this->zmin;
        zmax = this->zmax;
        r = this->r;
        umax = this->umax;
    }

    a = dir[COMP_X] * dir[COMP_X] + dir[COMP_Y] * dir[COMP_Y];
    b = 2 * dir[COMP_X] * from[COMP_X] + 2 * dir[COMP_Y] * from[COMP_Y] - r * r * dir[COMP_Z] / zmax;
    c = from[COMP_X] * from[COMP_X] + from[COMP_Y] * from[COMP_Y] - r * r * from[COMP_Z] / zmax;

    if (a < C_EPSILON) {
        if (b == 0)
            return;
        s[0] = -c / b;
        ns = 1;
    }
    else {
        if ((ns = solveQuadric<double>(a, b, c, s)) == 0)
            return;
    }

    if (zmin < zmax) {
        zmi = zmin;
        zma = zmax;
    }
    else {
        zmi = zmax;
        zma = zmin;
    }

    for (i = 0; i < ns; i++) {
        double t = s[i];
        double u;
        double P[3];

        if (t <= rv->tmin)
            continue;
        if (t >= rv->t)
            return;

        P[0] = from[0] + dir[0] * t;
        P[1] = from[1] + dir[1] * t;
        P[2] = from[2] + dir[2] * t;

        if (P[COMP_Z] < zmi)
            continue;
        if (P[COMP_Z] > zma)
            continue;

        if (r > 0)
            u = atan2(P[COMP_Y], P[COMP_X]);
        else
            u = atan2(-P[COMP_Y], -P[COMP_X]);

        if (u < 0)
            u = 2 * C_PI + u;

        if (umax < 0) {
            u = u - 2 * C_PI;
            if (u < umax)
                continue;
        }
        else {
            if (u > umax)
                continue;
        }

        const float tmp = umax * (zmax - zmin);
        Nt[COMP_X] = (float)(P[COMP_X] * tmp);
        Nt[COMP_Y] = (float)(P[COMP_Y] * tmp);
        Nt[COMP_Z] = (float)(-r * r * tmp / (2 * zmax));

        if ((attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ xform->flip) {
            mulvf(Nt, -1);
        }

        if (!(attributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED)) {
            if (dotvv(oDir, Nt) > 0)
                continue;
        }

        if (!regionKeepsHit(this, context, rv, t, Nt))
            continue;

        rv->object = this;
        rv->u = (float)(u / umax);
        rv->v = (float)((P[COMP_Z] - zmin) / (zmax - zmin));
        rv->t = (float)t;
        mulmn(rv->N, xform->to, Nt);
        return;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Method				:	sample
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CParaboloid::sample(int start, int numVertices, float **varying, float ***, unsigned int &up) const {
    const float *u = varying[VARIABLE_U] + start;
    const float *v = varying[VARIABLE_V] + start;

    // Allocate memory for precomputing sin(u) , cos(u)
    float *sinu = (float *)alloca(numVertices * 3 * sizeof(float));
    float *cosu = sinu + numVertices;
    float *sqrtz = cosu + numVertices;

    if ((nextData != NULL) && (!(up & (PARAMETER_BEGIN_SAMPLE | PARAMETER_END_SAMPLE)))) {
        float *r = (float *)alloca(numVertices * 4 * sizeof(float));
        float *umax = r + numVertices;
        float *zmax = umax + numVertices;
        float *zmin = zmax + numVertices;
        const float *time = varying[VARIABLE_TIME] + start;
        float *dest = varying[VARIABLE_P] + start * 3;

        // For each point to compute
        for (int i = 0; i < numVertices; ++i) {
            r[i] = (float)(this->r * (1.0 - (double)time[i]) + nextData[0] * (double)time[i]);
            zmin[i] = (float)(this->zmin * (1.0 - (double)time[i]) + nextData[1] * (double)time[i]);
            zmax[i] = (float)(this->zmax * (1.0 - (double)time[i]) + nextData[2] * (double)time[i]);
            umax[i] = (float)(this->umax * (1.0 - (double)time[i]) + nextData[3] * (double)time[i]);
            cosu[i] = cosf(u[i] * umax[i]);
            sinu[i] = sinf(u[i] * umax[i]);
            sqrtz[i] = sqrtf(((zmax[i] - zmin[i]) * v[i] + zmin[i]) / zmax[i]);

            dest[COMP_Z] = (zmax[i] - zmin[i]) * v[i] + zmin[i];
            dest[COMP_X] = r[i] * sqrtz[i] * cosu[i];
            dest[COMP_Y] = r[i] * sqrtz[i] * sinu[i];
            dest += 3;
        }

        // Set Ng if needed
        if (up & PARAMETER_NG) {
            float *Ng = varying[VARIABLE_NG] + start * 3;
            const float *P = varying[VARIABLE_P] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                const float tmp2 = umax[i] * (zmax[i] - zmin[i]);
                Ng[COMP_X] = P[COMP_X] * tmp2;
                Ng[COMP_Y] = P[COMP_Y] * tmp2;
                Ng[COMP_Z] = -r[i] * r[i] * tmp2 / (2 * zmax[i]);
                Ng += 3;
                P += 3;
            }
        }

        // Set dPdu/dPdv if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                dPdu[COMP_X] = -umax[i] * sqrtz[i] * sinu[i];
                dPdu[COMP_Y] = umax[i] * sqrtz[i] * cosu[i];
                dPdu[COMP_Z] = 0;
                dPdv[COMP_X] = r[i] * (zmax[i] - zmin[i]) * cosu[i] / (2 * zmax[i] * sqrtz[i]);
                dPdv[COMP_Y] = r[i] * (zmax[i] - zmin[i]) * sinu[i] / (2 * zmax[i] * sqrtz[i]);
                dPdv[COMP_Z] = zmax[i] - zmin[i];
                dPdu += 3;
                dPdv += 3;
            }
        }
    }
    else {
        float r = this->r;
        float zmin = this->zmin;
        float zmax = this->zmax;
        float umax = this->umax;

        if ((up & PARAMETER_END_SAMPLE) && (nextData != NULL)) {
            r = nextData[0];
            zmin = nextData[1];
            zmax = nextData[2];
            umax = nextData[3];
        }

        // Precompute the sin/cos for u points
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int i = 0; i < numVertices; ++i) {
            cosu[i] = cosf(u[i] * umax);
            sinu[i] = sinf(u[i] * umax);
            sqrtz[i] = sqrtf(((zmax - zmin) * v[i] + zmin) / zmax);

            dest[COMP_Z] = (zmax - zmin) * v[i] + zmin;
            dest[COMP_X] = r * sqrtz[i] * cosu[i];
            dest[COMP_Y] = r * sqrtz[i] * sinu[i];
            dest += 3;
        }

        // Set Ng if needed
        if (up & PARAMETER_NG) {
            float *Ng = varying[VARIABLE_NG] + start * 3;
            const float *P = varying[VARIABLE_P] + start * 3;
            float tmp2 = umax * (zmax - zmin);

            for (int i = 0; i < numVertices; ++i) {
                Ng[COMP_X] = P[COMP_X] * tmp2;
                Ng[COMP_Y] = P[COMP_Y] * tmp2;
                Ng[COMP_Z] = -r * r * tmp2 / (2 * zmax);
                Ng += 3;
                P += 3;
            }
        }

        // Set dPdu/dPdv if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                dPdu[COMP_X] = -umax * sqrtz[i] * sinu[i];
                dPdu[COMP_Y] = umax * sqrtz[i] * cosu[i];
                dPdu[COMP_Z] = 0;
                dPdv[COMP_X] = r * (zmax - zmin) * cosu[i] / (2 * zmax * sqrtz[i]);
                dPdv[COMP_Y] = r * (zmax - zmin) * sinu[i] / (2 * zmax * sqrtz[i]);
                dPdv[COMP_Z] = zmax - zmin;
                dPdu += 3;
                dPdv += 3;
            }
        }
    }

    if (xform->flip == 1) {
        float *N = varying[VARIABLE_NG] + start * 3;

        for (int i = numVertices; i > 0; --i, N += 3) {
            mulvf(N, -1);
        }
    }

    // Set dPdtime
    if (up & PARAMETER_DPDTIME) {
        // This is where we'll store the result
        float *dest = varying[VARIABLE_DPDTIME] + start * 3;

        // Are we moving?
        if (nextData != NULL) {
            // Get the xform matrices to move to the camera system
            const float *fromStart = xform->from;
            const float *fromEnd = (xform->next != NULL ? xform->next->from : xform->from);

            assert(u == (varying[VARIABLE_U] + start));
            assert(v == (varying[VARIABLE_V] + start));

            // Compute the xforms
            for (int i = 0; i < numVertices; ++i, dest += 3) {
                const float rStart = r;
                const float zminStart = zmin;
                const float zmaxStart = zmax;
                const float umaxStart = umax;
                const float rEnd = nextData[0];
                const float zminEnd = nextData[1];
                const float zmaxEnd = nextData[2];
                const float umaxEnd = nextData[3];
                vector Pstart, Pend;

                // Compute the position at the start
                Pstart[COMP_Z] = (zmaxStart - zminStart) * v[i] + zminStart;
                Pstart[COMP_X] = rStart * sqrtf(((zmaxStart - zminStart) * v[i] + zminStart) / zmaxStart) * cosf(u[i] * umaxStart);
                Pstart[COMP_Y] = rStart * sqrtf(((zmaxStart - zminStart) * v[i] + zminStart) / zmaxStart) * sinf(u[i] * umaxStart);
                mulmp(Pstart, fromStart, Pstart);

                // Compute the position at the end
                Pend[COMP_Z] = (zmaxEnd - zminEnd) * v[i] + zminEnd;
                Pend[COMP_X] = rEnd * sqrtf(((zmaxEnd - zminEnd) * v[i] + zminEnd) / zmaxEnd) * cosf(u[i] * umaxEnd);
                Pend[COMP_Y] = rEnd * sqrtf(((zmaxEnd - zminEnd) * v[i] + zminEnd) / zmaxEnd) * sinf(u[i] * umaxEnd);
                mulmp(Pend, fromEnd, Pend);

                // Compute the dPdtime
                subvv(dest, Pend, Pstart);
                mulvf(dest, CRenderer::invShutterTime);
            }
        }
        else {
            // Get the xform matrices
            const float *fromStart = xform->from + element(0, 3);
            const float *fromEnd = (xform->next != NULL ? xform->next->from + element(0, 3) : xform->from + element(0, 3));
            vector D;
            subvv(D, fromEnd, fromStart);
            mulvf(D, CRenderer::invShutterTime);

            // dPdtime is zero
            for (int i = 0; i < numVertices; ++i, dest += 3)
                movvv(dest, D);
        }
    }

    // Transform the points
    transformPoints();

    up &= ~parametersF;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Method				:	interpolate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CParaboloid::interpolate(int numVertices, float **varying, float ***locals) const {
    if (parameters != NULL)
        parameters->dispatch(numVertices, varying, locals);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CParaboloid::instantiate(CAttributes *a, CXform *x, CRiInterface *cx) const {
    CXform *nx = new CXform(x);

    nx->concat(xform); // Concetenate the local xform

    if (a == NULL)
        a = attributes;

    if (nextData == NULL)
        cx->addObject(new CParaboloid(a, nx, parameters, parametersF, r, zmin, zmax, umax));
    else
        cx->addObject(new CParaboloid(a, nx, parameters, parametersF, r, zmin, zmax, umax, nextData[0], nextData[1], nextData[2], nextData[3]));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Method				:	computeObjectBound
// Description			:	Compute the bounding box in the object space
// Return Value			:	-
// Comments				:
void CParaboloid::computeObjectBound(float *bmin, float *bmax, float r, float zmin, float zmax, float) {
    r = absf(r);

    float minZ2;
    if (zmax < zmin) {
        minZ2 = zmax;
    }
    else {
        minZ2 = zmin;
    }
    float maxZ2;
    if (zmax > zmin) {
        maxZ2 = zmax;
    }
    else {
        maxZ2 = zmin;
    }
    initv(bmin, -r, -r, minZ2);
    initv(bmax, r, r, maxZ2);
}

//////////////////////////////////////////// C Y L I N D E R  ///////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Method				:	CCylinder
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CCylinder::CCylinder(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float ra, float zmina, float zmaxa, float anglea) : CSurface(a, x) {
    atomicIncrement(stats.numGprims);

    r = ra;
    zmin = zmina;
    zmax = zmaxa;
    umax = anglea;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = NULL;

    computeObjectBound(bmin, bmax, r, zmin, zmax, umax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Method				:	CCylinder
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CCylinder::CCylinder(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float r0, float zmin0, float zmax0, float angle0, float r1, float zmin1, float zmax1, float angle1) : CSurface(a, x) {
    vector tbmin, tbmax;

    atomicIncrement(stats.numGprims);

    r = r0;
    zmin = zmin0;
    zmax = zmax0;
    umax = angle0;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = new float[4];
    nextData[0] = r1;
    nextData[1] = zmin1;
    nextData[2] = zmax1;
    nextData[3] = angle1;

    computeObjectBound(bmin, bmax, r, zmin, zmax, umax);
    computeObjectBound(tbmin, tbmax, nextData[0], nextData[1], nextData[2], nextData[3]);
    addBox(bmin, bmax, tbmin);
    addBox(bmin, bmax, tbmax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Method				:	~CCylinder
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CCylinder::~CCylinder() {
    atomicDecrement(stats.numGprims);

    if (parameters != NULL)
        delete parameters;
    if (nextData != NULL)
        delete[] nextData;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Method				:	intersect
// Description			:	Compute the intersection with a ray
// Return Value			:	TRUE if intersects
// Comments				:
void CCylinder::intersect(CShadingContext *context, CRay *rv) {
    vector Nt;

    if (checkRayGuard(rv, context))
        return;

    vector oFrom, oDir;
    transform(oFrom, oDir, xform, rv);

    float *from = oFrom;
    float *dir = oDir;
    double a, b, c, s[2];
    unsigned int ns, i;
    float r, zmin, zmax, umax;

    if (nextData != NULL) {
        zmin = (float)(this->zmin * (1.0 - (double)rv->time) + nextData[1] * (double)rv->time);
        zmax = (float)(this->zmax * (1.0 - (double)rv->time) + nextData[2] * (double)rv->time);
        r = (float)(this->r * (1.0 - (double)rv->time) + nextData[0] * (double)rv->time);
        umax = (float)(this->umax * (1.0 - (double)rv->time) + nextData[3] * (double)rv->time);
    }
    else {
        zmin = this->zmin;
        zmax = this->zmax;
        r = this->r;
        umax = this->umax;
    }

    a = dir[COMP_X] * dir[COMP_X] + dir[COMP_Y] * dir[COMP_Y];
    b = 2 * dir[COMP_X] * from[COMP_X] + 2 * dir[COMP_Y] * from[COMP_Y];
    c = from[COMP_X] * from[COMP_X] + from[COMP_Y] * from[COMP_Y] - r * r;

    if (a == 0) {
        if (b == 0)
            return;

        s[0] = -c / b;
        ns = 1;
    }
    else {
        if ((ns = solveQuadric<double>(a, b, c, s)) == 0)
            return;
    }

    for (i = 0; i < ns; i++) {
        float t = (float)s[i];
        double u;
        double P[3];

        if (t <= rv->tmin)
            continue;
        if (t >= rv->t)
            return;

        P[0] = from[0] + dir[0] * t;
        P[1] = from[1] + dir[1] * t;
        P[2] = from[2] + dir[2] * t;

        if (zmin < zmax) {
            if (P[COMP_Z] < zmin)
                continue;
            if (P[COMP_Z] > zmax)
                continue;
        }
        else {
            if (P[COMP_Z] < zmax)
                continue;
            if (P[COMP_Z] > zmin)
                continue;
        }

        if (r > 0)
            u = atan2(P[COMP_Y], P[COMP_X]);
        else
            u = atan2(-P[COMP_Y], -P[COMP_X]);

        if (u < 0)
            u = 2 * C_PI + u;
        if (umax < 0) {
            u = u - 2 * C_PI;
            if (u < umax)
                continue;
        }
        else {
            if (u > umax)
                continue;
        }

        Nt[COMP_X] = (float)(P[COMP_X] * umax * (zmax - zmin));
        Nt[COMP_Y] = (float)(P[COMP_Y] * umax * (zmax - zmin));
        Nt[COMP_Z] = 0;

        if ((attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ xform->flip) {
            mulvf(Nt, -1);
        }

        if (!(attributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED)) {
            if (dotvv(oDir, Nt) > 0)
                continue;
        }

        if (!regionKeepsHit(this, context, rv, t, Nt))
            continue;

        rv->object = this;
        rv->u = (float)(u / umax);
        rv->v = (float)(P[COMP_Z] - zmin) / (zmax - zmin);
        rv->t = (float)t;
        mulmn(rv->N, xform->to, Nt);
        return;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Method				:	sample
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CCylinder::sample(int start, int numVertices, float **varying, float ***, unsigned int &up) const {
    float *u = varying[VARIABLE_U] + start;
    float *v = varying[VARIABLE_V] + start;

    // Allocate memory for precomputing sin(u) , cos(u)
    float *sinu = (float *)alloca(numVertices * 2 * sizeof(float));
    float *cosu = sinu + numVertices;

    if ((nextData != NULL) && (!(up & (PARAMETER_BEGIN_SAMPLE | PARAMETER_END_SAMPLE)))) {
        float *r = (float *)alloca(numVertices * 4 * sizeof(float));
        float *umax = r + numVertices;
        float *zmax = umax + numVertices;
        float *zmin = zmax + numVertices;
        const float *time = varying[VARIABLE_TIME] + start;
        float *dest = varying[VARIABLE_P] + start * 3;

        // For every vertex to compute
        for (int i = 0; i < numVertices; ++i) {
            r[i] = (float)(this->r * (1.0 - (double)time[i]) + nextData[0] * (double)time[i]);
            zmin[i] = (float)(this->zmin * (1.0 - (double)time[i]) + nextData[1] * (double)time[i]);
            zmax[i] = (float)(this->zmax * (1.0 - (double)time[i]) + nextData[2] * (double)time[i]);
            umax[i] = (float)(this->umax * (1.0 - (double)time[i]) + nextData[3] * (double)time[i]);
            cosu[i] = (cosf(u[i] * umax[i])) * r[i];
            sinu[i] = (sinf(u[i] * umax[i])) * r[i];

            dest[COMP_X] = cosu[i];
            dest[COMP_Y] = sinu[i];
            dest[COMP_Z] = (zmax[i] - zmin[i]) * v[i] + zmin[i];
            dest += 3;
        }

        // Set Ng if needed
        if (up & PARAMETER_NG) {
            float *Ng = varying[VARIABLE_NG] + start * 3;
            float *P = varying[VARIABLE_P] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                const float tmp2 = umax[i] * (zmax[i] - zmin[i]);
                Ng[COMP_X] = P[COMP_X] * tmp2;
                Ng[COMP_Y] = P[COMP_Y] * tmp2;
                Ng[COMP_Z] = 0;
                Ng += 3;
                P += 3;
            }
        }

        // Set dPdu/dPdv if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                dPdu[COMP_X] = -umax[i] * sinu[i];
                dPdu[COMP_Y] = umax[i] * cosu[i];
                dPdu[COMP_Z] = 0;
                dPdv[COMP_X] = 0;
                dPdv[COMP_Y] = 0;
                dPdv[COMP_Z] = zmax[i] - zmin[i];
                dPdu += 3;
                dPdv += 3;
            }
        }
    }
    else {
        float r = this->r;
        float zmin = this->zmin;
        float zmax = this->zmax;
        float umax = this->umax;

        if ((up & PARAMETER_END_SAMPLE) && (nextData != NULL)) {
            r = nextData[0];
            zmin = nextData[1];
            zmax = nextData[2];
            umax = nextData[3];
        }

        // Set u/v/P
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int i = 0; i < numVertices; ++i) {
            cosu[i] = (cosf(u[i] * umax)) * r;
            sinu[i] = (sinf(u[i] * umax)) * r;
            dest[COMP_X] = cosu[i];
            dest[COMP_Y] = sinu[i];
            dest[COMP_Z] = (zmax - zmin) * v[i] + zmin;
            dest += 3;
        }

        // Set Ng if needed
        if (up & PARAMETER_NG) {
            float *Ng = varying[VARIABLE_NG] + start * 3;
            float *P = varying[VARIABLE_P] + start * 3;
            const float tmp2 = umax * (zmax - zmin);

            for (int i = 0; i < numVertices; ++i) {
                Ng[COMP_X] = P[COMP_X] * tmp2;
                Ng[COMP_Y] = P[COMP_Y] * tmp2;
                Ng[COMP_Z] = 0;
                Ng += 3;
                P += 3;
            }
        }

        // Set dPdu/dPdv if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                dPdu[COMP_X] = -umax * sinu[i];
                dPdu[COMP_Y] = umax * cosu[i];
                dPdu[COMP_Z] = 0;
                dPdv[COMP_X] = 0;
                dPdv[COMP_Y] = 0;
                dPdv[COMP_Z] = zmax - zmin;
                dPdu += 3;
                dPdv += 3;
            }
        }
    }

    if (xform->flip) {
        float *N = varying[VARIABLE_NG] + start * 3;

        for (int i = numVertices; i > 0; --i, N += 3) {
            mulvf(N, -1);
        }
    }

    // Set dPdtime
    if (up & PARAMETER_DPDTIME) {
        // This is where we'll store the result
        float *dest = varying[VARIABLE_DPDTIME] + start * 3;

        // Are we moving?
        if (nextData != NULL) {
            // Get the xform matrices to move to the camera system
            const float *fromStart = xform->from;
            const float *fromEnd = (xform->next != NULL ? xform->next->from : xform->from);

            // Basic sanity check
            assert(u == (varying[VARIABLE_U] + start));
            assert(v == (varying[VARIABLE_V] + start));

            // Compute the xforms
            for (int i = 0; i < numVertices; ++i, dest += 3) {
                const float rStart = r;
                const float zminStart = zmin;
                const float zmaxStart = zmax;
                const float umaxStart = umax;
                const float rEnd = nextData[0];
                const float zminEnd = nextData[1];
                const float zmaxEnd = nextData[2];
                const float umaxEnd = nextData[3];
                vector Pstart, Pend;

                // Compute the position at the start
                Pstart[COMP_X] = (cosf(u[i] * umaxStart)) * rStart;
                Pstart[COMP_Y] = (sinf(u[i] * umaxStart)) * rStart;
                Pstart[COMP_Z] = (zmaxStart - zminStart) * v[i] + zminStart;
                mulmp(Pstart, fromStart, Pstart);

                // Compute the position at the end
                Pend[COMP_X] = (cosf(u[i] * umaxEnd)) * rEnd;
                Pend[COMP_Y] = (sinf(u[i] * umaxEnd)) * rEnd;
                Pend[COMP_Z] = (zmaxEnd - zminEnd) * v[i] + zminEnd;
                mulmp(Pend, fromEnd, Pend);

                // Compute the dPdtime
                subvv(dest, Pend, Pstart);
                mulvf(dest, CRenderer::invShutterTime);
            }
        }
        else {
            // Get the xform matrices
            const float *fromStart = xform->from + element(0, 3);
            const float *fromEnd = (xform->next != NULL ? xform->next->from + element(0, 3) : xform->from + element(0, 3));
            vector D;
            subvv(D, fromEnd, fromStart);
            mulvf(D, CRenderer::invShutterTime);

            // dPdtime is zero
            for (int i = 0; i < numVertices; ++i, dest += 3)
                movvv(dest, D);
        }
    }

    // Transform the points
    transformPoints();

    // Turn off the parameters we computed
    up &= ~parametersF;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Method				:	interpolate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CCylinder::interpolate(int numVertices, float **varying, float ***locals) const {
    if (parameters != NULL)
        parameters->dispatch(numVertices, varying, locals);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CCylinder::instantiate(CAttributes *a, CXform *x, CRiInterface *cx) const {
    CXform *nx = new CXform(x);

    nx->concat(xform); // Concetenate the local xform

    if (a == NULL)
        a = attributes;

    if (nextData == NULL)
        cx->addObject(new CCylinder(a, nx, parameters, parametersF, r, zmin, zmax, umax));
    else
        cx->addObject(new CCylinder(a, nx, parameters, parametersF, r, zmin, zmax, umax, nextData[0], nextData[1], nextData[2], nextData[3]));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Method				:	computeObjectBound
// Description			:	Compute the bounding box in the object space
// Return Value			:	-
// Comments				:
void CCylinder::computeObjectBound(float *bmin, float *bmax, float r, float zmin, float zmax, float) {
    r = absf(r);

    float minZ2;
    if (zmax < zmin) {
        minZ2 = zmax;
    }
    else {
        minZ2 = zmin;
    }
    float maxZ2;
    if (zmax > zmin) {
        maxZ2 = zmax;
    }
    else {
        maxZ2 = zmin;
    }
    initv(bmin, -r, -r, minZ2);
    initv(bmax, r, r, maxZ2);
}

//////////////////////////////////////////// H Y P E R B O L O I D  ///////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Method				:	CHyperboloid
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CHyperboloid::CHyperboloid(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, const float *p1a, const float *p2a, float anglea) : CSurface(a, x) {
    atomicIncrement(stats.numGprims);

    umax = anglea;
    movvv(p1, p1a);
    movvv(p2, p2a);

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = NULL;

    computeObjectBound(bmin, bmax, p1, p2, umax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Method				:	CHyperboloid
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CHyperboloid::CHyperboloid(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, const float *p10, const float *p20, float angle0, const float *p11, const float *p21, float angle1) : CSurface(a, x) {
    vector tbmin, tbmax;

    atomicIncrement(stats.numGprims);

    umax = angle0;
    movvv(p1, p10);
    movvv(p2, p20);

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = new float[7];
    movvv(&nextData[0], p11);
    movvv(&nextData[3], p21);
    nextData[6] = angle1;

    computeObjectBound(bmin, bmax, p1, p2, umax);
    computeObjectBound(tbmin, tbmax, &nextData[0], &nextData[3], nextData[6]);
    addBox(bmin, bmax, tbmin);
    addBox(bmin, bmax, tbmax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Method				:	~CHyperboloid
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CHyperboloid::~CHyperboloid() {
    atomicDecrement(stats.numGprims);

    if (parameters != NULL)
        delete parameters;
    if (nextData != NULL)
        delete[] nextData;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Method				:	intersect
// Description			:	Compute the intersection with a ray
// Return Value			:	TRUE if intersects
// Comments				:
void CHyperboloid::intersect(CShadingContext *context, CRay *rv) {

    if (checkRayGuard(rv, context))
        return;

    vector oFrom, oDir;
    transform(oFrom, oDir, xform, rv);

    float *from = oFrom;
    float *dir = oDir;
    unsigned int ns, i;
    double ts[2];
    double P[3];
    double a, b, c;
    vector p1, p2;
    float umax;
    vector Nt;

    if (nextData != NULL) {
        interpolatev(p1, this->p1, &nextData[0], rv->time);
        interpolatev(p2, this->p2, &nextData[3], rv->time);
        umax = (float)(this->umax * (1.0 - (double)rv->time) + nextData[6] * (double)rv->time);
    }
    else {
        movvv(p1, this->p1);
        movvv(p2, this->p2);
        umax = this->umax;
    }

    const double dx = p2[COMP_X] - p1[COMP_X];
    const double dy = p2[COMP_Y] - p1[COMP_Y];
    const double dz = p2[COMP_Z] - p1[COMP_Z];

    if (p1[COMP_Z] == p2[COMP_Z]) {
        // We're a disc
        ns = 1;
        ts[0] = (p1[COMP_Z] - from[COMP_Z]) / dir[COMP_Z];
    }
    else {
        double x;
        double y;
        double zmin, d, dmin;

        if ((dx * dx + dy * dy) < C_EPSILON) { // Make sure we don't crash if we're a cylinder
            x = p1[COMP_X];
            y = p1[COMP_Y];
            zmin = p1[COMP_Z];
        }
        else {
            double tmin = (-dx * p1[COMP_X] - dy * p1[COMP_Y]) / (dx * dx + dy * dy); // Nearest we come to the  z axis
            x = p1[COMP_X] + dx * tmin;
            y = p1[COMP_Y] + dy * tmin;
            zmin = p1[COMP_Z] + dz * tmin;
        }

        dmin = sqrt(x * x + y * y);

        if (fabs(p2[COMP_Z] - zmin) > fabs(p1[COMP_Z] - zmin)) {
            d = sqrt((p2[COMP_X] * p2[COMP_X] + p2[COMP_Y] * p2[COMP_Y]) - dmin * dmin) / (p2[COMP_Z] - zmin);
        }
        else {
            d = sqrt((p1[COMP_X] * p1[COMP_X] + p1[COMP_Y] * p1[COMP_Y]) - dmin * dmin) / (p1[COMP_Z] - zmin);
        }

        a = dir[COMP_X] * dir[COMP_X] + dir[COMP_Y] * dir[COMP_Y] - dir[COMP_Z] * dir[COMP_Z] * d * d;
        b = 2 * dir[COMP_X] * from[COMP_X] + 2 * dir[COMP_Y] * from[COMP_Y] - 2 * dir[COMP_Z] * (from[COMP_Z] - zmin) * d * d;
        c = from[COMP_X] * from[COMP_X] + from[COMP_Y] * from[COMP_Y] - dmin * dmin - (from[COMP_Z] - zmin) * (from[COMP_Z] - zmin) * d * d;

        if (a == 0) {
            if (b == 0)
                return;

            ns = 1;
            ts[0] = -c / b;
        }
        else {
            if ((ns = solveQuadric<double>(a, b, c, ts)) == 0)
                return;
        }
    }

    for (i = 0; i < ns; i++) {
        double t = ts[i];
        double x, y;
        double ustart;
        double u, v;

        if (t >= rv->t)
            return;
        if (t <= rv->tmin)
            continue;

        P[0] = from[0] + dir[0] * t;
        P[1] = from[1] + dir[1] * t;
        P[2] = from[2] + dir[2] * t;

        if (p1[COMP_Z] < p2[COMP_Z]) {
            if (P[COMP_Z] < p1[COMP_Z])
                continue;
            if (P[COMP_Z] > p2[COMP_Z])
                continue;
            v = (P[COMP_Z] - p1[COMP_Z]) / (p2[COMP_Z] - p1[COMP_Z]);
        }
        else if (p1[COMP_Z] > p2[COMP_Z]) {
            if (P[COMP_Z] < p2[COMP_Z])
                continue;
            if (P[COMP_Z] > p1[COMP_Z])
                continue;
            v = (P[COMP_Z] - p1[COMP_Z]) / (p2[COMP_Z] - p1[COMP_Z]);
        }
        else {
            const double r1 = sqrt(p1[COMP_X] * p1[COMP_X] + p1[COMP_Y] * p1[COMP_Y]);
            const double r2 = sqrt(p2[COMP_X] * p2[COMP_X] + p2[COMP_Y] * p2[COMP_Y]);
            const double r = sqrt(P[COMP_X] * P[COMP_X] + P[COMP_Y] * P[COMP_Y]);

            // We're a disc
            v = (r - r1) / (r2 - r1);
            if (v < 0)
                continue;

            if (v > 1)
                continue;
        }

        x = (float)(p1[COMP_X] * (1.0 - v) + p2[COMP_X] * (double)v);
        y = (float)(p1[COMP_Y] * (1.0 - v) + p2[COMP_Y] * (double)v);

        u = atan2(P[COMP_Y], P[COMP_X]);
        ustart = atan2(y, x);

        if (u < 0)
            u = 2 * C_PI + u;
        if (ustart < 0)
            ustart = 2 * C_PI + ustart;

        u -= ustart;
        if (u < 0)
            u = 2 * C_PI + u;
        if (umax < 0) {
            u = u - 2 * C_PI;
            if (u < umax)
                continue;
        }
        else {
            if (u > umax)
                continue;
        }

        {
            Nt[COMP_X] = (float)((p2[COMP_Z] - p1[COMP_Z]) * umax * P[COMP_X]);
            Nt[COMP_Y] = (float)((p2[COMP_Z] - p1[COMP_Z]) * umax * P[COMP_Y]);
            Nt[COMP_Z] = (float)(-umax * ((-1 + v) * p1[COMP_X] * p1[COMP_X] +
                                          p1[COMP_X] * (p2[COMP_X] - 2 * v * p2[COMP_X]) +
                                          v * (p2[COMP_X] * p2[COMP_X] + (p2[COMP_Y] - p1[COMP_Y]) * (p2[COMP_Y] - p1[COMP_Y])) +
                                          p1[COMP_Y] * (p2[COMP_Y] - p1[COMP_Y])));

            if ((attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ xform->flip) {
                mulvf(Nt, -1);
            }

            if (!(attributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED)) {
                if (dotvv(oDir, Nt) > 0)
                    continue;
            }
        }

        if (!regionKeepsHit(this, context, rv, t, Nt))
            continue;

        rv->object = this;
        rv->u = (float)(u / umax);
        rv->v = (float)(v);
        rv->t = (float)t;
        mulmn(rv->N, xform->to, Nt);

        return;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Method				:	sample
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CHyperboloid::sample(int start, int numVertices, float **varying, float ***, unsigned int &up) const {
    float *u = varying[VARIABLE_U] + start;
    float *v = varying[VARIABLE_V] + start;
    float *sinu = (float *)alloca(numVertices * 2 * sizeof(float));
    float *cosu = sinu + numVertices;

    if ((nextData != NULL) && (!(up & (PARAMETER_BEGIN_SAMPLE | PARAMETER_END_SAMPLE)))) {
        float *umax = (float *)alloca(numVertices * sizeof(float));
        float *p1 = (float *)alloca(numVertices * sizeof(float) * 3);
        float *p2 = (float *)alloca(numVertices * sizeof(float) * 3);
        float *dx = (float *)alloca(numVertices * sizeof(float));
        float *dy = (float *)alloca(numVertices * sizeof(float));
        float *dz = (float *)alloca(numVertices * sizeof(float));
        const float *tp1 = this->p1;
        float *np1 = &nextData[0];
        const float *tp2 = this->p2;
        float *np2 = &nextData[3];
        float *cp1 = p1;
        float *cp2 = p2;
        const float *time = varying[VARIABLE_TIME] + start;

        // Precompute the sin/cos for u points
        for (int i = 0; i < numVertices; ++i) {
            umax[i] = (float)(this->umax * (1.0 - (double)time[i]) + nextData[6] * (double)time[i]);
            cp1[COMP_X] = (float)(tp1[COMP_X] * (1.0 - (double)time[i]) + np1[COMP_X] * (double)time[i]);
            cp1[COMP_Y] = (float)(tp1[COMP_Y] * (1.0 - (double)time[i]) + np1[COMP_Y] * (double)time[i]);
            cp1[COMP_Z] = (float)(tp1[COMP_Z] * (1.0 - (double)time[i]) + np1[COMP_Z] * (double)time[i]);
            cp2[COMP_X] = (float)(tp2[COMP_X] * (1.0 - (double)time[i]) + np2[COMP_X] * (double)time[i]);
            cp2[COMP_Y] = (float)(tp2[COMP_Y] * (1.0 - (double)time[i]) + np2[COMP_Y] * (double)time[i]);
            cp2[COMP_Z] = (float)(tp2[COMP_Z] * (1.0 - (double)time[i]) + np2[COMP_Z] * (double)time[i]);
            dx[i] = cp2[COMP_X] - cp1[COMP_X];
            dy[i] = cp2[COMP_Y] - cp1[COMP_Y];
            dz[i] = cp2[COMP_Z] - cp1[COMP_Z];
            cosu[i] = cosf(u[i] * umax[i]);
            sinu[i] = sinf(u[i] * umax[i]);
            cp1 += 3;
            cp2 += 3;
        }

        // Set u/v/P
        cp1 = p1;
        cp2 = p2;
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int i = 0; i < numVertices; ++i) {
            const float x = cp1[COMP_X] + dx[i] * v[i];
            const float y = cp1[COMP_Y] + dy[i] * v[i];
            const float z = cp1[COMP_Z] + dz[i] * v[i];

            dest[COMP_X] = (x * cosu[i] - y * sinu[i]);
            dest[COMP_Y] = (x * sinu[i] + y * cosu[i]);
            dest[COMP_Z] = z;
            dest += 3;
            cp1 += 3;
            cp2 += 3;
        }

        // Set dPdu/dPdv/Ng if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU | PARAMETER_NG)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;
            float *Ng = varying[VARIABLE_NG] + start * 3;

            cp1 = p1;
            cp2 = p2;
            for (int i = 0; i < numVertices; ++i) {
                const float x = cp1[COMP_X] + dx[i] * v[i];
                const float y = cp1[COMP_Y] + dy[i] * v[i];
                dPdu[COMP_X] = -x * umax[i] * sinu[i] - y * umax[i] * cosu[i];
                dPdu[COMP_Y] = x * umax[i] * cosu[i] - y * umax[i] * sinu[i];
                dPdu[COMP_Z] = 0;

                dPdv[COMP_X] = dx[i] * cosu[i] - dy[i] * sinu[i];
                dPdv[COMP_Y] = dx[i] * sinu[i] + dy[i] * cosu[i];
                dPdv[COMP_Z] = dz[i];

                crossvv(Ng, dPdu, dPdv);

                dPdu += 3;
                dPdv += 3;
                Ng += 3;
            }
        }
    }
    else {
        const float *p1 = this->p1;
        const float *p2 = this->p2;
        float umax = this->umax;

        if ((up & PARAMETER_END_SAMPLE) && (nextData != NULL)) {
            p1 = &nextData[0];
            p2 = &nextData[3];
            umax = nextData[6];
        }

        const float dx = p2[COMP_X] - p1[COMP_X];
        const float dy = p2[COMP_Y] - p1[COMP_Y];
        const float dz = p2[COMP_Z] - p1[COMP_Z];

        // Set u/v/P
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int i = 0; i < numVertices; ++i) {
            const float x = p1[COMP_X] + dx * v[i];
            const float y = p1[COMP_Y] + dy * v[i];
            const float z = p1[COMP_Z] + dz * v[i];
            cosu[i] = cosf(u[i] * umax);
            sinu[i] = sinf(u[i] * umax);
            dest[COMP_X] = (x * cosu[i] - y * sinu[i]);
            dest[COMP_Y] = (x * sinu[i] + y * cosu[i]);
            dest[COMP_Z] = z;
            dest += 3;
        }

        // Set dPdu/dPdv/Ng if needed
        if (up & (PARAMETER_DPDU | PARAMETER_DPDU | PARAMETER_NG)) {
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;
            float *Ng = varying[VARIABLE_NG] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                const float x = p1[COMP_X] + dx * v[i];
                const float y = p1[COMP_Y] + dy * v[i];
                dPdu[COMP_X] = -x * umax * sinu[i] - y * umax * cosu[i];
                dPdu[COMP_Y] = x * umax * cosu[i] - y * umax * sinu[i];
                dPdu[COMP_Z] = 0;

                dPdv[COMP_X] = dx * cosu[i] - dy * sinu[i];
                dPdv[COMP_Y] = dx * sinu[i] + dy * cosu[i];
                dPdv[COMP_Z] = dz;

                crossvv(Ng, dPdu, dPdv);

                dPdu += 3;
                dPdv += 3;
                Ng += 3;
            }
        }
    }

    // Do we need to flip orientation?
    if (xform->flip == 1) {
        float *N = varying[VARIABLE_NG] + start * 3;

        for (int i = numVertices; i > 0; --i, N += 3) {
            mulvf(N, -1);
        }
    }

    // Set dPdtime
    if (up & PARAMETER_DPDTIME) {
        // This is where we'll store the result
        float *dest = varying[VARIABLE_DPDTIME] + start * 3;

        // Are we moving?
        if (nextData != NULL) {
            // Get the xform matrices to move to the camera system
            const float *fromStart = xform->from;
            const float *fromEnd = (xform->next != NULL ? xform->next->from : xform->from);

            assert(u == (varying[VARIABLE_U] + start));
            assert(v == (varying[VARIABLE_V] + start));

            // Compute the xforms
            for (int i = 0; i < numVertices; ++i, dest += 3) {
                const float *p1End = nextData;
                const float *p2End = nextData + 3;
                const float umaxEnd = nextData[6];
                const float dx = p2[COMP_X] - p1[COMP_X];
                const float dy = p2[COMP_Y] - p1[COMP_Y];
                const float dz = p2[COMP_Z] - p1[COMP_Z];
                const float dxEnd = p2End[COMP_X] - p1End[COMP_X];
                const float dyEnd = p2End[COMP_Y] - p1End[COMP_Y];
                const float dzEnd = p2End[COMP_Z] - p1End[COMP_Z];
                vector Pstart, Pend;

                // Compute the position at the start
                {
                    const float x = p1[COMP_X] + dx * v[i];
                    const float y = p1[COMP_Y] + dy * v[i];
                    const float z = p1[COMP_Z] + dz * v[i];
                    Pstart[COMP_X] = (x * cosf(u[i] * umax) - y * sinf(u[i] * umax));
                    Pstart[COMP_Y] = (x * sinf(u[i] * umax) + y * cosf(u[i] * umax));
                    Pstart[COMP_Z] = z;
                }
                mulmp(Pstart, fromStart, Pstart);

                // Compute the position at the end
                {
                    const float x = p1End[COMP_X] + dxEnd * v[i];
                    const float y = p1End[COMP_Y] + dyEnd * v[i];
                    const float z = p1End[COMP_Z] + dzEnd * v[i];
                    Pend[COMP_X] = (x * cosf(u[i] * umaxEnd) - y * sinf(u[i] * umaxEnd));
                    Pend[COMP_Y] = (x * sinf(u[i] * umaxEnd) + y * cosf(u[i] * umaxEnd));
                    Pend[COMP_Z] = z;
                }
                mulmp(Pend, fromEnd, Pend);

                // Compute the dPdtime
                subvv(dest, Pend, Pstart);
                mulvf(dest, CRenderer::invShutterTime);
            }
        }
        else {
            // Get the xform matrices
            const float *fromStart = xform->from + element(0, 3);
            const float *fromEnd = (xform->next != NULL ? xform->next->from + element(0, 3) : xform->from + element(0, 3));
            vector D;
            subvv(D, fromEnd, fromStart);
            mulvf(D, CRenderer::invShutterTime);

            // dPdtime is zero
            for (int i = 0; i < numVertices; ++i, dest += 3)
                movvv(dest, D);
        }
    }

    // Transform the points
    transformPoints();

    // Turn off the parameters we computed
    up &= ~parametersF;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Method				:	interpolate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CHyperboloid::interpolate(int numVertices, float **varying, float ***locals) const {
    if (parameters != NULL)
        parameters->dispatch(numVertices, varying, locals);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CHyperboloid::instantiate(CAttributes *a, CXform *x, CRiInterface *cx) const {
    CXform *nx = new CXform(x);

    nx->concat(xform); // Concetenate the local xform

    if (a == NULL)
        a = attributes;

    if (nextData == NULL)
        cx->addObject(new CHyperboloid(a, nx, parameters, parametersF, p1, p2, umax));
    else
        cx->addObject(new CHyperboloid(a, nx, parameters, parametersF, p1, p2, umax, nextData, nextData + 3, nextData[6]));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Method				:	computeObjectBound
// Description			:	Compute the bounding box in the object space
// Return Value			:	-
// Comments				:
void CHyperboloid::computeObjectBound(float *bmin, float *bmax, float *p1, float *p2, float) {
    float d = p1[COMP_X] * p1[COMP_X] + p1[COMP_Y] * p1[COMP_Y];
    float d2 = p2[COMP_X] * p2[COMP_X] + p2[COMP_Y] * p2[COMP_Y];

    if (d2 > d)
        d = d2;

    d = sqrtf(d);

    // FIXME: smaller bound is possible
    bmin[COMP_X] = -d;
    bmin[COMP_Y] = -d;
    if (p2[COMP_Z] < p1[COMP_Z]) {
        bmin[COMP_Z] = p2[COMP_Z];
    }
    else {
        bmin[COMP_Z] = p1[COMP_Z];
    }
    bmax[COMP_X] = +d;
    bmax[COMP_Y] = +d;
    if (p2[COMP_Z] > p1[COMP_Z]) {
        bmax[COMP_Z] = p2[COMP_Z];
    }
    else {
        bmax[COMP_Z] = p1[COMP_Z];
    }
}

//////////////////////////////////////////// T O R O I D  ///////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Method				:	CToroid
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CToroid::CToroid(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float rmina, float rmaxa, float vmina, float vmaxa, float umaxa) : CSurface(a, x) {
    atomicIncrement(stats.numGprims);

    rmin = rmina;
    rmax = rmaxa;
    vmin = vmina;
    vmax = vmaxa;
    umax = umaxa;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = NULL;

    computeObjectBound(bmin, bmax, rmin, rmax, vmin, vmax, umax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Method				:	CToroid
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CToroid::CToroid(CAttributes *a, CXform *x, CParameter *c, unsigned int pf, float rmin0, float rmax0, float vmin0, float vmax0, float umax0, float rmin1, float rmax1, float vmin1, float vmax1, float umax1) : CSurface(a, x) {
    vector tbmin, tbmax;

    atomicIncrement(stats.numGprims);

    rmin = rmin0;
    rmax = rmax0;
    vmin = vmin0;
    vmax = vmax0;
    umax = umax0;

    parameters = c;
    parametersF = PARAMETER_P | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_DPDTIME | pf;

    nextData = new float[5];
    nextData[0] = rmin1;
    nextData[1] = rmax1;
    nextData[2] = vmin1;
    nextData[3] = vmax1;
    nextData[4] = umax1;

    computeObjectBound(bmin, bmax, rmin, rmax, vmin, vmax, umax);
    computeObjectBound(tbmin, tbmax, nextData[0], nextData[1], nextData[2], nextData[3], nextData[4]);
    addBox(bmin, bmax, tbmin);
    addBox(bmin, bmax, tbmax);
    xform->transformBound(bmin, bmax);
    makeBound(bmin, bmax);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Method				:	~CToroid
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CToroid::~CToroid() {
    atomicDecrement(stats.numGprims);

    if (parameters != NULL)
        delete parameters;
    if (nextData != NULL)
        delete[] nextData;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Method				:	intersect
// Description			:	Compute the intersection with a ray
// Return Value			:	TRUE if intersects
// Comments				:
void CToroid::intersect(CShadingContext *context, CRay *rv) {

    if (checkRayGuard(rv, context))
        return;

    vector oFrom, oDir;
    transform(oFrom, oDir, xform, rv);

    // nextData holds {rmin, rmax, vmin, vmax, umax}, as the constructor and
    // sample() lay it out
    double rmin, rmax, umax, vmin, vmax;

    if (nextData != NULL) {
        rmin = (float)(this->rmin * (1.0 - (double)rv->time) + nextData[0] * (double)rv->time);
        rmax = (float)(this->rmax * (1.0 - (double)rv->time) + nextData[1] * (double)rv->time);
        vmin = (float)(this->vmin * (1.0 - (double)rv->time) + nextData[2] * (double)rv->time);
        vmax = (float)(this->vmax * (1.0 - (double)rv->time) + nextData[3] * (double)rv->time);
        umax = (float)(this->umax * (1.0 - (double)rv->time) + nextData[4] * (double)rv->time);
    }
    else {
        rmin = this->rmin;
        rmax = this->rmax;
        vmin = this->vmin;
        vmax = this->vmax;
        umax = this->umax;
    }

    // The torus about the z axis (as sample() generates it), with major radius
    // R = rmax and tube radius r = rmin:
    //   (|P|^2 + R^2 - r^2)^2 - 4 R^2 (x^2 + y^2) = 0
    // For P = F + t D (D as transformed, not normalized, so t stays the ray's
    // own parameter): |P|^2 = a t^2 + b t + c
    const double F[3] = {oFrom[COMP_X], oFrom[COMP_Y], oFrom[COMP_Z]};
    const double D[3] = {oDir[COMP_X], oDir[COMP_Y], oDir[COMP_Z]};
    const double R2 = rmax * rmax;
    const double a = D[0] * D[0] + D[1] * D[1] + D[2] * D[2];
    const double b = 2 * (F[0] * D[0] + F[1] * D[1] + F[2] * D[2]);
    const double e = F[0] * F[0] + F[1] * F[1] + F[2] * F[2] + R2 - rmin * rmin;
    double c[5], s[4];
    c[4] = a * a;
    c[3] = 2 * a * b;
    c[2] = b * b + 2 * a * e - 4 * R2 * (D[0] * D[0] + D[1] * D[1]);
    c[1] = 2 * b * e - 8 * R2 * (F[0] * D[0] + F[1] * D[1]);
    c[0] = e * e - 4 * R2 * (F[0] * F[0] + F[1] * F[1]);

    if (c[4] == 0)
        return;

    const int ns = solveQuartic<double>(c, s);

    // Nearest first. An insertion sort, not std::sort: GCC 13 analyses
    // std::sort's 16-element insertion threshold against s[4] and reports a
    // false -Warray-bounds (an error under -Werror), though ns <= 4
    for (int i = 1; i < ns; i++)
        for (int j = i; j > 0 && s[j] < s[j - 1]; j--)
            std::swap(s[j], s[j - 1]);

    const double vlo = std::min(vmin, vmax), vhi = std::max(vmin, vmax);

    for (int i = 0; i < ns; i++) {
        const double t = s[i];

        if (t <= rv->tmin)
            continue;
        if (t >= rv->t)
            return;

        const double P[3] = {F[0] + D[0] * t, F[1] + D[1] * t, F[2] + D[2] * t};

        // Sweep around the axis
        double u = (rmax > 0) ? atan2(P[COMP_Y], P[COMP_X]) : atan2(-P[COMP_Y], -P[COMP_X]);
        if (u < 0)
            u = 2 * C_PI + u;
        if (umax < 0) {
            u = u - 2 * C_PI;
            if (u < umax)
                continue;
        }
        else {
            if (u > umax)
                continue;
        }

        // Angle around the tube, brought into [vmin, vmax] if any turn of it is
        const double rho = sqrt(P[COMP_X] * P[COMP_X] + P[COMP_Y] * P[COMP_Y]) - fabs(rmax);
        double v = (rmin > 0) ? atan2(P[COMP_Z], rho) : atan2(-P[COMP_Z], -rho);
        bool inRange = false;
        for (int k = -2; k <= 2 && !inRange; k++) {
            const double w = v + k * 2 * C_PI;
            if (w >= vlo && w <= vhi) {
                v = w;
                inRange = true;
            }
        }
        if (!inRange)
            continue;

        // The normal sample() derives (dPdu x dPdv)
        vector Nt;
        const double scale = rmin * umax * (vmax - vmin) * (rmax + rmin * cos(v));
        Nt[COMP_X] = (float)(scale * cos(u) * cos(v));
        Nt[COMP_Y] = (float)(scale * sin(u) * cos(v));
        Nt[COMP_Z] = (float)(scale * sin(v));

        if ((attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ xform->flip) {
            mulvf(Nt, -1);
        }

        if (!(attributes->flags & ATTRIBUTES_FLAGS_DOUBLE_SIDED)) {
            if (dotvv(oDir, Nt) > 0)
                continue;
        }

        if (!regionKeepsHit(this, context, rv, t, Nt))
            continue;

        rv->object = this;
        rv->u = (float)(u / umax);
        rv->v = (float)((v - vmin) / (vmax - vmin));
        rv->t = (float)t;
        mulmn(rv->N, xform->to, Nt);
        return;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Method				:	sample
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CToroid::sample(int start, int numVertices, float **varying, float ***, unsigned int &up) const {
    float *u = varying[VARIABLE_U] + start;
    float *v = varying[VARIABLE_V] + start;
    float *sinu = (float *)alloca(numVertices * 4 * sizeof(float));
    float *cosu = sinu + numVertices;
    float *sinv = cosu + numVertices;
    float *cosv = sinv + numVertices;

    if ((nextData != NULL) && (!(up & (PARAMETER_BEGIN_SAMPLE | PARAMETER_END_SAMPLE)))) {
        float *rmin = (float *)alloca(numVertices * sizeof(float));
        float *rmax = (float *)alloca(numVertices * sizeof(float));
        float *vmin = (float *)alloca(numVertices * sizeof(float));
        float *vmax = (float *)alloca(numVertices * sizeof(float));
        float *umax = (float *)alloca(numVertices * sizeof(float));
        float *time = varying[VARIABLE_TIME] + start;

        // Precompute the sin/cos for u points
        for (int i = 0; i < numVertices; ++i) {
            rmin[i] = (float)(this->rmin * (1.0 - (double)time[i]) + nextData[0] * (double)time[i]);
            rmax[i] = (float)(this->rmax * (1.0 - (double)time[i]) + nextData[1] * (double)time[i]);
            vmin[i] = (float)(this->vmin * (1.0 - (double)time[i]) + nextData[2] * (double)time[i]);
            vmax[i] = (float)(this->vmax * (1.0 - (double)time[i]) + nextData[3] * (double)time[i]);
            umax[i] = (float)(this->umax * (1.0 - (double)time[i]) + nextData[4] * (double)time[i]);

            cosu[i] = cosf(u[i] * umax[i]);
            sinu[i] = sinf(u[i] * umax[i]);
            cosv[i] = cosf(v[i] * (vmax[i] - vmin[i]) + vmin[i]);
            sinv[i] = sinf(v[i] * (vmax[i] - vmin[i]) + vmin[i]);
        }

        // Set u/v/P
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int i = 0; i < numVertices; ++i) {
            const float r = rmin[i] * cosv[i];
            dest[COMP_Z] = rmin[i] * sinv[i];
            dest[COMP_X] = (rmax[i] + r) * cosu[i];
            dest[COMP_Y] = (rmax[i] + r) * sinu[i];

            dest += 3;
        }

        // Set Ng if needed
        if (up & (PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV)) {
            float *Ng = varying[VARIABLE_NG] + start * 3;
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                const float r = rmin[i] * cosv[i];
                dPdu[COMP_Z] = 0;
                dPdu[COMP_X] = -(rmax[i] + r) * sinu[i] * umax[i];
                dPdu[COMP_Y] = (rmax[i] + r) * cosu[i] * umax[i];
                dPdv[COMP_Z] = rmin[i] * cosv[i] * (vmax[i] - vmin[i]);
                dPdv[COMP_X] = (-rmin[i] * sinv[i] * (vmax[i] - vmin[i])) * cosu[i];
                dPdv[COMP_Y] = (-rmin[i] * sinv[i] * (vmax[i] - vmin[i])) * sinu[i];
                crossvv(Ng, dPdu, dPdv);
                Ng += 3;
                dPdu += 3;
                dPdv += 3;
            }
        }
    }
    else {
        float rmin = this->rmin;
        float rmax = this->rmax;
        float vmin = this->vmin;
        float vmax = this->vmax;
        float umax = this->umax;

        if ((up & PARAMETER_END_SAMPLE) && (nextData != NULL)) {
            rmin = nextData[0];
            rmax = nextData[1];
            vmin = nextData[2];
            vmax = nextData[3];
            umax = nextData[4];
        }

        // Set u/v/P
        float *dest = varying[VARIABLE_P] + start * 3;
        for (int i = 0; i < numVertices; ++i) {
            cosu[i] = cosf(u[i] * umax);
            sinu[i] = sinf(u[i] * umax);
            cosv[i] = cosf(v[i] * (vmax - vmin) + vmin);
            sinv[i] = sinf(v[i] * (vmax - vmin) + vmin);
            const float r = rmin * cosv[i];
            dest[COMP_Z] = rmin * sinv[i];
            dest[COMP_X] = (rmax + r) * cosu[i];
            dest[COMP_Y] = (rmax + r) * sinu[i];

            dest += 3;
        }

        // Set Ng if needed
        if (up & (PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV)) {
            float *Ng = varying[VARIABLE_NG] + start * 3;
            float *dPdu = varying[VARIABLE_DPDU] + start * 3;
            float *dPdv = varying[VARIABLE_DPDV] + start * 3;

            for (int i = 0; i < numVertices; ++i) {
                const float r = rmin * cosv[i];
                dPdu[COMP_Z] = 0;
                dPdu[COMP_X] = -(rmax + r) * sinu[i] * umax;
                dPdu[COMP_Y] = (rmax + r) * cosu[i] * umax;
                dPdv[COMP_Z] = rmin * cosv[i] * (vmax - vmin);
                dPdv[COMP_X] = (-rmin * sinv[i] * (vmax - vmin)) * cosu[i];
                dPdv[COMP_Y] = (-rmin * sinv[i] * (vmax - vmin)) * sinu[i];
                crossvv(Ng, dPdu, dPdv);

                Ng += 3;
                dPdu += 3;
                dPdv += 3;
            }
        }
    }

    if (xform->flip == 1) {
        float *N = varying[VARIABLE_NG] + start * 3;

        for (int i = numVertices; i > 0; --i, N += 3) {
            mulvf(N, -1);
        }
    }

    // Set dPdtime
    if (up & PARAMETER_DPDTIME) {
        // This is where we'll store the result
        float *dest = varying[VARIABLE_DPDTIME] + start * 3;

        // Are we moving?
        if (nextData != NULL) {
            // Get the xform matrices to move to the camera system
            const float *fromStart = xform->from;
            const float *fromEnd = (xform->next != NULL ? xform->next->from : xform->from);

            // Sanity check
            assert(u == (varying[VARIABLE_U] + start));
            assert(v == (varying[VARIABLE_V] + start));

            // Compute the xforms
            for (int i = 0; i < numVertices; ++i, dest += 3) {
                vector Pstart, Pend;

                // Compute the position at the start
                const float cosu = cosf(u[i] * umax);
                const float sinu = sinf(u[i] * umax);
                const float cosv = cosf(v[i] * (vmax - vmin) + vmin);
                const float sinv = sinf(v[i] * (vmax - vmin) + vmin);
                const float r = rmin * cosv;
                Pstart[COMP_Z] = rmin * sinv;
                Pstart[COMP_X] = (rmax + r) * cosu;
                Pstart[COMP_Y] = (rmax + r) * sinu;
                mulmp(Pstart, fromStart, Pstart);

                // Compute the position at the end
                {
                    const float rminEnd = nextData[0];
                    const float rmaxEnd = nextData[1];
                    const float vminEnd = nextData[2];
                    const float vmaxEnd = nextData[3];
                    const float umaxEnd = nextData[4];
                    const float cosu = cosf(u[i] * umaxEnd);
                    const float sinu = sinf(u[i] * umaxEnd);
                    const float cosv = cosf(v[i] * (vmaxEnd - vminEnd) + vminEnd);
                    const float sinv = sinf(v[i] * (vmaxEnd - vminEnd) + vminEnd);
                    const float r = rminEnd * cosv;
                    Pend[COMP_Z] = rminEnd * sinv;
                    Pend[COMP_X] = (rmaxEnd + r) * cosu;
                    Pend[COMP_Y] = (rmaxEnd + r) * sinu;
                    mulmp(Pend, fromEnd, Pend);
                }

                // Compute the dPdtime
                subvv(dest, Pend, Pstart);
                mulvf(dest, CRenderer::invShutterTime);
            }
        }
        else {
            // Get the xform matrices
            const float *fromStart = xform->from + element(0, 3);
            const float *fromEnd = (xform->next != NULL ? xform->next->from + element(0, 3) : xform->from + element(0, 3));
            vector D;
            subvv(D, fromEnd, fromStart);
            mulvf(D, CRenderer::invShutterTime);

            // dPdtime is zero
            for (int i = 0; i < numVertices; ++i, dest += 3)
                movvv(dest, D);
        }
    }

    // Transform the points
    transformPoints();

    // Note that the normals are computed by dPdu x dPdv, meaning that the normal vector may be degenerate
    normalFix();
    tangentFix();

    up &= ~parametersF;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Method				:	interpolate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CToroid::interpolate(int numVertices, float **varying, float ***locals) const {
    if (parameters != NULL)
        parameters->dispatch(numVertices, varying, locals);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Method				:	instantiate
// Description			:	See object.h
// Return Value			:	-
// Comments				:
void CToroid::instantiate(CAttributes *a, CXform *x, CRiInterface *cx) const {
    CXform *nx = new CXform(x);

    nx->concat(xform); // Concetenate the local xform

    if (a == NULL)
        a = attributes;

    if (nextData == NULL)
        cx->addObject(new CToroid(a, nx, parameters, parametersF, rmin, rmax, vmin, vmax, umax));
    else
        cx->addObject(new CToroid(a, nx, parameters, parametersF, rmin, rmax, vmin, vmax, umax, nextData[0], nextData[1], nextData[2], nextData[3], nextData[4]));
}

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Method				:	computeObjectBound
// Description			:	Compute the bounding box in the object space
// Return Value			:	-
// Comments				:
void CToroid::computeObjectBound(float *bmin, float *bmax, float rmin, float rmax, float, float, float) {
    float r = absf(rmin) + absf(rmax);

    bmin[COMP_X] = -r;
    bmin[COMP_Y] = -r;
    bmax[COMP_X] = r;
    bmax[COMP_Y] = r;
    bmin[COMP_Z] = -absf(rmin);
    bmax[COMP_Z] = absf(rmin);
}

//////////////////////////////////// C R O S S I N G S   /   I M P L I E D   S O L I D S ////////////////////////////////////
//
// Spec 021 (deferred CSG): unsigned ray-crossing counts and the closed solid
// each quadric implies (FR-021). Both are geometric queries in the quadric's
// object space at shutter open: they ignore ray flags, culling, LOD,
// displacement and orientation.

// Camera-space ray / point to object space (shutter open)
static void quadricObjectRay(const CXform *xform, const float *from, const float *dir, double *oFrom, double *oDir) {
    const float *m = xform->to;
    for (int i = 0; i < 3; i++) {
        oFrom[i] = (double)m[element(i, 0)] * from[0] + (double)m[element(i, 1)] * from[1] + (double)m[element(i, 2)] * from[2] + m[element(i, 3)];
        oDir[i] = (double)m[element(i, 0)] * dir[0] + (double)m[element(i, 1)] * dir[1] + (double)m[element(i, 2)] * dir[2];
    }
    const double w = (double)m[element(3, 0)] * from[0] + (double)m[element(3, 1)] * from[1] + (double)m[element(3, 2)] * from[2] + m[element(3, 3)];
    if (w != 1 && w != 0)
        for (int i = 0; i < 3; i++)
            oFrom[i] /= w;
}

static void quadricObjectPoint(const CXform *xform, const float *P, double *oP) {
    const float dir[3] = {0, 0, 0};
    double unused[3];
    quadricObjectRay(xform, P, dir, oP, unused);
}

// Is the azimuth of (x, y) within the sweep [0, umax] (umax < 0 sweeps the
// other way), measured from the azimuth `start`? Mirrors the intersectors.
static int quadricInSweep(double x, double y, double umax, int negative, double start = 0) {
    if (umax >= 2 * C_PI || umax <= -2 * C_PI)
        return TRUE;
    double u = negative ? atan2(-y, -x) : atan2(y, x);
    if (u < 0)
        u += 2 * C_PI;
    u -= start;
    if (u < 0)
        u += 2 * C_PI;
    if (umax < 0)
        return u - 2 * C_PI >= umax;
    return u <= umax;
}

// Roots t of a t^2 + b t + c = 0 (a may vanish)
static int quadricRoots(double a, double b, double c, double *t) {
    const double size = fabs(a) + fabs(b);
    if (fabs(a) <= 1e-14 * size || a == 0) {
        if (b == 0)
            return 0;
        t[0] = -c / b;
        return 1;
    }
    const double disc = b * b - 4 * a * c;
    if (disc < 0)
        return 0;
    const double sq = sqrt(disc);
    const double q = -0.5 * (b + (b >= 0 ? sq : -sq));
    int n = 0;
    t[n++] = q / a;
    if (q != 0)
        t[n++] = c / q;
    return n;
}

static double quadricZ(const double *f, const double *d, double t, int k) {
    return f[k] + d[k] * t;
}

/////////////////////////////// Sphere

int CSphere::countCrossings(CShadingContext *, const float *from, const float *dir) const {
    double f[3], d[3], t[2];
    quadricObjectRay(xform, from, dir, f, d);
    const int n = quadricRoots(d[0] * d[0] + d[1] * d[1] + d[2] * d[2], 2 * (d[0] * f[0] + d[1] * f[1] + d[2] * f[2]),
                               f[0] * f[0] + f[1] * f[1] + f[2] * f[2] - (double)r * r, t);
    // The angles are stored as floats: asin(+-1) overshoots +-pi/2 slightly,
    // so allow the poles a hair of slack
    const double z0 = r * sin((double)vmin), z1 = r * sin((double)vmax), slack = 1e-6 * fabs((double)r);
    const double zlo = std::min(z0, z1) - slack, zhi = std::max(z0, z1) + slack;
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (t[i] <= 0)
            continue;
        const double z = quadricZ(f, d, t[i], 2);
        if (z < zlo || z > zhi)
            continue;
        if (!quadricInSweep(quadricZ(f, d, t[i], 0), quadricZ(f, d, t[i], 1), umax, r < 0))
            continue;
        count++;
    }
    return count;
}

int CSphere::impliedSolidContains(const float *P) const {
    double o[3];
    quadricObjectPoint(xform, P, o);
    const double z0 = r * sin((double)vmin), z1 = r * sin((double)vmax);
    if (o[2] < std::min(z0, z1) || o[2] > std::max(z0, z1))
        return FALSE;
    if (o[0] * o[0] + o[1] * o[1] + o[2] * o[2] >= (double)r * r)
        return FALSE;
    return quadricInSweep(o[0], o[1], umax, r < 0);
}

/////////////////////////////// Disk

int CDisk::countCrossings(CShadingContext *, const float *from, const float *dir) const {
    double f[3], d[3];
    quadricObjectRay(xform, from, dir, f, d);
    if (d[2] == 0)
        return 0;
    const double t = (z - f[2]) / d[2];
    if (t <= 0)
        return 0;
    const double x = f[0] + d[0] * t, y = f[1] + d[1] * t;
    if (x * x + y * y > (double)r * r)
        return 0;
    return quadricInSweep(x, y, umax, r < 0);
}

/////////////////////////////// Cone

int CCone::countCrossings(CShadingContext *, const float *from, const float *dir) const {
    double f[3], d[3], t[2];
    quadricObjectRay(xform, from, dir, f, d);
    const double h = height, rr = (double)r * r;
    int n;
    if (h * h < C_EPSILON) {
        if (d[2] == 0)
            return 0;
        t[0] = -f[2] / d[2];
        n = 1;
    }
    else
        n = quadricRoots(h * h * (d[0] * d[0] + d[1] * d[1]) - rr * d[2] * d[2],
                         h * h * 2 * (f[0] * d[0] + f[1] * d[1]) - rr * 2 * (f[2] - h) * d[2],
                         h * h * (f[0] * f[0] + f[1] * f[1]) - rr * (f[2] - h) * (f[2] - h), t);
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (t[i] <= 0)
            continue;
        const double z = quadricZ(f, d, t[i], 2);
        if (z < std::min(0.0, h) || z > std::max(0.0, h))
            continue;
        const double x = quadricZ(f, d, t[i], 0), y = quadricZ(f, d, t[i], 1);
        if (h * h < C_EPSILON && x * x + y * y > rr)
            continue;
        if (!quadricInSweep(x, y, umax, r < 0))
            continue;
        count++;
    }
    return count;
}

int CCone::impliedSolidContains(const float *P) const {
    double o[3];
    quadricObjectPoint(xform, P, o);
    const double h = height;
    if (h == 0)
        return FALSE;
    const double s = o[2] / h; // 0 at the base, 1 at the apex
    if (s <= 0 || s >= 1)
        return FALSE;
    const double rad = fabs((double)r) * (1 - s);
    if (o[0] * o[0] + o[1] * o[1] >= rad * rad)
        return FALSE;
    return quadricInSweep(o[0], o[1], umax, r < 0);
}

/////////////////////////////// Paraboloid

int CParaboloid::countCrossings(CShadingContext *, const float *from, const float *dir) const {
    double f[3], d[3], t[2];
    quadricObjectRay(xform, from, dir, f, d);
    const double k = (double)r * r / zmax;
    const int n = quadricRoots(d[0] * d[0] + d[1] * d[1], 2 * (d[0] * f[0] + d[1] * f[1]) - k * d[2], f[0] * f[0] + f[1] * f[1] - k * f[2], t);
    const double zlo = std::min((double)zmin, (double)zmax), zhi = std::max((double)zmin, (double)zmax);
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (t[i] <= 0)
            continue;
        const double z = quadricZ(f, d, t[i], 2);
        if (z < zlo || z > zhi)
            continue;
        if (!quadricInSweep(quadricZ(f, d, t[i], 0), quadricZ(f, d, t[i], 1), umax, r < 0))
            continue;
        count++;
    }
    return count;
}

int CParaboloid::impliedSolidContains(const float *P) const {
    double o[3];
    quadricObjectPoint(xform, P, o);
    if (o[2] <= std::min((double)zmin, (double)zmax) || o[2] >= std::max((double)zmin, (double)zmax))
        return FALSE;
    if (o[0] * o[0] + o[1] * o[1] >= (double)r * r * o[2] / zmax)
        return FALSE;
    return quadricInSweep(o[0], o[1], umax, r < 0);
}

/////////////////////////////// Cylinder

int CCylinder::countCrossings(CShadingContext *, const float *from, const float *dir) const {
    double f[3], d[3], t[2];
    quadricObjectRay(xform, from, dir, f, d);
    const double a = d[0] * d[0] + d[1] * d[1];
    if (a == 0)
        return 0; // Along the axis: never crosses the wall
    const int n = quadricRoots(a, 2 * (d[0] * f[0] + d[1] * f[1]), f[0] * f[0] + f[1] * f[1] - (double)r * r, t);
    const double zlo = std::min((double)zmin, (double)zmax), zhi = std::max((double)zmin, (double)zmax);
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (t[i] <= 0)
            continue;
        const double z = quadricZ(f, d, t[i], 2);
        if (z < zlo || z > zhi)
            continue;
        if (!quadricInSweep(quadricZ(f, d, t[i], 0), quadricZ(f, d, t[i], 1), umax, r < 0))
            continue;
        count++;
    }
    return count;
}

int CCylinder::impliedSolidContains(const float *P) const {
    double o[3];
    quadricObjectPoint(xform, P, o);
    if (o[2] <= std::min((double)zmin, (double)zmax) || o[2] >= std::max((double)zmin, (double)zmax))
        return FALSE;
    if (o[0] * o[0] + o[1] * o[1] >= (double)r * r)
        return FALSE;
    return quadricInSweep(o[0], o[1], umax, r < 0);
}

/////////////////////////////// Hyperboloid

// The sweep of a hyperboloid starts at the azimuth of its generating line at
// height z, so it twists with z
static double hyperboloidStart(const float *p1, const float *p2, double v) {
    const double x = p1[0] * (1 - v) + p2[0] * v, y = p1[1] * (1 - v) + p2[1] * v;
    double s = atan2(y, x);
    if (s < 0)
        s += 2 * C_PI;
    return s;
}

int CHyperboloid::countCrossings(CShadingContext *, const float *from, const float *dir) const {
    if (p1[2] == p2[2])
        return 0; // An annulus: no crossings counted for a flat ring (encloses nothing)
    double f[3], d[3], t[2];
    quadricObjectRay(xform, from, dir, f, d);

    // Radius at height z: rho(z)^2 = |p1 + (p2 - p1) s|_xy^2, s = (z - z1) / (z2 - z1)
    // x^2 + y^2 - (A s^2 + B s + C) = 0, with s linear in t
    const double ex = p2[0] - p1[0], ey = p2[1] - p1[1], dz = p2[2] - p1[2];
    const double A = ex * ex + ey * ey, B = 2 * (p1[0] * ex + p1[1] * ey), C = (double)p1[0] * p1[0] + (double)p1[1] * p1[1];
    const double s0 = (f[2] - p1[2]) / dz, s1 = d[2] / dz; // s = s0 + s1 t
    const int n = quadricRoots(d[0] * d[0] + d[1] * d[1] - A * s1 * s1, 2 * (f[0] * d[0] + f[1] * d[1]) - 2 * A * s0 * s1 - B * s1,
                               f[0] * f[0] + f[1] * f[1] - A * s0 * s0 - B * s0 - C, t);
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (t[i] <= 0)
            continue;
        const double v = s0 + s1 * t[i];
        if (v < 0 || v > 1)
            continue;
        if (!quadricInSweep(quadricZ(f, d, t[i], 0), quadricZ(f, d, t[i], 1), umax, FALSE, hyperboloidStart(p1, p2, v)))
            continue;
        count++;
    }
    return count;
}

int CHyperboloid::impliesSolid() const {
    return p1[2] != p2[2];
}

int CHyperboloid::impliedSolidContains(const float *P) const {
    if (p1[2] == p2[2])
        return FALSE;
    double o[3];
    quadricObjectPoint(xform, P, o);
    const double v = (o[2] - p1[2]) / ((double)p2[2] - p1[2]);
    if (v <= 0 || v >= 1)
        return FALSE;
    const double x = p1[0] * (1 - v) + p2[0] * v, y = p1[1] * (1 - v) + p2[1] * v;
    if (o[0] * o[0] + o[1] * o[1] >= x * x + y * y)
        return FALSE;
    return quadricInSweep(o[0], o[1], umax, FALSE, hyperboloidStart(p1, p2, v));
}

/////////////////////////////// Torus (rmax: major radius, rmin: tube radius, v: tube angle phi)

// Is the tube angle of (rho - R, z) within [vmin, vmax]?
static int torusInPhi(double rr, double z, double R, double vmin, double vmax) {
    if (fabs(vmax - vmin) >= 2 * C_PI)
        return TRUE;
    double v = atan2(z, rr - R);
    const double lo = std::min(vmin, vmax), hi = std::max(vmin, vmax);
    for (int k = -2; k <= 2; k++) {
        const double w = v + k * 2 * C_PI;
        if (w >= lo && w <= hi)
            return TRUE;
    }
    return FALSE;
}

int CToroid::countCrossings(CShadingContext *, const float *from, const float *dir) const {
    double f[3], dd[3];
    quadricObjectRay(xform, from, dir, f, dd);
    const double len = sqrt(dd[0] * dd[0] + dd[1] * dd[1] + dd[2] * dd[2]);
    if (len == 0)
        return 0;
    const double d[3] = {dd[0] / len, dd[1] / len, dd[2] / len};

    // (|P|^2 + R^2 - r^2)^2 - 4 R^2 (x^2 + y^2) = 0, the torus about z
    const double R = fabs((double)rmax), r = fabs((double)rmin);
    const double b = 2 * (f[0] * d[0] + f[1] * d[1] + f[2] * d[2]);
    const double e = f[0] * f[0] + f[1] * f[1] + f[2] * f[2] + R * R - r * r;
    const double dxy = d[0] * d[0] + d[1] * d[1], fdxy = f[0] * d[0] + f[1] * d[1], fxy = f[0] * f[0] + f[1] * f[1];
    double c[5], s[4];
    c[4] = 1;
    c[3] = 2 * b;
    c[2] = b * b + 2 * e - 4 * R * R * dxy;
    c[1] = 2 * b * e - 8 * R * R * fdxy;
    c[0] = e * e - 4 * R * R * fxy;
    const int n = solveQuartic<double>(c, s); // The same roots CToroid::intersect() uses
    int count = 0;
    for (int i = 0; i < n; i++) {
        if (s[i] <= 0)
            continue;
        const double x = f[0] + d[0] * s[i], y = f[1] + d[1] * s[i], z = f[2] + d[2] * s[i];
        if (!quadricInSweep(x, y, umax, rmax < 0))
            continue;
        if (!torusInPhi(sqrt(x * x + y * y), z, R, vmin, vmax))
            continue;
        count++;
    }
    return count;
}

int CToroid::impliedSolidContains(const float *P) const {
    double o[3];
    quadricObjectPoint(xform, P, o);
    const double R = fabs((double)rmax), r = fabs((double)rmin);
    const double qx = sqrt(o[0] * o[0] + o[1] * o[1]) - R, qz = o[2];
    if (qx * qx + qz * qz >= r * r)
        return FALSE;
    if (!quadricInSweep(o[0], o[1], umax, rmax < 0))
        return FALSE;
    if (fabs((double)vmax - vmin) >= 2 * C_PI)
        return TRUE;

    // A tube cut across: the circular segment between the arc [vmin, vmax]
    // and the chord joining its ends, on the arc's side of the chord
    const double ax = r * cos((double)vmin), az = r * sin((double)vmin);
    const double bx = r * cos((double)vmax), bz = r * sin((double)vmax);
    const double m = 0.5 * ((double)vmin + vmax);
    const double mx = r * cos(m), mz = r * sin(m);
    const double ex = bx - ax, ez = bz - az;
    const double sideP = ex * (qz - az) - ez * (qx - ax);
    const double sideM = ex * (mz - az) - ez * (mx - ax);
    return (sideP > 0) == (sideM > 0) && sideP != 0;
}
