# Spec 023 (US2, SC-002): the shading library gets its infrastructure from
# libRIBCommon, never from the renderer library.
#
# SYMBOLS lists, one per line, the infrastructure symbols libshader_shading
# uses. Each must be either defined in libshader_shading itself or resolved
# from libRIBCommon. A line starting with "!" names a symbol that must not be
# referenced at all any more (CRenderer::globalMemory, replaced by globalArena).
#
#   macOS : nm -m <shading>. An undefined symbol must read "(from libRIBCommon)";
#           "(dynamically looked up)" means flat lookup, which is how it binds
#           to libRIBRender today.
#   Linux : readelf -d <shading> must list libRIBCommon in NEEDED, and
#           nm -D --defined-only <renderer> must define none of the symbols.
#
# Usage: cmake -DSHADING=<libshader_shading> -DRENDERER=<libRIBRender>
#              -DSYMBOLS=<file> -P shading_infra_binding.cmake

# -P scripts start at CMake's oldest policies; without this, CMake 3.28 reads
# quoted if() arguments such as "public" as variable names (CMP0054).
cmake_policy(VERSION 3.19)

foreach(var SHADING RENDERER SYMBOLS)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "shading_infra_binding: -D${var}=... is required")
    endif()
endforeach()

file(STRINGS "${SYMBOLS}" _lines)
set(required "")
set(forbidden "")
foreach(line IN LISTS _lines)
    string(REGEX REPLACE "#.*" "" line "${line}")
    string(STRIP "${line}" line)
    if(line MATCHES "^!(.+)$")
        list(APPEND forbidden "${CMAKE_MATCH_1}")
    elseif(line)
        list(APPEND required "${line}")
    endif()
endforeach()

set(problems "")

if(CMAKE_HOST_APPLE)
    execute_process(COMMAND nm -m "${SHADING}" OUTPUT_VARIABLE out RESULT_VARIABLE rc ERROR_QUIET)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "shading_infra_binding: nm -m failed on ${SHADING}")
    endif()
    string(REPLACE "\n" ";" out "${out}")
    foreach(sym IN LISTS required forbidden)
        set(found "")
        foreach(line IN LISTS out)
            # Mach-O names carry one extra leading underscore.
            if(line MATCHES " _${sym}( |$)")
                set(found "${line}")
                break()
            endif()
        endforeach()
        list(FIND forbidden "${sym}" is_forbidden)
        if(is_forbidden GREATER -1)
            if(found)
                list(APPEND problems "still referenced (must be gone): ${sym}\n      ${found}")
            endif()
        elseif(NOT found)
            message(STATUS "not referenced (fine): ${sym}")
        elseif(found MATCHES "\\(undefined\\)" AND NOT found MATCHES "\\(from libRIBCommon\\)")
            list(APPEND problems "not bound to libRIBCommon: ${sym}\n      ${found}")
        endif()
    endforeach()
else()
    execute_process(COMMAND readelf -d "${SHADING}" OUTPUT_VARIABLE dyn RESULT_VARIABLE rc ERROR_QUIET)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "shading_infra_binding: readelf -d failed on ${SHADING}")
    endif()
    if(NOT dyn MATCHES "NEEDED[^\n]*libRIBCommon")
        list(APPEND problems "libshader_shading has no NEEDED entry for libRIBCommon")
    endif()
    execute_process(COMMAND nm -D --defined-only "${RENDERER}" OUTPUT_VARIABLE defs RESULT_VARIABLE rc ERROR_QUIET)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "shading_infra_binding: nm -D failed on ${RENDERER}")
    endif()
    string(REPLACE "\n" ";" defs "${defs}")
    foreach(sym IN LISTS required forbidden)
        foreach(line IN LISTS defs)
            if(line MATCHES " ${sym}(@|$)")
                list(APPEND problems "the renderer library still defines ${sym}")
                break()
            endif()
        endforeach()
    endforeach()
    execute_process(COMMAND nm -D --undefined-only "${SHADING}" OUTPUT_VARIABLE undef ERROR_QUIET)
    foreach(sym IN LISTS forbidden)
        if(undef MATCHES " ${sym}(@|\n|$)")
            list(APPEND problems "still referenced (must be gone): ${sym}")
        endif()
    endforeach()
endif()

list(LENGTH required nreq)
list(LENGTH forbidden nforb)
if(problems)
    list(LENGTH problems n)
    string(REPLACE ";" "\n  - " shown "${problems}")
    message(FATAL_ERROR "shading_infra_binding: ${n} problem(s):\n  - ${shown}")
endif()
message(STATUS "shading_infra_binding: ${nreq} infrastructure symbols come from libRIBCommon (or libshader_shading itself); ${nforb} retired symbol(s) gone")
