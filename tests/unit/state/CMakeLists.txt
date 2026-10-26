# Graphics State tests (spec 025, graphics-state-handles). Every add_test()
# here carries the "state" label, so `ctest -L state` selects them all.

# FR-025 / SC-010: lights keep RIB order across an attribute push, a
# copy-on-write clone and a ResourceRestore. Links the renderer (it drives the
# Ri calls), so it is not one of the State-only tests.
add_executable(test_state_light_order test_light_order.cpp)
target_link_libraries(test_state_light_order PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_state_light_order PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_state_light_order PRIVATE cxx_std_20)
add_test(NAME State_LightOrder COMMAND test_state_light_order)
set_tests_properties(State_LightOrder PROPERTIES
    TIMEOUT 60
    LABELS "state"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
)
