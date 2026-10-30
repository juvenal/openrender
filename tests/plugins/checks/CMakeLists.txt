# Cross-family mechanical checks (spec 037): cmake -P scripts, registered as ordinary ctest tests
# so a red check never breaks the -Werror main build.
#
#   PluginAbi_DocPaths               FR-036  stale path references in the guides
#   ShadeopAbi_ParserSingleDefiner   FR-037  dsoParse is compiled into four libraries
#   PluginAbi_InstallHeaders         FR-038  an installed header includes an uninstalled one; each C header
#                                            also compiles alone as strict C17 (Phase 2)
#   PluginAbi_VersionHistory         FR-001, FR-047  every family header declares its version and a history
#   PluginAbi_SingleCheckRoutine     SC-008  checkPluginVersion has one definer (DISABLED until T093)

# The public (installed) header set of the STARTING tree: a hand copy of the install(FILES ...)
# list of src/ri/CMakeLists.txt (dlo.h dsply.h implicit.h ptcapi.h ri.h shadeop.h) plus file_base.h
# from src/file's CMakeLists. Names, not paths: stage_headers.cmake finds each in the directories
# the headers have lived in, so the check keeps working while headers move. Phase 7 replaces this
# variable with the real installed list.
set(OPENRENDER_PUBLIC_HEADER_NAMES dlo.h dsply.h implicit.h ptcapi.h ri.h shadeop.h file_base.h)
# orplugin.h and orplugin.hpp (Phase 2) join the set once they exist: the family headers include orplugin.h,
# so an install set without it is not self-contained.
foreach(_header orplugin.h orplugin.hpp)
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/include/${_header})
        list(APPEND OPENRENDER_PUBLIC_HEADER_NAMES ${_header})
    endif()
endforeach()
string(REPLACE ";" "|" _header_names "${OPENRENDER_PUBLIC_HEADER_NAMES}")

# The toolchain flags the compile checks use: the project's C++ flags, and on macOS the SDK.
set(_check_flags "${CMAKE_CXX_FLAGS}")
set(_check_c_flags "${CMAKE_C_FLAGS}")
if(CMAKE_OSX_SYSROOT)
    set(_check_flags "${_check_flags} -isysroot ${CMAKE_OSX_SYSROOT}")
    set(_check_c_flags "${_check_c_flags} -isysroot ${CMAKE_OSX_SYSROOT}")
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

# FR-012, FR-037 (T064) -- the new parser has one definer, libRIBShadeop. DISABLED until T069 creates the
# library (the path of a library that does not exist is a PRECONDITION failure of the script).
if(EXISTS ${CMAKE_SOURCE_DIR}/src/shadeop/CMakeLists.txt)
    set(_parser_new_library "$<TARGET_FILE:openrendershadeop>")
else()
    set(_parser_new_library "${CMAKE_BINARY_DIR}/src/shadeop/libRIBShadeop-not-built")
endif()
or_plugin_add_test(ShadeopAbi_ParserNewSymbol 60
    COMMAND ${CMAKE_COMMAND}
        -DSYMBOL=parseShadeopDeclaration
        "-DLIBRARIES=${_parser_new_library}|$<TARGET_FILE:ribRender>|$<TARGET_FILE:ribVector>|$<TARGET_FILE:ribOut>|$<TARGET_FILE:libshader_compiler>"
        -DMODE=only
        -DONLY_IN=RIBShadeop
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_single_definer.cmake)
set_tests_properties(ShadeopAbi_ParserNewSymbol PROPERTIES DISABLED TRUE)

# FR-038 -- every installed header compiles alone and includes only installed headers.
or_plugin_add_test(PluginAbi_InstallHeaders 120
    COMMAND ${CMAKE_COMMAND}
        "-DHEADERS=${_header_names}"
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DSTAGE=${OR_PLUGIN_BIN}/checks/stage_install
        -DCXX=${CMAKE_CXX_COMPILER}
        "-DFLAGS=${_check_flags}"
        -DCC=${CMAKE_C_COMPILER}
        "-DC_FLAGS=${_check_c_flags}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_install_headers.cmake)

# FR-001, FR-047 -- every family header declares its version and documents a history; enabled and RED until
# T041 to T043 (dsply.h joins in T085). The header directories are those of the tree at configure time.
set(_history_dirs "")
foreach(_dir src/include src/ri/render src/ri/geometry src/display src/shadeop src/procedures)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${_dir})
        list(APPEND _history_dirs ${CMAKE_SOURCE_DIR}/${_dir})
    endif()
endforeach()
string(REPLACE ";" "|" _history_dirs "${_history_dirs}")
or_plugin_add_test(PluginAbi_VersionHistory 60
    COMMAND ${CMAKE_COMMAND}
        "-DHEADERS=shadeop.h:SHADEOP|dlo.h:DLO,OR_DYNAMICLOAD|implicit.h:IMPLICIT"
        "-DINCLUDE_DIRS=${_history_dirs}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_version_history.cmake)

# SC-008 -- one version-check routine. DISABLED until T093: the display host does not exist before Phase 5.
# References: the libraries that call it (the renderer core holds the display host and the procedures
# objects; the shadeop library joins when the target exists).
set(_check_referencers "$<TARGET_FILE:ribRender>")
set(_check_others "$<TARGET_FILE:ribVector>|$<TARGET_FILE:ribOut>|$<TARGET_FILE:libshader_compiler>|$<TARGET_FILE:libshader_shading>")
# (tests/ is configured before src/, so TARGET would not see the library yet: test for its directory)
if(EXISTS ${CMAKE_SOURCE_DIR}/src/shadeop/CMakeLists.txt)
    set(_check_referencers "${_check_referencers}|$<TARGET_FILE:openrendershadeop>")
endif()
or_plugin_add_test(PluginAbi_SingleCheckRoutine 60
    COMMAND ${CMAKE_COMMAND}
        -DSYMBOL=checkPluginVersion
        -DDEFINER=$<TARGET_FILE:openrendercommon>
        "-DOTHERS=${_check_others}"
        "-DREFERENCERS=${_check_referencers}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_single_routine.cmake)
set_tests_properties(PluginAbi_SingleCheckRoutine PROPERTIES DISABLED TRUE)

# SC-008 -- osLoadModule is called only from the modules that own plug-in loading. Enabled and RED until
# Phase 5 (displayHost.cpp): the starting tree has six other call sites.
or_plugin_add_test(ProceduresAbi_NoLoadOutsideModule 60
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        "-DALLOWED=proceduralModules.cpp|shadeopModule.cpp|displayHost.cpp"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_load_outside.cmake)
