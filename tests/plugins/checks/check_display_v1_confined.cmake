# DisplayV1_NoCastCheck (FR-019, FR-046). Run as
#   cmake -DSOURCE_DIR=<repo root> -P this
#
# The version 1 display adapter is confined to src/ri/render/displayV1Adapter.{h,cpp} and
# src/ri/render/displayV1Abi.h. Mechanical check over src/ (test code excluded):
#   1. outside those three files no v1 display type is named (TDisplay*Function, ParameterType,
#      ParameterTypeV1), displayV1Abi.h is not included and the adapter's thread-local display binding
#      (boundDisplay) does not appear;
#   2. nowhere is a pointer cast to a display function type ((TDisplay...), (RtDisplay...Function),
#      static_cast / reinterpret_cast to one): the RI callbacks and the module entry points are converted
#      by the adapter, not by cast;
#   3. the adapter's two entry functions, displayV1PluginForModule and displayV1PluginForCustom, are called
#      from CDisplayHost::bindPlugin and nowhere else, and the three adapter files exist.
# Red on the starting tree: dsply.h declares the v1 types and rendererDisplay.cpp casts the custom
# display's RI callbacks and the resolved module symbols to them.
find_program(GREP grep)
if(NOT GREP)
    message(FATAL_ERROR "PRECONDITION: grep not found")
endif()

set(_allowed
    "src/ri/render/displayV1Adapter.h"
    "src/ri/render/displayV1Adapter.cpp"
    "src/ri/render/displayV1Abi.h")

# ERE patterns
set(_name_pattern "TDisplay[A-Za-z]*Function|(^|[^A-Za-z0-9_])ParameterType(V1)?([^A-Za-z0-9_]|$)|displayV1Abi\\.h|boundDisplay")
set(_cast_pattern "\\([ \t]*(TDisplay[A-Za-z]*|RtDisplay[A-Za-z]*Function)[ \t]*\\)|_cast<[ \t]*(TDisplay[A-Za-z]*|RtDisplay[A-Za-z]*Function)")
set(_selector_pattern "displayV1PluginFor(Module|Custom)")

set(_violations "")

# violations_of(<kind> <pattern> <allow adapter files> <also allow displayHost.cpp>)
function(collect kind pattern allow_adapter allow_host)
    execute_process(
        COMMAND "${GREP}" -rlE --include=*.cpp --include=*.h --include=*.hpp --include=*.c --include=*.y --include=*.mm -e "${pattern}" "${SOURCE_DIR}/src"
        OUTPUT_VARIABLE _files RESULT_VARIABLE _rc)
    string(REPLACE "\n" ";" _files "${_files}")
    set(_found "")
    foreach(_file IN LISTS _files)
        if(_file STREQUAL "")
            continue()
        endif()
        file(RELATIVE_PATH _rel "${SOURCE_DIR}" "${_file}")
        if(_rel MATCHES "/tests/")
            continue()
        endif()
        list(FIND _allowed "${_rel}" _is_allowed)
        if(allow_adapter AND NOT _is_allowed EQUAL -1)
            continue()
        endif()
        if(allow_host AND _rel STREQUAL "src/ri/render/displayHost.cpp")
            continue()
        endif()
        execute_process(COMMAND "${GREP}" -nE -m 3 -e "${pattern}" "${_file}" OUTPUT_VARIABLE _lines)
        string(STRIP "${_lines}" _lines)
        string(REPLACE "\n" " | " _lines "${_lines}")
        list(APPEND _found "${_rel}: ${kind}: ${_lines}")
    endforeach()
    set(_violations "${_violations};${_found}" PARENT_SCOPE)
endfunction()

# 1. v1 type names, the include of displayV1Abi.h and the thread-local binding: only the three files
collect("names a v1 display type or the adapter's binding" "${_name_pattern}" TRUE FALSE)
# 2. casts to a display function type: nowhere
collect("casts to a display function type" "${_cast_pattern}" FALSE FALSE)
# 3. the adapter's entry functions: the adapter files, and displayHost.cpp (its bindPlugin, checked below)
collect("selects the v1 adapter outside CDisplayHost::bindPlugin" "${_selector_pattern}" TRUE TRUE)

# the three adapter files exist
foreach(_needed IN LISTS _allowed)
    if(NOT EXISTS "${SOURCE_DIR}/${_needed}")
        list(APPEND _violations "${_needed}: does not exist (the adapter is not in place yet)")
    endif()
endforeach()

# displayHost.cpp: the selector calls sit inside the body of CDisplayHost::bindPlugin
set(_host "${SOURCE_DIR}/src/ri/render/displayHost.cpp")
if(NOT EXISTS "${_host}")
    list(APPEND _violations "src/ri/render/displayHost.cpp: does not exist (CDisplayHost is not in place yet)")
else()
    file(READ "${_host}" _hosttext)
    string(FIND "${_hosttext}" "CDisplayHost::bindPlugin" _at)
    if(_at EQUAL -1)
        list(APPEND _violations "src/ri/render/displayHost.cpp: has no CDisplayHost::bindPlugin")
    else()
        string(SUBSTRING "${_hosttext}" ${_at} -1 _from)
        string(FIND "${_from}" "\n}\n" _end)
        string(SUBSTRING "${_hosttext}" 0 ${_at} _before)
        if(_end EQUAL -1)
            set(_after "")
        else()
            math(EXPR _skip "${_end} + 3")
            string(SUBSTRING "${_from}" ${_skip} -1 _after)
        endif()
        if("${_before}${_after}" MATCHES "${_selector_pattern}")
            list(APPEND _violations "src/ri/render/displayHost.cpp: selects the v1 adapter outside CDisplayHost::bindPlugin")
        endif()
    endif()
endif()

list(FILTER _violations EXCLUDE REGEX "^$")
list(LENGTH _violations _n)
if(_n GREATER 0)
    string(REPLACE ";" "\n  " _dump "${_violations}")
    message(FATAL_ERROR "DEFECT FR-019/FR-046: ${_n} violation(s) of the v1 adapter's confinement:\n  ${_dump}")
endif()
message("the v1 adapter is confined")
