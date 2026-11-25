# GitHub #208: round() truncated toward zero in both backends; it must answer the closest integer, halves away from zero.
#
# Included from CMakeLists.txt after msgout-scenes.cmake. tests/visual/shaders/round_probe.sl is test-only, compiled into the build
# tree and found through a SHADERS path that lists its directory first; the scenes are written into the build tree too. The value
# is a shader parameter. Pixel (2, 4) holds Ci = (round(v), (1 / round(v) < 0 ? 1 : 0), round(-v)), checked exactly
# (test_pixel_probe). For a result of zero the second channel is 0 for +0 and 1 for -0, which pins the sign of zero (the
# truncating cast answered +0 for -0.4). The same values under .rslo and .slo make the two backends bit-identical.

set(ROUND_DIR ${CMAKE_CURRENT_BINARY_DIR}/round)
file(MAKE_DIRECTORY ${ROUND_DIR})
set(_rd_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _rd_formats slo)
endif()
set(_rd_outputs "")
foreach(_rd_fmt ${_rd_formats})
    set(_rd_jit "")
    if(_rd_fmt STREQUAL "slo")
        set(_rd_jit -DJIT=ON)
    endif()
    add_custom_command(
        OUTPUT ${ROUND_DIR}/round_probe.${_rd_fmt}
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/shaders/round_probe.sl
                -DOUT=${ROUND_DIR}/round_probe.${_rd_fmt}
                -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_rd_jit}
                -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/round_probe.sl oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        WORKING_DIRECTORY ${ROUND_DIR}
        COMMENT ""
        VERBATIM)
    list(APPEND _rd_outputs ${ROUND_DIR}/round_probe.${_rd_fmt})
endforeach()
add_custom_target(round_shaders ALL DEPENDS ${_rd_outputs})
add_dependencies(round_shaders openrender_shaders)

set(_rd_env "${VISUAL_ENV}")
list(TRANSFORM _rd_env REPLACE "^SHADERS=(.*)$" "SHADERS=${ROUND_DIR}:\\1")

# add_round_probe(<case> <hider> <format> <v> <channels> <values>)
function(add_round_probe CASE HIDER FMT V CHANNELS VALUES)
    set(_name ${CASE}-${FMT}-${HIDER})
    set(_rib ${ROUND_DIR}/${_name}.rib)
    file(WRITE ${_rib}
"##RenderMan RIB-Structure 1.0
# GitHub #208, written by round-scenes.cmake.
version 3.03
Hider \"${HIDER}\" \"jitter\" [0]
PixelSamples 1 1
PixelFilter \"box\" 1 1
Option \"shaderformat\" \"default\" [\"${FMT}\"]
Quantize \"rgba\" 0 0 0 0
Display \"${_name}.tif\" \"file\" \"rgba\"
Format 8 8 1
Projection \"perspective\" \"fov\" 30
Clipping 0.1 100
Translate 0 0 5
WorldBegin
  Surface \"round_probe\" \"float v\" [${V}]
  Disk 0 1 360
WorldEnd
")
    separate_arguments(_values UNIX_COMMAND "${VALUES}")
    set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/round-${_name})
    file(MAKE_DIRECTORY ${_scratch})
    add_test(NAME Round_${_name}
        COMMAND test_pixel_probe $<TARGET_FILE:orender> ${_rib} ${_name}.tif 2 4 0.000001 --channels=${CHANNELS} ${_values}
        WORKING_DIRECTORY ${_scratch})
    set_tests_properties(Round_${_name} PROPERTIES
        TIMEOUT 120 LABELS "visual;regression;shading_parity;round" ENVIRONMENT "${_rd_env}" DEPENDS orender)
endfunction()

foreach(_rd_hider reyes raytrace)
    foreach(_rd_fmt ${_rd_formats})
        # nonzero results: channel 0 is round(v), channel 2 is round(-v)
        add_round_probe(up27     ${_rd_hider} ${_rd_fmt}  2.7 "0,2" "3 -3")
        add_round_probe(down27   ${_rd_hider} ${_rd_fmt} -2.7 "0,2" "-3 3")
        add_round_probe(tie25    ${_rd_hider} ${_rd_fmt}  2.5 "0,2" "3 -3")
        add_round_probe(tieneg25 ${_rd_hider} ${_rd_fmt} -2.5 "0,2" "-3 3")
        add_round_probe(tie05    ${_rd_hider} ${_rd_fmt}  0.5 "0,2" "1 -1")
        add_round_probe(tieneg05 ${_rd_hider} ${_rd_fmt} -0.5 "0,2" "-1 1")
        add_round_probe(exact3   ${_rd_hider} ${_rd_fmt}  3 "0,2" "3 -3")
        # a result of zero: the sign of the zero, +0 for 0.4 and -0 for -0.4
        add_round_probe(zero04   ${_rd_hider} ${_rd_fmt}  0.4 "0,1" "0 0")
        add_round_probe(zeroneg04 ${_rd_hider} ${_rd_fmt} -0.4 "0,1" "0 1")
    endforeach()
endforeach()
