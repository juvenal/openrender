# Texturing-only isolation test (spec 027, T-T1, FR-018(a), SC-003, User Story 1).
#
# Compiles the Texturing sources (src/ri/texture) a SECOND time into a test library on a
# deliberately NARROW include path and links only openrendercommon plus the image libraries,
# so a bare #include "shading.h", "renderer.h", "options.h" or "attributes.h" in a Texturing
# source fails to compile here, and an undefined reference into the renderer core, Shading or
# the hiders fails to link. Precedent: tests/unit/state/CMakeLists.txt (spec 025).
#
# Why this is behind an option: archcheck's loadMakefiles walks every DependInfo.cmake in the
# build tree, EXCLUDE_FROM_ALL targets included, and exits 2 "incomplete build tree" when an
# in-scope object (any source under src/) is missing. The library below compiles src/ri/texture
# sources, so it must either be built or not exist. The option is ON since task T092 (the
# library and the test are part of `all`, so archcheck finds the objects); turning it OFF
# leaves `ctest -L architecture` with only the production units. The red evidence of T-T1 was
# taken in a scratch build directory (T034).
option(OPENRENDER_TEXTURE_ONLY_TESTS "Build the Texturing-only isolation test" ON)

if(OPENRENDER_TEXTURE_ONLY_TESTS)
    # A staging directory holding a copy of ri_config.h. src/ri/core itself is NOT on the
    # include path: it also holds the Shading shims shader.h and shading.h.
    set(TEXTURE_ONLY_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/texture_only_include)
    configure_file(${CMAKE_SOURCE_DIR}/src/ri/core/ri_config.h
                   ${TEXTURE_ONLY_INCLUDE_DIR}/ri_config.h COPYONLY)

    add_library(texturing_only_sources STATIC
        ${CMAKE_SOURCE_DIR}/src/ri/texture/texture.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/texture/texmake.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/texture/imageInput.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/texture/imageInputExr.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/texture/imageInputPng.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/texture/imageInputRgbe.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/texture/imageInputTiff.cpp
        ${CMAKE_SOURCE_DIR}/src/ri/texture/textureSystem.cpp
    )
    target_link_libraries(texturing_only_sources PUBLIC
        openrender_common_flags openrendercommon TIFF::TIFF PNG::PNG ZLIB::ZLIB)
    if(HAVE_OPENEXR)
        target_compile_definitions(texturing_only_sources PUBLIC HAVE_OPENEXR)
        target_link_libraries(texturing_only_sources PUBLIC ${OPENRENDER_OPENEXR_LIBS})
    endif()
    target_compile_features(texturing_only_sources PUBLIC cxx_std_20)
    target_include_directories(texturing_only_sources PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/ri/texture
        ${CMAKE_BINARY_DIR}
        ${TEXTURE_ONLY_INCLUDE_DIR}
    )

    add_executable(test_texture_only_standalone test_texture_only_standalone.cpp)
    target_link_libraries(test_texture_only_standalone PRIVATE texturing_only_sources)
    add_test(NAME TextureOnly_Standalone
        COMMAND test_texture_only_standalone
            ${CMAKE_SOURCE_DIR}/specs/027-texture-system/tools/master-lookup-values.txt
            ${CMAKE_SOURCE_DIR}/tests/unit/texture_tile/fixtures
            ${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures)
    set_tests_properties(TextureOnly_Standalone PROPERTIES
        TIMEOUT 60
        LABELS "texture_only;unit")
endif()
