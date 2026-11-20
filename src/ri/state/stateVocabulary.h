/**
 * Project: openRender
 *
 * File: stateVocabulary.h
 *
 * Description:
 *   Graphics State's own vocabulary for values it stores but does not
 *   define (spec 025, FR-008 and FR-009): the type of a display parameter and
 *   the blobby opcode order; and, since spec 057, the area-light mode.
 *
 *   The display plug-in ABI (dsply.h) and the blobby header (blobbyField.h)
 *   stay unchanged and keep their C enums. These scoped enums carry the same
 *   values, so State needs neither header. State_VocabularyConsistency
 *   (tests/unit/state) compares each pair at run time and names the first one
 *   that drifts.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef STATEVOCABULARY_H
#define STATEVOCABULARY_H

#include <cstdint>
#include <cstring>

// Must equal OR_DISPLAY_PARAM_FLOAT ... OR_DISPLAY_PARAM_INTEGER of dsply.h.
enum class EDisplayParameterType : int {
    Float = 0,
    Vector = 1,
    Matrix = 2,
    String = 3,
    Integer = 4
};

// Must equal BLOBBY_ORDER_RISPEC and BLOBBY_ORDER_APPNOTE of blobbyField.h.
enum class EBlobbyOrder : int {
    RiSpec = 0,
    AppNote = 1
};

// The area-light mode of the "arealight" attribute and option (spec 057, FR-018). Unset means "no value at this level"
// (the next level decides); Off at any level wins. Must equal the mirror in libshader/shading/areaLightSampler.h
// (EAreaLightMode there, added with the sampler interface); State_AreaLightVocabularyMirror compares the two value for value.
enum class EAreaLightMode : uint8_t {
    Unset,
    Full,
    Fast,
    Off
};

// The RIB spelling of a mode: "full", "fast" or "off"; nullptr for Unset (nothing to read back)
inline const char *areaLightModeName(EAreaLightMode mode) {
    switch (mode) {
        case EAreaLightMode::Full: return "full";
        case EAreaLightMode::Fast: return "fast";
        case EAreaLightMode::Off: return "off";
        case EAreaLightMode::Unset: break;
    }
    return nullptr;
}

// A mode from its RIB spelling; false (and `mode` untouched) for any other string. Unset has no spelling: it is the absence of the statement
inline bool areaLightModeFromName(const char *name, EAreaLightMode &mode) {
    if (name == nullptr)
        return false;
    if (strcmp(name, "full") == 0)
        mode = EAreaLightMode::Full;
    else if (strcmp(name, "fast") == 0)
        mode = EAreaLightMode::Fast;
    else if (strcmp(name, "off") == 0)
        mode = EAreaLightMode::Off;
    else
        return false;
    return true;
}

// Which implementation decodes and bakes images (spec 048). The core maps it to Texturing's EImageBackend with a switch
// and a static_assert that the two have the same enumerators and values (renderer.cpp).
enum class EImageIOBackend : int {
    Legacy = 0,
    OIIO = 1
};

#endif
