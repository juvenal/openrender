/**
 * Project: openRender
 *
 * File: diagnostics.cpp
 *
 * Description:
 *   The diagnostics service: an installable, process-wide sink and the
 *   error/warning/fatal/info reporting functions. Moved from the Scene
 *   Interface's error.cpp (spec 023), which now only installs the sink that
 *   forwards to the active RI context. With no sink installed, diagnostics
 *   follow RiErrorPrint's rules instead of dereferencing a missing context.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <atomic>

#include "diagnostics.h"
#include "logging.hpp"
#include "os.h"

// The one sink slot of the process (this file is built only into
// libRIBCommon). Constant-initialized, so reporting works during static
// initialization and teardown; atomic, so worker threads read it while
// another thread installs or restores a sink.
static std::atomic<CDiagnosticSink *> installedSink{nullptr};

///////////////////////////////////////////////////////////////////////
// Function				:	installDiagnosticSink
// Description			:	Install a sink, returning the previous one
// Return Value			:	The previously installed sink (nullptr = default)
// Comments				:
CDiagnosticSink *installDiagnosticSink(CDiagnosticSink *sink) {
    return installedSink.exchange(sink);
}

///////////////////////////////////////////////////////////////////////
// Function				:	diagnosticsPrintDefault
// Description			:	RiErrorPrint's stream rules, implemented once
// Return Value			:
// Comments				:	RiErrorPrint calls this after recording
//							RiLastError, so the two cannot drift apart.
void diagnosticsPrintDefault(EDiagnosticSeverity severity, const char *message) {
    if (severity == DIAGNOSTIC_SEVERE) {
        // Severe errors must still abort
        fprintf(stderr, "%s", message);
        fflush(stderr);
        exit(-1);
    }
    else if (severity == DIAGNOSTIC_ERROR) {
        fprintf(stderr, "%s", message);
        fflush(stderr);
    }
    else if (severity == DIAGNOSTIC_WARNING) {
        // RIB-level issues: always print regardless of log level
        fprintf(stdout, "%s", message);
        fflush(stdout);
    }
    else {
        // Algorithm/geometry events: respect the -x log level. Printed only
        // when -x 3 (INFO) or -x 4 (DEBUG) is passed.
        if (current_log_level <= LogLevel::INFO) {
            fprintf(stdout, "%s", message);
            fflush(stdout);
        }
    }
}

// Format a diagnostic exactly as the RI-routed functions always have (one
// OS_MAX_PATH_LENGTH buffer, vsnprintf truncation) and hand it to the sink.
static void dispatch(EErrorCode code, EDiagnosticSeverity severity, const char *mes, va_list args) {
    char tmp[OS_MAX_PATH_LENGTH];
    vsnprintf(tmp, sizeof(tmp), mes, args);

    CDiagnosticSink *sink = installedSink.load();
    if (sink != nullptr)
        sink->report(code, severity, tmp);
    else
        diagnosticsPrintDefault(severity, tmp);
}

///////////////////////////////////////////////////////////////////////
// Function				:	error
// Description			:	Generate an error message
// Return Value			:
// Comments				:
void error(EErrorCode code, const char *mes, ...) {
    va_list args;

    va_start(args, mes);
    dispatch(code, DIAGNOSTIC_ERROR, mes, args);
    va_end(args);
}

///////////////////////////////////////////////////////////////////////
// Function				:	warning
// Description			:	Generate a warning message
// Return Value			:
// Comments				:
void warning(EErrorCode code, const char *mes, ...) {
    va_list args;

    va_start(args, mes);
    dispatch(code, DIAGNOSTIC_WARNING, mes, args);
    va_end(args);
}

///////////////////////////////////////////////////////////////////////
// Function				:	fatal
// Description			:	Generate a fatal message
// Return Value			:
// Comments				:
void fatal(EErrorCode code, const char *mes, ...) {
    va_list args;

    va_start(args, mes);
    dispatch(code, DIAGNOSTIC_SEVERE, mes, args);
    va_end(args);
}

///////////////////////////////////////////////////////////////////////
// Function				:	info
// Description			:	Generate an info message
// Return Value			:
// Comments				:
void info(EErrorCode code, const char *mes, ...) {
    va_list args;

    va_start(args, mes);
    dispatch(code, DIAGNOSTIC_INFO, mes, args);
    va_end(args);
}
