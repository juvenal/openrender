/*
 *  vector3D.h
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
 *  $Id: vector3D.h,v 1.8 2008/07/15 03:24:58 juvenal.silva Exp $
 */

#ifndef _VECTOR3D_H
#define _VECTOR3D_H

// C includes
#include <math.h>

// C++ includes
#include <iostream>
#include <iomanip>

// Define basic type vector3D
class Vector3D {
    protected:
        // internal data members
        float x, y, z;
    public:
        // Constructors
        Vector3D(float _x = 0, float _y = 0, float _z = 0);
        // Member functions
        void   setxcomp(float _x);
        void   setycomp(float _y);
        void   setzcomp(float _z);
        float  getxcomp();
        float  getycomp();
        float  getzcomp();
        void   normalise();
        float  length();
        // Aritmetic operations
        Vector3D operator += (Vector3D a);
        Vector3D operator -= (Vector3D a);
        Vector3D operator + (Vector3D a);
        Vector3D operator - (Vector3D a);
        Vector3D operator - ();
        Vector3D operator ^ (Vector3D a);
        float    operator * (Vector3D a);
        Vector3D operator * (float s);
        Vector3D operator / (float s);
        // Logic comparisons
        bool operator != (Vector3D a);
        bool operator == (Vector3D a);
        // Friend stream output
        friend std::ostream& operator << (std::ostream &io, const Vector3D &v);
};

#endif /* _VECTOR3D_H */
