/*
 *  colour.h
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
 *  $Id: colour.h,v 1.11 2008/07/15 03:24:58 juvenal.silva Exp $
 */

#ifndef _COLOUR_H
#define _COLOUR_H

// Private includes
#include "utils.h"
#include "vector3D.h"
#include "matrix4D.h"

// Define basic type colour
class Colour {
    protected:
        // Protected data
        float R, G, B;
        // Protected member functions
        float getChannel(float q1, float q2, float hue);
    public:
        // Constructors & Destructor
        Colour(void);
        Colour(float c1, float c2, float c3);
        Colour(float c);
        Colour(const Vector3D &From);
        Colour(const float From[3]);
        ~Colour();
        // Member functions
        // Get colour values
        Vector3D getRGB(void);
        Vector3D getHLS(void);
        Vector3D getHSV(void);
        Vector3D getYIQ(void);
        Vector3D getYUV(void);
        Vector3D getCMY(void);
        // Set colour values
        void setRGB(Vector3D &v);
        void setHLS(Vector3D &v);
        void setHSV(Vector3D &v);
        void setYIQ(Vector3D &v);
        void setYUV(Vector3D &v);
        void setCMY(Vector3D &v);
        // Arithmetic operations
        Colour& operator += (Colour c);
        Colour& operator -= (Colour c);
        Colour& operator *= (Colour c);
        Colour& operator /= (Colour c);
        Colour  operator +  (Colour a);
        Colour  operator -  (Colour a);
        Colour  operator *  (Colour a);
        Colour  operator *  (float s);
        Colour  operator /  (Colour a);
        Colour  operator /  (float s);
        bool    operator == (Colour a);
        bool    operator != (Colour a);
        // Friend stream output
        friend  std::ostream &operator << (std::ostream &io, const Colour &c);
};

#endif /* _COLOUR_H */
