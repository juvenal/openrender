/**
 * Project: openRender
 *
 * File: shaderBind.cpp
 *
 * Description:
 *   What a shader instance needs at bind time from the renderer, asked through the services (spec 033, FR-009, GitHub #134):
 *   the named coordinate systems of its parameter defaults (bindCoordinateSystem), and the dispatch to the init of its format
 *   (initialiseInstance). The core no longer holds the init: it calls initialiseInstance() where it used to call its own
 *   CRendererContext::init().
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <atomic>

#include "common/algebra.h"
#include "common/diagnostics.h"
#include "rendererServices.h"
#include "serviceLookup.h"
#include "serviceTypes.h"
#include "shader.h"
#include "shaderBackend.h"
#include "shading.h" // CShadingContext::defaultServices(), shaderHandle()

namespace libshader {

    int bindCoordinateSystem(const char *name, const CShaderInstance *instance, const float *&from, const float *&to, ECoordinateSystem &kind) {
        using namespace openrender::shading;

        // The answer for a name nobody knows, and the default while the question is open
        from = identityMatrix;
        to = identityMatrix;
        kind = COORDINATE_CUSTOM;

        // One "service unavailable" warning per process for this kind of lookup (the pattern of serviceLookup.cpp)
        static std::atomic<bool> s_warnedUnavailable{false};

        CRendererServices *svc = CShadingContext::defaultServices();
        MatrixAnswer answer;
        if (svc != nullptr) {
            MatrixRequest request{name, ObjectHandle{}, CShadingContext::shaderHandle(instance), ShaderHandle{}, 0.0f};
            request.phase = EMatrixPhase::Bind;
            answer = svc->getMatrix(request);
        }

        if ((svc == nullptr || answer.status == EQueryStatus::Unavailable) && !s_warnedUnavailable.exchange(true))
            warning(CODE_NOTSTARTED, "renderer services do not provide coordinate systems: getMatrix() lookup of \"%s\" returns not found\n", name);

        if (answer.status != EQueryStatus::Found || answer.from == nullptr || answer.to == nullptr)
            return FALSE; // NotFound (and Unavailable, NoObject): the identity, silently (P-Q2)

        from = answer.from;
        to = answer.to;
        kind = answer.kind;
        return TRUE;
    }

    void initialiseInstance(CProgrammableShaderInstance &instance) {
        instance.parent->backend->initialise(instance);
    }

} // namespace libshader
