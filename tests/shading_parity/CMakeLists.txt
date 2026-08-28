# Test suite for JIT/interpreter shading-parity fixes (spec 014-jit-shading-parity)
#
# Executables in this directory link ri + libshader_shading + openrendercommon
# (the "tier-2" full-stack pattern, mirroring tests/imager/CMakeLists.txt) so
# they can load real .rslo/.slo shaders through CRenderer and compare backend
# behavior end-to-end. Every add_test() here MUST carry the "shading_parity"
# ctest label so `ctest -L shading_parity` selects the whole suite.
#
# add_executable() blocks are added per-user-story below as their oracle
# tests are written (T010, T019, T024) — intentionally empty at Setup time.

# Differential oracle: usedParameters bit-for-bit identical between a .slo
# and a .rslo load of the same never-writes-Ci/Oi fixture (T010, US1).
# Shells out to the real `oshader` CLI to produce both bitcode files, so it
# only needs to link the runtime-loading side (ri + libshader_shading), not
# libshader_compiler -- avoids linking compiler+shading together (research.md
# risk noted in plan.md Phase 4).
add_executable(test_shading_parity_used_parameters_oracle test_used_parameters_oracle.cpp)
target_link_libraries(test_shading_parity_used_parameters_oracle PRIVATE openrender_common_flags ri libshader_shading openrendercommon)
target_include_directories(test_shading_parity_used_parameters_oracle PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_parity_used_parameters_oracle PRIVATE cxx_std_20)
add_test(NAME ShadingParity_UsedParametersOracle COMMAND test_shading_parity_used_parameters_oracle)
set_tests_properties(ShadingParity_UsedParametersOracle PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};OSHADER_BIN=$<TARGET_FILE:oshader>"
)

# Ambient-Cl accumulation regressions (T019/T020, US3): crash-safety when
# alights is nullptr (RSLShading::shade() entry point) and the
# prepareAmbient() double-count fix. Interpreter (.rslo) backend only.
add_executable(test_shading_parity_ambient_accumulation test_ambient_accumulation.cpp)
target_link_libraries(test_shading_parity_ambient_accumulation PRIVATE openrender_common_flags ri libshader_shading openrendercommon)
target_include_directories(test_shading_parity_ambient_accumulation PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_parity_ambient_accumulation PRIVATE cxx_std_20)
add_test(NAME ShadingParity_AmbientAccumulation COMMAND test_shading_parity_ambient_accumulation)
set_tests_properties(ShadingParity_AmbientAccumulation PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};OSHADER_BIN=$<TARGET_FILE:oshader>"
)

# Ol wiring (T024, US4): PARAMETER_OL usedParameters bit identical between
# .slo and .rslo loads of a light shader that assigns Ol. research.md D7b:
# Ol has no runtime consumer in either backend, so this is the one
# Ol-related property that is actually backend-comparable.
add_executable(test_shading_parity_ol_wiring test_ol_wiring.cpp)
target_link_libraries(test_shading_parity_ol_wiring PRIVATE openrender_common_flags ri libshader_shading openrendercommon)
target_include_directories(test_shading_parity_ol_wiring PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_parity_ol_wiring PRIVATE cxx_std_20)
add_test(NAME ShadingParity_OlWiring COMMAND test_shading_parity_ol_wiring)
set_tests_properties(ShadingParity_OlWiring PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};OSHADER_BIN=$<TARGET_FILE:oshader>"
)
