file(GLOB ri_headers *.h hiders/*.h render/*.h state/*.h parse/*.h geometry/*.h texture/*.h output/*.h core/*.h dataviewer/*.h)

# for flex and bison files that depend on ri/ files
include_directories(.)
# hiders/, render/, state/, parse/, geometry/, texture/ and output/ are
# domains split out of the historically flat src/ri/ tree (see the
# domain-split plan) -- kept on the same flat include path as "." so every
# #include "reyes.h"/"renderer.h"/"attributes.h"/"ri.h" style reference
# elsewhere in the tree keeps working unqualified, exactly like "." itself
# already does. state/ (attributes, options, xform, displayChannel) and
# parse/ (RIB/DSO/variable/ifexpr grammars, ri.cpp's Ri*() dispatch,
# CRiInterface, error reporting) are not ribRender-only the way hiders/
# render are -- both are shared foundation ribVector also needs. geometry/
# (object/curves/points/polygons/quadrics/patches/patchgrid/
# bsplinePatchgrid/surface/implicitSurface/delayed/solidObject/csgTree/
# csgBoolean/csgTesselationDispatch/subdivision family/blobby family) and
# texture/ (texture/texture3d/texmake/brickmap/pointCloud/pointHierarchy)
# are shared foundation too -- both ribRender and ribVector need most of
# this content, just not always the same methods (see the real/stub
# extractions above). output/ (ribOut) has no CMake target of its own yet;
# it's still compiled as a plain source file into both ribRender and
# ribVector -- carving out a real, minimal ribOut target is a separate,
# larger undertaking left for later.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/hiders)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/render)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/state)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/parse)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/geometry)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/texture)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/output)
# core/ (memory, refCounter, stats, random, pl, fileResource, atomic,
# ray/map/noise, ri_config, and the Phase C1 shader/shading/shaderPl
# forwarding shims) and dataviewer/ (dataLoad/dataView/debug -- the
# orender-wire data-file inspection helpers) are two more domains split out
# of the historically flat src/ri/ tree, same flat-include treatment as the
# rest.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dataviewer)
# Allow ri/ files to include shading engine headers from their new location.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading)
# dsply.h (the Display-driver ABI) lives in the sibling src/display/ tree
# (see src/display/CMakeLists.txt); state/options.h includes it unqualified.
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../display)

set(ri_sources
	state/attributes.cpp
	geometry/blobby.cpp
	geometry/blobbyField.cpp
	geometry/blobbyPolygonize.cpp
	geometry/blobbyRepeller.cpp
	texture/brickmap.cpp
	texture/imageInput.cpp
	texture/imageInputExr.cpp
	texture/imageInputPng.cpp
	texture/imageInputRgbe.cpp
	texture/imageInputTiff.cpp
	../display/rgbe/rgbe.cpp
	hiders/bundles.cpp
	hiders/compositor.cpp
	geometry/curves.cpp
	geometry/bsplinePatchgrid.cpp
	geometry/csgBoolean.cpp
	geometry/csgTesselationDispatch.cpp
	geometry/csgTree.cpp
	dataviewer/dataLoad.cpp
	dataviewer/dataView.cpp
	dataviewer/debug.cpp
	geometry/delayed.cpp
	state/displayChannel.cpp
	geometry/dlobject.cpp
#	dso.cpp
	parse/error.cpp
	core/fileResource.cpp
	hcshader.cpp
	hiders/irradiance.cpp
	hiders/irradianceDispatch.cpp
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
	render/imager.cpp
	geometry/implicitSurface.cpp
	render/init.cpp
	hiders/linsys.cpp
	core/memory.cpp
	geometry/object.cpp
	state/options.cpp
	geometry/patches.cpp
	geometry/patchgrid.cpp
	core/pl.cpp
	geometry/points.cpp
	texture/pointCloud.cpp
	texture/pointHierarchy.cpp
	geometry/polygons.cpp
	texture/ptcapi.cpp
	geometry/quadrics.cpp
	core/random.cpp
	core/refCounter.cpp
	render/remoteChannel.cpp
	render/geometryDispatch.cpp
	render/renderer.cpp
	render/rendererClipping.cpp
	render/rendererContext.cpp
	render/rendererDeclarations.cpp
	render/rendererDeclarationsDispatch.cpp
	render/rendererDisplay.cpp
	render/rendererFiles.cpp
	render/rendererFileSearch.cpp
	render/rendererJobs.cpp
	render/rendererMutexes.cpp
	render/rendererNetwork.cpp
	render/rendererShadingServices.cpp
	render/rendererStatics.cpp
	render/resource.cpp
	parse/ri.cpp
#	rib.cpp
	parse/riInterface.cpp
	output/ribOut.cpp
#	sl.cpp
	geometry/solidObject.cpp
	core/stats.cpp
	geometry/subdivision.cpp
	geometry/subdivisionCreator.cpp
	geometry/subdivisionHierarchical.cpp
	geometry/subdivisionLoop.cpp
	geometry/surface.cpp
	texture/texmake.cpp
	texture/texture.cpp
	texture/texture3d.cpp
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

# spec 018 (multi-format texture source decoding): HAVE_OPENEXR and
# OPENRENDER_OPENEXR_LIBS (root CMakeLists.txt) previously only fed the
# openexr *display*-driver module. texture/imageInputExr.cpp is now also
# part of ri_sources (added unconditionally, guarded internally by
# #ifdef HAVE_OPENEXR so it's an empty translation unit when the macro isn't
# defined) -- but the macro itself, and the OpenEXR link dependency, are
# defined on ribRender_obj/ribRender/ribRender_static ONLY, not
# ribVector_obj/ribVector. ribVector (orender-wire's lightweight preview
# lib, deliberately "no shading, no LLVM") has no use for source-image
# decoding and should not gain an OpenEXR dependency just because it
# compiles texmake.cpp/imageInput.cpp too -- unlike TIFF/PNG/RGBE (already
# zero-cost there), OpenEXR is a real, optional external dependency this
# target should stay free of.
if(HAVE_OPENEXR)
    target_compile_definitions(ribRender_obj PRIVATE HAVE_OPENEXR)
    target_link_libraries(ribRender_obj PRIVATE ${OPENRENDER_OPENEXR_LIBS})
endif()

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/geometry>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/texture>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/output>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dataviewer>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../display>
)
target_link_libraries(ribRender PRIVATE libshader_shading ${openrendertiff_libs} openrendercommon)
if(HAVE_OPENEXR)
    target_link_libraries(ribRender PRIVATE ${OPENRENDER_OPENEXR_LIBS})
endif()
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
if(HAVE_OPENEXR)
    target_link_libraries(ribRender_static INTERFACE ${OPENRENDER_OPENEXR_LIBS})
endif()
if(WIN32)
    target_link_libraries(ribRender_static INTERFACE zlib ws2_32)
endif()
if(UNIX)
    target_link_libraries(ribRender_static INTERFACE z)
endif()

# ribVector: geometry-only extraction for orender-wire (see the domain-split
# plan) -- the same ri_sources list minus every file confirmed, via an
# `nm -u` sweep over ribRender_obj's built object files (not by reading
# source -- see geometryDispatch.cpp's and rendererDeclarationsDispatch.cpp's
# header comments for what that sweep found), to still reach
# CShadingContext/CReyes after the real/stub extractions above: the Reyes/
# raytrace/photon/stochastic/zbuffer/occlusion/irradiance/sampler/trace
# hiders, CPatch/CTesselationPatch's own adaptive-tessellation machinery
# (surface.cpp), the imager/DL-object/implicit-surface/hierarchical-cache-
# shader features (dlobject.cpp, hcshader.cpp, imager.cpp,
# implicitSurface.cpp -- none of which orender-wire's preview path ever
# constructs), rendererShadingServices.cpp, and CRenderer's actual render
# pipeline (renderer.cpp, confirmed dead code here by tracing
# ribpreview_load()'s call graph: RiBeginLite() -> RiInit(), which never
# touches CRenderer -- ribVector needs only its static data, split out to
# rendererStatics.cpp). Also excludes rendererContext.cpp: CRendererContext
# is the *other* CRiInterface implementation (the one orender's real
# RiBegin() path uses), never constructed by ribpreview_load() (which uses
# CPreviewContext, in libribpreview, instead) -- but its own compiled
# method bodies (RiWorldBegin/RiWorldEnd/RiGeometryV/getShader/etc.) call
# straight into CRenderer::beginFrame()/render()/etc. and construct
# CProgrammableShaderInstance/CShader/CDLObject/CImplicit/CPhotonMap/
# CQuadLight/CSphereLight, so linking it in would silently reintroduce most
# of what everything else in this list was excluded to avoid. geometryDispatch.cpp and
# rendererDeclarationsDispatch.cpp are swapped for their vector/*Stub.cpp
# counterparts rather than merely removed, since those methods are reached
# for real (or vtable-required) even in a build with no rendering pipeline.
#
# The six entries below were confirmed excludable by a second `nm -u` sweep
# after the above: init.cpp is a single function, CRendererContext::init(),
# dead for the same reason rendererContext.cpp is. render/rendererFiles.cpp
# (CRenderer::getShader/getPhotonMap/getCache, LLVM JIT shader loading) and
# render/rendererDisplay.cpp (CRenderer::dispatch() -> CImagerExecutor) are
# both full-pipeline-only, reached only from beginFrame()/render(), which
# ribpreview_load() never calls. csgTree.cpp and solidObject.cpp are RIB
# `Solid`/CSG support: CRibGeometryContext has no RiSolidBegin/RiSolidEnd
# override at all, so CSG is structurally unsupported on the preview path,
# and CSolidObject is constructed only from csgTree.cpp (confirmed by
# grepping every "new CSolidObject"/"CSolidObject(" site repo-wide) -- so
# excluding csgTree.cpp makes solidObject.cpp dead too. parse/ifexpr_generated.cpp
# (the RIB `if()`-conditional grammar) defines CRendererContext::ifParse(),
# called only from the two RiIfBeginV/RiElseIfV sites in the already-excluded
# rendererContext.cpp -- confirmed by grepping every ifParse call site
# repo-wide, so despite being a *separate* grammar file from the main RIB
# parser, it's exclusively a CRendererContext feature and drops out cleanly
# with it (verified this is NOT the same situation as rib_generated.cpp's own
# direct CRenderer::context->getAttributes() calls, which were load-bearing
# for PatchMesh/Curves parsing and got a real, generic fix instead -- see the
# CRiInterface::getAttributes() virtual and rib.y's getBasisSteps()).
#
# A third sweep (after the above) found remoteChannel.cpp and
# render/rendererJobs.cpp: the former's CRemoteChannel-derived classes
# (CRemoteICacheChannel/CRemoteTSMChannel/CRemotePtCloudChannel) are
# constructed only from rendererDisplay.cpp/rendererFiles.cpp (both already
# excluded, confirmed by grepping every "new CRemote*"/"CRemoteChannel("
# site repo-wide), and the latter is pure distributed-render job dispatch
# (bucket assignment, network server thread) -- neither is ever touched by
# orender-wire's preview path. That sweep also surfaced CRenderer::
# locateFile()/normalizeFileName() as a real gap: they're plain filesystem/
# network path-search helpers with no shading-engine dependency of their
# own, but they lived in render/rendererFiles.cpp alongside the genuinely
# full-pipeline-only getShader/getPhotonMap/etc. -- and they're actually
# needed (blobbyRepeller.cpp, texmake.cpp, texture.cpp, brickmap.cpp, and
# rib_generated.cpp's RiReadArchiveV archive-path search all call
# locateFile() directly). Moved to render/rendererFileSearch.cpp, compiled
# identically for both ribRender and ribVector -- no real/stub split needed,
# since there's nothing shading-specific to swap out.
#
# spec 018 (multi-format texture source decoding): texture/imageInput.cpp
# and texture/imageInputTiff.cpp stay in ribVector_sources for the same
# reason texmake.cpp itself does -- texmake.cpp unconditionally calls
# createImageInput() and dynamic_casts to CTiffImageInput now, so any target
# compiling texmake.cpp (ribVector included, even though it never calls
# makeTexture() et al. at runtime) must link those symbols too. This is
# zero-cost for TIFF/PNG-backed decoders since ribVector_obj already links
# TIFF+PNG via openrendertiff_libs; an OpenEXR-backed decoder (added in a
# later spec 018 phase) is instead kept out of ribVector by defining
# HAVE_OPENEXR only on ribRender_obj's compile definitions, not
# ribVector_obj's -- not by excluding the shared source file, since
# imageInput.cpp itself must stay linked into both.
set(ribVector_sources ${ri_sources})
list(REMOVE_ITEM ribVector_sources
    geometry/csgTesselationDispatch.cpp
    geometry/csgTree.cpp
    geometry/dlobject.cpp
    hcshader.cpp
    hiders/irradianceDispatch.cpp
    hiders/occlusion.cpp
    hiders/photon.cpp
    hiders/raytracer.cpp
    hiders/reyes.cpp
    hiders/sampler.cpp
    hiders/stochastic.cpp
    hiders/trace.cpp
    hiders/zbuffer.cpp
    ifexpr_generated.cpp
    render/imager.cpp
    geometry/implicitSurface.cpp
    render/init.cpp
    render/remoteChannel.cpp
    render/geometryDispatch.cpp
    render/renderer.cpp
    render/rendererContext.cpp
    render/rendererDeclarationsDispatch.cpp
    render/rendererDisplay.cpp
    render/rendererFiles.cpp
    render/rendererJobs.cpp
    render/rendererShadingServices.cpp
    geometry/solidObject.cpp
    geometry/surface.cpp
)
list(APPEND ribVector_sources
    vector/csgTesselationDispatchStub.cpp
    vector/geometryDispatchStub.cpp
    vector/irradianceDispatchStub.cpp
    vector/rendererDeclarationsDispatchStub.cpp
)

add_library(ribVector_obj OBJECT ${ribVector_sources} ${ri_headers})
set_target_properties(ribVector_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ribVector_obj PRIVATE . ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading)
target_link_libraries(ribVector_obj PRIVATE openrendercommon ${openrendertiff_libs})

# Deliberately no libshader_shading, no LLVM: that is the entire point of this
# target existing separately from ribRender.
add_library(ribVector SHARED $<TARGET_OBJECTS:ribVector_obj>)
target_include_directories(ribVector PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hiders>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/render>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/state>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/parse>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/geometry>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/texture>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/output>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dataviewer>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../display>
)
target_link_libraries(ribVector PRIVATE ${openrendertiff_libs} openrendercommon)
if(WIN32)
    target_link_libraries(ribVector PRIVATE zlib ws2_32)
endif()
if(UNIX)
    target_link_libraries(ribVector PRIVATE z)
endif()
set_target_properties(ribVector PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${OPENRENDER_COMPAT_SOVERSION}
)
if(APPLE)
    if(INSTALL_SELFCONTAINED)
        set_target_properties(ribVector PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(ribVector PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
endif()

# ribOut: RIB-text-output extraction (see the domain-split plan) -- the third
# and final CRiInterface implementation, needing even less than ribVector.
# CRibOut (output/ribOut.cpp) never constructs a single geometry class: it
# just re-serializes every Ri*() call it receives as RIB text, using its own
# internal CRibAttributes for uStep/vStep tracking rather than the real
# CAttributes/CXform state ribVector's CRibGeometryContext needs. So unlike
# ribVector (built by excluding a handful of files from the full ri_sources
# list), ribOut_sources is a small explicit whitelist -- deriving it by
# exclusion would mean excluding nearly everything (all of geometry/,
# texture/, hiders/, plus dlobject/hcshader/imager/implicitSurface/
# remoteChannel/state/*) to reach the same handful of files.
#
# ribOut needs the shared RIB-parsing grammar (parse/ri.cpp's RiBegin/RiEnd
# dispatch and factory-hook system in riHooks.h, parse/riInterface.cpp for
# the CRiInterface base, parse/error.cpp, and the bison/flex-generated
# parsers) because orender's "-ribout" mode works by parsing an input RIB
# file through the exact same grammar ribRender/ribVector use, with a
# CRibOut installed as the active renderMan instead of a CRendererContext/
# CPreviewContext -- riHooks.h's own header comment documents this
# self-registration design goal explicitly (CRibOut registers its factory
# via a static initializer in ribOut.cpp itself; ri.cpp never names
# CRendererContext or CRibOut directly, precisely so a consumer that links
# only one of them never pulls in the others' dependencies).
#
# ifexpr_generated.cpp is excluded for the same reason it is for ribVector:
# it defines CRendererContext::ifParse(), called only from the two
# RiIfBeginV/RiElseIfV sites in rendererContext.cpp, which ribOut doesn't
# link either.
#
# Note: CRibOut doesn't override getAttributes()/getOptions()/getXform() (it
# has no real CAttributes/COptions/CXform to return), so it inherits
# CRiInterface's NULL-returning defaults -- meaning rib.y's getBasisSteps()
# falls back to the RISpec default basis (bezier, step 3) while parsing
# through a CRibOut, same as ribVector, rather than reading back whatever
# RiBasis() the input RIB actually set. That's a pre-existing gap (the
# NULL-deref this session fixed for PatchMesh/Curves parsing existed for
# "-ribout" mode too, before today), not something this target-creation
# step introduces or fixes -- flagged as a follow-up, not fixed here.
set(ribOut_sources
    output/ribOut.cpp
    parse/error.cpp
    parse/ri.cpp
    parse/riInterface.cpp
    core/memory.cpp
    render/rendererDeclarations.cpp
    render/rendererFileSearch.cpp
    render/rendererMutexes.cpp
    render/rendererNetwork.cpp
    render/rendererStatics.cpp
    state/displayChannel.cpp
    core/stats.cpp
    vector/rendererDeclarationsDispatchStub.cpp
    dso_generated.cpp
    rib_generated.cpp
    variable_generated.cpp
)

add_library(ribOut_obj OBJECT ${ribOut_sources} ${ri_headers})
set_target_properties(ribOut_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ribOut_obj PRIVATE . ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading)
target_link_libraries(ribOut_obj PRIVATE openrendercommon ${openrendertiff_libs})

# Deliberately no libshader_shading, no LLVM, no geometry/texture/hiders: same
# rationale as ribVector.
add_library(ribOut SHARED $<TARGET_OBJECTS:ribOut_obj>)
target_include_directories(ribOut PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/hiders>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/render>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/state>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/parse>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/geometry>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/texture>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/output>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dataviewer>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../libshader/shading>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../display>
)
target_link_libraries(ribOut PRIVATE ${openrendertiff_libs} openrendercommon)
if(WIN32)
    target_link_libraries(ribOut PRIVATE zlib ws2_32)
endif()
if(UNIX)
    target_link_libraries(ribOut PRIVATE z)
endif()
set_target_properties(ribOut PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${OPENRENDER_COMPAT_SOVERSION}
)
if(APPLE)
    if(INSTALL_SELFCONTAINED)
        set_target_properties(ribOut PROPERTIES INSTALL_NAME_DIR "@rpath")
    else()
        set_target_properties(ribOut PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
    endif()
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
install(FILES
    geometry/dlo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../display/dsply.h
    geometry/implicit.h
    texture/ptcapi.h
    parse/ri.h
    render/shadeop.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

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
