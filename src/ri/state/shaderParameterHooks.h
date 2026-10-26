/**
 * Project: openRender
 *
 * File: shaderParameterHooks.h
 *
 * Description:
 *   Graphics State's shader-parameter hooks (spec 025, FR-005 to FR-007).
 *
 *   The set of parameters the bound shaders need (CAttributes::usedParameters)
 *   and the lookup of a shader parameter by name (CAttributes::findParameter)
 *   depend on what a shader is. State does not know that, so it asks through
 *   two function pointers that Shading implements and the renderer core
 *   installs when a session begins. The pattern is variableLookup.h's.
 *
 *   With no hooks installed (State-only programs, ribVector, ribOut) the
 *   answers are 0 and nullptr. No process that shades runs without them.
 *
 *   Only forward declarations here: no other domain's header.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADERPARAMETERHOOKS_H
#define SHADERPARAMETERHOOKS_H

class CAttributes;
class CVariable;

struct SShaderParameterHooks {
        // The OR of what the shaders bound in the block (and its lights) require.
        unsigned int (*usedParameters)(const CAttributes &);
        // The parameter called name of a bound shader, or nullptr.
        CVariable *(*findParameter)(const CAttributes &, const char *name);
};

// Installs the hooks (nullptr uninstalls). The pointed-to struct must outlive the installation.
// Installed and removed on one thread, with the renderer's lifetime
// (rendererShadingServicesInit / rendererShadingServicesShutdown).
void installShaderParameterHooks(const SShaderParameterHooks *hooks);

// The installed hooks, or nullptr when none are installed.
const SShaderParameterHooks *shaderParameterHooks();

#endif
