file(GLOB openrendercommon_headers *.h)

add_library(openrendercommon STATIC
    os.cpp
    algebra.cpp
    rslConstants.cpp  # RiBSplineBasis, filter functions (moved from ri.cpp for libshader)
    sobolTables.cpp   # Sobol sequence tables (moved from random.cpp for libshader)
    colorSpace.cpp    # convertColorFrom/convertColorTo (moved from ri/init.cpp for libshader)
    ${openrendercommon_headers}
)
# Installed as libRIBCommon.a / openRender::RIBCommon, matching the libRIB*
# naming; the in-tree target name stays openrendercommon.
set_target_properties(openrendercommon PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME RIBCommon
    EXPORT_NAME RIBCommon)
# BUILD_INTERFACE: the project's warning flags (-Werror included), generated
# config.h directory and HAVE_CONFIG_H are for building this tree only -- they
# must not leak into an external consumer through the exported target.
target_link_libraries(openrendercommon PUBLIC $<BUILD_INTERFACE:openrender_common_flags>)

if(UNIX)
	target_link_libraries(openrendercommon PUBLIC Threads::Threads dl)
endif(UNIX)

# Every shared library absorbs these objects at link time, so shared-library
# consumers never need this archive. It IS installed for the static archives
# (libRIB*.a, libshader_{compiler,runtime}.a): each names it as an INTERFACE
# link dependency instead of embedding its own copy, so a program linking
# several of them -- even whole-archive, as libRIBRender.a currently requires
# (GitHub #24) -- gets exactly one copy and no duplicate symbols.
install(TARGETS openrendercommon EXPORT openRenderStaticTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
