/**
 * Project: openRender
 *
 * File: sobolTables.h
 *
 * Description:
 *   Sobol quasi-random sequence tables (header).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef SOBOL_TABLES_H
#define SOBOL_TABLES_H

#define SOBOL_MAX_DIMENSION 40

// Sobol sequence primitive polynomials and direction table initialization data.
// Used by CSobol in shading.h for quasi-random sampling.
extern const int primitive_polynomials[SOBOL_MAX_DIMENSION];
extern const int degree_table[SOBOL_MAX_DIMENSION];
extern const int v_init[8][SOBOL_MAX_DIMENSION];

#endif // SOBOL_TABLES_H
