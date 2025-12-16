file(GLOB pixiecommon_headers *.h)

add_library(pixiecommon STATIC os.cpp algebra.cpp ${pixiecommon_headers})
target_link_libraries(pixiecommon PUBLIC pixie_common_flags)
install(TARGETS pixiecommon LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
				ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
				RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(UNIX)
	target_link_libraries(pixiecommon PUBLIC pthread dl)
endif(UNIX)

if(APPLE)
	# some Apple specific dependencies
	target_link_libraries(pixiecommon PUBLIC "-framework CoreServices")

	# install name for dylibs on Mac
	set_target_properties(pixiecommon PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
endif(APPLE)

