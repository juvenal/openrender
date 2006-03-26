//
//  micropolygon.cpp - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sun Jun 8 2003
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
//  $Id: micropolygon.cpp,v 1.6 2006/03/26 15:51:23 juvenal.silva Exp $
//

// C++ includes
#include <iostream>
#include <iomanip>

// Private includes
#include "micropolygon.h"

// Constructor code
MicroPolygon::MicroPolygon () {
    this->point[0] = Point3D (0, 0, 0);
    this->point[1] = Point3D (0, 0, 0);
    this->point[2] = Point3D (0, 0, 0);
    this->point[3] = Point3D (0, 0, 0);
    this->colour   = Colour (0, 0, 0);
    this->opacity  = Opacity (1, 1, 1);
}

MicroPolygon::MicroPolygon (Point3D &p1, Point3D &p2, Point3D &p3, Point3D &p4
                            Colour &colour, Opacity &opacity) {
    this->point[0] = p1;
    this->point[1] = p2;
    this->point[2] = p3;
    this->point[3] = p4;
    this->colour   = colour;
    this->opacity  = opacity;
}

void MicroPolygon::transformToScreenSpace (Matrix4D &transform) {
    int index;

    for (index = 0; index < 4; index++) {
        if (this->point[index].z < 0.1) {
            printf ("Error Z value %f <= 0.1\n", this->point[index].z);
            exit (EXIT_FAILURE);
        }
        this->point[index].x = this->point[index].x / this->point[index].z;
        this->point[index].y = this->point[index].y / this->point[index].z;
        this->point[index] = transform * this->point[index];
    }
}

bool MicroPolygon::sample (float sx, float sy, float &z, Colour &colour, Opacity &opacity) {
    // Local variables
    float ax, ay, bx, by, r;
    int   index, inside = 0;

    for (index = 0; index < 4; index++) {
        // Calc edge vector
        ax = this->point[( index + 1) % 4].x - this->point[index].x;
        ay = this->point[( index + 1) % 4].y - this->point[index].y;
        // Calc vector from vertex to sample point
        bx = sx - this->point[index].x;
        by = sy - this->point[index].y;
        // Calc. side (a to left of b if +v.e and vice versa)
        r = ax * by - ay * bx;
        inside += ( r >= 0) ? 1 : 0;
    }
    if (inside == 4) {
        colour  = this->colour;
        opacity = this->opacity;
        z = this->point[0].z;
        return true;
    }
    else {
        return false;
    }
}

void MicroPolygon::rasterize (FrameBuffer &fb) {

}


// Destructor code
MicroPolygon::~MicroPolygon () {
}
