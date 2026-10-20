/**
 * Project: openRender
 *
 * File: rsloPrototypeMatch.h
 *
 * Description:
 *   The .rslo loader's built-in function prototype matcher, used by
 *   setOpcode() in rslo.y to bind a call in a compiled shader to an entry of
 *   the functions[] table. Pure: it reads only the two strings it is given.
 *
 *   A prototype is "<return>=<parameters>". The table prototype is a
 *   pattern over the call prototype's parameters:
 *     - a type letter matches the same letter exactly ('s' and 'S' differ);
 *     - '.' matches any one parameter;
 *     - "x+" matches one or more x, "x*" zero or more x (x may be '.');
 *       nothing may follow the repeated element in the call;
 *     - '!' matches a parameter list: zero or more (name, value) pairs,
 *       each name being 's' or 'S', the value any one parameter; a name
 *       with no value does not match.
 *   The return types must be identical.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RSLOPROTOTYPEMATCH_H
#define RSLOPROTOTYPEMATCH_H

#include <cstring>

namespace libshader {

    // TRUE if the call prototype `call` binds to the table prototype `table`.
    // Both strings are NUL-terminated.
    inline bool rsloPrototypeMatches(const char *table, const char *call) {
        if (strcmp(table, call) == 0)
            return true;

        // The return types must match exactly
        if (table[0] != call[0])
            return false;

        int j, k;

        for (j = 2, k = 2; (call[j] != '\0') && (table[k] != '\0'); j++, k++) {
            // Do the current parameters match
            if (call[j] == table[k])
                continue;
            // Does the function match any parameter ?
            else if (table[k] == '.')
                continue;
            // Is the current parameter a * or + ?
            else if ((table[k] == '+') || (table[k] == '*')) {
                k -= 2;
                j--;
                continue;
                // Is the current parameter a parameter list ?
            }
            else if (table[k] == '!') {
                if ((call[j] == 's') || (call[j] == 'S')) {
                    // A name with no value after it: reject (GitHub #67).
                    // Stepping over the missing value would read past the
                    // call's terminator, and plBegin() (execute.cpp) reads a
                    // value operand for every name, one past the
                    // instruction's arguments here.
                    if (call[j + 1] == '\0')
                        return false;
                    j++;
                    k--;
                }
                else {
                    return false;
                }
            }
            else {
                return false;
            }
        }

        // table[k + 1] is read only when table[k] is not the terminator
        // (GitHub #67): a table prototype that ends before the call does
        // not match.
        return ((table[k] == '\0') && (call[j] == '\0')) || ((table[k] == '+') || (table[k] == '*')) ||
               (table[k] == '!') || ((table[k] != '\0') && (table[k + 1] == '*'));
    }

} // namespace libshader

#endif // RSLOPROTOTYPEMATCH_H
