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
        .target(
            name: "CRibPreview",
            path: "CRibPreview",
            publicHeadersPath: "include"
        ),
        .executableTarget(
            name: "orender-wire-macos",
            dependencies: ["CRibPreview"],
            path: "Sources",
            exclude: ["Shaders.metal"],  // Metal file for reference; inline source used at runtime
            linkerSettings: [
                .linkedFramework("Metal"),
                .linkedFramework("MetalKit"),
                .linkedFramework("AppKit"),
            ]
        ),
    ]
)
