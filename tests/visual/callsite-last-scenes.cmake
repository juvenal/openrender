# GitHub #268: the .rslo loader wrote the call-site operand of a stochastic instruction past the end of its argument array, onto the
# first constants, whenever that instruction was the last one in the shader that takes operands. texture(), environment(), shadow()
# and trace() with a literal option value then crashed orender (a string constant such as "blur" lost half of its pointer), and
# `Ci = random();` as the last statement did not load.
#
# Included from CMakeLists.txt after atan2-scenes.cmake. Each shader exists twice: cl_<call>_last ends with the call, cl_<call>_after
# adds one statement after it (which always worked and is the control). Under .rslo the two must render bit for bit alike.
# CallSiteLast_texture_engaged shows the literal option is honoured: blur 0.5 against blur 0 must differ.
# The probe shaders are test-only and compiled into the build tree. A twin pair defines the same surface name, cl_<call>, in two
# directories (a render depends on the shader's name), and each scene puts its own directory first on the shader search path.

set(CL_DIR ${CMAKE_CURRENT_BINARY_DIR}/callsite-last)
file(MAKE_DIRECTORY ${CL_DIR})
set(_cl_calls texture environment shadow trace)
# <variant> <source suffix>: the compiled shader is always <CL_DIR>/<variant>/cl_<call>.rslo, so a twin renders under the same name
set(_cl_outputs "")
function(cl_compile CALL SUFFIX VARIANT)
    file(MAKE_DIRECTORY ${CL_DIR}/${VARIANT})
    add_custom_command(
        OUTPUT ${CL_DIR}/${VARIANT}/cl_${CALL}.rslo
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/shaders/cl_${CALL}_${SUFFIX}.sl
                -DOUT=${CL_DIR}/${VARIANT}/cl_${CALL}.rslo -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes
                -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cl_${CALL}_${SUFFIX}.sl oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        WORKING_DIRECTORY ${CL_DIR}/${VARIANT}
        COMMENT ""
        VERBATIM)
    set(_cl_outputs ${_cl_outputs} ${CL_DIR}/${VARIANT}/cl_${CALL}.rslo PARENT_SCOPE)
endfunction()
foreach(_c ${_cl_calls})
    cl_compile(${_c} last last)
    cl_compile(${_c} after after)
endforeach()
cl_compile(texture last0 zero)
cl_compile(random last last)
add_custom_target(callsite_last_shaders ALL DEPENDS ${_cl_outputs})
add_dependencies(callsite_last_shaders openrender_shaders)

# One sphere shaded by the probe, a floor and a distant light, so trace() and shadow() have something to meet. The scene names its
# variant's directory first on the shader search path.
function(cl_write_scene CALL VARIANT)
    file(WRITE ${CL_DIR}/${VARIANT}/cl_${CALL}.rib
"##RenderMan RIB-Structure 1.0
# GitHub #268, written by callsite-last-scenes.cmake.
version 3.03
Hider \"reyes\" \"jitter\" [0]
Option \"searchpath\" \"shader\" [\"${CL_DIR}/${VARIANT}:&\"]
Option \"sampling\" \"sampler\" [\"sobol\"]
Option \"sampling\" \"seed\" [0]
Option \"shaderformat\" \"default\" [\"rslo\"]
Option \"limits\" \"numthreads\" [1]
PixelSamples 1 1
PixelFilter \"box\" 1 1
Quantize \"rgba\" 0 0 0 0
Display \"cl_${CALL}_${VARIANT}.tif\" \"file\" \"rgba\"
Projection \"perspective\"
Format 24 24 1
Clipping 0.1 100
Translate 0 0 5
WorldBegin
  LightSource \"distantlight\" 1 \"intensity\" [1] \"from\" [2 4 -2] \"to\" [0 0 0]
  Attribute \"visibility\" \"int diffuse\" [1] \"int specular\" [1] \"string transmission\" [\"opaque\"]
  AttributeBegin
    Surface \"matte\"
    Translate 0 -1.5 0
    Polygon \"P\" [-5 0 -5  5 0 -5  5 0 5  -5 0 5]
  AttributeEnd
  Surface \"cl_${CALL}\"
  Sphere 1 -1 1 360
WorldEnd
")
endfunction()

set(_cl_env "${VISUAL_ENV}")
foreach(_c ${_cl_calls})
    cl_write_scene(${_c} last)
    cl_write_scene(${_c} after)
endforeach()
cl_write_scene(texture zero)
cl_write_scene(random last)

foreach(_c ${_cl_calls})
    set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/callsite-last-${_c})
    file(MAKE_DIRECTORY ${_scratch})
    add_test(NAME CallSiteLast_${_c}_rslo
        COMMAND test_render_compare $<TARGET_FILE:orender> same
                ${CL_DIR}/last/cl_${_c}.rib cl_${_c}_last.tif ${CL_DIR}/after/cl_${_c}.rib cl_${_c}_after.tif
        WORKING_DIRECTORY ${_scratch})
    set_tests_properties(CallSiteLast_${_c}_rslo PROPERTIES
        TIMEOUT 120 LABELS "visual;regression;shading_parity;callsite_last" ENVIRONMENT "${_cl_env}" DEPENDS orender)
endforeach()

set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/callsite-last-texture-engaged)
file(MAKE_DIRECTORY ${_scratch})
add_test(NAME CallSiteLast_texture_engaged
    COMMAND test_render_compare $<TARGET_FILE:orender> differ
            ${CL_DIR}/last/cl_texture.rib cl_texture_last.tif ${CL_DIR}/zero/cl_texture.rib cl_texture_zero.tif
    WORKING_DIRECTORY ${_scratch})
set_tests_properties(CallSiteLast_texture_engaged PROPERTIES
    TIMEOUT 120 LABELS "visual;regression;shading_parity;callsite_last" ENVIRONMENT "${_cl_env}" DEPENDS orender)

# `Ci = random();` as the last statement: random() has no operand but its call-site ordinal, and the shader used to fail to load
# ("Missing call-site operand", then "Failed to find shader"). Its values are keyed per shader, so there is no twin to compare with:
# the test is that orender renders it without any error.
set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/callsite-last-random)
file(MAKE_DIRECTORY ${_scratch})
add_test(NAME CallSiteLast_random_rslo COMMAND orender ${CL_DIR}/last/cl_random.rib WORKING_DIRECTORY ${_scratch})
set_tests_properties(CallSiteLast_random_rslo PROPERTIES
    TIMEOUT 120 LABELS "visual;regression;shading_parity;callsite_last" ENVIRONMENT "${_cl_env}" DEPENDS orender
    FAIL_REGULAR_EXPRESSION "Failed to find shader|Missing call-site operand|Error in shader")
