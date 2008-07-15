/*
 *  opacity.h
 *  openRender
 *
 *  Description:
 *    {Description}
 *
 *  Creation:
 *    Sun Jun 8 2003
 *
 *  Original Development:
 *    (C) 2006 by Juvenal A. Silva Jr. <juvenal.silva@v2-home.com.br>
 *
 *  Contributions:
 *
 *  Statement:
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *  $Id: opacity.h,v 1.7 2008/07/15 03:24:58 juvenal.silva Exp $
 */

#ifndef _OPACITY_H
#define _OPACITY_H

// Superclass include
#include "colour.h"

// Define basic type opacity
class Opacity : public Colour {
    public:
        // Constructors
        Opacity();
        Opacity(float _r, float _g, float _b);
        // All the other methods inherited from parent superclass
};

#endif /* _OPACITY_H */
