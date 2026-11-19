# Spec 033 (Phase 2): run a program once per mode (a mode may carry more arguments, separated by ':') and fail when any run
# exits non-zero. One RiBegin session per process (GitHub #2), several families per backend (GitHub #176).
#
#   cmake -DEXE=<program> -DMODES=rslo:matrices,slo:messages -P check_all_modes.cmake
foreach(_v EXE MODES)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "check_all_modes.cmake: ${_v} is not set")
    endif()
endforeach()
string(REPLACE "," ";" _modes "${MODES}")
set(_failures "")
foreach(_mode IN LISTS _modes)
    string(REPLACE ":" ";" _args "${_mode}")
    execute_process(COMMAND "${EXE}" ${_args} RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 300)
    if(NOT _rc EQUAL 0)
        string(APPEND _failures "--- ${_mode}: exit ${_rc}\n${_out}\n${_err}\n")
    endif()
endforeach()
if(NOT "${_failures}" STREQUAL "")
    message(FATAL_ERROR "${EXE}: one or more runs failed:\n${_failures}")
endif()
