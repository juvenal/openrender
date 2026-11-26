/**
 * Project: openRender
 *
 * File: closureStep.h
 *
 * Description:
 *   The surface step of CShadingContext::shade() (spec 035, contracts/closuremode.md "The surface step"), out of line so that shade() keeps
 *   three small edits: planSurfaceStep() (the mode, the capability, the consumer, the rule that applies), runSurfaceStep() (the surface
 *   execution of a non-color plan) and deliverClosures() (the batch record, handed to the consumer before the batch's arena is released).
 *
 *   A plan whose `buildTrees` is false is the color path: shade() runs today's code unchanged, which is also what IR-017 and `both` with no
 *   consumer run.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CLOSURESTEP_H
#define CLOSURESTEP_H

#include "closureConsumer.h"
#include "closureMode.h"
#include "closureTree.h"
#include "libshader/shading/shaderGlobals.h"

class CShaderInstance;
class CShadingContext;
class CMemPage;
class CAttributes;

namespace openrender::shading {

    class CBxdfInstance;

    struct SurfaceStepPlan {
            ClosureMode mode = ClosureMode::Color; // the mode the object runs in: Both or Closures when trees are built, else Color
            bool buildTrees = false;               // executeClosures runs and a tree per real point is built
            bool suppressLighting = false;         // the lighting gate is closed around executeClosures (`closures`)
            bool deliver = false;                  // a consumer exists: the trees are handed over
            IClosureConsumer *consumer = nullptr;
            ClosureTreeBuilder *builder = nullptr; // made by runSurfaceStep on the batch's arena
            const CBxdfInstance *bxdf = nullptr;   // a Bxdf governs and a consumer is installed: no shader runs, `build` makes the tree of every real point
            CShaderInstance *messageSurface = nullptr; // a Bxdf governs and a displacement or atmosphere shader passes messages: the Surface of the slot, which
                                                       // does not run but whose locals shade() prepares in ACCESSOR_SURFACE (RISpec 3.2 section 15.8)
    };

    // The geometry record a consumer's batch needs, ORed into usedParameters when trees are built (the interpolation then fills it)
    unsigned int closureBatchParameters();

    // `surface` may be null (no surface: today's path, whatever the mode). Warns through the interim rules; ORs closureBatchParameters()
    // into `usedParameters` only when the plan builds trees. When a Bxdf governs (Phase 4) the surface slot is not the one that runs: with no
    // consumer `surface` is replaced by the Bxdf's substitute (IR-026: the defaultsurface, or null for the no-shader path); with a consumer it is
    // set to null and the plan carries the Bxdf. The message-passing builtins still read the attributes' own Surface slot.
    SurfaceStepPlan planSurfaceStep(CShadingContext &context, const CAttributes &attributes, CShaderInstance *&surface, const ShaderGlobals &globals, unsigned int &usedParameters);

    // Runs the surface through executeClosures; with plan.bxdf, runs no shader: Bxdf::build per real point, Ci = 0, Oi = Os. Under `closures` the gate is closed around the call by a guard that opens it on every exit and
    // Ci is then zeroed (Oi stays what the shader wrote). Makes plan.builder on `arena`.
    void runSurfaceStep(CShadingContext &context, SurfaceStepPlan &plan, CShaderInstance *surface, float **locals, CMemPage *&arena);

    // Hands the batch's trees to the consumer, on the shading thread, before `arena` is released. A no-op unless plan.deliver.
    void deliverClosures(CShadingContext &context, const SurfaceStepPlan &plan, const ShaderGlobals &globals, CMemPage *&arena);

} // namespace openrender::shading

#endif
