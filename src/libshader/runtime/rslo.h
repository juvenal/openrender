/**
 * Project: openRender
 *
 * File: rslo.h
 *
 * Description:
 *   This file defines the interface for rslo (RenderMan Shading Language Object).
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
//  File				:	rslo.h
//  Classes				:	-
//  Description			:	The shader library interface
//
////////////////////////////////////////////////////////////////////////
#ifndef RSLO_H
#define RSLO_H

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
} ERSLObjectType;

// Shader type
typedef enum {
    SHADER_SURFACE,
    SHADER_DISPLACEMENT,
    SHADER_VOLUME,
    SHADER_LIGHT,
    SHADER_IMAGER
} ERSLObjectShaderType;

// Container class
typedef enum {
    CONTAINER_CONSTANT,
    CONTAINER_UNIFORM,
    CONTAINER_VARYING,
    CONTAINER_VERTEX
} ERSLObjectContainer;

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
typedef struct TRSLObjectParameter {
        char *name;                 // Name of the parameter
        ERSLObjectType type;        // Type of the parameter
        ERSLObjectContainer container; // Container class of the parameter
        int writable;               // Is it an output
        int numItems;               // Number of items (the number of items if an array, 1 otherwise)
        char *space;                // The space that the default value is expressed in
        UDefaultVal defaultValue;   // The default value
        struct TRSLObjectParameter *next; // The next parameter

} TRSLObjectParameter;

// Shader class
typedef struct TRSLObjectShader {
        char *name;                       // Name of the shader
        ERSLObjectShaderType type;        // Type of the shader
        struct TRSLObjectParameter *parameters; // A linked list of parameters to the shader
} TRSLObjectShader;

// The library interface
#ifdef __cplusplus
extern "C" {
#endif

LIB_EXPORT TRSLObjectShader *rsloGet(const char *, const char *); // Query a shader
LIB_EXPORT void rsloDelete(TRSLObjectShader *);                   // Delete a shader

#ifdef __cplusplus
}
#endif

#endif
