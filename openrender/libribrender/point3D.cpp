//  openRender
//
//  point3D.cpp - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Fri Nov 15 2002
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
//  $Id: point3D.cpp,v 1.6 2006/03/26 15:51:23 juvenal.silva Exp $
//

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "point3D.h"

// point3D implementations
// *******************************
// Constructors
// ========================================================
Point3D::Point3D (float _x, float _y, float _z) {
    this->x = _x;
    this->y = _y;
    this->z = _z;
}

Point3D::Point3D (Vector3D v) {
    this->x = v.getxcomp ();
    this->y = v.getycomp ();
    this->z = v.getzcomp ();
}
