# SceneInterface_HooksAndRecords (spec 038, FR-006, FR-007, FR-008): the back-end registration hooks and the
# Procedural record belong to the Scene Interface.
#
#   cmake -DINCLUDE_DIRS=<$<TARGET_PROPERTY:ribOut_obj,INCLUDE_DIRECTORIES>> -DSOURCE_DIR=<repo> -DBUILD_DIR=<build>
#         -DMAP=<domain-map.txt> -P check_hooks_and_records.cmake
#
# Fails when
#   1. riHooks.h is not at src/ri/parse/riHooks.h (or a copy still sits in src/ri/render/),
#   2. CDelayedData is defined (class / struct with a body) in a file the map does not classify
#      SceneInterface, or is defined nowhere,
#   3. a Scene Interface source (src/ri/parse/, src/ri/output/, the generated parsers) includes a file named
#      delayed.h (Geometry's header; Scene Interface may depend on Geometry, but the record it needs is its own).
# Red on the starting tree: riHooks.h is in render/, CDelayedData is defined in geometry/delayed.h, and rib.y
# and ri.cpp include delayed.h.
include(${CMAKE_CURRENT_LIST_DIR}/domain_classify.cmake)
or_check_begin()
or_map_load("${MAP}" "${SOURCE_DIR}" "${BUILD_DIR}")

# 1. riHooks.h
if(NOT EXISTS "${OR_SOURCE_DIR}/src/ri/parse/riHooks.h")
    or_check_fail("src/ri/parse/riHooks.h does not exist")
endif()
if(EXISTS "${OR_SOURCE_DIR}/src/ri/render/riHooks.h")
    or_check_fail("src/ri/render/riHooks.h still exists (a Core-classified file the Scene Interface depends on)")
endif()

# 2. CDelayedData
file(GLOB_RECURSE _headers "${OR_SOURCE_DIR}/src/*.h" "${OR_SOURCE_DIR}/src/*.hpp")
set(_defined 0)
foreach(_h IN LISTS _headers)
    file(STRINGS "${_h}" _hits REGEX "^${_or_ws}*(class|struct)${_or_ws}+CDelayedData${_or_ws}*([{]|:|$)")
    if(_hits)
        math(EXPR _defined "${_defined} + 1")
        or_map_path("${_h}" _mapped)
        or_map_class("${_mapped}" _class)
        if(NOT _class STREQUAL "SceneInterface")
            or_check_fail("CDelayedData is defined in ${_mapped} (${_class}), outside the Scene Interface")
        endif()
    endif()
endforeach()
if(_defined EQUAL 0)
    or_check_fail("CDelayedData is defined nowhere under src/")
endif()

# 3. Scene sources that include delayed.h
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
    endif()
endforeach()
foreach(_f IN LISTS _files)
    or_map_path("${_f}" _from)
    or_file_includes("${_f}" _names _quoted)
    foreach(_name IN LISTS _names)
        get_filename_component(_leaf "${_name}" NAME)
        if(_leaf STREQUAL "delayed.h")
            or_check_fail("${_from} includes geometry/delayed.h (\"${_name}\")")
        endif()
    endforeach()
endforeach()
or_check_end(SceneInterface_HooksAndRecords)
