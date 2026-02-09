file(GLOB openrendercommon_headers *.h)

add_library(openrendercommon STATIC os.cpp algebra.cpp ${openrendercommon_headers})
target_link_libraries(openrendercommon PUBLIC openrender_common_flags)
install(TARGETS openrendercommon LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
				ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
				RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(UNIX)
	target_link_libraries(openrendercommon PUBLIC pthread dl)
endif(UNIX)

if(APPLE)
	# some Apple specific dependencies
	target_link_libraries(openrendercommon PUBLIC "-framework CoreServices")

	# install name for dylibs on Mac
	set_target_properties(openrendercommon PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
endif(APPLE)

