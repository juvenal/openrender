/**
 * Project: openRender
 *
 * File: diagnostics.h
 *
 * Description:
 *   The diagnostics service every domain uses: error codes, severities, an
 *   installable sink, and the error/warning/fatal/info reporting functions.
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
//  File				:	diagnostics.h
//  Classes				:	CDiagnosticSink
//  Description			:	The error display functions
//
////////////////////////////////////////////////////////////////////////
#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "global.h" // The global header file

// Error codes for the error function (Used by sfError)
typedef enum {
    CODE_NOERROR,
    CODE_NOMEM,
    CODE_SYSTEM,
    CODE_NOFILE,
    CODE_BADFILE,
    CODE_VERSION,
    CODE_INCAPABLE,
    CODE_OPTIONAL,
    CODE_UNIMPLEMENT,
    CODE_LIMIT,
    CODE_BUG,
    CODE_NOTSTARTED,
    CODE_NESTING,
    CODE_NOTOPTIONS,
    CODE_NOTATTRIBS,
    CODE_NOTPRIMS,
    CODE_ILLSTATE,
    CODE_BADMOTION,
    CODE_BADSOLID,
    CODE_BADTOKEN,
    CODE_RANGE,
    CODE_CONSISTENCY,
    CODE_BADHANDLE,
    CODE_NOSHADER,
    CODE_MISSINGDATA,
    CODE_SYNTAX,
    CODE_MATH,
    CODE_LOG,
    CODE_SCRIPT,
    CODE_PRINTF,
    CODE_RESOLUTION,
    CODE_STATS,
    CODE_PROGRESS
} EErrorCode;

// How serious a diagnostic is. Scene Interface maps these onto the RI
// severities (RIE_INFO, RIE_WARNING, RIE_ERROR, RIE_SEVERE).
typedef enum {
    DIAGNOSTIC_INFO,
    DIAGNOSTIC_WARNING,
    DIAGNOSTIC_ERROR,
    DIAGNOSTIC_SEVERE
} EDiagnosticSeverity;

///////////////////////////////////////////////////////////////////////
// Class				:	CDiagnosticSink
// Description			:	Where diagnostics go. Scene Interface installs one
//							that forwards to the active RI context's error
//							handler; with none installed, the default rules
//							of diagnosticsPrintDefault() apply.
// Comments				:	report() may be called from any thread.
class CDiagnosticSink {
    public:
        virtual ~CDiagnosticSink() = default;

        // message is the fully formatted text
        virtual void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) = 0;
};

// Installs sink process-wide (nullptr = the default rules) and returns the
// previously installed one, which the caller restores when it is done.
CDiagnosticSink *installDiagnosticSink(CDiagnosticSink *sink);

// The default rules, those of RiErrorPrint: severe -> stderr then exit(-1);
// error -> stderr; warning -> stdout; info -> stdout only at INFO verbosity
// or more.
void diagnosticsPrintDefault(EDiagnosticSeverity severity, const char *message);

void error(EErrorCode, const char *, ...);
void warning(EErrorCode, const char *, ...);
void fatal(EErrorCode, const char *, ...);
void info(EErrorCode, const char *, ...);

#endif
