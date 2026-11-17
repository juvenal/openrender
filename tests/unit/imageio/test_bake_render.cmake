# ImageIO_BakeRender (spec 048, T090, US3-3): a scene rendered with a texture baked by the legacy writer and with the same texture baked
# through OIIO must look the same. One scene per kind (plain texture, cylinder environment, cubic environment, shadow map; the
# spherical environment is not reachable from RIB). Each source is baked twice with otexmake (without and with `-imageio oiio`), the
# scene is rendered with each, and the two images are compared with the visual block-average metric (test_visual_render) at the default
# parity threshold of 20, the lowest of the 20 to 40 range (approved 2026-11-14: these are new scenes, so they have no threshold of their
# own). Whether the two images are also byte-identical is REPORTED, not required: equal samples and equal tags are expected to make them so.
#
#   cmake -DOTEXMAKE=... -DORENDER=... -DTEST_VISUAL_RENDER=... -DSCENES=<dir of *.rib.in> -DBAKE=<fixtures/bake> -DWORK=<dir>
#         [-DTHRESHOLD=20] -P test_bake_render.cmake
# The environment of orender (SHADERS, ORENDERHOME, DISPLAYS) comes from the test's ENVIRONMENT property.
foreach(var OTEXMAKE ORENDER TEST_VISUAL_RENDER SCENES BAKE WORK)
    if(NOT ${var})
        message(FATAL_ERROR "ImageIO_BakeRender: pass -D${var}=...")
    endif()
endforeach()
if(NOT THRESHOLD)
    set(THRESHOLD 20)
endif()
file(MAKE_DIRECTORY "${WORK}")
# failures are kept in a global property: they are raised inside functions
set_property(GLOBAL PROPERTY BAKE_RENDER_FAILURES "")
function(fail_with what)
    message(STATUS "FAIL: ${what}")
    set_property(GLOBAL APPEND PROPERTY BAKE_RENDER_FAILURES "${what}")
endfunction()

function(run what)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        fail_with("${what} exited ${rc}\n${out}${err}")
    endif()
endfunction()

# scene name, otexmake flags (a ;-list), source files (a ;-list)
function(one_scene name flags sources)
    set(dir "${WORK}/${name}")
    file(MAKE_DIRECTORY "${dir}/legacy" "${dir}/oiio")
    # bake both textures
    run("${name}: the legacy bake" "${OTEXMAKE}" ${flags} ${sources} "${dir}/legacy.tex")
    run("${name}: the OIIO bake" "${OTEXMAKE}" -imageio oiio ${flags} ${sources} "${dir}/oiio.tex")
    if(NOT EXISTS "${dir}/legacy.tex" OR NOT EXISTS "${dir}/oiio.tex")
        fail_with("${name}: a baked texture is missing")
        return()
    endif()
    # the OIIO bake was really written by OIIO (its SOFTWARE tag says so) and the legacy bake was not: without this a bake that fell
    # back to the legacy writer would pass the comparison below for the wrong reason
    file(READ "${dir}/oiio.tex" oiio_hex HEX)
    file(READ "${dir}/legacy.tex" legacy_hex HEX)
    string(HEX "OpenImageIO" marker)
    string(FIND "${oiio_hex}" "${marker}" in_oiio)
    string(FIND "${legacy_hex}" "${marker}" in_legacy)
    if(in_oiio EQUAL -1)
        fail_with("${name}: the OIIO bake carries no OpenImageIO SOFTWARE tag (it was not written by OIIO)")
    endif()
    if(NOT in_legacy EQUAL -1)
        fail_with("${name}: the legacy bake carries an OpenImageIO tag")
    endif()
    # the scene with each texture
    foreach(which legacy oiio)
        file(READ "${SCENES}/bake-${name}.rib.in" scene)
        string(REPLACE "@TEXTURE@" "${dir}/${which}.tex" scene "${scene}")
        file(WRITE "${dir}/${which}/bake-${name}.rib" "${scene}")
    endforeach()
    # the legacy render is the reference
    execute_process(COMMAND "${ORENDER}" bake-${name}.rib WORKING_DIRECTORY "${dir}/legacy" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0 OR NOT EXISTS "${dir}/legacy/bake-${name}.tif")
        fail_with("${name}: the legacy-textured render failed (${rc})\n${out}${err}")
        return()
    endif()
    # the OIIO render, compared with the reference by the visual metric
    execute_process(COMMAND "${TEST_VISUAL_RENDER}" "${ORENDER}" "${dir}/oiio/bake-${name}.rib" bake-${name}.tif "${dir}/legacy/bake-${name}.tif" ${THRESHOLD}
                    WORKING_DIRECTORY "${dir}/oiio" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL 0)
        fail_with("${name}: the OIIO-baked render differs from the legacy-baked one beyond the threshold ${THRESHOLD}\n${out}${err}")
        return()
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${dir}/legacy/bake-${name}.tif" "${dir}/oiio/bake-${name}.tif" RESULT_VARIABLE same)
    if(same EQUAL 0)
        message(STATUS "ok:   ${name}: the images are byte-identical")
    else()
        message(STATUS "ok:   ${name}: within the threshold ${THRESHOLD} but NOT byte-identical (reported)")
    endif()
endfunction()

one_scene(plain "" "${BAKE}/tex40_u8_3.tif")
one_scene(cylinder "-envlatl" "${BAKE}/env_u8_3.tif")
one_scene(cubic "-envcube" "${BAKE}/env_u8_3.tif;${BAKE}/env_u8_3.tif;${BAKE}/env_u8_3.tif;${BAKE}/env_u8_3.tif;${BAKE}/env_u8_3.tif;${BAKE}/env_u8_3.tif")
one_scene(shadow "-shadow" "${BAKE}/shd_f32_1.tif")

get_property(all_failures GLOBAL PROPERTY BAKE_RENDER_FAILURES)
list(LENGTH all_failures n_failures)
if(n_failures GREATER 0)
    message(FATAL_ERROR "ImageIO_BakeRender: ${n_failures} check(s) failed")
endif()
