# PluginAbi_InstallHeaders (FR-038). Run as
#   cmake -DHEADERS="<a.h>|<b.h>|..." -DSOURCE_DIR=<repo> -DSTAGE=<scratch dir>
#         -DCXX=<c++ compiler> -DFLAGS="<flags>" -DCC=<c compiler> -DC_FLAGS="<flags>" -P this
#
# FR-038: no installed header may include a header that is not itself installed. The header set
# is the input (-DHEADERS, names separated by '|'); the script stages exactly that set into a
# scratch include directory and
#   1. scans every header's #include "..." lines: each must name a staged file;
#   2. compiles each header alone with -fsyntax-only as C++20 against the staged directory only;
#   3. compiles each C header (not the .hpp ones) alone as strict C17,
#      -std=c17 -Wall -Wextra -Wpedantic -Werror, against the staged directory only (Phase 2, T037).
# A control translation unit (empty) is compiled first with the same flags for each language, so a
# red result is the header, not the toolchain.
#
# Red on the starting tree: the starting tree's install set still holds file_base.h, which includes
# the uninstalled common/os.h.
include(${CMAKE_CURRENT_LIST_DIR}/stage_headers.cmake)

string(REPLACE "|" ";" _names "${HEADERS}")
string(REPLACE " " ";" _flags "${FLAGS}")
string(REPLACE " " ";" _c_flags "${C_FLAGS}")
or_stage_headers("${SOURCE_DIR}" "${STAGE}" "${_names}")
set(_failures "")
if(OR_STAGE_MISSING)
    message(FATAL_ERROR "PRECONDITION: the install set names headers the tree does not have: ${OR_STAGE_MISSING}")
endif()

# Control: the toolchain compiles an empty header against the staged directory.
file(WRITE "${STAGE}/control.h" "#ifndef CONTROL_H\n#define CONTROL_H\nextern int or_control_anchor;\n#endif\n")
execute_process(
    COMMAND "${CXX}" ${_flags} -std=c++20 -fsyntax-only -x c++-header -I "${STAGE}/include" "${STAGE}/control.h"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "PRECONDITION: the control header does not compile; the check cannot tell a toolchain problem from the defect:\n${_out}")
endif()

execute_process(
    COMMAND "${CC}" ${_c_flags} -std=c17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only -x c -I "${STAGE}/include" "${STAGE}/control.h"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "PRECONDITION: the C17 control header does not compile; the check cannot tell a toolchain problem from the defect:\n${_out}")
endif()

foreach(_name IN LISTS OR_STAGED)
    set(_path "${STAGE}/include/${_name}")

    # 1. include closure
    file(READ "${_path}" _text)
    string(REGEX MATCHALL "#[ \t]*include[ \t]*\"[^\"]+\"" _includes "${_text}")
    foreach(_inc IN LISTS _includes)
        string(REGEX REPLACE "#[ \t]*include[ \t]*\"([^\"]+)\"" "\\1" _target "${_inc}")
        if(NOT EXISTS "${STAGE}/include/${_target}")
            list(APPEND _failures "${_name}: #include \"${_target}\" names a header that is not in the installed set")
        endif()
    endforeach()

    # 2. compile alone
    execute_process(
        COMMAND "${CXX}" ${_flags} -std=c++20 -fsyntax-only -x c++-header -I "${STAGE}/include" "${_path}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(NOT _rc EQUAL 0)
        string(REGEX MATCH "[^\n]*error[^\n]*" _first_error "${_out}")
        list(APPEND _failures "${_name}: does not compile alone as C++20: ${_first_error}")
    endif()

    # 3. strict C17, for the headers that are C headers
    if(NOT _name MATCHES "\\.hpp$")
        execute_process(
            COMMAND "${CC}" ${_c_flags} -std=c17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only -x c -I "${STAGE}/include" "${_path}"
            RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
        if(NOT _rc EQUAL 0)
            string(REGEX MATCH "[^\n]*(error|warning)[^\n]*" _first_error "${_out}")
            list(APPEND _failures "${_name}: does not compile alone as C17: ${_first_error}")
        endif()
    endif()
endforeach()

if(_failures)
    string(REPLACE ";" "\n  " _report "${_failures}")
    message(FATAL_ERROR "DEFECT FR-038: the installed header set is not self-contained:\n  ${_report}")
endif()
