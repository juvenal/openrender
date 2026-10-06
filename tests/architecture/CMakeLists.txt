# Architecture check: constitution Principle VIII domain boundaries (spec 022).
#
# archcheck reads the build tree (compiler depfiles and object files) and fails
# on any cross-domain dependency the allowed-dependency table forbids, unless
# it is allowlisted against an Open entry in DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md.
#
# `ctest --test-dir build -L architecture` runs every test registered here.

# The tool itself. Test-only, never installed, and deliberately linked against
# no renderer library: it inspects the renderer's build output from outside.
file(GLOB ARCHCHECK_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/archcheck/*.cpp)
add_executable(archcheck ${ARCHCHECK_SOURCES})
target_compile_features(archcheck PRIVATE cxx_std_20)
target_link_libraries(archcheck PRIVATE openrender_common_flags)

# Parser unit tests (depfile, include directive, allowlist line, registry
# heading, domain-map rule).
add_executable(test_archcheck_parsers unit/test_archcheck_parsers.cpp)
target_compile_features(test_archcheck_parsers PRIVATE cxx_std_20)
target_include_directories(test_archcheck_parsers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/archcheck)
target_link_libraries(test_archcheck_parsers PRIVATE openrender_common_flags)
add_test(NAME Architecture_Parsers COMMAND test_archcheck_parsers)
set_tests_properties(Architecture_Parsers PROPERTIES
    TIMEOUT 60 LABELS "architecture;unit")

# archcheck_add_fixture(<name> <expected-exit> <pass-regex>
#                       [BUILD_DIR <dir>] [EXCLUDE <relpath>...] [ARGS <arg>...])
#
# Compiles tests/architecture/fixtures/<name>/src/** as an OBJECT library, so
# the fixture has real .o and .o.d files in this build tree, and registers
# ctest ArchFixture_<name> ('-' becomes '_') running archcheck against it.
#
# Isolation is by --source-dir scoping (research R10): archcheck only analyses
# sources under <source-dir>/src/, so a fixture sees only its own objects even
# though every fixture shares ${CMAKE_BINARY_DIR}.
#
# The test passes only if archcheck exits with exactly <expected-exit> AND its
# output matches <pass-regex>; run_fixture.cmake checks both, because ctest's
# PASS_REGULAR_EXPRESSION alone would ignore the exit status.
#
#   BUILD_DIR  overrides --build-dir (e.g. an empty directory, for the
#              incomplete-build scenario)
#   EXCLUDE    fixture sources (relative to the fixture's src/) left out of
#              the OBJECT library, for the not-built scenario
#   ARGS       extra archcheck arguments (e.g. --platform linux)
function(archcheck_add_fixture name expected_exit pass_regex)
    cmake_parse_arguments(PARSE_ARGV 3 FX "" "BUILD_DIR" "EXCLUDE;ARGS")

    set(fixture_dir ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/${name})
    string(REPLACE "-" "_" id "${name}")

    file(GLOB_RECURSE fixture_sources CONFIGURE_DEPENDS
        ${fixture_dir}/src/*.c ${fixture_dir}/src/*.cc ${fixture_dir}/src/*.cpp)
    foreach(excluded IN LISTS FX_EXCLUDE)
        list(REMOVE_ITEM fixture_sources ${fixture_dir}/src/${excluded})
    endforeach()
    if(fixture_sources)
        add_library(archfixture_${id} OBJECT ${fixture_sources})
        target_compile_features(archfixture_${id} PRIVATE cxx_std_20)
        target_include_directories(archfixture_${id} PRIVATE ${fixture_dir}/src)
        target_link_libraries(archfixture_${id} PRIVATE openrender_common_flags)
    endif()

    if(FX_BUILD_DIR)
        set(build_dir ${FX_BUILD_DIR})
    else()
        set(build_dir ${CMAKE_BINARY_DIR})
    endif()

    add_test(NAME ArchFixture_${id}
        COMMAND ${CMAKE_COMMAND}
            -DARCHCHECK=$<TARGET_FILE:archcheck>
            -DEXPECTED_EXIT=${expected_exit}
            -DPASS_REGEX=${pass_regex}
            "-DARCHCHECK_ARGS=--source-dir;${fixture_dir};--build-dir;${build_dir};--config;${fixture_dir};--registry;${fixture_dir}/REGISTRY.md;${FX_ARGS}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_fixture.cmake)
    set_tests_properties(ArchFixture_${id} PROPERTIES
        TIMEOUT 60 LABELS "architecture")
endfunction()
