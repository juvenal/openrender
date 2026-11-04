# Preview_NoCoreReach (spec 038, FR-012, SC-005; contracts/backend-registration.md): libribpreview reaches
# nothing of the renderer core.
#
#   cmake -DINCLUDE_DIRS=<$<TARGET_PROPERTY:ribpreview,INCLUDE_DIRECTORIES>> -DPREVIEW_DIR=<src/preview/libribpreview>
#         -DOBJECT_DIR=<build/src/preview/libribpreview/CMakeFiles/ribpreview.dir> -DNM=<nm>
#         -DSOURCE_DIR=<repo> -DBUILD_DIR=<build> -DMAP=<domain-map.txt> -P check_preview_no_core_reach.cmake
#
# Fails when
#   1. a libribpreview source or header includes a file domain-map.txt classifies Core (resolved as the
#      compiler does: the file's directory for a quoted include, then INCLUDE_DIRS in order), or
#   2. an object of the library has an undefined symbol of CRenderer or CRendererContext (nm -u; the mangled
#      names _ZN9CRenderer... and _ZN16CRendererContext..., with or without a leading underscore).
# Red on the starting tree: previewContext.cpp and ribGeometryContext.cpp include renderer.h and
# rendererContext.h, and previewContext.cpp.o needs CRenderer::initDeclarations / shutdownDeclarations.
include(${CMAKE_CURRENT_LIST_DIR}/../architecture/domain_classify.cmake)
or_check_begin()
or_map_load("${MAP}" "${SOURCE_DIR}" "${BUILD_DIR}")

file(GLOB_RECURSE _files "${PREVIEW_DIR}/*")
set(_n 0)
foreach(_f IN LISTS _files)
    if(NOT "${_f}" MATCHES "\\.(h|hpp|c|cc|cpp)$")
        continue()
    endif()
    or_map_path("${_f}" _from)
    or_file_includes("${_f}" _names _quoted)
    set(_i 0)
    foreach(_name IN LISTS _names)
        list(GET _quoted ${_i} _q)
        math(EXPR _i "${_i} + 1")
        or_resolve_include("${_f}" "${_name}" ${_q} "${INCLUDE_DIRS}" _abs)
        if(_abs STREQUAL "")
            continue()
        endif()
        or_map_path("${_abs}" _to)
        if(_to STREQUAL "")
            continue()
        endif()
        math(EXPR _n "${_n} + 1")
        or_map_class("${_to}" _class)
        if(_class STREQUAL "Core")
            or_check_fail("${_from} includes the Core header ${_to}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE _objects "${OBJECT_DIR}/*.o")
list(LENGTH _objects _no)
if(_no EQUAL 0)
    or_check_fail("no object files under ${OBJECT_DIR} (build the tree first)")
endif()
foreach(_o IN LISTS _objects)
    execute_process(COMMAND "${NM}" -u "${_o}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_QUIET)
    if(NOT _rc EQUAL 0)
        or_check_fail("nm -u failed on ${_o}")
        continue()
    endif()
    string(REPLACE "\n" ";" _syms "${_out}")
    foreach(_s IN LISTS _syms)
        if("${_s}" MATCHES "_ZN[K]?(9CRenderer|16CRendererContext)")
            string(REPLACE "${OBJECT_DIR}/" "" _short "${_o}")
            string(STRIP "${_s}" _s)
            or_check_fail("${_short} needs the core symbol ${_s}")
        endif()
    endforeach()
endforeach()
message("${_n} include(s) resolved inside the tree, ${_no} object(s) checked with nm")
or_check_end(Preview_NoCoreReach)
