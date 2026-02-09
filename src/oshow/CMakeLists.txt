
if(BUILD_SHOW)
	add_executable(oshow oshow.cpp)
	target_link_libraries(oshow ri openrendercommon FLTK::FLTK)
	install(TARGETS oshow RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif(BUILD_SHOW)
