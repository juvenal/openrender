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
//  $Id: bound3D.h,v 1.1 2004/01/07 11:33:19 juvenal Exp $
//

#ifndef BOUND3D_H
#define BOUND3D_H

class bound3D {
    public:
        // Constructors
        bound3D (point3D _pMin, point3D _pMax);
        bound3D (float _xMin, float _xMax, float _yMin, float _yMax, float _zMin, float _zMax);
        // Member functions
        float getMixZ ();
        bool intersects (bound3D other);
        bool contains (point3D point);
        bound3D getBoundingBox ();
        void sortMinMax ();
        friend bound3D operator * (matrix4D transform, bound3D bound);
    protected:
        point3D pMin;
        point3D pMax;
};

#endif // BOUND3D_H
