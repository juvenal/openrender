/**
 * Project: openRender
 *
 * File: stateVocabulary.h
 *
 * Description:
 *   Graphics State's own vocabulary for two values it stores but does not
 *   define (spec 025, FR-008 and FR-009): the type of a display parameter and
 *   the blobby opcode order.
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

// Must equal FLOAT_PARAMETER ... INTEGER_PARAMETER of dsply.h.
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

#endif
