//
//  functions.c - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Tue Oct 01 2002
//
//  Original Development:
//    (C) 2002 by Juvenal A. Silva Jr. <juvenal@v2-home.com.br>
//
//  Contributions:
//
//  Statement:
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//  $Id: functions.c,v 1.2 2002/10/22 12:13:06 juvenal Exp $
//

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Private includes
#include "functions.h"

// Data type definitions
struct functionTemplate_t {
  char  *name;
  char  *stack;
  char  *opcode;
  char  rtype;
};

// Define array with functions for shader interpreter
struct functionTemplate_t functions[] = {
            {"faceforward",     "vv",   "faceforward",      'v'},
            {"faceforward",     "nv",   "faceforward",      'n'},
            {"normalize",       "v",    "normalize",        'v'},
            {"normalize",       "n",    "normalize",        'n'},
            {"transform",       "sv",   "vtransforms",      'v'},
            {"transform",       "sn",   "ntransforms",      'n'},
            {"transform",       "sp",   "ptransforms",      'p'},
            {"vtransform",      "sv",   "vtransforms",      'v'},
            {"ntransform",      "sn",   "ntransforms",      'n'},
            {"ptransform",      "sp",   "ptransforms",      'p'},
            {"transform",       "ssv",  "vtransformss",     'v'},
            {"transform",       "ssn",  "ntransformss",     'n'},
            {"transform",       "ssp",  "ptransformss",     'p'},
            {"vtransform",      "ssv",  "vtransformss",     'v'},
            {"ntransform",      "ssn",  "ntransformss",     'n'},
            {"ptransform",      "ssp",  "ptransformss",     'p'},
            {"ambient",         "",     "ambient",          'c'},
            {"diffuse",         "n",    "diffuse",          'c'},
            {"specular",        "nvf",  "specular",         'c'},
            {"area",            "p",    "area",             'f'},
            {"clamp",           "fff",  "fclamp",           'f'},
            {"clamp",           "ccc",  "cclamp",           'c'},
            {"sqrt",            "f",    "sqrt",             'f'},
            {"radians",         "f",    "radians",          'f'},
            {"sin",             "f",    "sin",              'f'},
            {"cos",             "f",    "cos",              'f'},
            {"tan",             "f",    "tan",              'f'},
            {"asin",            "f",    "asin",             'f'},
            {"acos",            "f",    "acos",             'f'},
            {"atan",            "f",    "atan",             'f'},
            {"atan",            "ff",   "atan2",            'f'},
            {"radians",         "f",    "radians",          'f'},
            {"degrees",         "f",    "degrees",          'f'},
            {"pow",             "ff",   "pow",              'f'},
            {"exp",             "f",    "exp",              'f'},
            {"mod",             "ff",   "mod",              'f'},
            {"abs",             "f",    "abs",              'f'},
            {"floor",           "f",    "floor",            'f'},
            {"round",           "f",    "round",            'f'},
            {"min",             "ff",   "fmin",             'f'},
            {"max",             "ff",   "fmax",             'f'},
            {"depth",           "p",    "depth",            'f'},
            {"length",          "v",    "length",           'f'},
            {"distance",        "pp",   "distance",         'f'},
            {"ptlined",         "ppp",  "ptlined",          'f'},
            {"reflect",         "vn",   "reflect",          'v'},
            {"reflect",         "nv",   "reflect",          'n'},
            {"xcomp",           "v",    "xcomp",            'f'},
            {"ycomp",           "v",    "ycomp",            'f'},
            {"zcomp",           "v",    "zcomp",            'f'},
            {"xcomp",           "p",    "xcomp",            'f'},
            {"ycomp",           "p",    "ycomp",            'f'},
            {"zcomp",           "p",    "zcomp",            'f'},
            {"xcomp",           "n",    "xcomp",            'f'},
            {"ycomp",           "n",    "ycomp",            'f'},
            {"zcomp",           "n",    "zcomp",            'f'},
            {"Du",              "f",    "Duf",              'f'},
            {"Dv",              "f",    "Dvf",              'f'},
            {"Du",              "v",    "Duv",              'v'},
            {"Dv",              "v",    "Dvv",              'v'},
            {"Du",              "p",    "Duv",              'v'},
            {"Dv",              "p",    "Dvv",              'v'},
            {"smoothstep",      "fff",  "smoothstep",       'f'},
            {"mix",             "ccf",  "cmix",             'c'},
            {"random",          "",     "random",           'f'},
            {"noise",           "p",    "noisev",           'f'},
            {"noise",           "v",    "noisev",           'f'},
            {"noise",           "f",    "noisef",           'f'},
            {"noise",           "ff",   "noiseff",          'f'},
            {"Inoise",          "ff",   "inoiseff",         'f'},
            {"Inoise",          "ffff", "inoiseffff",       'f'},
            {"cellnoise",       "p",    "cellnoisev",       'f'},
            {"cellnoise",       "v",    "cellnoisev",       'f'},
            {"cellnoise",       "f",    "cellnoisef",       'f'},
            {"cellnoise",       "ff",   "cellnoiseff",      'f'},
            {"calculatenormal", "p",    "calculatenormal",  'n'},
            {"comp",            "cf",   "comp",             'f'},
            {"comp",            "vf",   "comp",             'f'},
            {"comp",            "pf",   "comp",             'f'},
            {"comp",            "nf",   "comp",             'f'},
            {"trace",           "pv",   "trace",            'c'},
            // Cast Functions - must go after ordinar functions to
            // ensure correct version is picked.
            {"noise",           "p",    "colornoisev",      'c'},
            {"noise",           "v",    "colornoisev",      'c'},
            {"noise",           "f",    "colornoisef",      'c'},
            {"noise",           "ff",   "colornoiseff",     'c'},
            {"noise",           "p",    "pointnoisev",      'p'},
            {"noise",           "v",    "pointnoisev",      'p'},
            {"noise",           "f",    "pointnoisef",      'p'},
            {"noise",           "ff",   "pointnoiseff",     'p'},
            {"noise",           "p",    "pointnoisev",      'v'},
            {"noise",           "v",    "pointnoisev",      'v'},
            {"noise",           "f",    "pointnoisef",      'v'},
            {"noise",           "ff",   "pointnoiseff",     'v'},
            {"noise",           "p",    "pointnoisev",      'n'},
            {"noise",           "v",    "pointnoisev",      'n'},
            {"noise",           "f",    "pointnoisef",      'n'},
            {"noise",           "ff",   "pointnoiseff",     'n'},
            {"cellnoise",       "f",    "colorcellnoisef",  'c'},
            {"cellnoise",       "ff",   "colorcellnoiseff", 'c'},
            {"cellnoise",       "v",    "colorcellnoisev",  'c'},
            {"cellnoise",       "p",    "colorcellnoisev",  'c'},
            {"cellnoise",       "f",    "pointcellnoisef",  'v'},
            {"cellnoise",       "ff",   "pointcellnoiseff", 'v'},
            {"cellnoise",       "v",    "pointcellnoisev",  'v'},
            {"cellnoise",       "p",    "pointcellnoisev",  'v'},
            {NULL, NULL, NULL, NULL}
};

// Define array with
struct functionTemplate_t textures[] = {
            {"t",   "ffffffff",   "ftexture",       'f'},
            {"t",   "ffffffff",   "ctexture",       'c'},
            {"t",   "ff",         "ftexture",       'f'},
            {"t",   "ff",         "ctexture",       'c'},
            {"t",   "",           "ftexture",       'f'},
            {"t",   "",           "ctexture",       'c'},
            {"e",   "v",          "fenvironment",   'f'},
            {"e",   "v",          "cenvironment",   'c'},
            {"s",   "p",          "shadow",         'f'},
            {"b",   "nvv",        "bump",           'v'},
};

// External function calls
extern void
error ( char *msg);

// External variables
extern char string[];

// Global variables
int tcount = 10;





char
t ( char n) {
  if ( n == 'c' || n == 'f' || n == 's') {
    return n;
  }
  else {
    return 'v';
  }
}


struct list_t*
getList () {
  struct list_t *l;
  l = ( struct list_t*) malloc ( sizeof ( struct list_t));
  l->count = 0;
  l->stack[l->count] = 0;

  return l;
}


void
appendToList ( struct node_t *n, struct list_t *l) {
  l->code[l->count] = ( char*) malloc ( strlen ( n->code) + 1);
  strcpy ( l->code[l->count], n->code);
  l->stack[l->count] = n->stack;
  l->count++;
  l->stack[l->count] = 0;
}


void
freeList ( struct list_t *l) {
  int i;

  for ( i = 0; i < l->count; i++) {
    free ( l->code[i]);
  }
  free ( l);
}


struct node_t*
getNode ( char *cs, char ss) {
  struct node_t *n;

  //printf ( "getting: %s, %c\n", cs, ss);
  n = ( struct node_t*) malloc ( sizeof ( struct node_t));

  n->code = ( char*) malloc ( strlen ( cs) + 1);
  strcpy ( n->code, cs);

  n->stack = ss;

  return n;
}


void
freeNode ( struct node_t *n) {
  //printf ( "Freeing: %s\n", n->code);
  free ( n->code);
  free ( n);
}


struct node_t*
handleCastFunction ( char           retType,
                     char          *funcName,
                     struct list_t *params) {

  int  paramCount, nameMatch = 0, i;
  char tmpString[1000];

  for ( i = 0; functions[i].name != NULL; i++) {
    if ( strcmp ( functions[i].name, funcName) == 0) {
      if ( strcmp ( functions[i].stack, params->stack) == 0 &&
           ( retType == ' ' || functions[i].rtype == retType)) {
        string[0] = 0;
        for ( paramCount = params->count; paramCount-- > 0;) {
          strcat ( string, params->code[paramCount]);
        }
        strcat ( string, functions[i].opcode);
        strcat ( string, "\n");
        return getNode ( string, functions[i].rtype);
      }
      else {
        nameMatch = 1;
      }
    }
  }

  // special cases
  if ( strcmp ( "spline", funcName) == 0) {
    if ( params->count < 5) {
      error ( "Insufficient parameters for spline");
    }

    // If no return type, then use the first parameter...
    if ( retType == ' ') {
      retType = params->stack[1];
    }

    for ( i = params->count; i-- > 0;) {
      if ( i != 0 && params->stack[i] != retType) {
        error ( "Bad parameters for spline");
      }
      if ( i == 0 && params->stack[i] != 'f') {
        error ( "Bad parameters for spline");
      }
      strcat ( string, params->code[i]);
    }
    sprintf ( tmpString, "pushif %d\n", params->count-1);
    strcat ( string, tmpString);
    if ( retType == 'c') {
      strcat ( string, "colorspline\n");
      return getNode ( string, 'c');
    }
    if ( retType == 'f') {
      strcat ( string, "spline\n");
      return getNode ( string, 'f');
    }
    error ( "Bad type for spline");
  }

  // OK give up
  if ( nameMatch) {
    sprintf ( string, "Bad parameters %c (%s) to function:%s",
              retType, params->stack, funcName);
  }
  else {
    sprintf ( string, "Unknown function:%c %s (%s)",
              retType, funcName, params->stack);
  }
  error ( string);
  return 0;
}


struct node_t*
handleTextureFunction ( char           retType,
                        char           funcName,
                        struct node_t *filename,
                        struct node_t *channel,
                        struct list_t *params) {
  int  i, nameMatch = 0;
  char tmpString[1000];
  int  paramCount;

  for ( i = 0; i < tcount; i++) {
    if ( textures[i].name[0] == funcName) {
      if ( strncmp ( textures[i].stack, params->stack, strlen ( textures[i].stack)) == 0 &&
           ( retType == ' ' || textures[i].rtype == retType)) {
        string[0] = 0;
        for ( paramCount = params->count; paramCount-- > 0;) {
          strcat ( string, params->code[paramCount]);
        }
        // Channel
        strcat ( string, C ( channel));
        // Filename
        strcat ( string, C ( filename));
        // args...
        sprintf ( tmpString, "pushif %d\n", params->count);
        strcat ( string, tmpString);
        // stackFix
        sprintf ( tmpString, "pushif %d\n",
                  params->count - strlen ( textures[i].stack));
        strcat ( string, tmpString);
        // Opcode
        strcat ( string, textures[i].opcode);
        strcat ( string, "\n");
        return getNode ( string, textures[i].rtype);
      }
      else {
        nameMatch = 1;
      }
    }
  }

  // OK give up
  if ( nameMatch) {
    sprintf ( string, "Bad parameters %c (%s) to texture function: %c",
              retType, params->stack, funcName);
  }
  else {
    sprintf ( string, "Unknown texture function: %c %c (%s)",
              retType, funcName, params->stack);
  }
  error ( string);
  return 0;
}
