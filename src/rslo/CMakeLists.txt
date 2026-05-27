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

# Compile once with PIC so the same objects serve both the shared and static lib.
add_library(rslo_obj OBJECT ${rslo_sources} ${rslo_headers})
set_target_properties(rslo_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(rslo_obj PRIVATE . ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(rslo_obj PRIVATE openrendercommon)

# Shared library
add_library(rslo SHARED $<TARGET_OBJECTS:rslo_obj>)
target_link_libraries(rslo PUBLIC openrendercommon)
set_target_properties(rslo PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${OPENRENDER_COMPAT_SOVERSION}
)
if(APPLE)
    if(INSTALL_SELFCONTAINED)
        set_target_properties(rslo PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(rslo PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
endif()

# Static library
add_library(rslo_static STATIC $<TARGET_OBJECTS:rslo_obj>)
set_target_properties(rslo_static PROPERTIES OUTPUT_NAME rslo)
target_link_libraries(rslo_static INTERFACE openrendercommon)

install(TARGETS rslo
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS rslo_static ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES rslo.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
