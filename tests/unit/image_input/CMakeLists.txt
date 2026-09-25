# Unit tests for the multi-format texture source decode abstraction
# (spec 018-multi-format-texture-decode).
#
# `ctest --test-dir build -L image_input` runs every test registered here.

set(IMAGE_INPUT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/state
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/output
    ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_SOURCE_DIR}/src/ri/dataviewer
    ${CMAKE_SOURCE_DIR}/src
)
set(IMAGE_INPUT_FIXTURES_DIR ${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures)

add_executable(test_image_input_tiff test_image_input_tiff.cpp)
target_compile_features(test_image_input_tiff PRIVATE cxx_std_20)
target_include_directories(test_image_input_tiff PRIVATE ${IMAGE_INPUT_INCLUDE_DIRS})
target_link_libraries(test_image_input_tiff PRIVATE ribRender openrendercommon TIFF::TIFF)
add_test(NAME ImageInput_Tiff COMMAND test_image_input_tiff)
set_tests_properties(ImageInput_Tiff PROPERTIES TIMEOUT 30 LABELS "image_input;unit"
    ENVIRONMENT "IMAGE_INPUT_FIXTURES_DIR=${IMAGE_INPUT_FIXTURES_DIR}")

# T006: byte-identical bake regression (FR-007/SC-002) across all 4
# CLI-reachable otexmake bake modes, against checked-in references.
add_test(NAME ImageInput_TiffBakeByteIdentical
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_tiff_bake_byte_identical.sh
        $<TARGET_FILE:otexmake>
        ${IMAGE_INPUT_FIXTURES_DIR}/medium_rgb.tif
        ${IMAGE_INPUT_FIXTURES_DIR}/references)
set_tests_properties(ImageInput_TiffBakeByteIdentical PROPERTIES TIMEOUT 60 LABELS "image_input;unit")

add_executable(test_image_input_png test_image_input_png.cpp)
target_compile_features(test_image_input_png PRIVATE cxx_std_20)
target_include_directories(test_image_input_png PRIVATE ${IMAGE_INPUT_INCLUDE_DIRS})
target_link_libraries(test_image_input_png PRIVATE ribRender openrendercommon PNG::PNG)
add_test(NAME ImageInput_Png COMMAND test_image_input_png)
set_tests_properties(ImageInput_Png PROPERTIES TIMEOUT 30 LABELS "image_input;unit"
    ENVIRONMENT "IMAGE_INPUT_FIXTURES_DIR=${IMAGE_INPUT_FIXTURES_DIR}")

if(HAVE_OPENEXR)
    add_executable(test_image_input_exr test_image_input_exr.cpp)
    target_compile_features(test_image_input_exr PRIVATE cxx_std_20)
    target_include_directories(test_image_input_exr PRIVATE ${IMAGE_INPUT_INCLUDE_DIRS})
    target_compile_definitions(test_image_input_exr PRIVATE HAVE_OPENEXR)
    target_link_libraries(test_image_input_exr PRIVATE ribRender openrendercommon ${OPENRENDER_OPENEXR_LIBS})
    add_test(NAME ImageInput_Exr COMMAND test_image_input_exr)
    set_tests_properties(ImageInput_Exr PROPERTIES TIMEOUT 30 LABELS "image_input;unit"
        ENVIRONMENT "IMAGE_INPUT_FIXTURES_DIR=${IMAGE_INPUT_FIXTURES_DIR}")
endif()

add_executable(test_image_input_rgbe test_image_input_rgbe.cpp)
target_compile_features(test_image_input_rgbe PRIVATE cxx_std_20)
target_include_directories(test_image_input_rgbe PRIVATE ${IMAGE_INPUT_INCLUDE_DIRS})
target_link_libraries(test_image_input_rgbe PRIVATE ribRender openrendercommon)
add_test(NAME ImageInput_Rgbe COMMAND test_image_input_rgbe)
set_tests_properties(ImageInput_Rgbe PROPERTIES TIMEOUT 30 LABELS "image_input;unit"
    ENVIRONMENT "IMAGE_INPUT_FIXTURES_DIR=${IMAGE_INPUT_FIXTURES_DIR}")

# T031: cross-format negative-case coverage (FR-008/SC-004) -- unsupported
# extension returns nullptr, corrupted-but-right-extension files fail
# cleanly per decoder.
add_executable(test_image_input_errors test_image_input_errors.cpp)
target_compile_features(test_image_input_errors PRIVATE cxx_std_20)
target_include_directories(test_image_input_errors PRIVATE ${IMAGE_INPUT_INCLUDE_DIRS})
target_link_libraries(test_image_input_errors PRIVATE ribRender openrendercommon TIFF::TIFF PNG::PNG)
if(HAVE_OPENEXR)
    target_compile_definitions(test_image_input_errors PRIVATE HAVE_OPENEXR)
    target_link_libraries(test_image_input_errors PRIVATE ${OPENRENDER_OPENEXR_LIBS})
endif()
add_test(NAME ImageInput_Errors COMMAND test_image_input_errors)
set_tests_properties(ImageInput_Errors PROPERTIES TIMEOUT 30 LABELS "image_input;unit"
    ENVIRONMENT "IMAGE_INPUT_FIXTURES_DIR=${IMAGE_INPUT_FIXTURES_DIR}")

