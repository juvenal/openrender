/**
 * Project: openRender
 *
 * File: formatVersion.h
 *
 * Description:
 *   The compatibility rule shared by the shader file formats (rsloFormat.h for
 *   .rslo, sloFormat.h for .slo), and a strict parser for a "major.minor.patch"
 *   string.
 *
 *   A loader reads a file when the file's MAJOR equals the loader's and the
 *   file's MINOR is not greater than the loader's. The PATCH never takes part.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef FORMAT_VERSION_H
#define FORMAT_VERSION_H

#include <cctype>
#include <cstdlib>

struct SFormatVersion {
        int major = 0;
        int minor = 0;
        int patch = 0;
};

// True when a loader that supports `supportedMajor.supportedMinor` can read a file
// written in format `fileMajor.fileMinor`.
inline bool formatVersionReadable(int fileMajor, int fileMinor, int supportedMajor, int supportedMinor) {
    return fileMajor == supportedMajor && fileMinor <= supportedMinor;
}

// Parse "major.minor.patch": three non-negative decimal numbers, separated by
// dots, optionally followed by whitespace. Anything else is rejected and `out` is
// left alone.
inline bool parseFormatVersion(const char *text, SFormatVersion &out) {
    if (!text)
        return false;
    int part[3];
    const char *p = text;
    for (int i = 0; i < 3; ++i) {
        if (!isdigit(static_cast<unsigned char>(*p)))
            return false;
        char *end = nullptr;
        const long v = strtol(p, &end, 10);
        if (v < 0 || v > 1000000)
            return false;
        part[i] = static_cast<int>(v);
        p = end;
        if (i < 2) {
            if (*p != '.')
                return false;
            ++p;
        }
    }
    while (*p && isspace(static_cast<unsigned char>(*p)))
        ++p;
    if (*p)
        return false;
    out.major = part[0];
    out.minor = part[1];
    out.patch = part[2];
    return true;
}

#endif
