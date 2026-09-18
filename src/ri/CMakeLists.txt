file(GLOB ri_headers *.h hiders/*.h render/*.h state/*.h parse/*.h)

# for flex and bison files that depend on ri/ files
include_directories(.)
# hiders/, render/, state/ and parse/ are domains split out of the
# historically flat src/ri/ tree (see the domain-split plan) -- kept on the
# same flat include path as "." so every #include "reyes.h"/"renderer.h"/
# "attributes.h"/"ri.h" style reference elsewhere in the tree keeps working
# unqualified, exactly like "." itself already does. state/ (attributes,
# options, xform, displayChannel) and parse/ (RIB/DSO/variable/ifexpr
# grammars, ri.cpp's Ri*() dispatch, CRiInterface, error reporting) are not
# ribRender-only the way hiders/render are -- both are shared foundation
# future ribVector/ribOut will also need.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/hiders)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/render)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/state)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/parse)
# Allow ri/ files to include shading engine headers from their new location.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading)

set(ri_sources
	state/attributes.cpp
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
	state/displayChannel.cpp
	dlobject.cpp
#	dso.cpp
	parse/error.cpp
	fileResource.cpp
	hcshader.cpp
	hiders/irradiance.cpp
	hiders/occlusion.cpp
	hiders/photon.cpp
	hiders/photonMap.cpp
	hiders/raytracer.cpp
	hiders/reyes.cpp
	hiders/sampler.cpp
	hiders/stochastic.cpp
	hiders/trace.cpp
	hiders/zbuffer.cpp
#	ifexpr.cpp
	imager.cpp
	implicitSurface.cpp
	init.cpp
	linsys.cpp
	memory.cpp
	object.cpp
	state/options.cpp
	patches.cpp
	patchgrid.cpp
	pl.cpp
	points.cpp
	pointCloud.cpp
	pointHierarchy.cpp
	polygons.cpp
	ptcapi.cpp
	quadrics.cpp
	random.cpp
	refCounter.cpp
	remoteChannel.cpp
	render/geometryDispatch.cpp
	render/renderer.cpp
	render/rendererClipping.cpp
	render/rendererContext.cpp
	render/rendererDeclarations.cpp
	render/rendererDisplay.cpp
	render/rendererFiles.cpp
	render/rendererJobs.cpp
	render/rendererMutexes.cpp
	render/rendererNetwork.cpp
	render/rendererShadingServices.cpp
	resource.cpp
	parse/ri.cpp
#	rib.cpp
	parse/riInterface.cpp
	ribOut.cpp
#	sl.cpp
	solidObject.cpp
	stats.cpp
	subdivision.cpp
	subdivisionCreator.cpp
	subdivisionHierarchical.cpp
	subdivisionLoop.cpp
	surface.cpp
	texmake.cpp
	texture.cpp
	texture3d.cpp
#	variable.cpp
	state/xform.cpp
)

# this is to make sure generated bison cpp's include the correct lex.xxx.cpp
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR})

# define flex targets
# Inputs moved to parse/ (see the domain-split plan); outputs still land
# flat in CMAKE_CURRENT_BINARY_DIR regardless -- FLEX_FILE/BISON_FILE
# (CMake/openRenderUse{Flex,Bison}.cmake) hardcode PATH_OPT to "", so the
# generated lex.*.cpp/*_generated.cpp names and ri_sources entries below
# are unaffected by this move.
flex_file(parse/dso.l lex.dso.cpp dso)
flex_file(parse/rib.l lex.rib.cpp rib)
flex_file(parse/variable.l lex.var.cpp var)
flex_file(parse/ifexpr.l lex.if.cpp if)

# define bison targets (they depend on flex ones, via prefix - third arg)
bison_file(parse/dso.y dso_generated.cpp dso)
bison_file(parse/rib.y rib_generated.cpp rib)
bison_file(parse/variable.y variable_generated.cpp var)
bison_file(parse/ifexpr.y ifexpr_generated.cpp if)

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hiders>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/render>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/state>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/parse>
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
