/**
 * Project: openRender
 *
 * File: test_state_vocabulary.cpp
 *
 * Description:
 *   Spec 025 (graphics-state-handles), FR-008, FR-009, SC-007: the State
 *   vocabulary stays in step with the display plug-in ABI and the blobby
 *   header (State_VocabularyConsistency), modelled on
 *   Common_VocabularyConsistency.
 *
 *   State owns EDisplayParameterType and EBlobbyOrder (stateVocabulary.h)
 *   instead of including dsply.h and blobbyField.h. This test is a Consumer:
 *   it includes all three, so it uses the normal include path, and it
 *   compares all seven pairs at run time. It prints the first drifted pair by
 *   name.
 *
 *   Limitation (research R9): neither C enum has a count sentinel, so an
 *   enumerator ADDED to dsply.h or blobbyField.h is not detected. dsply.h is
 *   frozen in spec 025.
 *
 *   Red on the starting tree by build failure: stateVocabulary.h does not
 *   exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdio>

#include "blobbyField.h"
#include "dsply.h"
#include "stateVocabulary.h"

namespace {

    struct SPair {
            const char *state; // the State enumerator
            int stateValue;    // its value
            const char *other; // the enumerator it must equal
            int otherValue;    // its value
    };

} // namespace

int main() {
    const SPair pairs[] = {
        {"EDisplayParameterType::Float", static_cast<int>(EDisplayParameterType::Float), "FLOAT_PARAMETER",
         static_cast<int>(FLOAT_PARAMETER)},
        {"EDisplayParameterType::Vector", static_cast<int>(EDisplayParameterType::Vector), "VECTOR_PARAMETER",
         static_cast<int>(VECTOR_PARAMETER)},
        {"EDisplayParameterType::Matrix", static_cast<int>(EDisplayParameterType::Matrix), "MATRIX_PARAMETER",
         static_cast<int>(MATRIX_PARAMETER)},
        {"EDisplayParameterType::String", static_cast<int>(EDisplayParameterType::String), "STRING_PARAMETER",
         static_cast<int>(STRING_PARAMETER)},
        {"EDisplayParameterType::Integer", static_cast<int>(EDisplayParameterType::Integer), "INTEGER_PARAMETER",
         static_cast<int>(INTEGER_PARAMETER)},
        {"EBlobbyOrder::RiSpec", static_cast<int>(EBlobbyOrder::RiSpec), "BLOBBY_ORDER_RISPEC",
         static_cast<int>(BLOBBY_ORDER_RISPEC)},
        {"EBlobbyOrder::AppNote", static_cast<int>(EBlobbyOrder::AppNote), "BLOBBY_ORDER_APPNOTE",
         static_cast<int>(BLOBBY_ORDER_APPNOTE)},
    };

    // the values the contract (state-vocabulary.md) promises
    const int expected[] = {0, 1, 2, 3, 4, 0, 1};

    int drifted = 0;
    int index = 0;
    for (const SPair &pair : pairs) {
        if (pair.stateValue != pair.otherValue) {
            if (drifted == 0)
                fprintf(stderr, "FIRST DRIFTED PAIR: %s = %d, but %s = %d\n", pair.state, pair.stateValue, pair.other,
                        pair.otherValue);
            ++drifted;
        }
        else if (pair.stateValue != expected[index]) {
            if (drifted == 0)
                fprintf(stderr, "FIRST DRIFTED PAIR: %s = %d, the contract says %d\n", pair.state, pair.stateValue,
                        expected[index]);
            ++drifted;
        }
        ++index;
    }

    if (drifted != 0) {
        fprintf(stderr, "State_VocabularyConsistency: %d of 7 pairs drifted\n", drifted);
        return 1;
    }
    printf("State_VocabularyConsistency: all 7 pairs agree\n");
    return 0;
}
