/**
 * Project: openRender
 *
 * File: shadeopDeclaration.h
 *
 * Description:
 *   The shadeop declaration parser (spec 037, FR-012, FR-037; contracts/shadeop.md section 2). It
 *   replaces the flex/bison parser dso.{l,y} that was compiled into four libraries and kept its
 *   result in file statics: this one is reentrant (every piece of state is on the caller's stack)
 *   and defined in libRIBShadeop only.
 *
 *   Grammar, exactly what dso.l/dso.y accepted:
 *       declaration := type name '(' [ parameter { ',' parameter } ] ')'
 *       parameter   := type [ name ]
 *       type        := void | float | vector | normal | point | color | matrix | string
 *       name        := [A-Za-z0-9_]+   (not one of the type words)
 *   The word `output` is ignored wherever it stands. Blanks, tabs and newlines separate tokens; any
 *   other character is skipped silently (the old scanner echoed it to stdout). A token is a maximal
 *   run of name characters, so `floatx` is a name, not the type `float`.
 *
 *   The prototype is the return type letter, '=', and one letter per parameter (o f v n p c m s);
 *   "float f(point, color)" gives name "f", prototype "f=pc". There is no length limit.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef SHADEOPDECLARATION_H
#define SHADEOPDECLARATION_H

#include <string>
#include <string_view>

struct SShadeopDeclaration {
        std::string name;
        std::string prototype;
};

// Parses one declaration. Returns false on any syntax error (out is then unspecified). Reentrant.
bool parseShadeopDeclaration(std::string_view decl, SShadeopDeclaration &out);

#endif
