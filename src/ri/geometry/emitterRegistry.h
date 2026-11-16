/**
 * Project: openRender
 *
 * File: emitterRegistry.h
 *
 * Description:
 *   Spec 057 (area-light geometry), FR-001 to FR-005. The emitter registry records which primitives belong to which
 *   bound area light: one CEmitterSet per light declared by AreaLightSource, holding the light's ROOTS (the world-block
 *   objects created while the light was the current one) in RIB order. One registry per frame: the renderer core
 *   creates it in beginFrame() before any world-block statement, installs it, and releases it in endFrame() before the
 *   hierarchy is destroyed (the registry holds a reference to every root, as the frame's other resources do).
 *
 *   Geometry owns the type and never calls the renderer core: the core fills it (declareLight from RiAreaLightSourceV,
 *   record from addObject). Identity of a light is `const CShaderGroupRefTarget *` everywhere (Graphics State's base of the
 *   shader instance), never void *.
 *
 *   This is the recording half. CEmitterSet::resolve() (the expansion of procedurals, instances and solids into sampleable
 *   leaves, and the area tables) and sample() are declared for the sampling phase and are not defined yet.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef EMITTERREGISTRY_H
#define EMITTERREGISTRY_H

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "object.h"
#include "rayContext.h"
#include "stateHandles.h"
#include "stateVocabulary.h"

struct CAreaSample;

///////////////////////////////////////////////////////////////////////
// Class				:	CEmitterSet
// Description			:	The emitters of one bound area light
// Comments				:	Recording (declare, record) runs on the main thread during the world block; afterwards the
//						set is read-only except for its one-time resolution
class CEmitterSet {
    public:
        CEmitterSet(const CShaderGroupRefTarget *light, int lightSamples, EAreaLightMode lightMode)
            : light_(light), lightSamples_(lightSamples), lightMode_(lightMode) {}
        CEmitterSet(const CEmitterSet &) = delete;
        CEmitterSet &operator=(const CEmitterSet &) = delete;

        const CShaderGroupRefTarget *light() const { return light_; }
        int lightSamples() const { return lightSamples_; }      // the light level controls in force at the statement
        EAreaLightMode lightMode() const { return lightMode_; } // (0 / Unset when the statement's block set none)
        const std::vector<CObject *> &roots() const { return roots_; } // attached, in RIB order

        // Declared for the sampling phase, not defined yet: resolve once (std::call_once) the roots into leaves and tables;
        // draw a point uniformly by area.
        void resolve(IRayContext &context) const;
        double totalArea() const;
        void sample(double u1, double u2, CAreaSample &out) const;

        // Counters filled by resolution and reported once per light
        mutable int numUnsupported = 0; // primitives of the scope with no sampler
        mutable int numCsgLeaves = 0;   // leaves of solids, sampled without their region (D-4)

    private:
        friend class CEmitterRegistry;
        const CShaderGroupRefTarget *light_;
        int lightSamples_;
        EAreaLightMode lightMode_;
        std::vector<CObject *> roots_;
        mutable std::once_flag resolved_;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CEmitterRegistry
// Description			:	Every bound area light of the frame and its emitters
// Comments				:	declareLight() and record() run on the main thread, in the world block, before the frame renders
class CEmitterRegistry {
    public:
        CEmitterRegistry(int sceneSamples, EAreaLightMode sceneMode);
        ~CEmitterRegistry(); // detaches every root

        CEmitterRegistry(const CEmitterRegistry &) = delete;
        CEmitterRegistry &operator=(const CEmitterRegistry &) = delete;

        // A bound light: its light level controls as in force at the AreaLightSource statement. Declaring a light twice
        // keeps the first declaration.
        void declareLight(const CShaderGroupRefTarget *light, int samples, EAreaLightMode mode);

        // A world-block object created in a light's scope: root->attributes->areaLight names a declared light. The registry
        // attaches the root. An object whose light is not declared is ignored.
        void record(CObject *root);

        const CEmitterSet *find(const CShaderGroupRefTarget *light) const; // nullptr: not a bound light
        const CEmitterSet *lightAt(int index) const;                       // in statement order; nullptr out of range
        int numLights() const;                                             // declared bound lights this frame
        int sceneSamples() const { return sceneSamples_; }
        EAreaLightMode sceneMode() const { return sceneMode_; }

    private:
        int sceneSamples_;
        EAreaLightMode sceneMode_;
        std::vector<std::unique_ptr<CEmitterSet>> lights_;
        std::unordered_map<const CShaderGroupRefTarget *, int> index_;
};

// The core installs the frame's registry; nullptr uninstalls
void installEmitterRegistry(const CEmitterRegistry *registry);

// Never null: the installed registry, or an empty one with no lights
const CEmitterRegistry &emitterRegistry();

// A test hook, null in production: called once by the renderer core at the start of endFrame(), while the registry and its
// roots still exist, with the frame's registry (the precedent is the trace-request observer of spec 026). It is the only
// way an in-process test reads the per-frame registry.
using EmitterRegistryObserver = void (*)(const CEmitterRegistry &);
void setEmitterRegistryObserver(EmitterRegistryObserver observer);
EmitterRegistryObserver emitterRegistryObserver();

#endif
