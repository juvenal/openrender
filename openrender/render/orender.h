//  openRender
//
//  orender.h - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Sat Mar 20 2004
//
//  Original Development:
//    (C) 2004 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software, you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: orender.h,v 1.2 2004/05/11 10:48:53 juvenal Exp $
//

#ifndef _ORENDER_H
#define _ORENDER_H

class oRender: public openRenderCLI {
    public:
        oRender();
        int processRequest(int argc, char *argv[]);
    protected:
        static const int APP_MAJOR_NUMBER = 0;
        static const int APP_MIDLE_NUMBER = 1;
        static const int APP_MINOR_NUMBER = 0;
        static const int APP_EXTRA_NUMBER = 0;
        static const String APP_MSG_NAME("orender");
        static const String APP_MSG_DESC("Stand alone renderer for RIB files.");
        static const String APP_MSG_COPY("(c) Copyright 2002 Juvenal A. Silva Jr.\nAll Rights Reserved.");
        static const char OPTION_STATISTICS= 's';
        static const char OPTION_LASTFRAME= 'e';
        static const char OPTION_FIRSTFRAME= 'f';
        static const char OPTION_FRAMEBUFFER= 'd';
        static const char OPTION_VERSION= 'v';
        static const char OPTION_HELP= 'h';
        static const char OPTION_PROGRESS= 'p';
        static const char OPTION_QUALITY = 'q';
        // Internal member functions
        optionsCLI prepareOptions();
        void printHeader();
        void printUsage();
        void printVersion();
}
#endif /* _ORENDER_H */
