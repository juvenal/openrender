/**
 * Project: openRender
 *
 * File: sobolTables.cpp
 *
 * Description:
 *   Sobol quasi-random sequence tables.
 *   Moved from src/ri/random.cpp to break circular dependency with libshader.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "sobolTables.h"

// primitive polynomials in binary encoding
const int primitive_polynomials[SOBOL_MAX_DIMENSION] = {
    1, 3, 7, 11, 13, 19, 25, 37, 59, 47,
    61, 55, 41, 67, 97, 91, 109, 103, 115, 131,
    193, 137, 145, 143, 241, 157, 185, 167, 229, 171,
    213, 191, 253, 203, 211, 239, 247, 285, 369, 299};

// degrees of the primitive polynomials
const int degree_table[SOBOL_MAX_DIMENSION] = {
    0, 1, 2, 3, 3, 4, 4, 5, 5, 5,
    5, 5, 5, 6, 6, 6, 6, 6, 6, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 8, 8, 8};

// initial values for direction tables, following
// Bratley+Fox, taken from [Sobol+Levitan, preprint 1976]
const int v_init[8][SOBOL_MAX_DIMENSION] = {
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 1, 3, 1, 3, 1, 3, 3, 1,
     3, 1, 3, 1, 3, 1, 1, 3, 1, 3,
     1, 3, 1, 3, 3, 1, 3, 1, 3, 1,
     3, 1, 3, 1, 3, 1, 3, 5, 7, 7},
    {0, 0, 0, 7, 5, 1, 3, 3, 7, 5,
     1, 3, 3, 7, 5, 1, 1, 5, 3, 3,
     1, 7, 5, 1, 3, 3, 7, 5, 1, 3,
     3, 7, 5, 1, 3, 3, 7, 9, 5, 13},
    {0, 0, 0, 0, 0, 1, 7, 9, 13, 11,
     1, 3, 7, 9, 5, 13, 13, 11, 3, 15,
     5, 3, 15, 7, 9, 13, 9, 1, 11, 7,
     5, 15, 1, 15, 11, 5, 3, 1, 7, 9},
    {0, 0, 0, 0, 0, 0, 0, 9, 3, 27,
     15, 29, 21, 23, 19, 11, 25, 7, 13, 17,
     1, 25, 29, 3, 31, 11, 5, 23, 27, 19,
     21, 5, 1, 17, 13, 7, 15, 9, 31, 9},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 37, 33, 7, 5, 11, 39, 63,
     27, 17, 15, 23, 29, 3, 21, 13, 31, 25,
     9, 49, 33, 19, 29, 11, 19, 27, 15, 25},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 13,
     33, 115, 41, 79, 17, 29, 119, 75, 73, 105,
     7, 59, 65, 21, 3, 113, 61, 89, 45, 107},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 7, 23, 39}};
