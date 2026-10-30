# Unit tests for the RIB parser (src/ri/parse).
#
# `ctest --test-dir build -L rib_parse` runs every test registered here.

add_executable(test_rib_parser_sessions test_parser_sessions.cpp)
target_compile_features(test_rib_parser_sessions PRIVATE cxx_std_20)
target_include_directories(test_rib_parser_sessions PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri/parse)
target_link_libraries(test_rib_parser_sessions PRIVATE ribRender openrendercommon)
add_test(NAME RibParse_SessionsReadArchives COMMAND test_rib_parser_sessions)
set_tests_properties(RibParse_SessionsReadArchives PROPERTIES TIMEOUT 60 LABELS "rib_parse;unit"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")

# Spec 039 (Phase 3, US5): Option "sampling". The options are read back through
# the public interface, so these link the renderer library and need the
# internal headers of the parser, the state and the renderer.
set(SAMPLING_OPTION_INCLUDES
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output
    ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/dataviewer ${CMAKE_SOURCE_DIR}/src/libshader/shading)
# ORENDERHOME points at the source root, which has no .orenderrc, so a Display
# in tests/visual's .orenderrc cannot make these sessions open a file display.
set(SAMPLING_OPTION_ENV "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")

add_executable(test_sampling_option test_sampling_option.cpp)
target_compile_features(test_sampling_option PRIVATE cxx_std_20)
target_include_directories(test_sampling_option PRIVATE ${SAMPLING_OPTION_INCLUDES})
target_link_libraries(test_sampling_option PRIVATE ribRender openrendercommon)
add_test(NAME SamplingOption_Parse COMMAND test_sampling_option parse)
add_test(NAME SamplingOption_UnknownName COMMAND test_sampling_option unknown_name)
add_test(NAME SamplingOption_UnknownKeepsCurrent COMMAND test_sampling_option unknown_keeps_current)
set_tests_properties(SamplingOption_Parse SamplingOption_UnknownName SamplingOption_UnknownKeepsCurrent PROPERTIES TIMEOUT 60
    LABELS "rib_parse;unit;sampling" ENVIRONMENT "${SAMPLING_OPTION_ENV}")

add_executable(test_sampling_option_roundtrip test_sampling_option_roundtrip.cpp)
target_compile_features(test_sampling_option_roundtrip PRIVATE cxx_std_20)
target_include_directories(test_sampling_option_roundtrip PRIVATE ${SAMPLING_OPTION_INCLUDES})
target_link_libraries(test_sampling_option_roundtrip PRIVATE ribRender openrendercommon)
add_test(NAME SamplingOption_RibRoundTrip COMMAND test_sampling_option_roundtrip)
set_tests_properties(SamplingOption_RibRoundTrip PROPERTIES TIMEOUT 60
    LABELS "rib_parse;unit;sampling" ENVIRONMENT "${SAMPLING_OPTION_ENV}")

# Spec 039, Phase 3 (T033, RSL half): option("sampling:sampler") and
# option("sampling:seed") on a real shading context.
add_executable(test_sampling_option_rsl test_sampling_option_rsl.cpp)
target_compile_features(test_sampling_option_rsl PRIVATE cxx_std_20)
target_include_directories(test_sampling_option_rsl PRIVATE ${SAMPLING_OPTION_INCLUDES})
target_link_libraries(test_sampling_option_rsl PRIVATE ribRender libshader_shading openrendercommon)
add_test(NAME SamplingOption_RslQuery COMMAND test_sampling_option_rsl query)
add_test(NAME SamplingOption_RslQueryNoServices COMMAND test_sampling_option_rsl no_services)
set_tests_properties(SamplingOption_RslQuery SamplingOption_RslQueryNoServices PROPERTIES TIMEOUT 60
    LABELS "rib_parse;unit;sampling" ENVIRONMENT "${SAMPLING_OPTION_ENV}")

# Spec 025 (graphics-state-handles), FR-023 / US3.5: the RIB writer keeps the order of
# Surface/Displacement/Atmosphere/Interior/Exterior/LightSource/Imager. A characterization
# test: green on the starting tree, and it must stay green.
add_executable(test_ribout_shader_order test_ribout_shader_order.cpp)
target_compile_features(test_ribout_shader_order PRIVATE cxx_std_20)
target_include_directories(test_ribout_shader_order PRIVATE ${SAMPLING_OPTION_INCLUDES})
target_link_libraries(test_ribout_shader_order PRIVATE ribRender openrendercommon)
add_test(NAME RibOut_ShaderStatementOrder COMMAND test_ribout_shader_order)
set_tests_properties(RibOut_ShaderStatementOrder PROPERTIES TIMEOUT 60
    LABELS "rib_parse;unit" ENVIRONMENT "${SAMPLING_OPTION_ENV}")
