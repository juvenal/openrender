//  openRender
//
//  vector3D.cpp - {Summary}
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
//  $Id: vector3D.cpp,v 1.2 2002/10/21 00:27:51 juvenal Exp $
//

// C includes
#include <math.h>

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "vector3D.h"

// Constructors
// ========================================================
vector3D::vector3D ( float x, float y, float z) {
  this->x = x;
  this->y = y;
  this->z = z;
}

// Member Functions
// ========================================================
bool vector3D::setxcomp ( float _x) {
  this->x = _x;
  return true;
}

bool vector3D::setycomp ( float _y) {
  this->y = _y;
  return true;
}

bool vector3D::setzcomp ( float _z) {
  this->z = _z;
  return true;
}

float vector3D::getxcomp () {
  return this->x;
}

float vector3D::getycomp () {
  return this->y;
}

float vector3D::getzcomp () {
  return this->z;
}

bool vector3D::normalise () {
  float d;

  d = length ();
  if ( d > 0) {
    this->x = this->x / d;
    this->y = this->y / d;
    this->z = this->z / d;
  }
  return true;
}

float vector3D::length () {
  return sqrt ( this->x*this->x + this->y*this->y + this->z*this->z);
}

// Arithmetic
// ========================================================
// Addition
vector3D operator + ( vector3D a, vector3D b) {
  return vector3D ( a.x + b.x, a.y + b.y, a.z + b.z);
}

// Subtraction
vector3D operator - ( vector3D a, vector3D b) {
  return vector3D ( a.x - b.x, a.y - b.y, a.z - b.z);
}

// Inversion
vector3D operator - ( vector3D v) {
  return vector3D ( -v.x, -v.y, -v.z);
}

// Cross-product
vector3D operator ^ ( vector3D a, vector3D b) {
  return vector3D ( a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x);
}

// Dot-product
float operator * ( vector3D a, vector3D b) {
  return ( a.x * b.x + a.y * b.y + a.z * b.z);
}

// Scale 1
vector3D operator * ( float s, vector3D v) {
  return vector3D ( s * v.x, s * v.y, s * v.z);
}

// Scale 2
vector3D operator * ( vector3D v, float s) {
  return vector3D ( s * v.x, s * v.y, s * v.z);
}

// Inverse Scale
vector3D operator / ( vector3D v, float s) {
  return vector3D ( v.x / s, v.y / s, v.z / s);
}

// Common Addition
vector3D vector3D::operator += ( vector3D a) {
  return vector3D ( this->x + a.x, this->y + a.y, this->z + a.z);
}

// Common Subtraction
vector3D vector3D::operator -= ( vector3D a) {
  return vector3D ( this->x - a.x, this->y - a.y, this->z - a.z);
}

// Not like
bool operator != ( vector3D a, vector3D b) {
  return ( ( a.x != b.x) ||
           ( a.y != b.y) ||
           ( a.z != b.z));
}

// Like
bool operator == ( vector3D a, vector3D b) {
  return ( ( a.x == b.x) &&
           ( a.y == b.y) &&
           ( a.z == b.z));
}

// Stream output
// ========================================================
std::ostream &operator << ( std::ostream &io, const vector3D &v) {
  io.setf ( std::ios::showpoint);
  io.setf ( std::ios::right);
  io.setf ( std::ios::fixed);
  io << "( ";
  io << std::setprecision ( 2) << v.x;
  io << " ,";
  io << std::setprecision ( 2) << v.y;
  io << " ,";
  io << std::setprecision ( 2) << v.z;
  io << ")";
  return io;
}
