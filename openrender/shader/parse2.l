%{
//
//  parse2.l - {Summary}
//
//  Description:
//    {Description}
//
//  Creation:
//    Wed Oct 10 2002
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
//  $Id: parse2.l,v 1.1 2002/10/18 20:19:44 juvenal Exp $
//

// C includes
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Private includes
#include "parse3.h"

int  debug = 0,
     line  = 1,
     yylval;
char cppFilename[1024],
     debugString[1024];

%}
%%

"("             return(LBRACKET);
")"             return(RBRACKET);
"{"             return(LBRACE);
"}"             return(RBRACE);
";"             return(SEPERATOR);
","             return(COMMA);
"?"             return(QUERY);
":"             return(COLON);
"."             return(DOT);
"^"             return(CROSS);
"*"             return(MUL);
"/"             return(DIV);
"+"             return(ADD);
"-"             return(SUB);
"+="            return(ADDASSIGN);
"-="            return(SUBASSIGN);
"*="            return(MULASSIGN);
"/="            return(DIVASSIGN);
"="             return(ASSIGN);
"=="            return(EQUALS);
"!="            return(NEQ);
"&&"            {yylval='&';return(BOOLEANOP);};
"||"            {yylval='|';return(BOOLEANOP);};
"<"             {yylval='<';return(FLOATCMP);};
">"             {yylval='>';return(FLOATCMP);};
"<="            {yylval='l';return(FLOATCMP);};
">="            {yylval='g';return(FLOATCMP);};
"if"            return(IF);
"else"          return(ELSE);
"while"         return(WHILE);
"for"           return(FOR);
"break"         return(BREAK);
"continue"      return (CONTINUE);

float           {yylval='f';return(VARTYPE);};
point           {yylval='p';return(VARTYPE);};
normal          {yylval='n';return(VARTYPE);};
vector          {yylval='v';return(VARTYPE);};
color           {yylval='c';return(VARTYPE);};
string          {yylval='s';return(VARTYPE);};

uniform         {yylval='u';return(STORAGECLASS);};
varying         {yylval='v';return(STORAGECLASS);};

light           {yylval=0;return(SHADERTYPE);};
displacement    {yylval=1;return(SHADERTYPE);};
surface         {yylval=2;return(SHADERTYPE);};
volume          {yylval=3;return(SHADERTYPE);};
imager          {yylval=4;return(SHADERTYPE);};
geometry        {yylval=5;return(SHADERTYPE);};
transformation  {yylval=6;return(SHADERTYPE);};

solar           return(SOLAR);
illuminate      return(ILLUMINATE);
illuminance     return(ILLUMINANCE);
fresnel         return(FRESNEL);
return          return(RETURN);

texture         {yylval='t';return(TEXTURE);};
environment     {yylval='e';return(TEXTURE);};
bump            {yylval='b';return(TEXTURE);};
shadow          {yylval='s';return(TEXTURE);};

PI              {
                  yylval = ( int) malloc ( 100);
                  sprintf ( ( char*) yylval, "%f", M_PI);
                  return DIGIT;
                }

\[[0-9]+\]      {
                  yylval = *( yytext + 1);
                  return ( CHANNEL);
                }

\"[^\"]*\"      {
                  // printf ( "got %s\n", yytext);
                  yylval = ( int) malloc ( strlen ( yytext) + 1);
                  strcpy ( ( char*) yylval, ( char*) yytext);
                  return ( STRING) ;
                }

[0-9]*("."[0-9]*)?(e-?[0-9]*("."[0-9]*)?)?    {
                  // This is actually too lax, as it matches ""
                  // However it seems to work OK...
                  yylval = ( int) malloc ( strlen ( yytext) + 1);
                  strcpy ( ( char*) yylval, ( char*) yytext);
                  return ( DIGIT) ;
                }

"\n"            {
                  line++;
                  if ( debug) {
                    sprintf ( debugString, "# %s, line %d\n#########\n", cppFilename, line);
                  }
                  else {
                    debugString[0] = 0;
                  }
                }

[ \t]           NULL;

^#.*$           {
                  sscanf ( yytext, "# %d %s\n", &line, cppFilename);
                  if ( debug) {
                    sprintf ( debugString, "# %d %s\n", line, cppFilename);
                  }
                  else {
                    debugString[0] = 0;
                  }
                  //printf ( "Now At:%s:%d\n", cppFilename, line);
                  line--;
                }

[a-zA-Z][a-zA-Z0-9_]*   {
                  yylval = ( int) malloc ( strlen ( yytext) + 1);
                  strcpy ( ( char*) yylval, ( char*) yytext);
                  return ( VARIABLE) ;
                }
