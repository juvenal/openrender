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

#include <cmath>
#include <cstring>
#include <new>

#include "bxdfInstance.h"
#include "closureFrame.h"
#include "closureInterim.h"
#include "common/memory.h"
#include "common/rendererc.h"
#include "libshader/shading/shader.h"
#include "libshader/shading/shaderRefs.h"
#include "libshader/shading/shaderBackend.h"
#include "libshader/shading/shading.h"
#include "ri/state/attributes.h"

namespace openrender::shading {

    unsigned int closureBatchParameters() {
        return PARAMETER_P | PARAMETER_N | PARAMETER_NG | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_I | PARAMETER_U | PARAMETER_V;
    }

    SurfaceStepPlan planSurfaceStep(CShadingContext &context, const CAttributes &attributes, CShaderInstance *&surface, const ShaderGlobals &globals, unsigned int &usedParameters) {
        (void)globals;
        SurfaceStepPlan plan;
        const ClosureMode attributeMode = static_cast<ClosureMode>(attributes.closureMode);

        // A Bxdf governs (the last-bound record says so and the slot holds a Bxdf instance: a foreign object in the slot never selects this path).
        // The mode is ignored and the Surface slot does not run (the rows of contracts/closuremode.md "The plan").
        if (attributes.governingMaterial() == EMaterialBinding::Bxdf) {
            if (const CBxdfInstance *bxdf = bxdfInstanceOf(attributes.bxdf())) {
                const ClosureFrame *bxdfFrame = context.closureFrame();
                IClosureConsumer *bxdfConsumer = context.closureConsumer();
                // The Surface of the slot does not run while a Bxdf governs, but the message-passing builtins still read it (E12)
                if (attributes.usedParameters & PARAMETER_MESSAGEPASSING)
                    plan.messageSurface = shaderInstanceOf(attributes.surface());
                if (bxdfConsumer == nullptr) {
                    // IR-026: the substitute surface renders, on the colour path
                    interimBxdfWithoutConsumer(bxdfFrame != nullptr ? const_cast<ClosureFrame &>(*bxdfFrame) : ClosureFrame::fallback(), bxdf->table->name);
                    surface = interimSurfaceFor(*bxdf);
                    return plan;
                }
                plan.mode = ClosureMode::Closures;
                plan.buildTrees = true;
                plan.deliver = true;
                plan.consumer = bxdfConsumer;
                plan.bxdf = bxdf;
                surface = nullptr;
                usedParameters |= closureBatchParameters() | PARAMETER_CS | PARAMETER_OS;
                return plan;
            }
        }

        // No surface: today's no-shader path, whatever the mode, with no diagnostic (E7)
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

        // The batch arena as the allocator a Bxdf's `build` writes through
        void *arenaAllocate(void *context, size_t size, size_t align) {
            CMemPage *&page = *static_cast<CMemPage **>(context);
            char *raw = static_cast<char *>(ralloc((int)(size + align), page));
            return reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(raw) + (align - 1)) & ~static_cast<uintptr_t>(align - 1));
        }

        // A Bxdf governs and a consumer is installed (contracts/closuremode.md, the Bxdf row): no shader runs. The table's `build` makes the tree of
        // every real point from the point's own varying (declarative: no ray, no light); Ci = 0 and Oi = Os (the primitive's Os when it gives
        // one, else the attribute's: interpolation filled VARIABLE_OS because the plan asked for PARAMETER_OS).
        void runBxdfStep(CShadingState *state, SurfaceStepPlan &plan, CMemPage *&arena) {
            const OrBxdfTable *table = plan.bxdf->table;
            OrClosureAllocator alloc = {&arena, arenaAllocate};
            for (int i = 0; i < state->numRealVertices; ++i) {
                OrBxdfPoint point;
                std::memset(&point, 0, sizeof(point));
                point.structSize = sizeof(point);
                std::memcpy(point.P, state->varying[VARIABLE_P] + 3 * i, sizeof(point.P));
                std::memcpy(point.Ng, state->varying[VARIABLE_NG] + 3 * i, sizeof(point.Ng));
                std::memcpy(point.dPdu, state->varying[VARIABLE_DPDU] + 3 * i, sizeof(point.dPdu));
                std::memcpy(point.dPdv, state->varying[VARIABLE_DPDV] + 3 * i, sizeof(point.dPdv));
                std::memcpy(point.I, state->varying[VARIABLE_I] + 3 * i, sizeof(point.I));
                point.u = state->varying[VARIABLE_U][i];
                point.v = state->varying[VARIABLE_V][i];
                std::memcpy(point.Cs, state->varying[VARIABLE_CS] + 3 * i, sizeof(point.Cs));
                std::memcpy(point.Os, state->varying[VARIABLE_OS] + 3 * i, sizeof(point.Os));
                // N is faceforward(normalize(N), I), as every Appendix A surface takes it: I points from the eye to the surface
                const float *N = state->varying[VARIABLE_N] + 3 * i;
                const float length = std::sqrt(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
                const float scale = length > 0.0f ? 1.0f / length : 1.0f;
                float sign = (N[0] * point.I[0] + N[1] * point.I[1] + N[2] * point.I[2]) > 0.0f ? -1.0f : 1.0f;
                for (int k = 0; k < 3; ++k)
                    point.N[k] = sign * scale * N[k] + 0.0f; // + 0.0f: a flipped zero is +0, so equal normals have equal bytes (trees are hashed and compared bit for bit)
                plan.builder->setRoot(i, table->build(plan.bxdf->data, &point, &alloc));
            }
            std::memset(state->varying[VARIABLE_CI], 0, sizeof(float) * 3 * state->numVertices);
            std::memcpy(state->varying[VARIABLE_OI], state->varying[VARIABLE_OS], sizeof(float) * 3 * state->numVertices);
        }

    } // namespace

    void runSurfaceStep(CShadingContext &context, SurfaceStepPlan &plan, CShaderInstance *surface, float **locals, CMemPage *&arena) {
        CShadingState *state = context.currentShadingState;
        ClosureFrame *frame = const_cast<ClosureFrame *>(context.closureFrame());
        ClosureFrame &diagnosticsFrame = frame != nullptr ? *frame : ClosureFrame::fallback();

        void *memory = ralloc(sizeof(ClosureTreeBuilder), arena);
        plan.builder = new (memory) ClosureTreeBuilder(arena, state->numRealVertices, &diagnosticsFrame.diagnostics());

        if (plan.bxdf != nullptr) {
            runBxdfStep(state, plan, arena);
            return;
        }

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
        batch.fromBxdf = plan.bxdf != nullptr;
        plan.consumer->consume(context, batch);
    }

} // namespace openrender::shading
