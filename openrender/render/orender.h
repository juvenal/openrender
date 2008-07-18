/*
 *  orender.h
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
 *  $Id: orender.h,v 1.8 2008/07/18 17:17:50 juvenal.silva Exp $
 */

#ifndef _ORENDER_H
#define _ORENDER_H

// Public project include
#include <openrender.h>

// Private project include
#include "optionsCLI.h"

class oRender: public openRender {
    protected:
        static const int APP_MAJOR_VERSION = 0;
        static const int APP_MIDLE_VERSION = 1;
        static const int APP_MINOR_VERSION = 0;
        static const int APP_EXTRA_RELEASE = 0;
        static const char *APP_MSG_NAME;
        static const char *APP_MSG_DESC;
        static const char *APP_MSG_COPY;
        static const char OPTION_STATISTICS = 's';
        static const char OPTION_FIRSTFRAME = 'b';
        static const char OPTION_LASTFRAME = 'e';
        static const char OPTION_FRAMEBUFFER = 'd';
        static const char OPTION_VERSION = 'v';
        static const char OPTION_HELP = 'h';
        static const char OPTION_PROGRESS = 'p';
        static const char OPTION_QUALITY = 'q';
        optionsCLI activeOptions;
        // Internal member functions
        void prepareOptions(int argc, char *argv[]);
        void printHeader();
        void printUsage();
        void printVersion();
    public:
        oRender();
        int processRequest(int argc, char *argv[]);
        ~oRender();
};

// Fill the static constants
const char *oRender::APP_MSG_NAME = "orender";
const char *oRender::APP_MSG_DESC = "Stand alone renderer for RIB files.";
const char *oRender::APP_MSG_COPY = "(c) Copyright 2008 Juvenal A. Silva Jr.\nAll Rights Reserved.";

#endif /* _ORENDER_H */
