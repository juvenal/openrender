# Integration tests for imager shader support

# See tests/imager/CMakeLists.txt: compiled shaders live in the build tree,
# and DISPLAYS points at the file display plugin's own build-tree output
# rather than the (uninstalled-by-default) deploy tree.
set(IMAGER_INTEGRATION_DISPLAY_PATH "$<TARGET_FILE_DIR:file>")

add_executable(test_imager_render test_imager_render.cpp)
target_link_libraries(test_imager_render PRIVATE openrender_common_flags openrendercommon)
target_include_directories(test_imager_render PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_imager_render PRIVATE cxx_std_20)

# Integration test 10: background imager fills non-geometry pixels
add_test(NAME ImagerBackgroundRender
    COMMAND test_imager_render
        $<TARGET_FILE:orender>
        ${CMAKE_CURRENT_SOURCE_DIR}/ribs/background-test.rib
        ${CMAKE_BINARY_DIR}/imager_bg_test.tif
        background
)
set_tests_properties(ImagerBackgroundRender PROPERTIES
    TIMEOUT 120
    LABELS "imager;integration;render"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=${IMAGER_INTEGRATION_DISPLAY_PATH}"
    DEPENDS orender
)

# Integration test 11: no-imager render is bit-identical across two runs
add_test(NAME ImagerRegressionRender
    COMMAND test_imager_render
        $<TARGET_FILE:orender>
        ${CMAKE_CURRENT_SOURCE_DIR}/ribs/no-imager-regression.rib
        ${CMAKE_BINARY_DIR}/imager_noimager_test.tif
        regression
)
set_tests_properties(ImagerRegressionRender PROPERTIES
    TIMEOUT 120
    LABELS "imager;integration;regression"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=${IMAGER_INTEGRATION_DISPLAY_PATH}"
    DEPENDS orender
)

# Integration test 13: parameter override (blue background)
add_test(NAME ImagerParamOverrideRender
    COMMAND test_imager_render
        $<TARGET_FILE:orender>
        ${CMAKE_CURRENT_SOURCE_DIR}/ribs/param-override-test.rib
        ${CMAKE_BINARY_DIR}/imager_param_test.tif
        param_override
)
set_tests_properties(ImagerParamOverrideRender PROPERTIES
    TIMEOUT 120
    LABELS "imager;integration;params"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=${IMAGER_INTEGRATION_DISPLAY_PATH}"
    DEPENDS orender
)

# Integration test 14: dual-display (FR-003)
add_test(NAME ImagerDualDisplayRender
    COMMAND test_imager_render
        $<TARGET_FILE:orender>
        ${CMAKE_CURRENT_SOURCE_DIR}/ribs/dual-display-test.rib
        ${CMAKE_BINARY_DIR}/imager_dual_test.tif
        dual_display
)
set_tests_properties(ImagerDualDisplayRender PROPERTIES
    TIMEOUT 120
    LABELS "imager;integration;dual-display"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=${IMAGER_INTEGRATION_DISPLAY_PATH}"
    DEPENDS orender
)

message(STATUS "  - ImagerBackgroundRender")
message(STATUS "  - ImagerRegressionRender")
message(STATUS "  - ImagerParamOverrideRender")
message(STATUS "  - ImagerDualDisplayRender")
