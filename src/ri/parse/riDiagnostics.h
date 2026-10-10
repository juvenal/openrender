/**
 * Project: openRender
 *
 * File: riDiagnostics.h
 *
 * Description:
 *   Attaches the common diagnostics service to the RI context: while an RI
 *   context exists, error()/warning()/fatal()/info() reach its error handler
 *   (spec 023).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef RIDIAGNOSTICS_H
#define RIDIAGNOSTICS_H

// Install the RI diagnostic sink, saving the previously installed sink.
// Idempotent: only the first call after a detach saves and installs.
void riDiagnosticsAttach();

// Restore the sink installed before the first attach. Called where the last
// RI context ends (renderMan becomes NULL).
void riDiagnosticsDetach();

#endif
