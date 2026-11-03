/**
 * Project: openRender
 *
 * File: riCondition.h
 *
 * Description:
 *   The `if` evaluator of the Scene Interface (spec 038): IfBegin and ElseIf conditions, evaluated against the
 *   attributes and options of the context they are given. It used to be CRendererContext::ifParse.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef RICONDITION_H
#define RICONDITION_H

class CRiInterface;

// Evaluates expr using context->getAttributes(TRUE) and context->getOptions(); a context whose accessors
// answer nullptr finds nothing ("Failed to find variable", the expression is false). Returns 1 (true), 0
// (false) or 2 (the expression did not parse).
int riEvaluateCondition(CRiInterface *context, const char *expr);

#endif
