# T-L3 Link_NoWorkaroundText (spec 029, FR-002, FR-003, FR-005, SC-003).
#
# None of the link workarounds that stood in for a self-contained shading library is spelled in a build file:
#   force_load  whole-archive  export-dynamic  allow-shlib-undefined  dynamic_lookup  no-as-needed
# in CMake/, every CMakeLists.txt and *.cmake under src/libshader/, tests/unit/common/CMakeLists.txt and
# tests/shading_parity/CMakeLists.txt; and tests/shading_contract/load_stub.cpp is absent.
# tests/plugins/dynamicload/ is not scanned (its dynamic_lookup belongs to plug-in fixtures).
#
#   -DSOURCE_DIR=<repository root>
# Red on the starting tree: every one of those strings is present (the package block, libshader_shading's link options and
# comments, the static cross-library test, four --no-as-needed lines) and the load stub exists.

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "check_no_workaround_text: -DSOURCE_DIR= is required")
endif()

set(_words force_load whole-archive export-dynamic allow-shlib-undefined dynamic_lookup no-as-needed)

file(GLOB_RECURSE _files
    "${SOURCE_DIR}/CMake/*")
file(GLOB_RECURSE _libshader_files
    "${SOURCE_DIR}/src/libshader/CMakeLists.txt" "${SOURCE_DIR}/src/libshader/*/CMakeLists.txt" "${SOURCE_DIR}/src/libshader/*.cmake")
list(APPEND _files ${_libshader_files}
    "${SOURCE_DIR}/tests/unit/common/CMakeLists.txt" "${SOURCE_DIR}/tests/shading_parity/CMakeLists.txt")
list(REMOVE_DUPLICATES _files)

set(_hits 0)
foreach(_f IN LISTS _files)
    if(NOT EXISTS "${_f}" OR IS_DIRECTORY "${_f}")
        continue()
    endif()
    file(RELATIVE_PATH _rel "${SOURCE_DIR}" "${_f}")
    file(READ "${_f}" _text)
    foreach(_w IN LISTS _words)
        set(_from 0)
        while(TRUE)
            string(SUBSTRING "${_text}" ${_from} -1 _rest)
            string(FIND "${_rest}" "${_w}" _pos)
            if(_pos EQUAL -1)
                break()
            endif()
            math(EXPR _at "${_from} + ${_pos}")
            string(SUBSTRING "${_text}" 0 ${_at} _before)
            string(REGEX MATCHALL "\n" _newlines "${_before}")
            list(LENGTH _newlines _count)
            math(EXPR _line "${_count} + 1")
            message(STATUS "FAIL: ${_rel}:${_line}: ${_w}")
            math(EXPR _hits "${_hits} + 1")
            string(LENGTH "${_w}" _wl)
            math(EXPR _from "${_at} + ${_wl}")
        endwhile()
    endforeach()
endforeach()

if(EXISTS "${SOURCE_DIR}/tests/shading_contract/load_stub.cpp")
    message(STATUS "FAIL: tests/shading_contract/load_stub.cpp exists")
    math(EXPR _hits "${_hits} + 1")
endif()

if(_hits GREATER 0)
    message(FATAL_ERROR "Link_NoWorkaroundText: ${_hits} workaround(s) present")
endif()
message(STATUS "Link_NoWorkaroundText: no workaround text")
