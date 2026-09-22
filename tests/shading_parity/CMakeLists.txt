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
target_link_libraries(test_shading_parity_used_parameters_oracle PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
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
target_link_libraries(test_shading_parity_ambient_accumulation PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
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
target_link_libraries(test_shading_parity_ol_wiring PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
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

# GitHub #11: printf() was a silent no-op under the LLVM JIT. Renders a
# real fixture through the actual orender CLI on both backends (not just
# compiling it) -- the operand off-by-one and \n-escape bugs found while
# fixing this only show up once a shader with a real per-vertex value
# argument actually gets shaded, not in a compile-only or isolated
# op_printf() unit test.
add_executable(test_shading_parity_printf_jit test_printf_jit.cpp)
target_link_libraries(test_shading_parity_printf_jit PRIVATE openrender_common_flags)
target_compile_features(test_shading_parity_printf_jit PRIVATE cxx_std_20)
add_test(NAME ShadingParity_PrintfJit COMMAND test_shading_parity_printf_jit)
set_tests_properties(ShadingParity_PrintfJit PROPERTIES
    TIMEOUT 60
    LABELS "shading_parity"
    ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>;ORENDER_BIN=$<TARGET_FILE:orender>;TEST_DISPLAYS_DIR=$<TARGET_FILE_DIR:file>"
)

# GitHub #8 (root cause) / #6 (a symptom of #8, not a separate bug):
# collapseArgs's uniform fast path discarded the real per-vertex tags
# array for any uniform-classified instruction, so a uniform assignment
# inside an if/else (or the equivalent ternary) body ran unconditionally
# regardless of the branch. Renders a real fixture through the actual
# orender CLI on both backends -- this is a control-flow codegen bug, not
# visible from a compile-only test.
add_executable(test_shading_parity_uniform_conditional_jit test_uniform_conditional_jit.cpp)
target_link_libraries(test_shading_parity_uniform_conditional_jit PRIVATE openrender_common_flags)
target_compile_features(test_shading_parity_uniform_conditional_jit PRIVATE cxx_std_20)
add_test(NAME ShadingParity_UniformConditionalJit COMMAND test_shading_parity_uniform_conditional_jit)
set_tests_properties(ShadingParity_UniformConditionalJit PROPERTIES
    TIMEOUT 60
    LABELS "shading_parity"
    ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>;ORENDER_BIN=$<TARGET_FILE:orender>;TEST_DISPLAYS_DIR=$<TARGET_FILE_DIR:file>"
)

# GitHub #10: transform()/vtransform()/ntransform()'s matrix-argument
# ("p=mp"), two-space ("p=SSp") and space+matrix ("p=Smp") overloads were
# all dispatched under --jit as if they were the single-space ("p=Sp")
# overload, unconditionally reading operand 0 as a space-name string.
# Renders a real fixture through the actual orender CLI on both backends.
add_executable(test_shading_parity_matrix_transform_jit test_matrix_transform_jit.cpp)
target_link_libraries(test_shading_parity_matrix_transform_jit PRIVATE openrender_common_flags)
target_compile_features(test_shading_parity_matrix_transform_jit PRIVATE cxx_std_20)
add_test(NAME ShadingParity_MatrixTransformJit COMMAND test_shading_parity_matrix_transform_jit)
set_tests_properties(ShadingParity_MatrixTransformJit PROPERTIES
    TIMEOUT 60
    LABELS "shading_parity"
    ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>;ORENDER_BIN=$<TARGET_FILE:orender>;TEST_DISPLAYS_DIR=$<TARGET_FILE_DIR:file>"
)

# GitHub #4: Attribute "irradiance" "handle"/"filemode" [""] must not
# collapse to NULL in RiAttributeV, and CRenderer::getCache() must not
# crash on a NULL name/mode even if some other caller does pass one.
add_executable(test_shading_parity_irradiance_empty_string test_irradiance_empty_string.cpp)
target_link_libraries(test_shading_parity_irradiance_empty_string PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_shading_parity_irradiance_empty_string PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_parity_irradiance_empty_string PRIVATE cxx_std_20)
add_test(NAME ShadingParity_IrradianceEmptyString COMMAND test_shading_parity_irradiance_empty_string)
set_tests_properties(ShadingParity_IrradianceEmptyString PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
)
