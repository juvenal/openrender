//  openRender
//
//  primitive.cpp - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sun Oct 27 2002
//
//  Original Development:
//    (C) 2002 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software, you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: primitive.cpp,v 1.5 2004/07/14 18:55:46 juvenal Exp $
//

// C++ includes
#include <iostream>
#include <iomanip>

// STL includes
#include <list>

// Reyes includes
#include "ri.h"
#include "useful.h"
#include "camera.h"
#include "microgrid.h"
#include "surface.h"
#include "displacement_shaders.h"
#include "boundbox.h"
#include "primitive.h"

// Basic virtual implementations (must be overriden by real primitives)
// =======================================================================
void Primitive::dump () {
    std::cout << "dump(): Unknown primitive!" << std::endl;
}

bool Primitive::transformToEyeSpace (Matrix4D t_position, Matrix4D t_vector) {
    std::cout << "transformToEyeSpace(): Unknown primitive!" << std::endl;
}

void Primitive::doDice (MicroGrid &microgrid, int us, int vs) {
    std::cout << "doDice(): Unknown primitive!" << std::endl;
}

bool Primitive::splitable () {
    std::cout << "splitable(): Unknown primitive!" << std::endl;
    return false;
}

void Primitive::split (list<Primitive*> &primlist) {
    std::cout << "split(): Unknown primitive!" << std::endl;
}

bool Primitive::eyeBound (BoundBox &bb) {
    std::cout << "eyeBound(): Unknown primitive!" << std::endl;
    return false;
}

// Common methods to all primitives (not virtual methods)
// =======================================================================
void Primitive::dice (MicroGrid &microgrid, float xscale, float yscale) {
    int us, vs;

    estimateGridSize (xscale, yscale, us, vs);
    doDice (microgrid, us, vs);
}

bool Primitive::diceable (float xscale, float yscale) {
    int us, vs;

    estimateGridSize (xscale, yscale, us, vs);
    if (us * vs <= RiGlobal.options.maxMicroGridSize) {
        return true;
    }
    else {
        return false;
    }
}

void Primitive::estimateGridSize (float xscale, float yscale, int &us, int &vs) {
    MicroGrid microgrid;
    float zmin, zmax, maxusize, maxvsize;

    doDice (microgrid, 10, 10);
    microgrid.Statistics (zmin, zmax, maxusize, maxvsize);
    us = MAX (4, (int) (10 * xscale * maxusize / (RiCurrent.shadingAttributes.shadingRate * RiGlobal.display.xSamplingRate)));
    vs = MAX (4, (int) (10 * yscale * maxvsize / (RiCurrent.shadingAttributes.shadingRate * RiGlobal.display.ySamplingRate)));
    // Make size a power of 2
    us = 1 << (int) (logn (2, us));
    vs = 1 << (int) (logn (2, vs));
    us = MAX (us, vs);
    vs = MAX (us, vs);
}

