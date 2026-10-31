/**
 * Project: openRender
 *
 * File: stateConversions.h
 *
 * Description:
 *   Conversions the renderer core makes where Graphics State's vocabulary
 *   (stateVocabulary.h) meets the display plug-in ABI (dsply.h) and the blobby
 *   header (blobbyField.h) (spec 025, FR-009). State stores the type of a display parameter as an
 *   EDisplayParameterType; the plug-in ABI speaks OrDisplayParamType. The two carry
 *   the same values, and a static_assert per pair pins that at compile time. (The version 1 adapter
 *   asserts the same for its own enumeration in displayV1Adapter.cpp.)
 *
 *   Core-private: only the renderer core includes this header, because it is
 *   the one place that may name both vocabularies.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef STATECONVERSIONS_H
#define STATECONVERSIONS_H

#include "blobbyField.h"
#include "dsply.h"
#include "stateVocabulary.h"

static_assert(static_cast<int>(EDisplayParameterType::Float) == static_cast<int>(OR_DISPLAY_PARAM_FLOAT));
static_assert(static_cast<int>(EDisplayParameterType::Vector) == static_cast<int>(OR_DISPLAY_PARAM_VECTOR));
static_assert(static_cast<int>(EDisplayParameterType::Matrix) == static_cast<int>(OR_DISPLAY_PARAM_MATRIX));
static_assert(static_cast<int>(EDisplayParameterType::String) == static_cast<int>(OR_DISPLAY_PARAM_STRING));
static_assert(static_cast<int>(EDisplayParameterType::Integer) == static_cast<int>(OR_DISPLAY_PARAM_INTEGER));

static_assert(static_cast<int>(EBlobbyOrder::RiSpec) == static_cast<int>(BLOBBY_ORDER_RISPEC));
static_assert(static_cast<int>(EBlobbyOrder::AppNote) == static_cast<int>(BLOBBY_ORDER_APPNOTE));

// The plug-in ABI's type for a stored display-parameter type.
inline OrDisplayParamType toPluginType(EDisplayParameterType type) {
    return static_cast<OrDisplayParamType>(static_cast<int>(type));
}

// The stored type for the plug-in ABI's type.
inline EDisplayParameterType toStateType(OrDisplayParamType type) {
    return static_cast<EDisplayParameterType>(static_cast<int>(type));
}

#endif
