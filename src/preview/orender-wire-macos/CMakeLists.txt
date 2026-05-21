# orender-wire-macos — macOS wireframe viewer (Swift 6 / Metal / AppKit)
# Mirrors the orender-fb-macos SPM + CMake pattern exactly.

cmake_minimum_required(VERSION 3.15)

if(NOT APPLE)
    return()
endif()

find_program(SWIFT_EXECUTABLE swift)
if(NOT SWIFT_EXECUTABLE)
    message(WARNING "swift not found — orender-wire-macos will not be built")
    return()
endif()

set(WIRE_MACOS_SRC_DIR   "${CMAKE_CURRENT_SOURCE_DIR}")
set(WIRE_MACOS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/swift-build")
set(WIRE_MACOS_BINARY    "${WIRE_MACOS_BUILD_DIR}/release/orender-wire-macos")

set(WIRE_MACOS_APP_DIR  "${CMAKE_CURRENT_BINARY_DIR}/orender-wire.app")
set(WIRE_MACOS_CONTENTS "${WIRE_MACOS_APP_DIR}/Contents")

file(GLOB_RECURSE WIRE_MACOS_SOURCES
    "${WIRE_MACOS_SRC_DIR}/Sources/*.swift"
    "${WIRE_MACOS_SRC_DIR}/Sources/*.metal"
    "${WIRE_MACOS_SRC_DIR}/Sources/*.h"
    "${WIRE_MACOS_SRC_DIR}/CRibPreview/include/*.h"
)

# Paths to C++ static libraries built by CMake that libribpreview depends on.
set(LIBRIBPREVIEW_A  "${CMAKE_BINARY_DIR}/src/preview/libribpreview/libribpreview.a")
set(LIBCOMMON_A      "${CMAKE_BINARY_DIR}/src/common/libopenrendercommon.a")
set(LIBRI_DYLIB_DIR  "${CMAKE_BINARY_DIR}/src/ri")

add_custom_command(
    OUTPUT  "${WIRE_MACOS_BINARY}"
    COMMAND "${SWIFT_EXECUTABLE}" build
            --configuration release
            --package-path "${WIRE_MACOS_SRC_DIR}"
            --build-path    "${WIRE_MACOS_BUILD_DIR}"
            -Xlinker "${LIBRIBPREVIEW_A}"
            -Xlinker "${LIBCOMMON_A}"
            -Xlinker "-L${LIBRI_DYLIB_DIR}"
            -Xlinker "-lri"
            -Xlinker "-rpath" -Xlinker "@executable_path/../Frameworks"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${WIRE_MACOS_CONTENTS}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${WIRE_MACOS_CONTENTS}/MacOS"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${WIRE_MACOS_CONTENTS}/Resources"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${WIRE_MACOS_CONTENTS}/Frameworks"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${WIRE_MACOS_BINARY}"
            "${WIRE_MACOS_CONTENTS}/MacOS/orender-wire"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${WIRE_MACOS_SRC_DIR}/Info.plist"
            "${WIRE_MACOS_CONTENTS}/Info.plist"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${LIBRI_DYLIB_DIR}/libri.dylib"
            "${WIRE_MACOS_CONTENTS}/Frameworks/libri.dylib"
    COMMAND codesign --force --deep -s - "${WIRE_MACOS_APP_DIR}"
    DEPENDS ${WIRE_MACOS_SOURCES}
            "${WIRE_MACOS_SRC_DIR}/Package.swift"
            "${WIRE_MACOS_SRC_DIR}/Info.plist"
            "${LIBRIBPREVIEW_A}"
            "${LIBCOMMON_A}"
    COMMENT "Building orender-wire.app (Swift/Metal)"
    VERBATIM
)

add_custom_target(orender-wire-macos ALL
    DEPENDS "${WIRE_MACOS_BINARY}"
)

add_dependencies(orender-wire-macos ribpreview)

add_custom_command(TARGET orender-wire-macos POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_BINARY_DIR}/orender-wire.app"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${WIRE_MACOS_APP_DIR}"
            "${CMAKE_BINARY_DIR}/orender-wire.app"
    COMMENT "Copying orender-wire.app to build root"
)

install(DIRECTORY "${WIRE_MACOS_APP_DIR}"
    DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}"
    USE_SOURCE_PERMISSIONS
)

install(CODE
    "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
     execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
        ../libexec/orender-wire.app/Contents/MacOS/orender-wire
        \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/orender-wire\")"
)

message(STATUS "  - orender-wire-macos (Swift/Metal .app bundle via swift build)")
