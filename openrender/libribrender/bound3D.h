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
//    (C) 2006 by Juvenal A. Silva Jr. <juvenal.silva@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software, you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: bound3D.h,v 1.4 2008/07/07 20:17:29 juvenal.silva Exp $
//

#ifndef BOUND3D_H
#define BOUND3D_H

class Bound3D {
    protected:
        Point3D pMin;
        Point3D pMax;
    public:
        // Constructors
        Bound3D(Point3D _pMin, Point3D _pMax);
        Bound3D(float _xMin, float _xMax, float _yMin, float _yMax, float _zMin, float _zMax);
        // Member functions
        float   getMixZ();
        bool    intersects(Bound3D other);
        bool    contains(Point3D point);
        Bound3D getBoundingBox();
        void    sortMinMax();
        friend Bound3D operator * (Matrix4D transform, Bound3D bound);
};

#endif // BOUND3D_H
