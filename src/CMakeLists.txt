include_directories(.)
# ri/ headers are included by many targets via renderer.h/options.h/shader.h chains.
# After Phase C1 the shading engine headers (shader.h, shading.h, ...) moved to
# libshader/shading/ but still include ri/ files without prefix.  Adding ri/ here
# makes those transitive includes work for all targets in the project.
include_directories(ri)
# hiders/ and render/ are the first two domains split out of the historically
# flat ri/ tree (see the domain-split plan; renderer.h in particular is one of
# the widely-included headers the comment above already calls out). Flat
# alongside ri/ itself for the same reason: every #include "reyes.h" /
# #include "renderer.h" elsewhere in the tree keeps resolving unqualified.
include_directories(ri/hiders)
include_directories(ri/render)
include_directories(libshader/shading)

add_subdirectory(common)
add_subdirectory(file)
add_subdirectory(framebuffer)
add_subdirectory(openexr)
add_subdirectory(precomp)
add_subdirectory(rgbe)
add_subdirectory(libshader)  # RSL shader library (compiler + runtime + shading engine)
add_subdirectory(ri)
# Phase C: libshader_shading is a real target; ribRender links against it.
# These aliases let existing consumers link 'libshader' without knowing about ribRender.
add_library(libshader        ALIAS ribRender)
add_library(libshader_static ALIAS ribRender_static)
add_subdirectory(preview)
add_subdirectory(orender)
add_subdirectory(oshader)
add_subdirectory(rslo)
add_subdirectory(sloinfo)
add_subdirectory(otexmake)
add_subdirectory(python)
add_subdirectory(lua)
