---
title: "Contributing Guidelines"
date: 2025-12-10
---

# Contributing Guidelines

Placeholder content for contributing guidelines.

## Optional: OpenImageIO

`-DOPENRENDER_ENABLE_OIIO=ON` (off by default) adds an OpenImageIO image backend (`libRIBImageIO`), the `oiio` display driver and the
`Option "imageio"` selection. It needs OpenImageIO 3.0 or newer. On macOS with vcpkg the `oiio` feature installs into its own directory,
`<VCPKG_INSTALLED_DIR>-oiio`, never the shared tree; with Homebrew, `brew install openimageio` (untested: this release validated only vcpkg static OpenImageIO on macOS and a shared prefix build on Linux). On Linux, Ubuntu 24.04's OpenImageIO (2.4.17) is too old:
build OpenImageIO 3.1.14 into a prefix with `-DOpenImageIO_BUILD_MISSING_DEPS=all`, apply the pystring pin (in `<build>/deps/pystring`:
`git fetch --unshallow`, then `git checkout -B master 381829c2c1696ffec9277b339952a9588e6e67cf`, because upstream `master` has moved and the configure
stops at the tag check; never `OpenImageIO_DEPENDENCY_BUILD_ALLOW_UNVERIFIED_TAGS=ON`), and configure openRender with `-DOpenImageIO_ROOT=<prefix>`. OpenColorIO
2.3 is required. CI and the release builds do not bundle OpenImageIO in this release. Details: `INSTALL.md` and `DEVNOTES_DETAILS/IMAGE_IO.md`.
