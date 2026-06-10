# src/oshader/CMakeLists.txt
#
# oshader — RSL compiler CLI tool
#
# All compilation logic has moved to src/libshader/compiler/.
# This target is now a thin CLI wrapper that links against libshader.

add_executable(oshader oshader.cpp)

target_compile_features(oshader PRIVATE c_std_17 cxx_std_20)

target_compile_options(oshader PRIVATE
    -Wall -Wextra -Wimplicit-fallthrough -Wmissing-declarations
    -Wnull-dereference -Wshadow
    $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic -Wsuggest-override>
)

# oshader.cpp includes rslo.h and opcodes.h which are now in libshader/compiler/
target_include_directories(oshader PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/compiler
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src/libshader/compiler  # Flex/Bison generated headers
)

target_link_libraries(oshader PRIVATE libshader_compiler openrendercommon)

install(TARGETS oshader RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
