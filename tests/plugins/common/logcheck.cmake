# logcheck.cmake -- reads and checks the call log the fixtures write (pluginLog.h).
#
# Two uses:
#
#   1. include() it from a cmake -P script for the functions below.
#
#   2. Stand-alone:  cmake -DLOG=<file> -DEXPECT_ORDER="<a>|<b>|<c>" -P logcheck.cmake
#      asserts that a line matching regex <a> is followed (later in the file) by a line
#      matching <b>, followed by one matching <c>. -DEXPECT_ABSENT="<re>|<re>" asserts that
#      no line matches. -DEXPECT_PRESENT="<re>|<re>" asserts at least one line matches.
#      Separator is '|' so a value survives add_test() and the shell; a regex that needs a
#      literal '|' cannot be given here (use the functions).
#
# A log line is "<sequence> <tag>:<call> <thread-id>". The expected patterns are matched
# against the whole line, so "fixture:call" and "^[0-9]+ fixture:call " both work.

# or_log_read(<file> <out-list>): the lines of the log, in file order; an empty list if absent.
function(or_log_read file out)
    set(_lines "")
    if(EXISTS "${file}")
        file(STRINGS "${file}" _lines)
    endif()
    set(${out} "${_lines}" PARENT_SCOPE)
endfunction()

# or_log_find(<out-index> <start> <regex> <line>...): index of the first line at or after
# <start> that matches <regex>, or -1.
function(or_log_find out start regex)
    set(_index 0)
    set(_found -1)
    foreach(_line IN LISTS ARGN)
        if(_index GREATER_EQUAL ${start} AND _line MATCHES "${regex}")
            set(_found ${_index})
            break()
        endif()
        math(EXPR _index "${_index} + 1")
    endforeach()
    set(${out} ${_found} PARENT_SCOPE)
endfunction()

# or_log_count(<out> <regex> <line>...): how many lines match.
function(or_log_count out regex)
    set(_n 0)
    foreach(_line IN LISTS ARGN)
        if(_line MATCHES "${regex}")
            math(EXPR _n "${_n} + 1")
        endif()
    endforeach()
    set(${out} ${_n} PARENT_SCOPE)
endfunction()

# or_log_check_order(<out-message> <regex-list> <line>...): empty message when the patterns
# occur in order, else a sentence naming the first pattern that did not.
function(or_log_check_order out patterns)
    set(_start 0)
    set(_message "")
    foreach(_re IN LISTS patterns)
        or_log_find(_at ${_start} "${_re}" ${ARGN})
        if(_at EQUAL -1)
            set(_message "no line matching \"${_re}\" at or after line ${_start}")
            break()
        endif()
        math(EXPR _start "${_at} + 1")
    endforeach()
    set(${out} "${_message}" PARENT_SCOPE)
endfunction()

# Stand-alone mode only when this file is the script (cmake -P logcheck.cmake), not when included.
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE AND DEFINED LOG)
    or_log_read("${LOG}" _log_lines)
    set(_failures 0)
    if(DEFINED EXPECT_ORDER)
        string(REPLACE "|" ";" _order "${EXPECT_ORDER}")
        or_log_check_order(_why "${_order}" ${_log_lines})
        if(NOT _why STREQUAL "")
            message("FAIL: order: ${_why}")
            math(EXPR _failures "${_failures} + 1")
        endif()
    endif()
    if(DEFINED EXPECT_PRESENT)
        string(REPLACE "|" ";" _present "${EXPECT_PRESENT}")
        foreach(_re IN LISTS _present)
            or_log_count(_n "${_re}" ${_log_lines})
            if(_n EQUAL 0)
                message("FAIL: no line matches \"${_re}\"")
                math(EXPR _failures "${_failures} + 1")
            endif()
        endforeach()
    endif()
    if(DEFINED EXPECT_ABSENT)
        string(REPLACE "|" ";" _absent "${EXPECT_ABSENT}")
        foreach(_re IN LISTS _absent)
            or_log_count(_n "${_re}" ${_log_lines})
            if(NOT _n EQUAL 0)
                message("FAIL: ${_n} line(s) match \"${_re}\"")
                math(EXPR _failures "${_failures} + 1")
            endif()
        endforeach()
    endif()
    if(_failures GREATER 0)
        string(REPLACE ";" "\n  " _dump "${_log_lines}")
        message(FATAL_ERROR "log ${LOG} (${_failures} failed check(s)):\n  ${_dump}")
    endif()
endif()
