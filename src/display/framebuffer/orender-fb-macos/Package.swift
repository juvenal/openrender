// swift-tools-version: 6.0
// Package.swift — orender-fb-macos framebuffer display helper

import PackageDescription

let package = Package(
    name: "orender-fb-macos",
    platforms: [
        .macOS(.v12)
    ],
    targets: [
        .executableTarget(
            name: "orender-fb-macos",
            path: "Sources",
            // AppIcon.icns and AppIcon.icon/ are not Swift resources: CMakeLists.txt
            // and run_actool.cmake copy/compile them into the .app bundle directly
            // (see FB_MACOS_ICON_PKG there). Excluded here so SPM's own build
            // doesn't flag them as unhandled files in this target's source directory.
            exclude: [
                "AppIcon.icns",
                "AppIcon.icon",
            ]
        )
    ]
)
