# src/imageio/CMakeLists.txt -- libRIBImageIO, the Image I/O utility library (spec 048, constitution 1.5.0).
#
# Built only when HAVE_OIIO (the root CMakeLists.txt adds this directory under that guard). It wraps OpenImageIO
# behind orImageIO.h: shared only (a static variant would put OIIO into a second image, FR-004), OIIO PRIVATE, only
# libRIBCommon PUBLIC. It depends on no domain and on no renderer-core file: ImageIO_NoDomainInclude and archcheck's
# class ImageIO enforce it.

# src/CMakeLists.txt adds the Scene Interface, renderer, shading-engine and plug-in directories to every directory
# below it. This library may see none of them: keep only src/ itself (for "common/..."), so a stray include of a
# domain header would not even resolve. The property is per directory; no other target is affected.
# (src/CMakeLists.txt's own entry is src/ itself, but it is set, not filtered, so the rule is explicit.)
set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/src")

add_library(openrenderimageio SHARED
    oiioInit.cpp
    reader.cpp
    tiledTiffWriter.cpp
    orImageIO.h
)
target_compile_features(openrenderimageio PRIVATE cxx_std_20)
target_include_directories(openrenderimageio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openrenderimageio
    PRIVATE $<BUILD_INTERFACE:openrender_common_flags> OpenImageIO::OpenImageIO
    PUBLIC  openrendercommon)
set_target_properties(openrenderimageio PROPERTIES
    OUTPUT_NAME RIBImageIO
    EXPORT_NAME RIBImageIO
    POSITION_INDEPENDENT_CODE ON
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${OPENRENDER_COMPAT_SOVERSION}
    # a shared OIIO (Homebrew, the Linux prefix) is found at run time through the install RPATH
    INSTALL_RPATH_USE_LINK_PATH TRUE)

if(APPLE)
    # Only orimageio:: is exported; the static OIIO/OCIO archives stay private to this image.
    target_link_options(openrenderimageio PRIVATE
        "LINKER:-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/exports.txt"
        "LINKER:-dead_strip")
    set_property(TARGET openrenderimageio APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/exports.txt")
    if(INSTALL_SELFCONTAINED)
        set_target_properties(openrenderimageio PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(openrenderimageio PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
else()
    set_target_properties(openrenderimageio PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    target_link_options(openrenderimageio PRIVATE "LINKER:--exclude-libs,ALL")
endif()

# orImageIO.h is deliberately not in OPENRENDER_PUBLIC_HEADERS: the interface stays internal until spec 049.
install(TARGETS openrenderimageio EXPORT openRenderTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
