# ProceduresAbi_NoLoadOutsideModule (spec 037, SC-008). Run as
#   cmake -DSOURCE_DIR=<repo> "-DALLOWED=<file>|<file>" -P this
#
# Every osLoadModule call site in src/ and examples/ is in one of the modules that own plug-in
# loading: proceduralModules.cpp (Procedures, Phase 3), shadeopModule.cpp (Shader DSO Operands,
# Phase 4) and displayHost.cpp (the display host, Phase 5). os.cpp and os.h define and declare it.
# A call site is the text `osLoadModule(` in a source or header file other than those.
#
# Red until Phase 5: the starting tree has six such sites (ri.cpp, rendererDisplay.cpp,
# rendererFiles.cpp, rendererContext.cpp, implicitSurface.cpp, rslo.cpp).
string(REPLACE "|" ";" _allowed "${ALLOWED}")
list(APPEND _allowed os.cpp os.h)

file(GLOB_RECURSE _files
    "${SOURCE_DIR}/src/*.cpp" "${SOURCE_DIR}/src/*.h" "${SOURCE_DIR}/src/*.hpp"
    "${SOURCE_DIR}/src/*.c" "${SOURCE_DIR}/src/*.y" "${SOURCE_DIR}/src/*.l" "${SOURCE_DIR}/src/*.mm"
    "${SOURCE_DIR}/examples/*.cpp" "${SOURCE_DIR}/examples/*.c" "${SOURCE_DIR}/examples/*.h")
set(_sites "")
foreach(_file IN LISTS _files)
    get_filename_component(_name "${_file}" NAME)
    list(FIND _allowed "${_name}" _is_allowed)
    if(NOT _is_allowed EQUAL -1)
        continue()
    endif()
    file(READ "${_file}" _text)
    string(REGEX MATCHALL "osLoadModule[ \t]*\\(" _hits "${_text}")
    if(_hits)
        string(REPLACE "${SOURCE_DIR}/" "" _relative "${_file}")
        list(LENGTH _hits _n)
        list(APPEND _sites "${_relative} (${_n})")
    endif()
endforeach()

list(LENGTH _sites _count)
message("${_count} file(s) outside ${ALLOWED} call osLoadModule")
if(_sites)
    string(REPLACE ";" "\n  " _report "${_sites}")
    message(FATAL_ERROR "DEFECT SC-008: osLoadModule is called outside the plug-in loading modules:\n  ${_report}")
endif()
