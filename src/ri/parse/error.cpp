/**
 * Project: openRender
 *
 * File: error.cpp
 *
 * Description:
 *   Routes the common diagnostics service (common/diagnostics.h) to the
 *   active RI context's error handler: translates diagnostic codes into RI
 *   error codes and installs the RI diagnostic sink while a context exists.
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
//  File				:	error.cpp
//  Classes				:	CRiDiagnosticSink
//  Description			:	Diagnostics routing into the RI error handler
//
////////////////////////////////////////////////////////////////////////
#include <stdarg.h>

#include "common/diagnostics.h"
#include "ri.h"
#include "riDiagnostics.h"
#include "riInterface.h"
#include "ri_config.h"

///////////////////////////////////////////////////////////////////////
// Function				:	translate
// Description			:	Translate an error code
// Return Value			:
// Comments				:
static int translate(EErrorCode c) {
    int code;

    switch (c) {
        case CODE_NOERROR:
            code = RIE_NOERROR;
            break;
        case CODE_NOMEM:
            code = RIE_NOMEM;
            break;
        case CODE_SYSTEM:
            code = RIE_SYSTEM;
            break;
        case CODE_NOFILE:
            code = RIE_NOFILE;
            break;
        case CODE_BADFILE:
            code = RIE_BADFILE;
            break;
        case CODE_VERSION:
            code = RIE_VERSION;
            break;
        case CODE_INCAPABLE:
            code = RIE_INCAPABLE;
            break;
        case CODE_OPTIONAL:
            code = RIE_OPTIONAL;
            break;
        case CODE_UNIMPLEMENT:
            code = RIE_UNIMPLEMENT;
            break;
        case CODE_LIMIT:
            code = RIE_LIMIT;
            break;
        case CODE_BUG:
            code = RIE_BUG;
            break;
        case CODE_NOTSTARTED:
            code = RIE_NOTSTARTED;
            break;
        case CODE_NESTING:
            code = RIE_NESTING;
            break;
        case CODE_NOTOPTIONS:
            code = RIE_NOTOPTIONS;
            break;
        case CODE_NOTATTRIBS:
            code = RIE_NOTATTRIBS;
            break;
        case CODE_NOTPRIMS:
            code = RIE_NOTPRIMS;
            break;
        case CODE_ILLSTATE:
            code = RIE_ILLSTATE;
            break;
        case CODE_BADMOTION:
            code = RIE_BADMOTION;
            break;
        case CODE_BADSOLID:
            code = RIE_BADSOLID;
            break;
        case CODE_BADTOKEN:
            code = RIE_BADTOKEN;
            break;
        case CODE_RANGE:
            code = RIE_RANGE;
            break;
        case CODE_CONSISTENCY:
            code = RIE_CONSISTENCY;
            break;
        case CODE_BADHANDLE:
            code = RIE_BADHANDLE;
            break;
        case CODE_NOSHADER:
            code = RIE_NOSHADER;
            break;
        case CODE_MISSINGDATA:
            code = RIE_MISSINGDATA;
            break;
        case CODE_SYNTAX:
            code = RIE_SYNTAX;
            break;
        case CODE_MATH:
            code = RIE_MATH;
            break;
        case CODE_LOG:
            code = RIE_LOG;
            break;
        case CODE_SCRIPT:
            code = RIE_SCRIPT;
            break;
        case CODE_PRINTF:
            code = RIE_PRINTF;
            break;
        case CODE_RESOLUTION:
            code = RIE_LOG;
            break;
        case CODE_STATS:
            code = RIE_STATS;
            break;
        case CODE_PROGRESS:
            code = RIE_PROGRESS;
            break;
        default:
            error(CODE_BUG, "Unknown error code used\n");
            code = RIE_NOERROR;
            break;
    }

    return code;
}

///////////////////////////////////////////////////////////////////////
// Function				:	toRIE
// Description			:	Map a diagnostic severity onto the RI severity
// Return Value			:
// Comments				:
static int toRIE(EDiagnosticSeverity severity) {
    switch (severity) {
        case DIAGNOSTIC_INFO:
            return RIE_INFO;
        case DIAGNOSTIC_WARNING:
            return RIE_WARNING;
        case DIAGNOSTIC_ERROR:
            return RIE_ERROR;
        case DIAGNOSTIC_SEVERE:
        default:
            return RIE_SEVERE;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRiDiagnosticSink
// Description			:	Forwards diagnostics to the active RI context's
//							error handler, exactly as error()/warning()/
//							fatal()/info() used to do themselves
// Comments				:	renderMan is read on every report: between the
//							context's destruction and the detach in RiEnd it
//							is NULL, and the default rules apply instead.
class CRiDiagnosticSink : public CDiagnosticSink {
    public:
        void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
            if (renderMan != NULL)
                renderMan->RiError(translate(code), toRIE(severity), message);
            else
                diagnosticsPrintDefault(severity, message);
        }
};

static CRiDiagnosticSink riSink;
static bool riSinkAttached = false;
static CDiagnosticSink *sinkBeforeAttach = NULL;

///////////////////////////////////////////////////////////////////////
// Function				:	riDiagnosticsAttach
// Description			:	Route diagnostics to the RI context
// Return Value			:
// Comments				:	Idempotent: called from every CRiInterface
//							constructor, it saves the previously installed
//							sink only the first time.
void riDiagnosticsAttach() {
    if (riSinkAttached)
        return;
    sinkBeforeAttach = installDiagnosticSink(&riSink);
    riSinkAttached = true;
}

///////////////////////////////////////////////////////////////////////
// Function				:	riDiagnosticsDetach
// Description			:	Restore the sink installed before the first
//							RI context
// Return Value			:
// Comments				:	Called only where renderMan becomes NULL (RiEnd).
void riDiagnosticsDetach() {
    if (!riSinkAttached)
        return;
    installDiagnosticSink(sinkBeforeAttach);
    sinkBeforeAttach = NULL;
    riSinkAttached = false;
}
