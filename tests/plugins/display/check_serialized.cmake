# DisplayV1_SerializedData (FR-043, SC-017). Run as
#   cmake -DORENDER=<orender> -DRIBDIR=<dir> -DWORKROOT=<dir> -DCOMMON=<dir> -P this
#
# A version 1 driver's displayData has never been safe to call from two threads at once (the v1 interface
# says nothing about it), so the adapter calls it under a per-display lock. The fixture logs "enter" and
# "exit=<result>" around a short sleep; the scene renders 8 x 8 buckets on 8 threads.
#   ok     (v1_serial): the log alternates enter, exit, enter, exit: never two entries before an exit; the
#                       module is unloaded after the last exit
#   failed (v1_serial_fail, the third call returns 0): after the failing exit nothing is entered again (no
#                       call is forwarded once displayData has failed), displayFinish is never called (a
#                       failing v1 displayData has always meant "no finish afterwards"), and the module is
#                       not unloaded before the last exit (nothing is in flight when it goes)
# Red on the starting tree: dispatch() calls displayData from every render thread, and a failing call unloads
# the module while others may be inside.
include(${COMMON}/render.cmake)
set(_or_failures 0)

foreach(_case ok failed)
    if(_case STREQUAL "ok")
        set(_tag v1_serial)
    else()
        set(_tag v1_serial_fail)
    endif()
    or_render(ORENDER "${ORENDER}" RIB "${RIBDIR}/${_tag}.rib" WORKDIR "${WORKROOT}/${_case}" LOG "${WORKROOT}/${_case}.log" RESULT _rc OUTPUT _out)
    or_log_read("${WORKROOT}/${_case}.log" _lines)
    or_rc_is_status("${_rc}" _is_status)
    if(NOT _is_status)
        or_fail("${_case}: orender crashed ('${_rc}')")
    endif()

    set(_inside 0)
    set(_overlap 0)
    set(_enters 0)
    set(_failed_at -1)
    set(_entered_after_failure 0)
    set(_last_exit -1)
    set(_unload -1)
    set(_finishes 0)
    set(_i 0)
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "^[0-9]+ ${_tag}:enter ")
            math(EXPR _enters "${_enters} + 1")
            math(EXPR _inside "${_inside} + 1")
            if(_inside GREATER 1)
                math(EXPR _overlap "${_overlap} + 1")
            endif()
            if(_failed_at GREATER -1)
                math(EXPR _entered_after_failure "${_entered_after_failure} + 1")
            endif()
        elseif(_line MATCHES "^[0-9]+ ${_tag}:exit=([01]) ")
            math(EXPR _inside "${_inside} - 1")
            set(_last_exit ${_i})
            if(CMAKE_MATCH_1 EQUAL 0 AND _failed_at EQUAL -1)
                set(_failed_at ${_i})
            endif()
        elseif(_line MATCHES "^[0-9]+ ${_tag}:unload ")
            set(_unload ${_i})
        elseif(_line MATCHES "^[0-9]+ ${_tag}:displayFinish")
            math(EXPR _finishes "${_finishes} + 1")
        endif()
        math(EXPR _i "${_i} + 1")
    endforeach()

    if(_enters LESS 4)
        or_fail("${_case}: PRECONDITION: only ${_enters} displayData call(s) logged; the scene should produce at least 4")
    endif()
    if(NOT _overlap EQUAL 0)
        or_fail("${_case}: ${_overlap} displayData call(s) were entered while another was inside (FR-043)")
    endif()
    if(_unload GREATER -1 AND _unload LESS _last_exit)
        or_fail("${_case}: the module was unloaded (line ${_unload}) before the last displayData call returned (line ${_last_exit})")
    endif()
    if(_case STREQUAL "failed")
        if(_failed_at EQUAL -1)
            or_fail("failed: PRECONDITION: the failing call never happened")
        endif()
        if(NOT _entered_after_failure EQUAL 0)
            or_fail("failed: ${_entered_after_failure} displayData call(s) were forwarded after displayData failed")
        endif()
        if(NOT _finishes EQUAL 0)
            or_fail("failed: displayFinish was called ${_finishes} time(s) after a failed displayData")
        endif()
    endif()
endforeach()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "DisplayV1_SerializedData: ${_or_failures} failure(s).")
endif()
