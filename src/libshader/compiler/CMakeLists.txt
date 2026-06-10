# src/libshader/compiler/CMakeLists.txt
#
# libshader_compiler — RSL→.rslo compiler library
#   Moved from src/oshader/ (all sources except oshader.cpp)
#   The oshader binary in src/oshader/ links against this library.

if(USE_FLEX_BISON)
    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
    include_directories(AFTER  ${CMAKE_CURRENT_SOURCE_DIR})
    include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri)

    flex_file(rslo.l lex.rslo.cpp rslo)
    flex_file(../../ri/dso.l lex.dso.cpp dso)  # relative from src/libshader/compiler/

    bison_file(rslo.y rslo_generated.cpp rslo)
    bison_file(../../ri/dso.y dso_generated.cpp dso)

    set(parser_sources rslo_generated.cpp dso_generated.cpp)
else()
    set(parser_sources ../../ri/dso.cpp)  # rslo.cpp always compiled; only dso.cpp added here
endif()

set(compiler_sources
    libshader_compiler_init.cpp   # defines LOGGING_IMPLEMENTATION for this library
    expression.cpp
    ir.cpp
    irBuilder.cpp
    opcodes.cpp
    rslo.cpp        # CScriptContext non-grammar methods (always compiled; parser from rslo.y/rslo_generated.cpp)
    rsloEmitter.cpp
    preproc/pp_all.c
    passes/passManager.cpp
    passes/passDCE.cpp
    passes/passUniformLifting.cpp
    passes/passConstFold.cpp
    passes/passCSE.cpp
)

add_library(libshader_compiler_obj OBJECT
    ${compiler_sources}
    ${parser_sources}
)

set_target_properties(libshader_compiler_obj PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(libshader_compiler_obj PRIVATE c_std_17 cxx_std_20)

target_compile_options(libshader_compiler_obj PRIVATE
    -Wall -Wextra -Wimplicit-fallthrough -Wmissing-declarations
    -Wnull-dereference -Wshadow
    $<$<COMPILE_LANGUAGE:C>:
        -Wold-style-definition -Wstrict-prototypes -Wmissing-prototypes>
    $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic -Wsuggest-override>
)

target_include_directories(libshader_compiler_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/libshader/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}         # self-includes within compiler/
        ${CMAKE_CURRENT_BINARY_DIR}         # Flex/Bison generated files
        ${CMAKE_SOURCE_DIR}/src             # common/, ri_config.h
        ${CMAKE_SOURCE_DIR}/src/ri          # dso.h, shadeop.h
)

target_link_libraries(libshader_compiler_obj PRIVATE openrendercommon)

# Shared + static libraries
add_library(libshader_compiler SHARED
    $<TARGET_OBJECTS:libshader_compiler_obj>)
target_link_libraries(libshader_compiler PUBLIC openrendercommon)
target_include_directories(libshader_compiler PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/libshader/include>)
set_target_properties(libshader_compiler PROPERTIES
    OUTPUT_NAME   shader_compiler
    VERSION       ${PROJECT_VERSION}
    SOVERSION     ${OPENRENDER_COMPAT_SOVERSION}
    PREFIX        "lib")

add_library(libshader_compiler_static STATIC
    $<TARGET_OBJECTS:libshader_compiler_obj>)
set_target_properties(libshader_compiler_static PROPERTIES
    OUTPUT_NAME shader_compiler)
target_link_libraries(libshader_compiler_static INTERFACE openrendercommon)

install(TARGETS libshader_compiler
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS libshader_compiler_static
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
