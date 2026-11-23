# GitHub #117: camerahitmode on every camera hider and every primitive kind.
#
# Included from CMakeLists.txt after arealight-scenes.cmake. The scenes are examples/rib/tests/camerahitmode-*.rib; the
# probe shader (tests/visual/shaders/camerahitmode_probe.sl) is test-only, compiled into the build tree and found through a
# SHADERS path that lists its directory first, like transmission_destination_probe.
#
# camerahitmode is a hint about the opacity of a camera-visible object: the shader runs whatever the mode, and "primitive"
# replaces the shader's Oi by the Opacity attribute (PRMan 12.5 release notes; PRMan User's Manual, Attributes). The probe
# returns Ci (0.2 0.4 0.6) and Oi 0.25, the Opacity attribute is 1:
#   "primitive": RGBA (0.2 0.4 0.6 1)      "shader": RGBA (0.2 0.4 0.6 0.25)
# Same probe values for the reyes, zbuffer and raytrace hiders.

set(CAMERAHITMODE_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${CAMERAHITMODE_SHADER_DIR})
set(_chm_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/camerahitmode_probe.sl)
add_custom_command(
    OUTPUT ${CAMERAHITMODE_SHADER_DIR}/camerahitmode_probe.rslo
    COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_chm_sl}
            -DOUT=${CAMERAHITMODE_SHADER_DIR}/camerahitmode_probe.rslo
            -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes
            -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
    DEPENDS ${_chm_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
    WORKING_DIRECTORY ${CAMERAHITMODE_SHADER_DIR}
    COMMENT ""
    VERBATIM)
add_custom_target(camerahitmode_probe_shader ALL DEPENDS ${CAMERAHITMODE_SHADER_DIR}/camerahitmode_probe.rslo)
add_dependencies(camerahitmode_probe_shader openrender_shaders)

set(_chm_env "${VISUAL_ENV}")
list(TRANSFORM _chm_env REPLACE "^SHADERS=(.*)$" "SHADERS=${CAMERAHITMODE_SHADER_DIR}:\\1")

# add_camerahitmode_probe(<mode> <primitive> <hider> <alpha>)
macro(add_camerahitmode_probe MODE PRIM HIDER ALPHA)
    set(_chm_name camerahitmode-${MODE}-${PRIM}-${HIDER})
    set(_chm_scratch ${CMAKE_BINARY_DIR}/tests/visual/${_chm_name})
    file(MAKE_DIRECTORY ${_chm_scratch})
    add_test(NAME CameraHitMode_${MODE}_${PRIM}_${HIDER}
        COMMAND test_pixel_probe $<TARGET_FILE:orender> ${CMAKE_SOURCE_DIR}/examples/rib/tests/${_chm_name}.rib
                ${_chm_name}.tif 4 4 0.00001 0.2 0.4 0.6 ${ALPHA}
        WORKING_DIRECTORY ${_chm_scratch})
    set_tests_properties(CameraHitMode_${MODE}_${PRIM}_${HIDER} PROPERTIES
        TIMEOUT 120 LABELS "visual;regression;shading_parity" ENVIRONMENT "${_chm_env}" DEPENDS orender)
endmacro()

foreach(_hider reyes zbuffer raytrace)
    add_camerahitmode_probe(primitive disk ${_hider} 1)
endforeach()
foreach(_hider reyes raytrace)
    add_camerahitmode_probe(shader disk ${_hider} 0.25)
endforeach()
# zbuffer is opaque-only (it ignores Oi), so "shader" has no probe there. Raytrace draws no Points (GitHub #195), so a points
# primitive is probed under reyes and zbuffer only. The zbuffer one has a Clipping statement and hung before GitHub #198
# (the point rasterizer tested the Clipping option bit as RASTER_UNSHADED), hence the short timeout.
add_camerahitmode_probe(primitive points reyes 1)
add_camerahitmode_probe(primitive points zbuffer 1)
set_tests_properties(CameraHitMode_primitive_points_zbuffer PROPERTIES TIMEOUT 30)
add_camerahitmode_probe(shader points reyes 0.25)
# A camera hit reads camerahitmode, never specularhitmode: specular "primitive" with camera "shader" still runs the shader
add_camerahitmode_probe(specprim-camshader disk raytrace 0.25)

# GitHub #198: points under the zbuffer hider with no Clipping statement. The point rasterizer read RASTER_UNSHADED, RASTER_MATTE
# and RASTER_SHADE_HIDDEN from CRenderer::flags instead of the grid's flags, so with no Clipping it never shaded a point
# (a plain point came out black) and with a Clipping statement (option bit 3) it hung. The zbuffer hider is opaque-only: alpha 1.
#   points-shaded-zbuffer: the shader's colour (0.9 0.1 0.1, 1)      matte-points-zbuffer: a matte point is black (0 0 0, 1)
# The matte probe cannot fail on its own against the unfixed tree (no point was shaded there, so it was black anyway); it
# pins the RASTER_MATTE test of the fixed tree, and fails when only that line is reverted.
macro(add_zbuffer_points_probe NAME R G B A)
    set(_mp_scratch ${CMAKE_BINARY_DIR}/tests/visual/${NAME})
    file(MAKE_DIRECTORY ${_mp_scratch})
    add_test(NAME ZbufferPoints_${NAME}
        COMMAND test_pixel_probe $<TARGET_FILE:orender> ${CMAKE_SOURCE_DIR}/examples/rib/tests/${NAME}.rib
                ${NAME}.tif 4 4 0.00001 ${R} ${G} ${B} ${A}
        WORKING_DIRECTORY ${_mp_scratch})
    set_tests_properties(ZbufferPoints_${NAME} PROPERTIES
        TIMEOUT 30 LABELS "visual;regression" ENVIRONMENT "${VISUAL_ENV}" DEPENDS orender)
endmacro()
add_zbuffer_points_probe(points-shaded-zbuffer 0.9 0.1 0.1 1)
add_zbuffer_points_probe(matte-points-zbuffer 0 0 0 1)
