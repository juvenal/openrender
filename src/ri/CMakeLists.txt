file(GLOB ri_headers *.h)

# for flex and bison files that depend on ri/ files
include_directories(.)
# Allow ri/ files to include shading engine headers from their new location.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading)

set(ri_sources
	attributes.cpp
	brickmap.cpp
	bundles.cpp
	compositor.cpp
	curves.cpp
	bsplinePatchgrid.cpp
	debug.cpp
	delayed.cpp
	displayChannel.cpp
	dlobject.cpp
#	dso.cpp
	error.cpp
	fileResource.cpp
	hcshader.cpp
#	ifexpr.cpp
	imager.cpp
	implicitSurface.cpp
	init.cpp
	irradiance.cpp
	linsys.cpp
	memory.cpp
	object.cpp
	occlusion.cpp
	options.cpp
	patches.cpp
	patchgrid.cpp
	photon.cpp
	photonMap.cpp
	pl.cpp
	points.cpp
	pointCloud.cpp
	pointHierarchy.cpp
	polygons.cpp
	ptcapi.cpp
	quadrics.cpp
	random.cpp
	raytracer.cpp
	refCounter.cpp
	remoteChannel.cpp
	renderer.cpp
	rendererClipping.cpp
	rendererContext.cpp
	rendererDeclarations.cpp
	rendererDisplay.cpp
	rendererFiles.cpp
	rendererJobs.cpp
	rendererMutexes.cpp
	rendererNetwork.cpp
	rendererShadingServices.cpp
	resource.cpp
	sampler.cpp
	reyes.cpp
	ri.cpp
#	rib.cpp
	riInterface.cpp
	ribOut.cpp
	show.cpp
#	sl.cpp
	stats.cpp
	stochastic.cpp
	subdivision.cpp
	subdivisionCreator.cpp
	subdivisionHierarchical.cpp
	subdivisionLoop.cpp
	surface.cpp
	texmake.cpp
	texture.cpp
	texture3d.cpp
	trace.cpp
#	variable.cpp
	xform.cpp
	zbuffer.cpp
)

if(USE_FLEX_BISON)
	# this is to make sure generated bison cpp's include the correct lex.xxx.cpp
	include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
	include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})

	# define flex targets
	flex_file(dso.l lex.dso.cpp dso)
	flex_file(rib.l lex.rib.cpp rib)
	flex_file(variable.l lex.var.cpp var)
	flex_file(ifexpr.l lex.if.cpp if)

	# define bison targets (they depend on flex ones, via prefix - third arg)
	bison_file(dso.y dso_generated.cpp dso)
	bison_file(rib.y rib_generated.cpp rib)
	bison_file(variable.y variable_generated.cpp var)
	bison_file(ifexpr.y ifexpr_generated.cpp if)

	# add the generated bison cpp's
	set(ri_sources ${ri_sources} dso_generated.cpp rib_generated.cpp variable_generated.cpp ifexpr_generated.cpp)
else()
	set(ri_sources ${ri_sources} dso.cpp rib.cpp variable.cpp ifexpr.cpp)
endif(USE_FLEX_BISON)

# Compile once with PIC so the same objects serve both the shared and static lib.
add_library(ri_obj OBJECT ${ri_sources} ${ri_headers})
set_target_properties(ri_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ri_obj PRIVATE . ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading)
# Propagate compile-time interface requirements: HAVE_CONFIG_H, warning flags,
# include dirs, and external lib headers (TIFF, PNG).
target_link_libraries(ri_obj PRIVATE openrendercommon ${openrendertiff_libs})

# Shared library — external deps (TIFF, PNG, z) are PRIVATE (implementation detail
# of libri; not part of its public API, so not propagated to linking executables).
# Phase C: ri links against libshader_shading (shading engine in its own library).
# LLVM is NOT added explicitly here; it propagates from libshader_shading PUBLIC link deps.
add_library(ri SHARED $<TARGET_OBJECTS:ri_obj>)
# Propagate include paths to consumers: ri headers (options.h, shader.h forwarding, etc.)
# and shading engine headers (shader.h actual, shading.h actual) must be findable by
# any target that includes ri's headers transitively.
target_include_directories(ri PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading>
)
target_link_libraries(ri PRIVATE libshader_shading ${openrendertiff_libs} openrendercommon)
if(WIN32)
    target_link_libraries(ri PRIVATE zlib ws2_32)
endif()
if(UNIX)
    target_link_libraries(ri PRIVATE z)
endif()
set_target_properties(ri PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${OPENRENDER_COMPAT_SOVERSION}
)
if(APPLE)
    if(INSTALL_SELFCONTAINED)
        set_target_properties(ri PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(ri PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
endif()

# Static library — same object files, library consumers inherit the link requirements
add_library(ri_static STATIC $<TARGET_OBJECTS:ri_obj>)
set_target_properties(ri_static PROPERTIES OUTPUT_NAME ri)
target_link_libraries(ri_static INTERFACE libshader_shading ${openrendertiff_libs} openrendercommon)
if(WIN32)
    target_link_libraries(ri_static INTERFACE zlib ws2_32)
endif()
if(UNIX)
    target_link_libraries(ri_static INTERFACE z)
endif()

install(TARGETS ri
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS ri_static ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES dlo.h dsply.h implicit.h ptcapi.h ri.h shadeop.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
