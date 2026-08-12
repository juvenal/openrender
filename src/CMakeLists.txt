include_directories(.)
# ri/ headers are included by many targets via renderer.h/options.h/shader.h chains.
# After Phase C1 the shading engine headers (shader.h, shading.h, ...) moved to
# libshader/shading/ but still include ri/ files without prefix.  Adding ri/ here
# makes those transitive includes work for all targets in the project.
include_directories(ri)
include_directories(libshader/shading)

add_subdirectory(common)
add_subdirectory(file)
add_subdirectory(framebuffer)
add_subdirectory(openexr)
add_subdirectory(precomp)
add_subdirectory(rgbe)
add_subdirectory(libshader)  # RSL shader library (compiler + runtime + shading engine)
add_subdirectory(ri)
# Phase C: libshader_shading is a real target; ri links against it.
# These aliases let existing consumers link 'libshader' without knowing about ri.
add_library(libshader        ALIAS ri)
add_library(libshader_static ALIAS ri_static)
add_subdirectory(preview)
add_subdirectory(orender)
add_subdirectory(oshader)
add_subdirectory(rslo)
add_subdirectory(sloinfo)
add_subdirectory(oshow)
add_subdirectory(otexmake)
add_subdirectory(python)
add_subdirectory(lua)
