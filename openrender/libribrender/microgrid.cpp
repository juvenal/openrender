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
//    (C) 2006 by Juvenal A. Silva Jr. <juvenal.silva@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: microgrid.cpp,v 1.6 2006/03/26 15:51:23 juvenal.silva Exp $
//

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "microgrid.h"

// Constructor code
MicroGrid::MicroGrid (_width, _height) {
    this->allocate(_width, _height);
    this->setTextureCoords(0, 0, 1, 1);
}

// Destructor code
MicroGrid::~MicroGrid () {
    this->free();
}

void MicroGrid::allocate (int _width, int _height) {
    // Set the values of microgrid
    this->width  = _width;
    this->height = _height;
    // Create 2D array of points
    this->point = new (Point3D*)[_width + 1];
    point_block = new Point3D[(_width + 1) * (_height + 1)];
    if (!point_block) {
        RiError("Cannot reserve memory for point grid %d x %d", width + 1, height + 1);
    }
    for (x = 0; x < _width + 1; x++) {
        this->point[x] = &point_block[x * (_height + 1)];
    }
    // Create 2D array of colors
    this->colour = new (Colour*)[_width];
    colour_block = new Colour[_width * _height];
    if (!point_block) {
        RiError("Cannot reserve memory for colour grid %d x %d", width, height);
    }
    for (x = 0; x < _width; x++) {
        this->colour[x] = &colour_block[x * _height];
    }
    // Create 2D array of opacities
    this->opacity = new (Opacity*)[_width];
    opacity_block = new Opacity[_width * _height];
    if (!opacity_block) {
        RiError("Cannot reserve memory for opacity grid %d x %d", width, height);
    }
    for (x = 0; x < _width; x++) {
        this->opacity[x] = &opacity_block[x * _height];
    }
    // Create 2D array of normals
    this->normal = new (Vector3D*)[_width];
    normal_block = new Vector3D[(_width + 1) * (_height + 1)];
    if (!normal_block) {
        RiError("Cannot reserve memory for normal grid %d x %d", width, height);
    }
    for (x = 0; x < _width + 1; x++) {
        this->normal[x] = &normal_block[x * (_height + 1)];
    }
}

void MicroGrid::free () {
    if (point) {
        delete[] point[0];
        delete[] point;
        point = NULL;
    }
    if (colour) {
        delete[] colour[0];
        delete[] colour;
        colour = NULL;
    }
    if (opacity) {
        delete[] opacity[0];
        delete[] opacity;
        opacity = NULL;
    }
    if (normal) {
        delete[] normal[0];
        delete[] normal;
        normal = NULL;
    }
}

bool MicroGrid::extractMicroPolygon (MicroPolygon &m, int _u, int _v) {
    // If not culled
    if ((this->normal[_u][_v] * this->point[_u][_v]) < 0) {
        m = MicroPolygon(point[_u][_v], point[_u + 1][_v],
                         point[_u + 1][_v + 1], point[_u][_v + 1],
                         colour[_u][_v], opacity[_u][_v]);
        return true;
    }
    else {
        // If culled but double sided
        if (RiCurrent.geometryAttributes.sides == 2) {
            m = MicroPolygon(point[_u][_v], point[_u][_v + 1],
                             point[_u + 1][_v + 1], point[_u + 1][_v],
                             colour[_u][_v], opacity[_u][_v]);
            return true;
        }
    }
    // If culled and not double sided
    return false;
}

int MicroGrid::width () {
    return width;
}

int MicroGrid::height () {
    return height;
}

void MicroGrid::setSize (int _width, int _height) {
    this->free();
    this->allocate(_width, _height);
}

void MicroGrid::setTextureCoords (float _umin, float _vmin, float _umax, float _vmax) {
    umin = _umin;
    vmin = _vmin;
    umax = _umax;
    vmax = _vmax;
}

void MicroGrid::computeNormals () {
}

void MicroGrid::shade (list<Light*> &lights) {
}

void MicroGrid::displace () {
}

void MicroGrid::statistics (float &zmin, float &zmax, float &maxusize, float &maxvsize) {
}

// Stream input/output
friend ostream &operator << (ostream &io, const MicroGrid &m) {
}
