# The Shader DSO Operands domain (spec 037): shadeop.h (the ABI header, installed under its name),
# shadeopDeclaration.{h,cpp} (the reentrant declaration parser that replaces the flex/bison dso.{l,y}) and
# shadeopModule.{h,cpp} (module scanning, the <name>_shadeops table lookup, the version check, operand
# acquisition, the per-thread state and the release).
#
# Packaged twice from one set of position-independent objects, like libRIBCommon:
#   openrendershadeop         SHARED libRIBShadeop -- the one image in a process that owns the registry code;
#                             ribRender, libshader_shading and libshader_compiler link it
#   openrendershadeop_static  STATIC libRIBShadeop.a -- only for fully static programs
# ribRender_static links the SHARED library (never a static copy): libshader_shading is shared-only and binds
# to the shared one, and a second copy would be a second parser and a second scan (gotcha 22's rule).
#
# Depends on its own headers and Infra (common/, include/) only: no renderer or shading header.

add_library(openrendershadeop_obj OBJECT shadeopDeclaration.cpp shadeopDeclaration.h shadeopModule.cpp shadeopModule.h shadeop.h)
target_include_directories(openrendershadeop_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src)
set_target_properties(openrendershadeop_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(openrendershadeop_obj PRIVATE cxx_std_20)
target_link_libraries(openrendershadeop_obj PUBLIC $<BUILD_INTERFACE:openrender_common_flags>)

add_library(openrendershadeop SHARED $<TARGET_OBJECTS:openrendershadeop_obj>)
# The shared libRIBCommon (checkPluginVersion, diagnostics, os), not an embedded copy of the archive.
target_link_libraries(openrendershadeop PUBLIC openrendercommon)
set_target_properties(openrendershadeop PROPERTIES
    OUTPUT_NAME RIBShadeop
    EXPORT_NAME RIBShadeop
    VERSION     ${PROJECT_VERSION}
    SOVERSION   ${OPENRENDER_COMPAT_SOVERSION})
if(APPLE)
    if(INSTALL_SELFCONTAINED)
        set_target_properties(openrendershadeop PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(openrendershadeop PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
endif()

add_library(openrendershadeop_static STATIC $<TARGET_OBJECTS:openrendershadeop_obj>)
set_target_properties(openrendershadeop_static PROPERTIES
    OUTPUT_NAME RIBShadeop
    EXPORT_NAME RIBShadeop_static)
target_link_libraries(openrendershadeop_static INTERFACE openrendercommon_static)

install(TARGETS openrendershadeop EXPORT openRenderTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${OPENRENDER_INCLUDE_INSTALL_DIR}")
install(TARGETS openrendershadeop_static EXPORT openRenderStaticTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${OPENRENDER_INCLUDE_INSTALL_DIR}")
