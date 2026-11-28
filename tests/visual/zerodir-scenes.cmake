# GitHub #156: a ray with an exactly zero z direction component missed every bilinear quad it crossed. The three bilinear
# intersectors (CPolygonQuad, CBilinearPatch and the tessellation's intersectQuadsBilin) eliminated the ray parameter through
# the z axis, so for dir.z == 0 their two equations became the same one and no root came out. Included from CMakeLists.txt
# after visibility-scenes.cmake. The probe shader is test-only (compiled into the build tree for both formats, found through a
# SHADERS path that lists its directory first) and the scenes are written into the build tree.
#
#   ZeroDir156_<case>_<hider>_<fmt>   an 8 x 8 orthographic frame (as Visibility169_*), pixel (4,4), float RGB. The receiver is a
#                                     disk at z = 0 and the probe shoots one ray from its P. A half-opaque sheet (transmission
#                                     visible) lies across the ray's path.
#       poly-x          4-vertex Polygon (a CPolygonQuad) at x = 1, ray along +x, direction (1, 0, 0)       0.5 (1 on the starting tree)
#       poly-y          the same sheet at y = 1, ray along +y, direction (0, 1, 0)                         0.5 (1)
#       bilinear-x      Patch "bilinear" (CBilinearPatch) at x = 1, ray along +x                           0.5 (1)
#       bicubic-x       Patch "bicubic" (a CTesselationPatch, intersectQuadsBilin) at x = 1, ray along +x   0.5 (1)
#       visibility-x    the Polygon sheet, opaque and flagged, visibility(): 0 (1)
#       poly-x-dz       the Polygon sheet, ray (1, 0, -0.001): z component not zero, 0.5 on every tree (control)
#       poly-z          a sheet at z = 1, ray along +z: 0.5 on every tree (control)
#       visibility-x-dz the visibility case with z component -0.001: 0 on every tree (control)

set(ZD156_DIR ${CMAKE_CURRENT_BINARY_DIR}/zerodir156)
file(MAKE_DIRECTORY ${ZD156_DIR})
set(_z_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _z_formats slo)
endif()
set(_z_outputs "")
set(_z_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/zerodir156_probe.sl)
foreach(_z_fmt ${_z_formats})
    set(_z_jit "")
    if(_z_fmt STREQUAL "slo")
        set(_z_jit -DJIT=ON)
    endif()
    add_custom_command(
        OUTPUT ${ZD156_DIR}/zerodir156_probe.${_z_fmt}
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_z_sl} -DOUT=${ZD156_DIR}/zerodir156_probe.${_z_fmt}
                -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_z_jit}
                -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        DEPENDS ${_z_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        WORKING_DIRECTORY ${ZD156_DIR}
        COMMENT ""
        VERBATIM)
    list(APPEND _z_outputs ${ZD156_DIR}/zerodir156_probe.${_z_fmt})
endforeach()
add_custom_target(zerodir156_shaders ALL DEPENDS ${_z_outputs})
add_dependencies(zerodir156_shaders openrender_shaders)

set(_z_env "${VISUAL_ENV}")
list(TRANSFORM _z_env REPLACE "^SHADERS=(.*)$" "SHADERS=${ZD156_DIR}:\\1")

# the sheets, in world space (the camera only translates in z)
set(_z_poly_x "Polygon \"P\" [1 -3 -3  1 -3 3  1 3 3  1 3 -3]")
set(_z_poly_y "Polygon \"P\" [-3 1 -3  -3 1 3  3 1 3  3 1 -3]")
set(_z_poly_z "Polygon \"P\" [-3 -3 1  3 -3 1  3 3 1  -3 3 1]")
set(_z_bilinear_x "Patch \"bilinear\" \"P\" [1 -3 -3  1 3 -3  1 -3 3  1 3 3]")
# the z range is off centre (-2.9 to 3.1): the probe's ray runs at z = 0, and a ray exactly on a seam between two tessellated
# quads is rejected by both (strict 0 < u, v < 1), which is a different defect from this one
set(_z_bicubic_x "Patch \"bicubic\" \"P\" [1 -3 -2.9  1 -1 -2.9  1 1 -2.9  1 3 -2.9  1 -3 -0.9  1 -1 -0.9  1 1 -0.9  1 3 -0.9  1 -3 1.1  1 -1 1.1  1 1 1.1  1 3 1.1  1 -3 3.1  1 -1 3.1  1 1 3.1  1 3 3.1]")

# case: name | sheet | dx | dy | dz | mode | opacity | want
set(_z_cases
    "poly-x|poly_x|3|0|0|transmission|0.5|0.5"
    "poly-y|poly_y|0|3|0|transmission|0.5|0.5"
    "bilinear-x|bilinear_x|3|0|0|transmission|0.5|0.5"
    "bicubic-x|bicubic_x|3|0|0|transmission|0.5|0.5"
    "visibility-x|poly_x|3|0|0|visibility|1|0"
    "poly-x-dz|poly_x|3|0|-0.001|transmission|0.5|0.5"
    "poly-z|poly_z|0|0|2|transmission|0.5|0.5"
    "visibility-x-dz|poly_x|3|0|-0.001|visibility|1|0")

foreach(_h raytrace reyes)
    foreach(_f ${_z_formats})
        foreach(_c ${_z_cases})
            string(REPLACE "|" ";" _p "${_c}")
            list(GET _p 0 _name)
            list(GET _p 1 _sheet)
            list(GET _p 2 _dx)
            list(GET _p 3 _dy)
            list(GET _p 4 _dz)
            list(GET _p 5 _mode)
            list(GET _p 6 _opacity)
            list(GET _p 7 _want)
            set(_scene ${_name}_${_h}_${_f})
            file(WRITE ${ZD156_DIR}/${_scene}.rib
"##RenderMan RIB-Structure 1.0
# GitHub #156, written by zerodir-scenes.cmake.
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
    Surface \"zerodir156_probe\" \"float dx\" [${_dx}] \"float dy\" [${_dy}] \"float dz\" [${_dz}] \"string mode\" [\"${_mode}\"]
    Disk 0 30 360
  AttributeEnd
  AttributeBegin
    Attribute \"visibility\" \"int transmission\" [1]
    Opacity ${_opacity} ${_opacity} ${_opacity}
    Surface \"constant\"
    ${_z_${_sheet}}
  AttributeEnd
WorldEnd
")
            set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/zerodir156/${_scene})
            file(MAKE_DIRECTORY ${_scratch})
            add_test(NAME ZeroDir156_${_name}_${_h}_${_f}
                COMMAND test_pixel_probe $<TARGET_FILE:orender> ${ZD156_DIR}/${_scene}.rib ${_scene}.tif 4 4 0.000001 ${_want} ${_want} ${_want}
                WORKING_DIRECTORY ${_scratch})
            set_tests_properties(ZeroDir156_${_name}_${_h}_${_f} PROPERTIES TIMEOUT 120
                LABELS "visual;regression;shading_parity;zerodir156" ENVIRONMENT "${_z_env}" DEPENDS orender)
            if(_f STREQUAL "slo" AND NOT OPENRENDER_HAVE_LLVM)
                set_tests_properties(ZeroDir156_${_name}_${_h}_${_f} PROPERTIES DISABLED TRUE)
            endif()
        endforeach()
    endforeach()
endforeach()
