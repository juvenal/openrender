//
//  functions.h - {Summary}
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
//  $Id: functions.h,v 1.1 2002/10/18 20:19:44 juvenal Exp $
//

struct list_t {
  char *code[100];
  char stack[100];
  int count;
};

struct node_t {
  char *code;
  char stack;
};

struct list_t *getList();
void appendToList ( struct node_t *n, struct list_t *l);
void freeList ( struct list_t *l);

struct node_t *getNode ( char *cs, char ss);
void freeNode ( struct node_t *n);

struct node_t *handleFunction ( char *fName, struct list_t *params);
struct node_t *handleCastFunction ( char retType, char *fName,
                                    struct list_t* params);

struct node_t *handleTextureFunction ( char retType,
                                       char funcName,
                                       struct node_t *filename,
                                       struct node_t *channel,
                                       struct list_t*params);

#define C(X) (((struct node_t *) X)->code)
#define S(X) (((struct node_t *) X)->stack)

// This converts from the "NEW" SL types to the old "everything a vector"
// style used by BMRT.
// Maybe one day I'll leave this stuff in for runtime debungging purposes!

#define T(X) t(((struct node_t *) X)->stack)
extern char t(char n);
