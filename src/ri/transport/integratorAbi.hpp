/**
 * Project: openRender
 *
 * File: integratorAbi.hpp
 *
 * Description:
 *   Static registration of integrators (spec 030, contracts/integrator-interface.md). C++20, internal.
 *
 *   OR_REGISTER_INTEGRATOR(getter) places a file-local static object whose constructor hands `getter` (a function that
 *   returns the integrator's table) to orIntegratorAddPending, defined once, out of line, in integratorRegistry.cpp:
 *   the pending list must not be a header-reached static, or a shared-library consumer would register into its own
 *   copy (gotcha 22's class of defect). integratorRegistry() takes the pending getters in append order after the
 *   built-in integrators.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef TRANSPORT_INTEGRATORABI_HPP
#define TRANSPORT_INTEGRATORABI_HPP

#include "transport/integratorAbi.h"

typedef const OrIntegratorTable *(*OrIntegratorTableGetter)(void);

// Defined once, in integratorRegistry.cpp.
void orIntegratorAddPending(OrIntegratorTableGetter getter);

#define OR_INTEGRATOR_CONCAT_(a, b) a##b
#define OR_INTEGRATOR_CONCAT(a, b) OR_INTEGRATOR_CONCAT_(a, b)

#define OR_REGISTER_INTEGRATOR(getter)                                                                      \
    namespace {                                                                                             \
        struct OR_INTEGRATOR_CONCAT(SIntegratorRegistrar_, __LINE__) {                                      \
                OR_INTEGRATOR_CONCAT(SIntegratorRegistrar_, __LINE__)() { orIntegratorAddPending(getter); } \
        } OR_INTEGRATOR_CONCAT(integratorRegistrar_, __LINE__);                                             \
    }

#endif
