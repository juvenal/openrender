# SceneInterface_RibVectorResidual (spec 038, FR-015, SC-001, decision Q7): ribVector compiles exactly the Core
# files listed in tests/architecture/ribvector-core-residual.txt, and the list may only shrink.
#
#   cmake -DSOURCES=<$<TARGET_PROPERTY:ribVector_obj,SOURCES>> -DRESIDUAL=<ribvector-core-residual.txt>
#         -DTARGET_DIR=<src/ri> -DTARGET_BINARY_DIR=<build/src/ri> -DSOURCE_DIR=<repo> -DBUILD_DIR=<build>
#         -DMAP=<domain-map.txt> -P check_ribvector_residual.cmake
#
# Fails when
#   1. ribVector_obj compiles a Core file the list does not name,
#   2. the list names a file ribVector_obj no longer compiles (stale: delete the line),
#   3. a "needed by" cell names a file under parse/, output/ or preview/ (the residual files exist only for
#      Geometry and Hiders; a Scene Interface or preview need is exactly what this spec removes).
# The list format is "<repo-relative path>  <needed by>", '#' starts a comment. Red until Phase 4 on the
# starting tree: rendererClipping.cpp and resource.cpp are compiled and unlisted (Q7).
include(${CMAKE_CURRENT_LIST_DIR}/domain_classify.cmake)
or_check_begin()
or_map_load("${MAP}" "${SOURCE_DIR}" "${BUILD_DIR}")

set(_compiled_core "")
foreach(_s IN LISTS SOURCES)
    or_is_compiled_source("${_s}" _is_tu)
    if(NOT _is_tu)
        continue()
    endif()
    if(IS_ABSOLUTE "${_s}")
        set(_abs "${_s}")
    elseif(EXISTS "${TARGET_DIR}/${_s}")
        set(_abs "${TARGET_DIR}/${_s}")
    else()
        set(_abs "${TARGET_BINARY_DIR}/${_s}")
    endif()
    or_map_path("${_abs}" _mapped)
    or_map_class("${_mapped}" _class)
    if(_class STREQUAL "Core")
        list(APPEND _compiled_core "${_mapped}")
    endif()
endforeach()

set(_listed "")
file(STRINGS "${RESIDUAL}" _rlines)
foreach(_l IN LISTS _rlines)
    string(REGEX REPLACE "#.*$" "" _l "${_l}")
    string(STRIP "${_l}" _l)
    if(_l STREQUAL "")
        continue()
    endif()
    string(REGEX MATCH "^([^ ${_or_tab}]+)${_or_ws}*(.*)$" _m "${_l}")
    set(_file "${CMAKE_MATCH_1}")
    set(_by "${CMAKE_MATCH_2}")
    list(APPEND _listed "${_file}")
    if("${_by}" STREQUAL "")
        or_check_fail("${_file}: the list gives no 'needed by' cell")
    endif()
    if("${_by}" MATCHES "(^|[^A-Za-z0-9_])(parse|output|preview)/")
        or_check_fail("${_file}: 'needed by' names a file under parse/, output/ or preview/ (${_by}); those need nothing of the core")
    endif()
endforeach()

foreach(_c IN LISTS _compiled_core)
    list(FIND _listed "${_c}" _i)
    if(_i EQUAL -1)
        or_check_fail("ribVector_obj compiles the Core file ${_c}, which ${RESIDUAL} does not list")
    endif()
endforeach()
foreach(_f IN LISTS _listed)
    list(FIND _compiled_core "${_f}" _i)
    if(_i EQUAL -1)
        or_check_fail("${RESIDUAL} lists ${_f}, which ribVector_obj no longer compiles (stale: delete the line)")
    endif()
endforeach()
list(LENGTH _compiled_core _nc)
list(LENGTH _listed _nl)
message("ribVector_obj: ${_nc} Core file(s) compiled, ${_nl} listed")
or_check_end(SceneInterface_RibVectorResidual)
