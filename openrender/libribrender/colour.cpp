//  openRender
//
//  colour.cpp - {Summary}
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
//  $Id: colour.cpp,v 1.5 2002/10/28 16:11:24 juvenal Exp $
//

// C includes
#include <math.h>

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "utils.h"
#include "vector3D.h"
#include "matrix4D.h"
#include "colour.h"

// Constructors
// ========================================================
colour::colour ( void) {
  this->R = 0;
  this->G = 0;
  this->B = 0;
}

colour::colour ( float c1, float c2, float c3) {
  // Clamp values on assign
  this->R = util::clampVal ( c1, 0, 1);
  this->G = util::clampVal ( c2, 0, 1);
  this->B = util::clampVal ( c3, 0, 1);
}

// Member Functions
// ========================================================
vector3D colour::getRGB ( void) {
  return vector3D ( this->R, this->G, this->B);
}

vector3D colour::getHLS ( void) {
  return vector3D ( 1, 1, 1);
}

vector3D colour::getHSV ( void) {
  return vector3D ( 1, 1, 1);
}

vector3D colour::getYIQ ( void) {
  float Y, I, Q;

  Y = 0.299 * this->R + 0.587 * this->G + 0.114 * this->B;
  I = 0.596 * this->R - 0.274 * this->G - 0.322 * this->B;
  Q = 0.212 * this->R - 0.523 * this->G - 0.311 * this->B;

  return vector3D ( Y, I, Q);
}

vector3D colour::getYUV ( void) {
  float Y, U, V;

  Y =  0.299 * this->R + 0.587 * this->G + 0.114 * this->B;
  U = -0.147 * this->R - 0.289 * this->G + 0.437 * this->B;
  V =  0.615 * this->R - 0.515 * this->G - 0.100 * this->B;

  return vector3D ( Y, U, V);
}

vector3D colour::getCMY ( void) {
  float C, M, Y;

  C = 1 - R;
  M = 1 - G;
  Y = 1 - B;

  return vector3D ( C, M, Y);
}

// Set colour to RGB values (default)
bool colour::setRGB ( vector3D &v) {
  this->R = util::clampVal ( v.getxcomp(), 0, 1);
  this->G = util::clampVal ( v.getycomp(), 0, 1);
  this->B = util::clampVal ( v.getzcomp(), 0, 1);

  return true;
}

// Set colour to
bool colour::setHLS ( vector3D &v) {
  this->R = util::clampVal ( 0, 0, 1);
  this->G = util::clampVal ( 0, 0, 1);
  this->B = util::clampVal ( 0, 0, 1);

  return true;
}

// Set colour to
bool colour::setHSV ( vector3D &v) {

  float f, p, q, t, H, S, V;
  int   i;

  // Copy some values
  H = v.getxcomp();
  S = v.getycomp();
  V = v.getzcomp();

  // If saturation is 0, RGB is 0,0,0
  if ( S == 0) {
    this->R = this->G = this->B = S;
  }
  else {
    if ( H == 360) {
      H = 0;
    }
    H = H / 60;
    i = ( int) floor ( H);
    f = H - i;
    p = V * ( 1 - S);
    q = V * ( 1 - ( S * f));
    t = V * ( 1 - ( S * ( 1 - f)));
    switch ( i) {
      case 0:
        this->R = V;
        this->G = t;
        this->B = p;
        break;
      case 1:
        this->R = q;
        this->G = V;
        this->B = p;
        break;
      case 2:
        this->R = p;
        this->G = V;
        this->B = t;
        break;
      case 3:
        this->R = p;
        this->G = q;
        this->B = V;
        break;
      case 4:
        this->R = t;
        this->G = p;
        this->B = V;
        break;
      case 5:
        this->R = V;
        this->G = p;
        this->B = q;
        break;
    }
  }
  return true;
}

// Set colour to
bool colour::setYIQ ( vector3D &v) {
  this->R = util::clampVal ( 1 * v.getxcomp() + 0.956 * v.getycomp() + 0.621 * v.getzcomp(), 0, 1);
  this->G = util::clampVal ( 1 * v.getxcomp() - 0.272 * v.getycomp() - 0.647 * v.getzcomp(), 0, 1);
  this->B = util::clampVal ( 1 * v.getxcomp() - 1.105 * v.getycomp() + 1.702 * v.getzcomp(), 0, 1);

  return true;
}

// Set colour to
bool colour::setYUV ( vector3D &v) {
  this->R = util::clampVal ( 1 * v.getxcomp() + 0.000 * v.getycomp() + 1.140 * v.getzcomp(), 0, 1);
  this->G = util::clampVal ( 1 * v.getxcomp() - 0.394 * v.getycomp() - 0.581 * v.getzcomp(), 0, 1);
  this->B = util::clampVal ( 1 * v.getxcomp() + 2.028 * v.getycomp() + 0.000 * v.getzcomp(), 0, 1);

  return true;
}

// Set colour to
bool colour::setCMY ( vector3D &v) {
  this->R = util::clampVal ( 1 - v.getxcomp(), 0, 1);
  this->G = util::clampVal ( 1 - v.getycomp(), 0, 1);
  this->B = util::clampVal ( 1 - v.getzcomp(), 0, 1);

  return true;
}

// Arithmetic
// ========================================================
// Common addition
colour colour::operator += ( colour a) {
  float _R, _G, _B;

  _R = util::clampVal ( this->R + a.R, 0, 1);
  _G = util::clampVal ( this->G + a.G, 0, 1);
  _B = util::clampVal ( this->B + a.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Common subtraction
colour colour::operator -= ( colour a) {
  float _R, _G, _B;

  _R = util::clampVal ( this->R - a.R, 0, 1);
  _G = util::clampVal ( this->G - a.G, 0, 1);
  _B = util::clampVal ( this->B - a.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Common multiply
colour colour::operator *= ( colour a) {
  float _R, _G, _B;

  _R = util::clampVal ( this->R * a.R, 0, 1);
  _G = util::clampVal ( this->G * a.G, 0, 1);
  _B = util::clampVal ( this->B * a.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Common division
colour colour::operator /= ( colour a) {
  float _R, _G, _B;

  _R = util::clampVal ( this->R / a.R, 0, 1);
  _G = util::clampVal ( this->G / a.G, 0, 1);
  _B = util::clampVal ( this->B / a.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Addition
colour operator + ( colour a, colour b) {
  float _R, _G, _B;

  _R = util::clampVal ( a.R + b.R, 0, 1);
  _G = util::clampVal ( a.G + b.G, 0, 1);
  _B = util::clampVal ( a.B + b.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Subtraction
colour operator - ( colour a, colour b) {
  float _R, _G, _B;

  _R = util::clampVal ( a.R - b.R, 0, 1);
  _G = util::clampVal ( a.G - b.G, 0, 1);
  _B = util::clampVal ( a.B - b.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Multiplication
colour operator * ( colour a, colour b) {
  float _R, _G, _B;

  _R = util::clampVal ( a.R * b.R, 0, 1);
  _G = util::clampVal ( a.G * b.G, 0, 1);
  _B = util::clampVal ( a.B * b.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Division
colour operator / ( colour a, colour b) {
  float _R, _G, _B;

  _R = util::clampVal ( a.R / b.R, 0, 1);
  _G = util::clampVal ( a.G / b.G, 0, 1);
  _B = util::clampVal ( a.B / b.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Scale 1
colour operator * ( float s, colour c) {
  float _R, _G, _B;

  _R = util::clampVal ( s * c.R, 0, 1);
  _G = util::clampVal ( s * c.G, 0, 1);
  _B = util::clampVal ( s * c.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Scale 2
colour operator * ( colour c, float s) {
  float _R, _G, _B;

  _R = util::clampVal ( s * c.R, 0, 1);
  _G = util::clampVal ( s * c.G, 0, 1);
  _B = util::clampVal ( s * c.B, 0, 1);

  return colour ( _R, _G, _B);
}

// Inverse scale 1
colour operator / ( colour c, float s) {
  float _R, _G, _B;

  _R = util::clampVal ( c.R / s, 0, 1);
  _G = util::clampVal ( c.G / s, 0, 1);
  _B = util::clampVal ( c.B / s, 0, 1);

  return colour ( _R, _G, _B);
}

// Inverse scale 2
colour operator / ( float s, colour c) {
  float _R, _G, _B;

  _R = util::clampVal ( c.R / s, 0, 1);
  _G = util::clampVal ( c.G / s, 0, 1);
  _B = util::clampVal ( c.B / s, 0, 1);

  return colour ( _R, _G, _B);
}

// Not like
bool operator != ( colour a, colour b) {
  return ( ( a.R != b.R) ||
           ( a.G != b.G) ||
           ( a.B != b.B));
}

// Like
bool operator == ( colour a, colour b) {
  return ( ( a.R == b.R) &&
           ( a.G == b.G) &&
           ( a.B == b.B));
}

// Stream output
// ========================================================
std::ostream &operator << ( std::ostream &io, const colour &c) {
  //io.setf ( std::ios::showpoint + std::ios::right + std::ios::fixed);
  io.setf ( std::ios::showpoint);
  io.setf ( std::ios::right);
  io.setf ( std::ios::fixed);
  io << "( ";
  io << std::setprecision ( 2) << c.R;
  io << " ,";
  io << std::setprecision ( 2) << c.G;
  io << " ,";
  io << std::setprecision ( 2) << c.B;
  io << ")";
  return io;
}
