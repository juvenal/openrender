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
            resources: [
                .copy("AppIcon.icns")
            ]
        )
    ]
)
