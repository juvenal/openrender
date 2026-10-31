# tests/shading_contract/check_link_line.cmake
#
# ShadingContract_StandaloneLinkLine (spec 026, T051; US1 AS1, SC-001): the link command of
# test_shading_contract_standalone names no renderer.
#
#   cmake -DBUILD_DIR=<build tree of tests/shading_contract> -DTARGET=test_shading_contract_standalone
#         -P check_link_line.cmake
#
# It reads the link command the generator wrote (CMakeFiles/<target>.dir/link.txt) and fails if it names
# libRIBRender, ribRender_static, libribRender (any spelling of the renderer library) or an object under
# src/ri/{render,hiders,geometry}. The program is also required to exist: a missing link.txt is a failure,
# which is what keeps the test red until the program can be configured (OPENRENDER_SHADING_CONTRACT_TESTS).

if(NOT DEFINED BUILD_DIR OR NOT DEFINED TARGET)
    message(FATAL_ERROR "usage: cmake -DBUILD_DIR=<dir> -DTARGET=<target> -P check_link_line.cmake")
endif()

set(_link "${BUILD_DIR}/CMakeFiles/${TARGET}.dir/link.txt")
if(NOT EXISTS "${_link}")
    message(FATAL_ERROR "StandaloneLinkLine: no link command for ${TARGET} (${_link}); the program does not exist")
endif()

file(READ "${_link}" _line)
message(STATUS "link command: ${_line}")

set(_forbidden
    "libRIBRender"
    "ribRender_static"
    "libribRender"
    "ribRender.a"
    "ribRender.dylib"
    "ribRender.so"
    "src/ri/render/"
    "src/ri/hiders/"
    "src/ri/geometry/")
set(_bad "")
foreach(_f IN LISTS _forbidden)
    string(FIND "${_line}" "${_f}" _at)
    if(NOT _at EQUAL -1)
        list(APPEND _bad "${_f}")
    endif()
endforeach()

# Objects built from the renderer's sources are named by their directory in the object path
string(REGEX MATCHALL "[^ ]*CMakeFiles/[^ ]*(ribRender|ribVector)[^ ]*\\.o" _objs "${_line}")
foreach(_o IN LISTS _objs)
    list(APPEND _bad "${_o}")
endforeach()

if(_bad)
    string(REPLACE ";" ", " _bad_text "${_bad}")
    message(FATAL_ERROR "StandaloneLinkLine: the link command of ${TARGET} names the renderer: ${_bad_text}")
endif()

# And the pieces it must have
foreach(_must IN ITEMS "shader_shading" "RIBCommon")
    string(FIND "${_line}" "${_must}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "StandaloneLinkLine: the link command of ${TARGET} does not name ${_must}")
    endif()
endforeach()
message(STATUS "StandaloneLinkLine: ${TARGET} links no renderer")
