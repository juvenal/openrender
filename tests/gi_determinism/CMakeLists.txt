# Spec 040 (GI determinism): the tests that define "done" for Phases 2 to 7. Written red-first (Phase 1); each moves out of the red block below in
# the phase that makes it green (tasks.md T040, T050, T063, T077, T085 ...).
#
# Labels: gi_determinism (every test; the short forms) and gi_determinism_long (the 30-run forms: they carry only that label and are excluded from the
# everyday run the way the sweep is: ctest -LE 'determinism-sweep|gi_determinism_long'). -L is a regular expression: use -L '^gi_determinism$' and
# -L '^gi_determinism_long$'.
#
#   test_gi_photon_store   GiDet_PhotonFileBytes, _PhotonLookupPure, _PhotonInMemoryLookup, _PhotonRoundTrip   (the photon store alone)
#   test_gi_photon_claim   GiDet_PhotonClaim                      (the same source, compiled with GIDET_CLAIM: storePhotonForFrame does not exist yet)
#   test_gi_photon_runs    GiDet_PhotonRepeat, _PhotonRepeatReuse, _PhotonThreads, and the 30-run forms _PhotonRepeat30, _PhotonRepeatReuse30,
#                          _PhotonThreads4x30                     (separate orender processes, the .gpm compared byte for byte)
#   test_gi_cache_store    GiDet_LookupPure, _CommitOrder, _CommitDedupe, _CacheRoundTrip, _BatchClamp   (the irradiance store alone; red block, GIDET_STORE_PHASE5)
#   test_gi_accept         GiDet_AcceptPure   (the same source without the macro: the accept rule alone, Phase 4)
#   test_gi_miss           GiDet_MissIndependence_{rslo,slo}, _IndirectRayCount_{rslo,slo}, _RepeatedPoint_{rslo,slo}  (what a miss makes)
#   gi_quality             the image-quality tool of research.md R4 (a normal target: it does not depend on the code under test)
#
# The red tests are defined only when -DOPENRENDER_040_RED_TESTS=ON (default OFF); their targets are EXCLUDE_FROM_ALL, so the everyday build does not
# compile them before their phase. A test that does not compile yet is red by build failure naming the missing member.

option(OPENRENDER_040_RED_TESTS "Define the spec 040 red-first tests (default OFF: the targets are EXCLUDE_FROM_ALL and do not build before their phase)" OFF)

# The form of the record hook the tests use: "insertRecord" on the starting tree, "addPending" once Phase 5 has replaced it (tests/gi_determinism/gi_recording.h)
set(GIDET_RECORD_HOOK "insertRecord" CACHE STRING "The record hook of CIrradianceCache the recording tests override: insertRecord or addPending")
set_property(CACHE GIDET_RECORD_HOOK PROPERTY STRINGS insertRecord addPending)

if(APPLE)
    set(GI_DET_PLATFORM macos)
else()
    set(GI_DET_PLATFORM linux)
endif()

# The environment the visual tests run orender in (as tests/gi_split/CMakeLists.txt)
set(GI_DET_DISPLAY_PATH "$<TARGET_FILE_DIR:file>:$<TARGET_FILE_DIR:rgbe>:$<TARGET_FILE_DIR:framebuffer>")
if(HAVE_OPENEXR)
    set(GI_DET_DISPLAY_PATH "${GI_DET_DISPLAY_PATH}:$<TARGET_FILE_DIR:openexr>")
endif()
set(GI_DET_ENV
    "SHADERS=${CMAKE_BINARY_DIR}/shaders"
    "ORENDERHOME=${CMAKE_BINARY_DIR}/tests/visual/_home"
    "DISPLAYS=${GI_DET_DISPLAY_PATH}"
    "GEOMETRIES=${CMAKE_SOURCE_DIR}/geometry"
    "TEXTURES=${CMAKE_SOURCE_DIR}/examples/rib/tests")
# Heap garbage that is not zero, so that an unassigned byte of a photon map shows (FR-004)
set(GI_DET_SCRIBBLE_ENV ${GI_DET_ENV} "MallocPreScribble=1" "MALLOC_PERTURB_=165")

set(GI_DET_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR})
set(GI_DET_TOOLS ${CMAKE_SOURCE_DIR}/specs/040-gi-determinism/tools)
set(GI_DET_SCENES ${CMAKE_CURRENT_SOURCE_DIR}/scenes)

# One scratch directory per test: each test runs in its own, so a parallel run never shares a file
set(GI_DET_TEST_NAMES
    PhotonClaim PhotonFileBytes PhotonLookupPure PhotonInMemoryLookup PhotonRoundTrip
    PhotonRepeat PhotonRepeatReuse PhotonThreads PhotonRepeat30 PhotonRepeatReuse30 PhotonThreads4x30
    LookupPure CommitOrder CommitDedupe CacheRoundTrip AcceptPure BatchClamp
    MissIndependence_rslo MissIndependence_slo IndirectRayCount_rslo IndirectRayCount_slo RepeatedPoint_rslo RepeatedPoint_slo)
foreach(_n ${GI_DET_TEST_NAMES})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/work/${_n})
endforeach()

# ---------------------------------------------------------------------------------------------------------
# The image-quality tool (T029): libtiff only, a normal target
# ---------------------------------------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/gi_quality.cpp)
    add_executable(gi_quality tools/gi_quality.cpp)
    target_compile_features(gi_quality PRIVATE cxx_std_20)
    target_link_libraries(gi_quality PRIVATE TIFF::TIFF)
endif()

# ---------------------------------------------------------------------------------------------------------
# Phase 2 (spec 040): the photon map's tests, defined unconditionally (the targets are part of `all`)
# ---------------------------------------------------------------------------------------------------------
macro(gi_det_executable_always _target _source)
    add_executable(${_target} ${_source})
    target_compile_features(${_target} PRIVATE cxx_std_20)
    target_include_directories(${_target} PRIVATE ${GI_DET_INCLUDES})
    target_link_libraries(${_target} PRIVATE ribRender libshader_shading openrendercommon Threads::Threads)
endmacro()
gi_det_executable_always(test_gi_photon_store test_gi_photon_store.cpp)
gi_det_executable_always(test_gi_photon_claim test_gi_photon_store.cpp)
target_compile_definitions(test_gi_photon_claim PRIVATE GIDET_CLAIM)
gi_det_executable_always(test_gi_photon_runs test_gi_photon_runs.cpp)

add_test(NAME GiDet_PhotonFileBytes COMMAND test_gi_photon_store filebytes ${CMAKE_CURRENT_BINARY_DIR}/work/PhotonFileBytes)
set_tests_properties(GiDet_PhotonFileBytes PROPERTIES TIMEOUT 120 LABELS "gi_determinism;photon" ENVIRONMENT "${GI_DET_SCRIBBLE_ENV}")
foreach(_m lookup-pure:PhotonLookupPure inmemory-lookup:PhotonInMemoryLookup roundtrip:PhotonRoundTrip)
    string(REPLACE ":" ";" _mt ${_m})
    list(GET _mt 0 _mode)
    list(GET _mt 1 _name)
    add_test(NAME GiDet_${_name} COMMAND test_gi_photon_store ${_mode} ${CMAKE_CURRENT_BINARY_DIR}/work/${_name})
    set_tests_properties(GiDet_${_name} PROPERTIES TIMEOUT 120 LABELS "gi_determinism;photon" ENVIRONMENT "${GI_DET_SCRIBBLE_ENV}")
endforeach()

add_test(NAME GiDet_PhotonClaim COMMAND test_gi_photon_claim claim ${CMAKE_CURRENT_BINARY_DIR}/work/PhotonClaim)
set_tests_properties(GiDet_PhotonClaim PROPERTIES TIMEOUT 120 LABELS "gi_determinism;photon" ENVIRONMENT "${GI_DET_ENV}")

macro(gi_det_photon_runs _name _timeout _label)
    add_test(NAME GiDet_${_name} COMMAND test_gi_photon_runs ${ARGN} $<TARGET_FILE:orender> ${GI_DET_TOOLS} ${CMAKE_CURRENT_BINARY_DIR}/work/${_name})
    set_tests_properties(GiDet_${_name} PROPERTIES TIMEOUT ${_timeout} LABELS "${_label};photon" ENVIRONMENT "${GI_DET_ENV}" DEPENDS orender)
endmacro()
gi_det_photon_runs(PhotonRepeat 300 gi_determinism repeat fresh 5)
gi_det_photon_runs(PhotonRepeatReuse 300 gi_determinism repeat reuse 5)
gi_det_photon_runs(PhotonThreads 300 gi_determinism threads)
gi_det_photon_runs(PhotonRepeat30 900 gi_determinism_long repeat fresh 30)
gi_det_photon_runs(PhotonRepeatReuse30 900 gi_determinism_long repeat reuse 30)
gi_det_photon_runs(PhotonThreads4x30 900 gi_determinism_long threads4 30)

# ---------------------------------------------------------------------------------------------------------
# Phase 4 (spec 040): the accept rule and what a miss makes, defined unconditionally
# ---------------------------------------------------------------------------------------------------------
# test_gi_cache_store.cpp without GIDET_STORE_PHASE5 is the accept-pure mode alone (the red block builds the whole file with the macro)
gi_det_executable_always(test_gi_accept test_gi_cache_store.cpp)
add_test(NAME GiDet_AcceptPure COMMAND test_gi_accept accept-pure ${CMAKE_CURRENT_BINARY_DIR}/work/AcceptPure)
set_tests_properties(GiDet_AcceptPure PROPERTIES TIMEOUT 300 LABELS "gi_determinism;irradiance" ENVIRONMENT "${GI_DET_ENV}")

# test_gi_miss: MissIndependence and IndirectRayCount_slo are green from Phase 4; IndirectRayCount_rslo (GitHub #173) and RepeatedPoint stay in the red block (it needs the commit of Phase 5)
gi_det_executable_always(test_gi_miss test_gi_miss.cpp)
foreach(_variant rslo slo)
    if(_variant STREQUAL "slo")
        set(_suffix "-slo")
    else()
        set(_suffix "")
    endif()
    foreach(_m miss:MissIndependence:miss-probe raycount:IndirectRayCount:raycount)
        # IndirectRayCount_rslo is red until GitHub #173 (the interpreter hands the sampler a denormal `samples`) is fixed: it is in the red block below
        if(_m MATCHES "^raycount" AND _variant STREQUAL "rslo")
            continue()
        endif()
        string(REPLACE ":" ";" _mt ${_m})
        list(GET _mt 0 _mode)
        list(GET _mt 1 _name)
        list(GET _mt 2 _scene)
        add_test(NAME GiDet_${_name}_${_variant} COMMAND test_gi_miss ${_mode} ${GI_DET_SCENES}/${_scene}${_suffix}.rib $<TARGET_FILE:orender> ${CMAKE_CURRENT_BINARY_DIR}/work/${_name}_${_variant})
        set_tests_properties(GiDet_${_name}_${_variant} PROPERTIES TIMEOUT 300 LABELS "gi_determinism;irradiance" ENVIRONMENT "${GI_DET_ENV}" DEPENDS orender)
    endforeach()
endforeach()

# ---------------------------------------------------------------------------------------------------------
# The red block: spec 040's red-first tests. Each test below leaves this block (it is then defined unconditionally) in the phase that makes it green.
# ---------------------------------------------------------------------------------------------------------
if(OPENRENDER_040_RED_TESTS)
    macro(gi_det_executable _target _source)
        add_executable(${_target} EXCLUDE_FROM_ALL ${_source})
        target_compile_features(${_target} PRIVATE cxx_std_20)
        target_include_directories(${_target} PRIVATE ${GI_DET_INCLUDES})
        target_link_libraries(${_target} PRIVATE ribRender libshader_shading openrendercommon Threads::Threads)
        if(GIDET_RECORD_HOOK STREQUAL "addPending")
            target_compile_definitions(${_target} PRIVATE GIDET_RECORD_HOOK_ADDPENDING)
        endif()
    endmacro()

    # --- US2/US3: the irradiance store ------------------------------------------------------------
    gi_det_executable(test_gi_cache_store test_gi_cache_store.cpp)
    target_compile_definitions(test_gi_cache_store PRIVATE GIDET_STORE_PHASE5)
    foreach(_m lookup-pure:LookupPure commit-order:CommitOrder commit-dedupe:CommitDedupe batch-clamp:BatchClamp)
        string(REPLACE ":" ";" _mt ${_m})
        list(GET _mt 0 _mode)
        list(GET _mt 1 _name)
        add_test(NAME GiDet_${_name} COMMAND test_gi_cache_store ${_mode} ${CMAKE_CURRENT_BINARY_DIR}/work/${_name})
        set_tests_properties(GiDet_${_name} PROPERTIES TIMEOUT 300 LABELS "gi_determinism;irradiance" ENVIRONMENT "${GI_DET_ENV}")
    endforeach()
    # the round trip also loads the cache file the starting tree wrote (fixture F1 of spec 032)
    add_test(NAME GiDet_CacheRoundTrip COMMAND test_gi_cache_store roundtrip ${CMAKE_CURRENT_BINARY_DIR}/work/CacheRoundTrip ${CMAKE_BINARY_DIR}/tests/gi_split/fixture/icache-fixture.icache)
    set_tests_properties(GiDet_CacheRoundTrip PROPERTIES TIMEOUT 300 LABELS "gi_determinism;irradiance" ENVIRONMENT "${GI_DET_ENV}" FIXTURES_REQUIRED GiFixture_ICache)

    # --- US2: what a miss makes, the part that needs the commit (the executable is built from Phase 4) ---------
    foreach(_variant rslo slo)
        if(_variant STREQUAL "slo")
            set(_suffix "-slo")
        else()
            set(_suffix "")
        endif()
        foreach(_m repeat:RepeatedPoint:repeat-point raycount:IndirectRayCount:raycount)
            if(_m MATCHES "^raycount" AND _variant STREQUAL "slo")
                continue()
            endif()
            string(REPLACE ":" ";" _mt ${_m})
            list(GET _mt 0 _mode)
            list(GET _mt 1 _name)
            list(GET _mt 2 _scene)
            add_test(NAME GiDet_${_name}_${_variant} COMMAND test_gi_miss ${_mode} ${GI_DET_SCENES}/${_scene}${_suffix}.rib $<TARGET_FILE:orender> ${CMAKE_CURRENT_BINARY_DIR}/work/${_name}_${_variant})
            set_tests_properties(GiDet_${_name}_${_variant} PROPERTIES TIMEOUT 300 LABELS "gi_determinism;irradiance" ENVIRONMENT "${GI_DET_ENV}" DEPENDS orender)
        endforeach()
    endforeach()
endif()

message(STATUS "  - spec 040 gi_determinism tests (red block ${OPENRENDER_040_RED_TESTS}), record hook ${GIDET_RECORD_HOOK}, platform ${GI_DET_PLATFORM}")
