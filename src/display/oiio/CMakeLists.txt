# src/display/oiio/CMakeLists.txt -- the oiio display driver (spec 048). Added by src/display/CMakeLists.txt only when HAVE_OIIO.
#
# One display module, oiio.dsply (display interface 2.0.0), built on openrenderfilebase like the legacy drivers, so quantize, dither and gamma are
# the same code. It writes through libRIBImageIO and so reaches OpenImageIO only through that library: it has no OIIO include path and no OIIO
# library of its own (ImageIO_SingleCopy). HAVE_OPENEXR says whether OpenEXR is built, so the format set equals the legacy drivers'.
add_library(oiio MODULE oiio.cpp)
set_target_properties(oiio PROPERTIES PREFIX "" SUFFIX ".dsply")
target_compile_features(oiio PRIVATE cxx_std_20)
target_include_directories(oiio PRIVATE ${CMAKE_SOURCE_DIR}/src/imageio ${CMAKE_SOURCE_DIR}/src/display)
target_link_libraries(oiio PRIVATE openrenderfilebase openrenderimageio)
if(HAVE_OPENEXR)
    target_compile_definitions(oiio PRIVATE HAVE_OPENEXR)
endif()
install(TARGETS oiio LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
