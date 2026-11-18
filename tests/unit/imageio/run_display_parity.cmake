# ImageIO_DisplayParity, the driver of the test (spec 048, T103, SC-003): the synthetic cases through CDisplayHost, then the rendered scene.
#
#   cmake -DPARITY=<test_display_parity> -DORENDER=<orender> -DSCENE=<display-parity.rib.in> -DWORK=<dir> -P run_display_parity.cmake
# The environment (FILE_DSPLY, RGBE_DSPLY, OPENEXR_DSPLY, OIIO_DSPLY for the program; SHADERS, ORENDERHOME, DISPLAYS for orender) comes from
# the test's ENVIRONMENT property.
#
# For each render case the scene is rendered ONCE with two displays: `file` (or `openexr`, `rgbe`) and `oiio`. Both files are decoded by the fixed
# decoder of the format (libtiff, libpng, OpenEXR in test code; the OIIO Reader for RGBE) and every sample must be equal.
# (policy CMP0053: with the old rules "@QUANTIZE@" is evaluated as the variable of that name, so the template is not replaced; a -P script has no minimum version)
cmake_minimum_required(VERSION 3.19)
foreach(var PARITY ORENDER SCENE WORK)
    if(NOT ${var})
        message(FATAL_ERROR "ImageIO_DisplayParity: pass -D${var}=...")
    endif()
endforeach()
file(MAKE_DIRECTORY "${WORK}")
set(failures 0)

execute_process(COMMAND "${PARITY}" synthetic RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message(STATUS "synthetic: exit ${rc}\n${out}${err}")
if(NOT rc EQUAL 0)
    message(STATUS "FAIL: the synthetic cases (exit ${rc})")
    math(EXPR failures "${failures}+1")
endif()

# case name, format, extension, legacy display type, Display mode, Quantize statement, extra display parameters
function(render_case name format ext legacy_type mode quantize params)
    set(dir "${WORK}/${name}")
    file(MAKE_DIRECTORY "${dir}")
    set(QUANTIZE "${quantize}")
    set(DISPLAYS "Display \"${name}-legacy.${ext}\" \"${legacy_type}\" \"${mode}\" ${params}\nDisplay \"+${name}-oiio.${ext}\" \"oiio\" \"${mode}\" ${params}")
    file(READ "${SCENE}" scene)
    string(REPLACE "@QUANTIZE@" "${QUANTIZE}" scene "${scene}")
    string(REPLACE "@DISPLAYS@" "${DISPLAYS}" scene "${scene}")
    file(WRITE "${dir}/${name}.rib" "${scene}")
    execute_process(COMMAND "${ORENDER}" "${name}.rib" WORKING_DIRECTORY "${dir}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        message(STATUS "FAIL: ${name}: orender exited ${rc}\n${out}${err}")
        set(failures "${failures}+1" PARENT_SCOPE)
    endif()
    execute_process(COMMAND "${PARITY}" compare "${format}" "${dir}/${name}-legacy.${ext}" "${dir}/${name}-oiio.${ext}"
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(rc EQUAL 0)
        message(STATUS "ok:   ${name}: ${out}")
    else()
        message(STATUS "FAIL: ${name}: the two files differ or cannot be read\n${out}${err}")
        set_property(GLOBAL APPEND PROPERTY PARITY_RENDER_FAILURES "${name}")
    endif()
endfunction()

set_property(GLOBAL PROPERTY PARITY_RENDER_FAILURES "")
render_case(tiff8        tiff tif  file    rgba "Quantize \"rgba\" 255 0 255 0" "")
render_case(tiff16       tiff tif  file    rgba "Quantize \"rgba\" 65535 0 65535 0" "")
render_case(tiff_float   tiff tif  file    rgba "Quantize \"rgba\" 0 0 0 0" "")
render_case(tiff8_dither tiff tif  file    rgba "Quantize \"rgba\" 255 0 255 0.5" "")
render_case(png8         png  png  file    rgba "Quantize \"rgba\" 255 0 255 0" "")
render_case(png16        png  png  file    rgba "Quantize \"rgba\" 65535 0 65535 0" "")
if(HAVE_OPENEXR)
    render_case(exr_half exr  exr  openexr rgba "Quantize \"rgba\" 0 0 0 0" "")
endif()
render_case(rgbe_rgb     rgbe hdr  rgbe    rgb  "Quantize \"rgba\" 0 0 0 0" "")

get_property(render_failures GLOBAL PROPERTY PARITY_RENDER_FAILURES)
list(LENGTH render_failures n_render_failures)
math(EXPR failures "${failures} + ${n_render_failures}")
if(failures GREATER 0)
    message(FATAL_ERROR "ImageIO_DisplayParity: ${failures} failure(s): ${render_failures}")
endif()
