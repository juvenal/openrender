/*
 *  vector3D.cpp
 *  openRender
 *
 *  Description:
 *    {Description}
 *
 *  Creation:
 *    Sat Sep 28 2002
 *
 *  Original Development:
 *    (C) 2006 by Juvenal A. Silva Jr. <juvenal.silva@v2-home.com.br>
 *
 *  Contributions:
 *
 *  Statement:
 *    This program is free software, you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 2 of the License, or
 *    (at your option) any later version.
 *
 *  $Id: vector3D.cpp,v 1.9 2008/07/15 03:24:58 juvenal.silva Exp $
 */

// C includes
#include <math.h>

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "vector3D.h"

// vector3D implementation
// *******************************
// Constructors
// ========================================================
Vector3D::Vector3D(float _x, float _y, float _z) {
    this->x = _x;
    this->y = _y;
    this->z = _z;
}

// Member Functions
// ========================================================
void Vector3D::setxcomp(float _x) {
    this->x = _x;
}

void Vector3D::setycomp(float _y) {
    this->y = _y;
}

void Vector3D::setzcomp(float _z) {
    this->z = _z;
}

float Vector3D::getxcomp() {
    return this->x;
}

float Vector3D::getycomp() {
    return this->y;
}

float Vector3D::getzcomp() {
    return this->z;
}

void Vector3D::normalise() {
    float d;
    d = this->length();
    if (d > 0) {
        this->x = this->x / d;
        this->y = this->y / d;
        this->z = this->z / d;
    }
}

float Vector3D::length() {
    return sqrt (this->x * this->x + this->y * this->y + this->z * this->z);
}

// Arithmetic
// ========================================================
// Common Addition
Vector3D Vector3D::operator += (Vector3D a) {
    this->x += a.x;
    this->y += a.y;
    this->z += a.z;
    return *this;
    // return Vector3D (this->x + a.x, this->y + a.y, this->z + a.z);
}

// Common Subtraction
Vector3D Vector3D::operator -= (Vector3D a) {
    this->x -= a.x;
    this->y -= a.y;
    this->z -= a.z;
    return *this;
    // return Vector3D (this->x - a.x, this->y - a.y, this->z - a.z);
}

// Addition
Vector3D Vector3D::operator + (Vector3D a) {
    return Vector3D (this->x + a.x, this->y + a.y, this->z + a.z);
}

// Subtraction
Vector3D Vector3D::operator - (Vector3D a) {
    return Vector3D (this->x - a.x, this->y - a.y, this->z - a.z);
}

// Inversion
Vector3D Vector3D::operator - () {
    return Vector3D (-this->x, -this->y, -this->z);
}

// Cross-product
Vector3D Vector3D::operator ^ (Vector3D a) {
    return Vector3D (this->y * a.z - this->z * a.y,
                     this->z * a.x - this->x * a.z,
                     this->x * a.y - this->y * a.x);
}

// Dot-product
float Vector3D::operator * (Vector3D a) {
    return (this->x * a.x + this->y * a.y + this->z * a.z);
}

// Scale
Vector3D Vector3D::operator * (float s) {
    return Vector3D (s * this->x, s * this->y, s * this->z);
}

// Inverse Scale
Vector3D Vector3D::operator / (float s) {
    return Vector3D (this->x / s, this->y / s, this->z / s);
}

// Not like
bool Vector3D::operator != (Vector3D a) {
    return ((this->x != a.x) ||
            (this->y != a.y) ||
            (this->z != a.z));
}

// Like
bool Vector3D::operator == (Vector3D a) {
    return ((this->x == a.x) &&
            (this->y == a.y) &&
            (this->z == a.z));
}

// Friend stream output
// ========================================================
std::ostream &operator << (std::ostream &io, const Vector3D &v) {
    io.setf (std::ios::showpoint);
    io.setf (std::ios::right);
    io.setf (std::ios::fixed);
    io << "( ";
    io << std::setprecision (2) << v.x;
    io << " ,";
    io << std::setprecision (2) << v.y;
    io << " ,";
    io << std::setprecision (2) << v.z;
    io << ")";
    return io;
}
