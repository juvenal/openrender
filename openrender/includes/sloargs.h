//
//  sloargs.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Tue Oct 22 2002
//
//  Original Development:
//    (C) 2002 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id
//

#ifndef SLOARGS_H
#define SLOARGS_H

class sloArgs {
  public:

    // Define the types used
    enum Type {
      // Error signaling
      TYPE_ERROR   = -1,
      TYPE_UNKNOWN =  0,
      // Data types
      TYPE_FLOAT   =  1,
      TYPE_COLOR,
      TYPE_POINT,
      TYPE_VECTOR,
      TYPE_NORMAL,
      TYPE_MATRIX,
      TYPE_STRING,
      // Shader types
      TYPE_SURFACE = 16,
      TYPE_DISPLACEMENT,
      TYPE_LIGHT,
      TYPE_VOLUME,
      TYPE_IMAGER,
      TYPE_GEOMETRY,
      TYPE_TRANSFORMATION
    }
    
    // Define the symbols used
    enum Symbol {
      
    }
}