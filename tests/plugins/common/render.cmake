# render.cmake -- helpers for the render-level plugin_abi tests (include() from a cmake -P script).
#
# A render-level test is a cmake -P script run by ctest (never a test binary run by hand).
# It runs orender on a configured RIB with an explicit WORKING_DIRECTORY, the environment
# ctest set (SHADERS, ORENDERHOME), and OR_TEST_PLUGIN_LOG pointing at a fresh log, then
# asserts on the exit status, the output text and the log.

include(${CMAKE_CURRENT_LIST_DIR}/logcheck.cmake)

# or_render(ORENDER <exe> RIB <file> WORKDIR <dir> LOG <file>
#           RESULT <var> OUTPUT <var>)
# Runs orender. RESULT is its exit status, or the text ctest/cmake report for a crash
# (a signal makes it a string such as "Segmentation fault", not a number). OUTPUT is
# stdout and stderr together, truncated to its first 4000 characters. The log file is removed first.
function(or_render)
    cmake_parse_arguments(A "" "ORENDER;RIB;WORKDIR;LOG;RESULT;OUTPUT;TIMEOUT" "" ${ARGN})
    if(NOT A_TIMEOUT)
        set(A_TIMEOUT 120)
    endif()
    file(MAKE_DIRECTORY "${A_WORKDIR}")
    file(REMOVE "${A_LOG}")
    set(ENV{OR_TEST_PLUGIN_LOG} "${A_LOG}")
    execute_process(
        COMMAND "${A_ORENDER}" "${A_RIB}"
        WORKING_DIRECTORY "${A_WORKDIR}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _out
        TIMEOUT ${A_TIMEOUT})
    unset(ENV{OR_TEST_PLUGIN_LOG})
    # A failing scene can print the same diagnostic millions of times; keep the head of the output.
    string(LENGTH "${_out}" _length)
    if(_length GREATER 4000)
        string(SUBSTRING "${_out}" 0 4000 _out)
        set(_out "${_out}\n... (output truncated, ${_length} characters)")
    endif()
    set(${A_RESULT} "${_rc}" PARENT_SCOPE)
    set(${A_OUTPUT} "${_out}" PARENT_SCOPE)
endfunction()

# or_rc_is_status(<rc> <out-var>): TRUE when <rc> is a numeric exit status (not a crash text).
function(or_rc_is_status rc out)
    if("${rc}" MATCHES "^-?[0-9]+$")
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

# or_fail(<message>): record a failed check; the script ends with FATAL_ERROR when _or_failures > 0
# (set _or_failures to 0 first), so one run reports every failure.
macro(or_fail msg)
    message("FAIL: ${msg}")
    math(EXPR _or_failures "${_or_failures} + 1")
endmacro()
