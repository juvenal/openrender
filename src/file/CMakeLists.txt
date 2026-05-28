file(GLOB file_headers *.h)

# Shared scanline/pipeline base for all file-format display plugins.
add_library(openrenderfilebase STATIC file_base.cpp file_base.h)
target_link_libraries(openrenderfilebase PUBLIC openrendercommon)

add_library(file MODULE file.cpp file_tiff.cpp file_png.cpp ${file_headers})
target_compile_definitions(file PRIVATE HAVE_LIBPNG)
target_link_libraries(file ${openrendertiff_libs} openrenderfilebase)

if(UNIX)
	target_link_libraries(file z)
endif(UNIX)

if(WIN32)
	target_link_libraries(file zlib)
endif(WIN32)

set_target_properties(file PROPERTIES PREFIX "" SUFFIX ".dsply")
install(TARGETS file LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")

# Install file_base.h alongside dsply.h so third-party plugin authors can use it.
install(FILES file_base.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
