//
//  opacity.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sun Jun 8 2003
//
//  Original Development:
//    (C) 2003 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: opacity.h,v 1.3 2004/01/07 11:33:19 juvenal Exp $
//

#ifndef OPACITY_H
#define OPACITY_H

// Superclass include
#include "colour.h"

// Define basic type opacity
class opacity : public colour {
    public:
        // Constructors
        opacity ();
        opacity ( float _r = 1, float _g = 1, float _b = 1);
        // All the other methods inherited from parent superclass
};

#endif // OPACITY_H
