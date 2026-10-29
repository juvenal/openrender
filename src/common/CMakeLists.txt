file(GLOB openrendercommon_headers *.h)

# The common layer is built once, as position-independent objects, and
# packaged twice (spec 023, research R1):
#   openrendercommon         SHARED libRIBCommon -- the one image in a process
#                            that defines its process-wide state; every
#                            in-tree shared library and executable links it
#   openrendercommon_static  STATIC libRIBCommon.a -- only for fully static
#                            programs (ribVector/ribOut/libshader_* archives)
# A shared image must never embed the archive: each embedding image would
# carry its own copy of that state (two sinks, two arenas, two token tables).
# Common_StateOwnership (tests/unit/common) enforces this.
# The variable-declaration grammar (CVariable / parseVariable, spec 023 R5).
# The generated parser #includes the generated scanner, so only the parser is
# a source; the binary directory goes on the include path for lex.var.cpp.
flex_file(variable.l lex.var.cpp var)
bison_file(variable.y variable_generated.cpp var)

add_library(openrendercommon_obj OBJECT
    os.cpp
    algebra.cpp
    rslConstants.cpp  # RiBSplineBasis, filter functions (moved from ri.cpp for libshader)
    riTokens.cpp      # the RI_* token definitions (spec 023, from parse/ri.cpp)
    colorSpace.cpp    # convertColorFrom/convertColorTo (moved from ri/init.cpp for libshader)
    diagnostics.cpp   # error/warning/fatal/info + the installable sink (spec 023, from parse/error.cpp)
    logging.cpp       # the one current_log_level (spec 023)
    # Generic infrastructure moved from src/ri/core (spec 023): libshader_shading
    # resolves these from libRIBCommon, never from the renderer library.
    fileResource.cpp
    memory.cpp        # CMemPage pages + CArena / globalArena
    random.cpp        # sampleHemisphere / sampleCosineHemisphere
    sampling.cpp      # camera slots + unscrambled Sobol reference (spec 039)
    samplingPmj02.cpp # PMJ02 pattern tables, built once on first use (spec 039)
    refCounter.cpp
    rgbe.cpp          # the Radiance RGBE codec, extern "C" (spec 027, moved from display/rgbe: one definer)
    stats.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/variable_generated.cpp
    ${openrendercommon_headers}
)
target_include_directories(openrendercommon_obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(openrendercommon_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
# BUILD_INTERFACE: the project's warning flags (-Werror included), generated
# config.h directory and HAVE_CONFIG_H are for building this tree only -- they
# must not leak into an external consumer through the exported targets.
target_link_libraries(openrendercommon_obj PUBLIC $<BUILD_INTERFACE:openrender_common_flags>)

add_library(openrendercommon SHARED $<TARGET_OBJECTS:openrendercommon_obj>)
add_library(openrendercommon_static STATIC $<TARGET_OBJECTS:openrendercommon_obj>)
foreach(_lib openrendercommon openrendercommon_static)
    target_link_libraries(${_lib} PUBLIC $<BUILD_INTERFACE:openrender_common_flags>)
    if(UNIX)
        target_link_libraries(${_lib} PUBLIC Threads::Threads dl)
    endif()
endforeach()

# Installed as libRIBCommon.{dylib,so} / openRender::RIBCommon and
# libRIBCommon.a / openRender::RIBCommon_static, matching the libRIB* naming.
set_target_properties(openrendercommon PROPERTIES
    OUTPUT_NAME RIBCommon
    EXPORT_NAME RIBCommon
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${OPENRENDER_COMPAT_SOVERSION})
set_target_properties(openrendercommon_static PROPERTIES
    OUTPUT_NAME RIBCommon
    EXPORT_NAME RIBCommon_static)
if(APPLE)
    if(INSTALL_SELFCONTAINED)
        set_target_properties(openrendercommon PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(openrendercommon PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
endif()
# The bottom layer resolves every symbol from itself or the system: an upward
# reference into a renderer library would be a layering bug. ld64 already
# rejects undefined symbols in a dylib by default (and deprecates spelling
# out -undefined error); GNU ld needs to be told.
if(UNIX AND NOT APPLE)
    target_link_options(openrendercommon PRIVATE "LINKER:--no-undefined")
endif()

install(TARGETS openrendercommon EXPORT openRenderTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
# The static RI and libshader archives name this archive as an INTERFACE link
# dependency instead of embedding their own copy, so a fully static program
# linking several of them gets exactly one copy and no duplicate symbols.
install(TARGETS openrendercommon_static EXPORT openRenderStaticTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
