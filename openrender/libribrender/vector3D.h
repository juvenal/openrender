//  openRender
//
//  vector3D.h - {Summary}
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
//  $Id: vector3D.h,v 1.5 2004/07/14 18:55:46 juvenal Exp $
//

#ifndef VECTOR3D_H
#define VECTOR3D_H

// Define basic type vector3D
class Vector3D {
    public:
        // Constructors
        Vector3D (float _x = 0,
                  float _y = 0,
                  float _z = 0);
        // Member functions
        bool     setxcomp (float _x);
        bool     setycomp (float _y);
        bool     setzcomp (float _z);
        float    getxcomp ();
        float    getycomp ();
        float    getzcomp ();
        bool     normalise ();
        float    length ();
        // Aritmetic operations
        Vector3D operator += (Vector3D a);
        Vector3D operator -= (Vector3D a);
        // Friend operators and functions
        friend Vector3D operator + (Vector3D a, Vector3D b);
        friend Vector3D operator - (Vector3D a, Vector3D b);
        friend Vector3D operator - (Vector3D v);
        friend Vector3D operator ^ (Vector3D a, Vector3D b);
        friend float    operator * (Vector3D a, Vector3D b);
        friend Vector3D operator * (Vector3D v, float s);
        friend Vector3D operator * (float s, Vector3D v);
        friend Vector3D operator / (Vector3D v, float s);
        friend Vector3D operator / (float s, Vector3D v);
        friend bool     operator != (Vector3D a, Vector3D b);
        friend bool     operator == (Vector3D a, Vector3D b);
        // Stream output
        friend std::ostream& operator << (std::ostream &io, const Vector3D &v);
    protected:
        // Protected data
        float x, y, z;
};

#endif // VECTOR3D_H
