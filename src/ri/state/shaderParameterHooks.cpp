/**
 * Project: openRender
 *
 * File: shaderParameterHooks.cpp
 *
 * Description:
 *   Implements the Graphics State shader-parameter hooks.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "shaderParameterHooks.h"

// Installed and removed on one thread, with the renderer's lifetime.
static const SShaderParameterHooks *installedHooks = nullptr;

void installShaderParameterHooks(const SShaderParameterHooks *hooks) {
    installedHooks = hooks;
}

const SShaderParameterHooks *shaderParameterHooks() {
    return installedHooks;
}
