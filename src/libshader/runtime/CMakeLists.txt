# src/libshader/runtime/CMakeLists.txt
#
# libshader_runtime — .rslo inspection loader
#   Moved from src/rslo/
#   Provides rsloGet() / rsloDelete() for the sloinfo tool.

if(USE_FLEX_BISON)
    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
    include_directories(AFTER  ${CMAKE_CURRENT_SOURCE_DIR})

    flex_file(rslo.l lex.rslo.cpp rslo)
    bison_file(rslo.y rslo_generated.cpp rslo)

    set(runtime_sources rslo_generated.cpp)
else()
    set(runtime_sources rslo.cpp)
endif()

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
target_link_libraries(libshader_runtime PUBLIC openrendercommon)
target_include_directories(libshader_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_target_properties(libshader_runtime PROPERTIES
    OUTPUT_NAME   shader_runtime
    VERSION       ${PROJECT_VERSION}
    SOVERSION     ${OPENRENDER_COMPAT_SOVERSION}
    PREFIX        "lib")

add_library(libshader_runtime_static STATIC
    $<TARGET_OBJECTS:libshader_runtime_obj>)
set_target_properties(libshader_runtime_static PROPERTIES
    OUTPUT_NAME shader_runtime)
target_link_libraries(libshader_runtime_static INTERFACE openrendercommon)

install(TARGETS libshader_runtime
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS libshader_runtime_static
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES rslo.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
