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
//  $Id: colour.h,v 1.8 2006/03/26 15:51:23 juvenal.silva Exp $
//

#ifndef COLOUR_H
#define COLOUR_H

// Define basic type colour
class Colour {
    public:
        // Constructors
        Colour (void);
        Colour (float c1, float c2, float c3);
        // Member functions
        // Get colour values
        Vector3D getRGB (void);
        Vector3D getHLS (void);
        Vector3D getHSV (void);
        Vector3D getYIQ (void);
        Vector3D getYUV (void);
        Vector3D getCMY (void);
        // Set colour values
        bool setRGB (Vector3D &v);
        bool setHLS (Vector3D &v);
        bool setHSV (Vector3D &v);
        bool setYIQ (Vector3D &v);
        bool setYUV (Vector3D &v);
        bool setCMY (Vector3D &v);
        // Arithmetic operations
        Colour operator += (Colour c);
        Colour operator -= (Colour c);
        Colour operator *= (Colour c);
        Colour operator /= (Colour c);
        // Friend operators and functions
        friend Colour operator +  (Colour a, Colour b);
        friend Colour operator -  (Colour a, Colour b);
        friend Colour operator *  (Colour a, Colour b);
        friend Colour operator *  (float s, Colour c);
        friend Colour operator *  (Colour c, float s);
        friend Colour operator /  (Colour a, Colour b);
        friend Colour operator /  (Colour c, float s);
        friend Colour operator /  (float s, Colour c);
        friend bool   operator == (Colour a, Colour b);
        friend bool   operator != (Colour a, Colour b);
        // Stream output
        friend std::ostream &operator << (std::ostream &io, const Colour &c);
    protected:
        // Protected member functions
        float getChannel (float q1, float q2, float hue);
        // Protected data
        float R, G, B;
};

#endif // COLOUR H
