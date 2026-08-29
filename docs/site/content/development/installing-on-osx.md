---
title: "FAQ: Installing on macOS"
date: 2026-08-29
---

# FAQ: Installing on macOS

Build instructions for macOS live with every other platform's in
[Installing / running openRender](/openrender/manual/reference/installing-and-running/#macos).
They are kept in one place on purpose: this page and that one used to carry
the same instructions in duplicate, and they drifted.

The short version:

```bash
brew install cmake libtiff libpng bison
brew install llvm openexr fltk        # optional components

git clone https://github.com/juvenal/openrender.git
cd openrender
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Two macOS-specific points, both handled by the build system rather than by
you:

- **Homebrew's bison is required, and does not need to be on your `PATH`.**
  macOS ships bison 2.3, far too old for openRender's grammars. Homebrew's is
  keg-only for exactly that reason, and CMake looks for it at
  `/opt/homebrew/opt/bison/bin` and `/usr/local/opt/bison/bin` directly.
- **Homebrew's flex is not required.** The system flex at `/usr/bin/flex` is
  current enough, and is what CMake picks up.

The same keg-only fallback applies to Homebrew's LLVM, which enables the JIT
shading backend. Without it the build still succeeds; shaders run through the
bytecode interpreter instead.

## Historical note

Earlier versions of this page described installing dependencies through
[fink](http://www.finkproject.org/), symlinking `dlcompat` for Mac OS X 10.2,
and unpacking a binary tarball into `/Applications/Graphics`. None of that
applies any more: the project builds with CMake against Homebrew, and there is
no binary distribution. That material has been removed rather than kept, since
the systems it described are two decades out of support.

See also [FAQ: Why Won't openRender Run?](/openrender/development/faq/) for
help diagnosing a build or run that goes wrong.
