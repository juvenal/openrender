/**
 * Project: openRender
 *
 * File: pp_all.c
 *
 * Description:
 *   Consolidated C preprocessor — combines pp1.c through pp8.c into a
 *   single translation unit for maintainability.
 *
 *   The original Logical Systems PP preprocessor was split across 8 files
 *   that share global state via the EXTERN/MAIN convention in pp.h:
 *     - pp1.c defines #define MAIN before including pp.h, which causes
 *       pp.h to expand EXTERN as "" (making global variables definitions).
 *     - pp2.c–pp8.c do NOT define MAIN, so pp.h expands EXTERN as "extern"
 *       (making global variables extern declarations).
 *
 *   To replicate this in a single TU we include pp1.c first (so its globals
 *   are defined), then #undef MAIN, then include pp2–pp8 (so they get extern
 *   declarations pointing at pp1.c's definitions).
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

/* pp1.c defines MAIN before including pp.h, giving us the global definitions. */
#include "pp1.c"

/* After pp1.c, MAIN is defined in the macro namespace.
 * Undefine it so the remaining files get extern declarations via pp.h. */
#undef MAIN

#include "pp2.c"
#include "pp3.c"
#include "pp4.c"
#include "pp5.c"
#include "pp6.c"
#include "pp7.c"
#include "pp8.c"
