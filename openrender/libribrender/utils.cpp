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
//  $Id: utils.cpp,v 1.1 2002/10/28 15:33:20 juvenal Exp $
//

// Private includes
#include "utils.h"

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
