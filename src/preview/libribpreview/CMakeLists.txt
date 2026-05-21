cmake_minimum_required(VERSION 3.15)

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
)

target_include_directories(ribpreview PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..   # exposes ribpreview_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}      # exposes previewTypes.h, previewContext.h, etc.
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(ribpreview PUBLIC ri openrendercommon)
target_compile_features(ribpreview PRIVATE cxx_std_20)

# Suppress C++20 deprecation warning noise from ri headers
target_compile_options(ribpreview PRIVATE -Wno-deprecated-declarations)
