/**
 * Project: openRender
 *
 * File: hiderAbi.hpp
 *
 * Description:
 *   The internal C++ side of the hider registration (spec 030, contracts/hider-registration.md "C++ convenience"). Internal
 *   until spec 051: the reinterpret casts below are the one place a C handle becomes a C++ object.
 *
 *   OR_REGISTER_HIDER(getter) places a file-local static object whose constructor hands `getter` (a function returning the
 *   hider's table) to orHiderAddPending, defined once, out of line, in hiderRegistry.cpp: the pending list must not be a
 *   header-reached static, or a shared-library consumer would register into its own copy (gotcha 22's class of defect).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef HIDERABI_HPP
#define HIDERABI_HPP

#include "hiderAbi.h"
#include "hiderPeers.h"

class CAttributes;
class CHider;
namespace openrender {
    class ICamera;
}

// What the core hands a hider's create through OrHiderCreateInfo::frame.
struct SHiderFrame {
        const CHiderPeers *peers;                           // the frame's per-thread hiders, filled as they are constructed
        const openrender::ICamera *camera;                  // frameCamera()
        CAttributes *(*acquireWorldAttributes)(void *host); // context->getAttributes(TRUE); call only if needed
        void *host;
};

inline const SHiderFrame &frameOf(const OrHiderFrame *frame) {
    return *reinterpret_cast<const SHiderFrame *>(frame);
}
inline CHider *hiderOf(OrHiderInstance *instance) {
    return reinterpret_cast<CHider *>(instance);
}
inline OrHiderInstance *instanceOf(CHider *hider) {
    return reinterpret_cast<OrHiderInstance *>(hider);
}

typedef const OrHiderTable *(*OrHiderTableGetter)(void);

// Defined once, in hiderRegistry.cpp.
void orHiderAddPending(OrHiderTableGetter getter);

#define OR_HIDER_CONCAT_(a, b) a##b
#define OR_HIDER_CONCAT(a, b) OR_HIDER_CONCAT_(a, b)

#define OR_REGISTER_HIDER(getter)                                                            \
    namespace {                                                                              \
        struct OR_HIDER_CONCAT(SHiderRegistrar_, __LINE__) {                                 \
                OR_HIDER_CONCAT(SHiderRegistrar_, __LINE__)() { orHiderAddPending(getter); } \
        } OR_HIDER_CONCAT(hiderRegistrar_, __LINE__);                                        \
    }

#endif
