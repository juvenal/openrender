/**
 * Project: Pixie
 *
 * File: fileResource.h
 *
 * Description:
 *   This file defines the interface for fileResource.
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
//  File				:	fileResource.h
//  Classes				:	CFileResource
//  Description			:	Any class that reads from a file must be derived from this class
//
////////////////////////////////////////////////////////////////////////
#ifndef FILERESOURCE_H
#define FILERESOURCE_H

#include <stdio.h>

#include "common/global.h" // The global header file

// Come textual definitions for various binary files
extern const char *filePhotonMap;
extern const char *fileIrradianceCache;
extern const char *fileGatherCache;
extern const char *fileTransparencyShadow;
extern const char *filePointCloud;
extern const char *fileBrickMap;

const unsigned int magicNumber = 123456789;
const unsigned int magicNumberReversed = ((magicNumber & 0xFF000000) >> 24) |
                                         ((magicNumber & 0xFF0000) >> 8) |
                                         ((magicNumber & 0xFF00) << 8) |
                                         ((magicNumber & 0xFF) << 24);

///////////////////////////////////////////////////////////////////////
// Class				:	CFileResource
// Description			:	Any class that is read or written to a file must derive from this class
// Comments				:
class CFileResource {
    public:
        CFileResource(const char *);
        virtual ~CFileResource();

        const char *name;
};

FILE *ropen(const char *, const char *, const char *, int probe = FALSE);
FILE *ropen(const char *, char *);

#endif
