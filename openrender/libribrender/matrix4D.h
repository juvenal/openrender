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
//  $Id: matrix4D.h,v 1.6 2006/03/26 15:51:23 juvenal.silva Exp $
//

#ifndef MATRIX4D_H
#define MATRIX4D_H

// Define some usefull constants
#define DEGTORAD 0.017453293

// Define basic type matrix4D
class Matrix4D {
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
        // Friend operators and functions
        friend Vector3D operator * (Matrix4D m, Vector3D v);
        friend Matrix4D operator * (Matrix4D a, Matrix4D b);
        // Stream output
        friend std::ostream &operator << (std::ostream &io, Matrix4D &m);
    protected:
        // Protected data
        float element[4][4];
};

#endif // MATRIX4D_H
