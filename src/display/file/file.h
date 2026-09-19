/**
 * Project: openRender
 *
 * File: file.h
 *
 * Description:
 *   Compatibility header — the unified file-format plugin dispatcher.
 *   All format classes now derive from CFileOutputBase (file_base.h).
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 */

#ifndef FILE_H
#define FILE_H

#include "file_base.h"

// Legacy alias kept for any code that still refers to CFileFramebuffer.
using CFileFramebuffer = CFileOutputBase;

#endif // FILE_H
