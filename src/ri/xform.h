/**
 * Project: Pixie
 *
 * File: xform.h
 *
 * Description:
 *   This file defines the interface for xform.
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
//  File				:	xform.h
//  Classes				:	CXform
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef XFORM_H
#define XFORM_H

#include "common/algebra.h"
#include "common/global.h"
#include "refCounter.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CXform
// Description			:	This class encapsulates the transformation.
//							"from" is the transformation matrix from the
//							local system to the global system and "to"
//							is the transformation from global to local system
// Comments				:
class CXform : public CRefCounter {
    public:
        CXform();
        CXform(CXform *);
        virtual ~CXform();

        CXform *next; // points to the next xform in case of motion blur

        void restore(const CXform *xform);

        void identity();                     // Transformations
        void translate(float, float, float); // Concetenate from right
        void rotate(float, float, float, float);
        void scale(float, float, float);
        void skew(float, float, float, float, float, float, float);
        void concat(CXform *);
        void invert();
        void transformBound(float *, float *) const;
        void invTransformBound(float *, float *) const;
        void updateBound(float *, float *, int, const float *);

        int normalFlip() {
            if (flip == -1) {
                if (determinantm(from) < 0)
                    flip = TRUE;
                else
                    flip = FALSE;
            }

            return flip;
        }

        matrix from, to; // The actual transformation matrices
        int flip;        // TRUE if the determinant is < 0
};

void transformBound(float *bmax, float *bmin, const float *to, const float *obmin, const float *obmax);
#endif
