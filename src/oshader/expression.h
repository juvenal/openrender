/**
 * Project: Pixie
 *
 * File: expression.h
 *
 * Description:
 *   This file defines the interface for expression.
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
//  File				:	expressions.h
//  Classes				:	CExpression
//							CNullExpression
//							CVectorExpression
//							CMatrixExpression
//							CTerminalExpression
//							CConstantTerminalExpression
//							CBinaryExpression
//							CUnaryExpression
//							CSysConversionExpression
//							CFuncallExpression
//							CBuiltinExpression
//							CConditionalExpression
//							CAssignmentExpression
//							CUpdateExpression
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "common/containers.h"
#include "common/os.h"

// Forward definitions used
class CSymbol;            // CSymbol class
class CVariable;          // CVariable class
class CParameter;         // CParameter class
class CFunction;          // CFunction class
class CFunctionPrototype; // A function prototype
class CScriptContext;     // The main shader context. Holds everything

/////////////////////
//
//
//	The mighty expression class
//
//
/////////////////////

///////////////////////////////////////////////////////////////////////
// Class				:	CExpression
// Description			:	This class holds an expression (multiplication , function call, etc)
// Comments				:
class CExpression {
    public:
        CExpression(int);
        virtual ~CExpression();
        CExpression(const CExpression &) = delete;
        CExpression &operator=(const CExpression &) = delete;

        virtual void getCode(FILE *, CVariable *);
        [[nodiscard]] virtual CVariable *getVariable();
        virtual int value(char *);

        int type;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTwoExpressions
// Description			:	This expression is used to encapsulate multiple expressions
// Comments				:
class CTwoExpressions : public CExpression {
    public:
        CTwoExpressions(CExpression *, CExpression *);
        ~CTwoExpressions() override;

        void getCode(FILE *, CVariable *) override;

        CExpression *first, *second;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CNullExpression
// Description			:	This class is instanciated if there has been an error
// Comments				:
class CNullExpression : public CExpression {
    public:
        CNullExpression();
};

///////////////////////////////////////////////////////////////////////
// Class				:	CVectorExpression
// Description			:	Cast a vector from a collection of floats
// Comments				:
class CVectorExpression : public CExpression {
    public:
        CVectorExpression(CExpression *, CExpression *, CExpression *);
        ~CVectorExpression() override;

        void getCode(FILE *, CVariable *) override;
        int value(char *) override;

        CExpression *x, *y, *z;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CMatrixExpression
// Description			:	Cast a matrix from a collection of 16 floats
// Comments				:
class CMatrixExpression : public CExpression {
    public:
        CMatrixExpression(CExpression **);
        ~CMatrixExpression() override;

        void getCode(FILE *, CVariable *) override;
        int value(char *) override;

        CExpression *elements[16];
};

///////////////////////////////////////////////////////////////////////
// Class				:	CTerminalExpression
// Description			:	This encapsulates a variable reference
// Comments				:
class CTerminalExpression : public CExpression {
    public:
        CTerminalExpression(CVariable *);
        ~CTerminalExpression() override;

        void getCode(FILE *, CVariable *) override;
        [[nodiscard]] CVariable *getVariable() override { return variable; }

        CVariable *variable;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CArrayExpression
// Description			:	Extract a particular item from an array
// Comments				:
class CArrayExpression : public CExpression {
    public:
        CArrayExpression(CVariable *, CExpression *);
        ~CArrayExpression() override;

        void getCode(FILE *, CVariable *) override;

        CVariable *array;
        CExpression *item;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CConstantExpression
// Description			:	This encapsulates a constant expression
// Comments				:
class CConstantTerminalExpression : public CExpression {
    public:
        CConstantTerminalExpression(int, char *);
        ~CConstantTerminalExpression() override;

        void getCode(FILE *, CVariable *) override;
        [[nodiscard]] CVariable *getVariable() override { return dummy; }
        int value(char *) override;

        char *constant;
        CVariable *dummy;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBinaryExpression
// Description			:	A binary expression
// Comments				:
class CBinaryExpression : public CExpression {
    public:
        CBinaryExpression(int, const char *, CExpression *, CExpression *);
        ~CBinaryExpression() override;

        void getCode(FILE *, CVariable *) override;

        CExpression *first, *second;
        const char *opcode;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CUnaryExpression
// Description			:	A unary expression
// Comments				:
class CUnaryExpression : public CExpression {
    public:
        CUnaryExpression(int, const char *, CExpression *);
        ~CUnaryExpression() override;

        void getCode(FILE *, CVariable *) override;

        CExpression *first;
        const char *opcode;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CSysConversionExpression
// Description			:	System conversion
// Comments				:
class CSysConversionExpression : public CExpression {
    public:
        CSysConversionExpression(int, const char *, const char *, CExpression *);
        ~CSysConversionExpression() override;

        void getCode(FILE *, CVariable *) override;
        int value(char *) override;

        CExpression *first;
        const char *opcode;
        const char *system;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CFuncallExpression
// Description			:	Function call
// Comments				:
class CFuncallExpression : public CExpression {
    public:
        CFuncallExpression(CFunction *, CList<CExpression *> *);
        ~CFuncallExpression() override;

        void getCode(FILE *, CVariable *) override;

        CList<CExpression *> *arguments;
        CFunction *function;
        int error;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CBuiltinExpression
// Description			:	A built in function call
// Comments				:
class CBuiltinExpression : public CExpression {
    public:
        CBuiltinExpression(CFunctionPrototype *, CList<CExpression *> *);
        ~CBuiltinExpression() override;

        void getCode(FILE *, CVariable *) override;

        CList<CExpression *> *arguments;
        CFunctionPrototype *function;
        const char *replacementPrototype;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CConditionalExpression
// Description			:	A conditional statement (?:)
// Comments				:
class CConditionalExpression : public CExpression {
    public:
        CConditionalExpression(int, CExpression *, CExpression *, CExpression *);
        ~CConditionalExpression() override;

        void getCode(FILE *, CVariable *) override;

        CExpression *condition;
        CExpression *trueExpression;
        CExpression *falseExpression;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CAssignmentExpression
// Description			:	Assignment expression
// Comments				:
class CAssignmentExpression : public CExpression {
    public:
        CAssignmentExpression(CVariable *, CExpression *);
        ~CAssignmentExpression() override;

        void getCode(FILE *, CVariable *) override;

        CVariable *first;
        CExpression *second;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CArrayAssignmentExpression
// Description			:	Assignment expression
// Comments				:
class CArrayAssignmentExpression : public CExpression {
    public:
        CArrayAssignmentExpression(CVariable *, CExpression *, CExpression *);
        ~CArrayAssignmentExpression() override;

        void getCode(FILE *, CVariable *) override;

        CVariable *first;
        CExpression *index;
        CExpression *second;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CArrayAssignmentExpression
// Description			:	Assignment expression
// Comments				:
class CArrayUpdateExpression : public CExpression {
    public:
        CArrayUpdateExpression(CVariable *, CExpression *, CExpression *, const char *, const char *, const char *);
        ~CArrayUpdateExpression() override;

        void getCode(FILE *, CVariable *) override;

        CVariable *first;
        CVariable *indexVar;
        CExpression *index;
        CExpression *arrayAssigner;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CArray
// Description			:	Assignment expression
// Comments				:
class CArrayMove : public CExpression {
    public:
        CArrayMove(CVariable *, CList<CExpression *> *);
        ~CArrayMove() override;

        void getCode(FILE *, CVariable *) override;

        CVariable *first;
        CList<CExpression *> *items;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CUpdateExpression
// Description			:	Update expression
// Comments				:
class CUpdateExpression : public CExpression {
    public:
        CUpdateExpression(CVariable *, const char *, const char *, int, CExpression *);
        ~CUpdateExpression() override;

        void getCode(FILE *, CVariable *) override;

        CVariable *first;
        CExpression *second;
        const char *opcodeFloat, *opcodeVector;
        int pre;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CIfThenElse
// Description			:	If - Then - Else expression
// Comments				:
class CIfThenElse : public CExpression {
    public:
        CIfThenElse(CExpression *, CExpression *, CExpression *);
        ~CIfThenElse() override;

        void getCode(FILE *, CVariable *) override;

        CExpression *cond;
        CExpression *first;
        CExpression *second;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CGatherThenElse
// Description			:	Gather - Then - Else expression
// Comments				:
class CGatherThenElse : public CExpression {
    public:
        CGatherThenElse(CList<CExpression *> *, CExpression *, CExpression *);
        ~CGatherThenElse() override;

        void getCode(FILE *, CVariable *) override;

        CList<CExpression *> *parameterList;
        CExpression *first;
        CExpression *second;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CForLoop
// Description			:	For loop
// Comments				:
class CForLoop : public CExpression {
    public:
        CForLoop(CExpression *, CExpression *, CExpression *, CExpression *);
        ~CForLoop() override;

        void getCode(FILE *, CVariable *) override;

        CExpression *start, *cond, *update;
        CExpression *body;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CIlluminationLoop
// Description			:	Illumination loop
// Comments				:
class CIlluminationLoop : public CExpression {
    public:
        CIlluminationLoop(CList<CExpression *> *, CExpression *);
        ~CIlluminationLoop() override;

        void getCode(FILE *, CVariable *) override;

        CExpression *category, *P, *N, *angle;
        CExpression *body;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CIlluminateSolar
// Description			:	Illuminate/solar loop
// Comments				:
class CIlluminateSolar : public CExpression {
    public:
        CIlluminateSolar(const char *, const char *, CExpression *, CExpression *, CExpression *, CExpression *);
        ~CIlluminateSolar() override;

        void getCode(FILE *, CVariable *) override;

        CExpression *P, *N, *angle;
        CExpression *body;
        const char *beginOpcode, *endOpcode;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CFixedExpression
// Description			:	A fixed string expression
// Comments				:
class CFixedExpression : public CExpression {
    public:
        CFixedExpression(const char *);
        ~CFixedExpression() override;

        void getCode(FILE *, CVariable *) override;

        char *fixed;
};

////////////////////////////////////////////////////////////////////////
//
//	The following functions are used to generate operations and stuff
//
//	All the gunctions below ditch the arguments
//
////////////////////////////////////////////////////////////////////////

CExpression *getOperation(CExpression *first, CExpression *second, const char *opcodeFloat, const char *opcodeVector, const char *opcodeMatrix, const char *opcodeString, int typeOverwrite);
CExpression *getOperation(CExpression *first, const char *opcodeFloat, const char *opcodeVector, const char *opcodeMatrix, const char *opcodeString, int typeOverwrite);
CExpression *getConversion(int type, CExpression *first);
CExpression *getConversion(int type, const char *system, CExpression *first);
void getConversion(FILE *, CVariable *, CExpression *);
CExpression *getAssignment(CList<CVariable *> *, CExpression *);
CExpression *getAssignment(CList<CVariable *> *, CList<CExpression *> *);

// CCodeBlock	*getAssignment(char *,CExpression *);
#endif
