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
    # tests/unit/image_input/fixtures is also searched -- spec 020's
    # disk-cache/concurrency scenes (T019-T021, T024-T027) reference
    # spec 018's fixtures directly; a fresh /speckit.analyze pass (spec
    # 020, findings F1/F2) found this directory was never searched here
    # either, which would have made every one of those scenes fail on a
    # search-path miss, not a real feature regression.
    "TEXTURES=${CMAKE_SOURCE_DIR}/examples/rib/tests:${TEXTURE_TILE_FIXTURES_DIR}:${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures"
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
# Multi-threaded tile-fetch concurrency test targeting CSynthesizedTileSource
# specifically (spec 020-runtime-tile-paging, T024-T027, FR-011/SC-005)
# ---------------------------------------------------------------------------
# See test_tile_source_synthesized_concurrency.sh's header comment for the
# full design rationale -- same real-multi-threaded-render approach as
# TileSource_Concurrency above, but targeting the new synthesized backend
# (an unbaked source, no disk cache) rather than the baked-TIFF path that
# test already covers.
add_test(NAME TileSource_SynthesizedConcurrency
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_tile_source_synthesized_concurrency.sh
        $<TARGET_FILE:orender>
        $<TARGET_FILE:test_visual_render>
        ${CMAKE_CURRENT_SOURCE_DIR}/synthesized-concurrency-scene.rib
        ${TEXTURE_TILE_FIXTURES_DIR}/references/synthesized-concurrency-scene.tif
        5)
set_tests_properties(TileSource_SynthesizedConcurrency PROPERTIES
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

# ---------------------------------------------------------------------------
# CSynthesizedTileSource::info()/fetchTile() direct unit test (spec
# 020-runtime-tile-paging, T012)
# ---------------------------------------------------------------------------
# Reads large_rgb8.png (spec 018's fixture, tests/unit/image_input/fixtures/)
# via its own IMAGE_INPUT_FIXTURES_DIR env var, matching spec 018's own
# test_image_input_*.cpp convention -- this is a standalone binary, not a
# RIB render, so it never goes through the renderer's TEXTURES search path
# (analysis finding F3). Needs PNG::PNG (not just TIFF::TIFF) since
# createSynthesizedTileSource() decodes via CImageInput/createImageInput().
add_executable(test_tile_source_synthesized_info test_tile_source_synthesized_info.cpp)
target_compile_features(test_tile_source_synthesized_info PRIVATE cxx_std_20)
target_include_directories(test_tile_source_synthesized_info PRIVATE ${TEXTURE_TILE_INCLUDE_DIRS})
target_link_libraries(test_tile_source_synthesized_info PRIVATE ribRender openrendercommon PNG::PNG)
add_test(NAME TileSource_SynthesizedInfo COMMAND test_tile_source_synthesized_info)
set_tests_properties(TileSource_SynthesizedInfo PROPERTIES TIMEOUT 30 LABELS "texture_tile;unit"
    ENVIRONMENT "IMAGE_INPUT_FIXTURES_DIR=${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures")

# ---------------------------------------------------------------------------
# Opt-in disk cache tests (spec 020-runtime-tile-paging, User Story 2,
# T018b/T019/T020/T021/T022). Each drives real orender subprocesses
# against a shared cache-scene.rib.in template (see that file's own header
# comment) rather than a hand-constructed CShadingContext -- same
# precedent as TileSource_Concurrency above.
# ---------------------------------------------------------------------------
set(CACHE_SCENE_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/cache-scene.rib.in)
set(CACHE_SOURCE_PNG ${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures/large_rgb8.png)

add_test(NAME TextureTile_CacheByteIdentical
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_texture_cache_byte_identical.sh
        $<TARGET_FILE:orender>
        ${CACHE_SCENE_TEMPLATE}
        ${CACHE_SOURCE_PNG})
set_tests_properties(TextureTile_CacheByteIdentical PROPERTIES
    TIMEOUT 60
    LABELS "texture_tile;unit"
    ENVIRONMENT "${TEXTURE_TILE_ENV}"
    DEPENDS orender)

add_test(NAME TextureTile_CacheDefaultDirectory
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_texture_cache_default_directory.sh
        $<TARGET_FILE:orender>
        ${CACHE_SCENE_TEMPLATE}
        ${CACHE_SOURCE_PNG})
set_tests_properties(TextureTile_CacheDefaultDirectory PROPERTIES
    TIMEOUT 60
    LABELS "texture_tile;unit"
    ENVIRONMENT "${TEXTURE_TILE_ENV}"
    DEPENDS orender)

add_test(NAME TextureTile_CacheStaleDetection
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_texture_cache_stale_detection.sh
        $<TARGET_FILE:orender>
        ${CACHE_SCENE_TEMPLATE}
        ${CACHE_SOURCE_PNG}
        ${TEXTURE_TILE_FIXTURES_DIR}/cache_stale_v2.png)
set_tests_properties(TextureTile_CacheStaleDetection PROPERTIES
    TIMEOUT 60
    LABELS "texture_tile;unit"
    ENVIRONMENT "${TEXTURE_TILE_ENV}"
    DEPENDS orender)

add_test(NAME TextureTile_CacheConcurrentWrite
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_texture_cache_concurrent_write.sh
        $<TARGET_FILE:orender>
        ${CACHE_SCENE_TEMPLATE}
        ${CACHE_SOURCE_PNG}
        8)
set_tests_properties(TextureTile_CacheConcurrentWrite PROPERTIES
    TIMEOUT 120
    LABELS "texture_tile;unit"
    ENVIRONMENT "${TEXTURE_TILE_ENV}"
    DEPENDS orender)

# ---------------------------------------------------------------------------
# Texture system tests (spec 027, Phase 2: written first, red until their API exists)
# ---------------------------------------------------------------------------
# Every test below needs an API that does not exist yet (ri/texture/textureSystem.h from Phase 5,
# ri/texture/textureResolver.h from Phase 4), so each executable is EXCLUDE_FROM_ALL and each test
# DISABLED: the main build and every checkpoint stay green, and the red state is the executable's
# build failure (`cmake --build build-027 --target <exe>`, recorded by T033 in validation.md).
# The task that provides the API removes both markers and builds the test green:
#   TextureSystem_NullResolver                      T050a (Phase 4)
#   TextureSystem_InfoMiss                          T063a (Phase 5)
#   TextureSystem_ConcurrentFirstLoad               T064  (Phase 5, red by assertion first)
#   TextureSystem_ConcurrentFirstLoadExternalMutex  T071a (Phase 6)
#   TextureSystem_StreamCursor                      T081  (Phase 6)
# The tests do not use a renderer: no RiBegin, no frame; they link ribRender like TileSource_TiffInfo.
# ENABLED: the API exists, so the executable is part of `all` and the test runs (the enabling task passes it).
function(add_texture_system_test NAME EXE SOURCE)
    cmake_parse_arguments(ARG "ENABLED" "" "" ${ARGN})
    if(ARG_ENABLED)
        add_executable(${EXE} ${SOURCE})
    else()
        add_executable(${EXE} EXCLUDE_FROM_ALL ${SOURCE})
    endif()
    target_compile_features(${EXE} PRIVATE cxx_std_20)
    target_include_directories(${EXE} PRIVATE ${TEXTURE_TILE_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${EXE} PRIVATE ribRender openrendercommon TIFF::TIFF PNG::PNG)
    add_test(NAME ${NAME} COMMAND ${EXE} ${ARG_UNPARSED_ARGUMENTS})
    set_tests_properties(${NAME} PROPERTIES
        TIMEOUT 120
        LABELS "texture_tile;unit"
        ENVIRONMENT "TEXTURE_TILE_FIXTURES_DIR=${TEXTURE_TILE_FIXTURES_DIR};IMAGE_INPUT_FIXTURES_DIR=${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures")
    if(NOT ARG_ENABLED)
        set_tests_properties(${NAME} PROPERTIES DISABLED TRUE)
    endif()
endfunction()

# T-C1 (T022, T022a): one executable, two entries selected by the argument. Run at -j2 (4 threads x 200 repeats).
add_texture_system_test(TextureSystem_ConcurrentFirstLoad test_texture_system_concurrent_first_load
    test_texture_system_concurrent_first_load.cpp)
add_test(NAME TextureSystem_ConcurrentFirstLoadExternalMutex
    COMMAND test_texture_system_concurrent_first_load external-mutex)
set_tests_properties(TextureSystem_ConcurrentFirstLoadExternalMutex PROPERTIES
    TIMEOUT 120
    LABELS "texture_tile;unit"
    DISABLED TRUE
    ENVIRONMENT "TEXTURE_TILE_FIXTURES_DIR=${TEXTURE_TILE_FIXTURES_DIR}")

# T022b: two small executables (NullResolver needs textureResolver.h only; InfoMiss needs textureSystem.h)
add_texture_system_test(TextureSystem_NullResolver test_texture_system_null_resolver
    test_texture_system_null_resolver.cpp ENABLED) # enabled by T050a (Phase 4): textureResolver.h exists
add_texture_system_test(TextureSystem_InfoMiss test_texture_system_info_miss
    test_texture_system_info_miss.cpp)

# T-K1 (T023): dimensions consumed per lookup class; renders the deep shadow fixture with orender
add_texture_system_test(TextureSystem_StreamCursor test_texture_system_stream_cursor
    test_texture_system_stream_cursor.cpp
    ${TEXTURE_TILE_FIXTURES_DIR}
    ${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures
    ${CMAKE_SOURCE_DIR}/examples/rib/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/deep-shadow-fixture.rib)
add_dependencies(test_texture_system_stream_cursor orender)
set_tests_properties(TextureSystem_StreamCursor PROPERTIES
    ENVIRONMENT "${TEXTURE_TILE_ENV};ORENDER_BIN=$<TARGET_FILE:orender>")
