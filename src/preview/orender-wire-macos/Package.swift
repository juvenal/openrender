// swift-tools-version: 6.0
// Package.swift — orender-wire wireframe scene viewer

import PackageDescription

let package = Package(
    name: "orender-wire-macos",
    platforms: [
        .macOS(.v12)
    ],
    targets: [
        // C module that exposes ribpreview_api.h types to Swift.
        // Actual library objects are linked via -Xlinker flags in CMakeLists.txt.
        //
        // CRibPreview/include/ribpreview_api.h is NOT a source file in this repository — it is
        // staged here at build time by CMakeLists.txt (copy_if_different from the real
        // src/preview/ribpreview_api.h) and gitignored. SPM rejects headerSearchPath entries
        // that reach outside the package root ("header search path should not be outside the
        // package root"), so a same-directory #include via a staged copy is the fallback this
        // contract (contracts/c-abi.md) anticipated.
        .target(
            name: "CRibPreview",
            path: "CRibPreview",
            publicHeadersPath: "include"
        ),
        .executableTarget(
            name: "orender-wire-macos",
            dependencies: ["CRibPreview"],
            path: "Sources",
            linkerSettings: [
                .linkedFramework("Metal"),
                .linkedFramework("MetalKit"),
                .linkedFramework("AppKit"),
                .linkedFramework("SwiftUI"),
            ]
        ),
    ]
)
