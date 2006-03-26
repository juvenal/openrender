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
//  $Id: opacity.cpp,v 1.5 2006/03/26 15:51:23 juvenal.silva Exp $
//

// Private includes
#include "opacity.h"

// Constructors
Opacity::Opacity () {
    this->R = 1;
    this->G = 1;
    this->B = 1;
}

Opacity::Opacity (float _r = 1, float _g = 1, float _b = 1) {
    this->R = _r;
    this->G = _g;
    this->B = _b;
}
