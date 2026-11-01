# display_cases.cmake -- run_display_case(), the check the render-level display tests share (include() it
# after setting ORENDER, WORKROOT, COMMON and _or_failures). A function, so regexes with backslashes in PARTS
# reach it unchanged; it hands the fixture log back in _dc_lines and the failure count in _or_failures.
#
#   run_display_case(<name> RIB <file> TAG <fixture log tag> STARTS <n> MESSAGES <n>
#                    [START <call>] [FINISH <call>] [PARTS <regex>...] [EXIT_ZERO])
#
# Renders RIB (a fresh log) and asserts: orender did not crash; the output holds exactly MESSAGES
# diagnostics of the display plug-in family ("the display plug-in ..." is the text of the one routine that
# prints them, common/pluginAbi.cpp) and every PARTS regex matches the output; the fixture log holds STARTS
# START calls (default displayStart) and STARTS FINISH calls (default displayFinish). EXIT_ZERO also
# asserts the exit status 0 (not for a case whose diagnostic is an error: orender exits 255 after any
# error()). Failures accumulate in _or_failures (render.cmake's or_fail).
include(${COMMON}/render.cmake)

function(run_display_case name)
    cmake_parse_arguments(_dc "EXIT_ZERO" "RIB;TAG;STARTS;MESSAGES;START;FINISH" "PARTS" ${ARGN})
    if(NOT _dc_START)
        set(_dc_START displayStart)
    endif()
    if(NOT _dc_FINISH)
        set(_dc_FINISH displayFinish)
    endif()
    or_render(ORENDER "${ORENDER}" RIB "${_dc_RIB}" WORKDIR "${WORKROOT}/${name}" LOG "${WORKROOT}/${name}.log" RESULT _dc_rc OUTPUT _dc_out)
    or_log_read("${WORKROOT}/${name}.log" _dc_lines)

    or_rc_is_status("${_dc_rc}" _dc_is_status)
    if(NOT _dc_is_status)
        or_fail("${name}: orender crashed ('${_dc_rc}')")
    elseif(_dc_EXIT_ZERO AND NOT _dc_rc EQUAL 0)
        or_fail("${name}: orender exited with status ${_dc_rc}, expected 0\n${_dc_out}")
    endif()

    string(REGEX MATCHALL "the display plug-in" _dc_found "${_dc_out}")
    list(LENGTH _dc_found _dc_messages)
    if(NOT _dc_messages EQUAL _dc_MESSAGES)
        or_fail("${name}: ${_dc_messages} display plug-in diagnostic(s), expected ${_dc_MESSAGES}:\n${_dc_out}")
    endif()
    foreach(_dc_part IN LISTS _dc_PARTS)
        if(NOT "${_dc_out}" MATCHES "${_dc_part}")
            string(REPLACE "\\" "" _dc_plain "${_dc_part}") # no backslash in an or_fail() argument (CMP0219)
            or_fail("${name}: the output does not match [${_dc_plain}]:\n${_dc_out}")
        endif()
    endforeach()

    or_log_count(_dc_starts "${_dc_TAG}:${_dc_START}( |$)" ${_dc_lines})
    or_log_count(_dc_finishes "${_dc_TAG}:${_dc_FINISH}( |$)" ${_dc_lines})
    if(NOT _dc_starts EQUAL _dc_STARTS OR NOT _dc_finishes EQUAL _dc_STARTS)
        or_fail("${name}: expected ${_dc_STARTS} ${_dc_START} and ${_dc_STARTS} ${_dc_FINISH} in the fixture log, got ${_dc_starts} and ${_dc_finishes}")
    endif()
    # results for the caller: the fixture log of this case, and the failures so far
    set(_dc_lines "${_dc_lines}" PARENT_SCOPE)
    set(_or_failures ${_or_failures} PARENT_SCOPE)
endfunction()
