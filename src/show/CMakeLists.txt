
if(BUILD_SHOW)
	add_executable(show show.cpp)
	target_link_libraries(show ri pixiecommon FLTK::FLTK)
	install(TARGETS show RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif(BUILD_SHOW)
