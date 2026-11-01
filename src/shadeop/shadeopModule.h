/**
 * Project: openRender
 *
 * File: shadeopModule.h
 *
 * Description:
 *   The Shader DSO Operands module (spec 037, FR-011 to FR-013, FR-033, FR-034; contracts/shadeop.md
 *   section 3): the one place that scans shadeop modules, looks up the <name>_shadeops table, checks
 *   the interface version, parses the declarations and owns the optional per-thread state. The
 *   renderer (CRenderer::getDSO) and the shader compiler (CScriptContext::enumerateDso) both go
 *   through it. Domain: ShaderDSO; it depends on its own headers and Infra (common/) only.
 *
 *   Scan rule. A search path is a list of directories, each ending with a separator (as a
 *   TSearchpath holds it; a missing one is added). Directories are visited in search-path order and,
 *   inside one, the module files in lexicographic order of their names, never in the order the file
 *   system enumerates them (FR-013). For each module: checkPluginVersion first (a Reject skips the
 *   module with its diagnostic and none of its entry points is entered), then the table
 *   <name>_shadeops. A row whose prototype matches resolves its exec symbol into the row's own
 *   variable and tests that one: an unresolved exec prints "shadeop <module>: <symbol> not found"
 *   once and the scan goes on to the next row and the next module (FR-033). A module that supplies
 *   nothing is unloaded at once.
 *
 *   Per-thread state (interface 1.1). A module of version 1.1 or newer may export
 *   <name>_shadeop_threads[], parallel to the table. For such an operand shadeopOperandData(op,
 *   thread) calls the row's threadInit(initdata, thread) lazily, once per thread index, with no lock
 *   on the hot path, and returns the data it made; the data lasts until the registry is released,
 *   where threadCleanup runs once per initialized slot, then cleanup, then the module is unloaded.
 *   A NULL result of threadInit is a value like any other. The slot arrays grow in beginFrame() when
 *   the thread count rises and are never shrunk. An operand without the entry points hands every
 *   thread the one initdata of its process-wide init.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef SHADEOPMODULE_H
#define SHADEOPMODULE_H

#include <functional>

#include "shadeop/shadeop.h"
#include "shadeop/shadeopDeclaration.h"

// The caller's search path; the strings must outlive the call.
struct SShadeopSearchPath {
        const char *const *directories;
        int count;
};

// Compiler: every declaration of every "<name>_shadeops" table found on the path, in scan order. The
// modules are version-checked (a rejected one is skipped with its diagnostic), and each one is loaded
// and unloaded within the scan.
void enumerateShadeopDeclarations(const SShadeopSearchPath &path, const char *name, const std::function<void(const SShadeopDeclaration &)> &visit);

// One operand the registry selected: its module stays loaded until the registry is released.
class CShadeopOperand;

// Called for every DSO call by the interpreter (DSOEXEC_PRE): the data to pass to exec for the shading
// thread `thread` (the per-thread slot, made lazily, or the process-wide initdata), and the exec function.
void *shadeopOperandData(CShadeopOperand *operand, int thread);
dsoExecFunction shadeopOperandExec(const CShadeopOperand *operand);

// A host object attached to an operand (the renderer keeps the shading library's CDSO here): destroyed by the
// registry when the operand is released, after its cleanup and before the module is unloaded.
void *shadeopOperandHostObject(const CShadeopOperand *operand);
void setShadeopOperandHostObject(CShadeopOperand *operand, void *object, void (*destroy)(void *));

// Owned by the caller (the renderer core: one per RiBegin..RiEnd).
class CShadeopRegistry {
    public:
        CShadeopRegistry();
        ~CShadeopRegistry(); // per operand: threadCleanup for every initialized slot, then cleanup, then the module is unloaded

        CShadeopRegistry(const CShadeopRegistry &) = delete;
        CShadeopRegistry &operator=(const CShadeopRegistry &) = delete;

        // Called by the core in CRenderer::beginFrame, before any shading thread exists: grows every
        // operand's slot array to the largest thread count seen (a per-frame option).
        void beginFrame(int numThreads);

        // The first operand whose definition parses to (name, prototype) and whose exec symbol resolves
        // (FR-013, FR-033), sized from the current frame's thread count; NULL if none. Cached by (name,
        // prototype).
        CShadeopOperand *acquire(const SShadeopSearchPath &path, const char *name, const char *prototype);

    private:
        struct SImpl;
        SImpl *impl;
};

#endif
