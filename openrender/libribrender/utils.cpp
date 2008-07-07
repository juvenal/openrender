//  openRender
//
//  utils.cpp - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Mon Oct 28 2002
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
//  $Id: utils.cpp,v 1.5 2008/07/07 20:17:29 juvenal.silva Exp $
//

// Private includes
#include "utils.h"

// Define the max of two values
float util::max(float v1, float v2) {
    return (v1 > v2) ? v1 : v2;
}

// Define the min of two values
float util::min(float v1, float v2) {
    return (v1 < v2) ? v1 : v2;
}

// Define the maxOf tree values
float util::maxOf(float v1, float v2, float v3) {
    float max;
    max = (v1 > v2) ? v1 : v2;
    max = (v3 > max) ? v3 : max;
    return max;
}

// Define the minOf tree values
float util::minOf(float v1, float v2, float v3) {
    float min;
    min = (v1 < v2) ? v1 : v2;
    min = (v3 < min) ? v3 : min;
    return min;
}

// Define the clampVal for a given value within a min, max range
float util::clampVal(float value, float min, float max) {
    float retval;
    if (value < min) {
        retval = min;
    }
    else if (value > max) {
        retval = max;
    }
    else {
        retval = value;
    }
    return retval;
}
