# Install into a temporary prefix and prove the tree is usable:
# public headers are present, a small C program compiles against them,
# and the installed orender answers -h and -v.
if(NOT DEFINED BUILD_DIR OR NOT DEFINED C_COMPILER OR NOT DEFINED SOURCE_FILE)
    message(FATAL_ERROR "Lts_InstallSmoke: BUILD_DIR, C_COMPILER and SOURCE_FILE are required")
endif()

set(_headers
    orplugin.h
    orplugin.hpp
    dsply.h
    shadeop.h
    dlo.h
    implicit.h
    ri.h
    ptcapi.h
    rslo.h)

set(_root "$ENV{TMPDIR}")
if(_root STREQUAL "")
    set(_root "/tmp")
endif()
string(TIMESTAMP _ts "%Y%m%d%H%M%S")
set(_stage "${_root}/openrender-lts-install-${_ts}")
file(MAKE_DIRECTORY "${_stage}")

# DESTDIR, not --prefix. OPENRENDER_HOMEDIR is the absolute prefix baked at
# configure time (self-contained builds use CMAKE_INSTALL_PREFIX), so an
# install-time --prefix leaves .orenderrc on the real prefix and a hosted
# runner cannot write there. stage-install.sh also rewrites the generated
# bundle copy so file(INSTALL) does not apply that DESTDIR a second time.
if(NOT DEFINED STAGE_INSTALL OR NOT EXISTS "${STAGE_INSTALL}")
    message(FATAL_ERROR "Lts_InstallSmoke: STAGE_INSTALL must point at scripts/ci/stage-install.sh")
endif()
find_program(_bash NAMES bash REQUIRED)
execute_process(
    COMMAND "${_bash}" "${STAGE_INSTALL}" "${BUILD_DIR}" "${_stage}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Lts_InstallSmoke: cmake --install failed (${_rc})")
endif()

execute_process(
    COMMAND find "${_stage}" -type f -path "*/bin/orender"
    OUTPUT_VARIABLE _found
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0 OR _found STREQUAL "")
    message(FATAL_ERROR "Lts_InstallSmoke: orender was not installed under ${_stage}")
endif()
string(REGEX MATCH "^[^\n]+" _bin "${_found}")
get_filename_component(_bindir "${_bin}" DIRECTORY)
get_filename_component(_prefix "${_bindir}" DIRECTORY)

set(_inc "")
if(EXISTS "${_prefix}/include/orplugin.h")
    set(_inc "${_prefix}/include")
elseif(EXISTS "${_prefix}/include/openRender/orplugin.h")
    set(_inc "${_prefix}/include/openRender")
else()
    message(FATAL_ERROR "Lts_InstallSmoke: orplugin.h is not in ${_prefix}/include or include/openRender")
endif()

foreach(_h IN LISTS _headers)
    if(NOT EXISTS "${_inc}/${_h}")
        message(FATAL_ERROR "Lts_InstallSmoke: missing installed header ${_inc}/${_h}")
    endif()
endforeach()

set(ENV{LD_LIBRARY_PATH} "${_prefix}/lib")
set(ENV{DYLD_LIBRARY_PATH} "${_prefix}/lib")
execute_process(COMMAND "${_bin}" -h RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Lts_InstallSmoke: orender -h exited ${_rc}\n${_out}\n${_err}")
endif()
if(NOT _out MATCHES "Usage: orender")
    message(FATAL_ERROR "Lts_InstallSmoke: orender -h did not print usage:\n${_out}")
endif()
execute_process(COMMAND "${_bin}" -v RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Lts_InstallSmoke: orender -v exited ${_rc}\n${_out}\n${_err}")
endif()

# Compile the same plug-in/RI program against the installed headers, not the source tree.
set(_obj "${_prefix}/lts_plugin_abi.o")
execute_process(
    COMMAND "${C_COMPILER}" -std=c17 -Wall -Werror -c "${SOURCE_FILE}" -I "${_inc}" -o "${_obj}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Lts_InstallSmoke: installed headers failed to compile:\n${_out}\n${_err}")
endif()

message(STATUS "Lts_InstallSmoke: ${_prefix}")
file(REMOVE_RECURSE "${_stage}")
