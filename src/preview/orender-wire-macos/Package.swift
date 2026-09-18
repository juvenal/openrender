// swift-tools-version: 6.0
// Package.swift — orender-wire wireframe scene viewer

import PackageDescription

let package = Package(
    name: "orender-wire-macos",
    // The real floor is macOS 13.3, not 13.0: this Swift target links
    // libribpreview.a/libri.dylib, whose C++ code uses std::format on
    // floating-point values, and Apple's libc++ backs that with
    // std::to_chars(double), only shipped from macOS 13.3 (see the
    // CMAKE_OSX_DEPLOYMENT_TARGET comment in the root CMakeLists.txt).
    // PackageDescription's MacOSVersion enum has no case finer than a whole
    // major version, so .v13 is the closest expressible floor -- an actual
    // 13.0-13.2 machine will still fail to load this binary (missing
    // to_chars symbol), which SPM's platforms: declaration cannot catch.
    // This was previously (wrongly) .v12, which understated the true floor
    // even before that gap.
    platforms: [
        .macOS(.v13)
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
        //
        // systemLibrary, not a regular target: this module has zero compilable sources by
        // design (linking happens externally, above), and a regular .target with no sources
        // is a known class of SPM fragility -- it can expect a .o file that's never produced.
        // systemLibrary is SPM's purpose-built type for "headers only, linked elsewhere" and
        // never expects one. Backed by CRibPreview/module.modulemap instead of
        // publicHeadersPath, since that parameter doesn't apply to this target type.
        .systemLibrary(
            name: "CRibPreview",
            path: "CRibPreview"
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
                // libribpreview.a/libopenrendercommon.a are raw C++ (exceptions + RTTI:
                // ___cxa_throw/___dynamic_cast/___gxx_personality_v0), and swiftc's own
                // driver -- unlike clang++ -- does not implicitly link libc++. Surfaced
                // only after raising the platform floor to .v13 above; some other linked
                // framework's TBD apparently pulled it in transitively at the old .v12
                // target, papering over this gap rather than this being new.
                .linkedLibrary("c++"),
            ]
        ),
    ]
)
