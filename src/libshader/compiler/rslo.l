%option never-interactive
%option noyywrap
%option nounistd
%option noinput
%option nounput
%option warn
%%
surface											return SL_SURFACE;
displacement									return SL_DISPLACEMENT;
light											return SL_LIGHT;
volume											return SL_VOLUME;
transformation									return SL_TRANSFORMATION;
imager											return SL_IMAGER;

float											return SL_FLOAT;
color											return SL_COLOR;
point											return SL_POINT;
vector											return SL_VECTOR;
normal											return SL_NORMAL;
matrix											return SL_MATRIX;
string											return SL_STRING;

void											return SL_VOID;
output											return SL_OUTPUT;
for												return SL_FOR;
while											return SL_WHILE;
if												return SL_IF;
gather											return SL_GATHER;
else											return SL_ELSE;
break											return SL_BREAK;
uniform											return SL_UNIFORM;
varying											return SL_VARIABLE;
continue										return SL_CONTINUE;
illuminance										return SL_ILLUMINANCE;
illuminate										return SL_ILLUMINATE;
solar											return SL_SOLAR;
return											return SL_RETURN;
extern											return SL_EXTERN;

texture											return SL_TEXTURE;
shadow											return SL_SHADOW;
environment										return SL_ENVIRONMENT;
bump											return SL_BUMP;

PI												return SL_FCN_PI;

\"[^\"]*\"										{	char	*token	=	strdup(rslotext);	rslolval.string = token;	    rslo->allocatedStrings->push(token); return SL_TEXT_VALUE;	}
[a-zA-Z\_][a-zA-Z0-9\_]*						{	char	*token	=	strdup(rslotext);	rslolval.string = token;	    rslo->allocatedStrings->push(token); return SL_IDENTIFIER_VALUE;			}
(([0-9]*\.[0-9]+)([eE][+-]?[0-9]+)?)f?			{	char	*token	=	strdup(rslotext);	rslolval.string = token;	    rslo->allocatedStrings->push(token); return SL_FLOAT_VALUE;				}
(([0-9]+\.[0-9]*)([eE][+-]?[0-9]+)?)f?			{	char	*token	=	strdup(rslotext);	rslolval.string = token;	    rslo->allocatedStrings->push(token); return SL_FLOAT_VALUE;				}
(([0-9]+)([eE][+-]?[0-9]+)?)f?					{	char	*token	=	strdup(rslotext);	rslolval.string = token;	    rslo->allocatedStrings->push(token); return SL_FLOAT_VALUE;				}


\,												return SL_COMMA;
\:												return SL_COLON;
\?												return SL_QUESTION;
\;												return SL_SEMI_COLON;

\+\+											return SL_INCREMENT;
\-\-											return SL_DECREMENT;
\+\=											return SL_INCREMENT_BY;
\-\=											return SL_DECREMENT_BY;
\*\=											return SL_MULTIPLY_BY;
\/\=											return SL_DIVIDE_BY;

\<\=											return SL_COMP_LESS_EQUAL;
\>\=											return SL_COMP_GREATER_EQUAL;
\<												return SL_COMP_LESS;
\>												return SL_COMP_GREATER;
\=\=											return SL_COMP_EQUAL;
\!\=											return SL_COMP_DIFFERENT;
\&\&											return SL_AND;
\|\|											return SL_OR;
\!												return SL_NOT;
\+												return SL_PLUS;
\-												return SL_MINUS;
\=												return SL_EQUAL;
\*												return SL_MULTIPLY;
\/												return SL_DIVIDE;
\^												return SL_CROSS;
\.												return SL_DOT;

\(												return SL_OPEN_PARANTHESIS;
\)												return SL_CLOSE_PARANTHESIS;
\{												return SL_OPEN_CRL_PARANTHESIS;
\}												return SL_CLOSE_CRL_PARANTHESIS;
\[												return SL_OPEN_SQR_PARANTHESIS;
\]												return SL_CLOSE_SQR_PARANTHESIS;

^\#line[ ][0-9]+[ ]\"[^\"]*\"					{ char tmp[OS_MAX_PATH_LENGTH]; sscanf(rslotext+6,"%d \"%s\"",&rslo->lineNo,tmp); rslo->lineNo--; tmp[strlen(tmp)-1] = '\0';  if (rslo->sourceFile != nullptr) free(rslo->sourceFile); rslo->sourceFile = strdup(tmp);}
^\#line[ ][0-9]+[^\n]*							{ sscanf(rslotext+6,"%d",&rslo->lineNo); rslo->lineNo--; }
^\#pragma[ ]nolint[^\n]*						;
^\#[^\n]*										{ rslo->warning("Skipping unknown preprocessor directive \"%s\"\n",rslotext); }
[ \r\t]+										;

\n												{ rslo->lineNo++; }
.												{ rslo->error("Stray '%c' in shader\n",rslotext[0]); }
%%

