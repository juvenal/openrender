# Spec 023 (FR-008, research R2): one definer per process-wide symbol.
#
# Every symbol listed in OWNED must be defined in exactly one of the scanned
# images, and that image must be COMMON (the shared libRIBCommon). Two
# definers would give a process two copies of the state: two diagnostic
# sinks, two arenas, two sets of statistics, two RI token tables.
#
#   macOS : nm -gU <image>             (re-exported symbols are not listed)
#   Linux : nm -D --defined-only <image>; in an executable, a definition backed
#           by an R_*_COPY relocation (PIE copy of library data) is a
#           reference, not a definition.
#
# Usage: cmake -DIMAGES=<a;b;...> -DCOMMON=<libRIBCommon> -DOWNED=<file>
#              [-DTOKENS_FROM=<ri.h>] [-DEXCLUDE=<a;b;...>] -P state_ownership.cmake
#
# TOKENS_FROM adds every RI_* token declared in that header (EXTERN(RtToken)
# groups): one definition per process matters, since tokens are compared by
# pointer in places.

# -P scripts start at CMake's oldest policies; without this, CMake 3.28 reads
# quoted if() arguments such as "public" as variable names (CMP0054).
cmake_policy(VERSION 3.19)

foreach(var IMAGES COMMON OWNED)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "state_ownership: -D${var}=... is required")
    endif()
endforeach()

# --- symbols to check ------------------------------------------------------
file(STRINGS "${OWNED}" _lines)
set(symbols "")
foreach(line IN LISTS _lines)
    string(REGEX REPLACE "#.*" "" line "${line}")
    string(STRIP "${line}" line)
    if(line)
        list(APPEND symbols "${line}")
    endif()
endforeach()
if(DEFINED TOKENS_FROM)
    include(${CMAKE_CURRENT_LIST_DIR}/ri_tokens.cmake)
    ri_tokens_declared("${TOKENS_FROM}" _tokens)
    list(APPEND symbols ${_tokens})
    list(REMOVE_DUPLICATES symbols)
endif()
list(LENGTH symbols nsym)

# --- defined symbols per image -----------------------------------------------
list(APPEND IMAGES "${COMMON}")
list(REMOVE_DUPLICATES IMAGES)
if(DEFINED EXCLUDE)
    list(REMOVE_ITEM IMAGES ${EXCLUDE})
endif()

set(defs_by_symbol "")
list(REMOVE_ITEM IMAGES "")   # optional targets that are not built expand to nothing
foreach(image IN LISTS IMAGES)
    if(NOT EXISTS "${image}")
        message(FATAL_ERROR "state_ownership: image not built: ${image}")
    endif()
    if(CMAKE_HOST_APPLE)
        execute_process(COMMAND nm -gU "${image}" OUTPUT_VARIABLE out ERROR_QUIET RESULT_VARIABLE rc)
    else()
        execute_process(COMMAND nm -D --defined-only "${image}" OUTPUT_VARIABLE out ERROR_QUIET RESULT_VARIABLE rc)
    endif()
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "state_ownership: nm failed on ${image}")
    endif()

    set(copies "")
    if(NOT CMAKE_HOST_APPLE)
        execute_process(COMMAND readelf -rW "${image}" OUTPUT_VARIABLE rel ERROR_QUIET)
        # "<offset> <info> R_X86_64_COPY <value> <name>[@VERSION] + <addend>":
        # take the name after the value; the line's last field is the addend.
        string(REGEX MATCHALL "R_[A-Z0-9_]+_COPY +[0-9a-fA-F]+ +[^ \n]+" copylines "${rel}")
        foreach(cl IN LISTS copylines)
            string(REGEX MATCH "[^ ]+$" csym "${cl}")
            string(REGEX REPLACE "@.*" "" csym "${csym}")
            list(APPEND copies "${csym}")
        endforeach()
    endif()

    string(REPLACE "\n" ";" out "${out}")
    foreach(line IN LISTS out)
        # "<addr> <type> <name>"; keep the name, drop any @VERSION suffix
        if(NOT line MATCHES "^[0-9a-fA-F]+ [A-Za-z] (.+)$")
            continue()
        endif()
        set(name "${CMAKE_MATCH_1}")
        string(REGEX REPLACE "@.*" "" name "${name}")
        if(CMAKE_HOST_APPLE AND name MATCHES "^_")
            # Mach-O adds exactly one leading underscore. (REGEX REPLACE "^_"
            # would strip every leading underscore: it re-anchors per match.)
            string(SUBSTRING "${name}" 1 -1 name)
        endif()
        list(FIND symbols "${name}" idx)
        if(idx LESS 0)
            continue()
        endif()
        list(FIND copies "${name}" cidx)
        if(cidx GREATER_EQUAL 0)
            continue()
        endif()
        string(MAKE_C_IDENTIFIER "${name}" key)
        list(APPEND DEF_${key} "${image}")
    endforeach()
endforeach()

# --- verdict ------------------------------------------------------------------
set(failures 0)
foreach(sym IN LISTS symbols)
    string(MAKE_C_IDENTIFIER "${sym}" key)
    set(definers ${DEF_${key}})
    list(REMOVE_DUPLICATES definers)
    list(LENGTH definers n)
    if(n EQUAL 1 AND definers STREQUAL "${COMMON}")
        continue()
    endif()
    math(EXPR failures "${failures} + 1")
    if(n EQUAL 0)
        message("NOT DEFINED  ${sym}  (expected exactly one definer: ${COMMON})")
    else()
        string(REPLACE ";" "\n               " shown "${definers}")
        message("${n} DEFINER(S) ${sym}:\n               ${shown}")
    endif()
endforeach()

list(LENGTH IMAGES nimg)
if(failures GREATER 0)
    message(FATAL_ERROR "state_ownership: ${failures} of ${nsym} symbols are not defined once, in ${COMMON} (${nimg} images scanned)")
endif()
message(STATUS "state_ownership: all ${nsym} symbols defined once, in ${COMMON} (${nimg} images scanned)")
