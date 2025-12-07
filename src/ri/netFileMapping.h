/**
 * Project: Pixie
 *
 * File: netFileMapping.h
 *
 * Description:
 *   This file defines the interface for netFileMapping.
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
//  File				:	netFileMapping.h
//  Classes				:	CNetFileMapping
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef NETFILEMAPPING_H
#define NETFILEMAPPING_H

///////////////////////////////////////////////////////////////////////
// Class				:	CNetFileMapping
// Description			:	Maps files to alternate paths
// Comments				:
class CNetFileMapping {
    public:
        CNetFileMapping(const char *from, const char *to) {
            this->from = strdup(from);
            this->to = strdup(to);
        }

        ~CNetFileMapping() {
            free(from);
            free(to);
        }

        char *from, *to;
};

#endif
