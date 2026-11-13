/**
 * Project: openRender
 *
 * File: builtinIntegrators.cpp
 *
 * Description:
 *   The built-in integrators the process registry starts with: `classic`. See integratorRegistry.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "transport/classicIntegrator.h"
#include "transport/integratorRegistry.h"

void registerBuiltinIntegrators(CIntegratorRegistry &registry) {
    registry.add(orIntegratorTableClassic(), "built-in classic");
}
