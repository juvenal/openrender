//  openRender
//
//  colour.h - {Summary}
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
//  $Id: colour.h,v 1.3 2002/10/21 02:16:36 juvenal Exp $
//

#ifndef COLOUR_H
#define COLOUR_H

// Define basic type colour
class colour {
  public:
    // Constructors
    colour ( void);
    colour ( float c1, float c2, float c3);
    // Member functions
    // Get colour values
    vector3D getRGB ( void);
    vector3D getHLS ( void);
    vector3D getHSV ( void);
    vector3D getYIQ ( void);
    vector3D getYUV ( void);
    vector3D getCMY ( void);
    // Set colour values
    bool setRGB ( vector3D &v);
    bool setHLS ( vector3D &v);
    bool setHSV ( vector3D &v);
    bool setYIQ ( vector3D &v);
    bool setYUV ( vector3D &v);
    bool setCMY ( vector3D &v);
    // Arithmetic operations
    colour operator += ( colour c);
    colour operator -= ( colour c);
    // Friend operators and functions
    friend colour operator +  ( colour a, colour b);
    friend colour operator -  ( colour a, colour b);
    friend colour operator *  ( colour a, colour b);
    friend colour operator *  ( float s, colour c);
    friend colour operator *  ( colour c, float s);
    friend colour operator /  ( colour c, float s);
    friend bool   operator == ( colour a, colour b);
    friend bool   operator != ( colour a, colour b);
    // Stream output
    friend std::ostream &operator << ( std::ostream &io, const colour &c);
  private:
    // Private data
    float R, G, B;
};

#endif // COLOUR_H
