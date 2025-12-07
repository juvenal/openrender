/**
 * Project: Pixie
 *
 * File: noise.h
 *
 * Description:
 *   This file defines the interface for noise.
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
//  File				:	noise.h
//  Classes				:	-
//  Description			:	The misc noise functions
//
////////////////////////////////////////////////////////////////////////
#ifndef NOISE_H
#define NOISE_H

#include "common/global.h"

float noiseFloat(float);
float noiseFloat(float, float);
float noiseFloat(const float *);
float noiseFloat(const float *, float);
void noiseVector(float *, float);
void noiseVector(float *, float, float);
void noiseVector(float *, const float *);
void noiseVector(float *, const float *, float);

float pnoiseFloat(float, float);
float pnoiseFloat(float, float, float, float);
float pnoiseFloat(const float *, const float *);
float pnoiseFloat(const float *, float, const float *, float);
void pnoiseVector(float *, float, float);
void pnoiseVector(float *, float, float, float, float);
void pnoiseVector(float *, const float *, const float *);
void pnoiseVector(float *, const float *, float, const float *, float);

float cellNoiseFloat(float);
float cellNoiseFloat(float, float);
float cellNoiseFloat(const float *);
float cellNoiseFloat(const float *, float);
void cellNoiseVector(float *, float);
void cellNoiseVector(float *, float, float);
void cellNoiseVector(float *, const float *);
void cellNoiseVector(float *, const float *, float);

#endif
