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
//  $Id: matrix4D.h,v 1.4 2004/01/07 11:33:19 juvenal Exp $
//

#ifndef MATRIX4D_H
#define MATRIX4D_H

// Define some usefull constants
#define DEGTORAD 0.017453293

// Define basic type matrix4D
class matrix4D {
    public:
        // Constructors
        matrix4D ();
        // Member functions
        bool     identity ();
        bool     scale (vector3D s);
        bool     translate (vector3D t);
        bool     rotate (vector3D r);
        float*   operator [] (int row) { return element[row];}
        matrix4D inverse ();
        // Arithmetic operations
        // Friend operators and functions
        friend vector3D operator * (matrix4D m, vector3D v);
        friend matrix4D operator * (matrix4D a, matrix4D b);
        // Stream output
        friend std::ostream &operator << (std::ostream &io, matrix4D &m);
    protected:
        // Protected data
        float element[4][4];
};

#endif // MATRIX4D_H
