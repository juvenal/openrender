# x64-osx-openrender.cmake — vcpkg triplet for openRender's self-contained
# macOS release builds (Intel). See arm64-osx-openrender.cmake for the full
# rationale; this is the same triplet for the x86_64 half of
# .github/workflows/release.yml's macOS build matrix (build-macos-x86_64,
# which also uses -DCMAKE_OSX_DEPLOYMENT_TARGET=13.3).

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)
set(VCPKG_OSX_DEPLOYMENT_TARGET "13.3")

set(VCPKG_BUILD_TYPE release)
