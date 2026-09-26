# Unit tests for the runtime texture tile-fetch abstraction
# (spec 019-tilesource-extraction).
#
# `ctest --test-dir build -L texture_tile` runs every test registered here.

set(TEXTURE_TILE_INCLUDE_DIRS
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
set(TEXTURE_TILE_FIXTURES_DIR ${CMAKE_SOURCE_DIR}/tests/unit/texture_tile/fixtures)

# ---------------------------------------------------------------------------
# Byte-identical rendered-output regression bar (T009/T010, FR-002/SC-001)
# ---------------------------------------------------------------------------
# Reuses tests/visual/CMakeLists.txt's exact hermetic-environment convention
# (VISUAL_RC_HOME's .orenderrc supplies Format/PixelSamples defaults most of
# these scenes don't set themselves) rather than reinventing it -- that
# directory is already configured by add_subdirectory(visual), which runs
# before add_subdirectory(unit/texture_tile) in tests/CMakeLists.txt.
set(TEXTURE_TILE_RC_HOME ${CMAKE_BINARY_DIR}/tests/visual/_home)
set(TEXTURE_TILE_DISPLAY_PATH "$<TARGET_FILE_DIR:file>:$<TARGET_FILE_DIR:rgbe>:$<TARGET_FILE_DIR:framebuffer>")
if(HAVE_OPENEXR)
    set(TEXTURE_TILE_DISPLAY_PATH "${TEXTURE_TILE_DISPLAY_PATH}:$<TARGET_FILE_DIR:openexr>")
endif()
set(TEXTURE_TILE_ENV
    "SHADERS=${CMAKE_BINARY_DIR}/shaders"
    "ORENDERHOME=${TEXTURE_TILE_RC_HOME}"
    "DISPLAYS=${TEXTURE_TILE_DISPLAY_PATH}"
    "TEXTURES=${CMAKE_SOURCE_DIR}/examples/rib/tests:${TEXTURE_TILE_FIXTURES_DIR}"
)

if(HAVE_OPENEXR)
    set(TEXTURE_TILE_HAVE_OPENEXR_FLAG 1)
else()
    set(TEXTURE_TILE_HAVE_OPENEXR_FLAG 0)
endif()

add_test(NAME TextureTile_RenderByteIdentical
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_render_byte_identical.sh
        $<TARGET_FILE:orender>
        ${CMAKE_SOURCE_DIR}/examples/rib/tests/parity
        ${TEXTURE_TILE_FIXTURES_DIR}/references
        ${TEXTURE_TILE_HAVE_OPENEXR_FLAG})
set_tests_properties(TextureTile_RenderByteIdentical PROPERTIES
    TIMEOUT 120
    LABELS "texture_tile;unit"
    ENVIRONMENT "${TEXTURE_TILE_ENV}"
    DEPENDS orender)

# ---------------------------------------------------------------------------
# Multi-threaded tile-fetch concurrency test (T012/T013/T023, FR-006/SC-002)
# ---------------------------------------------------------------------------
# See test_tile_source_concurrency.sh's header comment for the full design
# rationale (driven through a real multi-threaded render rather than a
# hand-constructed CShadingContext, and how the 15-point threshold was
# measured, not guessed).
add_test(NAME TileSource_Concurrency
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_tile_source_concurrency.sh
        $<TARGET_FILE:orender>
        $<TARGET_FILE:test_visual_render>
        ${CMAKE_CURRENT_SOURCE_DIR}/concurrency-scene.rib
        ${TEXTURE_TILE_FIXTURES_DIR}/references/concurrency-scene.tif
        5)
set_tests_properties(TileSource_Concurrency PROPERTIES
    TIMEOUT 300
    LABELS "texture_tile;unit"
    ENVIRONMENT "${TEXTURE_TILE_ENV}"
    DEPENDS "orender;test_visual_render")

# ---------------------------------------------------------------------------
# CTiffTileSource::info() direct unit test (T020)
# ---------------------------------------------------------------------------
add_executable(test_tile_source_tiff_info test_tile_source_tiff_info.cpp)
target_compile_features(test_tile_source_tiff_info PRIVATE cxx_std_20)
target_include_directories(test_tile_source_tiff_info PRIVATE ${TEXTURE_TILE_INCLUDE_DIRS})
target_link_libraries(test_tile_source_tiff_info PRIVATE ribRender openrendercommon TIFF::TIFF)
add_test(NAME TileSource_TiffInfo COMMAND test_tile_source_tiff_info)
set_tests_properties(TileSource_TiffInfo PROPERTIES TIMEOUT 30 LABELS "texture_tile;unit"
    ENVIRONMENT "TEXTURE_TILE_FIXTURES_DIR=${TEXTURE_TILE_FIXTURES_DIR}")
