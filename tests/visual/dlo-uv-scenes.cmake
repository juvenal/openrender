# GitHub #234: a dynamic-load object (Geometry "dlo") ray hit has defined u, v, s, t, du, dv, dPdu, dPdv and time (all 0: the dlo
# interface supplies no parametrisation). Included from CMakeLists.txt. The probe shaders are test-only, compiled into the build tree
# for both formats and found through a SHADERS path that lists their directory first; the scenes are written into the build tree at
# generate time (the fixture's path is a generator expression). Every scene is raytrace, 32 x 32, one thread. A plastic sphere in the
# top-left corner is shaded before the dlo in the same frame, which leaves its u, v and derivatives in the shading state's buffers
# (a fresh process reads zero pages and would pass on the starting tree by luck).
#   DloUv_width_*   (du, dv, u + v + s + t) at the dlo's pixel: 0 0 0, alpha 1
#   DloUv_deriv_*   (|dPdu| + |dPdv|, |Du(P)| + |Dv(P)|, area(P)): 0 0 1e-6, alpha 1
#   DloUv_mirror_*  the dlo reached by a mirror's secondary ray (depth 1), with a plastic sphere reached the same way: 0.25 0.5 0.75, alpha 1
#                   (the marked shader adds (0.25, 0.5, 0.75), so a hit is told from a miss)

set(DLOUV_DIR ${CMAKE_CURRENT_BINARY_DIR}/dlouv)
file(MAKE_DIRECTORY ${DLOUV_DIR})
set(_du_formats rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND _du_formats slo)
endif()
set(_du_outputs "")
foreach(_du_name dlo_uv_width dlo_uv_width_marked dlo_uv_deriv dlo_uv_mirror)
    set(_du_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_du_name}.sl)
    foreach(_du_fmt ${_du_formats})
        set(_du_jit "")
        if(_du_fmt STREQUAL "slo")
            set(_du_jit -DJIT=ON)
        endif()
        add_custom_command(
            OUTPUT ${DLOUV_DIR}/${_du_name}.${_du_fmt}
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_du_sl} -DOUT=${DLOUV_DIR}/${_du_name}.${_du_fmt}
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_du_jit}
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_du_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${DLOUV_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _du_outputs ${DLOUV_DIR}/${_du_name}.${_du_fmt})
    endforeach()
endforeach()
add_custom_target(dlouv_shaders ALL DEPENDS ${_du_outputs})
add_dependencies(dlouv_shaders openrender_shaders)

set(_du_env "${VISUAL_ENV}")
list(TRANSFORM _du_env REPLACE "^SHADERS=(.*)$" "SHADERS=${DLOUV_DIR}:\\1")

set(_du_scratch ${CMAKE_BINARY_DIR}/tests/visual/dlouv)
file(MAKE_DIRECTORY ${_du_scratch})
foreach(_du_fmt ${_du_formats})
    # <name>: the probe shader; <body>: the scene's geometry after the dirtying sphere
    foreach(_du_case width deriv mirror)
        set(_du_tag ${_du_case}_${_du_fmt})
        if(_du_case STREQUAL "mirror")
            # the camera is at z = -5 looking along +z: the disk at z = 0 reflects the centre pixel straight back, past the camera, to
            # the dlo at z = -10
            set(_du_body "  Attribute \"visibility\" \"int specular\" [1]
  AttributeBegin
    Surface \"dlo_uv_mirror\"
    Disk 0 3 360
  AttributeEnd
  AttributeBegin
    Surface \"dlo_uv_width_marked\"
    Translate 0 0 -10
    Geometry \"dlo\" \"string file\" [\"$<TARGET_FILE:dlo_sphere>\"]
  AttributeEnd
  AttributeBegin
    Surface \"dlo_uv_mirror\"
    Translate -3 3 0
    Disk 0 1 360
  AttributeEnd
  AttributeBegin
    Surface \"plastic\"
    Translate -3 3 -10
    Sphere 1.2 -1.2 1.2 360
  AttributeEnd
")
        else()
            set(_du_body "  AttributeBegin
    Surface \"dlo_uv_${_du_case}\"
    Geometry \"dlo\" \"string file\" [\"$<TARGET_FILE:dlo_sphere>\"]
  AttributeEnd
")
        endif()
        file(GENERATE OUTPUT ${_du_scratch}/${_du_tag}.rib CONTENT
"##RenderMan RIB-Structure 1.0
# GitHub #234, written by dlo-uv-scenes.cmake.
version 3.03
Hider \"raytrace\" \"jitter\" [0]
Option \"shaderformat\" \"default\" [\"${_du_fmt}\"]
Option \"limits\" \"numthreads\" [1]
Option \"trace\" \"int maxdepth\" [2]
PixelSamples 1 1
PixelFilter \"box\" 1 1
Quantize \"rgba\" 0 0 0 0
Display \"${_du_tag}.tif\" \"file\" \"rgba\"
Format 32 32 1
Projection \"perspective\"
Clipping 0.1 100
Translate 0 0 5
WorldBegin
  AttributeBegin
    Surface \"plastic\"
    Translate -3 3 0
    Sphere 1.2 -1.2 1.2 360
  AttributeEnd
${_du_body}WorldEnd
")
        set(_du_want 0 0 0 1)
        if(_du_case STREQUAL "mirror")
            set(_du_want 0.25 0.5 0.75 1)
        endif()
        if(_du_case STREQUAL "deriv")
            set(_du_want 0 0 0.000001 1)
        endif()
        add_test(NAME DloUv_${_du_tag}
            COMMAND test_pixel_probe $<TARGET_FILE:orender> ${_du_scratch}/${_du_tag}.rib ${_du_tag}.tif 16 16 0.000005 ${_du_want}
            WORKING_DIRECTORY ${_du_scratch})
        set_tests_properties(DloUv_${_du_tag} PROPERTIES TIMEOUT 120 LABELS "visual;regression;dlo_uv" ENVIRONMENT "${_du_env}" DEPENDS orender)
        if(_du_fmt STREQUAL "slo" AND NOT OPENRENDER_HAVE_LLVM)
            set_tests_properties(DloUv_${_du_tag} PROPERTIES DISABLED TRUE)
        endif()
    endforeach()
endforeach()
