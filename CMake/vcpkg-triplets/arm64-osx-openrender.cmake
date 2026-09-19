# arm64-osx-openrender.cmake — vcpkg triplet for openRender's self-contained
# macOS release builds (Apple Silicon).
#
# Deliberately NOT named "arm64-osx-release": vcpkg's own community triplet
# of that name means "VCPKG_BUILD_TYPE release" (skip the Debug
# configuration), an unrelated axis from what this triplet is about. Named
# "-openrender" instead to avoid colliding with that convention while still
# reading clearly as "the openRender project's own triplet".
#
# Based on vcpkg's stock arm64-osx triplet (static linkage, arm64), plus:
#   - VCPKG_OSX_DEPLOYMENT_TARGET pinned to 13.3, matching the
#     -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3 used for the arm64 self-contained
#     build in .github/workflows/release.yml (bumped there from a stale 12.0
#     alongside this triplet's introduction: openRender's own logging.hpp
#     uses std::format on floating-point values, and libc++ backs that with
#     std::to_chars(long double), only available from macOS 13.3 -- 12.0
#     failed to even *compile* libshader_shading, independent of vcpkg).
#     Keep these two in sync if this value ever changes.
#   - VCPKG_BUILD_TYPE release, since a release pipeline never needs vcpkg's
#     Debug configuration -- this alone cuts the dependency build from ~3
#     minutes to ~1.5 in local testing.
#
# Why this exists at all: Homebrew's bottles are pre-built for whatever
# macOS version Homebrew's own CI happened to run on, not for the
# deployment target this project claims -- see the "linker warnings"
# discussion in project memory / the PR this triplet ships with. Every
# dependency built under this triplet inherits the SAME, correct floor,
# which a Homebrew-linked build cannot guarantee.
#
# Static linkage means the libraries built here (zlib, libpng, tiff,
# openexr, imath, and their own transitive deps: libjpeg-turbo, liblzma,
# libdeflate, openjph) end up baked into openRender's binaries directly --
# no .dylib to bundle/rpath-fix at install time for these specifically.
# LLVM/the JIT are NOT part of this triplet and keep coming from Homebrew in
# CI, same as local dev builds; vendoring LLVM from source is a much larger,
# separate undertaking (build time alone is 45+ minutes) left for later.
# This triplet is used ONLY for the self-contained release build -- the FHS
# (system-integrated) build variant is deliberately left linking Homebrew's
# dynamic libraries, since an FHS package is supposed to depend on the
# target system's own package manager for its runtime libraries, exactly
# like a .deb/.rpm does on Linux.

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET "13.3")

set(VCPKG_BUILD_TYPE release)
