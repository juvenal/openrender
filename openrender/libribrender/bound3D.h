//  openRender
//
//  bound3D.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Mon Dec 29 2003
//
//  Original Development:
//    (C) 2003 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software, you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: bound3D.h,v 1.2 2004/07/14 18:55:46 juvenal Exp $
//

#ifndef BOUND3D_H
#define BOUND3D_H

class Bound3D {
    public:
        // Constructors
        Bound3D (point3D _pMin, point3D _pMax);
        Bound3D (float _xMin, float _xMax, float _yMin, float _yMax, float _zMin, float _zMax);
        // Member functions
        float getMixZ ();
        bool intersects (Bound3D other);
        bool contains (Point3D point);
        Bound3D getBoundingBox ();
        void sortMinMax ();
        friend Bound3D operator * (Matrix4D transform, Bound3D bound);
    protected:
        Point3D pMin;
        Point3D pMax;
};

#endif // BOUND3D_H
