/*
 *  openrender.h
 *  openRender
 *
 *  Description:
 *    {Description}
 *
 *  Creation:
 *    Sat Mar 20 2004
 *
 *  Original Development:
 *    (C) 2004 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
 *
 *  Contributions:
 *
 *  Statement:
 *    This program is free software, you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 2 of the License, or
 *    (at your option) any later version.
 *
 *  $Id: openrender.h,v 1.5 2008/07/17 20:08:11 juvenal.silva Exp $
 */

#ifndef _OPENRENDER_H
#define _OPENRENDER_H

#include <ri.h>

class openRender {
    public:
        static const int OR_MAJOR_VERSION = 1;
        static const int OR_MIDLE_VERSION = 0;
        static const int OR_MINOR_VERSION = 0;
        static const int OR_EXTRA_RELEASE = 1;
        static const char *OR_MSG_NAME;
        static const char *OR_MSG_ARCH;
        static const char *OR_MSG_DESC;
        static const char *OR_MSG_RMAN;
};

// openRender static constants
const char *openRender::OR_MSG_NAME = "openRender";
const char *openRender::OR_MSG_ARCH = "linux";
const char *openRender::OR_MSG_DESC = "(Adheres to the RenderMan Standard)";
const char *openRender::OR_MSG_RMAN = "The RenderMan (R) Interface Procedures and RIB Protocol are:\nCopyright 1988,1989, Pixar. All Rights Reserved.\nRenderMan (R) is a registered trademark of Pixar.\n";

/*
 openRender 1.0 Release 1 linux (Adheres to the RenderMan Standard)
 (c) Copyright 2002 Juvenal A. Silva Jr.  Portions (c) Ian Stephenson.
 All Rights Reserved.
 
 The RenderMan (R) Interface Procedures and RIB Protocol are:
 Copyright 1988,1989, Pixar. All Rights Reserved.
 RenderMan (R) is a registered trademark of Pixar.
 */

#endif /* _OPENRENDER_H */
