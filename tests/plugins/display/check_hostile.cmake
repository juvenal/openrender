# DisplayAbi_Hostile (v1_hostile and v2_hostile) and DisplayV1_HostileCopiesOnly (v1_hostile). Run as
#   cmake -DORENDER=<orender> -DTOOL=<plugin_abi_pixel_compare> -DRIBDIR=<dir> -DWORKROOT=<dir> -DCOMMON=<dir>
#         -DHOSTILE=<modules, '|'-separated> -P this
#
# A driver that overwrites everything it can reach must not change what the host does for anyone else
# (SC-005, SC-013, FR-006, FR-015). For each hostile module the scene has a normal in-tree display (a.tif,
# type "file") and the hostile one, with Exposure 1.8 2.2 and Quantize set, so a corrupted clip plane, matrix,
# gamma, gain or quantizer in the renderer would change the normal display's pixels. Three renders of the same
# scene: the normal display alone (the reference), the hostile display first, the hostile display last. The
# decoded pixels of a.tif (plugin_abi_pixel_compare decode) must be identical in all three, and the hostile
# display must itself start and finish once. Red on the starting tree: the v1 lookup hands out pointers into the
# renderer's statics (and into a string literal: the write crashes the process).
set(_or_failures 0)
include(${CMAKE_CURRENT_LIST_DIR}/display_cases.cmake)
string(REPLACE "|" ";" HOSTILE "${HOSTILE}")

function(decode_a dir out)
    execute_process(COMMAND "${TOOL}" decode "${dir}/a.tif" "${out}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _o)
    set(_decode_rc ${_rc} PARENT_SCOPE)
    set(_decode_out "${_o}" PARENT_SCOPE)
endfunction()

run_display_case(base RIB "${RIBDIR}/base.rib" TAG none STARTS 0 MESSAGES 0 EXIT_ZERO)
decode_a("${WORKROOT}/base" "${WORKROOT}/base.pix")
if(NOT _decode_rc EQUAL 0)
    or_fail("PRECONDITION: the reference render has no decodable a.tif: ${_decode_out}")
endif()

foreach(_module IN LISTS HOSTILE)
    if(_module MATCHES "^v2")
        set(_start start)
        set(_finish finish)
    else()
        set(_start displayStart)
        set(_finish displayFinish)
    endif()
    foreach(_order first last)
        set(_name ${_module}_${_order})
        run_display_case(${_name} RIB "${RIBDIR}/${_name}.rib" TAG ${_module} STARTS 1 MESSAGES 0 START ${_start} FINISH ${_finish} EXIT_ZERO)
        decode_a("${WORKROOT}/${_name}" "${WORKROOT}/${_name}.pix")
        if(NOT _decode_rc EQUAL 0)
            or_fail("${_name}: the normal display's a.tif could not be decoded: ${_decode_out}")
            continue()
        endif()
        execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${WORKROOT}/base.pix" "${WORKROOT}/${_name}.pix" RESULT_VARIABLE _same)
        if(NOT _same EQUAL 0)
            or_fail("${_name}: the normal display's pixels differ from the render without the hostile display (SC-005, SC-013)")
        endif()
    endforeach()
endforeach()

if(_or_failures GREATER 0)
    message(FATAL_ERROR "Hostile display test (${HOSTILE}): ${_or_failures} failure(s).")
endif()
