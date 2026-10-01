# Render-and-measure driver for spec 021 (deferred CSG evaluation) tests.
#
# Invoked by add_csg_metric_test() (tests/visual/CMakeLists.txt) as
#   cmake -DCONFIG=<file> -DORENDER=<exe> -DMETRIC_TOOL=<exe> -P csg_metric_test.cmake
# from the test's scratch working directory, with the visual-suite
# environment (SHADERS, ORENDERHOME, DISPLAYS, ...) already set by CTest.
#
# CONFIG is generated at configure time and sets:
#   CSG_SOURCE_DIR   repository root
#   CSG_COPY         directories (relative to the root) copied into the scratch dir
#   CSG_RENDERS      RIB files (relative to the root), each optionally ":<output.tif>"
#   CSG_METRICS      one string per test_csg_seam_metric invocation
#   CSG_LOG_PASS     regexes that must match the combined render output
#   CSG_LOG_FAIL     regexes that must not match it
#   CSG_STATS        groups of four: <render> <label> <op> <value|[factor*]render>
#   CSG_ALLOW_RENDER_ERROR  TRUE: a non-zero exit is accepted, only a crash fails
#
# Every check runs even after an earlier one fails, so one run reports all
# failures; the script fails at the end if any did. The render output is
# stdout and stderr together: statistics and warnings go to stdout, errors to
# stderr (src/ri/parse/ri.cpp). orender starts at log level NONE, so it is run
# with -x 3, or its INFO-level statistics would never print.

if(NOT CONFIG OR NOT ORENDER OR NOT METRIC_TOOL)
    message(FATAL_ERROR "csg_metric_test.cmake needs CONFIG, ORENDER and METRIC_TOOL")
endif()
include(${CONFIG})

set(_failures 0)
macro(_fail msg)
    message("FAIL: ${msg}")
    math(EXPR _failures "${_failures} + 1")
endmacro()

unset(ENV{ORENDER_INSTR_LEVEL})

foreach(_dir IN LISTS CSG_COPY)
    file(COPY ${CSG_SOURCE_DIR}/${_dir} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endforeach()

# --- Renders -----------------------------------------------------------------
set(_all_output "")
foreach(_entry IN LISTS CSG_RENDERS)
    string(REPLACE ":" ";" _parts "${_entry}")
    list(GET _parts 0 _rib)
    list(LENGTH _parts _nparts)
    get_filename_component(_name ${_rib} NAME_WE)

    execute_process(
        COMMAND ${ORENDER} -x 3 ${CSG_SOURCE_DIR}/${_rib}
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _out
    )
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${_name}.log "${_out}")
    string(APPEND _all_output "${_out}")
    set(_output_${_name} "${_out}")
    message("render ${_rib}: exit ${_rc}")

    if(NOT _rc MATCHES "^-?[0-9]+$")
        _fail("orender crashed on ${_rib}: ${_rc}")
    elseif(NOT _rc EQUAL 0 AND NOT CSG_ALLOW_RENDER_ERROR)
        _fail("orender exited ${_rc} on ${_rib}")
    endif()
    if(_nparts GREATER 1)
        list(GET _parts 1 _tif)
        if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${_tif})
            _fail("${_rib} did not write ${_tif}")
        endif()
    endif()
endforeach()

# --- Log checks --------------------------------------------------------------
foreach(_re IN LISTS CSG_LOG_PASS)
    if(NOT _all_output MATCHES "${_re}")
        _fail("render output does not match LOG_PASS regex: ${_re}")
    endif()
endforeach()
foreach(_re IN LISTS CSG_LOG_FAIL)
    if(_all_output MATCHES "${_re}")
        _fail("render output matches LOG_FAIL regex: ${_re}")
    endif()
endforeach()

# --- Statistics checks -------------------------------------------------------
# The value of <label> in a render's end-of-frame statistics (the last frame
# when there are several), or "" when the label is absent.
function(_stat_value render label out)
    get_filename_component(_n ${render} NAME_WE)
    set(_text "${_output_${_n}}")
    string(REGEX REPLACE "([][.*+?^$()|\\\\])" "\\\\\\1" _label_re "${label}")
    string(REGEX MATCHALL "${_label_re}[ \t]*[0-9.]+" _hits "${_text}")
    set(_v "")
    if(_hits)
        list(GET _hits -1 _last)
        string(REGEX MATCH "[0-9.]+$" _v "${_last}")
    endif()
    set(${out} "${_v}" PARENT_SCOPE)
endfunction()

# "12.5" -> 12500
function(_to_x1000 text out)
    string(REGEX MATCH "^([0-9]*)(\\.([0-9]*))?$" _m "${text}")
    set(_int "${CMAKE_MATCH_1}")
    set(_frac "${CMAKE_MATCH_3}000")
    string(SUBSTRING "${_frac}" 0 3 _frac)
    if(_int STREQUAL "")
        set(_int 0)
    endif()
    string(REGEX REPLACE "^0+([0-9])" "\\1" _frac "${_frac}")
    math(EXPR _v "${_int} * 1000 + ${_frac}")
    set(${out} ${_v} PARENT_SCOPE)
endfunction()

list(LENGTH CSG_STATS _nstats)
if(_nstats GREATER 0)
    math(EXPR _last "${_nstats} - 1")
    foreach(_i RANGE 0 ${_last} 4)
        math(EXPR _j1 "${_i} + 1")
        math(EXPR _j2 "${_i} + 2")
        math(EXPR _j3 "${_i} + 3")
        list(GET CSG_STATS ${_i} _render)
        list(GET CSG_STATS ${_j1} _label)
        list(GET CSG_STATS ${_j2} _op)
        list(GET CSG_STATS ${_j3} _expect)

        _stat_value(${_render} "${_label}" _actual)
        if(_actual STREQUAL "")
            _fail("${_render}: statistics label '${_label}' not printed")
            continue()
        endif()

        # <value> is a number, or [factor*]<render> naming another render.
        # Compare in fixed point (x1000) so fractional factors stay exact.
        _to_x1000("${_actual}" _lhs)
        if(_expect MATCHES "^[0-9.]+$")
            _to_x1000("${_expect}" _rhs)
        else()
            set(_factor 1)
            set(_other ${_expect})
            if(_expect MATCHES "^([0-9.]+)\\*(.+)$")
                set(_factor ${CMAKE_MATCH_1})
                set(_other ${CMAKE_MATCH_2})
            endif()
            _stat_value(${_other} "${_label}" _other_value)
            if(_other_value STREQUAL "")
                _fail("${_other}: statistics label '${_label}' not printed")
                continue()
            endif()
            _to_x1000("${_other_value}" _other_x1000)
            _to_x1000("${_factor}" _factor_x1000)
            math(EXPR _rhs "${_other_x1000} * ${_factor_x1000} / 1000")
        endif()

        set(_ok FALSE)
        if(_op STREQUAL "==" AND _lhs EQUAL _rhs)
            set(_ok TRUE)
        elseif(_op STREQUAL "<=" AND NOT _lhs GREATER _rhs)
            set(_ok TRUE)
        elseif(_op STREQUAL ">" AND _lhs GREATER _rhs)
            set(_ok TRUE)
        elseif(NOT _op MATCHES "^(==|<=|>)$")
            _fail("unknown STATS operator '${_op}'")
            continue()
        endif()
        message("stats ${_render} '${_label}' = ${_actual} (${_op} ${_expect})")
        if(NOT _ok)
            _fail("${_render}: '${_label}' ${_actual} is not ${_op} ${_expect}")
        endif()
    endforeach()
endif()

# --- Metric checks -----------------------------------------------------------
foreach(_metric IN LISTS CSG_METRICS)
    separate_arguments(_args UNIX_COMMAND "${_metric}")
    execute_process(
        COMMAND ${METRIC_TOOL} ${_args}
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    string(STRIP "${_out}${_err}" _msg)
    message("metric ${_metric}\n  ${_msg}")
    if(NOT _rc EQUAL 0)
        _fail("metric failed: ${_metric}")
    endif()
endforeach()

if(_failures GREATER 0)
    message(FATAL_ERROR "${_failures} check(s) failed")
endif()
message("all checks passed")
