# Cross-family mechanical checks (spec 037): cmake -P scripts, registered as ordinary ctest tests
# so a red check never breaks the -Werror main build.
#
#   PluginAbi_DocPaths               FR-036, FR-047, SC-020  stale path references and wrong version numbers in the guides, PLUGIN_ABI.md and the site page
#   ShadeopAbi_ParserSingleDefiner   FR-037  no library defines dsoParse (it was compiled into four)
#   ShadeopAbi_ParserNewSymbol       FR-037  parseShadeopDeclaration is defined only in libRIBShadeop
#   PluginAbi_InstallHeaders         FR-038, SC-011  the real header list staged for both layouts compiles alone
#                                            (C++20, C17) and the guide's example display builds and renders
#   PluginAbi_InstallList            FR-026, FR-048  install rules, INSTALL_ARTIFACTS.md and the guide name one list
#   PluginAbi_NoFileBaseInstalled    FR-026  no install() rule names file_base.h
#   PluginAbi_ShadeopSoversion       SC-019  libRIBShadeop carries OPENRENDER_COMPAT_SOVERSION
#   PluginAbi_InstallLayouts         SC-011, SC-019  scratch-prefix install of both layouts + a find_package consumer
#   PluginAbi_RegistryConsistency    FR-029, SC-020  DEVNOTES.md and DOMAIN_BOUNDARIES.md agree with the allowlist and the tree
#   PluginAbi_VersionHistory         FR-001, FR-047  every family header declares its version and a history
#   PluginAbi_SingleCheckRoutine     SC-008  checkPluginVersion has one definer (enabled by T093)

# The public (installed) header set: names, from the real OPENRENDER_PUBLIC_HEADERS
# (CMake/openRenderPublicHeaders.cmake), the one source of truth the install rules consume (FR-048). Names, not
# paths: stage_headers.cmake finds each in the directory it lives in.
set(OPENRENDER_PUBLIC_HEADER_NAMES "")
foreach(_path ${OPENRENDER_PUBLIC_HEADERS})
    get_filename_component(_name ${_path} NAME)
    list(APPEND OPENRENDER_PUBLIC_HEADER_NAMES ${_name})
endforeach()
string(REPLACE ";" "|" _header_names "${OPENRENDER_PUBLIC_HEADER_NAMES}")
# The include directory of each install layout, from the variables the install rules use.
set(_layouts "selfcontained=${OPENRENDER_INCLUDEDIR_SELFCONTAINED}|fhs=${OPENRENDER_INCLUDEDIR_FHS}")

# The toolchain flags the compile checks use: the project's C++ flags, and on macOS the SDK.
set(_check_flags "${CMAKE_CXX_FLAGS}")
set(_check_c_flags "${CMAKE_C_FLAGS}")
if(CMAKE_OSX_SYSROOT)
    set(_check_flags "${_check_flags} -isysroot ${CMAKE_OSX_SYSROOT}")
    set(_check_c_flags "${_check_c_flags} -isysroot ${CMAKE_OSX_SYSROOT}")
endif()

# FR-036, FR-047, SC-020 -- every path reference in the guides, PLUGIN_ABI.md and the site page must exist (a missing
# document is a failure), and every version number they state for an interface equals the header's (T108a).
set(_doc_list DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md INSTALL_ARTIFACTS.md DEVNOTES_DETAILS/PLUGIN_ABI.md
    docs/site/content/manual/reference/plugin-abi.md)
string(REPLACE ";" "|" _docs "${_doc_list}")
set(_versions "display=dsply.h:OR_DISPLAY|shadeop=shadeop.h:SHADEOP|dlo=dlo.h:DLO|dynamicload=dlo.h:OR_DYNAMICLOAD|implicit=implicit.h:IMPLICIT")
# which interface versions each document must state (the check compares what it finds; this makes a missing one a failure)
set(_required "DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md=display|DEVNOTES_DETAILS/PLUGIN_ABI.md=display,shadeop,dlo,dynamicload,implicit|docs/site/content/manual/reference/plugin-abi.md=display,shadeop")
or_plugin_add_test(PluginAbi_DocPaths 60
    COMMAND ${CMAKE_COMMAND}
        "-DDOCS=${_docs}"
        "-DHEADERS=${_header_names}"
        "-DVERSIONS=${_versions}"
        "-DREQUIRE=${_required}"
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DSTAGE=${OR_PLUGIN_BIN}/checks/stage_docpaths
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_doc_paths.cmake)

# FR-029, SC-009, SC-020 -- DEVNOTES.md and DOMAIN_BOUNDARIES.md agree with the allowlist and the tree: DB-003,
# DB-014 and DB-021 are Resolved by spec 037 with a date, no allowlist line is left for them (T108a).
or_plugin_add_test(PluginAbi_RegistryConsistency 60
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_registry_consistency.cmake)

# FR-037 -- dsoParse was compiled into four libraries; the parser is now hand-written and lives in libRIBShadeop,
# so no library defines dsoParse (MODE=none; the maintainer approved moving this test from MODE=single at T076).
or_plugin_add_test(ShadeopAbi_ParserSingleDefiner 60
    COMMAND ${CMAKE_COMMAND}
        -DSYMBOL=dsoParse
        "-DLIBRARIES=$<TARGET_FILE:ribRender>|$<TARGET_FILE:ribVector>|$<TARGET_FILE:ribOut>|$<TARGET_FILE:libshader_compiler>|$<TARGET_FILE:openrendershadeop>"
        -DMODE=none
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_single_definer.cmake)

# FR-012, FR-037 (T064) -- the new parser has one definer, libRIBShadeop.
or_plugin_add_test(ShadeopAbi_ParserNewSymbol 60
    COMMAND ${CMAKE_COMMAND}
        -DSYMBOL=parseShadeopDeclaration
        "-DLIBRARIES=$<TARGET_FILE:openrendershadeop>|$<TARGET_FILE:ribRender>|$<TARGET_FILE:ribVector>|$<TARGET_FILE:ribOut>|$<TARGET_FILE:libshader_compiler>"
        -DMODE=only
        -DONLY_IN=RIBShadeop
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_single_definer.cmake)

# FR-038, FR-026, SC-011, SC-019 -- the real public header list, staged for both layouts: every header compiles
# alone, includes only installed headers, and the guide's example display builds against the layout's include
# directory alone and renders (T102).
or_plugin_add_test(PluginAbi_InstallHeaders 180
    COMMAND ${CMAKE_COMMAND}
        "-DHEADERS=${_header_names}"
        "-DLAYOUTS=${_layouts}"
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DSTAGE=${OR_PLUGIN_BIN}/checks/stage_install
        -DCXX=${CMAKE_CXX_COMPILER}
        "-DFLAGS=${_check_flags}"
        -DCC=${CMAKE_C_COMPILER}
        "-DC_FLAGS=${_check_c_flags}"
        -DEXAMPLE=${CMAKE_SOURCE_DIR}/examples/display/simpledisplay.c
        -DORENDER=$<TARGET_FILE:orender>
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_install_headers.cmake)
set_tests_properties(PluginAbi_InstallHeaders PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

# FR-026, FR-048 -- the install list, INSTALL_ARTIFACTS.md and the guide agree; file_base.h is in neither.
or_plugin_add_test(PluginAbi_InstallList 60
    COMMAND ${CMAKE_COMMAND}
        "-DHEADERS=${_header_names}"
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DARTIFACTS=${CMAKE_SOURCE_DIR}/INSTALL_ARTIFACTS.md
        -DGUIDE=${CMAKE_SOURCE_DIR}/DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_install_list.cmake)

# FR-026 -- no install() rule names file_base.h.
or_plugin_add_test(PluginAbi_NoFileBaseInstalled 60
    COMMAND ${CMAKE_COMMAND}
        "-DHEADERS=${_header_names}"
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_file_base_installed.cmake)

# FR-048, SC-019 -- libRIBShadeop carries OPENRENDER_COMPAT_SOVERSION (tests/ is configured before src/: the
# target properties are read at generate time).
or_plugin_add_test(PluginAbi_ShadeopSoversion 60
    COMMAND ${CMAKE_COMMAND}
        -DEXPECTED=${OPENRENDER_COMPAT_SOVERSION}
        -DSHADEOP_SOVERSION=$<TARGET_PROPERTY:openrendershadeop,SOVERSION>
        -DSHADEOP_SONAME=$<TARGET_SONAME_FILE_NAME:openrendershadeop>
        -DCOMMON_SOVERSION=$<TARGET_PROPERTY:openrendercommon,SOVERSION>
        -DCOMMON_SONAME=$<TARGET_SONAME_FILE_NAME:openrendercommon>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_shadeop_soversion.cmake)

# T102a (SC-011, SC-019) -- the scratch-prefix install of both layouts and a find_package(openRender) consumer.
# The scratch trees and prefixes live under the build tree only; see check_install_layouts.cmake.
or_plugin_add_test(PluginAbi_InstallLayouts 1800
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DMAIN_BUILD=${CMAKE_BINARY_DIR}
        -DSCRATCH=${OR_PLUGIN_BIN}/checks/install_layouts
        "-DHEADERS=${_header_names}"
        -DINCLUDE_SELFCONTAINED=${OPENRENDER_INCLUDEDIR_SELFCONTAINED}
        -DINCLUDE_FHS=${OPENRENDER_INCLUDEDIR_FHS}
        -DCMAKE_COMMAND=${CMAKE_COMMAND}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_install_layouts.cmake)

# FR-001, FR-047 -- every family header declares its version and documents a history; enabled and RED until
# T041 to T043 (dsply.h joined in T085). The header directories are those of the tree at configure time.
set(_history_dirs "")
foreach(_dir src/include src/ri/render src/ri/geometry src/display src/shadeop src/procedures)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${_dir})
        list(APPEND _history_dirs ${CMAKE_SOURCE_DIR}/${_dir})
    endif()
endforeach()
string(REPLACE ";" "|" _history_dirs "${_history_dirs}")
or_plugin_add_test(PluginAbi_VersionHistory 60
    COMMAND ${CMAKE_COMMAND}
        "-DHEADERS=shadeop.h:SHADEOP|dlo.h:DLO,OR_DYNAMICLOAD|implicit.h:IMPLICIT|dsply.h:OR_DISPLAY"
        "-DINCLUDE_DIRS=${_history_dirs}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_version_history.cmake)

# SC-008 -- one version-check routine. Enabled by T093 (the display host did not exist before Phase 5).
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

# SC-008 -- osLoadModule is called only from the modules that own plug-in loading. Enabled and RED until
# Phase 5 (displayHost.cpp): the starting tree has six other call sites.
or_plugin_add_test(ProceduresAbi_NoLoadOutsideModule 60
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        "-DALLOWED=proceduralModules.cpp|shadeopModule.cpp|displayHost.cpp"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_load_outside.cmake)

# FR-019, FR-046 -- the version 1 display adapter is confined to its named files and nothing casts to a display
# function type. Enabled and RED on the starting tree (dsply.h declares the v1 types; the host casts).
or_plugin_add_test(DisplayV1_NoCastCheck 60
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_display_v1_confined.cmake)
