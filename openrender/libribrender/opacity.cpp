//
//  opacity.cpp - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sun Jun 8 2003
//
//  Original Development:
//    (C) 2006 by Juvenal A. Silva Jr. <juvenal.silva@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: opacity.cpp,v 1.6 2008/07/07 20:17:29 juvenal.silva Exp $
//

// Private includes
#include "opacity.h"

// Constructors
Opacity::Opacity () {
    this->R = 1.0f;
    this->G = 1.0f;
    this->B = 1.0f;
}

Opacity::Opacity (float _r = 1.0f, float _g = 1.0f, float _b = 1.0f) {
    this->R = _r;
    this->G = _g;
    this->B = _b;
}
