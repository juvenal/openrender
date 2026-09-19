/**
 * Project: openRender
 *
 * File: quadrics.h
 *
 * Description:
 *   This file defines the interface for quadrics.
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
//  File				:	quadrics.h
//  Classes				:	Quadric surfaces
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef QUADRICS_H
#define QUADRICS_H

#include "common/algebra.h"
#include "common/global.h"
#include "object.h"
#include "pl.h"
#include "shader.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CSphere
// Description			:	Encapsulates a sphere
// Comments				:
class CSphere : public CSurface {
    public:
        CSphere(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float);
        CSphere(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float, float, float, float, float);
        ~CSphere();

        void intersect(CShadingContext *, CRay *);
        int moving() const { return (nextData != NULL) | (xform->next != NULL); }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            int depthValue = 3 - depth;
            int clampedDepth;
            if (1 > depthValue) {
                clampedDepth = 1;
            } else {
                clampedDepth = depthValue;
            }
            minDivU = minDivV = clampedDepth;
            return 0;
        }

        void wireData(float &r, float &umax, float &vmin, float &vmax) const {
            r = this->r; umax = this->umax; vmin = this->vmin; vmax = this->vmax;
        }

    private:
        CParameter *parameters;
        unsigned int parametersF;
        float r, umax, vmin, vmax;
        float *nextData;

        void computeObjectBound(float *, float *, float, float, float, float);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDisk
// Description			:	Encapsulates a disk
// Comments				:
class CDisk : public CSurface {
    public:
        CDisk(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float);
        CDisk(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float, float, float);
        ~CDisk();

        void intersect(CShadingContext *, CRay *);
        int moving() const { return (nextData != NULL) | (xform->next != NULL); }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        void wireData(float &r, float &z, float &umax) const {
            r = this->r; z = this->z; umax = this->umax;
        }

    private:
        CParameter *parameters;
        unsigned int parametersF;
        float r, z, umax;
        float *nextData;

        void computeObjectBound(float *, float *, float, float, float);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCone
// Description			:	Encapsulates a cone
// Comments				:
class CCone : public CSurface {
    public:
        CCone(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float);
        CCone(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float, float, float);
        ~CCone();

        void intersect(CShadingContext *, CRay *);
        int moving() const { return (nextData != NULL) | (xform->next != NULL); }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            int depthValue2 = 3 - depth;
            if (1 > depthValue2) {
                minDivU = 1;
            } else {
                minDivU = depthValue2;
            }
            minDivV = 1;
            return 0;
        }

        void wireData(float &r, float &height, float &umax) const {
            r = this->r; height = this->height; umax = this->umax;
        }

    private:
        CParameter *parameters;
        unsigned int parametersF;
        float r, height, umax;
        float *nextData;

        void computeObjectBound(float *, float *, float, float, float);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CParaboloid
// Description			:	Encapsulates a paraboloid
// Comments				:
class CParaboloid : public CSurface {
    public:
        CParaboloid(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float);
        CParaboloid(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float, float, float, float, float);
        ~CParaboloid();

        void intersect(CShadingContext *, CRay *);
        int moving() const { return (nextData != NULL) | (xform->next != NULL); }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            int depthValue = 3 - depth;
            int clampedDepth;
            if (1 > depthValue) {
                clampedDepth = 1;
            } else {
                clampedDepth = depthValue;
            }
            minDivU = minDivV = clampedDepth;
            return 0;
        }

        void wireData(float &r, float &zmin, float &zmax, float &umax) const {
            r = this->r; zmin = this->zmin; zmax = this->zmax; umax = this->umax;
        }

    private:
        CParameter *parameters;
        unsigned int parametersF;
        float r, zmin, zmax, umax;
        float *nextData;

        void computeObjectBound(float *, float *, float, float, float, float);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCylinder
// Description			:	Encapsulates a cylinder
// Comments				:
class CCylinder : public CSurface {
    public:
        CCylinder(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float);
        CCylinder(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float, float, float, float, float);
        ~CCylinder();

        void intersect(CShadingContext *, CRay *);
        int moving() const { return (nextData != NULL) | (xform->next != NULL); }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            int depthValue2 = 3 - depth;
            if (1 > depthValue2) {
                minDivU = 1;
            } else {
                minDivU = depthValue2;
            }
            minDivV = 1;
            return 0;
        }

        void wireData(float &r, float &zmin, float &zmax, float &umax) const {
            r = this->r; zmin = this->zmin; zmax = this->zmax; umax = this->umax;
        }

    private:
        CParameter *parameters;
        unsigned int parametersF;
        float r, zmin, zmax, umax;
        float *nextData;

        void computeObjectBound(float *, float *, float, float, float, float);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CHyperboloid
// Description			:	Encapsulates a hyperboloid
// Comments				:
class CHyperboloid : public CSurface {
    public:
        CHyperboloid(CAttributes *, CXform *, CParameter *, unsigned int, const float *, const float *, float);
        CHyperboloid(CAttributes *, CXform *, CParameter *, unsigned int, const float *, const float *, float, const float *, const float *, float);
        ~CHyperboloid();

        void intersect(CShadingContext *, CRay *);
        int moving() const { return (nextData != NULL) | (xform->next != NULL); }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            int depthValue = 3 - depth;
            int clampedDepth;
            if (1 > depthValue) {
                clampedDepth = 1;
            } else {
                clampedDepth = depthValue;
            }
            minDivU = minDivV = clampedDepth;
            return 0;
        }

        void wireData(const float *&p1, const float *&p2, float &umax) const {
            p1 = this->p1; p2 = this->p2; umax = this->umax;
        }

    private:
        CParameter *parameters;
        unsigned int parametersF;
        vector p1, p2;
        float umax;
        float *nextData;

        void computeObjectBound(float *, float *, float *, float *, float);
};

///////////////////////////////////////////////////////////////////////
// Class				:	CToroid
// Description			:	Encapsulates a torus
// Comments				:
class CToroid : public CSurface {
    public:
        CToroid(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float, float);
        CToroid(CAttributes *, CXform *, CParameter *, unsigned int, float, float, float, float, float, float, float, float, float, float);
        ~CToroid();

        void intersect(CShadingContext *, CRay *);
        int moving() const { return (nextData != NULL) | (xform->next != NULL); }
        void sample(int, int, float **, float ***, unsigned int &) const;
        void interpolate(int, float **, float ***) const;
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        int getDicingStats(int depth, int &minDivU, int &minDivV) const {
            int depthValue = 3 - depth;
            int clampedDepth;
            if (1 > depthValue) {
                clampedDepth = 1;
            } else {
                clampedDepth = depthValue;
            }
            minDivU = minDivV = clampedDepth;
            return 2;
        }

        void wireData(float &rmax, float &rmin, float &vmin, float &vmax, float &umax) const {
            rmax = this->rmax; rmin = this->rmin; vmin = this->vmin; vmax = this->vmax; umax = this->umax;
        }

    private:
        CParameter *parameters;
        unsigned int parametersF;
        float rmin, rmax, vmin, vmax, umax;
        float *nextData;

        void computeObjectBound(float *, float *, float, float, float, float, float);
};

#endif
