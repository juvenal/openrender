# Shading_JitMessagesThroughServices (spec 033, FR-013, SC-005, contracts/checks-and-tests.md): the JIT's message-passing entry
# points reach the renderer only through its services.
#   src/libshader/shading/rslOps.cpp and rslBuiltins.cpp name none of
#       getParameterShape getParameter currentAttributes shaderInstanceOf lightInstanceOf
#   the bodies of CShadingContext::shaderName in shading.cpp (the two overloads) name no currentAttributes.
#
#   cmake -DSOURCE_DIR=<a tree holding src/...> -P check_jit_messages_through_services.cmake
#
# Every hit is printed as "VIOLATION <file>:<line>: <identifier>". Red on the starting tree: rslOps.cpp:924 getParameter and :938
# getParameterShape, and 12 currentAttributes lines in the shaderName bodies (shading.cpp:2909 to 2932), as recorded in
# specs/033-shading-system-backends/tools/scan-base.txt. The fixture run (fixtures/033-jit-messages) plants one and must fail.
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "check_jit_messages_through_services: -DSOURCE_DIR= is required")
endif()
include(${CMAKE_CURRENT_LIST_DIR}/scan_033_common.cmake)

set(_dir src/libshader/shading)
set(_hits "")
foreach(_f rslOps.cpp rslBuiltins.cpp)
    if(NOT EXISTS "${SOURCE_DIR}/${_dir}/${_f}")
        message(FATAL_ERROR "check_jit_messages_through_services: ${_dir}/${_f} does not exist")
    endif()
    scan_033_file("${SOURCE_DIR}/${_dir}/${_f}" "${_dir}/${_f}" "getParameterShape;getParameter;currentAttributes;shaderInstanceOf;lightInstanceOf" _hits)
endforeach()

# the bodies of CShadingContext::shaderName: from the signature line to the closing brace at depth 0
set(_shading "${SOURCE_DIR}/${_dir}/shading.cpp")
if(NOT EXISTS "${_shading}")
    message(FATAL_ERROR "check_jit_messages_through_services: ${_dir}/shading.cpp does not exist")
endif()
scan_033_lines("${_shading}" _lines)
set(_n 0)
set(_in FALSE)
set(_depth 0)
set(_started FALSE)
set(_bodies 0)
foreach(_line IN LISTS _lines)
    math(EXPR _n "${_n} + 1")
    if(NOT _in AND _line MATCHES "CShadingContext::shaderName[ \t]*\\(")
        set(_in TRUE)
        set(_depth 0)
        set(_started FALSE)
        math(EXPR _bodies "${_bodies} + 1")
    endif()
    if(_in)
        if(_line MATCHES "(^|[^A-Za-z0-9_])currentAttributes([^A-Za-z0-9_]|$)")
            list(APPEND _hits "${_dir}/shading.cpp:${_n}: currentAttributes")
        endif()
        string(REGEX MATCHALL "[{]" _open "${_line}")
        string(REGEX MATCHALL "[}]" _close "${_line}")
        list(LENGTH _open _o)
        list(LENGTH _close _c)
        math(EXPR _depth "${_depth} + ${_o} - ${_c}")
        if(_o GREATER 0)
            set(_started TRUE)
        endif()
        if(_started AND _depth LESS_EQUAL 0)
            set(_in FALSE)
        endif()
    endif()
endforeach()
if(_bodies EQUAL 0)
    message(FATAL_ERROR "check_jit_messages_through_services: no CShadingContext::shaderName found in ${_dir}/shading.cpp (the scan would pass vacuously)")
endif()

scan_033_report("${_hits}" _count)
if(_count GREATER 0)
    message(FATAL_ERROR "Shading_JitMessagesThroughServices: ${_count} direct read(s) of the renderer's records by the JIT's message entry points")
endif()
message(STATUS "Shading_JitMessagesThroughServices: the message builtins reach the renderer only through its services")
