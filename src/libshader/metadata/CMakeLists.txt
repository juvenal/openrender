# src/libshader/metadata/CMakeLists.txt
#
# libRSLInfo (spec 033, FR-016 to FR-018, contracts/shader-metadata.md): the inspection side of the shading engine's shaders.
#   - the shader-metadata API (include/openrender/ShaderMetadata.h): describeShaderFile() and the per-format readers;
#   - the .rslo inspection parser (../runtime, rsloGet()/rslo.h), which it contains: the old libshader_runtime;
#   - the .slo reader (CLLVMJitEngine's bitcode metadata probe, libRSLJitMeta), in the shared library only, with the JIT.
# libRSLInfo (shared) and libRSLInfo_static (the .rslo half: no LLVM, a .slo request answers Unsupported).
#
# NEVER linked into a program that also links libRSLRuntime (the shading engine): both define the 37 flex/bison "rslo*" globals
# (Link_InfoRuntimeNotCoLinked checks it). Old names, kept for one release: libshader_runtime and libshader_runtime_static (ALIAS
# targets, exported aliases, compatibility symlinks); libshader_metadata never existed outside this spec.

set(rslinfo_sources shaderMetadata.cpp rsloMetadataReader.cpp)

add_library(libRSLInfo SHARED $<TARGET_OBJECTS:rslinfo_rslo_obj> ${rslinfo_sources})
add_library(libRSLInfo_static STATIC $<TARGET_OBJECTS:rslinfo_rslo_obj> ${rslinfo_sources})

foreach(_t libRSLInfo libRSLInfo_static)
    target_compile_features(${_t} PRIVATE cxx_std_20)
    target_include_directories(${_t}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../runtime>   # rslo.h
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src)
endforeach()

# The shared libRIBCommon, not an embedded copy of the archive: one image per process defines the common layer's state (spec 023,
# research R1).
target_link_libraries(libRSLInfo PUBLIC openrendercommon)
set_target_properties(libRSLInfo PROPERTIES
    OUTPUT_NAME   RSLInfo
    EXPORT_NAME   RSLInfo
    VERSION       ${PROJECT_VERSION}
    SOVERSION     ${OPENRENDER_COMPAT_SOVERSION}
    PREFIX        "lib")

set_target_properties(libRSLInfo_static PROPERTIES
    OUTPUT_NAME   RSLInfo
    EXPORT_NAME   RSLInfo_static)
target_link_libraries(libRSLInfo_static INTERFACE openrendercommon_static)

# The .slo reader: LLVM sits inside the shared library (as it sat inside sloinfo before), never in the static archive
if(OPENRENDER_HAVE_LLVM)
    target_sources(libRSLInfo PRIVATE sloMetadataReader.cpp)
    target_compile_definitions(libRSLInfo PRIVATE RSLINFO_WITH_SLO=1)
    target_include_directories(libRSLInfo SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(libRSLInfo PRIVATE ${LLVM_DEFINITIONS})
    target_link_libraries(libRSLInfo PRIVATE libRSLJitMeta)
endif()

install(TARGETS libRSLInfo EXPORT openRenderTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${OPENRENDER_INCLUDE_INSTALL_DIR}")
install(TARGETS libRSLInfo_static EXPORT openRenderStaticTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${OPENRENDER_INCLUDE_INSTALL_DIR}")
# rslo.h installs with the other public headers (OPENRENDER_PUBLIC_HEADERS, src/ri/CMakeLists.txt).

# The old names (one release)
add_library(libshader_runtime ALIAS libRSLInfo)
add_library(libshader_runtime_static ALIAS libRSLInfo_static)

# Compatibility files libshader_runtime.{dylib,so,a} -> libRSLInfo.*: the commands must be in the directory that defines the
# target, so they are here and not in src/ri/CMakeLists.txt's loop (same mechanism, CMake/createCompatAlias.cmake). The install
# rules come after install(TARGETS) above: the script refuses to alias a file that is not there yet. Not on Windows.
if(NOT WIN32)
    set(_compat_alias_script "${PROJECT_SOURCE_DIR}/CMake/createCompatAlias.cmake")
    foreach(_kind SHARED STATIC)
        if(_kind STREQUAL "SHARED")
            set(_kind_tgt libRSLInfo)
        else()
            set(_kind_tgt libRSLInfo_static)
        endif()
        set(_real_file  "${CMAKE_${_kind}_LIBRARY_PREFIX}RSLInfo${CMAKE_${_kind}_LIBRARY_SUFFIX}")
        set(_alias_file "${CMAKE_${_kind}_LIBRARY_PREFIX}shader_runtime${CMAKE_${_kind}_LIBRARY_SUFFIX}")
        add_custom_command(TARGET ${_kind_tgt} POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DDIR=$<TARGET_FILE_DIR:${_kind_tgt}>"
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
    endforeach()
endif()
