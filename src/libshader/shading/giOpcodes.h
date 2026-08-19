/**
 * Project: openRender
 *
 * File: giOpcodes.h
 *
 * Description:
 *   This file defines the interface for giOpcodes.
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
//  File				:	giOpcodes.h
//  Classes				:	-
//  Description			:	Shading language Global Illumination opcodes.
//
////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	gather <else>
#ifndef INIT_SHADING
#define GATHEREXPR_PRE                                                \
    if (gatherSample(lastGather, tags, numActive, numPassive,         \
                      varying[VARIABLE_N], varying[VARIABLE_TIME])) { \
        jmp(argument(0));                                             \
    }

#else
#define GATHEREXPR_PRE
#endif

DEFOPCODE(Gather, "gather", 1, GATHEREXPR_PRE, NULL_EXPR, NULL_EXPR, NULL_EXPR, PARAMETER_N | PARAMETER_RAYTRACE)

#undef GATHEREXPR_PRE

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	gatheElse <endLabel>
#ifndef INIT_SHADING
#define GATHERELSEEXPR_PRE                                      \
    if (gatherElseFlip(tags, numActive, numPassive)) {           \
        jmp(argument(0));                                       \
    }

#else
#define GATHERELSEEXPR_PRE
#endif

DEFOPCODE(GatherElse, "gatherElse", 1, GATHERELSEEXPR_PRE, NULL_EXPR, NULL_EXPR, NULL_EXPR, 0)

#undef GATHERELSEEXPR_PRE

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	gatheEnd <gatherLabel>
#ifndef INIT_SHADING
#define GATHERENDEXPR_PRE                                       \
    if (gatherEndAdvance(lastGather, tags, numActive, numPassive)) { \
        jmp(argument(0));                                       \
    }

#else
#define GATHERENDEXPR_PRE
#endif

DEFOPCODE(GatherEnd, "gatherEnd", 1, GATHERENDEXPR_PRE, NULL_EXPR, NULL_EXPR, NULL_EXPR, 0)

#undef GATHERENDEXPR_PRE
