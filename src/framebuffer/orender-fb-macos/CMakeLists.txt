# orender-fb-macos — macOS framebuffer display helper (Swift 6 / SwiftUI / AppKit)
# Minimum deployment: macOS 12.0 (Monterey)
#
# Built via Swift Package Manager (swift build) to avoid the Xcode generator
# requirement for Swift in CMake. After the swift build the output is assembled
# into a proper .app bundle so that macOS reads Info.plist (menu-bar name, icon,
# etc.) when the process is spawned from fbq.cpp. The bundle is installed under
# libexec/; a relative symlink in bin/ makes it transparent to the spawner.

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

set(FB_MACOS_SRC_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(FB_MACOS_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/swift-build")
set(FB_MACOS_BINARY   "${FB_MACOS_BUILD_DIR}/release/orender-fb-macos")

# In-tree .app bundle assembled after each swift build
set(FB_MACOS_APP_DIR  "${CMAKE_CURRENT_BINARY_DIR}/openRender.app")
set(FB_MACOS_CONTENTS "${FB_MACOS_APP_DIR}/Contents")

# Sources that trigger a rebuild (includes icon resource)
file(GLOB_RECURSE FB_MACOS_SOURCES
    "${FB_MACOS_SRC_DIR}/Sources/*.swift"
    "${FB_MACOS_SRC_DIR}/Sources/*.icns"
)

add_custom_command(
    OUTPUT  "${FB_MACOS_BINARY}"
    # ── 1. Swift build ────────────────────────────────────────────────────────
    COMMAND "${SWIFT_EXECUTABLE}" build
            --configuration release
            --package-path "${FB_MACOS_SRC_DIR}"
            --build-path    "${FB_MACOS_BUILD_DIR}"
    # ── 2. Assemble proper .app bundle ────────────────────────────────────────
    # Wipe any stale Contents/ from a previous build before laying down fresh
    # content — prevents old SwiftPM bundles or removed resources from lingering.
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${FB_MACOS_CONTENTS}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${FB_MACOS_CONTENTS}/MacOS"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${FB_MACOS_CONTENTS}/Resources"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${FB_MACOS_BINARY}"
            "${FB_MACOS_CONTENTS}/MacOS/openRender"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${FB_MACOS_SRC_DIR}/Info.plist"
            "${FB_MACOS_CONTENTS}/Info.plist"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${FB_MACOS_SRC_DIR}/Sources/AppIcon.icns"
            "${FB_MACOS_CONTENTS}/Resources/AppIcon.icns"
    # ── 3. Ad-hoc sign the complete bundle ────────────────────────────────────
    # --deep covers nested bundles (SwiftPM resource bundle).
    # --force -s - strips the linker-signed flag macOS 26 rejects on external
    # volumes and replaces it with a plain adhoc identity.
    COMMAND codesign --force --deep -s - "${FB_MACOS_APP_DIR}"
    DEPENDS ${FB_MACOS_SOURCES}
            "${FB_MACOS_SRC_DIR}/Package.swift"
            "${FB_MACOS_SRC_DIR}/Info.plist"
    COMMENT "Building openRender.app (Swift)"
    VERBATIM
)

add_custom_target(orender-fb-macos ALL
    DEPENDS "${FB_MACOS_BINARY}"
)

# Copy the assembled .app bundle to the build root for development testing.
# Remove first so stale content from previous builds doesn't survive.
add_custom_command(TARGET orender-fb-macos POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_BINARY_DIR}/openRender.app"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${FB_MACOS_APP_DIR}"
            "${CMAKE_BINARY_DIR}/openRender.app"
    COMMENT "Copying openRender.app to build root"
)

# ── Install ───────────────────────────────────────────────────────────────────
# Bundle goes under libexec/openRender.app. Two symlinks give consistent access:
#   libexec/orender-fb  → openRender.app/Contents/MacOS/openRender  (within libexec)
#   bin/orender-fb      → ../libexec/openRender.app/Contents/MacOS/openRender
# macOS resolves both symlinks to the real binary inside Contents/MacOS/, which
# triggers full bundle semantics (Info.plist, Dock icon, CFBundleName menu name).

install(DIRECTORY "${FB_MACOS_APP_DIR}"
    DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}"
    USE_SOURCE_PERMISSIONS
)

install(CODE
    "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
        openRender.app/Contents/MacOS/openRender
        \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}/orender-fb\")
     file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
     execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink
        ../libexec/openRender.app/Contents/MacOS/openRender
        \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/orender-fb\")"
)

message(STATUS "  - orender-fb-macos (Swift/SwiftUI .app bundle via swift build)")
