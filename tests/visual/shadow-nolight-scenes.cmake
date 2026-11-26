# GitHub #236: a traced shadow() reads L, which only a light's illuminate() writes. Included from CMakeLists.txt after
# lightsource-scenes.cmake. The probe shaders are test-only, compiled into the build tree for both formats and found through a SHADERS
# path that lists their directory first; the scenes are written into the build tree.
#
# Every scene is an 8 x 8 frame, one thread, a probe disk in the middle (pixel 4,4) and an opaque wall behind it for the shadow ray to
# meet. The "stale" scenes first shade a lit disk of the probe's own size, nearer the camera and in the top-left corner of the frame
# (clear of the probe pixel), in an attribute block of its own, which leaves a light's L and Cl in the shading state's buffers; the
# "fresh" ones do not. Four families:
#   Shadow236_nolight_*     shadow("raytrace", P) from a surface with no light at all: exactly 0, in the float and the colour form
#   Shadow236_afterloop_*   the same after an illuminance loop (it must also have counted its one light)
#   Shadow236_readsL_*      (a) a surface reading L and Cl outside illuminance: the stale and the fresh scene agree
#   Shadow236_prelight_*    (b) a light calling shadow("raytrace") before illuminate(): the stale and the fresh scene agree

set(SHADOW236_DIR ${CMAKE_CURRENT_BINARY_DIR}/shadow236)
file(MAKE_DIRECTORY ${SHADOW236_DIR})
set(_sh_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _sh_formats slo)
endif()
set(_sh_outputs "")
foreach(_sh_name shadow_nolight_surface shadow_afterloop_surface shadow_readsL_surface shadow_sumcl_surface shadow_prelight_light shadow_maxdist_light)
    set(_sh_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_sh_name}.sl)
    foreach(_sh_fmt ${_sh_formats})
        set(_sh_jit "")
        if(_sh_fmt STREQUAL "slo")
            set(_sh_jit -DJIT=ON)
        endif()
        add_custom_command(
            OUTPUT ${SHADOW236_DIR}/${_sh_name}.${_sh_fmt}
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sh_sl} -DOUT=${SHADOW236_DIR}/${_sh_name}.${_sh_fmt}
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_sh_jit}
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_sh_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${SHADOW236_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _sh_outputs ${SHADOW236_DIR}/${_sh_name}.${_sh_fmt})
    endforeach()
endforeach()
add_custom_target(shadow236_shaders ALL DEPENDS ${_sh_outputs})
add_dependencies(shadow236_shaders openrender_shaders)

set(_sh_env "${VISUAL_ENV}")
list(TRANSFORM _sh_env REPLACE "^SHADERS=(.*)$" "SHADERS=${SHADOW236_DIR}:\\1")

# shadow236_scene(<name> <hider> <format> <stale: 0|1> <probe light line>) writes ${SHADOW236_DIR}/<name>.rib; the probe's surface is
# SURFACE in the caller's scope
function(shadow236_scene NAME HIDER FORMAT STALE PROBE_LIGHT)
    set(_prior "")
    if(STALE)
        # a lit disk of the probe's own size in the top-left corner: in the plane of the probe for the raytrace hider, nearer the camera
        # for reyes and zbuffer. Which object reaches the buffer index of the probe pixel with its L depends on the order a hider shades
        # in (measured, not derived), and this is the placement that leaves the starting tree's L in that slot for each
        set(_prior "")
        set(_zs -1)
        if(HIDER STREQUAL "raytrace")
            set(_zs 0)
        endif()
        foreach(_z ${_zs})
            set(_prior "${_prior}  AttributeBegin
    LightSource \"distantlight\" 1 \"from\" [3 5 -4] \"to\" [0 0 0]
    Surface \"matte\"
    Translate -0.6 0.6 ${_z}
    Disk 0 0.6 360
  AttributeEnd
")
        endforeach()
    endif()
    file(WRITE ${SHADOW236_DIR}/${NAME}.rib
"##RenderMan RIB-Structure 1.0
# GitHub #236, written by shadow-nolight-scenes.cmake.
version 3.03
Hider \"${HIDER}\" \"jitter\" [0]
PixelSamples 1 1
PixelFilter \"box\" 1 1
Option \"shaderformat\" \"default\" [\"${FORMAT}\"]
Option \"limits\" \"numthreads\" [1]
Quantize \"rgba\" 0 0 0 0
Display \"${NAME}.tif\" \"file\" \"rgba\"
Format 8 8 1
Projection \"perspective\" \"fov\" 30
Clipping 0.1 100
Translate 0 0 5
WorldBegin
  Attribute \"visibility\" \"string transmission\" [\"opaque\"]
${_prior}  AttributeBegin
${PROBE_LIGHT}    Surface \"${SURFACE}\"
    Translate 0.2 -0.2 0
    Disk 0 0.6 360
  AttributeEnd
  AttributeBegin
    Surface \"matte\"
    Translate 0 0 2
    Disk 0 30 360
  AttributeEnd
WorldEnd
")
endfunction()

# the light of an afterloop scene points one way or the other along z, so that the L it leaves behind is a ray toward the wall in one
# of the two (which one depends on the sign convention of L, and the test must not)
set(_sh_loop_light_a "    LightSource \"distantlight\" 7 \"from\" [0 0 -1] \"to\" [0 0 0]\n")
set(_sh_loop_light_b "    LightSource \"distantlight\" 7 \"from\" [0 0 1] \"to\" [0 0 0]\n")
set(_sh_pre_light "    LightSource \"shadow_prelight_light\" 7\n")
foreach(_h reyes zbuffer raytrace)
    foreach(_f ${_sh_formats})
        set(_tag ${_h}_${_f})
        set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/shadow236)
        file(MAKE_DIRECTORY ${_scratch})
        if(_f STREQUAL "slo" AND NOT OPENRENDER_HAVE_LLVM)
            set(_off DISABLED TRUE)
        endif()

        # no light at all (a stale variant gave 0 on the starting tree for every hider, by the luck of its ray, so it is not kept)
        foreach(_s fresh)
            set(_stale 0)
            if(_s STREQUAL "stale")
                set(_stale 1)
            endif()
            set(SURFACE shadow_nolight_surface)
            shadow236_scene(nolight_${_s}_${_tag} ${_h} ${_f} ${_stale} "")
            add_test(NAME Shadow236_nolight_${_s}_${_tag}
                COMMAND test_pixel_probe $<TARGET_FILE:orender> ${SHADOW236_DIR}/nolight_${_s}_${_tag}.rib nolight_${_s}_${_tag}.tif 4 4 0.000001 0 0 0
                WORKING_DIRECTORY ${_scratch})
            set_tests_properties(Shadow236_nolight_${_s}_${_tag} PROPERTIES TIMEOUT 120 LABELS "visual;regression;shading_parity;shadow236" ENVIRONMENT "${_sh_env}" DEPENDS orender)
        endforeach()

        # after an illuminance loop (one light): shadow is 0, and the loop counted its light
        foreach(_v a b)
            set(SURFACE shadow_afterloop_surface)
            shadow236_scene(afterloop_${_v}_${_tag} ${_h} ${_f} 1 "${_sh_loop_light_${_v}}")
            add_test(NAME Shadow236_afterloop_${_v}_${_tag}
                COMMAND test_pixel_probe $<TARGET_FILE:orender> ${SHADOW236_DIR}/afterloop_${_v}_${_tag}.rib afterloop_${_v}_${_tag}.tif 4 4 0.000001 0 0 1
                WORKING_DIRECTORY ${_scratch})
            set_tests_properties(Shadow236_afterloop_${_v}_${_tag} PROPERTIES TIMEOUT 120 LABELS "visual;regression;shading_parity;shadow236" ENVIRONMENT "${_sh_env}" DEPENDS orender)
        endforeach()

        # (a) a surface reading L and Cl outside illuminance, (b) a light calling shadow() before illuminate(): stale equals fresh
        set(SURFACE shadow_readsL_surface)
        shadow236_scene(readsL_fresh_${_tag} ${_h} ${_f} 0 "")
        shadow236_scene(readsL_stale_${_tag} ${_h} ${_f} 1 "")
        add_test(NAME Shadow236_readsL_${_tag}
            COMMAND test_crop_compare $<TARGET_FILE:orender> ${SHADOW236_DIR}/readsL_fresh_${_tag}.rib readsL_fresh_${_tag}.tif
                    ${SHADOW236_DIR}/readsL_stale_${_tag}.rib readsL_stale_${_tag}.tif 4 4 4 4
            WORKING_DIRECTORY ${_scratch})
        set_tests_properties(Shadow236_readsL_${_tag} PROPERTIES TIMEOUT 120 LABELS "visual;regression;shading_parity;shadow236" ENVIRONMENT "${_sh_env}" DEPENDS orender)

        set(SURFACE shadow_sumcl_surface)
        shadow236_scene(prelight_fresh_${_tag} ${_h} ${_f} 0 "${_sh_pre_light}")
        shadow236_scene(prelight_stale_${_tag} ${_h} ${_f} 1 "${_sh_pre_light}")
        add_test(NAME Shadow236_prelight_${_tag}
            COMMAND test_crop_compare $<TARGET_FILE:orender> ${SHADOW236_DIR}/prelight_fresh_${_tag}.rib prelight_fresh_${_tag}.tif
                    ${SHADOW236_DIR}/prelight_stale_${_tag}.rib prelight_stale_${_tag}.tif 4 4 4 4
            WORKING_DIRECTORY ${_scratch})
        set_tests_properties(Shadow236_prelight_${_tag} PROPERTIES TIMEOUT 120 LABELS "visual;regression;shading_parity;shadow236" ENVIRONMENT "${_sh_env}" DEPENDS orender)
        if(_off)
            set_tests_properties(Shadow236_nolight_fresh_${_tag} Shadow236_afterloop_a_${_tag} Shadow236_afterloop_b_${_tag}
                Shadow236_readsL_${_tag} Shadow236_prelight_${_tag} PROPERTIES DISABLED TRUE)
        endif()
        unset(_off)
    endforeach()
endforeach()

# The control that replaces ShadingParity_JitPLEngaged_envshadow_raytrace_maxdist_*: a traced shadow() called inside a light, with an
# occluder in the ray's way, honours "maxdist". The base scene (no maxdist) and the maxdist scene must differ.
foreach(_h raytrace reyes)
    foreach(_f ${_sh_formats})
        foreach(_c base short)
            set(_md "")
            if(_c STREQUAL "short")
                set(_md " \"maxdist\" [0.3]")
            endif()
            file(WRITE ${SHADOW236_DIR}/maxdist_${_c}_${_h}_${_f}.rib
"##RenderMan RIB-Structure 1.0
# GitHub #236, written by shadow-nolight-scenes.cmake.
version 3.03
Hider \"${_h}\" \"jitter\" [0]
PixelSamples 1 1
PixelFilter \"box\" 1 1
Option \"shaderformat\" \"default\" [\"${_f}\"]
Option \"limits\" \"numthreads\" [1]
Quantize \"rgba\" 0 0 0 0
Display \"maxdist_${_c}_${_h}_${_f}.tif\" \"file\" \"rgba\"
Format 8 8 1
Projection \"perspective\" \"fov\" 30
Clipping 0.1 100
Translate 0 0 5
WorldBegin
  Attribute \"visibility\" \"string transmission\" [\"opaque\"]
  AttributeBegin
    Surface \"matte\"
    Translate 0 0 1
    Disk 0 30 360
  AttributeEnd
  AttributeBegin
    LightSource \"shadow_maxdist_light\" 7 \"from\" [0 0 3]${_md}
    Surface \"shadow_sumcl_surface\"
    Translate 0.2 -0.2 0
    Disk 0 0.6 360
  AttributeEnd
WorldEnd
")
        endforeach()
        add_test(NAME Shadow236_maxdist_${_h}_${_f}
            COMMAND test_render_compare $<TARGET_FILE:orender> differ
                ${SHADOW236_DIR}/maxdist_base_${_h}_${_f}.rib maxdist_base_${_h}_${_f}.tif
                ${SHADOW236_DIR}/maxdist_short_${_h}_${_f}.rib maxdist_short_${_h}_${_f}.tif
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/visual/shadow236)
        set_tests_properties(Shadow236_maxdist_${_h}_${_f} PROPERTIES TIMEOUT 120 LABELS "visual;regression;shading_parity;shadow236" ENVIRONMENT "${_sh_env}" DEPENDS orender)
        if(_f STREQUAL "slo" AND NOT OPENRENDER_HAVE_LLVM)
            set_tests_properties(Shadow236_maxdist_${_h}_${_f} PROPERTIES DISABLED TRUE)
        endif()
    endforeach()
endforeach()
