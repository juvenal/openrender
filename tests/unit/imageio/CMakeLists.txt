# Unit tests for the optional OpenImageIO backend (spec 048-oiio-image-io).
#
# `ctest --test-dir build -L imageio` runs every test registered here. The label `imageio` is on every test.
#
# Two kinds of build (FR-003): with OIIO (HAVE_OIIO, set by the root CMakeLists.txt after the version check) the
# OIIO tests are registered; without it the "absent" tests are registered instead, and the default build is what
# every other platform and the CI jobs build.

set(IMAGEIO_FIXTURES_DIR ${CMAKE_SOURCE_DIR}/tests/unit/imageio/fixtures)

# --- registered in every build -----------------------------------------------------------------------------------

# P-Q11: CI never enables OIIO in spec 048, so every CI job is a "without OIIO" job.
add_test(NAME ImageIO_CiHasNoOiio
    COMMAND ${CMAKE_COMMAND}
        -DWORKFLOW=${CMAKE_SOURCE_DIR}/.github/workflows/release.yml
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ci_no_oiio.cmake)
set_tests_properties(ImageIO_CiHasNoOiio PROPERTIES TIMEOUT 30 LABELS "imageio;unit")

# FR-002: the manifest gains only the `oiio` feature; the shared dependency list and the baseline do not move.
add_test(NAME ImageIO_VcpkgManifest
    COMMAND ${CMAKE_COMMAND}
        -DMANIFEST=${CMAKE_SOURCE_DIR}/vcpkg.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_vcpkg_manifest.cmake)
set_tests_properties(ImageIO_VcpkgManifest PROPERTIES TIMEOUT 30 LABELS "imageio;unit")

# The environment orender needs, as the spec 019/020 tests set it (a hermetic .orenderrc home, the build's compiled shaders and the
# display drivers). Used by the selection tests, which render and bake.
set(IMAGEIO_DISPLAY_PATH "$<TARGET_FILE_DIR:file>:$<TARGET_FILE_DIR:rgbe>:$<TARGET_FILE_DIR:framebuffer>")
if(HAVE_OPENEXR)
    set(IMAGEIO_DISPLAY_PATH "${IMAGEIO_DISPLAY_PATH}:$<TARGET_FILE_DIR:openexr>")
endif()
set(IMAGEIO_RENDER_ENV
    "SHADERS=${CMAKE_BINARY_DIR}/shaders"
    "ORENDERHOME=${CMAKE_BINARY_DIR}/tests/visual/_home"
    "DISPLAYS=${IMAGEIO_DISPLAY_PATH}"
    "IMAGEIO_FIXTURES_DIR=${IMAGEIO_FIXTURES_DIR}")
set(IMAGEIO_SELECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_SOURCE_DIR}/src/ri/state
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_SOURCE_DIR}/src)

# Phase 5 (selection): a program that bakes through the RI calls and reads the result with libtiff.
function(imageio_select_program target source)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE ${IMAGEIO_SELECT_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE ribRender openrendercommon TIFF::TIFF)
endfunction()

# Phase 6 tools (always built: T093 records the legacy bake hashes on the OIIO-off tree). Test-only, never installed.
add_executable(gen_bake_fixtures tools/gen_bake_fixtures.cpp)
target_compile_features(gen_bake_fixtures PRIVATE cxx_std_20)
target_link_libraries(gen_bake_fixtures PRIVATE TIFF::TIFF)
add_executable(legacy_bake_hashes tools/legacy_bake_hashes.cpp)
target_compile_features(legacy_bake_hashes PRIVATE cxx_std_20)
target_include_directories(legacy_bake_hashes PRIVATE ${IMAGEIO_SELECT_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(legacy_bake_hashes PRIVATE ribRender openrendercommon TIFF::TIFF)

# --- build with OIIO ----------------------------------------------------------------------------------------------
if(HAVE_OIIO)
    # US5: src/imageio/ includes only the standard library, its own published header, common/ and OIIO's headers.
    add_test(NAME ImageIO_NoDomainInclude
        COMMAND ${CMAKE_COMMAND}
            -DIMAGEIO_DIR=${CMAKE_SOURCE_DIR}/src/imageio
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_domain_include.cmake)
    set_tests_properties(ImageIO_NoDomainInclude PROPERTIES TIMEOUT 30 LABELS "imageio;unit")

    # --- Phase 4: decode (US1) -----------------------------------------------------------------------------------
    # The tests touch OpenImageIO only through the Texturing interface (createImageInput(path, backend)): they link
    # ribRender and libRIBCommon, and have no OIIO include path or library.
    set(IMAGEIO_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/src/ri
        ${CMAKE_SOURCE_DIR}/src/ri/state
        ${CMAKE_SOURCE_DIR}/src/ri/parse
        ${CMAKE_SOURCE_DIR}/src/ri/texture
        ${CMAKE_SOURCE_DIR}/src/ri/core
        ${CMAKE_SOURCE_DIR}/src)
    set(IMAGEIO_TEST_ENV
        "IMAGEIO_FIXTURES_DIR=${IMAGEIO_FIXTURES_DIR}"
        "IMAGE_INPUT_FIXTURES_DIR=${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures")

    # The fixture generator (T051): a test-only tool, run by hand through cpujob; its output is checked in.
    add_executable(gen_fixtures tools/gen_fixtures.cpp)
    target_compile_features(gen_fixtures PRIVATE cxx_std_20)
    target_link_libraries(gen_fixtures PRIVATE ${openrendertiff_libs})
    if(HAVE_OPENEXR)
        target_compile_definitions(gen_fixtures PRIVATE HAVE_OPENEXR)
        target_link_libraries(gen_fixtures PRIVATE ${OPENRENDER_OPENEXR_LIBS})
    endif()

    function(imageio_decode_test target test source)
        add_executable(${target} ${source})
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_include_directories(${target} PRIVATE ${IMAGEIO_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(${target} PRIVATE ribRender openrendercommon)
        if(HAVE_OPENEXR)
            target_compile_definitions(${target} PRIVATE HAVE_OPENEXR)
        endif()
        add_test(NAME ${test} COMMAND ${target} ${ARGN})
        set_tests_properties(${test} PROPERTIES TIMEOUT 60 LABELS "imageio;unit" ENVIRONMENT "${IMAGEIO_TEST_ENV}")
    endfunction()

    imageio_decode_test(test_decode_parity ImageIO_DecodeParity test_decode_parity.cpp)
    imageio_decode_test(test_decode_errors ImageIO_DecodeErrors test_decode_errors.cpp)
    imageio_decode_test(test_decode_differences ImageIO_DecodeDifferences test_decode_differences.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/differences.txt)
    imageio_decode_test(test_decode_concurrent ImageIO_DecodeConcurrent test_decode_concurrent.cpp)

    # P-Q10: no colour conversion on read. The program prints digests; the script runs it under two colour
    # configurations (OCIO unset, and OCIO=fixtures/colour/other.ocio) and compares.
    add_executable(test_no_colour_conversion_read test_no_colour_conversion_read.cpp)
    target_compile_features(test_no_colour_conversion_read PRIVATE cxx_std_20)
    target_include_directories(test_no_colour_conversion_read PRIVATE ${IMAGEIO_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_no_colour_conversion_read PRIVATE ribRender openrendercommon)
    set(IMAGEIO_COLOUR_FIXTURES
        ${IMAGEIO_FIXTURES_DIR}/colour/cicp_rgb8.png
        ${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures/large_rgb8.png
        ${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures/large_rgb.tif
        ${IMAGEIO_FIXTURES_DIR}/parity/tiff_u16_3.tif)
    if(HAVE_OPENEXR)
        list(APPEND IMAGEIO_COLOUR_FIXTURES ${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures/large_rgb.exr)
    endif()
    add_test(NAME ImageIO_NoColourConversion
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:test_no_colour_conversion_read>
            -DOTHER_OCIO=${IMAGEIO_FIXTURES_DIR}/colour/other.ocio
            "-DFIXTURES=${IMAGEIO_COLOUR_FIXTURES}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_two_ocio_envs.cmake)
    set_tests_properties(ImageIO_NoColourConversion PROPERTIES TIMEOUT 120 LABELS "imageio;unit")

    # --- Phase 6: baking through OIIO (US3). The writers are compared by reading both files with libtiff ---
    function(imageio_bake_program target source)
        imageio_select_program(${target} ${source})
    endfunction()
    imageio_bake_program(test_bake_parity test_bake_parity.cpp)
    add_test(NAME ImageIO_BakeParity COMMAND test_bake_parity)
    set_tests_properties(ImageIO_BakeParity PROPERTIES TIMEOUT 120 LABELS "imageio;unit" ENVIRONMENT "${IMAGEIO_RENDER_ENV};IMAGE_INPUT_FIXTURES_DIR=${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures")
    imageio_bake_program(test_bake_exclusions test_bake_exclusions.cpp)
    add_test(NAME ImageIO_BakeExclusions COMMAND test_bake_exclusions)
    set_tests_properties(ImageIO_BakeExclusions PROPERTIES TIMEOUT 120 LABELS "imageio;unit" ENVIRONMENT "${IMAGEIO_RENDER_ENV};IMAGE_INPUT_FIXTURES_DIR=${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures")
    # US3-3: the scenes render alike with a legacy-baked and an OIIO-baked texture (visual metric, threshold 20)
    add_test(NAME ImageIO_BakeRender
        COMMAND ${CMAKE_COMMAND}
            -DOTEXMAKE=$<TARGET_FILE:otexmake>
            -DORENDER=$<TARGET_FILE:orender>
            -DTEST_VISUAL_RENDER=$<TARGET_FILE:test_visual_render>
            -DSCENES=${CMAKE_CURRENT_SOURCE_DIR}/scenes
            -DBAKE=${IMAGEIO_FIXTURES_DIR}/bake
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/bake-render
            -DTHRESHOLD=20
            -P ${CMAKE_CURRENT_SOURCE_DIR}/test_bake_render.cmake)
    set_tests_properties(ImageIO_BakeRender PROPERTIES TIMEOUT 300 LABELS "imageio;unit"
        ENVIRONMENT "${IMAGEIO_RENDER_ENV}" DEPENDS "orender;otexmake;test_visual_render")

    # FR-012: the difference list's data file and its rendering in DEVNOTES_DETAILS/IMAGE_IO.md name the same ids.
    add_test(NAME ImageIO_DifferenceListDoc
        COMMAND ${CMAKE_COMMAND}
            -DDATA=${CMAKE_CURRENT_SOURCE_DIR}/differences.txt
            -DDOC=${CMAKE_SOURCE_DIR}/DEVNOTES_DETAILS/IMAGE_IO.md
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_difference_list_doc.cmake)
    set_tests_properties(ImageIO_DifferenceListDoc PROPERTIES TIMEOUT 30 LABELS "imageio;unit")

    # --- Phase 5: selection (US2). Nothing changes unless OIIO is asked for (FR-013, FR-014) ---
    imageio_select_program(test_selection test_selection.cpp)
    add_test(NAME ImageIO_Selection
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_selection_cli.sh
            $<TARGET_FILE:test_selection>
            $<TARGET_FILE:otexmake>
            $<TARGET_FILE:orender>
            ${IMAGEIO_FIXTURES_DIR}
            ${CMAKE_SOURCE_DIR}/tests/unit/texture_tile/cache-scene.rib.in
            ${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures/large_rgb8.png)
    set_tests_properties(ImageIO_Selection PROPERTIES TIMEOUT 120 LABELS "imageio;unit"
        ENVIRONMENT "${IMAGEIO_RENDER_ENV}" DEPENDS "orender;otexmake")

    # --- Phase 7: the oiio display driver (US4). The modules are named by environment variables. ---
    set(IMAGEIO_OIIO_DSPLY $<TARGET_FILE:oiio>)
    set(IMAGEIO_DISPLAY_MODULES "FILE_DSPLY=$<TARGET_FILE:file>" "RGBE_DSPLY=$<TARGET_FILE:rgbe>" "OIIO_DSPLY=${IMAGEIO_OIIO_DSPLY}")
    if(HAVE_OPENEXR)
        list(APPEND IMAGEIO_DISPLAY_MODULES "OPENEXR_DSPLY=$<TARGET_FILE:openexr>")
    endif()
    # orender finds the drivers through DISPLAYS: the legacy ones and the oiio directory (ImageIO_DisplayParity renders with both)
    set(IMAGEIO_DISPLAY_RENDER_ENV
        "SHADERS=${CMAKE_BINARY_DIR}/shaders"
        "ORENDERHOME=${CMAKE_BINARY_DIR}/tests/visual/_home"
        "DISPLAYS=${IMAGEIO_DISPLAY_PATH}:$<TARGET_FILE_DIR:oiio>"
        ${IMAGEIO_DISPLAY_MODULES})

    find_package(Threads REQUIRED)
    function(imageio_display_program target source)
        add_executable(${target} ${source})
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_include_directories(${target} PRIVATE ${IMAGEIO_SELECT_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/src/display ${CMAKE_SOURCE_DIR}/src/imageio ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_BINARY_DIR})
        # the decoders of the fixed readers: libtiff and libpng (and OpenEXR); the OIIO Reader only for RGBE
        # the tests run the module: it is built with them
        add_dependencies(${target} oiio file rgbe)
        target_link_libraries(${target} PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon openrenderimageio
            Threads::Threads ${openrendertiff_libs})
        if(HAVE_OPENEXR)
            target_compile_definitions(${target} PRIVATE HAVE_OPENEXR)
            target_link_libraries(${target} PRIVATE ${OPENRENDER_OPENEXR_LIBS})
        endif()
    endfunction()

    # T103: SC-003 and FR-019. The synthetic cases through CDisplayHost, then one rendered scene with two displays.
    imageio_display_program(test_display_parity test_display_parity.cpp)
    set(IMAGEIO_PARITY_OPENEXR "")
    if(HAVE_OPENEXR)
        set(IMAGEIO_PARITY_OPENEXR -DHAVE_OPENEXR=1)
    endif()
    add_test(NAME ImageIO_DisplayParity
        COMMAND ${CMAKE_COMMAND}
            -DPARITY=$<TARGET_FILE:test_display_parity>
            -DORENDER=$<TARGET_FILE:orender>
            -DSCENE=${CMAKE_CURRENT_SOURCE_DIR}/display-parity.rib.in
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/display_parity
            ${IMAGEIO_PARITY_OPENEXR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_display_parity.cmake)
    set_tests_properties(ImageIO_DisplayParity PROPERTIES TIMEOUT 300 LABELS "imageio;unit" ENVIRONMENT "${IMAGEIO_DISPLAY_RENDER_ENV}")

    # T104: the module loads through CDisplayHost at display ABI 2.0.0 and keeps its include boundary
    imageio_display_program(test_display_load test_display_load.cpp)
    add_test(NAME ImageIO_DisplayLoad COMMAND test_display_load)
    set_tests_properties(ImageIO_DisplayLoad PROPERTIES TIMEOUT 120 LABELS "imageio;unit" ENVIRONMENT "${IMAGEIO_DISPLAY_MODULES}")
    add_test(NAME ImageIO_DisplayLoadIncludes
        COMMAND ${CMAKE_COMMAND} -DSOURCE=${CMAKE_SOURCE_DIR}/src/display/oiio/oiio.cpp -P ${CMAKE_CURRENT_SOURCE_DIR}/check_display_includes.cmake)
    set_tests_properties(ImageIO_DisplayLoadIncludes PROPERTIES TIMEOUT 30 LABELS "imageio;unit")

    # T105: the format of the file, and what the driver refuses. The no-OpenEXR case is registered only in a build without OpenEXR.
    imageio_display_program(test_display_format_errors test_display_format_errors.cpp)
    add_test(NAME ImageIO_DisplayFormatErrors COMMAND test_display_format_errors)
    set_tests_properties(ImageIO_DisplayFormatErrors PROPERTIES TIMEOUT 120 LABELS "imageio;unit" ENVIRONMENT "${IMAGEIO_DISPLAY_MODULES}")
    if(NOT HAVE_OPENEXR)
        add_test(NAME ImageIO_DisplayFormatErrorsNoExr COMMAND test_display_format_errors noexr)
        set_tests_properties(ImageIO_DisplayFormatErrorsNoExr PROPERTIES TIMEOUT 120 LABELS "imageio;unit" ENVIRONMENT "${IMAGEIO_DISPLAY_MODULES}")
    endif()

    # T106: P-Q10 on write. The T059 wrapper, unchanged: the program prints "<case> legacy=<digest> oiio=<digest>" per case.
    imageio_display_program(test_no_colour_conversion_write test_no_colour_conversion_write.cpp)
    add_test(NAME ImageIO_NoColourConversionWrite
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:test_no_colour_conversion_write>
            -DOTHER_OCIO=${IMAGEIO_FIXTURES_DIR}/colour/other.ocio
            "-DFIXTURES=png8;png8a;png16;png16a"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_two_ocio_envs.cmake)
    set_tests_properties(ImageIO_NoColourConversionWrite PROPERTIES TIMEOUT 120 LABELS "imageio;unit" ENVIRONMENT "${IMAGEIO_DISPLAY_MODULES}")
else()
    # FR-003: a build without OIIO carries no OIIO code and no OIIO driver.
    add_test(NAME ImageIO_AbsentBuild
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DNM=${CMAKE_NM}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_absent_build.cmake)
    set_tests_properties(ImageIO_AbsentBuild PROPERTIES TIMEOUT 600 LABELS "imageio;unit")

    # FR-003, P-Q6: asking for the OIIO backend in a build without it degrades as documented.
    imageio_select_program(test_select_without_oiio test_select_without_oiio.cpp)
    add_test(NAME ImageIO_SelectWithoutOIIO
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_select_without_oiio_cli.sh
            $<TARGET_FILE:test_select_without_oiio>
            $<TARGET_FILE:otexmake>
            $<TARGET_FILE:orender>
            ${IMAGEIO_FIXTURES_DIR})
    set_tests_properties(ImageIO_SelectWithoutOIIO PROPERTIES TIMEOUT 120 LABELS "imageio;unit"
        ENVIRONMENT "${IMAGEIO_RENDER_ENV}" DEPENDS "orender;otexmake")
endif()
