%{
/**
 * Project: openRender
 *
 * File: rslo.y
 *
 * Description:
 *   This is the parser file for CShader.
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
//  File				:	rslo.y
//  Classes				:	-
//  Description			:	This is the parser file for CShader
//
////////////////////////////////////////////////////////////////////////
#undef alloca
#include <math.h>
#include <string.h>

#include "common/algebra.h"
#include "common/os.h"
#include "rslo.h"

/////////////////////////////////////////////////////////////////////////////////////
//   First some temporary data structures used during the script parsing


// Some forward definitions
		void							rsloerror(const char *);		// Forward definition for stupid yacc
		int								rslolex(void );				// Forward definition for stupid yacc




		TRSLObjectParameter	*parameters;
		TRSLObjectParameter	*currentParameter;
		UDefaultVal		*currentDefaultItem;
		int				numArrayItemsRemaining;
		ERSLObjectShaderType	shaderType;
		char			parsedShaderName[512];	// Populated by the "#!name" pragma, if present

%}
%union rsloval {
	float			real;
	char			string[64];
	matrix			m;
	vector			v;
	TRSLObjectParameter *parameter;
}

// Some tokens
%token	 SCRL_PARAMETERS
%token	 SCRL_VARIABLES
%token   SCRL_INIT
%token   SCRL_CODE
%token	 SCRL_OUTPUT
%token	 SCRL_VARYING
%token	 SCRL_UNIFORM
%token	 SCRL_BOOLEAN
%token	 SCRL_FLOAT
%token	 SCRL_COLOR
%token	 SCRL_VECTOR
%token	 SCRL_NORMAL
%token	 SCRL_POINT
%token	 SCRL_MATRIX
%token	 SCRL_STRING

%token	 SCRL_SURFACE
%token	 SCRL_DISPLACEMENT
%token	 SCRL_IMAGER
%token	 SCRL_LIGHTSOURCE
%token	 SCRL_VOLUME
%token	 SCRL_GENERIC

%token	 SCRL_DSO

%token	 SCRL_DOT
%token	 SCRL_COLON
%token	 SCRL_EQUAL
%token	 SCRL_OPEN_PARANTHESIS
%token	 SCRL_CLOSE_PARANTHESIS
%token	 SCRL_OPEN_SQR_PARANTHESIS
%token	 SCRL_CLOSE_SQR_PARANTHESIS
%token	 SCRL_COMMA
%token   SCRL_NL

%token<string>	SCRL_TEXT_VALUE
%token<string>	SCRL_IDENTIFIER_VALUE
%token<string>	SCRL_LABEL_VALUE
%token<real>	SCRL_FLOAT_VALUE
%type<string>	rsloGlobalParameterContainer
%type<v>		rsloVectorIn
%type<v>		rsloVector
%%
start:		
			rsloType	
			rsloParameterDefinitions
			rsloVariableDefinitions
			SCRL_INIT
			SCRL_NL
			rsloCode
			SCRL_CODE
			SCRL_NL
			rsloCode
			rsloEmptySpace
			;

rsloEmptySpace: 
			|
			SCRL_NL
			rsloEmptySpace
			;
			
rsloVectorIn:	SCRL_TEXT_VALUE
			SCRL_FLOAT_VALUE
			{
				currentParameter->space			=	strdup($1);
				currentDefaultItem->vector		=	new float[3];
				currentDefaultItem->vector[0]	=	$2;
				currentDefaultItem->vector[1]	=	$2;
				currentDefaultItem->vector[2]	=	$2;
			}
			|
			SCRL_TEXT_VALUE
			SCRL_OPEN_SQR_PARANTHESIS
			SCRL_FLOAT_VALUE
			SCRL_FLOAT_VALUE
			SCRL_FLOAT_VALUE
			SCRL_CLOSE_SQR_PARANTHESIS
			{
				currentParameter->space			=	strdup($1);
				currentDefaultItem->vector		=	new float[3];
				currentDefaultItem->vector[0]	=	$3;
				currentDefaultItem->vector[1]	=	$4;
				currentDefaultItem->vector[2]	=	$5;
			}
			|
			SCRL_FLOAT_VALUE
			{
				currentDefaultItem->vector		=	new float[3];
				currentDefaultItem->vector[0]	=	$1;
				currentDefaultItem->vector[1]	=	$1;
				currentDefaultItem->vector[2]	=	$1;
			}
			|
			SCRL_OPEN_SQR_PARANTHESIS
			SCRL_FLOAT_VALUE
			SCRL_FLOAT_VALUE
			SCRL_FLOAT_VALUE
			SCRL_CLOSE_SQR_PARANTHESIS
			{
				currentDefaultItem->vector		=	new float[3];
				currentDefaultItem->vector[0]	=	$2;
				currentDefaultItem->vector[1]	=	$3;
				currentDefaultItem->vector[2]	=	$4;
			}
			;

rsloVector:	rsloVectorIn
			{
			}
			;

rsloVectorInit:	SCRL_EQUAL
				rsloVector
			|
			{
					currentParameter->defaultValue.vector		=	new float[3];
					currentParameter->defaultValue.vector[0]	=	0;
					currentParameter->defaultValue.vector[1]	=	0;
					currentParameter->defaultValue.vector[2]	=	0;
			}
			;
			
rsloContainer:	SCRL_UNIFORM
				{
				}
				|
				SCRL_VARYING
				{
				}
				|
				{
				}
				;

rsloType:
		SCRL_SURFACE
		SCRL_NL
		{
			shaderType	=	SHADER_SURFACE;
		}
	|
		SCRL_DISPLACEMENT
		SCRL_NL
		{
			shaderType	=	SHADER_DISPLACEMENT;
		}
	|
		SCRL_LIGHTSOURCE
		SCRL_NL
		{
			shaderType	=	SHADER_LIGHT;
		}
	|
		SCRL_VOLUME
		SCRL_NL
		{
			shaderType	=	SHADER_VOLUME;
		}
	|
		SCRL_IMAGER
		SCRL_NL
		{
			shaderType	=	SHADER_IMAGER;
		}
		;

rsloParameterDefinitions:
		SCRL_PARAMETERS
		SCRL_COLON
		SCRL_NL
		rsloParameters
		;

rsloParameters:
		rsloParameters
		rsloParameter
		SCRL_NL
		{
		}
	|
		{
		}
		;

rsloParameter:
		{
			currentParameter						=	new TRSLObjectParameter;
			currentParameter->space					=	NULL;
			currentParameter->numItems				=	1;
			currentParameter->defaultValue.string	=	NULL;
		}
		rsloGlobalParameterContainer
		rsloRegularParameter
		{
			currentParameter->next	=	parameters;
			parameters				=	currentParameter;
		}
	|
		{
			currentParameter						=	new TRSLObjectParameter;
			currentParameter->space					=	NULL;
			currentParameter->numItems				=	1;
			currentParameter->defaultValue.string	=	NULL;
			currentParameter->container				=	CONTAINER_UNIFORM;
		}
		rsloRegularParameter
		{
			currentParameter->next	=	parameters;
			parameters				=	currentParameter;
		}
		;

rsloRegularParameter:
		rsloFloatParameter
	|
		rsloStringParameter
	|
		rsloColorParameter
	|
		rsloVectorParameter
	|
		rsloNormalParameter
	|
		rsloPointParameter
	|
		rsloMatrixParameter
		;

//GSHTODO: This list is missing constant and facevarying!!
rsloGlobalParameterContainer:
		SCRL_UNIFORM
		{
			currentParameter->container	=	CONTAINER_UNIFORM;
			currentParameter->writable	=	FALSE;
		}
		|
		SCRL_VARYING
		{
			currentParameter->container	=	CONTAINER_VARYING;
			currentParameter->writable	=	FALSE;
		}
		|
		SCRL_OUTPUT
		SCRL_UNIFORM
		{
			currentParameter->container	=	CONTAINER_UNIFORM;
			currentParameter->writable	=	TRUE;
		}
		|
		SCRL_OUTPUT
		SCRL_VARYING
		{
			currentParameter->container	=	CONTAINER_VARYING;
			currentParameter->writable	=	TRUE;
		}
		|
		SCRL_OUTPUT
		{
			currentParameter->container	=	CONTAINER_UNIFORM;
			currentParameter->writable	=	TRUE;
		}
		;

rsloFloatParameter:
		SCRL_FLOAT
		SCRL_IDENTIFIER_VALUE
		SCRL_EQUAL
		SCRL_FLOAT_VALUE
		{
			currentParameter->type					=	TYPE_FLOAT;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.scalar	=	$4;
			currentParameter->numItems				=	1;
		}
	|
		SCRL_FLOAT
		SCRL_IDENTIFIER_VALUE
		{
			currentParameter->type					=	TYPE_FLOAT;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.scalar	=	0;
			currentParameter->numItems				=	1;
		}
	|
		SCRL_FLOAT
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		SCRL_EQUAL
		{
			currentParameter->type					=	TYPE_FLOAT;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			numArrayItemsRemaining = currentParameter->numItems;
		}
		rsloFloatArrayInitializer
	|
		SCRL_FLOAT
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			currentParameter->type					=	TYPE_FLOAT;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			for(int i=0;i<currentParameter->numItems;i++)
				currentDefaultItem[i].scalar = 0;
		}
		;

rsloFloatArrayInitializer:
		SCRL_OPEN_SQR_PARANTHESIS
		rsloFloatArrayInitializerItems
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			if(numArrayItemsRemaining){
				rsloerror("Wrong number of items in array initializer\n");
			}
		}
		;
		
rsloFloatArrayInitializerItems:
		rsloFloatArrayInitializerItems
		SCRL_FLOAT_VALUE
		{
			if(numArrayItemsRemaining){
				currentDefaultItem->scalar = $2;
				currentDefaultItem++;
				numArrayItemsRemaining--;
			}
			else{
				rsloerror("Wrong number of items in array initializer\n");
			}
		}	
	|
		;

rsloStringParameter:
		SCRL_STRING
		SCRL_IDENTIFIER_VALUE
		{
			currentParameter->type					=	TYPE_STRING;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	NULL;
			currentParameter->numItems				=	1;
		}
	|
		SCRL_STRING
		SCRL_IDENTIFIER_VALUE
		SCRL_EQUAL
		SCRL_TEXT_VALUE
		{
			currentParameter->type					=	TYPE_STRING;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.string	=	strdup($4);
			currentParameter->numItems				=	1;
		}
	|
		SCRL_STRING
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		SCRL_EQUAL
		{
			currentParameter->type					=	TYPE_STRING;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			numArrayItemsRemaining = currentParameter->numItems;
		}
		rsloStringArrayInitializer
	|
		SCRL_STRING
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			currentParameter->type					=	TYPE_STRING;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			for(int i=0;i<currentParameter->numItems;i++)
				currentDefaultItem[i].string = NULL;
		}
		;

rsloStringArrayInitializer:
		SCRL_OPEN_SQR_PARANTHESIS
		rsloStringArrayInitializerItems
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			if(numArrayItemsRemaining){
				rsloerror("Wrong number of items in array initializer\n");
			}
		}
		;
		
rsloStringArrayInitializerItems:
		rsloStringArrayInitializerItems
		SCRL_TEXT_VALUE
		{
			if(numArrayItemsRemaining){
				currentDefaultItem->string = strdup($2);
				currentDefaultItem++;
				numArrayItemsRemaining--;
			}
			else{
				rsloerror("Wrong number of items in array initializer\n");
			}
		}	
	|
		;

rsloColorParameter:
		SCRL_COLOR
		SCRL_IDENTIFIER_VALUE
		{
			currentParameter->type					=	TYPE_COLOR;
			currentParameter->name					=	strdup($2);
			currentParameter->numItems				=	1;
			
			currentDefaultItem = &currentParameter->defaultValue;
		}
		rsloVectorInit
		{
		}
		|
		SCRL_COLOR
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		SCRL_EQUAL
		{
			currentParameter->type					=	TYPE_COLOR;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			numArrayItemsRemaining = currentParameter->numItems;
		}
		rsloVectorArrayInitializer
		|
		SCRL_COLOR
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			currentParameter->type					=	TYPE_COLOR;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			for(int i=0;i<currentParameter->numItems;i++){
				currentDefaultItem[i].vector = new float[3];
				currentDefaultItem[i].vector[0] = 0;
				currentDefaultItem[i].vector[1] = 0;
				currentDefaultItem[i].vector[2] = 0;
			}
		}
		;

rsloVectorParameter:
		SCRL_VECTOR
		SCRL_IDENTIFIER_VALUE
		{
			currentParameter->type					=	TYPE_VECTOR;
			currentParameter->name					=	strdup($2);
			currentParameter->numItems				=	1;
			
			currentDefaultItem = &currentParameter->defaultValue;
		}
		rsloVectorInit
		{
		}
		|
		SCRL_VECTOR
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		SCRL_EQUAL
		{
			currentParameter->type					=	TYPE_VECTOR;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			numArrayItemsRemaining = currentParameter->numItems;
		}
		rsloVectorArrayInitializer
		|
		SCRL_VECTOR
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			currentParameter->type					=	TYPE_VECTOR;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			for(int i=0;i<currentParameter->numItems;i++){
				currentDefaultItem[i].vector = new float[3];
				currentDefaultItem[i].vector[0] = 0;
				currentDefaultItem[i].vector[1] = 0;
				currentDefaultItem[i].vector[2] = 0;
			}
		}
		;

rsloVectorArrayInitializer:
		SCRL_OPEN_SQR_PARANTHESIS
		rsloVectorArrayInitializerItems
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			if(numArrayItemsRemaining){
				rsloerror("Wrong number of items in array initializer\n");
		}
		}
		;
		
rsloVectorArrayInitializerItems:
		rsloVectorArrayInitializerItems
		{
			if(numArrayItemsRemaining){
				currentDefaultItem->vector = new float[3];
			}
			else{
				rsloerror("Wrong number of items in array initializer\n");
			}
		}
		rsloVector
		{
			currentDefaultItem++;
			numArrayItemsRemaining--;
		}
	|
		;

rsloNormalParameter:
		SCRL_NORMAL
		SCRL_IDENTIFIER_VALUE
		{
			currentParameter->type					=	TYPE_NORMAL;
			currentParameter->name					=	strdup($2);
			currentParameter->numItems				=	1;
			
			currentDefaultItem = &currentParameter->defaultValue;
		}
		rsloVectorInit
		{
		}
		|
		SCRL_NORMAL
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		SCRL_EQUAL
		{
			currentParameter->type					=	TYPE_NORMAL;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			numArrayItemsRemaining = currentParameter->numItems;
		}
		rsloVectorArrayInitializer
		|
		SCRL_NORMAL
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			currentParameter->type					=	TYPE_NORMAL;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			for(int i=0;i<currentParameter->numItems;i++){
				currentDefaultItem[i].vector = new float[3];
				currentDefaultItem[i].vector[0] = 0;
				currentDefaultItem[i].vector[1] = 0;
				currentDefaultItem[i].vector[3] = 0;
			}
		}
		;

rsloPointParameter:
		SCRL_POINT
		SCRL_IDENTIFIER_VALUE
		{
			currentParameter->type					=	TYPE_POINT;
			currentParameter->name					=	strdup($2);
			currentParameter->numItems				=	1;
			
			currentDefaultItem = &currentParameter->defaultValue;
		}
		rsloVectorInit
		{
		}
		|
		SCRL_POINT
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		SCRL_EQUAL
		{
			currentParameter->type					=	TYPE_POINT;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			numArrayItemsRemaining = currentParameter->numItems;
		}
		rsloVectorArrayInitializer
		|
		SCRL_POINT
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			currentParameter->type					=	TYPE_POINT;
			currentParameter->name					=	strdup($2);
			currentParameter->defaultValue.array	=	new UDefaultVal[(int) $4];
			currentParameter->numItems				=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			for(int i=0;i<currentParameter->numItems;i++){
				currentDefaultItem[i].vector	= new float[3];
				currentDefaultItem[i].vector[0] = 0;
				currentDefaultItem[i].vector[1] = 0;
				currentDefaultItem[i].vector[2] = 0;
			}
		}
		;

rsloMatrixParameter:
		SCRL_MATRIX
		SCRL_IDENTIFIER_VALUE
		SCRL_EQUAL
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			currentParameter->container					=	CONTAINER_UNIFORM;
			currentParameter->type						=	TYPE_MATRIX;
			currentParameter->name						=	strdup($2);
			currentParameter->defaultValue.matrix		=	new float[16];
			currentParameter->defaultValue.matrix[0]	=	$5;
			currentParameter->defaultValue.matrix[1]	=	$6;
			currentParameter->defaultValue.matrix[2]	=	$7;
			currentParameter->defaultValue.matrix[3]	=	$8;
			currentParameter->defaultValue.matrix[4]	=	$9;
			currentParameter->defaultValue.matrix[5]	=	$10;
			currentParameter->defaultValue.matrix[6]	=	$11;
			currentParameter->defaultValue.matrix[7]	=	$12;
			currentParameter->defaultValue.matrix[8]	=	$13;
			currentParameter->defaultValue.matrix[9]	=	$14;
			currentParameter->defaultValue.matrix[10]	=	$15;
			currentParameter->defaultValue.matrix[11]	=	$16;
			currentParameter->defaultValue.matrix[12]	=	$17;
			currentParameter->defaultValue.matrix[13]	=	$18;
			currentParameter->defaultValue.matrix[14]	=	$19;
			currentParameter->defaultValue.matrix[15]	=	$20;
			currentParameter->numItems					=	1;
		}
	|
		SCRL_MATRIX
		SCRL_IDENTIFIER_VALUE
		SCRL_EQUAL
		SCRL_FLOAT_VALUE
		{
			currentParameter->container					=	CONTAINER_UNIFORM;
			currentParameter->type						=	TYPE_MATRIX;
			currentParameter->name						=	strdup($2);
			currentParameter->defaultValue.matrix		=	new float[16];
			currentParameter->defaultValue.matrix[0]	=	$4;
			currentParameter->defaultValue.matrix[1]	=	0;
			currentParameter->defaultValue.matrix[2]	=	0;
			currentParameter->defaultValue.matrix[3]	=	0;
			currentParameter->defaultValue.matrix[4]	=	0;
			currentParameter->defaultValue.matrix[5]	=	$4;
			currentParameter->defaultValue.matrix[6]	=	0;
			currentParameter->defaultValue.matrix[7]	=	0;
			currentParameter->defaultValue.matrix[8]	=	0;
			currentParameter->defaultValue.matrix[9]	=	0;
			currentParameter->defaultValue.matrix[10]	=	$4;
			currentParameter->defaultValue.matrix[11]	=	0;
			currentParameter->defaultValue.matrix[12]	=	0;
			currentParameter->defaultValue.matrix[13]	=	0;
			currentParameter->defaultValue.matrix[14]	=	0;
			currentParameter->defaultValue.matrix[15]	=	$4;
			currentParameter->numItems					=	1;
		}
	|
		SCRL_MATRIX
		SCRL_IDENTIFIER_VALUE
		{
			currentParameter->container					=	CONTAINER_UNIFORM;
			currentParameter->type						=	TYPE_MATRIX;
			currentParameter->name						=	strdup($2);
			currentParameter->defaultValue.matrix		=	new float[16];
			currentParameter->defaultValue.matrix[0]	=	1;
			currentParameter->defaultValue.matrix[1]	=	0;
			currentParameter->defaultValue.matrix[2]	=	0;
			currentParameter->defaultValue.matrix[3]	=	0;
			currentParameter->defaultValue.matrix[4]	=	0;
			currentParameter->defaultValue.matrix[5]	=	1;
			currentParameter->defaultValue.matrix[6]	=	0;
			currentParameter->defaultValue.matrix[7]	=	0;
			currentParameter->defaultValue.matrix[8]	=	0;
			currentParameter->defaultValue.matrix[9]	=	0;
			currentParameter->defaultValue.matrix[10]	=	1;
			currentParameter->defaultValue.matrix[11]	=	0;
			currentParameter->defaultValue.matrix[12]	=	0;
			currentParameter->defaultValue.matrix[13]	=	0;
			currentParameter->defaultValue.matrix[14]	=	0;
			currentParameter->defaultValue.matrix[15]	=	1;
			currentParameter->numItems					=	1;
		}
	|
		SCRL_MATRIX
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		SCRL_EQUAL
		{
			currentParameter->container					=	CONTAINER_UNIFORM;
			currentParameter->type						=	TYPE_MATRIX;
			currentParameter->name						=	strdup($2);
			currentParameter->defaultValue.array		=	new UDefaultVal[(int) $4];
			currentParameter->numItems					=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			numArrayItemsRemaining = currentParameter->numItems;
		}
		rsloMatrixArrayInitializer
	|
		SCRL_MATRIX
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			currentParameter->container					=	CONTAINER_UNIFORM;
			currentParameter->type						=	TYPE_MATRIX;
			currentParameter->name						=	strdup($2);
			currentParameter->defaultValue.array		=	new UDefaultVal[(int) $4];
			currentParameter->numItems					=	(int) $4;
			
			currentDefaultItem = currentParameter->defaultValue.array;
			for(int i=0;i<currentParameter->numItems;i++){
				currentDefaultItem[i].matrix = new float[16];
				currentDefaultItem[i].matrix[0] = 1;
				currentDefaultItem[i].matrix[1] = 0;
				currentDefaultItem[i].matrix[2] = 0;
				currentDefaultItem[i].matrix[3] = 0;
				currentDefaultItem[i].matrix[4] = 0;
				currentDefaultItem[i].matrix[5] = 1;
				currentDefaultItem[i].matrix[6] = 0;
				currentDefaultItem[i].matrix[7] = 0;
				currentDefaultItem[i].matrix[8] = 0;
				currentDefaultItem[i].matrix[9] = 0;
				currentDefaultItem[i].matrix[10] = 1;
				currentDefaultItem[i].matrix[11] = 0;
				currentDefaultItem[i].matrix[12] = 0;
				currentDefaultItem[i].matrix[13] = 0;
				currentDefaultItem[i].matrix[14] = 0;
				currentDefaultItem[i].matrix[15] = 1;
			}
		}
		;

rsloMatrixArrayInitializer:
		SCRL_OPEN_SQR_PARANTHESIS
		rsloMatrixArrayInitializerItems
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			if(numArrayItemsRemaining){
				rsloerror("Wrong number of items in array initializer\n");
			}
		}
		;
		
rsloMatrixArrayInitializerItems:
		rsloMatrixArrayInitializerItems
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
			if(numArrayItemsRemaining){
				currentDefaultItem->matrix = new float[16];
				currentDefaultItem->matrix[0] = $3;
				currentDefaultItem->matrix[1] = $4;
				currentDefaultItem->matrix[2] = $5;
				currentDefaultItem->matrix[3] = $6;
				currentDefaultItem->matrix[4] = $7;
				currentDefaultItem->matrix[5] = $8;
				currentDefaultItem->matrix[6] = $9;
				currentDefaultItem->matrix[7] = $10;
				currentDefaultItem->matrix[8] = $11;
				currentDefaultItem->matrix[9] = $12;
				currentDefaultItem->matrix[10] = $13;
				currentDefaultItem->matrix[11] = $14;
				currentDefaultItem->matrix[12] = $15;
				currentDefaultItem->matrix[13] = $16;
				currentDefaultItem->matrix[14] = $17;
				currentDefaultItem->matrix[15] = $18;
				
				currentDefaultItem++;
				numArrayItemsRemaining--;
			}
			else{
				rsloerror("Wrong number of items in array initializer\n");
			}
		}	
	|
		rsloMatrixArrayInitializerItems
		SCRL_FLOAT_VALUE
		{
			if(numArrayItemsRemaining){
				currentDefaultItem->matrix = new float[16];
				currentDefaultItem->matrix[0] = $2;
				currentDefaultItem->matrix[1] = 0;
				currentDefaultItem->matrix[2] = 0;
				currentDefaultItem->matrix[3] = 0;
				currentDefaultItem->matrix[4] = 0;
				currentDefaultItem->matrix[5] = $2;
				currentDefaultItem->matrix[6] = 0;
				currentDefaultItem->matrix[7] = 0;
				currentDefaultItem->matrix[8] = 0;
				currentDefaultItem->matrix[9] = 0;
				currentDefaultItem->matrix[10] = $2;
				currentDefaultItem->matrix[11] = 0;
				currentDefaultItem->matrix[12] = 0;
				currentDefaultItem->matrix[13] = 0;
				currentDefaultItem->matrix[14] = 0;
				currentDefaultItem->matrix[15] = 1;
				
				currentDefaultItem++;
				numArrayItemsRemaining--;
			}
		}
	|
		;

rsloVariableDefinitions:
		SCRL_VARIABLES
		SCRL_COLON
		SCRL_NL
		rsloVariables
		;

rsloVariables:
		rsloVariables
		rsloVariable
	|
		;

rsloVariable:
		rsloContainer
		rsloBooleanVariable
		SCRL_NL
	|
		rsloContainer
		rsloFloatVariable
		SCRL_NL
	|
		rsloContainer
		rsloStringVariable
		SCRL_NL
	|
		rsloContainer
		rsloColorVariable
		SCRL_NL
	|
		rsloContainer
		rsloVectorVariable
		SCRL_NL
	|
		rsloContainer
		rsloNormalVariable
		SCRL_NL
	|
		rsloContainer
		rsloPointVariable
		SCRL_NL
	|
		rsloContainer
		rsloMatrixVariable
		SCRL_NL
		;

rsloBooleanVariable:
		SCRL_BOOLEAN
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_BOOLEAN
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
		}
		;


rsloFloatVariable:
		SCRL_FLOAT
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_FLOAT
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
		}
		;

rsloStringVariable:
		SCRL_STRING
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_STRING
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
		}
		;

rsloVectorVariable:
		SCRL_VECTOR
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_VECTOR
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
		}
		;

rsloColorVariable:
		SCRL_COLOR
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_COLOR
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
		}
		;

rsloNormalVariable:
		SCRL_NORMAL
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_NORMAL
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
		}
		;

rsloPointVariable:
		SCRL_POINT
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_POINT
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
		}
		;

rsloMatrixVariable:
		SCRL_MATRIX
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_MATRIX
		SCRL_IDENTIFIER_VALUE
		SCRL_OPEN_SQR_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_SQR_PARANTHESIS
		{
		}
		;

rsloCode:
		rsloCode
		rsloStatement
		SCRL_NL
	|
		rsloCode
		rsloLabelDefinition
		SCRL_NL
	|
		rsloCode
		rsloDSO
		SCRL_NL
	|
		;


rsloDSO:	SCRL_DSO
		SCRL_IDENTIFIER_VALUE
		{
		}
		SCRL_OPEN_PARANTHESIS
		SCRL_TEXT_VALUE
		SCRL_CLOSE_PARANTHESIS
		rsloOperandList
		{
		}
		;

rsloOpcode:
		SCRL_IDENTIFIER_VALUE
		{
		}
		|
		SCRL_DISPLACEMENT
		{
		}
		|
		SCRL_SURFACE
		{
		}
		;

rsloOperandList:
		rsloOperand
		rsloOperandList
		{
		}
		|
		{
		}
		;

rsloStatement:
		rsloOpcode
		rsloOperandList
		{
		}
		;

rsloLabelDefinition:
		SCRL_LABEL_VALUE
		SCRL_COLON
		{
		}
		;

rsloOperand:
		SCRL_TEXT_VALUE
		{
		}
	|
		SCRL_LABEL_VALUE
		{
		}
	|
		SCRL_IDENTIFIER_VALUE
		{
		}
	|
		SCRL_FLOAT_VALUE
		{
		}
	|
		SCRL_OPEN_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_PARANTHESIS
		{
		}
	|
		SCRL_OPEN_PARANTHESIS
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_COMMA
		SCRL_FLOAT_VALUE
		SCRL_CLOSE_PARANTHESIS
		{
		}
		|
		SCRL_OPEN_PARANTHESIS
		SCRL_TEXT_VALUE
		SCRL_CLOSE_PARANTHESIS
		{
		}
		;

%%

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#include "lex.rslo.cpp"
#pragma GCC diagnostic pop

int	rsloLineno	=	0;

///////////////////////////////////////////////////////////////////////
// Function				:	rsloerror
// Description			:	Parser error function
// Return Value			:
// Comments				:
void			rsloerror(const char *s) {
	fprintf(stdout,"%s\n",s);
}


///////////////////////////////////////////////////////////////////////
// Function				:	rsloGet
// Description			:	Parse a shader
// Return Value			:
// Comments				:
TRSLObjectShader		*rsloGet(const char *in,const char *searchpath) {
	TRSLObjectShader		*cShader;
	char			baseName[512];
	char			tmp[sizeof(baseName) + 6];
	const	char	*currentPath;
	char			*dest;

	// Strip explicit extensions
	strncpy(baseName, in, sizeof(baseName));
	baseName[sizeof(baseName)-1] = '\0';
	char *dot = strrchr(baseName, '.');
	if (dot && (strcmp(dot, ".sdr") == 0 || strcmp(dot, ".rslo") == 0)) {
		*dot = '\0';
	}

	// 1. Try .rslo (always prioritized)
	snprintf(tmp, sizeof(tmp), "%s.rslo", baseName);
	rsloin = fopen(tmp, "r");

	// 2. Try .sdr if .rslo fails
	if (rsloin == NULL) {
		snprintf(tmp, sizeof(tmp), "%s.sdr", baseName);
		rsloin = fopen(tmp, "r");
		if (rsloin) {
			fprintf(stdout, "[INFO] rslo: Falling back to .sdr shader \"%s.sdr\"\n", baseName);
		}
	}

	// 3. Try original name if both failed (in case it was a non-standard name)
	if (rsloin == NULL) {
		rsloin = fopen(in, "r");
	}

	if (rsloin == NULL) {
		if (searchpath != NULL) {
			for (dest=tmp,currentPath=searchpath;;) {
				if ((*currentPath == '\0') || (*currentPath == ':')) {		// End of the current path

					if ((dest - tmp) > 0) {		// Do we have anything to record ?
						dest--;

						if ((*dest == '/') || (*dest == '\\')) {	// The last character has to be a rsloash
							dest++;
						} else {
							dest++;
							*dest++	=	'/';
						}

						// Dual-extension search protocol
						char *pathEnd = dest;
						
						// 1. Try .rslo
						snprintf(pathEnd, sizeof(tmp) - (pathEnd - tmp), "%s.rslo", baseName);
						osFixSlashes(tmp);
						rsloin = fopen(tmp, "r");

						// 2. Try .sdr if .rslo fails
						if (rsloin == NULL) {
							snprintf(pathEnd, sizeof(tmp) - (pathEnd - tmp), "%s.sdr", baseName);
							osFixSlashes(tmp);
							rsloin = fopen(tmp, "r");
							if (rsloin) {
								fprintf(stdout, "[INFO] rslo: Falling back to .sdr shader \"%s.sdr\"\n", baseName);
							}
						}

						if (rsloin != NULL)	break;
					}

					dest			=	tmp;

					if (*currentPath == '\0')	break;

					currentPath++;
				} else if (*currentPath == '%') {
					const	char	*endOfCurrentPath	=	strchr(currentPath+1,'%');
					char			environmentVariable[OS_MAX_PATH_LENGTH];

					if (endOfCurrentPath!=NULL) {
						const	int		environmentLength	=	(int) (endOfCurrentPath - currentPath) - 1;
						const	char	*value;

						strncpy(environmentVariable,currentPath+1,environmentLength);
						environmentVariable[environmentLength]	=	'\0';

						value		=	osEnvironment(environmentVariable);
						if (value != NULL) {
							strcpy(dest,value);
							dest	+=	strlen(value);
						}

						currentPath	=	endOfCurrentPath+1;
					} else {
						currentPath++;
					}
				} else if ((*currentPath == '@') || (*currentPath == '&')) {
					currentPath++;
				} else {
					*dest++	=	*currentPath++;
				}
			}
		}
	}

	if (rsloin == NULL)	return NULL;

	parameters			=	NULL;
	parsedShaderName[0]	=	'\0';

	rsloparse();

	fclose(rsloin);

	cShader	=	new TRSLObjectShader;

	if (parsedShaderName[0] != '\0') {
		// The file carried a "#!name" pragma — use it.
		cShader->name = strdup(parsedShaderName);
	} else {
		// Older .rslo/.sdr file with no "#!name" pragma: derive the name
		// from the leaf filename (strip directory and extension).
		char fallbackName[512];
		const char *leaf = in;
		const char *lastSlash = strrchr(in, '/');
		const char *lastBackslash = strrchr(in, '\\');
		if (lastBackslash && (!lastSlash || lastBackslash > lastSlash)) lastSlash = lastBackslash;
		if (lastSlash) leaf = lastSlash + 1;

		strncpy(fallbackName, leaf, sizeof(fallbackName));
		fallbackName[sizeof(fallbackName)-1] = '\0';
		char *fallbackDot = strrchr(fallbackName, '.');
		if (fallbackDot && (strcmp(fallbackDot, ".sdr") == 0 || strcmp(fallbackDot, ".rslo") == 0)) {
			*fallbackDot = '\0';
		}
		cShader->name = strdup(fallbackName);
	}
	cShader->type		=	shaderType;

	// The grammar above prepends each parsed parameter to the head of
	// "parameters", which leaves the list in reverse declaration order.
	// Reverse it once here so callers see parameters in source order.
	TRSLObjectParameter *reversed = NULL;
	while (parameters != NULL) {
		TRSLObjectParameter *next = parameters->next;
		parameters->next = reversed;
		reversed = parameters;
		parameters = next;
	}
	cShader->parameters	=	reversed;

	return cShader;
}

///////////////////////////////////////////////////////////////////////
// Function				:	rsloDelete
// Description			:	Delete a shader
// Return Value			:
// Comments				:
void			rsloDelete(TRSLObjectShader *cShader) {
	TRSLObjectParameter	*cParameter;

	while((cParameter = cShader->parameters) != NULL) {
		cShader->parameters	=	cParameter->next;

		free(cParameter->name);
		if (cParameter->space != NULL) {
			free(cParameter->space);
		}

		if (cParameter->numItems == 1) {
			switch(cParameter->type) {
			case TYPE_FLOAT:
				break;
			case TYPE_VECTOR:
			case TYPE_NORMAL:
			case TYPE_POINT:
			case TYPE_COLOR:
				if (cParameter->defaultValue.vector != NULL) {
					delete [] cParameter->defaultValue.vector;
				}
				break;
			case TYPE_MATRIX:
				if (cParameter->defaultValue.matrix != NULL) {
					delete [] cParameter->defaultValue.matrix;
				}
				break;
			case TYPE_STRING:
				if (cParameter->defaultValue.string != NULL) {
					free(cParameter->defaultValue.string);
				}
				break;
			}
		}

		delete cParameter;
	}

	free(cShader->name);
	delete cShader;
}
