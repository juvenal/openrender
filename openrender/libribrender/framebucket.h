//
//  framebucket.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Thu Jun 12 2003
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
//  $Id: framebucket.h,v 1.1 2003/12/06 00:45:00 juvenal Exp $
//

#ifndef FRAMEBUCKET_H
#define FRAMEBUCKET_H

// Define basic frame buckets
class frameBucket {
  public: 
    // Constructor
    frameBucket();
    // Member functions
    bool    writePixel ( int x, int y, float z, colour c, opacity o);
    bool    writeColour ( int x, int y, colour c);
    bool    writeOpacity ( int x, int y, opacity o);
    bool    writeDepth ( int x, int y, float z);
    colour  getColour ( int x, int y);
    opacity getOpacity ( int x, int y);
    float   getDepth ( int x, int y);
    // Complete the bucket process
    bool    processBucket ();
    // Destructor
    ~frameBucket();
  protected:
    hidder *
};

#endif // FRAMEBUCKET_H
