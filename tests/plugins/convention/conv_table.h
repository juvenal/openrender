/**
 * Project: openRender
 *
 * File: conv_table.h
 *
 * Description:
 *   Spec 037, PluginAbi_Convention: the throwaway plug-in type the conv_* fixtures and the test
 *   program share. Strict C17 (and C++20): it includes orplugin.h only, the way a plug-in author
 *   would. "conv" stands for any family built on the shared convention; it is not a real interface.
 *
 *   MAJOR 1.0 of the table is the header and two entries (ConvPlugin). MINOR 1 appends a third entry
 *   (ConvPluginV11). The host's required size for MAJOR 1 is sizeof(ConvPlugin): a table of at least
 *   that size is accepted, a smaller one is rejected, and the host reads the fields added in a later
 *   MINOR only when structSize covers them.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef CONV_TABLE_H
#define CONV_TABLE_H

#include "orplugin.h"

typedef struct ConvPlugin {
        OrTableHeader header;
        void (*first)(void);
        void (*second)(void);
} ConvPlugin;

typedef struct ConvPluginV11 {
        OrTableHeader header;
        void (*first)(void);
        void (*second)(void);
        void (*third)(void);
} ConvPluginV11;

/* The registration entry point of the shape contracts/plugin-convention.md section 1 gives. */
typedef const void *(*ConvRegisterFunction)(const OrHostInfo *host);

#endif
