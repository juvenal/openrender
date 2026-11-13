# Registry_CoreNamesNoHider (spec 030, FR-007, SC-003): the renderer core names no concrete hider. In the five core
# files and the services implementation that read the frame's hider, there is no include of a concrete hider header,
# no concrete hider class name and no comparison of the hider's name.
#
# -DSOURCE_DIR=<repository root>
#
# Files: src/ri/render/{renderer,rendererContext,rendererStatics,rendererDisplay,rendererJobs}.cpp and
# src/ri/render/rendererServicesImpl.h. Patterns (the quickstart's step 6 grep):
#   #include "...(photon|raytracer|reyes|stochastic|zbuffer).h"
#   C(PhotonHider|Raytracer|Stochastic|Zbuffer|Reyes) as a whole word
#   strcmp(hider
# It never passes vacuously: a missing file is an error.
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "check_core_names_no_hider: -DSOURCE_DIR= is required")
endif()

# Reads a file into a list with one element per line. The characters CMake treats specially in lists (';', '[' and
# ']') are replaced first, so a line is never split or merged; the replacement changes no searched token.
function(read_lines path out)
    file(READ "${path}" _text)
    string(REPLACE ";" "<semi>" _text "${_text}")
    string(REPLACE "[" "<lb>" _text "${_text}")
    string(REPLACE "]" "<rb>" _text "${_text}")
    string(REPLACE "\n" ";" _text "${_text}")
    set(${out} "${_text}" PARENT_SCOPE)
endfunction()

set(_files
    src/ri/render/renderer.cpp
    src/ri/render/rendererContext.cpp
    src/ri/render/rendererStatics.cpp
    src/ri/render/rendererDisplay.cpp
    src/ri/render/rendererJobs.cpp
    src/ri/render/rendererServicesImpl.h)

set(_hits "")
foreach(_f IN LISTS _files)
    if(NOT EXISTS "${SOURCE_DIR}/${_f}")
        message(FATAL_ERROR "check_core_names_no_hider: ${_f} does not exist")
    endif()
    read_lines("${SOURCE_DIR}/${_f}" _lines)
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "#[ \t]*include[ \t]+\"[^\"]*(photon|raytracer|reyes|stochastic|zbuffer)\\.h\""
           OR _line MATCHES "(^|[^A-Za-z0-9_])C(PhotonHider|Raytracer|Stochastic|Zbuffer|Reyes)([^A-Za-z0-9_]|$)"
           OR _line MATCHES "strcmp[ \t]*\\([ \t]*hider")
            string(APPEND _hits "  ${_f}: ${_line}\n")
        endif()
    endforeach()
endforeach()

if(NOT "${_hits}" STREQUAL "")
    message(FATAL_ERROR "Registry_CoreNamesNoHider: the renderer core still names a hider:\n${_hits}")
endif()
message(STATUS "Registry_CoreNamesNoHider: no concrete hider named in the core files")
