%{
   #include <stdlib.h>

   // Included into variable_generated.cpp (variable.y), the only user of
   // this scanner: everything it needs is the generated var* interface.
%}
%option never-interactive
%option noyywrap
%option nounput noinput
%%
global												return	VAR_GLOBAL;
constant											return	VAR_CONSTANT;
uniform												return	VAR_UNIFORM;
varying												return	VAR_VARYING;
facevarying											return	VAR_FACEVARYING;
vertex												return	VAR_VERTEX;
float												return	VAR_FLOAT;
integer												return	VAR_INTEGER;
int													return	VAR_INTEGER;
vector												return	VAR_VECTOR;
color												return	VAR_COLOR;
normal												return	VAR_NORMAL;
point												return	VAR_POINT;
matrix												return	VAR_MATRIX;
mpoint												return	VAR_MPOINT;
htpoint												return	VAR_HTPOINT;
hpoint												return	VAR_HTPOINT;
double												return	VAR_DOUBLE;
string												return	VAR_STRING;
\[													return	VAR_OPEN;
\]													return	VAR_CLOSE;
[a-zA-Z\_][a-zA-Z0-9\_]*							{	assert(strlen(yytext) < (VARIABLE_NAME_LENGTH-1));	strcpy(currentVariable->name,yytext); return VAR_IDENTIFIER; }
([0-9]+|([0-9]*(\.[0-9]+)?)([eE][+-]?[0-9]+)?)		{	sscanf(yytext,"%f",&yylval.real); return VAR_FLOAT; }
[\t ]												;
%%

