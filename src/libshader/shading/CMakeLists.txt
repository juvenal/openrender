# src/libshader/shading/CMakeLists.txt
#
# libRSLRuntime (formerly libshader_shading) — the shading engine with both shader loaders
#   (spec 033: the old target, file and export names resolve for one release: ALIAS target, compatibility symlink, exported alias)
#
#   Interpreter:  execute.cpp, shading.cpp, shader.cpp, shaderPl.cpp, noise.cpp
#   RSLO loader:  rslo.y / rslo.l  (creates CShader objects at runtime)
#   JIT support:  llvmJit.cpp  (conditional, requires OPENRENDER_HAVE_LLVM)
#   Batch RSL:    rslOps.cpp, rslBuiltins.cpp  (called by JIT-compiled shaders)
#   Public API:   RSLShading.cpp
#
# Build note: the library is self-contained (spec 029). It links with undefined symbols as an error
# (ld64's default for a dylib, --no-undefined on ELF) and depends only on libRIBCommon, libRIBShadeop and
# LLVM; it imports no symbol of the renderer or of the hiders. It uses the headers of the domains it may
# depend on through its PRIVATE include path, and ri links against it (one-way dependency).

# src/CMakeLists.txt adds the Hiders' and Geometry's directories to every directory below it, but Shading includes no header of either
# domain (spec 029; ShadingScope_Greps S1a and S1e): drop those two from this directory's include path, the way the compiler's
# CMake drops the renderer's (src/libshader/compiler/CMakeLists.txt). The property is per directory, so no other target is affected.
get_property(_shading_dir_includes DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
set(_shading_kept_includes "")
foreach(_dir IN LISTS _shading_dir_includes)
    # A plain compare, not a regex: the source path may contain characters a regex treats as special.
    if(_dir STREQUAL "${CMAKE_SOURCE_DIR}/src/ri/hiders" OR _dir STREQUAL "${CMAKE_SOURCE_DIR}/src/ri/geometry")
        continue()
    endif()
    list(APPEND _shading_kept_includes "${_dir}")
endforeach()
set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "${_shading_kept_includes}")

# The rslo.y parser in this directory includes "lex.rslo.cpp" from the binary dir.
# Add binary and source dirs BEFORE so generated flex output is found first.
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})

set(shading_sources
    activeContext.cpp
    bundles.cpp
    execute.cpp
    executeMisc.cpp
    irradianceCalls.cpp
    noise.cpp
    RSLShading.cpp
    rslBuiltins.cpp
    rslOps.cpp
    rsloBackend.cpp
    rsloInit.cpp
    serviceLookup.cpp
    shader.cpp
    shaderBackend.cpp
    shaderBind.cpp
    shaderParameters.cpp
    shaderPl.cpp
    shaderGroup.cpp
    shading.cpp
    shadingSystem.cpp
    situation.cpp
    sloBackend.cpp
    sloLoader.cpp
    textureCalls.cpp
    traceCalls.cpp
)

# Spec 035 (closure tree and BSDF): the closure ABI's implementation lives in ../closures and joins this library. Listed one by one
# on purpose: a glob could pick up a later file silently.
set(closure_sources
    ../closures/bsdf.cpp
    ../closures/bsdfComponents.cpp
    ../closures/bsdfMath.cpp
    ../closures/closureRegistry.cpp
    ../closures/closureTree.cpp
)
list(APPEND shading_sources ${closure_sources})

flex_file(rslo.l lex.rslo.cpp rslo)
bison_file(rslo.y rslo_generated.cpp rslo)
list(APPEND shading_sources rslo_generated.cpp)

if(OPENRENDER_HAVE_LLVM)
    list(APPEND shading_sources llvmJit.cpp)
    llvm_map_components_to_libnames(SHADING_LLVM_LIBS
        core orcjit native bitreader support
    )

    # libRSLJitMeta (formerly libshader_jitmeta) — the .slo bitcode metadata probe
    # (CLLVMJitEngine::extractMetadataFromFile/extractMetadataFromModule), an internal static library, not installed.
    # Deliberately split out of the engine library: this code touches only LLVM bitcode APIs, never CRenderer/stats/vtables/etc.
    # from ri. libRSLInfo (the shader-metadata API, ../metadata) links it for its .slo reader, so the inspection tool needs no JIT
    # and no renderer; the engine links it as well, as before.
    llvm_map_components_to_libnames(JITMETA_LLVM_LIBS core bitreader support)
    add_library(libRSLJitMeta STATIC llvmJitMetadata.cpp)
    set_target_properties(libRSLJitMeta PROPERTIES OUTPUT_NAME RSLJitMeta POSITION_INDEPENDENT_CODE ON)
    target_include_directories(libRSLJitMeta PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(libRSLJitMeta SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(libRSLJitMeta PRIVATE ${LLVM_DEFINITIONS})
    target_link_libraries(libRSLJitMeta PUBLIC ${JITMETA_LLVM_LIBS})
    add_library(libshader_jitmeta ALIAS libRSLJitMeta)   # the old name, kept for one release
endif()

add_library(libRSLRuntime SHARED ${shading_sources})
set_target_properties(libRSLRuntime PROPERTIES
    OUTPUT_NAME RSLRuntime
    EXPORT_NAME RSLRuntime
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

if(APPLE)
    if(INSTALL_SELFCONTAINED)
        set_target_properties(libRSLRuntime PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(libRSLRuntime PROPERTIES
            INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
    # ld64's default for a dylib is already `-undefined error`, so an undefined symbol is a link error here too.
elseif(UNIX)
    # GNU ld has to be told: an undefined symbol in this library is a link error, as for libRIBCommon
    # (src/common/CMakeLists.txt). Nothing inherits a relaxation: no consumer needs one.
    target_link_options(libRSLRuntime PRIVATE "LINKER:--no-undefined")
endif()

# Shading engine needs the headers of the domains it may depend on (Graphics State, Texturing, the Infrastructure
# in src/ri/core and src/ri/parse's ri.h), plus its own directory for shading.h, shader.h, rslo_code.h, macro headers.
# Neither the Hiders' nor Geometry's directory is on the path (spec 029): the bundles, IPhotonLookup and IPointSource
# are Shading's own, in this directory.
target_include_directories(libRSLRuntime PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_SOURCE_DIR}/src/ri/state
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src/libshader/closures
    ${CMAKE_SOURCE_DIR}/src/include
)
# Public headers: the RSLShading.h API consumed by ri/ and external callers.
target_include_directories(libRSLRuntime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/libshader/include>
)

# The shared libRIBCommon: its infrastructure symbols (diagnostics, arena,
# stats, ref-counting, sampling, parseVariable) bind there, never to libRIBRender
# (spec 023, US2).
target_link_libraries(libRSLRuntime PRIVATE openrendercommon openrendershadeop)

if(OPENRENDER_HAVE_LLVM)
    # PUBLIC so that ri (which links this shared lib) automatically gets LLVM
    # on its link line without ri/CMakeLists.txt having to add it again.
    # BUILD_INTERFACE: only in-tree; LLVM is already linked into this shared
    # library, and an exported consumer cannot resolve LLVM component names.
    target_link_libraries(libRSLRuntime PUBLIC "$<BUILD_INTERFACE:${SHADING_LLVM_LIBS}>")
# SYSTEM: emit LLVM's headers via -isystem so GCC stops diagnosing code we do
# not own. LLVM 18 produces -Wredundant-move from its ORC headers, and
# -Wnull-dereference from BasicBlock::getTerminator() inlined into our call
# sites -- both reported at a location inside /usr/lib/llvm-*/include, and
# neither actionable here. A #pragma around the #includes cannot suppress the
# inlined-instantiation case, which is why this is done per target instead.
    target_include_directories(libRSLRuntime SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(libRSLRuntime PRIVATE ${LLVM_DEFINITIONS})
    target_link_libraries(libRSLRuntime PRIVATE libRSLJitMeta)
endif()

# Install the shared library
install(TARGETS libRSLRuntime EXPORT openRenderTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${OPENRENDER_INCLUDE_INSTALL_DIR}"
)

# The old name, kept for one release: an ALIAS target (every in-tree consumer that still links libshader_shading), and the
# compatibility file libshader_shading.{dylib,so} -> libRSLRuntime.{dylib,so} (shared only: there is no static engine). The
# POST_BUILD command must be in the directory that defines the target, so it is here and not in src/ri/CMakeLists.txt's loop (same
# mechanism, CMake/createCompatAlias.cmake); the install rule comes after install(TARGETS) above, since the script refuses to alias a
# file that is not there yet. Not on Windows.
add_library(libshader_shading ALIAS libRSLRuntime)
if(NOT WIN32)
    set(_compat_alias_script "${PROJECT_SOURCE_DIR}/CMake/createCompatAlias.cmake")
    set(_real_file  "${CMAKE_SHARED_LIBRARY_PREFIX}RSLRuntime${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(_alias_file "${CMAKE_SHARED_LIBRARY_PREFIX}shader_shading${CMAKE_SHARED_LIBRARY_SUFFIX}")
    add_custom_command(TARGET libRSLRuntime POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DDIR=$<TARGET_FILE_DIR:libRSLRuntime>"
            "-DTARGET_NAME=${_real_file}"
            "-DALIAS_NAME=${_alias_file}"
            -P "${_compat_alias_script}"
        COMMENT "Creating ${_alias_file} -> ${_real_file} compatibility alias"
        VERBATIM)
    install(CODE
        "execute_process(COMMAND \"${CMAKE_COMMAND}\"
            \"-DDIR=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\"
            \"-DTARGET_NAME=${_real_file}\"
            \"-DALIAS_NAME=${_alias_file}\"
            -P \"${_compat_alias_script}\"
         COMMAND_ERROR_IS_FATAL ANY)")
endif()
