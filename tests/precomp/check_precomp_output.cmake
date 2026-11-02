# Precomp_OutputUnchanged (spec 038, FR-023, SC-009, decision Q4; contracts/tools.md): the precomputation tool
# writes the same bytes as the unmodified tool did.
#
#   cmake -DMODE=compare|capture -DPRECOMP=<precomp> -DWORK=<scratch dir> -DREFERENCE=<stochasticPrimitives.raw.h>
#         [-DRECAPTURE=ON] -P check_precomp_output.cmake
#
# precomp writes ../ri/hiders/stochasticPrimitives.h relative to its working directory, so it runs in
# <WORK>/run and writes <WORK>/ri/hiders/stochasticPrimitives.h: never the source tree. The tracked header is
# not raw tool output (it carries the licence header and was reformatted, research M13), so the reference is the
# raw output of the unmodified tool, captured once.
#
# The caller (ctest, or the build target scene-interface-capture-baselines) already holds the machine-wide
# cpujob lock, so this script does not take it again.
#   compare  (default)  fails unless the bytes equal REFERENCE
#   capture             writes REFERENCE if it does not exist; replaces it only with RECAPTURE=ON
if(NOT MODE)
    set(MODE compare)
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/run" "${WORK}/ri/hiders")
execute_process(COMMAND "${PRECOMP}" WORKING_DIRECTORY "${WORK}/run"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 300)
set(_out_file "${WORK}/ri/hiders/stochasticPrimitives.h")
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_out_file}")
    message(FATAL_ERROR "precomp failed (exit '${_rc}') or wrote nothing at ${_out_file}\n${_out}\n${_err}")
endif()

if(MODE STREQUAL "capture-differences")
    # The output is platform independent text: on the Linux tree the capture step only compares
    set(MODE compare)
endif()
if(MODE STREQUAL "capture")
    if(EXISTS "${REFERENCE}" AND NOT RECAPTURE)
        message(STATUS "kept     ${REFERENCE} (exists; configure with -DOR038_RECAPTURE=ON to replace)")
        return()
    endif()
    get_filename_component(_dir "${REFERENCE}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dir}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy "${_out_file}" "${REFERENCE}" RESULT_VARIABLE _copied)
    if(NOT _copied EQUAL 0)
        message(FATAL_ERROR "cannot write ${REFERENCE}")
    endif()
    message(STATUS "captured ${REFERENCE}")
    return()
endif()

if(NOT EXISTS "${REFERENCE}")
    message(FATAL_ERROR "missing reference ${REFERENCE} (build the target scene-interface-capture-baselines)")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${_out_file}" "${REFERENCE}" RESULT_VARIABLE _same)
if(NOT _same EQUAL 0)
    file(SIZE "${_out_file}" _a)
    file(SIZE "${REFERENCE}" _b)
    message(FATAL_ERROR "Precomp_OutputUnchanged: precomp's output (${_a} bytes) differs from the reference (${_b} bytes): ${_out_file} against ${REFERENCE}")
endif()
message(STATUS "Precomp_OutputUnchanged: identical to the reference")
