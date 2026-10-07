# Architecture check: constitution Principle VIII domain boundaries (spec 022).
#
# archcheck reads the build tree (compiler depfiles and object files) and fails
# on any cross-domain dependency the allowed-dependency table forbids, unless
# it is allowlisted against an Open entry in DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md.
#
# `ctest --test-dir build -L architecture` runs every test registered here.

# The tool itself. Test-only, never installed, and deliberately linked against
# no renderer library: it inspects the renderer's build output from outside.
# Everything but main.cpp goes into archcheck_core, which the unit tests link.
file(GLOB ARCHCHECK_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/archcheck/*.cpp)
list(REMOVE_ITEM ARCHCHECK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/archcheck/main.cpp)
add_library(archcheck_core STATIC ${ARCHCHECK_SOURCES})
target_compile_features(archcheck_core PUBLIC cxx_std_20)
target_include_directories(archcheck_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/archcheck)
target_link_libraries(archcheck_core PRIVATE openrender_common_flags)

add_executable(archcheck archcheck/main.cpp)
target_link_libraries(archcheck PRIVATE archcheck_core openrender_common_flags)

# Parser unit tests (depfile, include directive, allowlist line, registry
# heading, domain-map rule).
add_executable(test_archcheck_parsers unit/test_archcheck_parsers.cpp)
target_link_libraries(test_archcheck_parsers PRIVATE archcheck_core openrender_common_flags)
add_test(NAME Architecture_Parsers COMMAND test_archcheck_parsers)
set_tests_properties(Architecture_Parsers PROPERTIES
    TIMEOUT 60 LABELS "architecture;unit")

# The check itself, on this build tree (spec 022 T042). It needs the whole
# tree built: an in-scope object or depfile that is missing is an
# environment error (exit 2), never a silently smaller report.
add_test(NAME Architecture_DomainBoundaries
    COMMAND archcheck --source-dir ${CMAKE_SOURCE_DIR} --build-dir ${CMAKE_BINARY_DIR}
            --config ${CMAKE_SOURCE_DIR}/tests/architecture)
set_tests_properties(Architecture_DomainBoundaries PROPERTIES
    TIMEOUT 120 LABELS "architecture")

# The report must be byte-identical from run to run (SC-007).
add_test(NAME Architecture_Determinism
    COMMAND ${CMAKE_COMMAND}
        -DARCHCHECK=$<TARGET_FILE:archcheck>
        "-DARCHCHECK_ARGS=--source-dir;${CMAKE_SOURCE_DIR};--build-dir;${CMAKE_BINARY_DIR};--config;${CMAKE_SOURCE_DIR}/tests/architecture"
        -DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_determinism.cmake)
set_tests_properties(Architecture_Determinism PROPERTIES
    TIMEOUT 120 LABELS "architecture")

# archcheck_add_fixture(<name> <expected-exit> <pass-regex>
#                       [FAIL_REGEX <regex>] [BUILD_DIR <dir>]
#                       [EXCLUDE <relpath>...] [ARGS <arg>...])
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
#   FAIL_REGEX output that must NOT appear
#   BUILD_DIR  overrides --build-dir (e.g. an empty directory, for the
#              incomplete-build scenario)
#   EXCLUDE    fixture sources (relative to the fixture's src/) left out of
#              the OBJECT library, for the not-built scenario
#   ARGS       extra archcheck arguments (e.g. --platform linux)
function(archcheck_add_fixture name expected_exit pass_regex)
    cmake_parse_arguments(PARSE_ARGV 3 FX "" "BUILD_DIR;FAIL_REGEX" "EXCLUDE;ARGS")

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
            -DFAIL_REGEX=${FX_FAIL_REGEX}
            "-DARCHCHECK_ARGS=--source-dir;${fixture_dir};--build-dir;${build_dir};--config;${fixture_dir};--registry;${fixture_dir}/REGISTRY.md;${FX_ARGS}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_fixture.cmake)
    set_tests_properties(ArchFixture_${id} PROPERTIES
        TIMEOUT 60 LABELS "architecture")
endfunction()

# An existing directory that is not a build tree (incomplete-build fixture).
set(ARCHCHECK_EMPTY_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/empty-build-dir)
file(MAKE_DIRECTORY ${ARCHCHECK_EMPTY_BUILD_DIR})

# --- User Story 1: new boundary violations are caught (spec 022 T017-T023) ---
archcheck_add_fixture(clean 0 "RESULT +PASS"
    FAIL_REGEX "VIOLATION|CYCLE")
archcheck_add_fixture(forbidden-include 1
    "VIOLATION include src/state/options.cpp -> src/shading/shader.h +\\[GraphicsState -> Shading\\] rule:")
archcheck_add_fixture(forbidden-link 1
    "VIOLATION link +src/texture/lookup.cpp -> _Z8traceRayi \\(traceRay\\(int\\)\\) +\\[Texturing -> Hiders\\] defined in: src/hiders/raytracer.cpp"
    FAIL_REGEX "VIOLATION include")
archcheck_add_fixture(new-cycle 1
    "CYCLE +[A-Za-z >-]*Geometry[A-Za-z >-]*Shading[^[]*\\[FAILING\\]"
    FAIL_REGEX "\\[allowlisted\\]")
archcheck_add_fixture(new-cycle-allowlisted 0
    "CYCLE +[A-Za-z >-]*Geometry[A-Za-z >-]*Shading[^[]*\\[allowlisted\\]"
    FAIL_REGEX "VIOLATION|FAILING")
archcheck_add_fixture(unclassified 2 "unclassified: src/misc/stray.cpp"
    FAIL_REGEX "notes\\.txt")
archcheck_add_fixture(forwarding-shim 1
    "VIOLATION include src/state/options.cpp -> src/common/shader.h +\\[GraphicsState -> Shading\\]")
archcheck_add_fixture(incomplete-build 2 "CMakeCache\\.txt"
    BUILD_DIR ${ARCHCHECK_EMPTY_BUILD_DIR})

# --- User Story 2: allowlist and registry integrity (spec 022 T030-T034) -----
archcheck_add_fixture(stale-entry 1
    "STALE +DB-001 include src/state/options.cpp -> src/shading/shader.h +\\(edge no longer present")
archcheck_add_fixture(stale-now-allowed 1
    "STALE +DB-001 include src/hiders/reyes.cpp -> src/geometry/object.h +\\(edge now allowed")
archcheck_add_fixture(unknown-id 1
    "BADID +DB-099 include src/state/options.cpp -> src/shading/shader.h +\\(no Open registry entry DB-099\\)"
    FAIL_REGEX "VIOLATION")
archcheck_add_fixture(resolved-id 1
    "BADID +DB-002 include src/state/options.cpp -> src/shading/shader.h +\\(registry entry DB-002 is Resolved\\)"
    FAIL_REGEX "VIOLATION")
archcheck_add_fixture(other-platform-tag 1
    "STALE +DB-001 include src/state/options.cpp -> src/texture/gone\\.h"
    FAIL_REGEX "missing\\.h"
    ARGS --platform linux)
archcheck_add_fixture(not-built 0
    "NOTBUILT +DB-001 include src/state/legacy\\.cpp -> src/shading/shader\\.h.*NOTBUILT +DB-001 link src/state/legacy\\.cpp -> _Z8traceRayi.*RESULT +PASS"
    FAIL_REGEX "STALE|VIOLATION"
    EXCLUDE state/legacy.cpp)
archcheck_add_fixture(tagged-untagged-dup 2
    "allowlist\\.txt:3: same edge listed tagged and untagged")

# --- User Story 3: renderer-core usage metric (spec 022 T046-T047) ----------
archcheck_add_fixture(metric-increase 0
    "WARNING +metric Geometry refs 3 > baseline 2 \\(\\+1\\).*WARNING +metric Geometry distinct 2 > baseline 1 \\(\\+1\\).*RESULT +PASS")
archcheck_add_fixture(metric-decrease 0
    "NOTE +metric Geometry refs 3 < baseline 5 \\(-2\\).*NOTE +metric Geometry distinct 2 < baseline 3 \\(-1\\).*RESULT +PASS"
    FAIL_REGEX "WARNING")
