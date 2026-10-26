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

# GitHub #55: an optional parameter list's diagnostics ("Unknown parameter",
# the uniform-channel warning) are printed once per call site by both
# backends; the JIT used to drop the list unread. Renders a real fixture
# through the actual orender CLI on both backends. The image parity of the
# lists themselves is tested in tests/visual (ShadingParity_JitPL*).
add_executable(test_shading_parity_jit_paramlist_diagnostics test_jit_paramlist_diagnostics.cpp)
target_link_libraries(test_shading_parity_jit_paramlist_diagnostics PRIVATE openrender_common_flags)
target_compile_features(test_shading_parity_jit_paramlist_diagnostics PRIVATE cxx_std_20)
add_test(NAME ShadingParity_JitPLDiagnostics COMMAND test_shading_parity_jit_paramlist_diagnostics)
set_tests_properties(ShadingParity_JitPLDiagnostics PROPERTIES
    TIMEOUT 60
    LABELS "shading_parity"
    ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>;ORENDER_BIN=$<TARGET_FILE:orender>;TEST_DISPLAYS_DIR=$<TARGET_FILE_DIR:file>"
)

# The 6 tests above (UsedParametersOracle, OlWiring, PrintfJit,
# UniformConditionalJit, MatrixTransformJit, JitPLDiagnostics) all compare/exercise an actual
# .slo (LLVM JIT bitcode) load or JIT-compiled render -- there is no
# meaningful "JIT vs. interpreter parity" to check without a JIT. Registered
# either way, so the suite's total never quietly shrinks, but marked
# DISABLED when there is no JIT behind them (mirrors
# tests/visual/CMakeLists.txt's identical treatment of its own .slo scenes)
# rather than failing outright on an oshader --jit invocation that can't
# possibly succeed in this build.
if(NOT OPENRENDER_HAVE_LLVM)
    set_tests_properties(
        ShadingParity_UsedParametersOracle
        ShadingParity_OlWiring
        ShadingParity_PrintfJit
        ShadingParity_UniformConditionalJit
        ShadingParity_MatrixTransformJit
        ShadingParity_JitPLDiagnostics
        PROPERTIES DISABLED TRUE)
    message(STATUS "  - ShadingParity_{UsedParametersOracle,OlWiring,PrintfJit,UniformConditionalJit,MatrixTransformJit,JitPLDiagnostics}: DISABLED (no LLVM JIT in this build)")
endif()

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

# GitHub #66: every parameter's default buffer in a loaded CShader must hold
# what CProgrammableShaderInstance's constructor copies out of it (a char * per
# string slot). A .slo string parameter got a float-sized buffer, an 8-byte read
# from a 4-byte allocation. Deterministic without a sanitizer: the test replaces
# operator new[] with a size tracker and puts a canary past each allocation.
# Checks the .rslo load as the control and the .slo load, so it needs the JIT.
if(OPENRENDER_HAVE_LLVM)
    add_executable(test_shading_slo_default_buffer test_slo_default_buffer.cpp)
    target_link_libraries(test_shading_slo_default_buffer PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    target_include_directories(test_shading_slo_default_buffer PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}
    )
    target_compile_features(test_shading_slo_default_buffer PRIVATE cxx_std_20)
    # SloDefaultBuffer: the buffer sizes and the canary. SloStringDefaults: the
    # .slo instance holds the same defaults as the .rslo one (string defaults were
    # lost under .slo, since the loader never parsed them).
    foreach(_case IN ITEMS "SloDefaultBuffer;buffer" "SloStringDefaults;parity")
        list(GET _case 0 _name)
        list(GET _case 1 _arg)
        add_test(NAME ShadingParity_${_name} COMMAND test_shading_slo_default_buffer ${_arg})
        set_tests_properties(ShadingParity_${_name} PROPERTIES
            TIMEOUT 30
            LABELS "shading_parity"
            ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};OSHADER_BIN=$<TARGET_FILE:oshader>"
        )
    endforeach()
endif()

# Spec 024 (shading services routing): the renderer services are registered
# for the whole session (RiBegin .. RiEnd), and a substitute installed after
# RiBegin survives beginFrame(). Later phases add the routing checks here.
add_executable(test_shading_services_routing test_services_routing.cpp)
target_link_libraries(test_shading_services_routing PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_shading_services_routing PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_services_routing PRIVATE cxx_std_20)
add_test(NAME ShadingServices_Routing COMMAND test_shading_services_routing)
set_tests_properties(ShadingServices_Routing PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity"
    # The compiled shaders live in the build tree (the default surface and light
    # load at RiWorldBegin); Case D's fixture loads from the cwd.
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>;OSHADER_BIN=$<TARGET_FILE:oshader>"
)

# Spec 024 (shading services routing), US1: the illumination hook under both
# shader backends. Runs four light shaders (illuminate(P), illuminate(P, N, a),
# solar(D, a), solar()) as .rslo and as .slo through a probe context whose
# hooks only log, with the hook on, off and toggled. Needs the JIT.
if(OPENRENDER_HAVE_LLVM)
    add_executable(test_shading_illumination_hook test_illumination_hook.cpp)
    target_link_libraries(test_shading_illumination_hook PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    target_include_directories(test_shading_illumination_hook PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}
    )
    target_compile_features(test_shading_illumination_hook PRIVATE cxx_std_20)
    add_test(NAME ShadingParity_IlluminationHook COMMAND test_shading_illumination_hook)
    set_tests_properties(ShadingParity_IlluminationHook PROPERTIES
        TIMEOUT 60
        LABELS "shading_parity"
        # The compiled shaders live in the build tree (the default surface and
        # light load at RiWorldBegin); the fixtures themselves load from the cwd.
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>;OSHADER_BIN=$<TARGET_FILE:oshader>"
    )

    # GitHub #68: the values the illumination hook is handed when lane 0 is
    # inactive (entered pre-tagged, or inside a varying conditional), with the
    # light's scratch memory filled with ASan's 0xbe so that an operand lane the
    # shader never wrote is read deterministically. Needs the JIT for the .slo
    # control rows.
    add_executable(test_shading_illumination_hook_operands test_illumination_hook_operands.cpp)
    target_link_libraries(test_shading_illumination_hook_operands PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    target_include_directories(test_shading_illumination_hook_operands PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}
    )
    target_compile_features(test_shading_illumination_hook_operands PRIVATE cxx_std_20)
    add_test(NAME ShadingParity_IlluminationHookOperands COMMAND test_shading_illumination_hook_operands)
    set_tests_properties(ShadingParity_IlluminationHookOperands PROPERTIES
        TIMEOUT 60
        LABELS "shading_parity"
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>;OSHADER_BIN=$<TARGET_FILE:oshader>"
    )

    # GitHub #68 follow-ups: the hook under a varying conditional. A varying
    # operand is handed from the first active point; a construct inside an if
    # that is false for every point calls no hook.
    add_executable(test_shading_illumination_hook_conditions test_illumination_hook_conditions.cpp)
    target_link_libraries(test_shading_illumination_hook_conditions PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    target_include_directories(test_shading_illumination_hook_conditions PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}
    )
    target_compile_features(test_shading_illumination_hook_conditions PRIVATE cxx_std_20)
    add_test(NAME ShadingParity_IlluminationHookConditions COMMAND test_shading_illumination_hook_conditions varying)
    set_tests_properties(ShadingParity_IlluminationHookConditions PROPERTIES
        TIMEOUT 60
        LABELS "shading_parity"
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>;OSHADER_BIN=$<TARGET_FILE:oshader>"
    )
    # Disabled: the JIT runs a construct inside an if that is false for every
    # point and calls the hook, where the interpreter jumps over the block
    # (GitHub #71, found under #68). Enable it with the #71 fix.
    add_test(NAME ShadingParity_IlluminationHookAllFalse COMMAND test_shading_illumination_hook_conditions allfalse)
    set_tests_properties(ShadingParity_IlluminationHookAllFalse PROPERTIES
        TIMEOUT 60
        LABELS "shading_parity"
        DISABLED TRUE
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>;OSHADER_BIN=$<TARGET_FILE:oshader>"
    )
endif()

# Spec 024 (shading services routing), US2: the libshader::lookup* helpers
# (serviceLookup.h) return what the installed services return. Linked against
# ribRender because libshader_shading still leaves some imports (the hiders and
# the texturing code) unresolved; on Linux the load would fail without it, as
# LibShader_McompConsistency does.
add_executable(test_shading_service_lookup test_service_lookup.cpp)
target_link_libraries(test_shading_service_lookup PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
# These two tests reference no libRIBRender symbol, so Ubuntu's GCC (--as-needed by default)
# drops libRIBRender from the executable's NEEDED list, and libshader_shading's remaining
# imports (the 7 DB-001 symbols, GitHub #44) then fail to resolve at load time. Keep it.
target_link_options(test_shading_service_lookup PRIVATE $<$<PLATFORM_ID:Linux>:LINKER:--no-as-needed>)
target_include_directories(test_shading_service_lookup PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_service_lookup PRIVATE cxx_std_20)
add_test(NAME ShadingServices_Lookup COMMAND test_shading_service_lookup)
set_tests_properties(ShadingServices_Lookup PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity"
)

# Spec 024 (shading services routing), US3: with no services installed every
# routed lookup returns its not-found value and warns once per kind, and an
# unresolved light category is skipped. Runs in its own process with no
# RiBegin() and no CShadingContext (GitHub #53). ribRender only resolves
# libshader_shading's remaining imports; the test never calls into it.
add_executable(test_shading_no_services test_no_services.cpp)
target_link_libraries(test_shading_no_services PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
# Same reason as test_shading_service_lookup: keep libRIBRender NEEDED on Linux (GitHub #44).
target_link_options(test_shading_no_services PRIVATE $<$<PLATFORM_ID:Linux>:LINKER:--no-as-needed>)
target_include_directories(test_shading_no_services PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_no_services PRIVATE cxx_std_20)
add_test(NAME ShadingServices_NoServices COMMAND test_shading_no_services)
set_tests_properties(ShadingServices_NoServices PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity"
)

# GitHub #53: a CShadingContext built with no renderer services installed, or
# with no globalArena checkpoint, must not write through an unset
# traceObjectHash. The contexts are built over storage whose every pointer slot
# points at a test-owned sentinel, so a write through the unset pointer is
# counted deterministically instead of crashing (or not) by luck. Case C, the
# renderer's own path inside RiBegin/RiWorldBegin, is the control and loads the
# default shaders at RiWorldBegin.
add_executable(test_shading_context_no_services test_context_no_services.cpp)
target_link_libraries(test_shading_context_no_services PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
# Same reason as test_shading_service_lookup: keep libRIBRender NEEDED on Linux (GitHub #44).
target_link_options(test_shading_context_no_services PRIVATE $<$<PLATFORM_ID:Linux>:LINKER:--no-as-needed>)
target_include_directories(test_shading_context_no_services PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_context_no_services PRIVATE cxx_std_20)
add_test(NAME ShadingServices_ContextNoServices COMMAND test_shading_context_no_services)
set_tests_properties(ShadingServices_ContextNoServices PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>"
)

# GitHub #59: shading states and the renderer services they are built from.
# newState() must fail cleanly (a diagnostic, no state) without services or
# with an inconsistent table, and freeState() must free a state by the shape it
# was built with, never by the services' current table. One executable, one
# process per case (argv[1]), so a crash in one case cannot hide another.
add_executable(test_shading_state_services test_state_services.cpp)
target_link_libraries(test_shading_state_services PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
# Same reason as test_shading_service_lookup: keep libRIBRender NEEDED on Linux (GitHub #44).
target_link_options(test_shading_state_services PRIVATE $<$<PLATFORM_ID:Linux>:LINKER:--no-as-needed>)
target_include_directories(test_shading_state_services PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_state_services PRIVATE cxx_std_20)
foreach(_case IN ITEMS
        "StateNoServices;no-services"
        "StateStubServices;stub"
        "StateTableGrowth;table-growth"
        "StateFullServices;full"
        "StateStringGlobal;string-global"
        "StateConsistentTable;consistent-table")
    list(GET _case 0 _name)
    list(GET _case 1 _arg)
    add_test(NAME ShadingServices_${_name} COMMAND test_shading_state_services ${_arg})
    set_tests_properties(ShadingServices_${_name} PROPERTIES
        TIMEOUT 30
        LABELS "shading_parity"
        # The RiBegin cases load the default surface and light at RiWorldBegin
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>"
    )
endforeach()

# GitHub #59, end to end: the same table growth from a RIB file, through the
# real orender CLI. Passes when the render exits cleanly.
add_test(NAME ShadingServices_DeclareGlobalInWorld
    COMMAND $<TARGET_FILE:orender> ${CMAKE_CURRENT_SOURCE_DIR}/declare-global-in-world.rib
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(ShadingServices_DeclareGlobalInWorld PROPERTIES
    TIMEOUT 60
    LABELS "shading_parity"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>"
)

# Derivative lanes (spec 039, Phase 7, T084b): an inactive offset lane of a ray-hit shading
# batch must not change the derivative of its real lane (GitHub #61). Fills the inactive lanes
# with two different bytes and requires identical results, and a derivative of 0.
add_executable(test_shading_parity_derivative_inactive_lanes test_derivative_inactive_lanes.cpp)
target_link_libraries(test_shading_parity_derivative_inactive_lanes PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_shading_parity_derivative_inactive_lanes PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_parity_derivative_inactive_lanes PRIVATE cxx_std_20)
add_test(NAME ShadingParity_DerivativeInactiveLanes COMMAND test_shading_parity_derivative_inactive_lanes)
set_tests_properties(ShadingParity_DerivativeInactiveLanes PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity;determinism"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
)

# Grid derivative lanes (GitHub #61, grid half): the twin of ShadingParity_DerivativeInactiveLanes
# for REYES grids. A lane the lane tags mark inactive is never read: its active neighbours take
# the one-sided difference, or 0 with no active neighbour. Fills the inactive lanes with 0x00 and
# 0xFF (NaN) and requires identical results, exact one-sided values, and unchanged other lanes.
add_executable(test_shading_parity_derivative_inactive_grid_lanes test_derivative_inactive_grid_lanes.cpp)
target_link_libraries(test_shading_parity_derivative_inactive_grid_lanes PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_shading_parity_derivative_inactive_grid_lanes PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_shading_parity_derivative_inactive_grid_lanes PRIVATE cxx_std_20)
add_test(NAME ShadingParity_DerivativeInactiveGridLanes COMMAND test_shading_parity_derivative_inactive_grid_lanes)
set_tests_properties(ShadingParity_DerivativeInactiveGridLanes PROPERTIES
    TIMEOUT 30
    LABELS "shading_parity;determinism"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
)

# Derivative builtin destinations (found during the GitHub #61 investigation): Du()/Dv() leave
# the inactive lanes of their destination untouched (DerivativeDestinationMask), and `x = Du(x)`,
# which compiles in place, gives the same values as with a separate destination
# (DerivativeInPlace). Renders one fixture through the real orender CLI on both backends and
# checks the printed values; Deriv(), area() and calculatenormal() are checked alongside. Needs
# the JIT.
if(OPENRENDER_HAVE_LLVM)
    add_executable(test_shading_parity_derivative_destination test_derivative_destination.cpp)
    target_link_libraries(test_shading_parity_derivative_destination PRIVATE openrender_common_flags)
    target_compile_features(test_shading_parity_derivative_destination PRIVATE cxx_std_20)
    foreach(_case IN ITEMS "DerivativeDestinationMask;mask" "DerivativeInPlace;alias")
        list(GET _case 0 _name)
        list(GET _case 1 _arg)
        add_test(NAME ShadingParity_${_name} COMMAND test_shading_parity_derivative_destination ${_arg})
        set_tests_properties(ShadingParity_${_name} PROPERTIES
            TIMEOUT 60
            LABELS "shading_parity"
            ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>;ORENDER_BIN=$<TARGET_FILE:orender>;TEST_DISPLAYS_DIR=$<TARGET_FILE_DIR:file>"
        )
    endforeach()
endif()
