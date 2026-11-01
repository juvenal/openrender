# DisplayAbi_Versions_V2 (SC-002) and DisplayAbi_HiddenVisibility (US2-3). Run as
#   cmake -DORENDER=<orender> -DRIBDIR=<dir of <case>.rib> -DWORKROOT=<dir> -DCOMMON=<dir> -DCASES=<a|b> -P this
#
# Version 2 drivers load through orDisplayPlugin: v2 prints nothing; v2_newer (2.9.0) prints one newer-MINOR
# warning naming 2.9.0; v2_hidden, built with hidden symbol visibility, loads with nothing printed (its
# exports come from the header's OR_VISIBLE). Each starts and finishes once, through the v2 table.
set(_or_failures 0)
include(${CMAKE_CURRENT_LIST_DIR}/display_cases.cmake)
string(REPLACE "|" ";" CASES "${CASES}")

foreach(_case IN LISTS CASES)
    if(_case STREQUAL "v2_newer")
        run_display_case(${_case} RIB "${RIBDIR}/${_case}.rib" TAG ${_case} STARTS 1 MESSAGES 1 START start FINISH finish EXIT_ZERO
            PARTS "${_case}\\.dsply" "2\\.9\\.0")
    else()
        run_display_case(${_case} RIB "${RIBDIR}/${_case}.rib" TAG ${_case} STARTS 1 MESSAGES 0 START start FINISH finish EXIT_ZERO)
    endif()
    or_log_count(_tables "${_case}:orDisplayPlugin " ${_dc_lines})
    if(NOT _tables EQUAL 1)
        or_fail("${_case}: orDisplayPlugin was called ${_tables} time(s), expected once")
    endif()
endforeach()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "Display version test (${CASES}): ${_or_failures} failure(s).")
endif()
