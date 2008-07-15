/*
 *  micropolygon.h - {Summary}
 *
 *  Description:
 *    {Description}
 *
 *  Creation:
 *    Sun Jun 8 2003
 *
 *  Original Development:
 *    (C) 2006 by Juvenal A. Silva Jr. <juvenal.silva@v2-home.com.br>
 *
 *  Contributions:
 *
 *  Statement:
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *  $Id: micropolygon.h,v 1.8 2008/07/15 03:24:58 juvenal.silva Exp $
 */

#ifndef _MICROPOLYGON_H
#define _MICROPOLYGON_H

// Define micropolygon class
class MicroPolygon {
    protected:
        Point3D  point[4];    // screen-space with Z in eye-space
        Colour   colour;      // Micropolygon color
        Opacity  opacity;     // Micropolygon opacity
    public:
        // Constructors & Destructor
        MicroPolygon();
        MicroPolygon(Point3D &p1, Point3D &p2, Point3D &p3, Point3D &p4,
                     Colour &colour, Opacity &opacity);
        ~MicroPolygon();
        // Member functions
        void transformToScreenSpace(Matrix4D &transform);
        bool sample(float sx, float sy, float &z, Colour &colour, Opacity &opacity);
        void rasterize(FrameBuffer &fb);
        // Friend functions
        friend ostream &operator << (ostream &io,const MicroPolygon &m);
};

#endif /* _MICROPOLYGON_H */
