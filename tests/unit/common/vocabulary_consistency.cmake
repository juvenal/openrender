# Spec 023 (FR-004): the RI token list exists in two headers on purpose --
# the public ri.h stays byte-identical, while every domain takes the tokens
# from common/riVocabulary.h -- and the tokens are defined once, in
# common/riTokens.cpp. This check keeps the three lists identical: same names,
# no duplicates.
#
# Usage: cmake -DRI_H=<ri.h> -DVOCABULARY_H=<riVocabulary.h>
#              -DTOKENS_CPP=<riTokens.cpp> -P vocabulary_consistency.cmake

# -P scripts start at CMake's oldest policies; without this, CMake 3.28 reads
# quoted if() arguments such as "public" as variable names (CMP0054).
cmake_policy(VERSION 3.19)

foreach(var RI_H VOCABULARY_H TOKENS_CPP)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "vocabulary_consistency: -D${var}=... is required")
    endif()
    if(NOT EXISTS "${${var}}")
        message(FATAL_ERROR "vocabulary_consistency: ${var} does not exist: ${${var}}")
    endif()
endforeach()

include(${CMAKE_CURRENT_LIST_DIR}/ri_tokens.cmake)
ri_tokens_declared("${RI_H}" public)
ri_tokens_declared("${VOCABULARY_H}" vocabulary)
ri_tokens_defined("${TOKENS_CPP}" definitions)

set(problems "")
foreach(listname public vocabulary definitions)
    set(copy ${${listname}})
    list(LENGTH copy n)
    list(REMOVE_DUPLICATES copy)
    list(LENGTH copy u)
    # ri.h itself redeclares six tokens (RI_WORLD, RI_OBJECT, RI_RASTER,
    # RI_SCREEN, RI_CAMERA, RI_SHADOW appear in two groups each). That is
    # legal, and the public header stays byte-identical, so duplicates count
    # only in the two common-layer files.
    if(NOT n EQUAL u AND NOT listname STREQUAL "public")
        list(APPEND problems "${listname}: ${n} entries but only ${u} distinct names")
    endif()
    set(${listname}_set ${copy})
endforeach()

macro(compare a b)
    set(only_a ${${a}_set})
    list(REMOVE_ITEM only_a ${${b}_set})
    set(only_b ${${b}_set})
    list(REMOVE_ITEM only_b ${${a}_set})
    if(only_a)
        string(REPLACE ";" " " shown "${only_a}")
        list(APPEND problems "in ${a} but not ${b}: ${shown}")
    endif()
    if(only_b)
        string(REPLACE ";" " " shown "${only_b}")
        list(APPEND problems "in ${b} but not ${a}: ${shown}")
    endif()
endmacro()
compare(public vocabulary)
compare(public definitions)

list(LENGTH public_set n)
if(n EQUAL 0)
    list(APPEND problems "no tokens found in ${RI_H}")
endif()

if(problems)
    string(REPLACE ";" "\n  - " shown "${problems}")
    message(FATAL_ERROR "vocabulary_consistency:\n  - ${shown}")
endif()
message(STATUS "vocabulary_consistency: ri.h, riVocabulary.h and riTokens.cpp agree on ${n} RI tokens")
