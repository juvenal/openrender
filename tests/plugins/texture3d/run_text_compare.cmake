# run_text_compare.cmake -- runs a test program and compares (or captures) its stdout.
# Run as
#   cmake -DMODE=compare|capture -DEXE=<program> -DARG=<argument> -DWORKDIR=<dir> -DREF=<data file>
#         [-DRECAPTURE=ON|OFF] -P run_text_compare.cmake
#
# compare   the program's stdout must equal the data file exactly
# capture   writes the data file; REFUSES to overwrite an existing one unless RECAPTURE is ON
#           (the plugin-abi-capture-baselines target runs this; no ctest does)
file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")
execute_process(
    COMMAND "${EXE}" "${ARG}"
    WORKING_DIRECTORY "${WORKDIR}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
    TIMEOUT 120)
if(NOT "${_rc}" STREQUAL "0")
    message(FATAL_ERROR "${EXE} ${ARG} ended with '${_rc}'\nstdout:\n${_out}\nstderr:\n${_err}")
endif()

if(MODE STREQUAL "capture")
    if(EXISTS "${REF}" AND NOT RECAPTURE)
        message(FATAL_ERROR "${REF} exists; refusing to overwrite (configure with -DOR037_RECAPTURE=ON to recapture on purpose)")
    endif()
    get_filename_component(_dir "${REF}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dir}")
    file(WRITE "${REF}" "${_out}")
    message("captured ${REF}")
else()
    if(NOT EXISTS "${REF}")
        message(FATAL_ERROR "no reference ${REF}")
    endif()
    file(READ "${REF}" _expected)
    if(NOT "${_out}" STREQUAL "${_expected}")
        message(FATAL_ERROR "the report changed.\n--- expected (${REF})\n${_expected}\n--- actual\n${_out}\nstderr:\n${_err}")
    endif()
endif()
