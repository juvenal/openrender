# Test suite for imager shader support

# Unit test: option storage (Tests 1–3, 12)
add_executable(test_imager_options test_imager_options.cpp)
target_link_libraries(test_imager_options PRIVATE openrender_common_flags ri openrendercommon)
target_include_directories(test_imager_options PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_imager_options PRIVATE cxx_std_20)
add_test(NAME ImagerOptionTests COMMAND test_imager_options)
set_tests_properties(ImagerOptionTests PROPERTIES
    TIMEOUT 30
    LABELS "imager;unit;options"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
)

# Unit test: WorldBegin guard (Tests 4–5b)
add_executable(test_imager_guard test_imager_guard.cpp)
target_link_libraries(test_imager_guard PRIVATE openrender_common_flags ri openrendercommon)
target_include_directories(test_imager_guard PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_imager_guard PRIVATE cxx_std_20)
add_test(NAME ImagerGuardTests COMMAND test_imager_guard)
set_tests_properties(ImagerGuardTests PROPERTIES
    TIMEOUT 30
    LABELS "imager;unit;guard"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
)

# Unit test: CImagerExecutor (Tests 6–9, including P and ncomps/time/dtime)
add_executable(test_imager_execution test_imager_execution.cpp)
target_link_libraries(test_imager_execution PRIVATE openrender_common_flags ri openrendercommon)
target_include_directories(test_imager_execution PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)
target_compile_features(test_imager_execution PRIVATE cxx_std_20)
add_test(NAME ImagerExecutionTests COMMAND test_imager_execution)
set_tests_properties(ImagerExecutionTests PROPERTIES
    TIMEOUT 30
    LABELS "imager;unit;execution"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
)

add_subdirectory(integration)

message(STATUS "  - ImagerOptionTests")
message(STATUS "  - ImagerGuardTests")
message(STATUS "  - ImagerExecutionTests")
