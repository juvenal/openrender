# Test suite for libribpreview

set(PREVIEW_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/preview
    ${CMAKE_SOURCE_DIR}/src/preview/libribpreview
    ${CMAKE_BINARY_DIR}
)

set(PREVIEW_TEST_LIBS ribpreview ribRender openrendercommon)
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
add_preview_test(test_dataview_chunking   test_dataview_chunking.cpp)
add_preview_test(test_debugdump_roundtrip test_debugdump_roundtrip.cpp)
add_preview_test(test_data_world_init      test_data_world_init.cpp)
add_preview_test(test_data_pointcloud      test_data_pointcloud.cpp)
add_preview_test(test_data_keys            test_data_keys.cpp)
add_preview_test(test_wire_cli             test_wire_cli.cpp)
add_preview_test(test_data_disk_expand     test_data_disk_expand.cpp)
add_preview_test(test_preview_subdiv       test_preview_subdiv.cpp)
set_tests_properties(test_preview_subdiv PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
add_preview_test(test_preview_integration test_integration.cpp)
set_tests_properties(test_preview_integration PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ---------------------------------------------------------------------------------------------------------
# Spec 038 (Scene Interface untangle), US4: the preview back-end.
#
#   Preview_ExtractionUnchanged  characterization: what ribpreview_load extracts from each scene of the preview
#                                suite (reference/<platform>/<scene>.txt), run from the repository root.
#                                Green on the starting tree by design.
#   Preview_NoCoreReach          cmake -P: libribpreview includes no Core header and no object of it needs a
#                                CRenderer / CRendererContext symbol. Green since Phase 4; labels preview and
#                                architecture.
#   Preview_SelfRegistration     a program linking libribpreview alone gets its context from
#                                RiBeginBackEnd("preview", nullptr). Enabled in Phase 4 (it was DISABLED, with an
#                                EXCLUDE_FROM_ALL executable that did not compile while RiBeginBackEnd did not exist).
# ---------------------------------------------------------------------------------------------------------
add_executable(test_preview_extraction_unchanged test_extraction_unchanged.cpp)
target_include_directories(test_preview_extraction_unchanged PRIVATE ${PREVIEW_TEST_INCLUDES}
    ${CMAKE_SOURCE_DIR}/tests/unit/scene_interface)
target_link_libraries(test_preview_extraction_unchanged PRIVATE ${PREVIEW_TEST_LIBS})
target_compile_features(test_preview_extraction_unchanged PRIVATE cxx_std_20)
# GEOMETRIES is the tracked geometry/ directory (teapot.rib), not the gitignored deploy tree the other preview tests name
set(PREVIEW_EXTRACTION_ENV
    "SHADERS=${CMAKE_SOURCE_DIR}/openrender/shaders"
    "ORENDERHOME=${CMAKE_SOURCE_DIR}/openrender"
    "DISPLAYS=${CMAKE_SOURCE_DIR}/openrender/displays"
    "GEOMETRIES=${CMAKE_SOURCE_DIR}/geometry")
add_test(NAME Preview_ExtractionUnchanged
    COMMAND test_preview_extraction_unchanged ${CMAKE_CURRENT_SOURCE_DIR}/reference)
set_tests_properties(Preview_ExtractionUnchanged PROPERTIES
    TIMEOUT 120
    LABELS "preview;regression"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "${PREVIEW_EXTRACTION_ENV}")

add_custom_target(si_capture_preview
    COMMAND ${CMAKE_COMMAND} -E env ${PREVIEW_EXTRACTION_ENV} $<TARGET_FILE:test_preview_extraction_unchanged> ${CMAKE_CURRENT_SOURCE_DIR}/reference ${OR038_CAPTURE_FLAG}
            ${OR038_RECAPTURE_FLAG}
    DEPENDS test_preview_extraction_unchanged
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Capturing the preview extraction references (an existing reference is never overwritten)"
    VERBATIM)
add_dependencies(scene-interface-capture-baselines si_capture_preview)

add_test(NAME Preview_NoCoreReach
    COMMAND ${CMAKE_COMMAND}
            "-DINCLUDE_DIRS=$<TARGET_PROPERTY:ribpreview,INCLUDE_DIRECTORIES>"
            -DPREVIEW_DIR=${CMAKE_SOURCE_DIR}/src/preview/libribpreview
            -DOBJECT_DIR=${CMAKE_BINARY_DIR}/src/preview/libribpreview/CMakeFiles/ribpreview.dir
            -DNM=${CMAKE_NM}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DMAP=${CMAKE_SOURCE_DIR}/tests/architecture/domain-map.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_preview_no_core_reach.cmake)
set_tests_properties(Preview_NoCoreReach PROPERTIES TIMEOUT 60 LABELS "preview;architecture")

add_executable(test_preview_self_registration test_self_registration.cpp)
target_include_directories(test_preview_self_registration PRIVATE ${PREVIEW_TEST_INCLUDES}
    ${CMAKE_SOURCE_DIR}/tests/unit/scene_interface)
target_link_libraries(test_preview_self_registration PRIVATE ribpreview openrendercommon)
target_compile_features(test_preview_self_registration PRIVATE cxx_std_20)
add_test(NAME Preview_SelfRegistration
    COMMAND test_preview_self_registration ${CMAKE_CURRENT_BINARY_DIR}/self_registration_work)
set_tests_properties(Preview_SelfRegistration PROPERTIES
    TIMEOUT 60
    LABELS "preview"
    ENVIRONMENT "${PREVIEW_TEST_ENV}")

# ---------------------------------------------------------------------------------------------------------
# Spec 032 (GI cache split), US3: Preview_DataSceneUnchanged. Characterization: the data viewer's scene for the
# five document types (ribdata_* C API and the CDataView draw stream), before and after each key, against
# reference/<platform>/datascene.txt. Green on the starting tree by design; the cache split moves the data
# structures off CDataView and this test shows the viewer's output did not change. The photon map is written by an
# orender photon pass and the irradiance cache is fixture F1's file (tests/gi_split, GiFixture_ICache), so the test
# includes no photon-map header. Recorded once, on the unmodified tree, by its own --record mode.
# ---------------------------------------------------------------------------------------------------------
if(APPLE)
    set(GI_PREVIEW_PLATFORM macos)
else()
    set(GI_PREVIEW_PLATFORM linux)
endif()
set(GI_PREVIEW_DISPLAY_PATH "$<TARGET_FILE_DIR:file>:$<TARGET_FILE_DIR:rgbe>:$<TARGET_FILE_DIR:framebuffer>")
if(HAVE_OPENEXR)
    set(GI_PREVIEW_DISPLAY_PATH "${GI_PREVIEW_DISPLAY_PATH}:$<TARGET_FILE_DIR:openexr>")
endif()
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data_scene_unchanged)
add_executable(test_data_scene_unchanged test_data_scene_unchanged.cpp)
target_include_directories(test_data_scene_unchanged PRIVATE ${PREVIEW_TEST_INCLUDES})
target_link_libraries(test_data_scene_unchanged PRIVATE ${PREVIEW_TEST_LIBS})
target_compile_features(test_data_scene_unchanged PRIVATE cxx_std_20)
add_test(NAME Preview_DataSceneUnchanged
    COMMAND test_data_scene_unchanged $<TARGET_FILE:orender> ${CMAKE_SOURCE_DIR}/examples/rib/tests
            ${CMAKE_BINARY_DIR}/tests/gi_split/fixture/icache-fixture.icache
            ${CMAKE_CURRENT_BINARY_DIR}/data_scene_unchanged
            ${CMAKE_CURRENT_SOURCE_DIR}/reference/${GI_PREVIEW_PLATFORM}/datascene.txt)
set_tests_properties(Preview_DataSceneUnchanged PROPERTIES
    TIMEOUT 180
    LABELS "preview;regression"
    FIXTURES_REQUIRED GiFixture_ICache
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_BINARY_DIR}/tests/visual/_home;DISPLAYS=${GI_PREVIEW_DISPLAY_PATH};GEOMETRIES=${CMAKE_SOURCE_DIR}/geometry;TEXTURES=${CMAKE_SOURCE_DIR}/examples/rib/tests")
