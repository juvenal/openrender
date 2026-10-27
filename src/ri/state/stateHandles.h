/**
 * Project: openRender
 *
 * File: stateHandles.h
 *
 * Description:
 *   Graphics State's opaque handles (spec 025, FR-001 to FR-004).
 *
 *   An attribute block holds shaders and photon maps that belong to other
 *   domains (Shading, Hiders). State never inspects them: it holds them
 *   through reference-counted handles to two empty base classes that those
 *   domains derive from, CShaderGroupRefTarget (implemented by CShaderInstance)
 *   and CMapRefTarget (implemented by CPhotonMap).
 *
 *   The domain that owns a target resolves the handle back to its own type with
 *   an inline static_cast (shaderRefs.h in Shading, photonMap.h in Hiders).
 *   State never casts a target.
 *
 *   Header-only on purpose: every operation is inline, so State exports no
 *   out-of-line symbol that Shading or Hiders would have to import.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef STATEHANDLES_H
#define STATEHANDLES_H

#include <utility>

#include "common/refCounter.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CShaderGroupRefTarget
// Description			:	Anything a shader slot can hold: the base of CShaderInstance.
//							Adds no data members.
// Comments				:
class CShaderGroupRefTarget : public CRefCounter {
    protected:
        CShaderGroupRefTarget() = default;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CMapRefTarget
// Description			:	Anything a map slot can hold: a base of CPhotonMap.
//							Adds no data members.
// Comments				:
class CMapRefTarget : public CRefCounter {
    protected:
        CMapRefTarget() = default;
};

///////////////////////////////////////////////////////////////////////
// Class				:	TStateRef
// Description			:	A reference-counted handle. A non-empty handle owns exactly one
//							reference to its object; an empty handle owns nothing.
// Comments				:	adopt() takes over a reference the caller already holds (no count
//							change); retain() adds one. A null pointer gives an empty handle.
template <class T>
class TStateRef {
    public:
        TStateRef() noexcept = default;

        static TStateRef adopt(T *object) noexcept {
            TStateRef handle;
            handle.object = object;
            return handle;
        }

        static TStateRef retain(T *object) noexcept {
            if (object != nullptr)
                object->attach();
            return adopt(object);
        }

        TStateRef(const TStateRef &other) noexcept : object(other.object) {
            if (object != nullptr)
                object->attach();
        }

        TStateRef(TStateRef &&other) noexcept : object(other.object) { other.object = nullptr; }

        // Retains the new object before it releases the old one, so assigning a handle to the
        // object it already holds (or to a copy of itself) never frees it on the way.
        TStateRef &operator=(const TStateRef &other) noexcept {
            T *const incoming = other.object;
            if (incoming != nullptr)
                incoming->attach();
            T *const outgoing = object;
            object = incoming;
            if (outgoing != nullptr)
                outgoing->detach();
            return *this;
        }

        TStateRef &operator=(TStateRef &&other) noexcept {
            if (this != &other) {
                T *const outgoing = object;
                object = other.object;
                other.object = nullptr;
                if (outgoing != nullptr)
                    outgoing->detach();
            }
            return *this;
        }

        ~TStateRef() {
            if (object != nullptr)
                object->detach();
        }

        void reset() noexcept {
            T *const outgoing = object;
            object = nullptr;
            if (outgoing != nullptr)
                outgoing->detach();
        }

        T *get() const noexcept { return object; }

        explicit operator bool() const noexcept { return object != nullptr; }

        friend bool operator==(const TStateRef &a, const TStateRef &b) noexcept { return a.object == b.object; }
        friend bool operator==(const TStateRef &a, const T *b) noexcept { return a.object == b; }

    private:
        T *object = nullptr;
};

// Distinct types: a shader handle does not convert to a map handle, or back.
using ShaderGroupRef = TStateRef<CShaderGroupRefTarget>;
using MapRef = TStateRef<CMapRefTarget>;

#endif
