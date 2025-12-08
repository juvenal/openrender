# Test suite for Pixie C++20 modernization

# Create test executable for 64-bit portability
add_executable(test_64bit_portability test_64bit_portability.cpp)

# Link against common flags (includes C++20 standard)
target_link_libraries(test_64bit_portability PRIVATE pixie_common_flags)

# Include directories for accessing source headers
target_include_directories(test_64bit_portability PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}  # For config.h
)

# Add test to CTest
add_test(NAME Portability64Bit COMMAND test_64bit_portability)

# Set test properties
set_tests_properties(Portability64Bit PROPERTIES
    TIMEOUT 30
    LABELS "portability;64bit;core"
)

# Future tests can be added here:
# add_executable(test_atomics test_atomics.cpp)
# add_test(NAME AtomicOperations COMMAND test_atomics)

# Print test information
message(STATUS "Configured tests:")
message(STATUS "  - test_64bit_portability")
