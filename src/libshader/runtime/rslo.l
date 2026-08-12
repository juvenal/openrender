%option never-interactive
%option noyywrap
%option nounput noinput
%%
\#\!parameters										return SCRL_PARAMETERS;
\#\!variables										return SCRL_VARIABLES;
output												return SCRL_OUTPUT;
uniform												return SCRL_UNIFORM;
varying												return SCRL_VARYING;
float												return SCRL_FLOAT;
boolean												return SCRL_BOOLEAN;
color												return SCRL_COLOR;
vector												return SCRL_VECTOR;
normal												return SCRL_NORMAL;
point												return SCRL_POINT;
matrix												return SCRL_MATRIX;
string												return SCRL_STRING;

surface												return SCRL_SURFACE;
displacement										return SCRL_DISPLACEMENT;
imager												return SCRL_IMAGER;
light												return SCRL_LIGHTSOURCE;
volume												return SCRL_VOLUME;
generic												return SCRL_GENERIC;

DSO													return SCRL_DSO;

\#\!Init\:											return SCRL_INIT;
\#\!Code\:											return SCRL_CODE;

\.													return SCRL_DOT;
\:													return SCRL_COLON;
\=													return SCRL_EQUAL;
\(													return SCRL_OPEN_PARANTHESIS;
\)													return SCRL_CLOSE_PARANTHESIS;
\[													return SCRL_OPEN_SQR_PARANTHESIS;
\]													return SCRL_CLOSE_SQR_PARANTHESIS;
\,													return SCRL_COMMA;

\#\!version[ ]+[0-9]+\.[0-9]+\.[0-9]+\n				{	int	r,b,a;	sscanf(rslotext+9,"%d.%d.%d",&r,&b,&a);	if ((r != VERSION_MAJOR) || (b != VERSION_MINOR)) rsloerror("Version mismatch");	}
\#\!name[ ]+\"[^\"]*\"\n							{	const char *start = strchr(rslotext,'"') + 1; const char *end = strrchr(rslotext,'"'); size_t len = (size_t) (end - start); if (len >= sizeof(parsedShaderName)) len = sizeof(parsedShaderName) - 1; memcpy(parsedShaderName,start,len); parsedShaderName[len] = '\0'; }
\"[^\"]*\"											{   strcpy(rslolval.string,&rslotext[1]);	rslolval.string[strlen(rslolval.string)-1] = '\0'; osProcessEscapes(rslolval.string); return SCRL_TEXT_VALUE;}
[a-zA-Z\_][a-zA-Z0-9\_]*							{	strcpy(rslolval.string,rslotext); return SCRL_IDENTIFIER_VALUE; }
\#\![a-zA-Z][a-zA-Z0-9\_]*							{	strcpy(rslolval.string,rslotext); return SCRL_LABEL_VALUE; }

[+-]?([0-9]+|([0-9]*(\.[0-9]+)?)([eE][+-]?[0-9]+)?)	{	sscanf(rslotext,"%f",&rslolval.real); return SCRL_FLOAT_VALUE; }
[+-]?([0-9]+|([0-9]+(\.[0-9]*)?)([eE][+-]?[0-9]+)?) {	sscanf(rslotext,"%f",&rslolval.real); return SCRL_FLOAT_VALUE; }
[\t ]												;

\n													{	extern int rsloLineno; rsloLineno++; return SCRL_NL;	}
%%

