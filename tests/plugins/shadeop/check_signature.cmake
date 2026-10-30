# ShadeopAbi_SignatureMatches (FR-031), both halves. Run as
#   cmake -DCXX=<c++ compiler> -DCC=<c compiler> -DFLAGS="<c++ flags>" -DC_FLAGS="<c flags>"
#         -DINCLUDE_DIR=<dir holding shadeop.h> -DEXTRA_INCLUDE_DIRS="<dir>|<dir>" -DWORKDIR=<dir> -P this
#
# The C++ half is below. The C half (Phase 2, T036) compiles a strict C17 translation unit
#     SHADEOP(f) { ... }   dsoExecFunction p = f;
# under -std=c17 -Wall -Wextra -Wpedantic -Werror: the macro's function must be assignable to the exec
# typedef without a cast (the same signature). Red on the starting tree for the reason the task
# names, C-includability: the macros expand to `extern "C"`, which a C compiler cannot parse.
#
# Compiles, with the project's C++ compiler and -std=c++20 -Werror, a translation unit that
# defines an operand with the SHADEOP macro and asserts
#     std::is_same_v<decltype(&f), dsoExecFunction>
# i.e. the macro and the exec typedef describe one signature.
#
# A control translation unit with the same flags and no assertion is compiled first: if the control
# fails, the toolchain setup is wrong and the check stops with that message rather than reporting
# the defect.
#
# Red on the starting tree: SHADEOP defines void f(void *, int, char *argv[]) while dsoExecFunction
# is void (*)(void *, int, void *[]).
string(REPLACE " " ";" _flags "${FLAGS}")
file(MAKE_DIRECTORY "${WORKDIR}")

file(WRITE "${WORKDIR}/control.cpp" [=[
#include <type_traits>
#include "shadeop.h"

SHADEOP(probe_control) {
    (void)initdata;
    (void)argc;
    (void)argv;
}
]=])

file(WRITE "${WORKDIR}/signature.cpp" [=[
#include <type_traits>
#include "shadeop.h"

SHADEOP(probe_signature) {
    (void)initdata;
    (void)argc;
    (void)argv;
}

static_assert(std::is_same_v<decltype(&probe_signature), dsoExecFunction>,
              "SHADEOP and dsoExecFunction describe different signatures");
]=])

set(_extra_includes "")
string(REPLACE "|" ";" _extra_dirs "${EXTRA_INCLUDE_DIRS}")
foreach(_dir IN LISTS _extra_dirs)
    list(APPEND _extra_includes -I "${_dir}")
endforeach()

foreach(_tu control signature)
    execute_process(
        COMMAND "${CXX}" ${_flags} -std=c++20 -Werror -Wall -Wextra -fsyntax-only -I "${INCLUDE_DIR}" ${_extra_includes} "${WORKDIR}/${_tu}.cpp"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _out)
    set(_rc_${_tu} ${_rc})
    set(_out_${_tu} "${_out}")
endforeach()

# --- the C half ---
string(REPLACE " " ";" _c_flags "${C_FLAGS}")
file(WRITE "${WORKDIR}/control.c" [=[
#include <stdint.h>
extern int or_control_anchor;
]=])
file(WRITE "${WORKDIR}/signature.c" [=[
#include "shadeop.h"

SHADEOP(probe_signature_c) {
    (void)initdata;
    (void)argc;
    (void)argv;
}

dsoExecFunction or_exec_pointer = probe_signature_c;
]=])
foreach(_tu control signature)
    execute_process(
        COMMAND "${CC}" ${_c_flags} -std=c17 -Wall -Wextra -Wpedantic -Werror -fsyntax-only -I "${INCLUDE_DIR}" ${_extra_includes} "${WORKDIR}/${_tu}.c"
        RESULT_VARIABLE _c_rc_${_tu}
        OUTPUT_VARIABLE _c_out_${_tu}
        ERROR_VARIABLE _c_out_${_tu})
endforeach()

if(NOT _rc_control EQUAL 0)
    message(FATAL_ERROR "PRECONDITION: the C++ control translation unit does not compile; the check cannot tell a toolchain problem from the defect:\n${_out_control}")
endif()
if(NOT _c_rc_control EQUAL 0)
    message(FATAL_ERROR "PRECONDITION: the C control translation unit does not compile; the check cannot tell a toolchain problem from the defect:\n${_c_out_control}")
endif()
set(_failures "")
if(NOT _rc_signature EQUAL 0)
    string(APPEND _failures "\nC++ half: SHADEOP and dsoExecFunction describe different signatures:\n${_out_signature}")
endif()
if(NOT _c_rc_signature EQUAL 0)
    string(APPEND _failures "\nC half: shadeop.h as strict C17 with dsoExecFunction p = f:\n${_c_out_signature}")
endif()
if(NOT _failures STREQUAL "")
    message(FATAL_ERROR "DEFECT FR-031 (US2-4):${_failures}")
endif()
