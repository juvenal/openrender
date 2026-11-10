# Spec 029 (hider / shading separation): the link and scope tests. Label `link` (everyday run) and `link-package`
# (opt-in, registered only with -DOPENRENDER_LINK_PACKAGE_TEST=ON).
#
#   Link_ShadingSelfContained (T-L1)  libshader_shading imports nothing: no flat-namespace bind (macOS), no symbol libRIBRender defines (Linux)
#   Link_StaticConsumer       (T-L2)  a program linked plainly against ribRender_static renders the #24 scene, with a renderer backend
#   Link_NoWorkaroundText     (T-L3)  none of the link workarounds is spelled in a build file; the load stub is gone
#   Link_PackageStaticConsumer(T-L4)  the same through the installed package (opt-in: builds and installs a second tree)
#   ShadingScope_Greps        (T-S1)  the scope of Shading and the hiders, one sub-check per phase
#
# None of these carries the `architecture` label (a regular expression: `link` and `link-package` do not contain it).

# The sub-checks of ShadingScope_Greps that are true so far. Each phase appends the sub-checks it makes true
# (Phase 3: S1a;S1b, Phase 5: S1c;S1f, Phase 6: S1d, Phase 7: S1e). An empty list is an error of the script, so the
# registered test can never pass vacuously.
set(OR029_SCOPE_CHECKS "S1a;S1b;S1c;S1d;S1e;S1f")  # Phase 7

# ---- T-L1 ------------------------------------------------------------------------------------------------------
# Enabled in Phase 3 (registered like an ordinary test; no DISABLED marker)
add_test(NAME Link_ShadingSelfContained
    COMMAND ${CMAKE_COMMAND} "-DLIB=$<TARGET_FILE:libshader_shading>" "-DRENDER=$<TARGET_FILE:ribRender>" "-DSYSTEM=${CMAKE_SYSTEM_NAME}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_shading_self_contained.cmake)
set_tests_properties(Link_ShadingSelfContained PROPERTIES TIMEOUT 120 LABELS "link")

# ---- T-L3 ------------------------------------------------------------------------------------------------------
# Enabled in Phase 4 (registered like an ordinary test; no DISABLED marker)
add_test(NAME Link_NoWorkaroundText
    COMMAND ${CMAKE_COMMAND} "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}" -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_workaround_text.cmake)
set_tests_properties(Link_NoWorkaroundText PROPERTIES TIMEOUT 60 LABELS "link")

# ---- T-S1 ------------------------------------------------------------------------------------------------------
# The include path of the shading library is read from the generated compile_commands.json when it exists and from
# the flags.make of its target otherwise (build tree, generator-specific): the script takes the directory of the
# target's build files.
# Enabled in Phase 3 with the sub-checks that are true so far (OR029_SCOPE_CHECKS; S1c and S1f joined in Phase 5, S1d in Phase 6, S1e in Phase 7)
add_test(NAME ShadingScope_Greps
    COMMAND ${CMAKE_COMMAND} "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}" "-DCHECKS=${OR029_SCOPE_CHECKS}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_scope.cmake)
set_tests_properties(ShadingScope_Greps PROPERTIES TIMEOUT 60 LABELS "link")

# ---- T-L2 ------------------------------------------------------------------------------------------------------
# A program linked plainly against ribRender_static (no force-load, whole-archive or export-dynamic). Enabled in
# Phase 4 (it joined the normal build then; before, it aborted at launch).
add_executable(test_link_static_consumer static_consumer.cpp)
target_compile_features(test_link_static_consumer PRIVATE cxx_std_20)
target_include_directories(test_link_static_consumer PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/parse)
target_link_libraries(test_link_static_consumer PRIVATE openrender_common_flags ribRender_static libshader_shading openrendercommon ${CMAKE_DL_LIBS})
add_dependencies(test_link_static_consumer openrender_shaders)

set(OR029_STATIC_CONSUMER_FORMATS rslo)
if(OPENRENDER_HAVE_LLVM)
    list(APPEND OR029_STATIC_CONSUMER_FORMATS slo)
endif()
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/Link_StaticConsumer)
add_test(NAME Link_StaticConsumer
    COMMAND ${CMAKE_COMMAND} "-DEXE=$<TARGET_FILE:test_link_static_consumer>" "-DFORMATS=${OR029_STATIC_CONSUMER_FORMATS}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_static_consumer.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/Link_StaticConsumer)
set_tests_properties(Link_StaticConsumer PROPERTIES TIMEOUT 300 LABELS "link"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>")

# ---- Spec 030: Link_StaticConsumerRegistries (FR-006, SC-008, research R13) -------------------------------------
# A sibling of the approved Link_StaticConsumer, which stays byte-for-byte as it is: the same plain static link, and the
# registries must still list every built-in hider (raytrace reyes zbuffer photon, aliases hidden and stochastic) and the
# integrator classic, and render one frame with each hider under Integrator "classic". Red until Phase 9 (no registries):
# behind the red-first option.
if(OPENRENDER_030_RED_TESTS)
    add_executable(test_link_static_consumer_registries static_consumer_registries.cpp)
    target_compile_features(test_link_static_consumer_registries PRIVATE cxx_std_20)
    target_include_directories(test_link_static_consumer_registries PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri
        ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/transport)
    target_link_libraries(test_link_static_consumer_registries PRIVATE openrender_common_flags ribRender_static libshader_shading openrendercommon ${CMAKE_DL_LIBS})
    add_dependencies(test_link_static_consumer_registries openrender_shaders)

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/Link_StaticConsumerRegistries)
    add_test(NAME Link_StaticConsumerRegistries
        COMMAND ${CMAKE_COMMAND} "-DEXE=$<TARGET_FILE:test_link_static_consumer_registries>" "-DFORMATS=${OR029_STATIC_CONSUMER_FORMATS}"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_static_consumer_registries.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/Link_StaticConsumerRegistries)
    set_tests_properties(Link_StaticConsumerRegistries PROPERTIES TIMEOUT 300 LABELS "link"
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>")
endif()

# ---- T-L4 ------------------------------------------------------------------------------------------------------
# Opt-in: configures, builds and installs a scratch copy of this tree (CMAKE_INSTALL_PREFIX=<scratch>, so GitHub #23 is
# not hit), then configures, builds and runs the consumer of package/ through find_package(openRender COMPONENTS Static).
# The runner builds with `cmake --build -j<n>` directly (never cpujob: the outer ctest job holds the machine-wide lock).
# PREFIX_DIR reuses an install a previous run made (the Phase 2 red run and T015's build-pkg); without it the runner
# builds its own. On macOS the consumer is configured with the vcpkg toolchain line read from this build's cache
# (CMAKE_TOOLCHAIN_FILE, VCPKG_MANIFEST_MODE=OFF, VCPKG_INSTALLED_DIR, VCPKG_TARGET_TRIPLET): GitHub #141.
set(OR029_PACKAGE_PREFIX "" CACHE PATH "Link_PackageStaticConsumer: reuse an existing install prefix instead of building and installing a scratch tree (evidence runs)")
set(OR029_PACKAGE_REMOVE_BLOCK OFF CACHE BOOL "Link_PackageStaticConsumer: delete the package's workaround block from a scratch copy of the installed config (the red mechanism; the consumer is then expected to fail)")
if(OPENRENDER_LINK_PACKAGE_TEST)
    if(APPLE)
        set(_package_jobs 4)
    else()
        set(_package_jobs 2)
    endif()
    add_test(NAME Link_PackageStaticConsumer
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}" "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/package_work"
            "-DCONSUMER_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package" "-DJOBS=${_package_jobs}"
            "-DFORMATS=${OR029_STATIC_CONSUMER_FORMATS}" "-DGENERATOR=${CMAKE_GENERATOR}" "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
            "-DPREFIX_DIR=${OR029_PACKAGE_PREFIX}" "-DREMOVE_PACKAGE_BLOCK=${OR029_PACKAGE_REMOVE_BLOCK}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_package_test.cmake)
    set_tests_properties(Link_PackageStaticConsumer PROPERTIES TIMEOUT 7200 LABELS "link-package")
endif()
