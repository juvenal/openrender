//  openRender
//
//  microgrid.cpp - {Summary}
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
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: microgrid.cpp,v 1.3 2003/06/10 02:10:55 juvenal Exp $
//

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "microgrid.h"

// Constructor code
microGrid::microGrid ( _width, _height) {
  this->allocate ( _width, _height);
  this->setTextureCoords ( 0, 0, 1, 1);
}

// Destructor code
microGrid::~microGrid () {
  this->free ();
}

void microGrid::allocate ( int _width, int _height) {
  // Set the values of microgrid
  this->width  = _width;
  this->height = _height;
  // Create 2D array of points
  this->point = new ( point3D*)[ _width + 1];
  point_block = new point3D[ ( _width + 1) * ( _height + 1)];
  if (!point_block) RiError("Cannot reserve memory for point grid %d x %d",width+1,height+1);
  for ( x = 0; x < _width + 1; x++) {
    this->point[x] = &point_block[ x * ( _height + 1)];
  }
  // Create 2D array of colors
  this->colour = new ( colour*)[ _width];
  colour_block = new colour[ _width * _height];
  if (!point_block) RiError("Cannot reserve memory for colour grid %d x %d",width,height);
  for ( x = 0; x < _width; x++) {
    this->colour[x] = &colour_block[ x * _height];
  }
  // Create 2D array of opacities
  this->opacity = new ( opacity*)[ _width];
  opacity_block = new opacity[ _width * _height];
  if (!opacity_block) RiError("Cannot reserve memory for opacity grid %d x %d",width,height);
  for ( x = 0; x < _width; x++) {
    this->opacity[x] = &opacity_block[ x * _height];
  }
  // Create 2D array of normals
  this->normal = new (vector3D*)[ _width];
  normal_block = new vector3D[ ( _width + 1) * ( _height + 1)];
  if (!normal_block) RiError("Cannot reserve memory for normal grid %d x %d",width,height);
  for ( x = 0; x < _width + 1; x++) {
    this->normal[x] = &normal_block[ x * ( _height + 1)];
  }
}

void microGrid::free () {
  if ( point) {
    delete[] point[0];
    delete[] point;
    point = NULL;
  }
  if ( colour) {
    delete[] colour[0];
    delete[] colour;
    colour = NULL;
  }
  if ( opacity) {
    delete[] opacity[0];
    delete[] opacity;
    opacity = NULL;
  }
  if ( normal) {
    delete[] normal[0];
    delete[] normal;
    normal = NULL;
  }
}

bool microGrid::extractMicroPolygon ( microPolygon &m, int _u, int _v) {
  // If not culled
  if ( ( this->normal[_u][_v] * this->point[_u][_v]) < 0) {
    m = microPolygon ( point[_u][_v], point[_u + 1][_v],
                       point[_u + 1][_v + 1], point[_u][_v + 1],
                       colour[_u][_v], opacity[_u][_v]);
    return true;
  }
  else {
    // If culled but double sided
    if ( RiCurrent.geometryAttributes.sides == 2) {
      m = microPolygon ( point[_u][_v], point[_u][_v + 1],
                         point[_u + 1][_v + 1], point[_u + 1][_v],
                         colour[_u][_v], opacity[_u][_v]);
      return true;
    }
  }
  // If culled and not double sided
  return false;
}

int microGrid::width () {
  return width;
}

int microGrid::height () {
  return height;
}

void microGrid::setSize ( int _width, int _height) {
  this->free ();
  this->allocate ( _width, _height);
}

void microGrid::setTextureCoords ( float _umin, float _vmin, float _umax, float _vmax) {
  umin = _umin;
  vmin = _vmin;
  umax = _umax;
  vmax = _vmax;
}

void microGrid::computeNormals () {
}

void microGrid::shade ( list<Light*> &lights) {
}

void microGrid::displace () {
}

void microGrid::statistics ( float &zmin, float &zmax, float &maxusize, float &maxvsize) {
}

// Stream input/output
friend ostream &operator << ( ostream &io, const MicroGrid &m) {
}
