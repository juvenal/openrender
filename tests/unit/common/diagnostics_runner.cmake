# Spec 023 (SC-003): run test_common_diagnostics in one mode and check where
# its marker went and how the process ended.
#
# Usage: cmake -DEXE=<test_common_diagnostics> -DMODE=<mode> -P diagnostics_runner.cmake
#
#   mode        marker on        CONTINUED   exit status
#   error       stderr only      yes         0
#   warning     stdout only      yes         0
#   info        stdout only      yes         0
#   info_quiet  nowhere          yes         0
#   fatal       stderr only      no          255 (exit(-1), not a crash)

foreach(var EXE MODE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "diagnostics_runner: -D${var}=... is required")
    endif()
endforeach()

execute_process(COMMAND "${EXE}" "${MODE}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)

set(marker "MARKER-${MODE}-no-context")
string(FIND "${out}" "${marker}" in_out)
string(FIND "${err}" "${marker}" in_err)
string(FIND "${out}" "CONTINUED" continued)

if(MODE STREQUAL "error")
    set(want_out FALSE)
    set(want_err TRUE)
    set(want_continued TRUE)
    set(want_rc 0)
elseif(MODE STREQUAL "warning" OR MODE STREQUAL "info")
    set(want_out TRUE)
    set(want_err FALSE)
    set(want_continued TRUE)
    set(want_rc 0)
elseif(MODE STREQUAL "info_quiet")
    set(want_out FALSE)
    set(want_err FALSE)
    set(want_continued TRUE)
    set(want_rc 0)
elseif(MODE STREQUAL "fatal")
    set(want_out FALSE)
    set(want_err TRUE)
    set(want_continued FALSE)
    set(want_rc 255)
else()
    message(FATAL_ERROR "diagnostics_runner: unknown mode ${MODE}")
endif()

set(problems "")
macro(expect cond what)
    if(NOT (${cond}))
        list(APPEND problems "${what}")
    endif()
endmacro()

if(want_out)
    expect("in_out GREATER -1" "marker missing from stdout")
else()
    expect("in_out EQUAL -1" "marker unexpectedly on stdout")
endif()
if(want_err)
    expect("in_err GREATER -1" "marker missing from stderr")
else()
    expect("in_err EQUAL -1" "marker unexpectedly on stderr")
endif()
if(want_continued)
    expect("continued GREATER -1" "execution did not continue (no CONTINUED)")
else()
    expect("continued EQUAL -1" "execution continued after a fatal diagnostic")
endif()
expect("\"${rc}\" STREQUAL \"${want_rc}\"" "exit status ${rc}, expected ${want_rc}")

if(problems)
    string(REPLACE ";" "\n  - " shown "${problems}")
    message(FATAL_ERROR "diagnostics ${MODE}:\n  - ${shown}\n--- stdout ---\n${out}--- stderr ---\n${err}")
endif()
message(STATUS "diagnostics ${MODE}: marker routing and exit status ${rc} as expected")
