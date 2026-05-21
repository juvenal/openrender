# Test suite for libribpreview

set(PREVIEW_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/preview
    ${CMAKE_SOURCE_DIR}/src/preview/libribpreview
    ${CMAKE_BINARY_DIR}
)

set(PREVIEW_TEST_LIBS ribpreview ri openrendercommon)
set(PREVIEW_TEST_ENV
    "SHADERS=${CMAKE_SOURCE_DIR}/openrender/shaders"
    "ORENDERHOME=${CMAKE_SOURCE_DIR}/openrender"
    "DISPLAYS=${CMAKE_SOURCE_DIR}/openrender/displays"
    "GEOMETRIES=${CMAKE_SOURCE_DIR}/openrender/geometry"
)

macro(add_preview_test name src)
    add_executable(${name} ${src})
    target_include_directories(${name} PRIVATE ${PREVIEW_TEST_INCLUDES})
    target_link_libraries(${name} PRIVATE ${PREVIEW_TEST_LIBS})
    target_compile_features(${name} PRIVATE cxx_std_20)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        TIMEOUT 30
        LABELS "preview"
        ENVIRONMENT "${PREVIEW_TEST_ENV}"
    )
    message(STATUS "  - ${name}")
endmacro()

add_preview_test(test_preview_polygon    test_polygon.cpp)
add_preview_test(test_preview_patch      test_patch.cpp)
add_preview_test(test_preview_quadric    test_quadric.cpp)
add_preview_test(test_preview_nurbs      test_nurbs.cpp)
add_preview_test(test_preview_camera     test_camera.cpp)
add_preview_test(test_preview_arcball    test_arcball.cpp)
add_preview_test(test_camera_export      test_camera_export.cpp)
add_preview_test(test_camera_replace     test_camera_replace.cpp)
add_preview_test(test_preview_integration test_integration.cpp)
set_tests_properties(test_preview_integration PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
