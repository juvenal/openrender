//  openRender
//
//  matrix4D.cpp - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sat Sep 28 2002
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
//  $Id: matrix4D.cpp,v 1.3 2002/10/21 02:16:36 juvenal Exp $
//

// C includes
#include <math.h>

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "vector3D.h"
#include "matrix4D.h"

// Constructors
// ========================================================
matrix4D::matrix4D () {
  identity ();
}

// Member Functions
// ========================================================
bool matrix4D::identity () {
  int i, j;

  for ( i = 0; i < 4; i++)
    for ( j = 0; j < 4; j++)
      element[i][j] = ( i == j) ? 1 : 0;

  return true;
}

bool matrix4D::scale ( vector3D s) {
  matrix4D m;

  m[0][0] = s.getxcomp();
  m[1][1] = s.getycomp();
  m[2][2] = s.getzcomp();
  *this = ( *this) * m;

  return true;
}

bool matrix4D::translate ( vector3D t) {
  matrix4D m;

  m[0][3] = t.getxcomp();
  m[1][3] = t.getycomp();
  m[2][3] = t.getzcomp();
  *this = ( *this) * m;

  return true;
}

bool matrix4D::rotate ( vector3D r) {
  matrix4D m;
  float rx = DEGTORAD * r.getxcomp();
  float ry = DEGTORAD * r.getycomp();
  float rz = DEGTORAD * r.getzcomp();

  m[1][1] = cos ( rx);
  m[1][2] = sin ( rx);
  m[2][1] = -sin ( rx);
  m[2][2] = cos ( rx);
  *this = ( *this) * m;

  m.identity ();
  m[0][0] = cos ( ry);
  m[0][1] = sin ( ry);
  m[2][1] = -sin ( ry);
  m[2][2] = cos ( ry);
  *this = ( *this) * m;

  m.identity ();
  m[0][0] = cos ( rz);
  m[0][1] = sin ( rz);
  m[1][0] = -sin ( rz);
  m[1][1] = cos ( rz);
  *this = ( *this) * m;

  return true;
}

matrix4D matrix4D::inverse () {
  matrix4D r;
  matrix4D t = *this;
  int i, j, k;
  float scale;

  // Get zero's in non-identity positions
  for ( k = 0; k < 4; k++) {
    for ( j = 0; j < 4; j++) {
      if ( j != k) {
        scale = -t[j][k] / t[k][k];
        for ( i = 0; i < 4; i++) {
          t[j][i] += scale * t[k][i];
          r[j][i] += scale * r[k][i];
        }
      }
    }
  }

  // Now scale each row so diagonal elements are 1
  for ( j = 0; j < 4; j++) {
    scale = 1 / t[j][j];
    for ( i = 0; i < 4; i++) {
      t[j][i] = t[j][i] * scale;
      r[j][i] = r[j][i] * scale;
    }
  }

  return r;
}

// Friend Functions - Arithmetic
// ========================================================
vector3D operator * ( matrix4D m, vector3D v) {
  vector3D r;

  r.setxcomp (( m[0][0] * v.getxcomp()) + ( m[0][1] * v.getycomp()) +
              ( m[0][2] * v.getzcomp()) + m[0][3]);
  r.setycomp (( m[1][0] * v.getxcomp()) + ( m[1][1] * v.getycomp()) +
              ( m[1][2] * v.getzcomp()) + m[1][3]);
  r.setzcomp (( m[2][0] * v.getxcomp()) + ( m[2][1] * v.getycomp()) +
              ( m[2][2] * v.getzcomp()) + m[2][3]);
  return r;

}

matrix4D operator * ( matrix4D a, matrix4D b) {
  int i,j,k;
  matrix4D r;

  for ( i = 0; i < 4; i++) {
    for ( j = 0; j < 4; j++) {
      r[i][j] = 0;
      for ( k = 0; k < 4; k++) {
        r[i][j] += a[i][k] * b[k][j];
      }
    }
  }
  return r;
}

// Stream output
// ========================================================
std::ostream &operator << ( std::ostream &io, matrix4D &m) {
  int i, j;

  for ( i = 0; i < 4; i++) {
    std::cout << "[";
    for ( j = 0; j < 4; j++) {
      std::cout << m[i][j];
      if ( j < 3)
        std::cout << " ";
    }
    std::cout << "]\n";
  }
  return io;
}
