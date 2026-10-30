# openRenderPublicHeaders.cmake -- the public (installed) header list (spec 037, FR-048).
#
# OPENRENDER_PUBLIC_HEADERS holds the source path of every header the install step puts in
# ${CMAKE_INSTALL_INCLUDEDIR}. src/ri/CMakeLists.txt installs exactly this list, the install-list
# check compares the installed names with it, and the guides name no header that is not in it.
# A header that moves inside the tree updates its entry here and nowhere else.
#
# file_base.h is not in the list: it is an in-tree helper of the in-tree file drivers. (Until Phase 7
# removes it, src/display/file/CMakeLists.txt still installs it.)
set(OPENRENDER_PUBLIC_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/../src/ri/parse/ri.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/ri/texture/ptcapi.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/display/dsply.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/ri/render/shadeop.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/procedures/dlo.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/procedures/implicit.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/include/orplugin.h
    ${CMAKE_CURRENT_LIST_DIR}/../src/include/orplugin.hpp)

# orplugin.h and orplugin.hpp live in src/include/, which the root CMakeLists.txt puts on the include path of
# every target (the tree-wide include path replaces the build-tree copy an earlier revision made), so the family
# headers' `#include "orplugin.h"` resolves in the tree as it does for a plug-in author.
