file(GLOB openrendercommon_headers *.h)

add_library(openrendercommon STATIC
    os.cpp
    algebra.cpp
    rslConstants.cpp  # RiBSplineBasis, filter functions (moved from ri.cpp for libshader)
    sobolTables.cpp   # Sobol sequence tables (moved from random.cpp for libshader)
    ${openrendercommon_headers}
)
set_target_properties(openrendercommon PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(openrendercommon PUBLIC openrender_common_flags)
# openrendercommon is an internal build helper; its object code is embedded in
# libri and librslo, so there is nothing useful to install for external consumers.

if(UNIX)
	target_link_libraries(openrendercommon PUBLIC Threads::Threads dl)
endif(UNIX)


