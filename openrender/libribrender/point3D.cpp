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
//  $Id: point3D.cpp,v 1.2 2003/06/08 19:13:51 juvenal Exp $
//

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "vector3D.h"
#include "point3D.h"

// point3D implementations
// *******************************
// Constructors
// ========================================================
point3D::point3D ( float _x, float _y, float _z) {
  this->x = _x;
  this->y = _y;
  this->z = _z;
}

point3D::point3D ( vector3D v) {
  this->x = v.getxcomp ();
  this->y = v.getycomp ();
  this->z = v.getzcomp ();
}
