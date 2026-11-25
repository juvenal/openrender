/**
 * Project: openRender
 *
 * File: closureStep.cpp
 *
 * Description:
 *   See closureStep.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "closureStep.h"

#include <cstring>
#include <new>

#include "closureFrame.h"
#include "closureInterim.h"
#include "common/memory.h"
#include "common/rendererc.h"
#include "libshader/shading/shader.h"
#include "libshader/shading/shaderBackend.h"
#include "libshader/shading/shading.h"

namespace openrender::shading {

    unsigned int closureBatchParameters() {
        return PARAMETER_P | PARAMETER_N | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_I | PARAMETER_U | PARAMETER_V;
    }

    SurfaceStepPlan planSurfaceStep(CShadingContext &context, ClosureMode attributeMode, CShaderInstance *surface, const ShaderGlobals &globals, unsigned int &usedParameters) {
        (void)globals;
        SurfaceStepPlan plan;

        // No surface (and, from Phase 4, no Bxdf): today's no-shader path, whatever the mode, with no diagnostic (E7)
        if (surface == nullptr)
            return plan;

        const ClosureFrame *frame = context.closureFrame();
        const ClosureMode effective = resolveClosureMode(attributeMode, frame);
        if (effective == ClosureMode::Color)
            return plan;

        ClosureFrame &reportFrame = frame != nullptr ? const_cast<ClosureFrame &>(*frame) : ClosureFrame::fallback();

        // IR-017: a surface that cannot emit closures renders as color
        const IShaderBackend *backend = surface->shaderDescription()->backend;
        if (!backend->emitsClosures(*surface)) {
            interimSurfaceNotClosureCapable(reportFrame, surface->getName(), effective);
            return plan;
        }

        IClosureConsumer *consumer = context.closureConsumer();
        if (effective == ClosureMode::Both) {
            if (consumer == nullptr)
                return plan; // `both` with no consumer takes the color path, silently (Q5b)
            plan.mode = ClosureMode::Both;
            plan.deliver = true;
        }
        else {
            plan.mode = ClosureMode::Closures;
            plan.suppressLighting = true;
            plan.deliver = consumer != nullptr;
            if (consumer == nullptr)
                interimClosuresWithoutConsumer(reportFrame); // IR-018: built, not delivered
        }
        plan.consumer = consumer;
        plan.buildTrees = true;
        usedParameters |= closureBatchParameters();
        return plan;
    }

    namespace {

        // Opens the lighting gate on every exit from the execution it brackets
        class LightingGate {
            public:
                explicit LightingGate(CShadingState *state, bool close) : state_(state), closed_(close) {
                    if (closed_)
                        state_->lightingSuppressed = 1;
                }
                ~LightingGate() {
                    if (closed_)
                        state_->lightingSuppressed = 0;
                }
                LightingGate(const LightingGate &) = delete;
                LightingGate &operator=(const LightingGate &) = delete;

            private:
                CShadingState *state_;
                bool closed_;
        };

    } // namespace

    void runSurfaceStep(CShadingContext &context, SurfaceStepPlan &plan, CShaderInstance *surface, float **locals, CMemPage *&arena) {
        CShadingState *state = context.currentShadingState;
        ClosureFrame *frame = const_cast<ClosureFrame *>(context.closureFrame());
        ClosureFrame &diagnosticsFrame = frame != nullptr ? *frame : ClosureFrame::fallback();

        void *memory = ralloc(sizeof(ClosureTreeBuilder), arena);
        plan.builder = new (memory) ClosureTreeBuilder(arena, state->numRealVertices, &diagnosticsFrame.diagnostics());

        {
            LightingGate gate(state, plan.suppressLighting);
            surface->shaderDescription()->backend->executeClosures(context, *surface, locals, *plan.builder);
        }

        // `closures` writes no colour: Ci = 0; Oi stays what the shader wrote (Q-R5)
        if (plan.suppressLighting)
            std::memset(state->varying[VARIABLE_CI], 0, sizeof(float) * 3 * state->numVertices);
    }

    void deliverClosures(CShadingContext &context, const SurfaceStepPlan &plan, const ShaderGlobals &globals, CMemPage *&arena) {
        if (!plan.deliver || plan.builder == nullptr)
            return;
        CShadingState *state = context.currentShadingState;
        const int n = state->numRealVertices;
        const OrClosureNode **trees = (const OrClosureNode **)ralloc(sizeof(const OrClosureNode *) * (n > 0 ? n : 1), arena);
        for (int i = 0; i < n; ++i)
            trees[i] = plan.builder->root(i);

        ClosureBatch batch;
        batch.numPoints = n;
        batch.trees = trees;
        batch.P = state->varying[VARIABLE_P];
        batch.N = state->varying[VARIABLE_N];
        batch.Ng = state->varying[VARIABLE_NG];
        batch.dPdu = state->varying[VARIABLE_DPDU];
        batch.dPdv = state->varying[VARIABLE_DPDV];
        batch.I = state->varying[VARIABLE_I];
        batch.u = state->varying[VARIABLE_U];
        batch.v = state->varying[VARIABLE_V];
        batch.object = context.objectHandle();
        batch.keys = state->pointKeys;
        batch.rayType = globals.rayType;
        batch.pathDepth = globals.pathDepth;
        batch.shape = globals.shape;
        batch.mode = plan.mode;
        batch.fromBxdf = false;
        plan.consumer->consume(context, batch);
    }

} // namespace openrender::shading
