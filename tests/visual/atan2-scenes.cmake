# GitHub #229: under the JIT the two-argument forms of atan, log and rotate ran the one-argument or matrix operation of the same
# name and ignored their other operands. Each overload pair shares one RSL name, so the JIT must pick the operation by the call's
# operand count.
#
# Included from CMakeLists.txt after round-scenes.cmake. The probe shaders in tests/visual/shaders are test-only, compiled into the
# build tree and found through a SHADERS path that lists their directory first; the scenes are written into the build tree too.
# The values are shader parameters. Pixel (2, 4) holds Ci, checked (test_pixel_probe) against hand-computed numbers, under .rslo
# and .slo, reyes and raytrace: the two backends and both camera hiders must agree.
#
#   Atan2_*       atan(y, x): the four quadrants and the x = 0 axis
#   LogBase_*     log(x, base)
#   LogOne_*      log(x): the clamp of its argument at 1e-6
#   RotatePoint_* rotate(Q, angle, P1, P2)

set(A2_DIR ${CMAKE_CURRENT_BINARY_DIR}/atan2)
file(MAKE_DIRECTORY ${A2_DIR})
set(_a2_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _a2_formats slo)
endif()
set(_a2_outputs "")
foreach(_a2_shader atan2_probe logbase_probe logone_probe rotatep_probe)
    foreach(_a2_fmt ${_a2_formats})
        set(_a2_jit "")
        if(_a2_fmt STREQUAL "slo")
            set(_a2_jit -DJIT=ON)
        endif()
        add_custom_command(
            OUTPUT ${A2_DIR}/${_a2_shader}.${_a2_fmt}
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_a2_shader}.sl
                    -DOUT=${A2_DIR}/${_a2_shader}.${_a2_fmt}
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_a2_jit}
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_a2_shader}.sl oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${A2_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _a2_outputs ${A2_DIR}/${_a2_shader}.${_a2_fmt})
    endforeach()
endforeach()
add_custom_target(atan2_shaders ALL DEPENDS ${_a2_outputs})
add_dependencies(atan2_shaders openrender_shaders)

set(_a2_env "${VISUAL_ENV}")
list(TRANSFORM _a2_env REPLACE "^SHADERS=(.*)$" "SHADERS=${A2_DIR}:\\1")

# add_overload_probe(<prefix> <shader> <case> <hider> <format> <parameters> <values>)
#   <parameters> is the RIB parameter list of the Surface, <values> the expected r g b.
function(add_overload_probe PREFIX SHADER CASE HIDER FMT PARAMS VALUES)
    set(_name ${CASE}-${FMT}-${HIDER})
    set(_rib ${A2_DIR}/${PREFIX}_${_name}.rib)
    file(WRITE ${_rib}
"##RenderMan RIB-Structure 1.0
# GitHub #229, written by atan2-scenes.cmake.
version 3.03
Hider \"${HIDER}\" \"jitter\" [0]
PixelSamples 1 1
PixelFilter \"box\" 1 1
Option \"shaderformat\" \"default\" [\"${FMT}\"]
Quantize \"rgba\" 0 0 0 0
Display \"${PREFIX}_${_name}.tif\" \"file\" \"rgba\"
Format 8 8 1
Projection \"perspective\" \"fov\" 30
Clipping 0.1 100
Translate 0 0 5
WorldBegin
  Surface \"${SHADER}\" ${PARAMS}
  Disk 0 1 360
WorldEnd
")
    separate_arguments(_values UNIX_COMMAND "${VALUES}")
    set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/${PREFIX}-${_name})
    file(MAKE_DIRECTORY ${_scratch})
    add_test(NAME ${PREFIX}_${_name}
        COMMAND test_pixel_probe $<TARGET_FILE:orender> ${_rib} ${PREFIX}_${_name}.tif 2 4 0.00001 --channels=0,1,2 ${_values}
        WORKING_DIRECTORY ${_scratch})
    set_tests_properties(${PREFIX}_${_name} PROPERTIES
        TIMEOUT 120 LABELS "visual;regression;shading_parity;overload_dispatch" ENVIRONMENT "${_a2_env}" DEPENDS orender)
endfunction()

# Values, to float precision: PI/4 0.7853982, 3PI/4 2.3561945, PI/2 1.5707964, PI 3.1415927.
foreach(_a2_hider reyes raytrace)
    foreach(_a2_fmt ${_a2_formats})
        # Ci = (atan(y, x), atan(-y, x), atan(y, -x))
        add_overload_probe(Atan2 atan2_probe q1      ${_a2_hider} ${_a2_fmt} "\"float y\" [1] \"float x\" [1]"    "0.7853982 -0.7853982 2.3561945")
        add_overload_probe(Atan2 atan2_probe q2      ${_a2_hider} ${_a2_fmt} "\"float y\" [1] \"float x\" [-1]"   "2.3561945 -2.3561945 0.7853982")
        add_overload_probe(Atan2 atan2_probe q3      ${_a2_hider} ${_a2_fmt} "\"float y\" [-1] \"float x\" [-1]"  "-2.3561945 2.3561945 -0.7853982")
        add_overload_probe(Atan2 atan2_probe q4      ${_a2_hider} ${_a2_fmt} "\"float y\" [-1] \"float x\" [1]"   "-0.7853982 0.7853982 -2.3561945")
        # the x = 0 axis: +-PI/2 whatever the sign of y; (y, x) = (1, 0), then (-1, 0)
        add_overload_probe(Atan2 atan2_probe axisy   ${_a2_hider} ${_a2_fmt} "\"float y\" [1] \"float x\" [0]"    "1.5707964 -1.5707964 1.5707964")
        add_overload_probe(Atan2 atan2_probe axisyn  ${_a2_hider} ${_a2_fmt} "\"float y\" [-1] \"float x\" [0]"   "-1.5707964 1.5707964 -1.5707964")
        # the y = 0 axis, x < 0: PI, and -PI for the negated (-0) y; the sign of a zero y is the one atan2 reads
        add_overload_probe(Atan2 atan2_probe axisxn  ${_a2_hider} ${_a2_fmt} "\"float y\" [0] \"float x\" [-1]"   "3.1415927 -3.1415927 0")

        # Ci = (log(x, base), log(x), log(base, x)); the middle channel is the unchanged one-argument natural logarithm
        add_overload_probe(LogBase logbase_probe b2   ${_a2_hider} ${_a2_fmt} "\"float x\" [8] \"float base\" [2]"    "3 2.0794415 0.3333333")
        add_overload_probe(LogBase logbase_probe b10  ${_a2_hider} ${_a2_fmt} "\"float x\" [100] \"float base\" [10]" "2 4.6051702 0.5")
        add_overload_probe(LogBase logbase_probe frac ${_a2_hider} ${_a2_fmt} "\"float x\" [0.5] \"float base\" [2]"  "-1 -0.6931472 -1")

        # Ci = (log(x), log(-x), log(x + x)); an argument at or below 1e-6 is clamped to it: log(1e-6) = -13.815511
        add_overload_probe(LogOne logone_probe zero  ${_a2_hider} ${_a2_fmt} "\"float x\" [0]"      "-13.815511 -13.815511 -13.815511")
        add_overload_probe(LogOne logone_probe neg   ${_a2_hider} ${_a2_fmt} "\"float x\" [-2]"     "-13.815511 0.6931472 -13.815511")
        add_overload_probe(LogOne logone_probe tiny  ${_a2_hider} ${_a2_fmt} "\"float x\" [1e-7]"   "-13.815511 -13.815511 -13.815511")
        add_overload_probe(LogOne logone_probe small ${_a2_hider} ${_a2_fmt} "\"float x\" [1e-3]"   "-6.9077553 -13.815511 -6.2146081")
        add_overload_probe(LogOne logone_probe e     ${_a2_hider} ${_a2_fmt} "\"float x\" [2.7182817]" "1 -13.815511 1.6931472")

        # Ci = rotate(q, angle, p1, p2)
        add_overload_probe(RotatePoint rotatep_probe quarter ${_a2_hider} ${_a2_fmt}
            "\"float qx\" [1] \"float qy\" [0] \"float qz\" [0] \"float angle\" [1.5707964] \"float p1x\" [0] \"float p1y\" [0] \"float p1z\" [0] \"float p2x\" [0] \"float p2y\" [0] \"float p2z\" [1]" "0 1 0")
        add_overload_probe(RotatePoint rotatep_probe offset  ${_a2_hider} ${_a2_fmt}
            "\"float qx\" [2] \"float qy\" [0] \"float qz\" [0] \"float angle\" [1.5707964] \"float p1x\" [1] \"float p1y\" [0] \"float p1z\" [0] \"float p2x\" [1] \"float p2y\" [0] \"float p2z\" [1]" "1 1 0")
        add_overload_probe(RotatePoint rotatep_probe half    ${_a2_hider} ${_a2_fmt}
            "\"float qx\" [1] \"float qy\" [0] \"float qz\" [0] \"float angle\" [3.1415927] \"float p1x\" [0] \"float p1y\" [0] \"float p1z\" [0] \"float p2x\" [0] \"float p2y\" [0] \"float p2z\" [1]" "-1 0 0")
        add_overload_probe(RotatePoint rotatep_probe onaxis  ${_a2_hider} ${_a2_fmt}
            "\"float qx\" [0] \"float qy\" [0] \"float qz\" [3] \"float angle\" [1] \"float p1x\" [0] \"float p1y\" [0] \"float p1z\" [0] \"float p2x\" [0] \"float p2y\" [0] \"float p2z\" [2]" "0 0 3")
    endforeach()
endforeach()
