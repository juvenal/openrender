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
//  $Id: utils.cpp,v 1.2 2003/05/30 16:46:43 juvenal Exp $
//

// Private includes
#include "utils.h"

// Define the maxOf entity of util namespace
float util::maxOf ( float v1, float v2, float v3) {
  float max;

  max = ( v1 > v2) ? v1 : v2;
  max = ( v3 > max) ? v3 : max;

  return max;
}

// Define the minOf entity of util namespace
float util::minOf ( float v1, float v2, float v3) {
  float min;

  min = ( v1 < v2) ? v1 : v2;
  min = ( v3 < min) ? v3 : min;

  return min;
}

// Define the clampVal entity of util namespace
float util::clampVal ( float value, float min, float max) {
  float retval;

  if ( value < min) {
    retval = min;
  }
  else if ( value > max) {
    retval = max;
  }
  else {
    retval = value;
  }

  return retval;
}
