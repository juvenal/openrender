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
//  $Id: colour.cpp,v 1.7 2003/05/30 16:46:43 juvenal Exp $
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
  float maxVal, minVal, diff, rD, gD, bD, H, L, S;
  const float minDif = 0.0000001;
  // Main function
  maxVal = util::maxOf ( this->R, this->G, this->B);
  minVal = util::minOf ( this->R, this->G, this->B);
  diff   = maxVal - minVal;
  L = ( maxVal + minVal) / 2;
  if ( fabsf ( diff) < minDif) {
    S = 0;
    H = INFINITY;
  }
  else {
    if ( L <= 0.5) {
      S = diff / ( maxVal + minVal);
    }
    else {
      S  = diff / ( 2 - maxVal - minVal);
      rD = ( maxVal - this->R) / diff;
      gD = ( maxVal - this->G) / diff;
      bD = ( maxVal - this->B) / diff;
      if ( this->R == maxVal) {
        H = bD - gD;
      }
      else if ( this->G == maxVal) {
        H = 2 + rD - bD;
      }
      else if ( this->B == maxVal) {
        H = 4 + gD - rD;
      }
      H = ( ( H * 60) < 0) ? ( H * 60) + 360 : H * 60;
    }
  }
  return vector3D ( H, L, S);
}

vector3D colour::getHSV ( void) {
  float maxVal, minVal, diff, rD, gD, bD, H, S, V;
  // Main function
  maxVal = util::maxOf ( this->R, this->G, this->B);
  minVal = util::minOf ( this->R, this->G, this->B);
  diff   = maxVal - minVal;
  V = maxVal;
  if ( maxVal != 0) {
    S  = diff / maxVal;
    rD = ( maxVal - this->R) / diff;
    gD = ( maxVal - this->G) / diff;
    bD = ( maxVal - this->B) / diff;
    if ( this->R == maxVal) {
      H = bD - gD;
    }
    else if ( this->G == maxVal) {
      H = 2 + rD - bD;
    }
    else if ( this->B == maxVal) {
      H = 4 + gD - rD;
    }
    H = ( ( H * 60) < 0) ? ( H * 60) + 360 : H * 60;
  }
  else {
    S = 0;
    H = INFINITY;
  }
  return vector3D ( H, S, V);
}

vector3D colour::getYIQ ( void) {
  float Y, I, Q;
  // Linear transform
  Y = 0.299 * this->R + 0.587 * this->G + 0.114 * this->B;
  I = 0.596 * this->R - 0.274 * this->G - 0.322 * this->B;
  Q = 0.212 * this->R - 0.523 * this->G - 0.311 * this->B;
  return vector3D ( Y, I, Q);
}

vector3D colour::getYUV ( void) {
  float Y, U, V;
  // Linear transform
  Y =  0.299 * this->R + 0.587 * this->G + 0.114 * this->B;
  U = -0.147 * this->R - 0.289 * this->G + 0.437 * this->B;
  V =  0.615 * this->R - 0.515 * this->G - 0.100 * this->B;
  return vector3D ( Y, U, V);
}

vector3D colour::getCMY ( void) {
  float C, M, Y;
  // Linear transform
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

// Set colour from HLS to RGB
bool colour::setHLS ( vector3D &v) {
  float p1, p2, H, L, S;
  // Get the values
  H = v.getxcomp ();
  L = v.getycomp ();
  S = v.getzcomp ();
  // Main convertion
  if ( S == 0) {
    this->R = this->G = this->B = S;
  }
  else {
    if ( L <= 0.5) {
      p2 = L * ( 1 + S);
    }
    else {
      p2 = L + S - L * S;
    }
    p1 = 2 * L - p2;
    // Final convertion
    this->R = util::clampVal ( this->getChannel ( p1, p2, H + 120), 0, 1);
    this->G = util::clampVal ( this->getChannel ( p1, p2, H), 0, 1);
    this->B = util::clampVal ( this->getChannel ( p1, p2, H - 120), 0, 1);
  }
  return true;
}

// Set colour from HSV to RGB
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
    H /= 60;
    i = ( int) floor ( H);
    f = H - i;
    V = util::clampVal ( V, 0 ,1);
    p = util::clampVal ( V * ( 1 - S), 0, 1);
    q = util::clampVal ( V * ( 1 - ( S * f)), 0, 1);
    t = util::clampVal ( V * ( 1 - ( S * ( 1 - f))), 0, 1);
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

// Set colour from YIQ to RGB
bool colour::setYIQ ( vector3D &v) {
  this->R = util::clampVal ( 1 * v.getxcomp() + 0.956 * v.getycomp() + 0.621 * v.getzcomp(), 0, 1);
  this->G = util::clampVal ( 1 * v.getxcomp() - 0.272 * v.getycomp() - 0.647 * v.getzcomp(), 0, 1);
  this->B = util::clampVal ( 1 * v.getxcomp() - 1.105 * v.getycomp() + 1.702 * v.getzcomp(), 0, 1);
  return true;
}

// Set colour from YUV to RGB
bool colour::setYUV ( vector3D &v) {
  this->R = util::clampVal ( 1 * v.getxcomp() + 0.000 * v.getycomp() + 1.140 * v.getzcomp(), 0, 1);
  this->G = util::clampVal ( 1 * v.getxcomp() - 0.394 * v.getycomp() - 0.581 * v.getzcomp(), 0, 1);
  this->B = util::clampVal ( 1 * v.getxcomp() + 2.028 * v.getycomp() + 0.000 * v.getzcomp(), 0, 1);
  return true;
}

// Set colour from CMY to RGB
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
  _R = util::clampVal ( c.R * s, 0, 1);
  _G = util::clampVal ( c.G * s, 0, 1);
  _B = util::clampVal ( c.B * s, 0, 1);
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
  _R = util::clampVal ( s / c.R, 0, 1);
  _G = util::clampVal ( s / c.G, 0, 1);
  _B = util::clampVal ( s / c.B, 0, 1);
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

// Private helper methods
// ========================================================
float colour::getChannel ( float q1, float q2, float hue) {
  float channel;
  // Fix out of range values of hue
  if ( hue > 360) hue -= 360;
  if ( hue < 0)   hue += 360;
  // When value in range process it
  if ( hue < 60) {
    channel = q1 + ( q2 - q1) * hue / 60;
  }
  else if ( hue < 180) {
    channel = q2;
  }
  else if ( hue < 240) {
    channel = q1 + ( q2 - q1) * ( 240 - hue) / 60;
  }
  else {
    channel = q1;
  }
  return channel;
}
