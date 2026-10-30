# PluginAbi_HeadersC17 and PluginAbi_HeadersCxx20 (spec 037, FR-005, SC-003, US2-1 and US2-2). Run as
#   cmake -DLANG=c17|cxx20 -DCOMPILER=<cc or c++> -DFLAGS="<toolchain flags>" -DINCLUDE_DIRS="<dir>|<dir>"
#         -DHEADERS="<a.h>|<b.h>" -DWORKDIR=<dir> -P this
#
# Compiles, with -fsyntax-only, in the given language and with -Wall -Wextra -Wpedantic -Werror
# (C++ adds -Wformat-security, the project's own warning set):
#   * one translation unit per header, the header alone;
#   * one translation unit with all the headers in the given order, and one in the reverse order
#     (the headers must be includable in any combination).
# The header directories come from the caller (src/include, src/ri/render, src/procedures (or src/ri/geometry), src/display
# on the starting tree; T055 and T066 only change this argument when a header moves).
#
# A control translation unit with the same flags and no header is compiled first: a red result is a
# header, not the toolchain.
#
# Red on the starting tree: orplugin.h does not exist, and dsply.h, dlo.h and implicit.h open
# `extern "C" {` unguarded, so a C compiler cannot parse them.
string(REPLACE "|" ";" _dirs "${INCLUDE_DIRS}")
string(REPLACE "|" ";" _headers "${HEADERS}")
string(REPLACE " " ";" _toolchain "${FLAGS}")

if(LANG STREQUAL "c17")
    set(_std -std=c17 -x c)
    set(_warn -Wall -Wextra -Wpedantic -Werror)
elseif(LANG STREQUAL "cxx20")
    set(_std -std=c++20 -x c++)
    set(_warn -Wall -Wextra -Wpedantic -Wformat-security -Werror)
else()
    message(FATAL_ERROR "PRECONDITION: -DLANG must be c17 or cxx20")
endif()
set(_includes "")
foreach(_dir IN LISTS _dirs)
    list(APPEND _includes -I "${_dir}")
endforeach()

file(REMOVE_RECURSE "${WORKDIR}")
file(MAKE_DIRECTORY "${WORKDIR}")

function(_or_compile name text out_failed out_message)
    set(_file "${WORKDIR}/${name}.src")
    file(WRITE "${_file}" "${text}")
    execute_process(
        COMMAND "${COMPILER}" ${_toolchain} ${_std} ${_warn} -fsyntax-only ${_includes} "${_file}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(_rc EQUAL 0)
        set(${out_failed} FALSE PARENT_SCOPE)
    else()
        string(REGEX MATCH "[^\n]*(error|warning)[^\n]*" _first "${_out}")
        set(${out_failed} TRUE PARENT_SCOPE)
        set(${out_message} "${_first}" PARENT_SCOPE)
    endif()
endfunction()

_or_compile(control "#include <stdint.h>\nextern int or_control_anchor;\n" _control_failed _control_message)
if(_control_failed)
    message(FATAL_ERROR "PRECONDITION: the control translation unit does not compile; the check cannot tell a toolchain problem from a header: ${_control_message}")
endif()

set(_failures "")
set(_all "")
set(_reverse "")
set(_n 0)
foreach(_header IN LISTS _headers)
    math(EXPR _n "${_n} + 1")
    set(_include "#include \"${_header}\"\n")
    string(APPEND _all "${_include}")
    string(PREPEND _reverse "${_include}")
    _or_compile(alone_${_n} "${_include}extern int or_anchor_${_n};\n" _failed _message)
    if(_failed)
        list(APPEND _failures "${_header} alone: ${_message}")
    endif()
endforeach()
_or_compile(all_in_order "${_all}extern int or_anchor_all;\n" _failed _message)
if(_failed)
    list(APPEND _failures "all headers in order (${HEADERS}): ${_message}")
endif()
_or_compile(all_reversed "${_reverse}extern int or_anchor_rev;\n" _failed _message)
if(_failed)
    list(APPEND _failures "all headers in reverse order: ${_message}")
endif()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT (US2, SC-003): the headers are not includable as ${LANG}:\n  ${_report}")
endif()
