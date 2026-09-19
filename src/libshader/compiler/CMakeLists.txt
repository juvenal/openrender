# src/libshader/compiler/CMakeLists.txt
#
# libshader_compiler — RSL→.rslo compiler library
#   Moved from src/oshader/ (all sources except oshader.cpp)
#   The oshader binary in src/oshader/ links against this library.

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
include_directories(AFTER  ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri)
include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri/hiders)
include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri/render)
include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri/state)
include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri/parse)
include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri/geometry)
include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri/texture)
include_directories(AFTER  ${CMAKE_SOURCE_DIR}/src/ri/output)

flex_file(rslo.l lex.rslo.cpp rslo)
flex_file(../../ri/parse/dso.l lex.dso.cpp dso)  # relative from src/libshader/compiler/

bison_file(rslo.y rslo_generated.cpp rslo)
bison_file(../../ri/parse/dso.y dso_generated.cpp dso)

set(parser_sources rslo_generated.cpp dso_generated.cpp)

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

if(OPENRENDER_HAVE_LLVM)
    list(APPEND compiler_sources llvmEmitter.cpp)
    llvm_map_components_to_libnames(_compiler_llvm_libs core bitwriter support)
    list(REMOVE_DUPLICATES _compiler_llvm_libs)
endif()

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

# Header-only: llvmEmitter.cpp re-expands the interpreter's own opcode/function
# tables (shaderOpcodes.h/shaderFunctions.h/giOpcodes.h/giFunctions.h) via
# local X-macro redefinition (spec 014-jit-shading-parity, mirrors the
# rslo_code.h precedent). No link dependency is introduced by this — the two
# libraries remain unlinked to each other in either direction.
target_include_directories(libshader_compiler_obj PRIVATE ${CMAKE_SOURCE_DIR}/src/libshader/shading)

target_link_libraries(libshader_compiler_obj PRIVATE openrendercommon)
if(OPENRENDER_HAVE_LLVM)
    # Include dirs and defines only — no link dep on the OBJECT library.
    # The SHARED library below carries the actual LLVM link to avoid duplicates.
# SYSTEM: emit LLVM's headers via -isystem so GCC stops diagnosing code we do
# not own. LLVM 18 produces -Wredundant-move from its ORC headers, and
# -Wnull-dereference from BasicBlock::getTerminator() inlined into our call
# sites -- both reported at a location inside /usr/lib/llvm-*/include, and
# neither actionable here. A #pragma around the #includes cannot suppress the
# inlined-instantiation case, which is why this is done per target instead.
    target_include_directories(libshader_compiler_obj SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

    # -Wnull-dereference is switched off for llvmEmitter.cpp alone, under GCC only.
    #
    # GCC reports "potential null pointer dereference" at llvm/IR/Value.h against
    # `this` inside Value::getValueID(), reached by inlining
    # Instruction::isTerminator() <- BasicBlock::back(). The pointer it doubts is
    # &bb->back(), inside LLVM's container -- not one we hand it.
    #
    # It is a false positive rooted in LLVM's intrusive ilist, whose end() IS a
    # sentinel node, so back() is *--end(). GCC's value-range propagation cannot
    # correlate empty() with back() yielding a real node, and there is no way to
    # write the test that avoids it: LLVM 23's own hasTerminator() is
    # `!InstList.empty() && InstList.back().isTerminator()` -- the identical shape
    # that warns here, and the LLVM 18 getTerminator() this code used before warned
    # the same way.
    #
    # Two narrower fixes were tried on Ubuntu 24.04 / GCC 13 / LLVM 18 and both
    # failed: marking the LLVM headers SYSTEM (above) -- ineffective because this
    # is a middle-end warning raised after inlining, not a front-end one -- and
    # guarding with `!bb->empty() &&` in currentBlockHasTerminator(), which GCC
    # cannot see through the sentinel. A #pragma in the source is attributed to
    # Value.h rather than to us, so it does not reach it either.
    #
    # Scope is one file, one compiler, one warning. The single pointer this code
    # actually owns -- IRBuilder::GetInsertBlock() -- is still asserted and
    # explicitly null-checked in currentBlockHasTerminator(); that guard is real
    # and stays.
    set_source_files_properties(llvmEmitter.cpp PROPERTIES
        COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU>:-Wno-null-dereference>")
    target_compile_definitions(libshader_compiler_obj PRIVATE ${LLVM_DEFINITIONS})
endif()

# Shared + static libraries
add_library(libshader_compiler SHARED
    $<TARGET_OBJECTS:libshader_compiler_obj>)
target_link_libraries(libshader_compiler PUBLIC openrendercommon)
if(OPENRENDER_HAVE_LLVM)
    target_link_libraries(libshader_compiler PUBLIC ${_compiler_llvm_libs})
    # LLVM's component map produces overlapping transitive deps (Core, Support appear
    # in both explicit and transitive lists).  Suppress the harmless ld64 warning.
    if(APPLE)
        target_link_options(libshader_compiler PRIVATE -Wl,-no_warn_duplicate_libraries)
    endif()
endif()
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
