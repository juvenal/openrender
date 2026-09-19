/**
 * Project: openRender
 *
 * File: blobbyRepeller.h
 *
 * Description:
 *   This file defines the interface for blobbyRepeller.
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
//  File				:	blobbyRepeller.h
//  Classes				:	CBlobbyRepeller
//  Description			:	Opcode 1003, the repelling ground plane: a
//							context-free depth-file read plus the
//							repulsion profile (spec 015, research
//							Decision 5).
//
//							The depth file is read once, whole, at
//							RiBlobby time. CTexture::lookupz() is
//							deliberately NOT used: it reaches
//							lookupPixel(), which dereferences
//							context->thread, and no CShadingContext exists
//							at build time -- a null context there is a
//							crash, not a degradation.
//
////////////////////////////////////////////////////////////////////////
#ifndef BLOBBYREPELLER_H
#define BLOBBYREPELLER_H

#include "common/algebra.h"
#include "common/global.h"

///////////////////////////////////////////////////////////////////////
// The repulsion profile, from PRMan Application Note #31.
//
// The published C for bump() is corrupted: it reads `if(r=2.) return 0.;`,
// an assignment rather than a comparison, so it is always true and the
// function would return zero unconditionally -- silently deleting the
// bulge term. It compiles without a diagnostic in C. The guard below is
// reconstructed from the note's prose, which states the bump "is exactly
// zero outside the range 0 <= z <= 2C". The polynomial itself is intact
// and satisfies its stated anchors bump(0)=0, bump(1)=1, bump(2)=0, all
// three of which tests/unit/blobby/test_repeller.cpp asserts precisely so
// a transcription slip cannot pass unnoticed.
///////////////////////////////////////////////////////////////////////
float blobbyBumpProfile(float r);
float blobbyEase(float r);
float blobbyRepulsion(float z, float A, float B, float C, float D);

///////////////////////////////////////////////////////////////////////
// Class				:	CBlobbyRepeller
// Description			:	One loaded depth file plus its shaping
//							parameters.
// Comments				:	A missing or unreadable file is not fatal: a
//							diagnostic naming the file is issued, the
//							field contributes zero, and the render
//							continues (US7 scenario 4).
///////////////////////////////////////////////////////////////////////
class CBlobbyRepeller {
    public:
        // `toWorld` is the primitive's own local-to-world transform,
        // composed with the depth file's view transforms at load so an
        // object-space point can be taken straight to the map's frame --
        // the same composition CRenderer::environmentLoad() does for a
        // shadow map. NULL means the primitive is already in world space.
        CBlobbyRepeller(const char *fileName, const float *toWorld, float A, float B, float C, float D);
        ~CBlobbyRepeller();

        int isValid() const { return valid; }

        // Field contribution at an object-space point, and its gradient.
        // Returns zero with a zero gradient when the file did not load.
        float evaluate(const float *P, float *gradient) const;

    private:
        // Height of the evaluation point above the depth surface, measured
        // in the view direction the depth file was generated in.
        int heightAbove(const float *P, float *z) const;

        char *fileName;
        float *depth;
        int width;
        int height;
        matrix toNDC;
        matrix toCamera;
        float A, B, C, D;
        int valid;
};

#endif
