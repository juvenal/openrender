cmake_minimum_required(VERSION 3.15)

add_library(libribpreview STATIC
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

target_include_directories(libribpreview PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..   # exposes ribpreview_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}      # exposes previewTypes.h, previewContext.h, etc.
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(libribpreview PUBLIC ri openrendercommon)
target_compile_features(libribpreview PRIVATE cxx_std_20)

# Suppress C++20 deprecation warning noise from ri headers
target_compile_options(libribpreview PRIVATE -Wno-deprecated-declarations)
