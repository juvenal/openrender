//
//  micropolygon.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sun Jun 8 2003
//
//  Original Development:
//    (C) 2003 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: micropolygon.h,v 1.3 2003/12/06 00:45:01 juvenal Exp $
//

#ifndef MICROPOLYGON_H
#define MICROPOLYGON_H

// Define micropolygon class
class microPolygon {
  public:
    // Constructors
    microPolygon ();
    microPolygon ( point3D &p1, point3D &p2, point3D &p3, point3D &p4,
                   colour &colour, opacity &opacity);
    // Member functions
    void transformToScreenSpace ( matrix4D &transform);
    bool sample ( float sx, float sy, float &z, colour &colour, opacity &opacity);
    void rasterize ( frameBuffer &fb);
    // Friend functions
    friend ostream &operator<<(ostream &io,const MicroPolygon &m);
  protected:
    point3D  point[4];    // screen-space with Z in eye-space
    colour   colour;      // Micropolygon color
    opacity  opacity;     // Micropolygon opacity
};

#endif // MICROPOLYGON_H
