# SceneInterface_NoCoreInclude (spec 038, FR-001, FR-004, FR-006, SC-001): the Scene Interface's sources include
# no renderer-core header.
#
#   cmake -DINCLUDE_DIRS=<$<TARGET_PROPERTY:ribOut_obj,INCLUDE_DIRECTORIES>> -DSOURCE_DIR=<repo> -DBUILD_DIR=<build>
#         -DMAP=<domain-map.txt> -P check_no_core_include.cmake
#
# Scans every file under src/ri/parse/ and src/ri/output/ (the grammar and lexer sources rib.y, rib.l, ifexpr.y
# and ifexpr.l included) and the generated parsers (rib_generated.cpp, ifexpr_generated.cpp, lex.rib.cpp,
# lex.if.cpp in the build tree). Every `#include "x"` / `<x>` is resolved the way the compiler does (the
# including file's directory for a quoted include, then the target's include directories in order) and the
# result is classified through domain-map.txt; a file classified Core is a failure. Red on the starting tree:
# rib.y, ifexpr.y (renderer.h, rendererContext.h), ri.cpp and ribOut.cpp (render/riHooks.h) and the generated
# parsers that copy rib.y's and ifexpr.y's prologues.
include(${CMAKE_CURRENT_LIST_DIR}/domain_classify.cmake)
or_check_begin()
or_map_load("${MAP}" "${SOURCE_DIR}" "${BUILD_DIR}")

set(_files "")
foreach(_dir src/ri/parse src/ri/output)
    file(GLOB _found "${OR_SOURCE_DIR}/${_dir}/*")
    foreach(_f IN LISTS _found)
        if(NOT IS_DIRECTORY "${_f}" AND "${_f}" MATCHES "\\.(h|hpp|c|cc|cpp|y|l)$")
            list(APPEND _files "${_f}")
        endif()
    endforeach()
endforeach()
foreach(_g rib_generated.cpp ifexpr_generated.cpp lex.rib.cpp lex.if.cpp)
    if(EXISTS "${OR_BUILD_DIR}/src/ri/${_g}")
        list(APPEND _files "${OR_BUILD_DIR}/src/ri/${_g}")
    else()
        or_check_fail("generated parser ${_g} not found in ${OR_BUILD_DIR}/src/ri (build the tree first)")
    endif()
endforeach()

set(_n 0)
foreach(_f IN LISTS _files)
    or_map_path("${_f}" _from)
    or_file_includes("${_f}" _names _quoted)
    set(_i 0)
    foreach(_name IN LISTS _names)
        list(GET _quoted ${_i} _q)
        math(EXPR _i "${_i} + 1")
        or_resolve_include("${_f}" "${_name}" ${_q} "${INCLUDE_DIRS}" _abs)
        if(_abs STREQUAL "")
            continue() # a system header, or one outside this tree
        endif()
        or_map_path("${_abs}" _to)
        if(_to STREQUAL "")
            continue()
        endif()
        or_map_class("${_to}" _class)
        math(EXPR _n "${_n} + 1")
        if(_class STREQUAL "Core")
            or_check_fail("${_from} includes the Core header ${_to}")
        endif()
    endforeach()
endforeach()
list(LENGTH _files _nf)
message("${_nf} file(s) scanned, ${_n} include(s) resolved inside the tree")
or_check_end(SceneInterface_NoCoreInclude)
