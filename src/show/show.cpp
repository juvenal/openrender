/**
 * Project: Pixie
 *
 * File: show.cpp
 *
 * Description:
 *   This file implements the functionality for show.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	show.cpp
//  Classes				:	-
//  Description			:	The main show file
//
////////////////////////////////////////////////////////////////////////
#include "common/global.h"
#include "common/os.h"
#include "ri/ri.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////
// Function				:	main
// Description			:	Da main function for show
// Return Value			:	1 on failure, 0 on success
// Comments				:
int main(int argc, char *argv[]) {

    if (argc == 1) {
        fprintf(stdout, "Usage: show <options> <file_name>[,mode]\n");
        fprintf(stdout, "\tKeys:\tbrickmap\tm:more detailed level\n");
        fprintf(stdout, "\t\t\t\tl:less detailed level\n");
        fprintf(stdout, "\t\t\t\tb:show boxes\n");
        fprintf(stdout, "\t\t\t\td:show discs\n");
        fprintf(stdout, "\t\t\t\tp:show points\n");
        fprintf(stdout, "\t\t\t\tq:previous channel\n");
        fprintf(stdout, "\t\t\t\tw:next channel\n");
        fprintf(stdout, "\t\tpoint cloud\tp:show points\n");
        fprintf(stdout, "\t\t\t\td:show discs\n");
        fprintf(stdout, "\t\t\t\tq:previous channel\n");
        fprintf(stdout, "\t\t\t\tw:next channel\n");
        fprintf(stdout, "\t\tirradiance\tp:show points\n");
        fprintf(stdout, "\t\t\t\td:show discs\n\n");
        fprintf(stdout, "\tMouse:\tleft\t\trotate\n");
        fprintf(stdout, "\t\tmiddle\t\tzoom\n");
        fprintf(stdout, "\t\tright\t\tpan\n");
        return 1;
    } else {
        char tmp[OS_MAX_PATH_LENGTH + 6];

        sprintf(tmp, "show:%s", argv[1]);

        // This is how we show things
        RiBegin(RI_NULL);
        RiHider(tmp, RI_NULL);
        RiWorldBegin(); // Force fake rendering (show the window)
        RiWorldEnd();
        RiEnd();

        return 0;
    }
}
