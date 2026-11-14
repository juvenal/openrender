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

find_program(_cmake NAMES cmake REQUIRED)
set(_stage "$ENV{TMPDIR}")
if(_stage STREQUAL "")
    set(_stage "/tmp")
endif()
string(TIMESTAMP _ts "%Y%m%d%H%M%S")
set(_prefix "${_stage}/openrender-lts-install-${_ts}")
file(MAKE_DIRECTORY "${_prefix}")

execute_process(
    COMMAND "${_cmake}" --install "${BUILD_DIR}" --prefix "${_prefix}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Lts_InstallSmoke: cmake --install failed (${_rc})")
endif()

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

set(_bin "${_prefix}/bin/orender")
if(NOT EXISTS "${_bin}")
    message(FATAL_ERROR "Lts_InstallSmoke: ${_bin} was not installed")
endif()

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
file(REMOVE_RECURSE "${_prefix}")
