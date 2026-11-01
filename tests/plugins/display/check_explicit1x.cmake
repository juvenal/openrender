# DisplayV1_Explicit1x. Run as
#   cmake -DORENDER=<orender> -DRIBDIR=<dir of <case>.rib> -DWORKROOT=<dir> -DCOMMON=<dir> -P this
#
# A driver that declares version 1.x explicitly is a version 1 driver like any other: it starts, receives
# tiles and finishes, in that order, and displayRawData (not part of display 2.0) is never called.
# v1_explicit (1.0.0) prints nothing; v1_newer (1.4.0) prints exactly one newer-MINOR warning. Green on the
# starting tree for the behaviour, red for the warning.
set(_or_failures 0)
include(${CMAKE_CURRENT_LIST_DIR}/display_cases.cmake)

foreach(_case v1_explicit v1_newer)
    if(_case STREQUAL "v1_newer")
        set(_messages 1)
    else()
        set(_messages 0)
    endif()
    run_display_case(${_case} RIB "${RIBDIR}/${_case}.rib" TAG ${_case} STARTS 1 MESSAGES ${_messages} EXIT_ZERO)
    or_log_check_order(_why "${_case}:displayStart;${_case}:displayData;${_case}:displayFinish" ${_dc_lines})
    if(NOT _why STREQUAL "")
        or_fail("${_case}: start, data, finish not in this order: ${_why}")
    endif()
    or_log_count(_raw "${_case}:displayRawData" ${_dc_lines})
    if(NOT _raw EQUAL 0)
        or_fail("${_case}: displayRawData was called ${_raw} time(s)")
    endif()
endforeach()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "DisplayV1_Explicit1x: ${_or_failures} failure(s).")
endif()
