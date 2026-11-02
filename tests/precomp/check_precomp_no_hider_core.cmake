# Precomp_NoHiderCore (spec 038, FR-023, SC-009, decision Q4; contracts/tools.md): the precomputation tool
# includes neither reyes.h nor any renderer-core header.
#
#   cmake -DPRECOMP_SOURCE=<src/precomp/precomp.cpp> -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -DMAP=<domain-map.txt>
#         -P check_precomp_no_hider_core.cmake
#
# Every `#include` of precomp.cpp is resolved against the file's own directory, then src/ (the tool's include
# path) and the Scene/State directories a bare name could come from; the check fails when the file included is
# named reyes.h, or is classified Core by domain-map.txt (src/ri/render/, src/ri/vector/). Red on the starting
# tree: precomp.cpp:35-36 includes ri/hiders/reyes.h and ri/render/renderer.h.
include(${CMAKE_CURRENT_LIST_DIR}/../architecture/domain_classify.cmake)
or_check_begin()
or_map_load("${MAP}" "${SOURCE_DIR}" "${BUILD_DIR}")

set(_dirs "${OR_SOURCE_DIR}/src" "${OR_SOURCE_DIR}/src/ri" "${OR_SOURCE_DIR}/src/ri/hiders"
          "${OR_SOURCE_DIR}/src/ri/render" "${OR_SOURCE_DIR}/src/ri/state" "${OR_SOURCE_DIR}/src/ri/parse"
          "${OR_SOURCE_DIR}/src/ri/geometry" "${OR_SOURCE_DIR}/src/ri/core" "${OR_BUILD_DIR}")
or_file_includes("${PRECOMP_SOURCE}" _names _quoted)
list(LENGTH _names _nn)
if(_nn EQUAL 0)
    or_check_fail("no #include found in ${PRECOMP_SOURCE}")
endif()
set(_i 0)
foreach(_name IN LISTS _names)
    list(GET _quoted ${_i} _q)
    math(EXPR _i "${_i} + 1")
    get_filename_component(_leaf "${_name}" NAME)
    if(_leaf STREQUAL "reyes.h")
        or_check_fail("precomp.cpp includes reyes.h (\"${_name}\")")
    endif()
    or_resolve_include("${PRECOMP_SOURCE}" "${_name}" ${_q} "${_dirs}" _abs)
    if(_abs STREQUAL "")
        continue()
    endif()
    or_map_path("${_abs}" _to)
    or_map_class("${_to}" _class)
    if(_class STREQUAL "Core")
        or_check_fail("precomp.cpp includes the Core header ${_to}")
    endif()
endforeach()
message("${_nn} include(s) in precomp.cpp")
or_check_end(Precomp_NoHiderCore)
