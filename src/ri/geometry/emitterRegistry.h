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
#include <string>
#include <unordered_map>
#include <vector>

#include "areaSampling.h"
#include "object.h"
#include "rayContext.h"
#include "stateHandles.h"
#include "stateVocabulary.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CEmitterLeaf
// Description			:	One sampleable leaf of a light: its surface, its area and its own table
struct CEmitterLeaf {
        CSurface *surface; // attached by the set until the frame ends
        double area;
        CAreaTable table;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CEmitterSet
// Description			:	The emitters of one bound area light
// Comments				:	Recording (declare, record) runs on the main thread during the world block; afterwards the
//						set is read-only except for its one-time resolution
class CEmitterSet {
    public:
        CEmitterSet(const CShaderGroupRefTarget *light, int lightSamples, EAreaLightMode lightMode, const char *name = nullptr)
            : light_(light), lightSamples_(lightSamples), lightMode_(lightMode), name_(name != nullptr ? name : "") {}
        ~CEmitterSet(); // lets go of the leaves
        CEmitterSet(const CEmitterSet &) = delete;
        CEmitterSet &operator=(const CEmitterSet &) = delete;

        const CShaderGroupRefTarget *light() const { return light_; }
        int lightSamples() const { return lightSamples_; }      // the light level controls in force at the statement
        EAreaLightMode lightMode() const { return lightMode_; } // (0 / Unset when the statement's block set none)
        const char *name() const { return name_.c_str(); }      // the light shader's name, for the reports
        const std::vector<CObject *> &roots() const { return roots_; } // attached, in RIB order

        // Resolve once (std::call_once, any thread): collectEmitters over the roots in RIB order, build the leaf cdf, report
        // (once per light) the primitives with no sampler, the CSG leaves (D-4) and an empty set (A7). Everything below reads
        // immutable tables afterwards.
        void resolve(IRayContext &context) const;

        // After resolve(): the total area of the emitters (camera space, shutter open), and the leaves in collection order
        double totalArea() const { return totalArea_; }
        int numLeaves() const { return static_cast<int>(leaves_.size()); }
        const CSurface *leafSurface(int i) const { return leaves_[i].surface; }
        double leafArea(int i) const { return leaves_[i].area; }

        // A point uniformly by area over every leaf, a pure function of (u1, u2): u1 chooses the leaf in the cdf and is
        // rescaled in double, then the leaf's table places the point. pdfA = 1 / totalArea(); an empty set (or one not yet
        // resolved) gives pdfA = 0 and no leaf. Leaves and faces of zero area are never chosen.
        void sample(double u1, double u2, CAreaSample &out) const;

        // Counters filled by resolution and reported once per light
        mutable int numUnsupported = 0; // primitives of the scope with no sampler
        mutable int numCsgLeaves = 0;   // leaves of solids, sampled without their region (D-4)

    private:
        friend class CEmitterRegistry;
        const CShaderGroupRefTarget *light_;
        int lightSamples_;
        EAreaLightMode lightMode_;
        std::string name_;
        std::vector<CObject *> roots_;
        mutable std::once_flag resolved_;
        mutable std::vector<CEmitterLeaf> leaves_;
        mutable std::vector<double> cdf_;
        mutable double totalArea_ = 0.0;
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
        void declareLight(const CShaderGroupRefTarget *light, int samples, EAreaLightMode mode, const char *name = nullptr);

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
