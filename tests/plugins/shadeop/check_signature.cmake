# ShadeopAbi_SignatureMatches (FR-031), C++ half. Run as
#   cmake -DCXX=<compiler> -DFLAGS="<flags>" -DINCLUDE_DIR=<dir holding shadeop.h> -DWORKDIR=<dir>
#         -P this
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

foreach(_tu control signature)
    execute_process(
        COMMAND "${CXX}" ${_flags} -std=c++20 -Werror -Wall -Wextra -fsyntax-only -I "${INCLUDE_DIR}" "${WORKDIR}/${_tu}.cpp"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _out)
    set(_rc_${_tu} ${_rc})
    set(_out_${_tu} "${_out}")
endforeach()

if(NOT _rc_control EQUAL 0)
    message(FATAL_ERROR "PRECONDITION: the control translation unit does not compile; the check cannot tell a toolchain problem from the defect:\n${_out_control}")
endif()
if(NOT _rc_signature EQUAL 0)
    message(FATAL_ERROR "DEFECT FR-031: SHADEOP and dsoExecFunction describe different signatures:\n${_out_signature}")
endif()
