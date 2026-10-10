# src/libshader/runtime/CMakeLists.txt
#
# libshader_runtime — .rslo inspection loader
#   Moved from src/rslo/
#   Provides rsloGet() / rsloDelete() for the sloinfo tool.

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
include_directories(AFTER  ${CMAKE_CURRENT_SOURCE_DIR})

flex_file(rslo.l lex.rslo.cpp rslo)
bison_file(rslo.y rslo_generated.cpp rslo)

set(runtime_sources rslo_generated.cpp)

add_library(libshader_runtime_obj OBJECT ${runtime_sources})
set_target_properties(libshader_runtime_obj PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(libshader_runtime_obj PRIVATE c_std_17 cxx_std_20)

target_include_directories(libshader_runtime_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/libshader/include>
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(libshader_runtime_obj PRIVATE openrendercommon)

# Shared + static libraries
add_library(libshader_runtime SHARED
    $<TARGET_OBJECTS:libshader_runtime_obj>)
# The shared libRIBCommon, not an embedded copy of the archive: one image per
# process defines the common layer's state (spec 023, research R1).
target_link_libraries(libshader_runtime PUBLIC openrendercommon)
target_include_directories(libshader_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_target_properties(libshader_runtime PROPERTIES
    OUTPUT_NAME   shader_runtime
    VERSION       ${PROJECT_VERSION}
    SOVERSION     ${OPENRENDER_COMPAT_SOVERSION}
    PREFIX        "lib"
    EXPORT_NAME   shader_runtime)

add_library(libshader_runtime_static STATIC
    $<TARGET_OBJECTS:libshader_runtime_obj>)
set_target_properties(libshader_runtime_static PROPERTIES
    OUTPUT_NAME shader_runtime
    EXPORT_NAME shader_runtime_static)
target_link_libraries(libshader_runtime_static INTERFACE openrendercommon_static)

install(TARGETS libshader_runtime EXPORT openRenderTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(TARGETS libshader_runtime_static EXPORT openRenderStaticTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES rslo.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
