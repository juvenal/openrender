# GitHub #169: visibility(P1, P2) always returned 1, so an occluder never shadowed quadlight/spherelight. Included from
# CMakeLists.txt after dlo-uv-scenes.cmake. The probe shaders are test-only, compiled into the build tree for both formats and
# found through a SHADERS path that lists their directory first; the scenes are written into the build tree.
#
# The rule under test is the one transmission() and a ray-traced shadow() use: only an object with
# Attribute "visibility" "int transmission" [1] occludes, and the value is the fraction of the probe's samples that reach
# the end point (visibility() takes no parameter list, so it has one sample and the value is 1 or 0).
#
#   Visibility169_<case>_<hider>_<fmt>   an 8 x 8 orthographic frame, pixel (4,4), float RGB. The receiver is a disk at
#                                        z = 0 and the camera looks along +z from z = -5, so the segment from the receiver
#                                        to P + (0, 0, dist) runs along the view ray.
#       occluded        a flagged disk at z = 1 between the points: 0 (1 on the starting tree)
#       unflagged       the same disk without the flag: 1
#       clear           nothing between the points: 1
#       endpoint        the receiver and a disk at z = 2 are both flagged and the end point is on that disk: 1. The ray starts
#                       on a flagged surface and stops a bias short of another; neither may count
#       beyond          a flagged disk at z = 3 beyond the end point (z = 2): 1
#   Visibility169_light_raytrace_<fmt>   the scene of the report: a point light at (0, 4, 0) whose intensity is scaled by
#                       visibility(Ps, from), a flagged disk of radius 0.8 at y = 2 and a floor at y = 0, seen from
#                       (0, 5.785, -6.894) (Translate 0 0 9, Rotate -40 about x). The ray through pixel (128, 158) meets the
#                       floor at z = -1.353; the light reaches that point through y = 2 at z = -0.676, inside the disk, so the
#                       point is in the umbra (radius 1.6 on the floor) and the pixel is 0. On the starting tree it is lit.

set(VIS169_DIR ${CMAKE_CURRENT_BINARY_DIR}/visibility169)
file(MAKE_DIRECTORY ${VIS169_DIR})
set(_v_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _v_formats slo)
endif()
set(_v_outputs "")
foreach(_v_name visibility169_surface visibility169_light)
    set(_v_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_v_name}.sl)
    foreach(_v_fmt ${_v_formats})
        set(_v_jit "")
        if(_v_fmt STREQUAL "slo")
            set(_v_jit -DJIT=ON)
        endif()
        add_custom_command(
            OUTPUT ${VIS169_DIR}/${_v_name}.${_v_fmt}
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_v_sl} -DOUT=${VIS169_DIR}/${_v_name}.${_v_fmt}
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_v_jit}
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_v_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${VIS169_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _v_outputs ${VIS169_DIR}/${_v_name}.${_v_fmt})
    endforeach()
endforeach()
add_custom_target(visibility169_shaders ALL DEPENDS ${_v_outputs})
add_dependencies(visibility169_shaders openrender_shaders)

set(_v_env "${VISUAL_ENV}")
list(TRANSFORM _v_env REPLACE "^SHADERS=(.*)$" "SHADERS=${VIS169_DIR}:\\1")

# case table: name, receiver flag, occluder z (or "none"), occluder flag, dist, expected value
set(_v_cases
    "occluded|0|1|1|2|0"
    "unflagged|0|1|0|2|1"
    "clear|0|none|0|2|1"
    "endpoint|1|2|1|2|1"
    "beyond|0|3|1|2|1")

foreach(_h raytrace reyes)
    foreach(_f ${_v_formats})
        foreach(_c ${_v_cases})
            string(REPLACE "|" ";" _p "${_c}")
            list(GET _p 0 _name)
            list(GET _p 1 _rflag)
            list(GET _p 2 _oz)
            list(GET _p 3 _oflag)
            list(GET _p 4 _dist)
            list(GET _p 5 _want)
            set(_scene ${_name}_${_h}_${_f})
            set(_rattr "")
            if(_rflag)
                set(_rattr "    Attribute \"visibility\" \"int transmission\" [1]\n")
            endif()
            set(_occ "")
            if(NOT _oz STREQUAL "none")
                set(_oattr "")
                if(_oflag)
                    set(_oattr "    Attribute \"visibility\" \"int transmission\" [1]\n")
                endif()
                set(_occ "  AttributeBegin
${_oattr}    Surface \"constant\"
    Translate 0 0 ${_oz}
    Disk 0 30 360
  AttributeEnd
")
            endif()
            file(WRITE ${VIS169_DIR}/${_scene}.rib
"##RenderMan RIB-Structure 1.0
# GitHub #169, written by visibility-scenes.cmake.
version 3.03
Hider \"${_h}\" \"jitter\" [0]
PixelSamples 1 1
PixelFilter \"box\" 1 1
Option \"shaderformat\" \"default\" [\"${_f}\"]
Option \"limits\" \"numthreads\" [1]
Quantize \"rgba\" 0 0 0 0
Display \"${_scene}.tif\" \"file\" \"rgba\"
Format 8 8 1
Projection \"orthographic\"
Clipping 0.1 100
Translate 0 0 5
WorldBegin
  AttributeBegin
${_rattr}    Surface \"visibility169_surface\" \"float dist\" [${_dist}]
    Disk 0 30 360
  AttributeEnd
${_occ}WorldEnd
")
            set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/visibility169/${_scene})
            file(MAKE_DIRECTORY ${_scratch})
            add_test(NAME Visibility169_${_name}_${_h}_${_f}
                COMMAND test_pixel_probe $<TARGET_FILE:orender> ${VIS169_DIR}/${_scene}.rib ${_scene}.tif 4 4 0.000001 ${_want} ${_want} ${_want}
                WORKING_DIRECTORY ${_scratch})
            set_tests_properties(Visibility169_${_name}_${_h}_${_f} PROPERTIES TIMEOUT 120
                LABELS "visual;regression;shading_parity;visibility169" ENVIRONMENT "${_v_env}" DEPENDS orender)
            if(_f STREQUAL "slo" AND NOT OPENRENDER_HAVE_LLVM)
                set_tests_properties(Visibility169_${_name}_${_h}_${_f} PROPERTIES DISABLED TRUE)
            endif()
        endforeach()
    endforeach()
endforeach()

# the scene of the report
foreach(_f ${_v_formats})
    set(_scene light_raytrace_${_f})
    file(WRITE ${VIS169_DIR}/${_scene}.rib
"##RenderMan RIB-Structure 1.0
# GitHub #169, written by visibility-scenes.cmake.
version 3.03
Hider \"raytrace\" \"jitter\" [0]
PixelSamples 1 1
PixelFilter \"box\" 1 1
Option \"shaderformat\" \"default\" [\"${_f}\"]
Option \"limits\" \"numthreads\" [1]
Quantize \"rgba\" 0 0 0 0
Display \"${_scene}.tif\" \"file\" \"rgba\"
Projection \"perspective\" \"fov\" 50
Format 256 256 1
Clipping 0.1 1000
Translate 0 0 9
Rotate -40 1 0 0
WorldBegin
  LightSource \"visibility169_light\" 1 \"from\" [0 4 0] \"intensity\" [0.8]
  AttributeBegin
    Attribute \"visibility\" \"int transmission\" [1]
    Surface \"constant\"
    Translate 0 2 0
    Rotate 90 1 0 0
    Disk 0 0.8 360
  AttributeEnd
  AttributeBegin
    Surface \"matte\"
    Polygon \"P\" [-6 0 -6  6 0 -6  6 0 6  -6 0 6]
  AttributeEnd
WorldEnd
")
    set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/visibility169/${_scene})
    file(MAKE_DIRECTORY ${_scratch})
    add_test(NAME Visibility169_${_scene}
        COMMAND test_pixel_probe $<TARGET_FILE:orender> ${VIS169_DIR}/${_scene}.rib ${_scene}.tif 128 158 0.000001 0 0 0
        WORKING_DIRECTORY ${_scratch})
    set_tests_properties(Visibility169_${_scene} PROPERTIES TIMEOUT 120
        LABELS "visual;regression;shading_parity;visibility169" ENVIRONMENT "${_v_env}" DEPENDS orender)
    if(_f STREQUAL "slo" AND NOT OPENRENDER_HAVE_LLVM)
        set_tests_properties(Visibility169_${_scene} PROPERTIES DISABLED TRUE)
    endif()
endforeach()
