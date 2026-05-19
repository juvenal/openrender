file(GLOB rslo_headers *.h)

if(USE_FLEX_BISON)
	# this is to make sure generated bison cpp's include the correct lex.xxx.cpp
	include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
	include_directories(AFTER .)

	# define flex targets
	flex_file(rslo.l lex.rslo.cpp rslo)

	# define bison targets (they depend on flex ones, via prefix - third arg)
	bison_file(rslo.y rslo_generated.cpp rslo)
	
	# add the generated bison cpp's
	set(rslo_sources rslo_generated.cpp)
else(USE_FLEX_BISON)
	# add the ones from the source tree
	set(rslo_sources rslo.cpp)
endif(USE_FLEX_BISON)

add_library(rslo SHARED ${rslo_sources} ${rslo_headers})
target_link_libraries(rslo openrendercommon)
install(TARGETS rslo LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
			ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
			RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES rslo.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(APPLE)
	set_target_properties(rslo PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
endif(APPLE)
