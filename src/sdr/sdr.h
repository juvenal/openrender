/**
 * Project: Pixie
 *
 * File: sdr.h
 *
 * Description:
 *   This file defines the interface for sdr.
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
//  File				:	sdr.h
//  Classes				:	-
//  Description			:	The shader library interface
//
////////////////////////////////////////////////////////////////////////
#ifndef SDR_H
#define SDR_H

#ifndef LIB_EXPORT
#ifdef _WINDOWS
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT extern
#endif
#endif

// Variable type
typedef enum {
    TYPE_FLOAT,
    TYPE_VECTOR,
    TYPE_NORMAL,
    TYPE_POINT,
    TYPE_COLOR,
    TYPE_MATRIX,
    TYPE_STRING
} ESdrType;

// Shader type
typedef enum {
    SHADER_SURFACE,
    SHADER_DISPLACEMENT,
    SHADER_VOLUME,
    SHADER_LIGHT,
    SHADER_IMAGER
} ESdrShaderType;

// Container class
typedef enum {
    CONTAINER_CONSTANT,
    CONTAINER_UNIFORM,
    CONTAINER_VARYING,
    CONTAINER_VERTEX
} ESdrContainer;

// Default value holder
typedef union UDefaultVal *UDefaultValPtr;
typedef union UDefaultVal {
        float *matrix;
        float *vector;
        float scalar;
        char *string;
        UDefaultValPtr array;
} UDefaultVal;

// Linked list of shader parameters
typedef struct TSdrParameter {
        char *name;                 // Name of the parameter
        ESdrType type;              // Type of the parameter
        ESdrContainer container;    // COntainer class of the parameter
        int writable;               // Is it an output
        int numItems;               // Number of items (the number of items if an array, 1 otherwise)
        char *space;                // The space that the default value is expressed in
        UDefaultVal defaultValue;   // The default value
        struct TSdrParameter *next; // The next parameter

} TSdrParameter;

// Shader class
typedef struct TSdrShader {
        char *name;                       // Name of the shader
        ESdrShaderType type;              // Type of the shader
        struct TSdrParameter *parameters; // A linked list of parameters to the shader
} TSdrShader;

// The library interface
#ifdef __cplusplus
extern "C" {
#endif

LIB_EXPORT TSdrShader *sdrGet(const char *, const char *); // Query a shader
LIB_EXPORT void sdrDelete(TSdrShader *);                   // Delete a shader

#ifdef __cplusplus
}
#endif

#endif
