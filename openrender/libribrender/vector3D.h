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
//  $Id: vector3D.h,v 1.3 2003/05/30 16:46:43 juvenal Exp $
//

#ifndef VECTOR3D_H
#define VECTOR3D_H

// Define basic type vector3D
class vector3D {
  public:
    // Constructors
    vector3D ( float _x = 0,
               float _y = 0,
               float _z = 0);
    // Member functions
    bool     setxcomp ( float _x);
    bool     setycomp ( float _y);
    bool     setzcomp ( float _z);
    float    getxcomp ();
    float    getycomp ();
    float    getzcomp ();
    bool     normalise ();
    float    length ();
    // Aritmetic operations
    vector3D operator += ( vector3D a);
    vector3D operator -= ( vector3D a);
    // Friend operators and functions
    friend vector3D operator + ( vector3D a, vector3D b);
    friend vector3D operator - ( vector3D a, vector3D b);
    friend vector3D operator - ( vector3D v);
    friend vector3D operator ^ ( vector3D a, vector3D b);
    friend float    operator * ( vector3D a, vector3D b);
    friend vector3D operator * ( vector3D v, float s);
    friend vector3D operator * ( float s, vector3D v);
    friend vector3D operator / ( vector3D v, float s);
    friend vector3D operator / ( float s, vector3D v);
    friend bool     operator != ( vector3D a, vector3D b);
    friend bool     operator == ( vector3D a, vector3D b);
    // Stream output
    friend std::ostream& operator << ( std::ostream& io, const vector3D& v);
  protected:
    // Protected data
    float x, y, z;
};

#endif // VECTOR3D_H
