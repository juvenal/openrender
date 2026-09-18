file(GLOB ri_headers *.h)

# for flex and bison files that depend on ri/ files
include_directories(.)
# Allow ri/ files to include shading engine headers from their new location.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading)

set(ri_sources
	attributes.cpp
	blobby.cpp
	blobbyField.cpp
	blobbyPolygonize.cpp
	blobbyRepeller.cpp
	brickmap.cpp
	bundles.cpp
	compositor.cpp
	curves.cpp
	bsplinePatchgrid.cpp
	csgBoolean.cpp
	csgTree.cpp
	dataLoad.cpp
	dataView.cpp
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
#	sl.cpp
	solidObject.cpp
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

# Compile once with PIC so the same objects serve both the shared and static lib.
#
# Target names: ri/ri_static/ri_obj were renamed to ribRender/ribRender_static/
# ribRender_obj as part of splitting this library into domain-scoped pieces
# (ribRender = full renderer, ribVector = geometry-only extraction for
# orender-wire, ribOut = RIB text output) -- see the domain-split plan.
#
# The compiled artifact itself is renamed too, from libri to libRIBRender --
# nothing outside this repo distributes libri, so this is the cheap moment to
# do it. libri is kept as a symlinked alias (below) purely so anything not yet
# updated to the new name keeps working; new/internal consumers should link
# against RIBRender directly rather than rely on the alias.
add_library(ribRender_obj OBJECT ${ri_sources} ${ri_headers})
set_target_properties(ribRender_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ribRender_obj PRIVATE . ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading)
# Propagate compile-time interface requirements: HAVE_CONFIG_H, warning flags,
# include dirs, and external lib headers (TIFF, PNG).
target_link_libraries(ribRender_obj PRIVATE openrendercommon ${openrendertiff_libs})

# Shared library — external deps (TIFF, PNG, z) are PRIVATE (implementation detail
# of libri; not part of its public API, so not propagated to linking executables).
# Phase C: ri links against libshader_shading (shading engine in its own library).
# LLVM is NOT added explicitly here; it propagates from libshader_shading PUBLIC link deps.
add_library(ribRender SHARED $<TARGET_OBJECTS:ribRender_obj>)
set_target_properties(ribRender PROPERTIES OUTPUT_NAME RIBRender)
# Propagate include paths to consumers: ri headers (options.h, shader.h forwarding, etc.)
# and shading engine headers (shader.h actual, shading.h actual) must be findable by
# any target that includes ri's headers transitively.
target_include_directories(ribRender PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading>
)
target_link_libraries(ribRender PRIVATE libshader_shading ${openrendertiff_libs} openrendercommon)
if(WIN32)
    target_link_libraries(ribRender PRIVATE zlib ws2_32)
endif()
if(UNIX)
    target_link_libraries(ribRender PRIVATE z)
endif()
set_target_properties(ribRender PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${OPENRENDER_COMPAT_SOVERSION}
)
if(APPLE)
    if(INSTALL_SELFCONTAINED)
        set_target_properties(ribRender PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(ribRender PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
endif()

# Static library — same object files, library consumers inherit the link requirements
add_library(ribRender_static STATIC $<TARGET_OBJECTS:ribRender_obj>)
set_target_properties(ribRender_static PROPERTIES OUTPUT_NAME RIBRender)
target_link_libraries(ribRender_static INTERFACE libshader_shading ${openrendertiff_libs} openrendercommon)
if(WIN32)
    target_link_libraries(ribRender_static INTERFACE zlib ws2_32)
endif()
if(UNIX)
    target_link_libraries(ribRender_static INTERFACE z)
endif()

# libri compatibility alias: a symlink next to the real libRIBRender artifact
# (both in the build tree and once installed), for anything not yet updated
# to the new name. Points at the unversioned dev name, which itself resolves
# through CMake's own VERSION/SOVERSION symlink chain for the shared library.
add_custom_command(TARGET ribRender POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
        "${CMAKE_SHARED_LIBRARY_PREFIX}RIBRender${CMAKE_SHARED_LIBRARY_SUFFIX}"
        "${CMAKE_SHARED_LIBRARY_PREFIX}ri${CMAKE_SHARED_LIBRARY_SUFFIX}"
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:ribRender>"
    COMMENT "Creating libri -> libRIBRender compatibility alias"
    VERBATIM
)
add_custom_command(TARGET ribRender_static POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
        "${CMAKE_STATIC_LIBRARY_PREFIX}RIBRender${CMAKE_STATIC_LIBRARY_SUFFIX}"
        "${CMAKE_STATIC_LIBRARY_PREFIX}ri${CMAKE_STATIC_LIBRARY_SUFFIX}"
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:ribRender_static>"
    COMMENT "Creating libri.a -> libRIBRender.a compatibility alias"
    VERBATIM
)

install(TARGETS ribRender
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS ribRender_static ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES dlo.h dsply.h implicit.h ptcapi.h ri.h shadeop.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# Recreate the libri alias at install time too -- install(TARGETS) only installs
# the real files CMake generated for ribRender/ribRender_static (including its
# own SONAME symlink chain), not the extra alias created above by POST_BUILD.
install(CODE
    "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
        \"${CMAKE_SHARED_LIBRARY_PREFIX}RIBRender${CMAKE_SHARED_LIBRARY_SUFFIX}\"
        \"${CMAKE_SHARED_LIBRARY_PREFIX}ri${CMAKE_SHARED_LIBRARY_SUFFIX}\"
     WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")"
)
install(CODE
    "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
        \"${CMAKE_STATIC_LIBRARY_PREFIX}RIBRender${CMAKE_STATIC_LIBRARY_SUFFIX}\"
        \"${CMAKE_STATIC_LIBRARY_PREFIX}ri${CMAKE_STATIC_LIBRARY_SUFFIX}\"
     WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")"
)
