# LTS verification suite. Every test carries the label "lts", so
# `ctest -L '^lts$'` selects exactly these checks. Release-branch CI runs
# that label on top of the ordinary suite. The checks are ABI/install smoke
# tests, not image comparisons: they stay meaningful on a release branch
# that no longer moves its visual references.

# No cmake_minimum_required() here: this directory is only reached through
# add_subdirectory(), and a nested call would reset the policy stack.

add_executable(lts_plugin_abi lts_plugin_abi.c)
target_link_libraries(lts_plugin_abi PRIVATE openrender_common_flags)
target_include_directories(lts_plugin_abi PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/display
    ${CMAKE_SOURCE_DIR}/src/shadeop
    ${CMAKE_SOURCE_DIR}/src/procedures
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/libshader/runtime)
add_test(NAME Lts_PluginAbiHeaders COMMAND lts_plugin_abi)
set_tests_properties(Lts_PluginAbiHeaders PROPERTIES TIMEOUT 60 LABELS "lts")

set(_lts_libs
    "$<TARGET_FILE:openrendercommon>|$<TARGET_FILE:openrendershadeop>|$<TARGET_FILE:ribRender>|$<TARGET_FILE:ribVector>|$<TARGET_FILE:ribOut>|$<TARGET_FILE:libshader_runtime>|$<TARGET_FILE:libshader_compiler>|$<TARGET_FILE:libshader_shading>")

add_test(NAME Lts_SoVersion
    COMMAND ${CMAKE_COMMAND}
        -DSOVERSION=${OPENRENDER_COMPAT_SOVERSION}
        "-DLIBRARIES=${_lts_libs}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_soversion.cmake)
set_tests_properties(Lts_SoVersion PROPERTIES TIMEOUT 120 LABELS "lts")

add_test(NAME Lts_ExportedSymbols
    COMMAND ${CMAKE_COMMAND}
        "-DLIBRARIES=${_lts_libs}"
        -DSYMBOL_LIST=${CMAKE_CURRENT_SOURCE_DIR}/required-symbols.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_exported_symbols.cmake)
set_tests_properties(Lts_ExportedSymbols PROPERTIES TIMEOUT 120 LABELS "lts")

add_test(NAME Lts_InstallSmoke
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DC_COMPILER=${CMAKE_C_COMPILER}
        -DSOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/lts_plugin_abi.c
        -DSTAGE_INSTALL=${CMAKE_SOURCE_DIR}/scripts/ci/stage-install.sh
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_install.cmake)
set_tests_properties(Lts_InstallSmoke PROPERTIES
    TIMEOUT 900
    LABELS "lts"
    RESOURCE_LOCK "lts-install")

add_test(NAME Lts_AbiDump
    COMMAND ${CMAKE_COMMAND}
        "-DLIBRARIES=${_lts_libs}"
        -DSYMBOL_LIST=${CMAKE_CURRENT_SOURCE_DIR}/required-symbols.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_abi_dump.cmake)
set_tests_properties(Lts_AbiDump PROPERTIES TIMEOUT 300 LABELS "lts")

add_test(NAME Lts_PolicyFile
    COMMAND ${CMAKE_COMMAND}
        -DLTS_JSON=${CMAKE_SOURCE_DIR}/LTS.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_lts_policy.cmake)
set_tests_properties(Lts_PolicyFile PROPERTIES TIMEOUT 30 LABELS "lts")

find_program(OPENRENDER_SWIFTC swiftc)
if(OPENRENDER_SWIFTC)
    file(GLOB _fb_swift "${CMAKE_SOURCE_DIR}/src/display/framebuffer/orender-fb-macos/Sources/*.swift")
    file(GLOB _wire_swift "${CMAKE_SOURCE_DIR}/src/preview/orender-wire-macos/Sources/*.swift")
    set(_swift_files
        ${_fb_swift}
        ${_wire_swift}
        "${CMAKE_SOURCE_DIR}/tests/framebuffer/test_protocol.swift"
        "${CMAKE_SOURCE_DIR}/tests/framebuffer/test_imagestore.swift")
    list(JOIN _swift_files "|" _swift_joined)
    add_test(NAME Lts_SwiftParse
        COMMAND ${CMAKE_COMMAND}
            -DSWIFTC=${OPENRENDER_SWIFTC}
            "-DSOURCES=${_swift_joined}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_swift_parse.cmake)
    set_tests_properties(Lts_SwiftParse PROPERTIES TIMEOUT 180 LABELS "lts")
    message(STATUS "  - Lts_SwiftParse (${OPENRENDER_SWIFTC})")
else()
    message(STATUS "  - Lts_SwiftParse skipped: swiftc not found")
endif()
