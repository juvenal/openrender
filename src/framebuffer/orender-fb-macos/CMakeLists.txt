# orender-fb-macos — macOS framebuffer display helper (Swift 6 / SwiftUI / AppKit)
# Minimum deployment: macOS 12.0 (Monterey)
#
# Built via Swift Package Manager (swift build) to avoid the Xcode generator
# requirement for Swift in CMake. The resulting binary is copied into the build
# tree and installed alongside the orender executable.

cmake_minimum_required(VERSION 3.15)

if(NOT APPLE)
    return()
endif()

find_program(SWIFT_EXECUTABLE swift)
if(NOT SWIFT_EXECUTABLE)
    message(WARNING "swift not found — orender-fb-macos will not be built")
    return()
endif()

find_program(CODESIGN_EXECUTABLE codesign)

set(FB_MACOS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(FB_MACOS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/swift-build")
set(FB_MACOS_BINARY "${FB_MACOS_BUILD_DIR}/release/orender-fb-macos")

# Sources that trigger a rebuild
file(GLOB_RECURSE FB_MACOS_SOURCES
    "${FB_MACOS_SRC_DIR}/Sources/*.swift"
)

add_custom_command(
    OUTPUT  "${FB_MACOS_BINARY}"
    COMMAND "${SWIFT_EXECUTABLE}" build
            --configuration release
            --package-path "${FB_MACOS_SRC_DIR}"
            --build-path    "${FB_MACOS_BUILD_DIR}"
    # Re-sign with plain adhoc — strips the linker-signed flag that macOS 26
    # rejects for binaries on external/non-system volumes.
    COMMAND codesign --force -s - "${FB_MACOS_BINARY}"
    DEPENDS ${FB_MACOS_SOURCES}
            "${FB_MACOS_SRC_DIR}/Package.swift"
    COMMENT "Building orender-fb-macos (Swift)"
    VERBATIM
)

add_custom_target(orender-fb-macos ALL
    DEPENDS "${FB_MACOS_BINARY}"
)

# Copy binary into the main build output directory for easy testing
add_custom_command(TARGET orender-fb-macos POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${FB_MACOS_BINARY}"
            "${CMAKE_BINARY_DIR}/orender-fb-macos"
    COMMENT "Copying orender-fb-macos to build root"
)

install(PROGRAMS "${FB_MACOS_BINARY}"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

message(STATUS "  - orender-fb-macos (Swift/SwiftUI helper via swift build)")
