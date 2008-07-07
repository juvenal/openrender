//  openRender
//
//  matrix4D.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sat Sep 28 2002
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
//  $Id: matrix4D.h,v 1.7 2008/07/07 20:17:29 juvenal.silva Exp $
//

#ifndef MATRIX4D_H
#define MATRIX4D_H

// Define some usefull constants
#define DEGTORAD 0.017453293

// Define basic type matrix4D
class Matrix4D {
    protected:
        // Protected data members
        float element[4][4];
    public:
        // Constructors
        Matrix4D();
        // Member functions
        bool     identity();
        bool     scale(Vector3D s);
        bool     translate(Vector3D t);
        bool     rotate(Vector3D r);
        float*   operator[] (int row) { return element[row];}
        Matrix4D inverse();
        // Arithmetic operations
        Vector3D operator * (Vector3D v);
        Matrix4D operator * (Matrix4D a);
        // Friend stream output
        friend std::ostream &operator << (std::ostream &io, Matrix4D &m);
};

#endif // MATRIX4D_H
