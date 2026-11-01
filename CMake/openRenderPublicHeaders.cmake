# openRenderPublicHeaders.cmake -- the public (installed) header list (spec 037, FR-048).
#
# OPENRENDER_PUBLIC_HEADERS holds the source path of every header the install step puts in
# ${CMAKE_INSTALL_INCLUDEDIR}. src/ri/CMakeLists.txt installs exactly this list, the install-list
# check compares the installed names with it, and the guides name no header that is not in it.
# A header that moves inside the tree updates its entry here and nowhere else.
#
# file_base.h is not in the list and no install rule names it: it is an in-tree helper of the in-tree file
# drivers. rslo.h (the .rslo/.slo loader API of libshader_runtime) is in the list: it is installed and documented.
set(OPENRENDER_PUBLIC_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/../src/ri/parse/ri.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/ri/texture/ptcapi.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/display/dsply.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/shadeop/shadeop.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/procedures/dlo.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/procedures/implicit.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/include/orplugin.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/include/orplugin.hpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/libshader/runtime/rslo.h)

# orplugin.h and orplugin.hpp live in src/include/, which the root CMakeLists.txt puts on the include path of
# every target (the tree-wide include path replaces the build-tree copy an earlier revision made), so the family
# headers' `#include "orplugin.h"` resolves in the tree as it does for a plug-in author.

# The include directory of each install layout (spec 037, FR-026, FR-048), relative to the install prefix: the
# self-contained layout (INSTALL_SELFCONTAINED=ON, <ORENDERHOME>/include) uses ${CMAKE_INSTALL_INCLUDEDIR}, the FHS
# layout (OFF) ${CMAKE_INSTALL_INCLUDEDIR}/openRender, so openRender's headers do not mix with the system's. The install
# rules, the INCLUDES DESTINATION of every exported target, the package config and the tests all use these.
set(OPENRENDER_INCLUDEDIR_SELFCONTAINED "${CMAKE_INSTALL_INCLUDEDIR}")
set(OPENRENDER_INCLUDEDIR_FHS "${CMAKE_INSTALL_INCLUDEDIR}/openRender")
if(INSTALL_SELFCONTAINED)
    set(OPENRENDER_INCLUDE_INSTALL_DIR "${OPENRENDER_INCLUDEDIR_SELFCONTAINED}")
else()
    set(OPENRENDER_INCLUDE_INSTALL_DIR "${OPENRENDER_INCLUDEDIR_FHS}")
endif()
