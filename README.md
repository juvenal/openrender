# openRender

[![License](https://img.shields.io/badge/license-LGPL--2.1-blue.svg)](https://opensource.org/licenses/LGPL-2.1)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![Standard](https://img.shields.io/badge/C++-C++20-blue.svg)](https://en.cppreference.com/w/cpp/compiler_support)

openRender is a photorealistic renderer that uses a Pixar's RenderMan-like interface. Originally based on Pixie by Okan Arikan, this project continues to evolve as an open-source rendering solution.

## Table of Contents
- [About](#about)
- [Features](#features)
- [Installation](#installation)
- [Dependencies](#dependencies)
- [Building](#building)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)
- [Authors](#authors)

## About

openRender is a sophisticated photorealistic renderer that implements a RenderMan-compliant interface. It is designed to be compatible with RenderMan Interface Bytestream (RIB) files and supports a wide range of rendering techniques including ray tracing, Reyes-style rendering, and advanced shading.

## Features

- RenderMan-compliant interface supporting RIB files
- Advanced shading language support
- Multiple rendering backends (Reyes, ray tracing)
- Support for various image formats (TIFF, PNG, Radiance RGBE)
- Multi-threaded rendering
- Network rendering capabilities
- Procedural geometry support
- Extensive texture mapping options
- OpenEXR support for high dynamic range output

## Installation

### Prerequisites

- C++20 compliant compiler (GCC 10+, Clang 10+, or MSVC 2019+)
- CMake 3.15 or higher
- Git

### Dependencies

- **libtiff**: Image format support (http://www.libtiff.org)
- **flex/bison**: Parser generation (available on Unix platforms by default)
- **fltk**: GUI support for the interactive viewer (http://www.fltk.org)
- **OpenEXR**: High dynamic range image support (http://www.openexr.com)

## Building

### Unix/Linux/macOS

```bash
# Clone the repository
git clone https://github.com/juvenal/openrender.git
cd openrender

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compile (using multiple cores)
make -j$(nproc || sysctl -n hw.ncpu)

# Optionally install
sudo make install
```

### Windows

Using Visual Studio or similar IDE:

1. Open CMake GUI or use command line
2. Set source directory to openrender root
3. Set build directory (e.g., `openrender/build`)
4. Click "Configure" and select your generator (Visual Studio, Ninja, etc.)
5. Click "Generate"
6. Build using your IDE or run `cmake --build . --config Release`

Alternatively, using command line:

```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### CMake Options

- `USE_FLEX_BISON`: Use flex and bison to regenerate parsers (default: ON)
- `BUILD_SHOW`: Build the show program (default: ON)
- `INSTALL_SELFCONTAINED`: Build for a self-contained setup (default: ON)

## Usage

### Basic Rendering

```bash
# Render a RIB file
./build/src/orender scene.rib

# Render with multiple threads
./build/src/orender -t:4 scene.rib

# Render specific frame range
./build/src/orender -f 1:10 scene.rib
```

### Network Rendering

```bash
# Start a render server
./build/src/orender -r

# Render on network servers
./build/src/orender -s server1:port1,server2:port2 scene.rib
```

## Project Structure

```
├── CMake/                 # CMake modules
├── doc/                  # Documentation
├── docs/                 # Additional documentation
├── examples/             # Example scenes
├── geometry/             # Geometry examples
├── man/                  # Manual pages
├── shaders/              # Default shaders
├── specs/                # Specification documents
├── src/                  # Source code
│   ├── common/           # Common utilities
│   ├── dsotest/          # DSO test utilities
│   ├── file/             # File I/O operations
│   ├── framebuffer/      # Framebuffer implementations
│   ├── gui/              # GUI components
│   ├── openexr/          # OpenEXR support
│   ├── orender/          # Main renderer executable
│   ├── oshader/          # Shader compiler
│   ├── oshow/            # Interactive viewer
│   ├── otexmake/         # Texture processing tools
│   ├── precomp/          # Precomputation tools
│   ├── rgbe/             # Radiance RGBE support
│   ├── ri/               # RenderMan interface implementation
│   ├── sdr/              # Shader runtime
│   ├── sdrinfo/          # Shader info utility
│   └── ...
├── tests/                # Unit tests
├── textures/             # Sample textures
├── CMakeLists.txt        # Main CMake build file
├── COMPILING.txt         # Compilation instructions
├── README.md             # This file
├── COPYING               # License information
└── ...
```

## Contributing

We welcome contributions to openRender! Please feel free to submit pull requests, create issues for bugs and feature requests, or contribute to documentation.

### Development Guidelines

- Follow C++20 standards
- Maintain compatibility with existing RIB files
- Write unit tests for new functionality
- Update documentation as needed

## License

openRender is licensed under the GNU Lesser General Public License v2.1 (LGPL-2.1). See the [COPYING](COPYING) file for details.

## Authors

### Current Maintainer
- **Juvenal A. Silva Jr.** <juvenal.silva@v2-labs.press>

### Original Creator
- **Okan Arikan** - Original Pixie renderer

### Historical Contributors
The original Pixie project contributors include:
- Okan Arikan (Head developer)
- George Harker
- Gary Oliver & Kirk Bailey
- Bruce Walter
- Moritz Moeller
- Jordan Smith
- Mayur Patel
- Ashton Edwin-Kent
- Eliot Mack
- Raphael Sebbe

For a complete list of contributors, see the [AUTHORS](AUTHORS) file.

---

*openRender continues the legacy of the original Pixie renderer, extending its capabilities while maintaining compatibility with the RenderMan interface.*