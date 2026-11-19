# src/libshader/runtime/CMakeLists.txt
#
# The .rslo inspection parser (rsloGet() / rsloDelete()), as an object library: libRSLInfo and libRSLInfo_static (../metadata) are
# made from it (spec 033; it was libshader_runtime, moved from src/rslo/). It shares the flex/bison prefix "rslo" with the loader of
# libRSLRuntime and the compiler's parser, so none of the three is ever linked into a program with another.

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
include_directories(AFTER  ${CMAKE_CURRENT_SOURCE_DIR})

flex_file(rslo.l lex.rslo.cpp rslo)
bison_file(rslo.y rslo_generated.cpp rslo)

set(runtime_sources rslo_generated.cpp)

add_library(rslinfo_rslo_obj OBJECT ${runtime_sources})
set_target_properties(rslinfo_rslo_obj PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(rslinfo_rslo_obj PRIVATE c_std_17 cxx_std_20)

target_include_directories(rslinfo_rslo_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/libshader/include>
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(rslinfo_rslo_obj PRIVATE openrendercommon)

# The libraries that contain this parser are libRSLInfo and libRSLInfo_static (../metadata/CMakeLists.txt), which also hold the
# shader-metadata API; libshader_runtime and libshader_runtime_static are their old names, kept as aliases for one release.
# rslo.h installs with the other public headers (OPENRENDER_PUBLIC_HEADERS, src/ri/CMakeLists.txt).
