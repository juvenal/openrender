# No cmake_minimum_required() here on purpose: this file is only ever reached
# through add_subdirectory(), and a subdirectory call would reset the policy
# stack, overriding the tiered floor set in the top-level CMakeLists.txt.

add_library(ribpreview STATIC
    ribGeometryContext.cpp
    previewContext.cpp
    tessellators/tessPolygon.cpp
    tessellators/tessPatch.cpp
    tessellators/tessNurbs.cpp
    tessellators/tessQuadric.cpp
    tessellators/tessCurve.cpp
    tessellators/tessPoints.cpp
    tessellators/tessSubdivision.cpp
    tessellators/tessProc.cpp
    cameraExport.cpp
    dataSink.cpp
    diskExpand.cpp
    wireCli.cpp
)

target_include_directories(ribpreview PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..   # exposes ribpreview_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}      # exposes previewTypes.h, previewContext.h, etc.
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/state
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/output
    ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_SOURCE_DIR}/src/ri/dataviewer
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(ribpreview PUBLIC ribVector openrendercommon)

# The preview registers itself as the Scene Interface back-end "preview" with a static initializer in
# previewContext.cpp (spec 038). In a static archive the linker keeps an object only when something
# references it, so a program that links libribpreview and begins the back-end without calling
# ribpreview_load() would not get the registration. Every link of this library therefore references
# ribpreview_load, which pulls previewContext.cpp.o (and its registrar) in.
if(APPLE)
    target_link_options(ribpreview INTERFACE "LINKER:-u,_ribpreview_load")
else()
    target_link_options(ribpreview INTERFACE "LINKER:-u,ribpreview_load")
endif()
target_compile_features(ribpreview PRIVATE cxx_std_20)

# Suppress C++20 deprecation warning noise from ri headers
target_compile_options(ribpreview PRIVATE -Wno-deprecated-declarations)
