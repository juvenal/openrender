# SceneInterface_ActiveContextAssignments (spec 038, FR-009, FR-010, US3-AS3): the active RI context, the global
# `renderMan`, is assigned only inside the Scene Interface (src/ri/parse/); everything else installs a context
# through riInstallContext().
#
#   cmake -DSOURCE_DIR=<repo> -P check_active_context_assignments.cmake
#
# Scans the sources under src/ and tests/ (this script excluded; specs/ and the documentation are not
# scanned, and neither is anything under src/ri/parse/). Fails on a line that matches
# `\brenderMan\s*=[^=]` (an assignment, not a comparison) or that names `savedRenderMan`. The comments that
# carry the same text (for example the stale one in previewContext.cpp) are matched too, on purpose: the
# tasks that change the code reword them. Red on the starting tree (research M6): previewContext.cpp,
# dataviewer/dataLoad.cpp, render/rendererContext.cpp and the four tests.
include(${CMAKE_CURRENT_LIST_DIR}/domain_classify.cmake)
or_check_begin()
get_filename_component(_root "${SOURCE_DIR}" REALPATH)

set(_self "${CMAKE_CURRENT_LIST_FILE}")
file(GLOB_RECURSE _all
    "${_root}/src/*.h" "${_root}/src/*.hpp" "${_root}/src/*.c" "${_root}/src/*.cc" "${_root}/src/*.cpp"
    "${_root}/src/*.mm" "${_root}/src/*.y" "${_root}/src/*.l"
    "${_root}/tests/*.h" "${_root}/tests/*.hpp" "${_root}/tests/*.c" "${_root}/tests/*.cc" "${_root}/tests/*.cpp"
    "${_root}/tests/*.mm" "${_root}/tests/*.y" "${_root}/tests/*.l")
set(_scanned 0)
foreach(_f IN LISTS _all)
    string(FIND "${_f}" "${_root}/src/ri/parse/" _in_parse)
    if(_in_parse EQUAL 0 OR _f STREQUAL _self)
        continue()
    endif()
    math(EXPR _scanned "${_scanned} + 1")
    file(STRINGS "${_f}" _hits REGEX "(^|[^A-Za-z0-9_])renderMan${_or_ws}*=([^=]|$)|savedRenderMan")
    foreach(_h IN LISTS _hits)
        string(REPLACE "${_root}/" "" _rel "${_f}")
        string(STRIP "${_h}" _h)
        or_check_fail("${_rel}: ${_h}")
    endforeach()
endforeach()
message("${_scanned} file(s) scanned")
or_check_end(SceneInterface_ActiveContextAssignments)
