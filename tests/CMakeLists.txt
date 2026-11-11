# Test suite for Pixie C++20 modernization

# Create test executable for 64-bit portability
add_executable(test_64bit_portability test_64bit_portability.cpp)

# Link against common flags (includes C++20 standard)
target_link_libraries(test_64bit_portability PRIVATE openrender_common_flags)

# Include directories for accessing source headers
target_include_directories(test_64bit_portability PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}  # For config.h
)

# Add test to CTest
add_test(NAME Portability64Bit COMMAND test_64bit_portability)

# Set test properties
set_tests_properties(Portability64Bit PROPERTIES
    TIMEOUT 30
    LABELS "portability;64bit;core"
)

# Create test executable for sampleDisk() (depth-of-field lens sampling)
add_executable(test_disk_sampling test_disk_sampling.cpp)

# Link against common flags (includes C++20 standard)
target_link_libraries(test_disk_sampling PRIVATE openrender_common_flags)

# Include directories for accessing source headers
target_include_directories(test_disk_sampling PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}  # For config.h
)

# Add test to CTest
add_test(NAME DiskSampling COMMAND test_disk_sampling)

# Set test properties
set_tests_properties(DiskSampling PROPERTIES
    TIMEOUT 30
    LABELS "sampling;core"
)

# Future tests can be added here:
# add_executable(test_atomics test_atomics.cpp)
# add_test(NAME AtomicOperations COMMAND test_atomics)

# Spec 038 (Scene Interface untangle): the characterization references are written by one build target, never by
# a ctest run. `cmake --build <build> --target scene-interface-capture-baselines` writes each reference that does
# not exist yet, from the tree as it is (run it once, on the unmodified tree, before Phase 2), and refuses to
# overwrite an existing one unless the tree was configured with -DOR038_RECAPTURE=ON. Modelled on
# plugin-abi-capture-baselines (tests/plugins/CMakeLists.txt). The target is not part of `all`. Each directory
# that owns a reference adds its own capture step with add_dependencies(), so this target must exist before
# those directories are added.
option(OR038_RECAPTURE "Let scene-interface-capture-baselines overwrite existing references" OFF)
# On the Linux validation tree the same target runs with -DOR038_CAPTURE_DIFFERENCES=ON: it then writes a
# reference/linux/<name> only where the Linux result differs from the macOS reference (and writes nothing
# for a result that is equal), so every platform difference is a file that can be read and explained.
option(OR038_CAPTURE_DIFFERENCES "scene-interface-capture-baselines writes reference/linux/ files only where Linux differs" OFF)
# (cache entries, because src/libshader/tests reads them and is not below this directory)
if(OR038_RECAPTURE)
    set(OR038_RECAPTURE_FLAG --recapture CACHE INTERNAL "")
else()
    set(OR038_RECAPTURE_FLAG "" CACHE INTERNAL "") # empty: expands to no argument
endif()
if(OR038_CAPTURE_DIFFERENCES)
    set(OR038_CAPTURE_FLAG --capture-differences CACHE INTERNAL "")
    set(OR038_CAPTURE_MODE capture-differences CACHE INTERNAL "")
else()
    set(OR038_CAPTURE_FLAG --capture CACHE INTERNAL "")
    set(OR038_CAPTURE_MODE capture CACHE INTERNAL "")
endif()
add_custom_target(scene-interface-capture-baselines
    COMMENT "Capturing the spec 038 characterization references (an existing reference is never overwritten)")

# Add test subdirectories
add_subdirectory(framebuffer)
add_subdirectory(imager)
add_subdirectory(preview)
add_subdirectory(visual)
add_subdirectory(shaders)
add_subdirectory(shading_parity)
add_subdirectory(unit/csg)
add_subdirectory(unit/blobby)
add_subdirectory(unit/image_input)
add_subdirectory(unit/texture_tile)
add_subdirectory(unit/texture_only)
add_subdirectory(unit/polynomial)
add_subdirectory(unit/rib_parse)
add_subdirectory(unit/photon_map)
add_subdirectory(unit/common)
add_subdirectory(unit/sampling)
add_subdirectory(unit/raster)
add_subdirectory(unit/geometry)
add_subdirectory(unit/state)
add_subdirectory(unit/camera)
add_subdirectory(unit/motion)
add_subdirectory(unit/scene_interface)  # spec 038: services, active context, writer and `if` characterizations
add_subdirectory(unit/registry)         # spec 030: hider and integrator registries (the characterization tests; the rest behind OPENRENDER_030_RED_TESTS)
add_subdirectory(precomp)               # spec 038: the precomputation tool (Precomp_OutputUnchanged, Precomp_NoHiderCore)
# Permanent: registers the opt-in Link_PackageStaticConsumer (label link-package). It builds and installs a second
# copy of the tree, so it is not part of the everyday run; the final phase runs it with this option ON.
option(OPENRENDER_LINK_PACKAGE_TEST "Register Link_PackageStaticConsumer: builds and installs a scratch tree, then builds a consumer through find_package(openRender COMPONENTS Static)" OFF)

# Spec 030 (hider and integrator registry): the red-first convention. Each new compiled test lives in its own
# executable inside `if(OPENRENDER_030_RED_TESTS)`, so one that cannot compile yet never breaks the build; the option is
# ON only in the red-evidence configure of Phase 2 (and by hand). The Test gate of the phase that makes a test compile
# moves its definition out of the block. Script tests are registered DISABLED while the option is OFF.
option(OPENRENDER_030_RED_TESTS "Spec 030: configure the new registry tests that are red until their phase (build failures are their red evidence)" OFF)

# After unit/state: shading_contract links its state_only_sources library
add_subdirectory(shading_contract)
add_subdirectory(link)
add_subdirectory(plugins)
add_subdirectory(convergence)
add_subdirectory(architecture)

# Spec 030 (hider and integrator registry), FR-017: the two NEW RIB-helper scripts (maintainer answer Q-C). The approved
# tests/python/test_prman.py and tests/lua/test_prman.lua were never registered in CTest and stay out of scope. Each new
# script is registered only when its interpreter is found; a host without one records a skip message at configure time
# (Lua is 5.5 on macOS and 5.4 on Linux: the script uses plain Lua). They were red until Phase 7 (the helpers had no
# Integrator method); the Test gate of Phase 7 enabled them.
find_program(OR030_PYTHON NAMES python3 python)
find_program(OR030_LUA NAMES lua lua5.4 lua5.3 luajit)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/helpers_work/python ${CMAKE_CURRENT_BINARY_DIR}/helpers_work/lua)
set(OR030_HELPER_TESTS "")
if(OR030_PYTHON)
    add_test(NAME Helpers_PrmanIntegratorPython COMMAND ${OR030_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/python/test_prman_integrator.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/helpers_work/python)
    set_tests_properties(Helpers_PrmanIntegratorPython PROPERTIES TIMEOUT 60 LABELS "helpers"
        ENVIRONMENT "PRMAN_PYTHON_DIR=${CMAKE_SOURCE_DIR}/src/python")
    list(APPEND OR030_HELPER_TESTS Helpers_PrmanIntegratorPython)
else()
    message(STATUS "Helpers_PrmanIntegratorPython skipped: no python3 found")
endif()
if(OR030_LUA)
    add_test(NAME Helpers_PrmanIntegratorLua COMMAND ${OR030_LUA} ${CMAKE_CURRENT_SOURCE_DIR}/lua/test_prman_integrator.lua
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/helpers_work/lua)
    set_tests_properties(Helpers_PrmanIntegratorLua PROPERTIES TIMEOUT 60 LABELS "helpers"
        ENVIRONMENT "PRMAN_LUA_DIR=${CMAKE_SOURCE_DIR}/src/lua")
    list(APPEND OR030_HELPER_TESTS Helpers_PrmanIntegratorLua)
else()
    message(STATUS "Helpers_PrmanIntegratorLua skipped: no lua interpreter found")
endif()

# Print test information
message(STATUS "Configured tests:")
message(STATUS "  - test_64bit_portability")
