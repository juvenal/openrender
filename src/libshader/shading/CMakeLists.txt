# src/libshader/shading/CMakeLists.txt
#
# libshader_shading — RSL shading engine
#
#   Interpreter:  execute.cpp, shading.cpp, shader.cpp, shaderPl.cpp, noise.cpp
#   RSLO loader:  rslo.y / rslo.l  (creates CShader objects at runtime)
#   JIT support:  llvmJit.cpp  (conditional, requires OPENRENDER_HAVE_LLVM)
#   Batch RSL:    rslOps.cpp, rslBuiltins.cpp  (called by JIT-compiled shaders)
#   Public API:   RSLShading.cpp
#
# Build note: this library uses headers from src/ri/ (CObject, CPhotonMap, etc.)
# via its PRIVATE include path.  It does NOT link against ri; instead ri links
# against libshader_shading (one-way dependency).  Cross-references to ri symbols
# (CObject::, stats global, etc.) are resolved when ri's shared library is linked.

# The rslo.y parser in this directory includes "lex.rslo.cpp" from the binary dir.
# Add binary and source dirs BEFORE so generated flex output is found first.
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})

set(shading_sources
    execute.cpp
    executeMisc.cpp
    noise.cpp
    RSLShading.cpp
    rslBuiltins.cpp
    rslOps.cpp
    shader.cpp
    shaderPl.cpp
    shading.cpp
)

if(USE_FLEX_BISON)
    flex_file(rslo.l lex.rslo.cpp rslo)
    bison_file(rslo.y rslo_generated.cpp rslo)
    list(APPEND shading_sources rslo_generated.cpp)
else()
    list(APPEND shading_sources rslo.cpp)
endif()

if(OPENRENDER_HAVE_LLVM)
    list(APPEND shading_sources llvmJit.cpp)
    llvm_map_components_to_libnames(SHADING_LLVM_LIBS
        core orcjit native bitreader support
    )
endif()

add_library(libshader_shading STATIC ${shading_sources})
set_target_properties(libshader_shading PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Shading engine needs src/ri/ headers for renderer types (CObject, CPhotonMap, etc.)
# and its own directory for shading.h, shader.h, rslo_code.h, macro headers.
target_include_directories(libshader_shading PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)
# Public headers: the RSLShading.h API consumed by ri/ and external callers.
target_include_directories(libshader_shading PUBLIC
    ${CMAKE_SOURCE_DIR}/src/libshader/include
)

target_link_libraries(libshader_shading PRIVATE openrendercommon)

if(OPENRENDER_HAVE_LLVM)
    target_link_libraries(libshader_shading PRIVATE ${SHADING_LLVM_LIBS})
    target_include_directories(libshader_shading PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(libshader_shading PRIVATE ${LLVM_DEFINITIONS})
endif()
