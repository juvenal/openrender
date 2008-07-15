/*
 *  framebucket.h
 *  openRender
 *
 *  Description:
 *    {Description}
 *
 *  Creation:
 *    Thu Jun 12 2003
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
 *  $Id: framebucket.h,v 1.5 2008/07/15 03:24:58 juvenal.silva Exp $
 */

#ifndef _FRAMEBUCKET_H
#define _FRAMEBUCKET_H

// Define basic frame buckets
class FrameBucket {
    protected:
        Hidder *
    public:
        // Constructor
        FrameBucket();
        // Member functions
        bool    writePixel (int x, int y, float z, Colour c, Opacity o);
        bool    writeColour (int x, int y, Colour c);
        bool    writeOpacity (int x, int y, Opacity o);
        bool    writeDepth (int x, int y, float z);
        Colour  getColour (int x, int y);
        Opacity getOpacity (int x, int y);
        float   getDepth (int x, int y);
        // Complete the bucket process
        bool    processBucket();
        // Destructor
        ~FrameBucket();
};

#endif /* _FRAMEBUCKET_H */
