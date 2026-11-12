# Spec 032 (GI cache split): the tests of the split of the irradiance cache and the photon map out of Hiders and of
# the viewer's ties out of Texturing.
#
#   Fixture F1   GiFixture_ICache   renders scenes/icache-fixture.rib at -t:1 and leaves a small irradiance cache
#                                   in the build tree (a CTest FIXTURES_SETUP). On the starting tree a cache can only
#                                   be filled by a render: insertion there is private to CIrradianceCache.
#   T-A          GiRemote_LoopbackCache, GiRemote_LoopbackPointCloud
#                                   the wire of CRemoteICacheChannel and CRemotePtCloudChannel over socketpair()
#                                   loopbacks in one process (FR-012). A characterization: recorded green on the
#                                   unmodified tree, green after every phase.
#   T-F          GiFiles_Unchanged  the photon map (field by field) and the irradiance cache (SHA-256) written by
#                                   two renders (FR-019). A characterization.
#
# The references are tests/gi_split/reference/<platform>/ (macos, linux). They are recorded ONCE on the unmodified
# tree, twice, the two recordings agreeing, by the tests' own `--record` mode (see specs/032-gi-cache-split/tasks.md,
# T020 and T023). A test never regenerates a reference.
#
# The tests of the later phases are red-first: they are defined only when -DOPENRENDER_032_RED_TESTS=ON, in a scratch
# tree, never in the main `build` (Phase 2 fills the block below; each is moved out of it in the phase that makes it
# green). tests/architecture's GiSplit_Boundary reads GI032_BOUNDARY_CHECKS, the sub-checks the finished phases enable.

option(OPENRENDER_032_RED_TESTS "Define the spec 032 red-first tests (scratch trees only: they do not build before their phase)" OFF)
# The sub-checks the finished phases have turned on (each phase appends its own in its test gate). A tree configured before a phase
# landed keeps its cached value: configure with -DGI032_BOUNDARY_CHECKS="<list>" (or an empty list to run none).
#   Phase 3: friends-texture texture-names-viewer hiders-no-viewer-irradiance
#   Phase 4: friends-hiders texture-names-shading texture-names-core texture-includes hiders-no-texture files-absent
#   Phase 5: hiders-no-viewer
set(GI032_BOUNDARY_CHECKS "friends-texture;texture-names-viewer;hiders-no-viewer-irradiance;friends-hiders;texture-names-shading;texture-names-core;texture-includes;hiders-no-texture;files-absent;hiders-no-viewer" CACHE STRING
    "Sub-checks of GiSplit_Boundary that are enabled (a ;-list); empty runs none")

if(APPLE)
    set(GI_SPLIT_PLATFORM macos)
else()
    set(GI_SPLIT_PLATFORM linux)
endif()
set(GI_SPLIT_REFERENCE ${CMAKE_CURRENT_SOURCE_DIR}/reference/${GI_SPLIT_PLATFORM})

# The environment the visual tests run orender in (tests/visual/CMakeLists.txt): the build tree's shaders and
# displays, the private ORENDERHOME that carries .orenderrc, and the tracked geometry and texture directories.
set(GI_SPLIT_DISPLAY_PATH "$<TARGET_FILE_DIR:file>:$<TARGET_FILE_DIR:rgbe>:$<TARGET_FILE_DIR:framebuffer>")
if(HAVE_OPENEXR)
    set(GI_SPLIT_DISPLAY_PATH "${GI_SPLIT_DISPLAY_PATH}:$<TARGET_FILE_DIR:openexr>")
endif()
set(GI_SPLIT_ENV
    "SHADERS=${CMAKE_BINARY_DIR}/shaders"
    "ORENDERHOME=${CMAKE_BINARY_DIR}/tests/visual/_home"
    "DISPLAYS=${GI_SPLIT_DISPLAY_PATH}"
    "GEOMETRIES=${CMAKE_SOURCE_DIR}/geometry"
    "TEXTURES=${CMAKE_SOURCE_DIR}/examples/rib/tests")

set(GI_SPLIT_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_BINARY_DIR})

# Scratch directories, one per test (each test runs in its own, so a parallel run never shares a file)
foreach(_d fixture files loopback-cache loopback-pointcloud)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_d})
endforeach()

# ---------------------------------------------------------------------------------------------------------
# Fixture F1
# ---------------------------------------------------------------------------------------------------------
set(GI_SPLIT_F1_CACHE ${CMAKE_CURRENT_BINARY_DIR}/fixture/icache-fixture.icache)
add_test(NAME GiFixture_ICache
    COMMAND orender -t:1 ${CMAKE_CURRENT_SOURCE_DIR}/scenes/icache-fixture.rib
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fixture)
set_tests_properties(GiFixture_ICache PROPERTIES
    TIMEOUT 120
    LABELS "gi_split"
    FIXTURES_SETUP GiFixture_ICache
    ENVIRONMENT "${GI_SPLIT_ENV}"
    DEPENDS orender)

# ---------------------------------------------------------------------------------------------------------
# T-F GiFiles_Unchanged
# ---------------------------------------------------------------------------------------------------------
add_executable(test_gi_files test_gi_files.cpp)
target_compile_features(test_gi_files PRIVATE cxx_std_20)
add_test(NAME GiFiles_Unchanged
    COMMAND test_gi_files $<TARGET_FILE:orender> ${CMAKE_SOURCE_DIR}/examples/rib/tests
            ${CMAKE_CURRENT_BINARY_DIR}/files ${GI_SPLIT_REFERENCE}/files.txt)
set_tests_properties(GiFiles_Unchanged PROPERTIES
    TIMEOUT 360
    LABELS "gi_split;regression;photon"
    ENVIRONMENT "${GI_SPLIT_ENV}"
    DEPENDS orender)

# ---------------------------------------------------------------------------------------------------------
# T-A GiRemote_LoopbackCache, GiRemote_LoopbackPointCloud
# ---------------------------------------------------------------------------------------------------------
add_executable(test_gi_remote_loopback test_gi_remote_loopback.cpp)
target_compile_features(test_gi_remote_loopback PRIVATE cxx_std_20)
target_include_directories(test_gi_remote_loopback PRIVATE ${GI_SPLIT_INCLUDES})
target_link_libraries(test_gi_remote_loopback PRIVATE ribRender openrendercommon Threads::Threads)

add_test(NAME GiRemote_LoopbackCache
    COMMAND test_gi_remote_loopback cache ${GI_SPLIT_F1_CACHE}
            ${CMAKE_CURRENT_BINARY_DIR}/loopback-cache ${GI_SPLIT_REFERENCE}/loopback-cache.bin)
add_test(NAME GiRemote_LoopbackPointCloud
    COMMAND test_gi_remote_loopback pointcloud -
            ${CMAKE_CURRENT_BINARY_DIR}/loopback-pointcloud ${GI_SPLIT_REFERENCE}/loopback-pointcloud.bin)
set_tests_properties(GiRemote_LoopbackCache GiRemote_LoopbackPointCloud PROPERTIES
    TIMEOUT 60
    LABELS "gi_split;regression"
    ENVIRONMENT "${GI_SPLIT_ENV}")
set_tests_properties(GiRemote_LoopbackCache PROPERTIES FIXTURES_REQUIRED GiFixture_ICache)

# ---------------------------------------------------------------------------------------------------------
# T-A2 GiRemote_LoopbackLookup, GiCache_KindMismatch (enabled in Phase 4, T069)
# ---------------------------------------------------------------------------------------------------------
# T-A2: GiRemote_LoopbackLookup and GiCache_KindMismatch (one source, one executable, two names)
add_executable(test_gi_remote_lookup test_gi_remote_lookup.cpp)
target_compile_features(test_gi_remote_lookup PRIVATE cxx_std_20)
target_include_directories(test_gi_remote_lookup PRIVATE ${GI_SPLIT_INCLUDES})
target_link_libraries(test_gi_remote_lookup PRIVATE ribRender openrendercommon Threads::Threads)
add_test(NAME GiRemote_LoopbackLookup
    COMMAND test_gi_remote_lookup lookup ${GI_SPLIT_F1_CACHE} ${CMAKE_CURRENT_BINARY_DIR}/loopback-lookup)
add_test(NAME GiCache_KindMismatch
    COMMAND test_gi_remote_lookup kind - ${CMAKE_CURRENT_BINARY_DIR}/kind-mismatch)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/loopback-lookup ${CMAKE_CURRENT_BINARY_DIR}/kind-mismatch)
set_tests_properties(GiRemote_LoopbackLookup GiCache_KindMismatch PROPERTIES
    TIMEOUT 60
    LABELS "gi_split;regression"
    ENVIRONMENT "${GI_SPLIT_ENV}")
set_tests_properties(GiRemote_LoopbackLookup PROPERTIES FIXTURES_REQUIRED GiFixture_ICache)

# ---------------------------------------------------------------------------------------------------------
# The reference capture: build targets, never ctests (neither is part of `all`). Modelled on spec 037's
# plugin-abi-capture-baselines.
#   gi-split-capture-references          writes the four references of T-A, T-B and T-F under reference/<platform>/ of
#                                        tests/gi_split and tests/preview; a missing one is written, an existing one is
#                                        never overwritten unless the tree was configured with -DOR032_RECAPTURE=ON
#   gi-split-capture-references-scratch  records the same four into the build tree, for the check that two recordings,
#                                        each made from scratch, are identical byte for byte
# Each run makes a fresh work directory and renders fixture F1 into it first.
# ---------------------------------------------------------------------------------------------------------
option(OR032_RECAPTURE "Let gi-split-capture-references overwrite existing references" OFF)
set(GI_SPLIT_PREVIEW_REFERENCE ${CMAKE_SOURCE_DIR}/tests/preview/reference/${GI_SPLIT_PLATFORM})
set(GI_SPLIT_CAPTURE_COMMON
    -DORENDER=$<TARGET_FILE:orender>
    -DRIB_DIR=${CMAKE_SOURCE_DIR}/examples/rib/tests
    -DF1_RIB=${CMAKE_CURRENT_SOURCE_DIR}/scenes/icache-fixture.rib
    -DT_FILES=$<TARGET_FILE:test_gi_files>
    -DT_LOOPBACK=$<TARGET_FILE:test_gi_remote_loopback>
    -DT_SCENE=$<TARGET_FILE:test_data_scene_unchanged>)
add_custom_target(gi-split-capture-references
    COMMAND ${CMAKE_COMMAND} -E env ${GI_SPLIT_ENV}
            ${CMAKE_COMMAND} -DRECAPTURE=${OR032_RECAPTURE} ${GI_SPLIT_CAPTURE_COMMON}
            -DWORKROOT=${CMAKE_CURRENT_BINARY_DIR}/capture_work
            -DOUT_GI=${GI_SPLIT_REFERENCE} -DOUT_PREVIEW=${GI_SPLIT_PREVIEW_REFERENCE}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/capture_references.cmake
    DEPENDS orender test_gi_files test_gi_remote_loopback test_data_scene_unchanged
    COMMENT "Recording the spec 032 references (an existing reference is never overwritten)"
    VERBATIM)
add_custom_target(gi-split-capture-references-scratch
    COMMAND ${CMAKE_COMMAND} -E env ${GI_SPLIT_ENV}
            ${CMAKE_COMMAND} -DRECAPTURE=ON ${GI_SPLIT_CAPTURE_COMMON}
            -DWORKROOT=${CMAKE_CURRENT_BINARY_DIR}/capture_work_scratch
            -DOUT_GI=${CMAKE_CURRENT_BINARY_DIR}/capture_scratch/gi -DOUT_PREVIEW=${CMAKE_CURRENT_BINARY_DIR}/capture_scratch/preview
            -P ${CMAKE_CURRENT_SOURCE_DIR}/capture_references.cmake
    DEPENDS orender test_gi_files test_gi_remote_loopback test_data_scene_unchanged
    COMMENT "Recording the spec 032 references into the build tree"
    VERBATIM)

# ---------------------------------------------------------------------------------------------------------
# T-E GiKeys_Photons, GiKeys_IrradianceRecords, GiKeys_PhotonsThreads (enabled in Phase 7, T108a)
# ---------------------------------------------------------------------------------------------------------
# The source takes exactly three arguments after the program name in every mode (its argc test), so the parent cases pass a placeholder
# third one, as GiCache_KindMismatch does
# GiKeys_PhotonsThreads is opt-in (label gi_split_threads, not in the everyday run)
add_executable(test_gi_keys test_gi_keys.cpp)
target_compile_features(test_gi_keys PRIVATE cxx_std_20)
target_include_directories(test_gi_keys PRIVATE ${GI_SPLIT_INCLUDES} ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(test_gi_keys PRIVATE ribRender libshader_shading openrendercommon Threads::Threads)
foreach(_d keys-photons keys-records keys-photons-threads)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${_d})
endforeach()
add_test(NAME GiKeys_Photons COMMAND test_gi_keys photons ${CMAKE_CURRENT_BINARY_DIR}/keys-photons -)
add_test(NAME GiKeys_IrradianceRecords COMMAND test_gi_keys records ${CMAKE_CURRENT_BINARY_DIR}/keys-records -)
add_test(NAME GiKeys_PhotonsThreads COMMAND test_gi_keys photons-threads ${CMAKE_CURRENT_BINARY_DIR}/keys-photons-threads -)
set_tests_properties(GiKeys_Photons GiKeys_IrradianceRecords PROPERTIES
    TIMEOUT 300 LABELS "gi_split;regression" ENVIRONMENT "${GI_SPLIT_ENV}")
set_tests_properties(GiKeys_PhotonsThreads PROPERTIES
    TIMEOUT 600 LABELS "gi_split_threads" ENVIRONMENT "${GI_SPLIT_ENV}")

# No red-first test is left: every one is defined above or in tests/unit/texture_only/CMakeLists.txt. The option stays so that a
# scratch configure that still passes it does not warn.

message(STATUS "  - spec 032 gi_split tests (GiFixture_ICache, GiFiles_Unchanged, GiRemote_Loopback*), platform ${GI_SPLIT_PLATFORM}")
