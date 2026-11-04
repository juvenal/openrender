# SceneInterface_RibOutNoCore (spec 038, FR-015, SC-001): the RIB-output library compiles no Core file.
#
#   cmake -DSOURCES=<$<TARGET_PROPERTY:ribOut_obj,SOURCES>> -DTARGET_DIR=<src/ri> -DTARGET_BINARY_DIR=<build/src/ri>
#         -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -DMAP=<domain-map.txt> -P check_ribout_no_core.cmake
#
# Fails when a translation unit of ribOut_obj is classified Core by domain-map.txt (the map is read with
# archcheck's rule order, domain_classify.cmake). On the starting tree it names the six Core files
# (rendererDeclarations, rendererFileSearch, rendererMutexes, rendererNetwork, rendererStatics and
# vector/rendererDeclarationsDispatchStub). Red until Phase 2.
include(${CMAKE_CURRENT_LIST_DIR}/domain_classify.cmake)
or_check_begin()
or_map_load("${MAP}" "${SOURCE_DIR}" "${BUILD_DIR}")

set(_core "")
set(_n 0)
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
        set(_abs "${TARGET_BINARY_DIR}/${_s}") # a generated parser
    endif()
    or_map_path("${_abs}" _mapped)
    or_map_class("${_mapped}" _class)
    math(EXPR _n "${_n} + 1")
    if(_class STREQUAL "")
        or_check_fail("${_s} (${_mapped}) is not classified by ${MAP}")
    elseif(_class STREQUAL "Core")
        list(APPEND _core "${_mapped}")
    endif()
endforeach()
if(_n EQUAL 0)
    or_check_fail("no translation unit found in SOURCES: ${SOURCES}")
endif()
list(LENGTH _core _nc)
foreach(_c IN LISTS _core)
    or_check_fail("ribOut_obj compiles the Core file ${_c}")
endforeach()
message("ribOut_obj: ${_n} translation unit(s), ${_nc} Core")
or_check_end(SceneInterface_RibOutNoCore)
