# Cross-family mechanical checks (spec 037): cmake -P scripts, registered as ordinary ctest tests
# so a red check never breaks the -Werror main build.
#
#   PluginAbi_DocPaths               FR-036  stale path references in the guides
#   ShadeopAbi_ParserSingleDefiner   FR-037  dsoParse is compiled into four libraries
#   PluginAbi_InstallHeaders         FR-038  an installed header includes an uninstalled one

# The public (installed) header set of the STARTING tree: a hand copy of the install(FILES ...)
# list of src/ri/CMakeLists.txt (dlo.h dsply.h implicit.h ptcapi.h ri.h shadeop.h) plus file_base.h
# from src/file's CMakeLists. Names, not paths: stage_headers.cmake finds each in the directories
# the headers have lived in, so the check keeps working while headers move. Phase 7 replaces this
# variable with the real installed list.
set(OPENRENDER_PUBLIC_HEADER_NAMES dlo.h dsply.h implicit.h ptcapi.h ri.h shadeop.h file_base.h)
string(REPLACE ";" "|" _header_names "${OPENRENDER_PUBLIC_HEADER_NAMES}")

# The toolchain flags the compile checks use: the project's C++ flags, and on macOS the SDK.
set(_check_flags "${CMAKE_CXX_FLAGS}")
if(CMAKE_OSX_SYSROOT)
    set(_check_flags "${_check_flags} -isysroot ${CMAKE_OSX_SYSROOT}")
endif()

# FR-036 -- every path reference in the guides must exist. PLUGIN_ABI.md and the site page are
# listed once they exist (Phase 7).
set(_doc_list DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md INSTALL_ARTIFACTS.md)
foreach(_doc DEVNOTES_DETAILS/PLUGIN_ABI.md docs/site/content/manual/reference/plugin-abi.md)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${_doc})
        list(APPEND _doc_list ${_doc})
    endif()
endforeach()
string(REPLACE ";" "|" _docs "${_doc_list}")
or_plugin_add_test(PluginAbi_DocPaths 60
    COMMAND ${CMAKE_COMMAND}
        "-DDOCS=${_docs}"
        "-DHEADERS=${_header_names}"
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DSTAGE=${OR_PLUGIN_BIN}/checks/stage_docpaths
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_doc_paths.cmake)

# FR-037 -- dsoParse is defined in exactly one library. The path list is built from the targets.
or_plugin_add_test(ShadeopAbi_ParserSingleDefiner 60
    COMMAND ${CMAKE_COMMAND}
        -DSYMBOL=dsoParse
        "-DLIBRARIES=$<TARGET_FILE:ribRender>|$<TARGET_FILE:ribVector>|$<TARGET_FILE:ribOut>|$<TARGET_FILE:libshader_compiler>"
        -DMODE=single
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_single_definer.cmake)

# FR-038 -- every installed header compiles alone and includes only installed headers.
or_plugin_add_test(PluginAbi_InstallHeaders 120
    COMMAND ${CMAKE_COMMAND}
        "-DHEADERS=${_header_names}"
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DSTAGE=${OR_PLUGIN_BIN}/checks/stage_install
        -DCXX=${CMAKE_CXX_COMPILER}
        "-DFLAGS=${_check_flags}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_install_headers.cmake)
