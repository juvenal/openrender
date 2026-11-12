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
    "VIOLATION include src/state/options.cpp:1 -> src/shading/shader.h +\\[GraphicsState -> Shading\\] rule:")
archcheck_add_fixture(forbidden-link 1
    "VIOLATION link +src/texture/lookup.cpp -> _Z8traceRayi \\(traceRay\\(int\\)\\) +\\[Texturing -> Hiders\\] rule: allowed-dependency table \\(Texturing may depend on: Infra\\) +defined in: src/hiders/raytracer.cpp"
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
    "VIOLATION include src/state/options.cpp:1 -> src/common/shader.h +\\[GraphicsState -> Shading\\]")
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

# --- Spec 030 (hider and integrator registry): the Transport domain (FR-030, US6 AS1) -----------------------
# Added after the Principle VIII amendment (constitution v1.4.0) and before the Transport class of archcheck: until
# the class exists, archcheck exits 2 on the Transport rows of these fixtures' rule tables.
archcheck_add_fixture(transport-allowed 0 "RESULT +PASS"
    FAIL_REGEX "VIOLATION|CYCLE")
archcheck_add_fixture(transport-cycle 1
    "VIOLATION include src/transport/classic.cpp:1 -> src/hiders/hider.h +\\[Transport -> Hiders\\].*CYCLE +[A-Za-z >-]*(Hiders[A-Za-z >-]*Transport|Transport[A-Za-z >-]*Hiders)[^[]*\\[FAILING\\]")

# --- User Story 3: renderer-core usage metric (spec 022 T046-T047) ----------
archcheck_add_fixture(metric-increase 0
    "WARNING +metric Geometry refs 3 > baseline 2 \\(\\+1\\).*WARNING +metric Geometry distinct 2 > baseline 1 \\(\\+1\\).*RESULT +PASS")
archcheck_add_fixture(metric-decrease 0
    "NOTE +metric Geometry refs 3 < baseline 5 \\(-2\\).*NOTE +metric Geometry distinct 2 < baseline 3 \\(-1\\).*RESULT +PASS"
    FAIL_REGEX "WARNING")

# --- Convergence (spec 022 T067): build options that remove code ------------
archcheck_add_fixture(conditional-include 0
    "NOTBUILT +DB-001 include src/state/options.cpp -> src/shading/shader.h +\\(directive inactive in this build; skipped\\).*RESULT +PASS"
    FAIL_REGEX "STALE")

# --- Spec 038 (Scene Interface untangle): the mechanical checks -------------------------------
#
# Each is a `cmake -P` script that reads the build system's own view of a target (a generator
# expression on the command line), never a hand-kept list. While spec 038 was in progress each one
# was red on the starting tree by design and carried a temporary label of its own instead of
# `architecture`, so that `ctest -L architecture` stayed green at every checkpoint; the phase that
# turned a check green moved it to `architecture`: Phase 2 for SceneInterface_RibOutNoCore,
# SceneInterface_NoCoreInclude and SceneInterface_HooksAndRecords, Phase 3 for
# SceneInterface_ActiveContextAssignments, Phase 4 for SceneInterface_RibVectorResidual and
# Preview_NoCoreReach (tests/preview), Phase 5 for Compiler_IncludeBoundary and Phase 7 for
# Precomp_NoHiderCore (tests/precomp). All of them carry `architecture` now; the temporary label is gone.
set(OR038_MAP ${CMAKE_CURRENT_SOURCE_DIR}/domain-map.txt)
set(OR038_COMMON
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DBUILD_DIR=${CMAKE_BINARY_DIR}
    -DMAP=${OR038_MAP})

add_test(NAME SceneInterface_RibOutNoCore
    COMMAND ${CMAKE_COMMAND} ${OR038_COMMON}
        "-DSOURCES=$<TARGET_PROPERTY:ribOut_obj,SOURCES>"
        -DTARGET_DIR=${CMAKE_SOURCE_DIR}/src/ri
        -DTARGET_BINARY_DIR=${CMAKE_BINARY_DIR}/src/ri
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ribout_no_core.cmake)

add_test(NAME SceneInterface_RibVectorResidual
    COMMAND ${CMAKE_COMMAND} ${OR038_COMMON}
        "-DSOURCES=$<TARGET_PROPERTY:ribVector_obj,SOURCES>"
        -DTARGET_DIR=${CMAKE_SOURCE_DIR}/src/ri
        -DTARGET_BINARY_DIR=${CMAKE_BINARY_DIR}/src/ri
        -DRESIDUAL=${CMAKE_CURRENT_SOURCE_DIR}/ribvector-core-residual.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ribvector_residual.cmake)

add_test(NAME SceneInterface_NoCoreInclude
    COMMAND ${CMAKE_COMMAND} ${OR038_COMMON}
        "-DINCLUDE_DIRS=$<TARGET_PROPERTY:ribOut_obj,INCLUDE_DIRECTORIES>"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_core_include.cmake)

add_test(NAME SceneInterface_HooksAndRecords
    COMMAND ${CMAKE_COMMAND} ${OR038_COMMON}
        "-DINCLUDE_DIRS=$<TARGET_PROPERTY:ribOut_obj,INCLUDE_DIRECTORIES>"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_hooks_and_records.cmake)

add_test(NAME SceneInterface_ActiveContextAssignments
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_active_context_assignments.cmake)

# The compiler's include path: its own property plus what its two linked libraries export (usage
# requirements the compile line carries but the target's INCLUDE_DIRECTORIES property does not).
add_test(NAME Compiler_IncludeBoundary
    COMMAND ${CMAKE_COMMAND} ${OR038_COMMON}
        "-DINCLUDE_DIRS=$<TARGET_PROPERTY:libshader_compiler_obj,INCLUDE_DIRECTORIES>;$<TARGET_PROPERTY:openrendercommon,INTERFACE_INCLUDE_DIRECTORIES>;$<TARGET_PROPERTY:openrendershadeop,INTERFACE_INCLUDE_DIRECTORIES>"
        -DCOMPILER_DIR=${CMAKE_SOURCE_DIR}/src/libshader/compiler
        -DCOMPILER_BINARY_DIR=${CMAKE_BINARY_DIR}/src/libshader/compiler
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_compiler_include_boundary.cmake)

# Spec 032 (GI cache split), T-G: the text-level boundary of the split. It runs the sub-checks listed in the cache variable
# GI032_BOUNDARY_CHECKS (tests/gi_split/CMakeLists.txt, default empty), which each phase extends in its test gate, so it is
# green at every checkpoint.
add_test(NAME GiSplit_Boundary
    COMMAND ${CMAKE_COMMAND} ${OR038_COMMON}
        "-DINCLUDE_DIRS=$<TARGET_PROPERTY:ribRender_obj,INCLUDE_DIRECTORIES>"
        "-DCHECKS=${GI032_BOUNDARY_CHECKS}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_gi_split_boundary.cmake)
set_tests_properties(GiSplit_Boundary PROPERTIES TIMEOUT 120 LABELS "architecture")

# Green since Phase 2 / 3 / 4 / 5 (spec 038): permanent architecture checks
foreach(_t SceneInterface_RibOutNoCore SceneInterface_NoCoreInclude SceneInterface_HooksAndRecords SceneInterface_ActiveContextAssignments SceneInterface_RibVectorResidual Compiler_IncludeBoundary)
    set_tests_properties(${_t} PROPERTIES TIMEOUT 60 LABELS "architecture")
endforeach()

# --- Spec 030 (hider and integrator registry): two mechanical checks ----------------------------------------
#
# Each is a `cmake -P` script over the tracked sources. They are red on the starting tree by design (the references
# and the strcmp chains exist), so they are registered DISABLED while OPENRENDER_030_RED_TESTS is OFF (the red-evidence
# configure turns the option ON and runs them); the phase that turns one green enabled it: Phase 4 for
# Hcshader_NoReferences, Phase 8 for Registry_CoreNamesNoHider.
set(OR030_HCSHADER_EXEMPT_FILES
    "tests/architecture/unit/test_archcheck_parsers.cpp|tests/visual/CMakeLists.txt|examples/rib/spherelight.rib|examples/rib/quadlight.rib|tests/architecture/check_hcshader_references.cmake|tests/unit/registry/test_hcshader_missing_shader.cpp")
set(OR030_HCSHADER_REGISTRATION_FILES "tests/architecture/CMakeLists.txt|tests/unit/registry/CMakeLists.txt")

add_test(NAME Hcshader_NoReferences
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        "-DEXEMPT_FILES=${OR030_HCSHADER_EXEMPT_FILES}" "-DREGISTRATION_FILES=${OR030_HCSHADER_REGISTRATION_FILES}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_hcshader_references.cmake)
set_tests_properties(Hcshader_NoReferences PROPERTIES TIMEOUT 120 LABELS "architecture")

add_test(NAME Registry_CoreNamesNoHider
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/check_core_names_no_hider.cmake)
set_tests_properties(Registry_CoreNamesNoHider PROPERTIES TIMEOUT 60 LABELS "architecture")

