%token DIGIT VARIABLE STRING SEPERATOR COMMA VARTYPE SHADERTYPE LBRACKET RBRACKET LBRACE RBRACE ADD SUB MUL DIV EQUALS ASSIGN NEQ FLOATCMP IF ELSE WHILE CONTINUE DOT CROSS ADDASSIGN SUBASSIGN MULASSIGN DIVASSIGN STORAGECLASS FOR BOOLEANOP QUERY COLON SOLAR ILLUMINATE ILLUMINANCE FRESNEL RETURN BREAK TEXTURE CHANNEL

%{
//
//  parse3.y - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Wed Oct 02 2002
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
//  $Id: parse3.y,v 1.2 2004/01/07 11:33:19 juvenal Exp $
//

// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Private includes
#include "functions.h"

// Some defines
#define MAXTABLE 100

// Global variables
char  string[100000],
      VarDecCode[100][100];
int   VarDecCount = 0,
      HasParamInit = 0,
      labelno = 0;

// External global variables
extern char debugString[1024];

// External functions
extern void error ( char *msg);
extern void warning ( char *msg);


// Data type definitions
struct refer {
  char name[100];
  char type ;
} table[MAXTABLE];

/* Locate a Global Variable's type */
char
lookup ( char *label) {
  int i;

  if ( strlen ( label) > 98) {     // Truncate
    label[98] = 0;
  }
  for ( i = 0; i < MAXTABLE; i++) {
    if ( strcmp ( label, table[i].name) == 0) {
      return ( table[i].type);
    }
  }

  return(0);
}

/* Create a Global Variable */
void
setreference ( char *name, char type) {
  int i;

  if ( strlen ( name) > 98) {
    name[98] = 0;
  }
  i = lookup ( name);                        // Does it already exist
  if ( i != 0) {
    sprintf ( string, "Variable %s multiply defined", name);
    error ( string);
  }
  for ( i = 0; table[i].type != 0; i++) {    // Find end of Table
    if ( i >= MAXTABLE) {
      error ( "Table overflow");
    }
  }
  strcpy ( table[i].name, name);             // Name and Type
  table[i].type = type;
}

struct node_t*
doPushVar ( char *name) {
  char varType = lookup ( name);

  if ( varType == 0) {
    sprintf ( string, "%s not defined", name);
    error ( string);
  }

  sprintf ( string, "\tpush%c\t%s\n", t ( varType), name);
  return getNode ( string, varType);
}

struct node_t*
doDrop ( struct node_t *expression) {
  int  i, testPoint=0;
  char searchString[10];

  if ( S ( expression) == ' ') {
    return expression;
  }

  sprintf ( searchString, "\tpush%c", T ( expression));

  for ( i = strlen ( C ( expression)) - 2; i > 0; i--) {
    if ( C ( expression)[i] == '\n') {
      testPoint = i + 1;
      break;
    }
  }

  if ( strncmp ( searchString, C ( expression) + testPoint, 5) == 0) {
    C ( expression)[i + 1] = 0;
    return expression;
  }
  else {
    error ( "Useless (and hard to compile!) expression");
  }

  return NULL;
}

void
doBreak ( struct node_t *expression, char *oldString, int label) {
  char *target;
  int i;

  while ( ( target = strstr ( C ( expression), oldString))) {
    sprintf ( string, "%d", label);
    for ( i = 0; i < strlen ( oldString); i++) {
      *( target + i) = ' ';
    }
    for ( i = 0; i < strlen ( string); i++) {
      *( target + i) = string[i];
    }
  }
}

struct node_t*
doAssign ( char *varName, struct node_t *expression,int push) {
  char varType = lookup ( varName);

  if ( varType == 0) {
    sprintf ( string, "%s not defined", varName);
    error ( string);
  }

  if ( varType == expression->stack) {
    sprintf ( string, "%s%spop%c\t%s\n", debugString, C(expression), t(varType), varName);
  }
  else if ( expression->stack == 'f') {
    sprintf ( string, "%s%s%cset\t%s\n", debugString, C(expression), t(varType), varName);
  }
  else if ( t(varType) == T(expression)) {
    sprintf ( string, "assigning: %c=%c", varType, expression->stack);
    warning ( string);
    sprintf ( string, "%s%spop%c\t%s\n", debugString, C(expression), t(varType), varName);
  }
  else {
    sprintf ( string,"Bad assign type:%c=%c", varType, expression->stack);
    error ( string);
  }
  if ( push) {
    char *str;
    str = string + strlen ( string);
    sprintf ( str, "\tpush%c\t%s\n", t(varType), varName);
  }
  return getNode ( string, push ? varType : ' ');
}

struct node_t*
doAdd ( struct node_t *lexp, struct node_t *rexp) {
  sprintf ( string, "%s%sadd%c%c\n", C ( rexp), C ( lexp), T ( lexp), T ( rexp));
  switch ( lexp->stack) {
    case 'c':
      switch ( rexp->stack) {
        case 'c':
          return getNode ( string, 'c');
          break;
        case 'v':
        case 'n':
        case 'p':
          error ( "missmatched Add operation (v+c)");
          break;
        case 'f':
          return getNode ( string, 'c');
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'v':
    case 'n':
    case 'p':
      switch ( rexp->stack) {
        case 'c':
          error ( "missmatched Add operation (c+v)");
        case 'v':
        case 'n':
        case 'p':
          /*
          if ( lexp->stack != rexp->stack)
            warning ( "missmatched Add operation);
          */
          return getNode ( string, lexp->stack);
          break;
        case 'f':
          error ( "missmatched Add operation (f+v)");
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'f':
      switch ( rexp->stack) {
        case 'c':
          return getNode ( string,'c');
          break;
        case 'v':
        case 'n':
        case 'p':
          error ( "missmatched Add operation (v+f)");
          break;
        case 'f':
          return getNode ( string, 'f');
          break;
        default:
          error ( "internal error");
      }
      break;
    default:
      error ( "internal error");
  }

  return NULL;
}

struct node_t*
doSub ( struct node_t *lexp, struct node_t *rexp) {
  sprintf ( string, "%s%ssub%c%c\n", C ( rexp), C ( lexp), T ( lexp), T ( rexp));
  switch ( lexp->stack) {
    case 'c':
      switch ( rexp->stack) {
        case 'c':
          return getNode ( string, 'c');
          break;
        case 'v':
        case 'n':
        case 'p':
          error ( "missmatched Subtract operation");
          break;
        case 'f':
          return getNode ( string, 'c');
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'v':
      switch ( rexp->stack) {
        case 'c':
          error ( "missmatched Subtract operation");
        case 'v':
        case 'n':
        case 'p':
          return getNode ( string, 'v');
          break;
        case 'f':
          return getNode ( string, 'v');
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'n':
      switch ( rexp->stack) {
        case 'c':
          error ( "missmatched Subtract operation");
        case 'v':
        case 'n':
        case 'p':
          return getNode ( string, 'v');
          break;
        case 'f':
          return getNode ( string, 'n');
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'p':
      switch ( rexp->stack) {
        case 'c':
          error ( "missmatched Subtract operation");
        case 'v':
        case 'n':
          return getNode ( string, 'p');
        case 'p':
          return getNode ( string, 'v');
          break;
        case 'f':
          return getNode ( string, 'p');
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'f':
      switch ( rexp->stack) {
        case 'c':
          return getNode ( string, 'c');
          break;
        case 'v':
        case 'n':
        case 'p':
          error ( "missmatched Subtract operation");
          break;
        case 'f':
          return getNode ( string, 'f');
          break;
        default:
          error ( "internal error");
      }
      break;
    default:
      error ( "internal error");
  }

  return NULL;
}


struct node_t*
doDiv ( struct node_t *lexp, struct node_t *rexp) {
  sprintf ( string, "%s%sdiv%c%c\n", C ( rexp), C ( lexp), T ( lexp), T ( rexp));
  switch ( lexp->stack) {
    case 'c':
      switch ( rexp->stack) {
        case 'c':
          error ( "missmatched Divide operation");
          break;
        case 'v':
        case 'n':
        case 'p':
          error ( "missmatched Divide operation");
          break;
        case 'f':
          return getNode ( string, 'c');
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'v':
    case 'n':
    case 'p':
      switch ( rexp->stack) {
        case 'c':
          error ( "missmatched Divide operation");
          break;
        case 'v':
        case 'n':
        case 'p':
          error ( "missmatched Divide operation");
          break;
        case 'f':
          return getNode ( string, lexp->stack);
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'f':
      switch ( rexp->stack) {
        case 'c':
          error ( "missmatched Divide operation");
          break;
        case 'v':
        case 'n':
        case 'p':
          error ( "missmatched Divide operation");
          break;
        case 'f':
          return getNode ( string, 'f');
          break;
        default:
          error ( "internal error");
      }
      break;
    default:
      error ( "internal error");
  }

  return NULL;
}

struct node_t*
doMul ( struct node_t *lexp, struct node_t *rexp) {
  sprintf ( string, "%s%smul%c%c\n", C ( rexp), C ( lexp), T ( lexp), T ( rexp));
  switch ( lexp->stack) {
    case 'c':
      switch ( rexp->stack) {
        case 'c':
          return getNode ( string, 'c');
          break;
        case 'v':
        case 'n':
        case 'p':
          error ( "missmatched Multiply operation");
          break;
        case 'f':
          return getNode ( string, 'c');
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'v':
    case 'n':
    case 'p':
      switch ( rexp->stack) {
        case 'c':
          error ( "missmatched Multiply operation");
        case 'v':
        case 'n':
        case 'p':
          return getNode ( string,'v');
          break;
        case 'f':
          return getNode ( string, lexp->stack);
          break;
        default:
          error ( "internal error");
      }
      break;
    case 'f':
      return getNode ( string, rexp->stack);
      break;
    default:
      error ( "internal error");
  }

  return NULL;
}

%}

%right ASSIGN ADDASSIGN SUBASSIGN MULASSIGN DIVASSIGN
%right QUERY
%left BOOLEANOP
%right EQUALS NEQ FLOATCMP
%left ADD SUB
%left MUL DIV
%left DOT

%%

input:
        preamble shaderdec
        | functiondec input
        ;

preamble:
                                  { printf ( "openShader 1.0\n"); }
        ;


paramDecList:
        paramDec SEPERATOR paramDecList
                                  { sprintf ( string, "%s%s", C ( $1), C ( $3));
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | paramDec
                                  { $$ = $1; }
        |
                                  { string[0] = 0;
                                    $$ = ( int) getNode ( string, ' ');
                                  }
        ;

paramDecStart:
        VARTYPE
                                  { string[0] = 0;
                                    $$ = ( int) getNode ( string, ( char) $1);
                                  }
        | STORAGECLASS VARTYPE
                                  { string[0] = 0;
                                    $$ = ( int) getNode ( string, ( char) $2);
                                  }
        | paramDec COMMA
                                  { $$ = $1; }
        ;

paramDec:
        paramDecStart VARIABLE ASSIGN STRING
                                  { if ( S ( $1) != 's') {
                                      error ( "Type Mismatch in Declaration");
                                    }
                                    sprintf( string, "param\tstring\t%s\t\t%s\n", ( char*) $2,( char*) $4);
                                    setreference ( ( char *) $2, 's');
                                    sprintf ( VarDecCode[VarDecCount], "%s", string);
                                    VarDecCount++;
                                    $$ = $1;
                                    free ( ( void *) $2);
                                    free ( ( void *) $4);
                                  }
        | paramDecStart VARIABLE ASSIGN expression
                                  { struct node_t *tmpNode;
                                    switch ( S ( $1)) {
                                      case 'f':
                                        sprintf ( string, "param\tfloat\t%s\t\t0\n", ( char*) $2);
                                        break;
                                      case 's':
                                        sprintf ( string, "param\tstring\t%s\t\t\"\"\n", ( char*) $2);
                                        break;
                                      case 'p':
                                        sprintf ( string, "param\tpoint\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'n':
                                        sprintf ( string, "param\tnormal\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'v':
                                        sprintf ( string, "param\tvector\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'c':
                                        sprintf ( string, "param\tcolor\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      default:
                                        sprintf ( string, "Bad type %c\n", ( char) $1);
                                        error ( string);
                                    }
                                    setreference ( ( char *) $2, S ( $1));
                                    sprintf ( VarDecCode[VarDecCount], "%s", string);
                                    VarDecCount++;
                                    tmpNode = doAssign ( ( char *) $2, ( struct node_t*) $4, 0);
                                    sprintf ( string, "%s%s", C ( $1), C ( tmpNode));
                                    $$ = ( int) getNode (string, S ( $1));
                                    HasParamInit = 1;
                                    free ( ( void *) $2);
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $4);
                                    freeNode ( tmpNode);
                                  }
        ;

variableDecList:
        variabledec SEPERATOR variableDecList
                                  { sprintf ( string, "%s%s", C ( $1), C ( $3));
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        |
                                  {
                                    string[0] = 0;
                                    $$ = ( int) getNode ( string, ' ');
                                  }
        ;

variableDecStart:
        VARTYPE
                                  {
                                    string[0] = 0;
                                    $$ = ( int) getNode ( string, ( char) $1);
                                  }
        | STORAGECLASS VARTYPE
                                  {
                                    string[0] = 0;
                                    $$ = ( int) getNode ( string, ( char) $2);
                                  }
        | variabledec COMMA
                                  {
                                    $$ = $1;
                                  }
        ;

variabledec:
        variableDecStart VARIABLE ASSIGN expression
                                  {
                                    struct node_t *tmpNode;
                                    switch ( ( ( struct node_t*) $1)->stack) {
                                      case 'f':
                                        sprintf ( string, "local\tfloat\t%s\t\t0\n", ( char*) $2);
                                        break;
                                      case 's':
                                        sprintf ( string, "local\tstring\t%s\t\t\"\"\n", ( char*) $2);
                                        break;
                                      case 'p':
                                        sprintf ( string, "local\tpoint\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'n':
                                        sprintf ( string, "local\tnormal\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'v':
                                        sprintf ( string, "local\tvector\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'c':
                                        sprintf ( string, "local\tcolor\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      default:
                                        error ( "Bad type");
                                    }
                                    setreference ( ( char *) $2, S ( $1));
                                    sprintf ( VarDecCode[VarDecCount], "%s", string);
                                    VarDecCount++;
                                    tmpNode = doAssign ( ( char *) $2, ( struct node_t*) $4, 0);
                                    sprintf ( string, "%s%s", C ( $1), C ( tmpNode));
                                    $$ = ( int) getNode ( string, S ( $1));
                                    free ( ( void *) $2);
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $4);
                                    freeNode ( tmpNode);
                                  }
        | variableDecStart VARIABLE
                                  {
                                    switch(((struct node_t*)$1)->stack) {
                                      case 'f':
                                        sprintf ( string, "local\tfloat\t%s\t\t0\n", ( char*) $2);
                                        break;
                                      case 's':
                                        sprintf ( string, "local\tstring\t%s\t\t\"\"\n", ( char*) $2);
                                        break;
                                      case 'p':
                                        sprintf ( string, "local\tpoint\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'n':
                                        sprintf ( string, "local\tnormal\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'v':
                                        sprintf ( string, "local\tvector\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      case 'c':
                                        sprintf ( string, "local\tcolor\t%s\t\t0 0 0\n", ( char*) $2);
                                        break;
                                      default:
                                        error ( "Bad type");
                                    }
                                    setreference ( ( char *) $2, S ( $1));
                                    sprintf ( VarDecCode[VarDecCount], "%s", string);
                                    VarDecCount++;
                                    $$ = $1;
                                    free ( ( void *) $2);
                                  }
        ;

shaderdec:
        shadertype VARIABLE LBRACKET paramDecList RBRACKET statement
                                  {
                                    int i;
                                    switch ( $1) {
                                      case 0:     // Light
                                        printf ( "light");
                                        break;
                                      case 1:     // Displacement
                                        printf ( "displacement");
                                        break;
                                      case 2:     // Surface
                                        printf ( "surface");
                                        break;
                                      case 3:     // Volume
                                        printf ( "volume");
                                        break;
                                      case 4:     // Imager
                                        printf ( "imager");
                                        break;
                                      case 5:     // Geometry
                                        printf ( "geometry");
                                        break;
                                      case 6:     // Transform
                                        printf ( "transformation");
                                        break;
                                      default:
                                        error ( "Unknown Shader type");
                                        break;
                                    }
                                    printf ( " %s\n", ( char*) $2);
                                    for ( i = 0; i < VarDecCount; i++) {
                                      printf ( "%s", VarDecCode[i]);
                                    }
                                    if ( HasParamInit) {
                                      printf ( C ( $4));
                                      printf ( "return\n\n");
                                    }
                                    printf ( C ( $6));
                                    printf ( "return\n");
                                    free ( ( void *) $2);
                                    freeNode ( ( struct node_t *) $4);
                                    freeNode ( ( struct node_t *) $6);
                                  }
        ;

shadertype:
        SHADERTYPE
                                  {
                                    switch ( $1) {
                                      case 0:     // Light
                                        setreference ( "P", 'p');
                                        setreference ( "dPdu", 'v');
                                        setreference ( "dPdv", 'v');
                                        setreference ( "N", 'n');
                                        setreference ( "Ng", 'n');
                                        setreference ( "Ps", 'p');
                                        //
                                        setreference ( "u", 'f');
                                        setreference ( "v", 'f');
                                        setreference ( "du", 'f');
                                        setreference ( "dv", 'f');
                                        //
                                        setreference ( "s", 'f');
                                        setreference ( "t", 'f');
                                        //
                                        setreference ( "L", 'v');
                                        setreference ( "Cl", 'c');
                                        break;
                                      case 1:     // Displacement
                                        setreference ( "P", 'p');
                                        setreference ( "dPdu", 'v');
                                        setreference ( "dPdv", 'v');
                                        setreference ( "N", 'n');
                                        setreference ( "Ng", 'n');
                                        //
                                        setreference ( "u", 'f');
                                        setreference ( "v", 'f');
                                        setreference ( "du", 'f');
                                        setreference ( "dv", 'f');
                                        //
                                        setreference ( "s", 'f');
                                        setreference ( "t", 'f');
                                        break;
                                      case 2:     // Surface
                                        setreference ( "Cs", 'c');
                                        setreference ( "Os", 'c');
                                        setreference ( "P", 'p');
                                        setreference ( "dPdu", 'v');
                                        setreference ( "dPdv", 'v');
                                        setreference ( "N", 'n');
                                        setreference ( "Ng", 'n');
                                        //
                                        setreference ( "u", 'f');
                                        setreference ( "v", 'f');
                                        setreference ( "du", 'f');
                                        setreference ( "dv", 'f');
                                        //
                                        setreference ( "s", 'f');
                                        setreference ( "t", 'f');
                                        //
                                        setreference ( "L", 'v');
                                        setreference ( "Cl", 'c');
                                        //
                                        setreference ( "I", 'v');
                                        //
                                        setreference ( "Ci", 'c');
                                        setreference ( "Oi", 'c');
                                        //
                                        setreference ( "E", 'v');
                                        break;
                                      case 3:     // Volume
                                        setreference ( "P", 'p');
                                        //
                                        setreference ( "I", 'v');
                                        //
                                        setreference ( "Ci", 'c');
                                        setreference ( "Oi", 'c');
                                        //
                                        setreference ( "E", 'v');
                                        break;
                                      case 4:     // Imager
                                        setreference ( "P", 'p');
                                        //
                                        setreference ( "Ci", 'c');
                                        setreference ( "Oi", 'c');
                                        setreference ( "alpha", 'f');
                                        break;
                                      case 5:     // Geometry
                                        setreference ( "P", 'p');
                                        setreference ( "N", 'n');
                                        setreference ( "u", 'f');
                                        setreference ( "v", 'f');
                                        setreference ( "du", 'f');
                                        setreference ( "dv", 'f');
                                        break;
                                      case 6:     // Transformation
                                        setreference ( "P", 'p');
                                        setreference ( "N", 'n');
                                        break;
                                      default:
                                        error ( "Unknown Shader type");
                                        break;
                                    }
                                    $$ = $1;
                                  }
        ;

statement:
        expression SEPERATOR
                                  {
                                    $$ = ( int) doDrop ( ( struct node_t *) $1);
                                  }
        | LBRACE variableDecList statementlist RBRACE
                                  {
                                    sprintf ( string, "%s\n%s\n", C ( $2), C ( $3));
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $2);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | IF LBRACKET expression RBRACKET statement
                                  {
                                    if ( S ( $3) != 'f') {
                                      error ( "Bad Conditional");
                                    }
                                    sprintf ( string, "%s%sifz %d\n\n%s\nlabel %d\n",
                                              debugString, C ( $3), labelno, C ( $5), labelno);
                                    labelno++;
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                  }
        | IF LBRACKET expression RBRACKET statement ELSE statement
                                  {
                                    if ( S ( $3) != 'f') {
                                      error("Bad Conditional");
                                    }
                                    sprintf ( string, "%s%sifz %d\n\n%sjump %d\n\nlabel %d\n%s\nlabel %d\n",
                                              debugString, C ( $3), labelno, C ( $5), labelno + 1, labelno,
                                              C ( $7), labelno + 1);
                                    labelno += 2;
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                    freeNode ( ( struct node_t *) $7);
                                  }
        | WHILE LBRACKET expression RBRACKET statement
                                  {
                                    if ( S ( $3) == 'f') {
                                      error ( "Bad Conditional");
                                    }
                                    sprintf ( string, "%slabel %d\n%sifz %d\n\n%sjump %d\n\nlabel %d\n",
                                              debugString, labelno, C ( $3), labelno + 1, C ( $5), labelno,
                                              labelno + 1);
                                    labelno += 2;
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                  }
        | FOR LBRACKET expression SEPERATOR expression SEPERATOR expression RBRACKET statement
                                  {
                                    struct node_t *init;
                                    struct node_t *incr;
                                    init = doDrop ( ( struct node_t *) $3);
                                    incr = doDrop ( ( struct node_t *) $7);
                                    // Replace any BREAKs with jumps to the end of loop...
                                    doBreak ( ( struct node_t *) $9, "_BREAK", labelno + 1);
                                    if ( S ( $5) != 'f') {
                                      error("Bad Conditional");
                                    }
                                    sprintf ( string, "%s%slabel %d\n%sifz %d\n\n%s%sjump %d\n\nlabel %d\n",
                                              debugString, C ( init), labelno, C ( $5), labelno + 1, C ( $9),
                                              C ( incr), labelno, labelno + 1);
                                    labelno += 2;
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( init);
                                    freeNode ( ( struct node_t *) $5);
                                    freeNode ( incr);
                                    freeNode ( ( struct node_t *) $9);
                                  }
        | SEPERATOR
                                  {
                                    $$ = ( int) getNode ( "", ' ');
                                  }
        | RETURN expression SEPERATOR
                                  {
                                    // This should do type checking!
                                    sprintf ( string, "%s%sjump _RETURN\n", debugString, C ( $2));
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $2);
                                  }
        | BREAK SEPERATOR
                                  {
                                    sprintf ( string, "%sjump _BREAK\n", debugString);
                                    $$ = ( int) getNode ( string, ' ');
                                  }
        | SOLAR LBRACKET expression COMMA expression RBRACKET statement
                                  {
                                    if ( S ( $3) != 'v' || S ( $5) != 'f') {
                                      error ( "Bad Solar");
                                    }
                                    sprintf ( string, "%s%s%ssolar2 %d\n%slabel %d\n",
                                              debugString, C ( $5), C ( $3), labelno,
                                              C ( $7), labelno);
                                    labelno += 1;
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                    freeNode ( ( struct node_t *) $7);
                                  }
        | ILLUMINATE LBRACKET expression RBRACKET statement
                                  {
                                    if ( S ( $3) != 'p') {
                                      error ( "Bad Illuminate");
                                    }
                                    sprintf ( string, "%s%silluminate1 %d\n%slabel %d\n",
                                              debugString, C ( $3), labelno, C ( $5), labelno);
                                    labelno += 1;
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                  }
        | ILLUMINATE LBRACKET expression COMMA expression COMMA expression RBRACKET statement
                                  {
                                    if ( S ( $3) != 'p' ||
                                         ( S ( $5) != 'v' && S ( $5) != 'n') ||
                                         S ( $7) != 'f') {
                                      error ( "Bad Illuminate");
                                    }
                                    sprintf ( string, "%s%s%s%silluminate3 %d\n%slabel %d\n",
                                              debugString, C ( $7), C ( $5), C ( $3), labelno,
                                              C ( $9), labelno);
                                    labelno += 1;
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                    freeNode ( ( struct node_t *) $7);
                                    freeNode ( ( struct node_t *) $9);
                                  }
        | ILLUMINANCE LBRACKET expression RBRACKET statement
                                  {
                                    if ( S ( $3) != 'p') {
                                      error ( "Bad Illuminance");
                                    }
                                    error ( "ILLUMINANCE not supported");
                                  }
        | ILLUMINANCE LBRACKET expression COMMA expression COMMA expression RBRACKET statement
                                  {
                                    if ( S ( $3) != 'p' ||
                                         ( S ( $5) != 'v' && S ( $5) != 'n') ||
                                         S ( $7) != 'f') {
                                      error ( "Bad Illuminance");
                                    }
                                    sprintf ( string, "%s%s%s%silluminance_start\nlabel %d\nilluminance3 %d\n%sjump %d\nlabel %d\n",
                                              debugString, C ( $7), C ( $5), C ( $3), labelno, labelno + 1, C ( $9),
                                              labelno, labelno + 1);
                                    labelno += 2;
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                    freeNode ( ( struct node_t *) $7);
                                    freeNode ( ( struct node_t *) $9);
                                  }
        | FRESNEL LBRACKET expression COMMA expression COMMA expression COMMA VARIABLE COMMA VARIABLE COMMA VARIABLE COMMA VARIABLE RBRACKET
                                  {
                                    sprintf ( string, "%s%s%sfresnel4\npopf %s\npopf %s\npopv %s\npopv %s\n",
                                              C ( $7), C ( $5), C ( $3), ( char *) $9, ( char *) $11,
                                              ( char *) $13, ( char *) $15);
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                    freeNode ( ( struct node_t *) $7);
                                    free ( ( char *) $9);
                                    free ( ( char *) $11);
                                    free ( ( char *) $13);
                                    free ( ( char *) $15);
                                  }
        ;

statementlist:
        statementlist statement
                                  {
                                    sprintf ( string, "%s\n%s", C ( $1), C ( $2));
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $2);
                                  }
        |
                                  {
                                    string[0]=0;
                                    $$ = ( int) getNode ( string, ' ');
                                  }
        ;

expression:
        VARIABLE
                                  {
                                    $$ = ( int) doPushVar ( ( char *) $1);
                                    free ( ( void *) $1);
                                  }
        | STRING
                                  {
                                    // Create a variabel to hold the string...
                                    char varName[10];
                                    sprintf ( varName, "$_str_%d", VarDecCount);
                                    setreference ( varName, 's');
                                    sprintf ( string, "local\tstring\t%s\t\t%s\n", varName, ( char*) $1);
                                    sprintf ( VarDecCode[VarDecCount], "%s", string);
                                    VarDecCount++;
                                    sprintf ( string, "\tpushs\t%s\n", varName);
                                    $$ = ( int) getNode ( string, 's');
                                    free ( ( void *) $1);
                                  }
        | VARTYPE LBRACKET expression COMMA expression COMMA expression RBRACKET
                                  {
                                    if ( ( ( char) $1) == 'f') {
                                      error ( "syntax error");
                                    }
                                    else {
                                      if ( ( S ( $3) == 'f') &&
                                           ( S ( $5) == 'f') &&
                                           ( S ( $7) == 'f')) {
                                        sprintf ( string, "%s%s%s", C ( $7), C ( $5), C ( $3));
                                        $$ = ( int) getNode ( string, ( char) $1);
                                      }
                                      else {
                                        error ( "bad type conversion");
                                      }
                                    }
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                    freeNode ( ( struct node_t *) $7);
                                  }
        | VARTYPE STRING LBRACKET expression COMMA expression COMMA expression RBRACKET
                                  {
                                    if ( ( ( char) $1) == 'f') {
                                      error ( "syntax error");
                                    }
                                    else {
                                      if ( S ( $4) == 'f' &&
                                           S ( $6) == 'f' &&
                                           S ( $8) == 'f') {
                                        if ( ( ( char) $1) == 'c') {
                                          if ( strcmp ( ( char *) $2, "\"hsv\"") == 0 ||
                                               strcmp ( ( char *) $2, "\"hsb\"") == 0) {
                                            sprintf ( string, "%s%s%shsv_to_rgb\n", C ( $8), C ( $6), C ( $4));
                                          }
                                          else if ( strcmp ( ( char *) $2, "\"rgb\"") == 0) {
                                            sprintf ( string, "%s%s%s", C ( $8), C ( $6), C ( $4));
                                          }
                                          else {
                                            sprintf ( string, "Unknown colorspace %s", ( char *) $2);
                                            error ( string);
                                          }
                                        }
                                        else {
                                          warning ( "Ignoring Space in Type Conversion");
                                          sprintf ( string, "%s%s%s", C ( $8), C ( $6), C ( $4));
                                        }
                                        $$ = ( int) getNode ( string, ( char) $1);
                                      }
                                      else {
                                        error ( "bad type conversion");
                                      }
                                    }
                                    free ( ( void *) $2);
                                    freeNode ( ( struct node_t *) $4);
                                    freeNode ( ( struct node_t *) $6);
                                    freeNode ( ( struct node_t *) $8);
                                  }
        | VARIABLE ASSIGN expression
                                  {
                                    $$ = ( int) doAssign ( ( char *) $1, ( struct node_t *) $3, 1);
                                    free ( ( void *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | VARIABLE ADDASSIGN expression
                                  {
                                    struct node_t *pushVar;
                                    struct node_t *sum;
                                    pushVar = doPushVar ( ( char *) $1);
                                    sum = doAdd ( pushVar, ( struct node_t *) $3);
                                    $$ = ( int) doAssign ( ( char *) $1, sum, 1);
                                    free ( ( void *) $1);
                                    freeNode ( pushVar);
                                    freeNode ( sum);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | VARIABLE SUBASSIGN expression
                                  {
                                    struct node_t *pushVar;
                                    struct node_t *sum;
                                    pushVar = doPushVar ( ( char *) $1);
                                    sum = doSub ( pushVar, ( struct node_t *) $3);
                                    $$ = ( int) doAssign ( ( char *) $1, sum, 1);
                                    free ( ( void *) $1);
                                    freeNode ( pushVar);
                                    freeNode ( sum);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | VARIABLE DIVASSIGN expression
                                  {
                                    struct node_t *pushVar;
                                    struct node_t *sum;
                                    pushVar = doPushVar ( ( char *) $1);
                                    sum = doDiv ( pushVar, ( struct node_t *) $3);
                                    $$ = ( int) doAssign ( ( char *) $1, sum, 1);
                                    free ( ( void *) $1);
                                    freeNode ( pushVar);
                                    freeNode ( sum);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | VARIABLE MULASSIGN expression
                                  {
                                    struct node_t *pushVar;
                                    struct node_t *sum;
                                    pushVar = doPushVar ( ( char *) $1);
                                    sum = doMul ( pushVar, ( struct node_t *) $3);
                                    $$ = ( int) doAssign ( ( char *) $1, sum, 1);
                                    free ( ( void *) $1);
                                    freeNode ( pushVar);
                                    freeNode ( sum);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | number
                                  {
                                    sprintf ( string, "\tpushif\t%s\n", C ( $1));
                                    $$ = ( int) getNode ( string, 'f');
                                    freeNode ( ( struct node_t *) $1);
                                  }
        | LBRACKET expression RBRACKET
                                  {
                                    $$ = $2;
                                  }
        | expression MUL expression
                                  {
                                    $$ = ( int) doMul ( ( struct node_t *) $1, ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression DIV expression
                                  {
                                    $$ = ( int) doDiv ( ( struct node_t *) $1, ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression ADD expression
                                  {
                                    $$ = ( int) doAdd ( ( struct node_t *) $1, ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression SUB expression
                                  {
                                    $$ = ( int) doSub ( ( struct node_t *) $1, ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | SUB expression
                                  {
                                    sprintf ( string, "%sneg%c\n", C ( $2), T ( $2));
                                    $$ = ( int) getNode ( string, S ( $2));
                                    freeNode ( ( struct node_t *) $2);
                                  }
        | expression EQUALS expression
                                  {
                                    if ( T ( $1) == 's' && T ( $3) == 's') {
                                      sprintf ( string, "%s%sseq\n", C ( $3), C ( $1));
                                    }
                                    else {
                                      if ( S ( $1) == S ( $3)) {
                                        sprintf ( string, "%s%s%ceq%c\n", C ( $3), C ( $1), T ( $1), T ( $3));
                                      }
                                      else {
                                        error ( "wrong compare types");
                                      }
                                    }
                                    $$ = ( int) getNode ( string, 'f');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression DOT expression
                                  {
                                    if ( ( S ( $1) == 'v' || S ( $1) == 'n' || S ( $1) == 'p') &&
                                         ( S ( $3) == 'v' || S ( $3) == 'n' || S ( $3) == 'p')) {
                                      sprintf ( string, "%s%svdot\n", C ( $3), C ( $1));
                                    }
                                    else {
                                      error ( "wrong dot product types");
                                    }
                                    $$ = ( int) getNode ( string, 'f');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression CROSS expression
                                  {
                                    if ( ( S ( $1) == 'v' || S ( $1) == 'n' || S ( $1) == 'p') &&
                                         ( S ( $3) == 'v' || S ( $3) == 'n' || S ( $3) == 'p')) {
                                      sprintf ( string, "%s%scross\n", C ( $3), C ( $1));
                                    }
                                    else {
                                      error ( "wrong cross product types");
                                    }
                                    $$ = ( int) getNode ( string, 'v');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression NEQ expression
                                  {
                                    if ( T ( $1) == 's' && T ( $3) == 's') {
                                      sprintf ( string, "%s%ssne\n", C ( $3), C ( $1));
                                    }
                                    else if ( S ( $1) == S ( $3)) {
                                      sprintf ( string, "%s%s%cne\n", C ( $1), C ( $3), T ( $3));
                                    }
                                    else {
                                      error ( "wrong compare types");
                                    }
                                    $$ = ( int) getNode ( string, 'f');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression FLOATCMP expression
                                  {
                                    if ( S ( $1) == 'f' &&
                                         S ( $3) == 'f') {
                                      char *op = "";
                                      if ( $2 == '<') {
                                        op = "lt";
                                      }
                                      if ( $2 == '>') {
                                        op = "gt";
                                      }
                                      if ( $2 == 'l') {
                                        op = "le";
                                      }
                                      if ( $2 == 'g') {
                                        op = "ge";
                                      }
                                      sprintf ( string, "%s%s%s\n", C ( $3), C ( $1), op);
                                    }
                                    else {
                                      error ( "wrong compare types");
                                    }
                                    $$ = ( int) getNode ( string, 'f');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression BOOLEANOP expression
                                  {
                                    if ( S ( $1) == 'f' && S ( $3) == 'f') {
                                      sprintf ( string, "%s%s%s\n", C ( $1), C ( $3),
                                                $2 == '&' ? "and": ( $2 == '|' ? "or" : "??BOOLEANOP???"));
                                    }
                                    else {
                                      error ( "wrong boolean types");
                                    }
                                    $$ = ( int) getNode ( string, 'f');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | expression QUERY expression COLON expression
                                  {
                                    if ( S ( $1) == 'f' && S ( $3) == S ( $5)) {
                                      sprintf ( string, "%sifz %d\n\n%sjump %d\n\nlabel %d\n%s\nlabel %d\n",
                                                C ( $1), labelno, C ( $3), labelno + 1, labelno, C ( $5), labelno + 1);
                                      labelno += 2;
                                    }
                                    else {
                                      error ( "wrong compare types");
                                    }
                                    $$ = ( int) getNode ( string, S ( $3));
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $5);
                                  }
        | VARIABLE LBRACKET RBRACKET
                                  {
                                    // Special case for functions with no parameters!
                                    struct list_t *l = getList ();
                                    $$ = ( int) handleCastFunction ( ' ', ( char *) $1, l);
                                    free ( ( void *) $1);
                                    freeList ( l);
                                  }
        | VARIABLE LBRACKET expressionList RBRACKET
                                  {
                                    $$ = ( int) handleCastFunction ( ' ', ( char *) $1, ( struct list_t*) $3);
                                    free ( ( void *) $1);
                                    freeList ( ( struct list_t *) $3);
                                  }
        | VARTYPE VARIABLE LBRACKET expressionList RBRACKET
                                  {
                                    $$ = ( int) handleCastFunction ( ( char) $1, ( char *) $2,
                                                                     ( struct list_t*) $4);
                                    free ( ( void *) $2);
                                    freeList ( ( struct list_t *) $4);
                                  }
        | VARTYPE VARIABLE LBRACKET RBRACKET
                                  {
                                    // Special case for functions with no parameters!
                                    struct list_t *l = getList ();
                                    $$ = ( int) handleCastFunction ( ( char) $1, ( char *) $2, l);
                                    free ( ( void *) $2);
                                    freeList ( l);
                                  }
        | TEXTURE LBRACKET expression channel RBRACKET
                                  {
                                    // Special case for functions with no parameters!
                                    struct list_t *l = getList ();
                                    $$ = ( int) handleTextureFunction ( ' ', ( char) $1, ( struct node_t *) $3,
                                                                        ( struct node_t *) $4, l);
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $4);
                                    freeList ( l);
                                  }
        | TEXTURE LBRACKET expression channel COMMA expressionList RBRACKET
                                  {
                                    $$ = ( int) handleTextureFunction ( ' ', ( char) $1, ( struct node_t *) $3,
                                                                        ( struct node_t *) $4, ( struct list_t *) $6);
                                    freeNode ( ( struct node_t *) $3);
                                    freeNode ( ( struct node_t *) $4);
                                    freeList ( ( struct list_t *) $6);
                                  }
        | VARTYPE TEXTURE LBRACKET expression channel RBRACKET
                                  {
                                    // Special case for functions with no parameters!
                                    struct list_t *l = getList ();
                                    $$ = ( int) handleTextureFunction ( ( char) $1, ( char) $2, ( struct node_t *) $4,
                                                                        ( struct node_t *) $5, l);
                                    freeNode ( ( struct node_t *) $4);
                                    freeNode ( ( struct node_t *) $5);
                                    freeList ( l);
                                  }
        | VARTYPE TEXTURE LBRACKET expression channel COMMA expressionList RBRACKET
                                  {
                                    $$ = ( int) handleTextureFunction ( ( char) $1, ( char) $2, ( struct node_t *) $4,
                                                                        ( struct node_t *) $5, ( struct list_t *) $7);
                                    freeNode ( ( struct node_t *) $4);
                                    freeNode ( ( struct node_t *) $5);
                                    freeList ( ( struct list_t *) $7);
                                  }
        ;

channel:
        CHANNEL
                                  {
                                    sprintf ( string, "\tpushif\t%c\n", ( char) $1);
                                    $$ = ( int) getNode ( string, 'f');
                                  }
        |
                                  {
                                    $$ = ( int) getNode ( "\tpushif\t0\n", 'f');
                                  }
        ;

expressionList:
        expressionList COMMA expression
                                  {
                                    appendToList ( ( struct node_t *) $3, ( struct list_t *) $1);
                                    $$ = $1;
                                  }
        | expression
                                  {
                                    struct list_t *l = getList ();
                                    appendToList ( ( struct node_t *) $1, l);
                                    $$ = ( int) l;
                                  }
        ;

functiondec:
        VARTYPE VARIABLE LBRACKET functionparamDecList RBRACKET statement
                                  {
                                    sprintf ( string, "Functions Not Supported:%s", ( char *) $2);
                                    error ( string);
                                    doBreak ( ( struct node_t *) $6, "_RETURN", labelno);
                                  }
        ;

functionparamDecList:
        functionparamDec SEPERATOR functionparamDecList
                                  {
                                    sprintf ( string, "%s%s", ( ( struct node_t *) $1)->code,
                                              ( ( struct node_t *) $3)->code);
                                    $$ = ( int) getNode ( string, ' ');
                                    freeNode ( ( struct node_t *) $1);
                                    freeNode ( ( struct node_t *) $3);
                                  }
        | functionparamDec
        |
                                  {
                                    string[0] = 0;
                                    $$ = ( int) getNode ( string, ' ');
                                  }
        ;

functionparamDecStart:
        VARTYPE
                                  {
                                    string[0] = 0;
                                    $$ = ( int) getNode ( string, ( char) $1);
                                  }
        | STORAGECLASS VARTYPE
                                  {
                                    string[0] = 0;
                                    $$ = ( int) getNode ( string, ( char) $2);
                                  }
        | functionparamDec COMMA
                                  {
                                    $$ = $1;
                                  }
        ;

functionparamDec:
        functionparamDecStart VARIABLE
                                  {
                                    string[0] = 0;
                                    $$ = (  int)  getNode ( string, S ( $1));
                                  }
        ;

number:
        DIGIT
                                  {
                                    sprintf ( string, "%s", ( char *) $1);
                                    $$ = ( int) getNode ( string, 'f');
                                    free ( ( void *) $1);
                                  }
        ;
