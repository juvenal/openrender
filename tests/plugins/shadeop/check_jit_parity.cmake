# ShadeopAbi_Jit_* (spec 038, US6, FR-020 to FR-022, SC-008; contracts/jit-dso-call.md). Run as
#   cmake -DCASE=<name> -DKIND=pixels|void|perthread|missing -DSHADER=<name> -DSOURCE=<shader.sl>
#         -DINCLUDE_DIRS=<dirs of the operand modules, ';' separated> -DOSHADER=<oshader> -DORENDER=<orender>
#         -DTOOL=<plugin_abi_pixel_compare> -DWORKROOT=<dir> -DCOMMON=<dir> -DTAG=<fixture tag>
#         -DRIB_RSLO=<rib> -DRIB_SLO=<rib> -DOUTPUT=<display file name> -P this
#
# One shader, compiled to .rslo and to .slo (oshader --jit) in this run, rendered by the interpreter and by the
# JIT in the same run, with the same scene (render_format.rib.in), and compared. The compiles happen here, at
# ctest time and not in the build: on the starting tree `oshader --jit` exits 3 ("JIT coverage gap ...
# 'DSO'"), which must fail these tests and not the build (tasks.md T044). Kinds:
#
#   pixels     the two images have zero differing samples (the scene is deterministic)
#   void       pixels, and the operand's records (the call log lines "<tag>:record <value>") are the same
#              multiset in both renders
#   perthread  pixels, and the .slo render makes as many threadInit calls with distinct data pointers and
#              as many threadCleanup calls as the .rslo render reports (8 threads: 8 and 8)
#   missing    the .slo render of a shader whose operand module is no longer in the search path: the load of
#              the .slo warns that the operand was not found, getShader falls back to the .rslo, whose load
#              prints the interpreter's message; nothing crashes (decision Q2)
include(${COMMON}/render.cmake)
set(_or_failures 0)

# --- compile both formats ---
# (a macro, so that or_fail counts in this script's scope)
macro(or_compile format out_dir)
    file(MAKE_DIRECTORY "${out_dir}")
    set(_args "")
    foreach(_d IN LISTS INCLUDE_DIRS)
        list(APPEND _args -I "${_d}")
    endforeach()
    if("${format}" STREQUAL "slo")
        set(_out "${out_dir}/${SHADER}.slo")
        set(_jit --jit)
    else()
        set(_out "${out_dir}/${SHADER}.rslo")
        set(_jit "")
    endif()
    file(REMOVE "${_out}")
    execute_process(COMMAND "${OSHADER}" ${_jit} -q ${_args} -o "${_out}" "${SOURCE}"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _o TIMEOUT 120)
    if(NOT _rc EQUAL 0 OR NOT EXISTS "${_out}")
        or_fail("oshader ${_jit} ${SHADER}.sl (${format}) exited ${_rc}, expected 0:\n${_o}")
    endif()
endmacro()

macro(or_render_case tag rib)
    or_render(ORENDER "${ORENDER}" RIB "${rib}" WORKDIR "${WORKROOT}/${tag}" LOG "${WORKROOT}/${tag}.log"
              RESULT _rc_${tag} OUTPUT _out_${tag})
    or_log_read("${WORKROOT}/${tag}.log" _lines_${tag})
    or_rc_is_status("${_rc_${tag}}" _status_${tag})
    if(NOT _status_${tag})
        or_fail("${CASE} (${tag}): orender crashed ('${_rc_${tag}}')\n${_out_${tag}}")
    endif()
endmacro()

if(KIND STREQUAL "missing")
    # both formats side by side, then a search path without the module
    or_compile(rslo "${WORKROOT}/both")
    or_compile(slo "${WORKROOT}/both")
    if(_or_failures GREATER 0)
        message(FATAL_ERROR "ShadeopAbi_Jit_${CASE}: ${_or_failures} failure(s).")
    endif()
    or_render_case(slo "${RIB_SLO}")
    if(NOT "${_out_slo}" MATCHES "Failed to locate DSO function[^\n]*probe_add")
        or_fail("${CASE}: the .slo load did not warn that the operand probe_add was not found (Q2):\n${_out_slo}")
    endif()
    string(REGEX MATCHALL "Failed to locate DSO function" _hits "${_out_slo}")
    list(LENGTH _hits _nhits)
    if(_nhits LESS 2)
        or_fail("${CASE}: expected the .slo warning and then the interpreter's message after the .rslo fallback (2 'Failed to locate DSO function'), saw ${_nhits}:\n${_out_slo}")
    endif()
    if(_or_failures GREATER 0)
        message(FATAL_ERROR "ShadeopAbi_Jit_${CASE}: ${_or_failures} failure(s).")
    endif()
    return()
endif()

or_compile(rslo "${WORKROOT}/rslo")
or_compile(slo "${WORKROOT}/slo")
if(_or_failures GREATER 0)
    message(FATAL_ERROR "ShadeopAbi_Jit_${CASE}: ${_or_failures} failure(s).")
endif()

or_render_case(rslo "${RIB_RSLO}")
or_render_case(slo "${RIB_SLO}")
foreach(_tag rslo slo)
    if(_status_${_tag} AND NOT _rc_${_tag} EQUAL 0)
        or_fail("${CASE}: the ${_tag} render exited ${_rc_${_tag}}, expected 0:\n${_out_${_tag}}")
    endif()
endforeach()

# --- pixels ---
foreach(_tag rslo slo)
    execute_process(COMMAND "${TOOL}" decode "${WORKROOT}/${_tag}/${OUTPUT}" "${WORKROOT}/${_tag}.pix"
                    RESULT_VARIABLE _drc OUTPUT_VARIABLE _dout ERROR_VARIABLE _dout)
    if(NOT _drc EQUAL 0)
        or_fail("${CASE} (${_tag}): the image could not be decoded: ${_dout}")
    endif()
endforeach()
if(_or_failures EQUAL 0)
    execute_process(COMMAND "${TOOL}" compare "${WORKROOT}/slo/${OUTPUT}" "${WORKROOT}/rslo.pix"
                    RESULT_VARIABLE _crc OUTPUT_VARIABLE _cout ERROR_VARIABLE _cout)
    if(NOT _crc EQUAL 0)
        or_fail("${CASE}: the .slo image differs from the .rslo image: ${_cout}")
    endif()
endif()

# --- the operand's records ---
if(KIND STREQUAL "void")
    foreach(_tag rslo slo)
        set(_records_${_tag} "")
        foreach(_line IN LISTS _lines_${_tag})
            if(_line MATCHES "^[0-9]+ ${TAG}:record ([^ ]+) [0-9]+$")
                list(APPEND _records_${_tag} "${CMAKE_MATCH_1}")
            endif()
        endforeach()
        list(SORT _records_${_tag})
        list(LENGTH _records_${_tag} _n_${_tag})
    endforeach()
    if(_n_rslo EQUAL 0)
        or_fail("${CASE}: the .rslo render logged no record: the void operand did not run")
    endif()
    if(NOT _n_rslo EQUAL _n_slo)
        or_fail("${CASE}: the void operand ran ${_n_rslo} times under .rslo and ${_n_slo} under .slo")
    elseif(NOT "${_records_rslo}" STREQUAL "${_records_slo}")
        or_fail("${CASE}: the records of the void operand differ between .rslo and .slo")
    endif()
endif()

# --- per-thread data ---
if(KIND STREQUAL "perthread")
    foreach(_tag rslo slo)
        set(_data_${_tag} "")
        foreach(_line IN LISTS _lines_${_tag})
            if(_line MATCHES "^[0-9]+ ${TAG}:threadInit idx=([0-9]+) data=([0-9a-f]+) ")
                list(APPEND _data_${_tag} "${CMAKE_MATCH_2}")
            endif()
        endforeach()
        list(LENGTH _data_${_tag} _inits_${_tag})
        set(_distinct_${_tag} ${_data_${_tag}})
        list(REMOVE_DUPLICATES _distinct_${_tag})
        list(LENGTH _distinct_${_tag} _distinct_n_${_tag})
        or_log_count(_cleanups_${_tag} "${TAG}:threadCleanup" ${_lines_${_tag}})
    endforeach()
    if(_inits_rslo EQUAL 0)
        or_fail("${CASE}: the .rslo render made no threadInit call")
    endif()
    if(NOT _distinct_n_slo EQUAL _distinct_n_rslo OR NOT _inits_slo EQUAL _inits_rslo)
        or_fail("${CASE}: threadInit under .slo: ${_inits_slo} calls, ${_distinct_n_slo} distinct pointers; under .rslo: ${_inits_rslo} calls, ${_distinct_n_rslo} distinct pointers")
    endif()
    if(NOT _cleanups_slo EQUAL _cleanups_rslo OR NOT _cleanups_slo EQUAL _inits_slo)
        or_fail("${CASE}: threadCleanup under .slo: ${_cleanups_slo}; under .rslo: ${_cleanups_rslo}; threadInit under .slo: ${_inits_slo}")
    endif()
    if(NOT _distinct_n_slo EQUAL 8)
        or_fail("${CASE}: expected 8 distinct per-thread data pointers under .slo, saw ${_distinct_n_slo}")
    endif()
endif()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "ShadeopAbi_Jit_${CASE}: ${_or_failures} failure(s).")
endif()
