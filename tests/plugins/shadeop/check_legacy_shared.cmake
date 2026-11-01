# ShadeopAbi_LegacyShared (FR-011, US4-2). Run as
#   cmake -DORENDER=<orender> -DTOOL=<plugin_abi_pixel_compare> -DRIBDIR=<dir> -DWORKROOT=<dir> -DCOMMON=<dir> -P this
#
# A 1.0 module with no per-thread table ("unaware") rendered on four shading threads: every exec gets
# the one pointer init returned, and the image is the one a single shading thread makes. Green on the
# starting tree by design; it must stay green. The images are compared by their decoded samples
# (plugin_abi_pixel_compare decode), not by bytes: the TIFF header carries a date.
include(${COMMON}/render.cmake)
set(_or_failures 0)

foreach(_threads mt st)
    or_render(ORENDER "${ORENDER}" RIB "${RIBDIR}/unaware_${_threads}.rib" WORKDIR "${WORKROOT}/${_threads}"
              LOG "${WORKROOT}/${_threads}.log" RESULT _rc_${_threads} OUTPUT _out_${_threads})
    or_log_read("${WORKROOT}/${_threads}.log" _lines_${_threads})
    or_rc_is_status("${_rc_${_threads}}" _is_status)
    if(NOT _is_status OR NOT _rc_${_threads} EQUAL 0)
        or_fail("unaware (${_threads}): orender ended with '${_rc_${_threads}}'\n${_out_${_threads}}")
    endif()
endforeach()

# one pointer for every thread
set(_init_data "")
foreach(_line IN LISTS _lines_mt)
    if(_line MATCHES "^[0-9]+ unaware:init data=([0-9a-f]+) ")
        list(APPEND _init_data ${CMAKE_MATCH_1})
    endif()
endforeach()
list(LENGTH _init_data _inits)
if(NOT _inits EQUAL 1)
    or_fail("unaware: init ran ${_inits} times, expected once (one process-wide init)")
endif()
set(_execs 0)
set(_tids "")
foreach(_line IN LISTS _lines_mt)
    if(_line MATCHES "^[0-9]+ unaware:exec data=([0-9a-f]+) ([0-9]+)$")
        math(EXPR _execs "${_execs} + 1")
        list(APPEND _tids ${CMAKE_MATCH_2})
        if(_inits EQUAL 1 AND NOT "${CMAKE_MATCH_1}" STREQUAL "${_init_data}")
            or_fail("unaware: exec got data ${CMAKE_MATCH_1}, not the pointer init returned (${_init_data})")
            break()
        endif()
    endif()
endforeach()
if(_execs EQUAL 0)
    or_fail("unaware: the operand never ran")
endif()
list(REMOVE_DUPLICATES _tids)
list(LENGTH _tids _threads_seen)
message("unaware: ${_execs} exec call(s) on ${_threads_seen} thread(s), one data pointer ${_init_data}")

# the image does not depend on the thread count
foreach(_threads mt st)
    execute_process(COMMAND "${TOOL}" decode "${WORKROOT}/${_threads}/unaware_${_threads}.tif" "${WORKROOT}/${_threads}.pix"
                    RESULT_VARIABLE _drc OUTPUT_VARIABLE _dout ERROR_VARIABLE _dout)
    if(NOT _drc EQUAL 0)
        or_fail("unaware (${_threads}): the image could not be decoded: ${_dout}")
    endif()
endforeach()
if(_or_failures EQUAL 0)
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${WORKROOT}/mt.pix" "${WORKROOT}/st.pix" RESULT_VARIABLE _crc)
    if(NOT _crc EQUAL 0)
        or_fail("unaware: the image rendered on four threads differs from the one-thread image")
    endif()
endif()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "ShadeopAbi_LegacyShared: ${_or_failures} failure(s).")
endif()
