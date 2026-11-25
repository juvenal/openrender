# GitHub #209: UniformLifting must not make the output variable of a message-passing builtin uniform.
#
# Included from CMakeLists.txt after lightsource-scenes.cmake. surface(), displacement(), atmosphere(), incident(), opposite(),
# attribute(), option(), rendererinfo() and textureinfo() write their output operand as a call argument, not as an instruction's
# result, so the pass saw `float v = -1;` as written once by a constant and made it uniform; a varying output parameter of the
# other shader was then refused (varying into uniform is a failure). Each reader initialises a varying local from a constant and
# passes it as the output. The centre-left pixel (2, 4) holds Ci = (value, found, 0), a number checked exactly (test_pixel_probe).
#
# Written to fail on the unfixed tree as .slo: displacement, atmosphere and surface (the other shader's output is a varying
# parameter that is read). incident, opposite and attribute read a uniform stored value, so they cannot fail before the fix and
# are guards. textureinfo has no reachable probe (it has no JIT form and a texture file would be needed). The shaders are
# test-only, compiled into the build tree and found through a SHADERS path that lists their directory first; the scenes are
# written into the build tree too.

set(MSGOUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/msgout)
file(MAKE_DIRECTORY ${MSGOUT_DIR})
set(_mo_outputs "")
set(_mo_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _mo_formats slo)
endif()
foreach(_mo_name msgout_disp msgout_disp_surface msgout_atmos msgout_atmos_surface msgout_surf_light msgout_surf_surface
                 msgout_vol msgout_inc_b msgout_scene_surface)
    set(_mo_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_mo_name}.sl)
    foreach(_mo_fmt ${_mo_formats})
        set(_mo_jit "")
        if(_mo_fmt STREQUAL "slo")
            set(_mo_jit -DJIT=ON)
        endif()
        add_custom_command(
            OUTPUT ${MSGOUT_DIR}/${_mo_name}.${_mo_fmt}
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_mo_sl}
                    -DOUT=${MSGOUT_DIR}/${_mo_name}.${_mo_fmt}
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_mo_jit}
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_mo_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${MSGOUT_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _mo_outputs ${MSGOUT_DIR}/${_mo_name}.${_mo_fmt})
    endforeach()
endforeach()
add_custom_target(msgout_shaders ALL DEPENDS ${_mo_outputs})
add_dependencies(msgout_shaders openrender_shaders)

set(_mo_env "${VISUAL_ENV}")
list(TRANSFORM _mo_env REPLACE "^SHADERS=(.*)$" "SHADERS=${MSGOUT_DIR}:\\1")

# add_msgout_probe(<case> <hider> <format> <preamble> <attributes> <surface-statement> <v> <found>)
function(add_msgout_probe CASE HIDER FMT PREAMBLE BLOCK V FOUND)
    set(_name ${CASE}-${FMT}-${HIDER})
    set(_rib ${MSGOUT_DIR}/${_name}.rib)
    file(WRITE ${_rib}
"##RenderMan RIB-Structure 1.0
# GitHub #209, written by msgout-scenes.cmake.
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
${PREAMBLE}
WorldBegin
${BLOCK}
  Disk 0 1 360
WorldEnd
")
    set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/msgout-${_name})
    file(MAKE_DIRECTORY ${_scratch})
    add_test(NAME MsgOut_${_name}
        COMMAND test_pixel_probe $<TARGET_FILE:orender> ${_rib} ${_name}.tif 2 4 0.0001 ${V} ${FOUND} 0
        WORKING_DIRECTORY ${_scratch})
    set_tests_properties(MsgOut_${_name} PROPERTIES
        TIMEOUT 120 LABELS "visual;regression;shading_parity;msgout" ENVIRONMENT "${_mo_env}" DEPENDS orender)
endfunction()

foreach(_mo_hider reyes raytrace)
    foreach(_mo_fmt ${_mo_formats})
        add_msgout_probe(displacement ${_mo_hider} ${_mo_fmt} ""
            "  Displacement \"msgout_disp\"\n  Surface \"msgout_disp_surface\"" 0 1)
        add_msgout_probe(atmosphere ${_mo_hider} ${_mo_fmt} "Atmosphere \"msgout_atmos\""
            "  Surface \"msgout_atmos_surface\"" 0.5 1)
        add_msgout_probe(surface ${_mo_hider} ${_mo_fmt} ""
            "  LightSource \"msgout_surf_light\" 1\n  Surface \"msgout_surf_surface\"" 0.5 1)
        # guards: the stored value is uniform, so these cannot fail on the unfixed tree
        add_msgout_probe(incident ${_mo_hider} ${_mo_fmt} ""
            "  Exterior \"msgout_vol\"\n  Interior \"msgout_vol\"\n  Surface \"msgout_inc_b\" \"float kind\" [0]" 0.5 1)
        add_msgout_probe(opposite ${_mo_hider} ${_mo_fmt} ""
            "  Exterior \"msgout_vol\"\n  Interior \"msgout_vol\"\n  Surface \"msgout_inc_b\" \"float kind\" [1]" 0.5 1)
        add_msgout_probe(attribute ${_mo_hider} ${_mo_fmt} "" "  Surface \"msgout_scene_surface\"" 1 1)
    endforeach()
endforeach()
