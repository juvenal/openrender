/**
 * Project: openRender
 *
 * File: proceduralModules.h
 *
 * Description:
 *   The Procedures module (spec 037, FR-022 to FR-024, FR-032, FR-034, FR-044): the one place that
 *   loads, checks and releases dlo modules, implicit-surface modules and RenderMan DynamicLoad
 *   generators.
 *
 *   Each acquire is osLoadModule, then checkPluginVersion (common/pluginAbi.h), then resolution of
 *   the family's symbols, and returns an attached reference or NULL with the diagnostics printed.
 *   One loaded module exists per resolved path while it is referenced; acquiring it from several
 *   threads at once yields one load and one version check. The last detach() unloads the module.
 *
 *   A DLO or implicit instance holds its module until the instance is destroyed. A DynamicLoad
 *   generator's module is held by one frame reference taken at its first expansion in a frame and
 *   dropped by releaseFrameProceduralModules(), which the renderer core calls right after
 *   root->destroy() in CRenderer::endFrame: every procedural a generator's Subdivide created (with
 *   function pointers into the module) is gone by then, so that is the first safe point.
 *
 *   Domain: Procedures. Depends on its own headers and Infra only; compiled into ribRender.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef PROCEDURALMODULES_H
#define PROCEDURALMODULES_H

#include <string>

#include "common/refCounter.h"
#include "dlo.h"
#include "implicit.h"

// The signatures of the RenderMan DynamicLoad entry points (RtPointer is `const void *`).
typedef const void *(*TConvertParameters)(char *);
typedef void (*TSubdivide)(const void *, float);
typedef void (*TFree)(const void *);

// A loaded procedural module. attach() and detach() are guarded by the module registry's lock, so a
// reference cannot be taken from a module whose last reference is being dropped.
class CProceduralModule : public CRefCounter {
    public:
        const char *path() const { return resolvedPath.c_str(); }

        void attach();
        void detach(); // the last reference unloads the module

    protected:
        CProceduralModule(void *moduleHandle, const std::string &key);
        virtual ~CProceduralModule();

    private:
        void *handle;
        std::string resolvedPath;
        std::string registryKey;
};

class CDloModule : public CProceduralModule {
    public:
        dloInitFunction init;
        dloIntersectFunction intersect;
        dloTiniFunction tini;

    private:
        friend CDloModule *acquireDloModule(const char *, const char *);
        CDloModule(void *h, const std::string &key) : CProceduralModule(h, key), init(nullptr), intersect(nullptr), tini(nullptr) {}
};

class CImplicitModule : public CProceduralModule {
    public:
        implicitInitFunction init;
        implicitEvalFunction eval;
        implicitEvalNormalFunction evalNormal; // may be NULL
        implicitTiniFunction tini;

    private:
        friend CImplicitModule *acquireImplicitModule(const char *, const char *);
        CImplicitModule(void *h, const std::string &key) : CProceduralModule(h, key), init(nullptr), eval(nullptr), evalNormal(nullptr), tini(nullptr) {}
};

class CDynamicLoadModule : public CProceduralModule {
    public:
        TConvertParameters convertParameters; // each may be NULL
        TSubdivide subdivide;
        TFree free;

    private:
        friend CDynamicLoadModule *acquireDynamicLoadModule(const char *);
        CDynamicLoadModule(void *h, const std::string &key) : CProceduralModule(h, key), convertParameters(nullptr), subdivide(nullptr), free(nullptr) {}
};

// Each: load the module -> checkPluginVersion(family) -> resolve -> an attached reference, or NULL with
// the diagnostics printed (a failed load, a rejected version, a missing symbol, each naming `displayName`,
// the name as the RIB wrote it). Thread-safe. The caller detach()es a DLO or implicit module when its
// instance is destroyed.
CDloModule *acquireDloModule(const char *locatedPath, const char *displayName);
CImplicitModule *acquireImplicitModule(const char *locatedPath, const char *displayName);

// Takes a frame reference (the generator name is loaded exactly as written, FR-024): the caller does NOT
// detach; the reference is dropped by releaseFrameProceduralModules(). Expanding the same generator again
// in the same frame reuses the module and the frame reference.
CDynamicLoadModule *acquireDynamicLoadModule(const char *generator);

// Drops every frame reference taken by acquireDynamicLoadModule since the last call.
void releaseFrameProceduralModules();

// Test introspection: counts of modules loaded and unloaded since the process started.
struct SProceduralModuleStats {
        int loaded;
        int unloaded;
};
SProceduralModuleStats proceduralModuleStats();

#endif
