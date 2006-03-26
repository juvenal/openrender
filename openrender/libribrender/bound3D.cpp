//  openRender
//
//  bound3D.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sun Dec 28 2003
//
//  Original Development:
//    (C) 2006 by Juvenal A. Silva Jr. <juvenal.silva@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software, you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: bound3D.cpp,v 1.3 2006/03/26 15:51:23 juvenal.silva Exp $
//

// C includes
#include <math.h>

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "utils.h"
#include "point3D.h"
#include "matrix4D.h"
#include "bound3D.h"

// bound3D implementation
// *******************************
// Constructors
// ========================================================
Bound3D::Bound3D (const Point3D _pMin, const Point3D _pMax) {
    this->pMin = _pMin;
    this->pMax = _pMax;
}

Bound3D::Bound3D (float _xMin, float _xMax, float _yMin, float _yMax, float _zMin, float _zMax) {
    this->pMin = Point3D (_xMin, _yMin, _zMin);
    this->pMax = Point3D (_xMax, _yMax, _zMax);
}

// Arithmetic
// ========================================================
// Multiplication
Bound3D operator * (Matrix4D transform, Bound3D bound) {
    Point3D vertex[8];
    int count;

    vertex[0] = Point3D(bound.pMin.getxcomp(), bound.pMin.getycomp(), bound.pMin.getzcomp());
    vertex[1] = Point3D(bound.pMin.getxcomp(), bound.pMin.getycomp(), bound.pMax.getzcomp());
    vertex[2] = Point3D(bound.pMin.getxcomp(), bound.pMax.getycomp(), bound.pMin.getzcomp());
    vertex[3] = Point3D(bound.pMin.getxcomp(), bound.pMax.getycomp(), bound.pMax.getzcomp());
    vertex[4] = Point3D(bound.pMax.getxcomp(), bound.pMin.getycomp(), bound.pMin.getzcomp());
    vertex[5] = Point3D(bound.pMax.getxcomp(), bound.pMin.getycomp(), bound.pMax.getzcomp());
    vertex[6] = Point3D(bound.pMax.getxcomp(), bound.pMax.getycomp(), bound.pMin.getzcomp());
    vertex[7] = Point3D(bound.pMax.getxcomp(), bound.pMax.getycomp(), bound.pMax.getzcomp());
    for(count = 0; count < 8; count++) {
        vertex[count] = transform * vertex[count];
    }
    pMinVertex = Point3D (util::min(util::min(util::min(vertex[0].getxcomp(), vertex[1].getxcomp()), util::min(util::min(vertex[2].getxcomp()), util::min())),
                                    util::min()));
    pMaxVertex = Point3D ()
    newBox.min.x = util::min(util::min(util::min(p[0].x, p[1].x), util::min(p[2].x, p[3].x)), util::min(util::min(p[4].x, p[5].x), util::min(p[6].x, p[7].x)));
    newBox.min.y = util::min(util::min(util::min(p[0].y, p[1].y), util::min(p[2].y, p[3].y)), util::min(util::min(p[4].y, p[5].y), util::min(p[6].y, p[7].y)));
    newBox.min.z = util::min(util::min(util::min(p[0].z, p[1].z), util::min(p[2].z, p[3].z)), util::min(util::min(p[4].z, p[5].z), util::min(p[6].z, p[7].z)));
    newBox.max.x = util::max(util::max(util::max(p[0].x, p[1].x), util::max(p[2].x, p[3].x)), util::max(util::max(p[4].x, p[5].x), util::max(p[6].x, p[7].x)));
    newBox.max.y = util::max(util::max(util::max(p[0].y, p[1].y), util::max(p[2].y, p[3].y)), util::max(util::max(p[4].y, p[5].y), util::max(p[6].y, p[7].y)));
    newBox.max.z = util::max(util::max(util::max(p[0].z, p[1].z), util::max(p[2].z, p[3].z)), util::max(util::max(p[4].z, p[5].z), util::max(p[6].z, p[7].z)));
    return newBox();
}
