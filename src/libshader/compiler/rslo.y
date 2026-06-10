/**
 * Project: openRender
 *
 * File: rslo.y
 *
 * Description:
 *   The main parser file.
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
//  Description			:	The main parser file
//
////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
//
//
//
// 	RenderMan Shading Language Compiler 
//	
//
//
//
//	Author  : Okan Arikan
//	Date	: 05/03/2000
//
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////



%require "3.0"

// The grammar has one known, benign shift/reduce conflict: a parenthesized
// arithmetic expression ( expr ) is ambiguous between rsloAritmeticTerminalValue
// and rsloVectorMatrixExpression (since arithmetic reduces to VM).  Bison
// resolves it by preferring shift, which is the correct behavior.
%expect 1

%{
//////////////////////////////////////////////////////////////////////////
// Misc C definitions
//////////////////////////////////////////////////////////////////////////
#undef alloca
#include	<stdlib.h>
#include	<string.h>

#include	"common/global.h"
#include	"common/os.h"
#include	"opcodes.h"
#include	"rslo.h"

	void				rsloerror(const char *);			// Forward definition for stupid yacc
	int					rslolex(void );					// Forward definition for stupid yacc
														
//////////////////////////////////////////////////////////////////////////
// Here's the only global CVariable
//////////////////////////////////////////////////////////////////////////
	// `rslo` is thread_local so that concurrent shader compilations (one per
	// thread) never share state.  Making the Bison parser pure (%define api.pure
	// full) and threading the context through every grammar action would require
	// pervasive changes that are deferred to a later refactoring pass.
	thread_local CScriptContext *rslo;
	
// This macro can be used to record the last parsed line number for accurate error reporting
#define	checkPoint()	rslo->statementLineNo	=	rslo->lineNo

//////////////////////////////////////////////////////////////////////////


%}
%union {
  const char			*string;
  CExpression			*code;
  float					real;
  int					integer;
  CList<CExpression *>	*array;
}
//////////////////////////////////////////////////////////////////////////
// Token definitions
//////////////////////////////////////////////////////////////////////////
%token			SL_SURFACE										// Shader types
%token			SL_DISPLACEMENT
%token			SL_LIGHT
%token			SL_VOLUME
%token			SL_TRANSFORMATION
%token			SL_IMAGER

%token			SL_FOR											// Keywords
%token			SL_WHILE
%token			SL_IF
%token			SL_GATHER
%token			SL_ELSE
%token			SL_BREAK
%token			SL_CONTINUE
%token			SL_ILLUMINANCE
%token			SL_ILLUMINATE
%token			SL_SOLAR
%token			SL_RETURN
%token			SL_VOID

%token			SL_FCN_PI										// Predefined PI constant

%token<string>	SL_IDENTIFIER_VALUE								// Identifier value	
%token<string>	SL_FLOAT_VALUE									// A floating point value

%token			SL_COMMA										// Punctuation marks
%token			SL_SEMI_COLON

%token			SL_OPEN_PARANTHESIS								// Paranthesis	(
%token			SL_CLOSE_PARANTHESIS							// )
%token			SL_OPEN_CRL_PARANTHESIS							// {
%token			SL_CLOSE_CRL_PARANTHESIS						// }
%token			SL_OPEN_SQR_PARANTHESIS							// [
%token			SL_CLOSE_SQR_PARANTHESIS						// ]

%token			SL_TEXTURE
%token			SL_SHADOW
%token			SL_ENVIRONMENT
%token			SL_BUMP

//////////////////////////////////////////////////////////////////////////
// Tokens with precedence relation
//////////////////////////////////////////////////////////////////////////
%left<string> 	SL_TEXT_VALUE

// Assignment (lowest)
%right 			SL_EQUAL
%right			SL_INCREMENT SL_DECREMENT SL_INCREMENT_BY SL_DECREMENT_BY 
%right			SL_MULTIPLY_BY SL_DIVIDE_BY

// Conditional execution
%left			SL_QUESTION SL_COLON

// Type decls
%left 			SL_FLOAT SL_COLOR SL_POINT SL_VECTOR SL_NORMAL SL_MATRIX SL_STRING

// Boolean operators
%left  			SL_OR
%left  			SL_AND
%left 			SL_NOT

// Relation operators
%left  			SL_COMP_EQUAL SL_COMP_DIFFERENT
%left  			SL_COMP_GREATER SL_COMP_GREATER_EQUAL SL_COMP_LESS SL_COMP_LESS_EQUAL

// Binary operators
%left  			SL_PLUS SL_MINUS 
%left  			SL_CROSS
%left  			SL_MULTIPLY SL_DIVIDE
%left  			SL_DOT

// Types
%left			SL_OUTPUT
%left			SL_EXTERN
%left			SL_UNIFORM
%left			SL_VARIABLE
%start	rsloStart

// Rule types
%type<integer>		rsloTypeDecl
%type<integer>		rsloInheritanceClass
%type<integer>		rsloOutputClass
%type<integer>		rsloContainerClass
%type<integer>		rsloFloatSpecifier
%type<integer>		rsloVectorSpecifier
%type<integer>		rsloMatrixSpecifier
%type<integer>		rsloStringSpecifier
%type<integer>		rsloTypeSpecifier
%type<real>			rsloFloatValue
%type<code>			rsloFunctionParameters
%type<code>			rsloFunctionParameter
%type<code>			rsloFunctionParameterIdentifierList
%type<integer>		rsloShaderType
%type<code>			rsloShaderParameters
%type<code>			rsloShaderParameter
%type<code>			rsloShaderParameterInitializer
%type<code>			rsloShaderParameterIdentifierToken
%type<code>			rsloShaderParameterIdentifierList
%type<code>			rsloBlock
%type<code>			rsloVariableInitializer
%type<code>			rsloVariableDeclarations
%type<code>			rsloVariableIdentifierList
%type<code>			rsloVariableIdentifierTail
%type<code>			rsloStatement
%type<code>			rsloStatements
%type<code>			rsloMatchedStatement
%type<code>			rsloUnmatchedStatement
%type<code>			rsloBreakStatement
%type<code>			rsloContinueStatement
%type<code>			rsloReturnStatement
%type<code>			rsloWhileStartStatement
%type<code>			rsloWhileStatement
%type<code>			rsloUnmatchedWhileStatement
%type<code>			rsloAssignmentStatement
%type<code>			rsloUpdateStatement
%type<code>			rsloForStatement
%type<code>			rsloUnmatchedForStatement
%type<code>			rsloForInitStatement
%type<code>			rsloForInitStatements
%type<code>			rsloForCheckStatement
%type<code>			rsloForIncrementStatement
%type<code>			rsloForIncrementStatements
%type<code>			rsloMatchedIfStatement
%type<code>			rsloUnmatchedIfStatement
%type<array>		rsloGatherParameterList
%type<array>		rsloGatherHeader
%type<code>			rsloMatchedGatherStatement
%type<code>			rsloUnmatchedGatherStatement
%type<code>			rsloIlluminanceStatement
%type<code>			rsloUnmatchedIlluminanceStatement
%type<code>			rsloIlluminateStatement
%type<code>			rsloUnmatchedIlluminateStatement
%type<code>			rsloSolarStatement
%type<code>			rsloUnmatchedSolarStatement
%type<array>		rsloArrayItems
%type<array>		rsloArrayList
%type<code>			rsloAritmeticExpression
%type<code>			rsloAritmeticTerminalValue
%type<code>			rsloAritmeticTypeCast
%type<code>			rsloVectorMatrixExpression
%type<code>			rsloVMExpression
%type<code>			rsloFunctionCall
%type<code>			rsloFunctionCallParameters
%type<string>		rsloFunCallHeader
%type<code>			rsloFunCall
%type<string>		rsloTextureName
%type<string>		rsloTextureCall
%type<code>			rsloTextureNameSpecifier
%type<code>			rsloTextureChannelSpecifier
%%
rsloStart:		
		////////////////////////////////////////////////
		// Initilization stuff
		////////////////////////////////////////////////
			{
			}
			rsloShader 
			{
			}
			;

rsloContainerClass:
			SL_UNIFORM
			{
				$$	=	SLC_UNIFORM;
			}
			|
			SL_VARIABLE
			{
				$$	=	SLC_VARYING;
			}
			|
			{
				$$	=	0;
			}
			;


rsloInheritanceClass:
			SL_EXTERN
			{
				$$	=	SLC_EXTERN;
			}
			|
			{
				$$	=	0;
			}
			;

rsloOutputClass:
			SL_OUTPUT
			{
				$$	=	SLC_OUTPUT;
			}
			|
			{
				$$	=	0;
			}
			;

rsloFloatSpecifier:
			SL_FLOAT
			{
				$$	=	SLC_FLOAT;
			}
			;
			
rsloVectorSpecifier:
			SL_COLOR
			{
				$$	=	SLC_VECTOR | SLC_VCOLOR;
			}
			|
			SL_VECTOR
			{
				$$	=	SLC_VECTOR | SLC_VVECTOR;
			}
			|
			SL_NORMAL
			{
				$$	=	SLC_VECTOR | SLC_VNORMAL;
			}
			|
			SL_POINT
			{
				$$	=	SLC_VECTOR | SLC_VPOINT;
			}
			;
			
rsloMatrixSpecifier:
			SL_MATRIX
			{
				$$	=	SLC_MATRIX;
			}
			;
			
rsloStringSpecifier:
			SL_STRING
			{
				$$	=	SLC_STRING | SLC_UNIFORM;
			}
			;
			
			
			
rsloTypeSpecifier:
			rsloFloatSpecifier
			{
				$$	=	$1;
			}
		|
			rsloVectorSpecifier
			{
				$$	=	$1;
			}
		|
			rsloMatrixSpecifier
			{
				$$	=	$1;
			}
		|
			rsloStringSpecifier
			{
				$$	=	$1;
			}
			;
			
rsloFloatValue:	rsloAritmeticExpression
			{
				$$	=	0;
				
				// Is this a float type?
				if ($1->type & SLC_FLOAT) {
					char	tmp[256];
					
					// Is this a simple expression?
					if ($1->value(tmp)) {
					
						// Yes, convert it to float
						$$	=	(float) atof(tmp);
					} else
						rslo->error("Expecting a simple float argument\n");
				} else
					rslo->error("Expecting a float argument\n");
			}
			;

rsloTypeDecl:
			rsloInheritanceClass
			rsloOutputClass
			rsloContainerClass
			rsloTypeSpecifier
			{
				$$	=	$1 | $2 | $3 | $4;
				rslo->desire($$);
				checkPoint();
			}
			;

		////////////////////////////////////////////////
		//
		// Shader file description:
		// Arbitrary number of functions followed by a 
		// shader body. I may need to change this layout if
		// I want to enable individual functions be defined
		// and used from different shader bodies
		//
		////////////////////////////////////////////////
rsloShader:		
		rsloMainOrFunction
		| 
		rsloShader rsloMainOrFunction
		;

rsloMainOrFunction:
		rsloMain | rsloFunction	;

		////////////////////////////////////////////////
		//
		// A function declaration:
		// rsloFunctionReturnType <CFunction_name> (
		// rsloFunctionParameterList ) rsloBlock 
		// 
		//
		////////////////////////////////////////////////
rsloFunctionHeader:
		rsloTypeDecl
		SL_IDENTIFIER_VALUE									// Name of the Function
		SL_OPEN_PARANTHESIS
		{
			CFunction	*thisFunction	=	rslo->newFunction($2);
			
			if ($1 & (SLC_OUTPUT | SLC_EXTERN | SLC_RDONLY)) {
				rslo->error("Invalid return type for function %s\n",$2);
				$1	&=	~(SLC_OUTPUT | SLC_EXTERN | SLC_RDONLY);
			}

			thisFunction->returnValue	=	new CParameter($2,$1,1);

			rslo->undesire();
			checkPoint();
		}
		|
		SL_VOID
		SL_IDENTIFIER_VALUE									// Name of the CFunction
		SL_OPEN_PARANTHESIS
		{
			CFunction	*thisFunction	=	rslo->newFunction($2);
			
			thisFunction->returnValue	=	nullptr;
			checkPoint();
		}
		;

rsloFunction:
		rsloFunctionHeader
		rsloFunctionParameters							// CFunction Parameter list
		SL_CLOSE_PARANTHESIS
		rsloBlock
		{
			CFunction	*cFun		=	rslo->popFunction();

			cFun->initExpression	=	$2;
			cFun->code				=	$4;

			if (cFun->returnValue != nullptr)
				if (cFun->returnValueGiven == FALSE) 
					rslo->error("Return value not given for %s\n",cFun->symbolName);

			checkPoint();
		}
		;
		

		////////////////////////////////////////////////
		// CFunction Parameters
		// No default Parameters
rsloFunctionParameters:
		rsloFunctionParameter								// Semi colon seperated
		SL_SEMI_COLON 
		rsloFunctionParameters
		{
			$$	=	new CTwoExpressions($1,$3);
			checkPoint();
		}
	|
		rsloFunctionParameter
		{
			$$	=	$1;
			checkPoint();
		}
		;

		////////////////////////////////////////////////
		// A single parameter definition
rsloFunctionParameter:
		rsloTypeDecl
		{
			int	type	=	rslo->desired();

			if (type & (SLC_EXTERN)) {
				rslo->error("Invalid parameter type\n");
				type	&=	~(SLC_EXTERN);
			}

			if (type & SLC_OUTPUT) {
				rslo->undesire();
				rslo->desire(type);					// Make sure we mark the desired type as READ-ONLY
			} else {
				rslo->undesire();
				rslo->desire(type | SLC_RDONLY);		// Make sure we mark the desired type as READ-ONLY
			}
		} 
		rsloFunctionParameterIdentifierList
		{
			$$					=	$3;
			rslo->undesire();						// We're done with the type
		}
		|
		{
			$$	=	new CNullExpression;
		}
		;

		////////////////////////////////////////////////
		// An identifier list for CFunction Parameters
rsloFunctionParameterIdentifierList:
		SL_IDENTIFIER_VALUE							// Default Parameter values are not supported yet
		{
			(void)rslo->newParameter($1,rslo->desired(),1);	// Add the Parameter to the current CFunction
		}
		SL_COMMA
		rsloFunctionParameterIdentifierList
		{
			$$	=	$4;
		}
	|
		SL_IDENTIFIER_VALUE
		{
			(void)rslo->newParameter($1,rslo->desired(),1);

			$$			=	new CNullExpression;
		}
	|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		SL_CLOSE_SQR_PARANTHESIS
		{
			(void)rslo->newParameter($1,rslo->desired() | SLC_ARRAY,-1);
		}
		SL_COMMA
		rsloFunctionParameterIdentifierList
		{
			$$	=	$6;
		}
	|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloFloatValue
		SL_CLOSE_SQR_PARANTHESIS
		{
			(void)rslo->newParameter($1,rslo->desired() | SLC_ARRAY, (int) $3);
		}
		SL_COMMA
		rsloFunctionParameterIdentifierList
		{
			$$	=	$7;
		}
	|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		SL_CLOSE_SQR_PARANTHESIS
		{
			(void)rslo->newParameter($1,rslo->desired() | SLC_ARRAY, 1);
			
			$$	=	new CNullExpression;
		}
	|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloFloatValue
		SL_CLOSE_SQR_PARANTHESIS
		{
			(void)rslo->newParameter($1,rslo->desired() | SLC_ARRAY, (int) $3);

			$$	=	new CNullExpression;
		}
		;

		////////////////////////////////////////////////
		//
		// Main shader body definition
		//
rsloMain:	rsloShaderType							// Type of the shader
		SL_IDENTIFIER_VALUE						// Name of the shader
		SL_OPEN_PARANTHESIS
		{
			if (rslo->shaderType) {
				rslo->error("Shader file contains multiple shaders\n");
				rslo->lastFunction			=	rslo->shaderFunction;
			} else {
				CFunction		*mainFunction	=	rslo->newFunction(constantShaderMain);

				mainFunction->returnValue		=	nullptr;

				rslo->shaderName				=	strdup($2);
				rslo->shaderType				=	$1;
				rslo->shaderFunction			=	mainFunction;
				rslo->lastFunction			=	mainFunction;
			}
		}
		rsloShaderParameters						// Shader Parameter list
		SL_CLOSE_PARANTHESIS
		{
			rslo->restoreParameters();
		}
		rsloBlock
		{
			CFunction	*cFun			=	rslo->popFunction();

			for (CParameter	*cParameter=cFun->parameters->first();cParameter!=nullptr;cParameter=cFun->parameters->next()) {
				rslo->variables->push(cParameter);
			}

			cFun->initExpression	=	$5;
			cFun->code				=	$8;
		}
		;

		////////////////////////////////////////////////
		// Shader type
rsloShaderType:	SL_SURFACE
		{
			$$	=	SLC_SURFACE;
		}
	|
		SL_DISPLACEMENT
		{
			$$	=	SLC_DISPLACEMENT;
		}
	|
		SL_LIGHT
		{
			$$	=	SLC_LIGHT;
		}
	|
		SL_VOLUME
		{
			$$	=	SLC_VOLUME;
		}
	|
		SL_TRANSFORMATION
		{
			$$	=	SLC_TRANSFORMATION;
		}
	|
		SL_IMAGER
		{
			$$	=	SLC_IMAGER;
		}
		;


		////////////////////////////////////////////////
		// Shader Parameters
rsloShaderParameters:
		rsloShaderParameter 
		SL_SEMI_COLON
		rsloShaderParameters
		{
			$$	=	new CTwoExpressions($1,$3);
		}
	|
		rsloShaderParameter
		{
			$$	=	$1;
		}
		;

		/////////////////////////////////////////////////
		// A single shader Parameter
rsloShaderParameter:
		rsloTypeDecl
		{
			int	type	=	$1;

			// Inside the shader param declarations, params
			// are uniform by default
			if (type & SLC_VARYING) {
				// clear this marker, only used to invert the
				// default from varying to uniform
				type &= ~SLC_VARYING;
			} else {	 
				type |= SLC_UNIFORM;	 
			}
			
			rslo->undesire();
			rslo->desire(type);
		}
		rsloShaderParameterIdentifierList
		{
			$$	=	$3;
		
			if ($1 & (SLC_EXTERN)) {
				rslo->error("Invalid parameter type for the shader\n");
			}

			rslo->undesire();
		}
		|
		{
			$$	=	new CNullExpression;
		}
		;

		////////////////////////////////////////////////
		// Shader Parameter initializer
rsloShaderParameterInitializer:
		SL_EQUAL
		rsloAritmeticExpression
		{
			$$	=	getAssignment(rslo->variableList,$2);
		}
		|
		SL_EQUAL
		rsloArrayList
		{
			$$	=	getAssignment(rslo->variableList,$2);
		}
		;


rsloShaderParameterIdentifierToken:
		SL_IDENTIFIER_VALUE
		SL_COMMA
		{
			CParameter	*cParameter	=	rslo->newParameter($1,rslo->desired() | SLC_PARAMETER,1);

			rslo->variableList->push(cParameter);	// Save the parameter so that we can generate init code later
		}
		rsloShaderParameterIdentifierToken
		{
			$$	=	$4;
		}
	|
		SL_IDENTIFIER_VALUE
		{
			CParameter	*cParameter	=	rslo->newParameter($1,rslo->desired() | SLC_PARAMETER,1);

			rslo->variableList->push(cParameter);
		}
		rsloShaderParameterInitializer
		{
			$$	=	$3;
		}
	|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloFloatValue
		SL_CLOSE_SQR_PARANTHESIS
		SL_COMMA
		{
			CParameter	*cParameter	=	rslo->newParameter($1,rslo->desired() | SLC_PARAMETER | SLC_ARRAY,(int) $3);

			rslo->variableList->push(cParameter);
		}
		rsloShaderParameterIdentifierToken
		{
			$$	=	$7;
		}
	|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloFloatValue
		SL_CLOSE_SQR_PARANTHESIS
		{
			CParameter	*cParameter	=	rslo->newParameter($1,rslo->desired() | SLC_PARAMETER | SLC_ARRAY,(int) $3);

			rslo->variableList->push(cParameter);
		}
		rsloShaderParameterInitializer
		{
			$$	=	$6;
		}

	|
		{
			$$	=	new CNullExpression;
		}

		;

rsloShaderParameterIdentifierList:
		rsloShaderParameterIdentifierToken
		{
			$$	=	$1;
		}
	|
		rsloShaderParameterIdentifierToken
		SL_COMMA
		rsloShaderParameterIdentifierList
		{
			$$	=	new CTwoExpressions($1,$3);
		}
	;

		
		////////////////////////////////////////////////
		// A block
rsloBlock:
		SL_OPEN_CRL_PARANTHESIS
		{
			(void)rslo->newFunction(constantBlockName);
		}
		rsloStatements
		SL_CLOSE_CRL_PARANTHESIS
		{
			CFunction	*cFun	=	rslo->popFunction();

			cFun->code			=	$3;

			$$					=	new	CFuncallExpression(cFun,nullptr);
		}
		;

		////////////////////////////////////////////////
		// A statement
		////////////////////////////////////////////////
		// Variable declarations in a block
rsloVariableDeclarations:
		rsloTypeDecl
		rsloVariableIdentifierList
		{
			CVariable	*cVar;

			if ($1 & (SLC_OUTPUT | SLC_RDONLY)) {
				rslo->error("Invalid container class for local variables\n");
			}

			// Remove the uninitialized variables from the list
			while((cVar = (CVariable *) rslo->variableList->pop()) != nullptr);

			rslo->undesire();

			$$	=	$2;
		}
		;

		////////////////////////////////////////////////
		// Variable identifier list
rsloVariableInitializer:
		SL_EQUAL
		rsloAritmeticExpression
		{
			$$	=	getAssignment(rslo->variableList,$2);
		}
		|
		SL_EQUAL
		rsloArrayList
		{
			$$	=	getAssignment(rslo->variableList,$2);
		}
		;

rsloVariableIdentifierList:
		SL_IDENTIFIER_VALUE
		{
			CVariable	*cVar	=	rslo->newVariable($1,rslo->desired(),1);

			rslo->variableList->push(cVar);
		}
		rsloVariableIdentifierTail
		{
			$$	=	$3;
		}
	|
		SL_IDENTIFIER_VALUE
		{
			CVariable	*cVar	=	rslo->newVariable($1,rslo->desired(),1);

			rslo->variableList->push(cVar);
		}
		rsloVariableInitializer
		rsloVariableIdentifierTail
		{
			$$	=	new CTwoExpressions($3,$4);
		}
	|
		
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloFloatValue
		SL_CLOSE_SQR_PARANTHESIS
		{
			CVariable	*cVar	=	rslo->newVariable($1,rslo->desired() | SLC_ARRAY,(int) $3);

			rslo->variableList->push(cVar);
		}
		rsloVariableIdentifierTail
		{
			$$	=	$6;
		}
	|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloFloatValue
		SL_CLOSE_SQR_PARANTHESIS
		{
			CVariable	*cVar	=	rslo->newVariable($1,rslo->desired() | SLC_ARRAY,(int) $3);

			rslo->variableList->push(cVar);
		}

		rsloVariableInitializer
		rsloVariableIdentifierTail
		{
			$$	=	new CTwoExpressions($6,$7);
		}
		;

rsloVariableIdentifierTail:
		SL_COMMA
		rsloVariableIdentifierList
		{
			$$	=	$2;
			checkPoint();
		}
	|
		SL_SEMI_COLON
		{
			$$	=	new CNullExpression;
		}
		;


		////////////////////////////////////////////////
		// A general statement
rsloStatement:
		rsloUnmatchedStatement
		{

			$$	=	$1;
			if (!(rslo->desired() & SLC_NONE))
				rslo->fatalbailout();
			checkPoint();
		}
	|
		rsloMatchedStatement
		{
			$$	=	$1;
			if (!(rslo->desired() & SLC_NONE))
				rslo->fatalbailout();
			checkPoint();
		}
	|
		error
		{
			// Recoverable error happened
			$$	=	new CNullExpression;
			if (!(rslo->desired() & SLC_NONE))
				rslo->fatalbailout();
			checkPoint();
		}

		;

rsloStatements:
		rsloStatements
		{
			rslo->statementLineNo	=	rslo->lineNo;
		}
		rsloStatement
		{
			$$	=	new CTwoExpressions($1,$3);
		}
	|
		{
			rslo->statementLineNo	=	rslo->lineNo;
			$$	=	new CNullExpression;
		}
		;


		////////////////////////////////////////////////
		// A Matched statement
rsloMatchedStatement:
		rsloForStatement
		{
			$$	=	$1;
		}
	|
		rsloWhileStatement
		{
			$$	=	$1;
		}
	|
		rsloMatchedIfStatement
		{
			$$	=	$1;
		}
	|
		rsloMatchedGatherStatement
		{
			$$	=	$1;
		}
	|
		rsloAssignmentStatement SL_SEMI_COLON
		{
			$$	=	$1;
		}
	|
		rsloUpdateStatement SL_SEMI_COLON
		{
			$$	=	$1;
		}
	|
		rsloBreakStatement 
		{
			$$	=	$1;
		}
	|
		rsloContinueStatement 
		{
			$$	=	$1;
		}
	|
		rsloReturnStatement
		{
			$$	=	$1;
		}
	|	
		rsloIlluminanceStatement
		{
			$$	=	$1;
		}
	|
		rsloIlluminateStatement
		{
			$$	=	$1;
		}
	|
		rsloSolarStatement
		{
			$$	=	$1;
		}
	|
		rsloBlock
		{
			$$	=	$1;
		}
	|
		rsloVariableDeclarations
		{
			$$	=	$1;
		}
	|
		rsloFunction
		{
			$$	=	new CNullExpression;
		}
	|
		rsloFunctionCall SL_SEMI_COLON
		{
			$$	=	$1;
		}
	|
		SL_SEMI_COLON
		{
			$$					=	new CNullExpression;
		}
		;

		////////////////////////////////////////////////
		// Unmatched shatement
rsloUnmatchedStatement:
		rsloUnmatchedIfStatement
		{
			$$	=	$1;
		}
	|
		rsloUnmatchedGatherStatement
		{
			$$	=	$1;
		}
	|
		rsloUnmatchedWhileStatement
		{
			$$	=	$1;
		}
	|
		rsloUnmatchedForStatement
		{
			$$	=	$1;
		}
	|
		rsloUnmatchedIlluminanceStatement
		{
			$$	=	$1;
		}
	|
		rsloUnmatchedIlluminateStatement
		{
			$$	=	$1;
		}
	|
		rsloUnmatchedSolarStatement
		{
			$$	=	$1;
		}
		;

		////////////////////////////////////////////////
		// Break statement
rsloBreakStatement:
		SL_BREAK SL_FLOAT_VALUE SL_SEMI_COLON
		{
			CFunction	*cFunction;
			int			bc;
			char		tmp[256];

			if (sscanf($2,"%d",&bc) != 1) rslo->error("Invalid break count: %s\n",$2);

			if (bc <= 0) rslo->error("Bad break count: %s\n",$2);

			for(cFunction = rslo->functionStack->last(); cFunction != nullptr; cFunction = rslo->functionStack->prev()) {
				if (strcmp(cFunction->symbolName,constantLoopName) == 0) bc--;
				else if (strcmp(cFunction->symbolName,constantBlockName) == 0) continue;

				if (bc == 0) break;

				rslo->error("Break target not found\n");
				break;
			}

			if (cFunction == nullptr) rslo->error("Break target not found\n");

			snprintf(tmp,sizeof(tmp),"%s\t%s\n",opcodeBreak,$2);

			$$	=	new CFixedExpression(tmp);
		}
	|
		SL_BREAK SL_SEMI_COLON
		{
			CFunction	*cFunction;
			char		tmp[256];
			int			bc	=	(int) 1;

			rslo->functionStack->push(rslo->lastFunction);
			for(cFunction = rslo->functionStack->last(); cFunction != nullptr; cFunction = rslo->functionStack->prev()) {
				if (strcmp(cFunction->symbolName,constantLoopName) == 0) bc--;
				else if (strcmp(cFunction->symbolName,constantBlockName) == 0) continue;

				if (bc == 0) break;

				rslo->error("Break target not found\n");
				break;
			}
			rslo->lastFunction	=	rslo->functionStack->pop();

			if (cFunction == nullptr) rslo->error("Break target not found\n");

			snprintf(tmp,sizeof(tmp),"%s\t1\n",opcodeBreak);

			$$	=	new CFixedExpression(tmp);
		}
		;

		////////////////////////////////////////////////
		// Continue statement
rsloContinueStatement:
		SL_CONTINUE SL_FLOAT_VALUE SL_SEMI_COLON
		{
			CFunction	*cFunction;
			char		tmp[256];
			int			bc;

			if (sscanf($2,"%d",&bc) != 1) rslo->error("Bad continue count: %s\n",$2);

			if (bc <= 0) rslo->error("Bad continue count: %s\n",$2);

			for(cFunction = rslo->functionStack->last(); cFunction != nullptr; cFunction = rslo->functionStack->prev()) {
				if (strcmp(cFunction->symbolName,constantLoopName) == 0) bc--;
				else if (strcmp(cFunction->symbolName,constantBlockName) == 0) continue;

				if (bc == 0) break;

				rslo->error("Continue target not found\n");
				break;
			}

			if (cFunction == nullptr) rslo->error("Continue target not found\n");

			snprintf(tmp,sizeof(tmp),"%s\t%s\n",opcodeContinue,$2);

			$$	=	new CFixedExpression(tmp);
		}
	|
		SL_CONTINUE SL_SEMI_COLON
		{
			CFunction	*cFunction;
			char		tmp[256];
			int			bc	=	(int) 1;

			for(cFunction = rslo->functionStack->last(); cFunction != nullptr; cFunction = rslo->functionStack->prev()) {
				if (strcmp(cFunction->symbolName,constantLoopName) == 0) bc--;
				else if (strcmp(cFunction->symbolName,constantBlockName) == 0) continue;

				if (bc == 0) break;
				
				rslo->error("Continue target not found\n");
				break;
			}

			if (cFunction == nullptr) rslo->error("Continue target not found\n");

			snprintf(tmp,sizeof(tmp),"%s\t1\n",opcodeContinue);

			$$	=	new CFixedExpression(tmp);
		}
		;

		////////////////////////////////////////////////
		// Return statement
rsloReturnStatement:
		SL_RETURN 
		{
			CFunction	*cFun = rslo->lastFunction;
			
			// Work out what we're returning from
			for (cFun = rslo->functionStack->last(); cFun != nullptr; cFun = rslo->functionStack->prev()) {
				if (strcmp(cFun->symbolName,constantBlockName) == 0) continue;
				if (strcmp(cFun->symbolName,constantLoopName) == 0) continue;
				break;
			}

			// Figure out what the return type is and desire it
			CParameter	*retParam = (cFun != nullptr) ? cFun->returnValue : nullptr;
			if (retParam) {
				int returnType = retParam->type;
				rslo->desire(returnType);
			} else {
				rslo->desire(SLC_NONE);
			}
		}
		rsloAritmeticExpression SL_SEMI_COLON
		{
			CFunction	*cFun	=	rslo->lastFunction;
			CExpression	*c;

			rslo->undesire();
			
			// Skip over loops
			for (cFun = rslo->functionStack->last(); cFun != nullptr; cFun = rslo->functionStack->prev()) {
				if (strcmp(cFun->symbolName,constantBlockName) == 0) continue;
				if (strcmp(cFun->symbolName,constantLoopName) == 0) continue;
				break;
			}

			if (cFun ==	nullptr) {
				rslo->error("Return target not found\n");
				$$	=	new CNullExpression;
			} else {
				if (cFun->returnValue == nullptr) {
					if (cFun == rslo->shaderFunction)
						rslo->warning("Shader was not expecting a return statement\n");
					else
						rslo->error("Function \"%s\" was not expecting a return value\n",cFun->symbolName);
					c	=	new CNullExpression;
				} else {
					// Warn if the actual return type is different from the declared return type. Some type conversions
					// (like upconvert float to vector) can be done, but may not be intended.
					if (($3->type & (SLC_FLOAT|SLC_VECTOR|SLC_STRING|SLC_MATRIX)) 
						!= (cFun->returnValue->type & (SLC_FLOAT|SLC_VECTOR|SLC_STRING|SLC_MATRIX)))
						rslo->warning("Return value of function \"%s\" does not match function declaration\n",cFun->symbolName);
						
					// if the return type is uniform, set the return value to uniform
					if ($3->type & SLC_UNIFORM) cFun->returnValue->type |= SLC_UNIFORM;
					c	=	new CAssignmentExpression(cFun->returnValue,$3);
				}

				$$	=	c;

				cFun->returnValueGiven	=	TRUE;
			}
		}
	|
		SL_RETURN SL_SEMI_COLON
		{
			CFunction	*cFun	=	rslo->lastFunction;

			// Skip over loops
			for (cFun = rslo->functionStack->last(); cFun != nullptr; cFun = rslo->functionStack->prev()) {
				if (strcmp(cFun->symbolName,constantBlockName) == 0) continue;
				if (strcmp(cFun->symbolName,constantLoopName) == 0) continue;
				break;
			}

			if (cFun ==	nullptr)	rslo->error("Return target not found\n");
			else {
				if (cFun->returnValue != nullptr) {
					rslo->error("Function \"%s\" was expecting a return value\n",cFun->symbolName);
				}
			}
		}
		;
		

		////////////////////////////////////////////////
		// While statement
rsloWhileStartStatement:
		SL_WHILE
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		{
			(void)rslo->newFunction(constantLoopName);

			$$	=	$3;
		}
		;


rsloWhileStatement:
		rsloWhileStartStatement
		rsloMatchedStatement
		{
			(void)rslo->popFunction();

			$$	=	new CForLoop(nullptr,$1,nullptr,$2);
		}
		;

rsloUnmatchedWhileStatement:
		rsloWhileStartStatement
		rsloUnmatchedStatement
		{
			(void)rslo->popFunction();

			$$	=	new CForLoop(nullptr,$1,nullptr,$2);
		}
		;

		////////////////////////////////////////////////
		// A general assignment statement
rsloAssignmentStatement:
		SL_IDENTIFIER_VALUE
		SL_EQUAL
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {	
				$$	=	new CNullExpression;
			} else {
				$$	=	new CAssignmentExpression(cVar,$4);
				rslo->undesire();
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		SL_EQUAL
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else if (cVar->type & SLC_ARRAY) {
				$$	=	new CArrayAssignmentExpression(cVar,$3,$7);
				rslo->undesire();
			} else {
				CList<CExpression *>	*dummyParams = new CList<CExpression *>;
				CFunctionPrototype		*cFun;

				dummyParams->push(new CTerminalExpression(cVar));
				dummyParams->push($3);
				dummyParams->push($7);

				// Check the builtin functions
				for (cFun = rslo->builtinFunctions->first(); cFun != nullptr; cFun = rslo->builtinFunctions->next()) {
					if (cFun->match("setcomp",dummyParams,rslo->desired())) break;
				}
				
				if (cFun == nullptr) {
					// Cleanup
					CExpression	*cCode;
					while((cCode = dummyParams->pop()) != nullptr) {
						delete cCode;
					}
					delete dummyParams;
					// Report error
					rslo->error("Can't assign to non array\n");
					$$	=	new CNullExpression;
				} else {
					$$	=	new CBuiltinExpression(cFun,dummyParams);
				}
				rslo->undesire();
			}
		}
		;

		////////////////////////////////////////////////
		// A general assignment statement
rsloUpdateStatement:
		SL_IDENTIFIER_VALUE
		SL_INCREMENT_BY
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else {
				$$	=	new CUpdateExpression(cVar,opcodeAddFloatFloat,opcodeAddVectorVector,FALSE,$4);
				rslo->undesire();
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_DECREMENT_BY
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else {
				$$	=	new CUpdateExpression(cVar,opcodeSubFloatFloat,opcodeSubVectorVector,FALSE,$4);
				rslo->undesire();
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_INCREMENT
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else {
				$$	=	new CUpdateExpression(cVar,opcodeAddFloatFloat,opcodeAddVectorVector,FALSE,new CConstantTerminalExpression(SLC_FLOAT,strdup("1")));
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_DECREMENT
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else {
				$$	=	new CUpdateExpression(cVar,opcodeAddFloatFloat,opcodeAddVectorVector,FALSE,new CConstantTerminalExpression(SLC_FLOAT,strdup("-1")));
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_MULTIPLY_BY
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else {
				$$	=	new CUpdateExpression(cVar,opcodeMulFloatFloat,opcodeMulVectorVector,FALSE,$4);
				rslo->undesire();
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_DIVIDE_BY
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else {
				$$	=	new CUpdateExpression(cVar,opcodeDivFloatFloat,opcodeDivVectorVector,FALSE,$4);
				rslo->undesire();
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		SL_INCREMENT_BY
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);
			
			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else {
				$$	=	new CArrayUpdateExpression(cVar,$3,$7,opcodeAddFloatFloat,opcodeAddVectorVector,opcodeAddMatrixMatrix);
				rslo->undesire();
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		SL_DECREMENT_BY
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else {
				$$	=	new CArrayUpdateExpression(cVar,$3,$7,opcodeSubFloatFloat,opcodeSubVectorVector,opcodeSubMatrixMatrix);
				rslo->undesire();
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		SL_INCREMENT
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else {
				$$	=	new CArrayUpdateExpression(cVar,$3,new CConstantTerminalExpression(SLC_FLOAT,strdup("1")),opcodeAddFloatFloat,opcodeAddVectorVector,opcodeAddMatrixMatrix);
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		SL_DECREMENT
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else {
				$$	=	new CArrayUpdateExpression(cVar,$3,new CConstantTerminalExpression(SLC_FLOAT,strdup("-1")),opcodeAddFloatFloat,opcodeAddVectorVector,opcodeAddMatrixMatrix);
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		SL_MULTIPLY_BY
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else {
				$$	=	new CArrayUpdateExpression(cVar,$3,$7,opcodeMulFloatFloat,opcodeMulVectorVector,opcodeMulMatrixMatrix);
				rslo->undesire();
			}
		}
		|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		SL_DIVIDE_BY
		{
			CVariable	*cVar				=	rslo->getVariable($1);

			if (cVar == nullptr)	rslo->error("Identifier \"%s\" is not found\n",$1);
			else				rslo->desire(cVar->type);
		}
		rsloAritmeticExpression
		{
			CVariable			*cVar		=	rslo->getVariable($1);

			if (cVar == nullptr) {
				$$	=	new CNullExpression;
			} else {
				$$	=	new CArrayUpdateExpression(cVar,$3,$7,opcodeDivFloatFloat,opcodeDivVectorVector,opcodeDivMatrixMatrix);
				rslo->undesire();
			}
		}
		;

		////////////////////////////////////////////////
		// A general for statement
rsloForStartStatement:
		SL_FOR
		{
			(void)rslo->newFunction(constantLoopName);
		}

rsloForStatement:
		rsloForStartStatement
		SL_OPEN_PARANTHESIS
		rsloForInitStatement
		SL_SEMI_COLON
		rsloForCheckStatement
		SL_SEMI_COLON
		rsloForIncrementStatement
		SL_CLOSE_PARANTHESIS
		rsloMatchedStatement
		{
			(void)rslo->popFunction();

			$$	=	new CForLoop($3,$5,$7,$9);
		}
		;

rsloUnmatchedForStatement:
		rsloForStartStatement
		SL_OPEN_PARANTHESIS
		rsloForInitStatement
		SL_SEMI_COLON
		rsloForCheckStatement
		SL_SEMI_COLON
		rsloForIncrementStatement
		SL_CLOSE_PARANTHESIS
		rsloUnmatchedStatement
		{
			(void)rslo->popFunction();

			$$	=	new CForLoop($3,$5,$7,$9);
		}
		;

		////////////////////////////////////////////////
		// For init statement list
rsloForInitStatement:
		rsloForInitStatements
		{
			$$	=	$1;
		}
	|
		{
			$$	=	new CNullExpression;
		}
		;

		////////////////////////////////////////////////
		// For init statements
rsloForInitStatements:
		rsloAssignmentStatement
		SL_COMMA
		rsloForInitStatements
		{
			$$	=	new CTwoExpressions($1,$3);
		}
	|
		rsloAssignmentStatement
		{
			$$	=	$1;
		}
		;

		////////////////////////////////////////////////
		// For check statement
rsloForCheckStatement:
		rsloAritmeticExpression
		{
			$$	=	getConversion(SLC_FLOAT,$1);
		}
	|
		{
			$$	=	new CConstantTerminalExpression(SLC_FLOAT | SLC_UNIFORM,strdup("1"));
		}
		;

		////////////////////////////////////////////////
		// For increment statement list
rsloForIncrementStatement:
		rsloForIncrementStatements
		{
			$$	=	$1;
		}
	|
		{
			$$	=	new CNullExpression;
		}
		;

		////////////////////////////////////////////////
		// For increment statements
rsloForIncrementStatements:
		rsloAssignmentStatement
		SL_COMMA
		rsloForIncrementStatements
		{
			$$	=	new CTwoExpressions($1,$3);
		}
	|
		rsloAssignmentStatement
		{
			$$	=	$1;
		}
	|
		rsloUpdateStatement
		SL_COMMA
		rsloForIncrementStatements
		{
			$$	=	new CTwoExpressions($1,$3);
		}
	|
		rsloUpdateStatement
		{
			$$	=	$1;
		}
		;

		////////////////////////////////////////////////
		// Matched if statement
rsloMatchedIfStatement:
		SL_IF
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloMatchedStatement
		SL_ELSE
		rsloMatchedStatement
		{
			$$	=	new CIfThenElse($3,$5,$7);
		}
		;

		////////////////////////////////////////////////
		// Unmatched if statement
rsloUnmatchedIfStatement:
		SL_IF
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloStatement
		{
			$$	=	new CIfThenElse($3,$5,nullptr);
		}
	|
		SL_IF
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloMatchedStatement
		SL_ELSE
		rsloUnmatchedStatement
		{
			$$	=	new CIfThenElse($3,$5,$7);
		}
		;



		////////////////////////////////////////////////
		// Gather parameter list
rsloGatherParameterList:
		rsloGatherParameterList
		SL_COMMA
		rsloAritmeticExpression
		{
			$$->push($3);
		}
		|
		rsloAritmeticExpression
		{
			rslo->actualParameters->push($1);
			$$	=	rslo->actualParameters;
		}
		;

rsloGatherHeader:
		SL_GATHER
		SL_OPEN_PARANTHESIS
		{
			rslo->actualParameterStack->push(rslo->actualParameters);
			rslo->actualParameters	=	new CList<CExpression *>;
		}
		rsloGatherParameterList
		SL_CLOSE_PARANTHESIS
		{
			$$	=	$4;
		}
		;

		////////////////////////////////////////////////
		// Matched if statement
rsloMatchedGatherStatement:
		rsloGatherHeader
		rsloMatchedStatement
		SL_ELSE
		rsloMatchedStatement
		{
			$$	=	new CGatherThenElse($1,$2,$4);
			rslo->actualParameters	=	rslo->actualParameterStack->pop();
		}
		;

		////////////////////////////////////////////////
		// Unmatched if statement
rsloUnmatchedGatherStatement:
		rsloGatherHeader
		rsloStatement
		{
			$$	=	new CGatherThenElse($1,$2,nullptr);
			rslo->actualParameters	=	rslo->actualParameterStack->pop();
		}
	|
		rsloGatherHeader
		rsloMatchedStatement
		SL_ELSE
		rsloUnmatchedStatement
		{
			$$	=	new CGatherThenElse($1,$2,$4);
			rslo->actualParameters	=	rslo->actualParameterStack->pop();
		}
		;

		////////////////////////////////////////////////
		// Illuminance statement
rsloIlluminanceStartStatement:
		SL_ILLUMINANCE
		{
			(void)rslo->newFunction(constantLoopName);

			rslo->requiredShaderContext	|=	SLC_SURFACE;
		}

rsloIlluminanceStatement:
		rsloIlluminanceStartStatement
		SL_OPEN_PARANTHESIS
		rsloArrayItems
		SL_CLOSE_PARANTHESIS
		rsloMatchedStatement
		{
			(void)rslo->popFunction();

			$$	=	new CIlluminationLoop($3,$5);

		}
		;

rsloUnmatchedIlluminanceStatement:
		rsloIlluminanceStartStatement
		SL_OPEN_PARANTHESIS
		rsloArrayItems
		SL_CLOSE_PARANTHESIS
		rsloUnmatchedStatement
		{
			(void)rslo->popFunction();

			$$	=	new CIlluminationLoop($3,$5);
		}

		;

		////////////////////////////////////////////////
		// Illuminate statement
rsloIlluminateStatement:
		SL_ILLUMINATE
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloMatchedStatement
		{
			rslo->requiredShaderContext	|=	SLC_LIGHT;

			$$	=	new CIlluminateSolar(opcodeIlluminate,opcodeEndIlluminate,$3,nullptr,nullptr,$5);
		}
	|
		SL_ILLUMINATE
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloMatchedStatement
		{
			rslo->requiredShaderContext	|=	SLC_LIGHT;

			$$	=	new CIlluminateSolar(opcodeIlluminate,opcodeEndIlluminate,$3,$5,$7,$9);
		}
		;

rsloUnmatchedIlluminateStatement:
		SL_ILLUMINATE
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloUnmatchedStatement
		{
			rslo->requiredShaderContext	|=	SLC_LIGHT;

			$$	=	new CIlluminateSolar(opcodeIlluminate,opcodeEndIlluminate,$3,nullptr,nullptr,$5);
		}
	|
		SL_ILLUMINATE
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloUnmatchedStatement
		{
			rslo->requiredShaderContext	|=	SLC_LIGHT;

			$$	=	new CIlluminateSolar(opcodeIlluminate,opcodeEndIlluminate,$3,$5,$7,$9);
		}
		;

		////////////////////////////////////////////////
		// Solar statement
rsloSolarStatement:
		SL_SOLAR
		SL_OPEN_PARANTHESIS
		SL_CLOSE_PARANTHESIS
		rsloMatchedStatement
		{
			rslo->requiredShaderContext	|=	SLC_LIGHT;

			$$	=	new CIlluminateSolar(opcodeSolar,opcodeEndSolar,nullptr,nullptr,nullptr,$4);
		}
	|
		SL_SOLAR
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloMatchedStatement
		{
			rslo->requiredShaderContext	|=	SLC_LIGHT;

			$$	=	new CIlluminateSolar(opcodeSolar,opcodeEndSolar,$3,$5,nullptr,$7);
		}
		;

rsloUnmatchedSolarStatement:
		SL_SOLAR
		SL_OPEN_PARANTHESIS
		SL_CLOSE_PARANTHESIS
		rsloUnmatchedStatement
		{
			rslo->requiredShaderContext	|=	SLC_LIGHT;

			$$	=	new CIlluminateSolar(opcodeSolar,opcodeEndSolar,nullptr,nullptr,nullptr,$4);
		}
	|
		SL_SOLAR
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		rsloUnmatchedStatement
		{
			rslo->requiredShaderContext	|=	SLC_LIGHT;

			$$	=	new CIlluminateSolar(opcodeSolar,opcodeEndSolar,$3,$5,nullptr,$7);
		}
		;

		////////////////////////////////////////////////
		// Aritmetic expression
rsloAritmeticExpression:
		rsloAritmeticTerminalValue
		{
			$$	=	$1;
		}
	|
		rsloAritmeticExpression
		SL_PLUS
		rsloAritmeticExpression
		{
			$$	=	getOperation($1,$3,opcodeAddFloatFloat,opcodeAddVectorVector,opcodeAddMatrixMatrix,nullptr,0);
		}
	|
		rsloAritmeticExpression
		SL_MINUS
		rsloAritmeticExpression
		{
			$$	=	getOperation($1,$3,opcodeSubFloatFloat,opcodeSubVectorVector,opcodeSubMatrixMatrix,nullptr,0);
		}
	|
		rsloAritmeticExpression
		SL_MULTIPLY
		rsloAritmeticExpression
		{
			$$	=	nullptr;

			if ($1->type & SLC_MATRIX) {
				if ($3->type & SLC_VVECTOR) {
					$$	=	new CBinaryExpression(SLC_VECTOR | SLC_VVECTOR,opcodeMulMatrixVector,$1,getConversion(SLC_VECTOR,$3));
				} else if ($3->type & SLC_VPOINT) {
					$$	=	new CBinaryExpression(SLC_VECTOR | SLC_VPOINT,opcodeMulMatrixPoint,$1,getConversion(SLC_VECTOR,$3));
				} else if ($3->type & SLC_VNORMAL) {
					$$	=	new CBinaryExpression(SLC_VECTOR | SLC_VNORMAL,opcodeMulMatrixNormal,$1,getConversion(SLC_VECTOR,$3));
				}
			}

			if ($$ == nullptr) {
				if ($3->type & SLC_MATRIX) {
					if ($1->type & SLC_VVECTOR) {
						$$	=	new CBinaryExpression(SLC_VECTOR | SLC_VVECTOR,opcodeMulVectorMatrix,$1,getConversion(SLC_VECTOR,$3));
					} else if ($1->type & SLC_VPOINT) {
						$$	=	new CBinaryExpression(SLC_VECTOR | SLC_VPOINT,opcodeMulPointMatrix,$1,getConversion(SLC_VECTOR,$3));
					} else if ($1->type & SLC_VNORMAL) {
						$$	=	new CBinaryExpression(SLC_VECTOR | SLC_VNORMAL,opcodeMulNormalMatrix,$1,getConversion(SLC_VECTOR,$3));
					}
				}
			}


			if ($$ == nullptr)
				$$	=	getOperation($1,$3,opcodeMulFloatFloat,opcodeMulVectorVector,opcodeMulMatrixMatrix,nullptr,0);
		}
	|
		rsloAritmeticExpression
		SL_DIVIDE
		rsloAritmeticExpression
		{
			$$	=	getOperation($1,$3,opcodeDivFloatFloat,opcodeDivVectorVector,opcodeDivMatrixMatrix,nullptr,0);
		}
	|
		rsloAritmeticExpression
		SL_DOT
		{
			rslo->desire(SLC_VECTOR | SLC_VVECTOR);
		}
		rsloAritmeticExpression
		{
			rslo->undesire();
			$$	=	new CBinaryExpression(SLC_FLOAT,opcodeDotProduct,getConversion(SLC_VECTOR,$1),getConversion(SLC_VECTOR,$4));
		}
	|
		rsloAritmeticExpression
		SL_CROSS
		{
			rslo->desire(SLC_VECTOR | SLC_VVECTOR);
		}
		rsloAritmeticExpression
		{
			rslo->undesire();
			$$	=	new CBinaryExpression(SLC_VECTOR,opcodeCrossProduct,getConversion(SLC_VECTOR,$1),getConversion(SLC_VECTOR,$4));
		}
	|
		SL_PLUS
		rsloAritmeticExpression
		{
			$$	=	$2;	
		}
	|
		SL_MINUS
		rsloAritmeticExpression
		{
			$$	=	getOperation($2,opcodeNegFloat,opcodeNegVector,opcodeNegMatrix,nullptr,0);
		}
	|
		rsloAritmeticExpression
		SL_QUESTION
		rsloAritmeticExpression
		SL_COLON
		rsloAritmeticExpression
		{
			if (($3->type & SLC_TYPE_MASK) == ($5->type & SLC_TYPE_MASK)) {
				$$	=	new CConditionalExpression($3->type,$1,$3,$5);
			} else {
				rslo->error("Type mismatch in conditional execution\n");
			}
		}
	|
		rsloAritmeticExpression
		SL_AND
		rsloAritmeticExpression
		{
			$$	=	getOperation($1,$3,opcodeAnd,nullptr,nullptr,nullptr,0);
		}
	|
		rsloAritmeticExpression
		SL_OR
		rsloAritmeticExpression
		{
			$$	=	getOperation($1,$3,opcodeOr,nullptr,nullptr,nullptr,0);
		}
	|
		rsloAritmeticExpression
		SL_COMP_GREATER
		rsloAritmeticExpression
		{
			$$			=	getOperation($1,$3,opcodeFloatGreater,opcodeVectorGreater,nullptr,nullptr,SLC_FLOAT);
		}
	|
		rsloAritmeticExpression
		SL_COMP_LESS
		rsloAritmeticExpression
		{
			$$			=	getOperation($1,$3,opcodeFloatLess,opcodeVectorLess,nullptr,nullptr,SLC_FLOAT);
		}
	|
		rsloAritmeticExpression
		SL_COMP_GREATER_EQUAL
		rsloAritmeticExpression
		{
			$$			=	getOperation($1,$3,opcodeFloatEGreater,opcodeVectorEGreater,nullptr,nullptr,SLC_FLOAT);
		}
	|
		rsloAritmeticExpression
		SL_COMP_LESS_EQUAL
		rsloAritmeticExpression
		{
			$$			=	getOperation($1,$3,opcodeFloatELess,opcodeVectorELess,nullptr,nullptr,SLC_FLOAT);
		}
	|
		rsloAritmeticExpression
		SL_COMP_EQUAL
		rsloAritmeticExpression
		{
			$$			=	getOperation($1,$3,opcodeFloatEqual,opcodeVectorEqual,nullptr,opcodeStringEqual,SLC_FLOAT);
		}
	|
		rsloAritmeticExpression
		SL_COMP_DIFFERENT
		rsloAritmeticExpression
		{
			$$			=	getOperation($1,$3,opcodeFloatNotEqual,opcodeVectorNotEqual,nullptr,opcodeStringNotEqual,SLC_FLOAT);
		}
	|
		SL_NOT
		rsloAritmeticExpression
		{
			$$			=	new CUnaryExpression(SLC_FLOAT,opcodeNot,getConversion(SLC_FLOAT,$2));
		}
		;

rsloArrayList:
		SL_OPEN_CRL_PARANTHESIS
		rsloArrayItems
		SL_CLOSE_CRL_PARANTHESIS
		{
			$$	=	$2;
		}
		;

rsloArrayItems:
		rsloAritmeticExpression
		SL_COMMA
		rsloArrayItems
		{
			$$	=	$3;
			$$->push($1);
		}
		|
		rsloAritmeticExpression
		{
			$$	=	new CList<CExpression *>;
			$$->push($1);
		}
		;

rsloAritmeticTerminalValue:
		SL_FCN_PI
		{
			$$	=	new CConstantTerminalExpression(SLC_FLOAT | SLC_UNIFORM,strdup("3.141592654"));
		}
	|
		rsloAssignmentStatement
		{
			$$	=	$1;
		}
	|
		rsloUpdateStatement
		{
			$$	=	$1;
		}
	|
		rsloAritmeticTypeCast
		{
			$$	=	$1;
		}
	|
		rsloFunctionCall
		{
			$$	=	$1;
		}
	|
		rsloTypeDecl
		rsloFunctionCall
		{
			$$	=	getConversion($1,$2);
			rslo->undesire();
		}
	|
		rsloTypeDecl
		SL_TEXT_VALUE
		rsloFunctionCall
		{
			$$	=	getConversion($1,$2,$3);
			rslo->undesire();
		}
	|
		SL_IDENTIFIER_VALUE
		{
	
			CVariable	*cVar	=	rslo->getVariable($1);

			if (cVar == nullptr) {
				rslo->error("Identifier \"%s\" is not found\n",$1);
				$$	=	new CNullExpression;
			} else { 
				$$	=	new CTerminalExpression(cVar);
			}
		}
	|
		rsloTypeDecl
		SL_IDENTIFIER_VALUE
		{
	
			CVariable	*cVar	=	rslo->getVariable($2);

			if (cVar == nullptr) {
				rslo->error("Identifier \"%s\" is not found\n",$2);
				$$	=	new CNullExpression;
			} else { 
				$$	=	getConversion($1,new CTerminalExpression(cVar));
			}
			
			rslo->undesire();
		}
	|
		SL_IDENTIFIER_VALUE
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		{
			CVariable *cVar	=	rslo->getVariable($1);

			if (cVar == nullptr) {
				rslo->error("Identifier \"%s\" is not found\n",$1);
				$$	=	new CNullExpression;
			} else if (cVar->type & SLC_ARRAY) { 
				$$	=	new CArrayExpression(cVar,$3);
			} else {
				CList<CExpression *>	*dummyParams = new CList<CExpression *>;
				CFunctionPrototype		*cFun;

				dummyParams->push(new CTerminalExpression(cVar));
				dummyParams->push($3);

				// Check the builtin functions
				for (cFun = rslo->builtinFunctions->first(); cFun != nullptr; cFun = rslo->builtinFunctions->next()) {
					if (cFun->match("comp",dummyParams,SLC_FLOAT)) break;
				}
				
				if (cFun == nullptr) {
					// Cleanup
					CExpression	*cCode;
					while((cCode = dummyParams->pop()) != nullptr) {
						delete cCode;
					}
					delete dummyParams;
					// Report error
					rslo->error("Can't index non array\n");
					$$	=	new CNullExpression;
				} else {
					$$	=	new CBuiltinExpression(cFun,dummyParams);
				}
			}
		}
	|
		SL_FLOAT_VALUE
		{
			$$	=	new CConstantTerminalExpression(SLC_FLOAT,strdup($1));
		}
	|
		SL_TEXT_VALUE
		{
			$$	=	new CConstantTerminalExpression(SLC_STRING,strdup($1));
		}
	|
		SL_OPEN_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_PARANTHESIS
		{	
			$$	=	$2;
		}
		;
	


rsloAritmeticTypeCast:
		rsloTypeDecl
		SL_TEXT_VALUE
		{
			// Change the expected type to float
			rslo->undesire();
			rslo->desire(SLC_FLOAT | ($1 & (~(SLC_TYPE_MASK | SLC_SUB_TYPE_MASK))));
		}
		rsloVectorMatrixExpression
		{
			$$	=	getConversion($1,$2,$4);

			rslo->undesire();
		}
	|
		rsloTypeDecl
		{
			// Change the expected type to float
			rslo->undesire();
			rslo->desire(SLC_FLOAT | ($1 & (~(SLC_TYPE_MASK | SLC_SUB_TYPE_MASK))));
		}
		rsloVectorMatrixExpression
		{
			$$	=	getConversion($1,$3);

			rslo->undesire();
		}
	|
		rsloTypeDecl
		{
			// Change the expected type to float
			rslo->undesire();
			rslo->desire(SLC_FLOAT | ($1 & (~(SLC_TYPE_MASK | SLC_SUB_TYPE_MASK))));
		}
		SL_FLOAT_VALUE
		{
			$$	=	getConversion($1,new CConstantTerminalExpression(SLC_FLOAT,strdup($3)));

			rslo->undesire();
		}
	|
		rsloVectorMatrixExpression
		{
			$$	=	$1;
		}
		;
		
rsloVectorMatrixExpression:
		SL_OPEN_PARANTHESIS
		rsloVMExpression
		SL_CLOSE_PARANTHESIS
		{
			$$	=	$2;
		}
		;
	
		
rsloVMExpression:
		rsloAritmeticExpression
		{
			$$	=	$1;
		}
	|
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		{
			$$	=	new CVectorExpression($1,$3,$5);
		}
	|
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		SL_COMMA
		rsloAritmeticExpression
		{
			CExpression	*elements[16];

			elements[0]		=	$1;
			elements[1]		=	$3;
			elements[2]		=	$5;
			elements[3]		=	$7;
			elements[4]		=	$9;
			elements[5]		=	$11;
			elements[6]		=	$13;
			elements[7]		=	$15;
			elements[8]		=	$17;
			elements[9]		=	$19;
			elements[10]	=	$21;
			elements[11]	=	$23;
			elements[12]	=	$25;
			elements[13]	=	$27;
			elements[14]	=	$29;
			elements[15]	=	$31;

			$$				=	new CMatrixExpression(elements);
		}
		
		////////////////////////////////////////////////
		// A CFunction call


		
rsloFunctionCall:
		rsloTextureCall
		{
			CFunctionPrototype		*cFun;
			CList<CExpression *>	*parameters	=	rslo->actualParameters;	// This is the list of parameters to the function

			// Clear the desired type flags
			rslo->undesire();

			// Restore the old parameters
			rslo->actualParameters		=	rslo->actualParameterStack->pop();

			// Check the builtin functions
			for (cFun = rslo->builtinFunctions->first(); cFun != nullptr; cFun = rslo->builtinFunctions->next()) {
				if (cFun->perfectMatch($1,parameters,rslo->desired())) break;
			}

			if (cFun == nullptr) {
				for (cFun = rslo->builtinFunctions->first(); cFun != nullptr; cFun = rslo->builtinFunctions->next()) {
					if (cFun->match($1,parameters,rslo->desired())) break;
				}
			}

			if (cFun != nullptr) {
				$$	=	new CBuiltinExpression(cFun,parameters);
			} else {
				rslo->error("Function \"%s\" is not found\n",$1);
				if (parameters->numItems != 0) {
					CExpression	*cCode;
					while((cCode = parameters->pop()) != nullptr) {
						delete cCode;
					}
				}
				delete parameters;
				$$	=	new CNullExpression;
			}
		}
		|
		rsloFunCall
		{
			$$	=	$1;
		}
		;

	

rsloFunCallHeader:
		SL_IDENTIFIER_VALUE
		SL_OPEN_PARANTHESIS
		{
			// Save the old parameters
			rslo->actualParameterStack->push(rslo->actualParameters);
			// Start a new parameter list
			rslo->actualParameters	=	new CList<CExpression *>;

			// Make sure we do not do something stupid with the parameters
			rslo->desire(SLC_NONE);

			// Set the name
			$$	=	$1;
		}
		|
		SL_SURFACE
		SL_OPEN_PARANTHESIS
		{
			// Save the old parameters
			rslo->actualParameterStack->push(rslo->actualParameters);
			// Start a new parameter list
			rslo->actualParameters	=	new CList<CExpression *>;

			// Make sure we do not do something stupid with the parameters
			rslo->desire(SLC_NONE);

			// Set the name
			$$	=	"surface";
		}
		|
		SL_DISPLACEMENT
		SL_OPEN_PARANTHESIS
		{
			// Save the old parameters
			rslo->actualParameterStack->push(rslo->actualParameters);
			// Start a new parameter list
			rslo->actualParameters	=	new CList<CExpression *>;

			// Make sure we do not do something stupid with the parameters
			rslo->desire(SLC_NONE);

			// Set the name
			$$	=	"displacement";
		}
		;

rsloFunCall:
		rsloFunCallHeader
		rsloFunctionCallParameterList
		SL_CLOSE_PARANTHESIS
		{
			CFunction				*cFun;
			CList<CExpression *>	*parameters	=	rslo->actualParameters;	// This is the list of parameters to the function

			// Clear the desired type flags
			rslo->undesire();

			// Restore the old parameters
			rslo->actualParameters		=	rslo->actualParameterStack->pop();

			// Search for the CFunction here....
			cFun	=	rslo->getFunction($1,parameters);

			if (cFun != nullptr) {													// Cool, the function exists
				// A function with the same name is defined ... 
				// Try to match the parameters;
				if (cFun->parameters->numItems == parameters->numItems) {	// The number of parameters match
					// A function with the same name is defined and the number of Parameters match
					$$	=	new CFuncallExpression(cFun,parameters);					
				}
			}

			// If not found, check the predefined CFunctions
			if (cFun == nullptr) {
				CFunctionPrototype	*cProto;
				// Check the builtin CFunctions

				for (cProto = rslo->builtinFunctions->first(); cProto != nullptr; cProto = rslo->builtinFunctions->next()) {
					if (cProto->perfectMatch($1,parameters,rslo->desired())) break;
				}

				if (cProto == nullptr) {
					for (cProto = rslo->builtinFunctions->first(); cProto != nullptr; cProto = rslo->builtinFunctions->next()) {
						if (cProto->match($1,parameters,rslo->desired())) break;
					}
				}

				// Check if there is a DSO implementing this function
				if (cProto == nullptr) {
					rslo->enumerateDso($1);

					for (cProto = rslo->builtinFunctions->first(); cProto != nullptr; cProto = rslo->builtinFunctions->next()) {
						if (cProto->perfectMatch($1,parameters,rslo->desired())) break;
					}

					if (cProto == nullptr) {
						for (cProto = rslo->builtinFunctions->first(); cProto != nullptr; cProto = rslo->builtinFunctions->next()) {
							if (cProto->match($1,parameters,rslo->desired())) break;
						}
					}
				}

				if (cProto != nullptr) {
					$$	=	new CBuiltinExpression(cProto,parameters);
				} else {
					rslo->error("Function \"%s\" is not found\n",$1);
					if (parameters->numItems != 0) {
						CExpression	*cCode;
						while((cCode = parameters->pop()) != nullptr) {
							delete cCode;
						}
					}
					delete parameters;
					$$	=	new CNullExpression;
				}

			}
		}
		;


		// Either text of value
rsloTextureNameSpecifier:
		SL_TEXT_VALUE
		{
			$$	=	new CConstantTerminalExpression(SLC_STRING | SLC_UNIFORM,strdup($1));
		}
		|
		SL_IDENTIFIER_VALUE
		{
			CVariable	*cVar	=	rslo->getVariable($1);

			if (cVar != nullptr)	$$	=	new CTerminalExpression(cVar);
			else				$$	=	new CNullExpression;
		}
		;


rsloTextureChannelSpecifier:
		SL_OPEN_SQR_PARANTHESIS
		rsloAritmeticExpression
		SL_CLOSE_SQR_PARANTHESIS
		{
			$$	=	getConversion(SLC_FLOAT | ($2->type & SLC_UNIFORM),$2);
		}
		|
		{
			$$	=	new CConstantTerminalExpression(SLC_FLOAT | SLC_UNIFORM,strdup("0"));
		}
		;

		// There are 4 cases
		//	texture(textureIdentifier,...)
		//	texture("textureName",...)
		//	texture(textureIdentifier[channelIdentifier],...)
		//	texture("textureName"[channelIdentifier],...)
rsloTextureCall:
		rsloTextureName
		SL_OPEN_PARANTHESIS
		rsloTextureNameSpecifier
		rsloTextureChannelSpecifier
		SL_COMMA
		rsloFunctionCallParameterList
		SL_CLOSE_PARANTHESIS
		{
			// Fake the parameters
			CList<CExpression *>	*parameters	=	rslo->actualParameters;	// This is the list of parameters to the function
			CList<CExpression *>	*pl			=	new CList<CExpression *>;
			CExpression				*cExpression;

			pl->push($3);
			pl->push($4);
			for (cExpression=parameters->first();cExpression!=nullptr;cExpression=parameters->next())
				pl->push(cExpression);

			delete parameters;
			rslo->actualParameters	=	pl;

			$$						=	$1;
		}
		|
		rsloTextureName
		SL_OPEN_PARANTHESIS
		rsloTextureNameSpecifier
		rsloTextureChannelSpecifier
		SL_CLOSE_PARANTHESIS
		{
			// Fake the parameters
			CList<CExpression *>	*parameters	=	rslo->actualParameters;	// This is the list of parameters to the function
			CList<CExpression *>	*pl			=	new CList<CExpression *>;
			CExpression				*cExpression;

			pl->push($3);
			pl->push($4);
			for (cExpression=parameters->first();cExpression!=nullptr;cExpression=parameters->next())
				pl->push(cExpression);

			delete parameters;
			rslo->actualParameters	=	pl;

			$$						=	$1;
		}
		;

rsloTextureName:
		SL_TEXTURE
		{
			$$	=	"texture";

			// Save the old parameters
			rslo->actualParameterStack->push(rslo->actualParameters);
			// Start a new parameter list
			rslo->actualParameters	=	new CList<CExpression *>;

			// Make sure we do not do something stupid with the parameters
			rslo->desire(SLC_NONE);
		}
		|
		SL_SHADOW
		{
			$$	=	"shadow";

			// Save the old parameters
			rslo->actualParameterStack->push(rslo->actualParameters);
			// Start a new parameter list
			rslo->actualParameters	=	new CList<CExpression *>;

			// Make sure we do not do something stupid with the parameters
			rslo->desire(SLC_NONE);
		}
		|
		SL_ENVIRONMENT
		{
			$$	=	"environment";

			// Save the old parameters
			rslo->actualParameterStack->push(rslo->actualParameters);
			// Start a new parameter list
			rslo->actualParameters	=	new CList<CExpression *>;

			// Make sure we do not do something stupid with the parameters
			rslo->desire(SLC_NONE);
		}
		|
		SL_BUMP
		{
			$$	=	"bump";

			// Save the old parameters
			rslo->actualParameterStack->push(rslo->actualParameters);
			// Start a new parameter list
			rslo->actualParameters	=	new CList<CExpression *>;

			// Make sure we do not do something stupid with the parameters
			rslo->desire(SLC_NONE);
		}
		;

		////////////////////////////////////////////////
		// CFunction Parameters
rsloFunctionCallParameterList:
		rsloFunctionCallParameters
		{
		}
	|
		{
		}
		;

rsloFunctionCallParameters:
		rsloAritmeticExpression
		SL_COMMA
		{
			rslo->actualParameters->push($1);
		}
		rsloFunctionCallParameters
		{
		}
	|
		rsloAritmeticExpression
		{
			rslo->actualParameters->push($1);
		}
		;

%%


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#include	"lex.rslo.cpp"
#pragma GCC diagnostic pop



int	CScriptContext::compile(FILE *in,char *outName) {
	
	rslo = this;

	rsloin	=	in;

	rsloparse();

	// Must have exactly one main shader function
	if (!rslo->shaderType)
		rslo->error("Shader file missing main shader body\n");

	if (compileError == 0) {
		char		*tmp;

		if (outName == nullptr) {
			// If there's no compile error, dump the compiled code
			tmp	=	new char[strlen(rslo->shaderName)+6];

			strcpy(tmp,rslo->shaderName);
			if (rslo->legacyRSLObjectExt)
				strcat(tmp,".sdr");
			else
				strcat(tmp,".rslo");
		} else {
			tmp	=	outName;
		}

		rslo->generateCode(tmp);

		if (tmp != outName) delete [] tmp;

		return TRUE;
	}
	return FALSE;
}


void	rsloerror(const char *) {
	if (rslotext && rslotext[0])
		rslo->error("Parse error before '%s'\n",rslotext);
	else
		rslo->error("Parse error\n");
}

