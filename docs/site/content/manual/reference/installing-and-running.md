---
title: "Installing And Running"
date: 2025-12-08
---

# Installing And Running

**This tutorial shows what you need to do to render images with openRender.**

openRender is a photorealistic renderer which communicates with modelers or your application through a RenderMan - like interface. openRender is not a modeler or an animation system. So it does not have any graphical user interface.

The scenes you want to render are described in a text file in a language very similar to Pixar's RenderMan. openRender also comes as a C/C++ library which you can link against your application. In order to find the details of this interface, you should read RenderMan Companion or RenderMan interface on [http://www.pixar.com](http://www.pixar.com).

openRender comes in following forms:

- **Source code for Windows** (openRender-src-X.Y.Z.zip)
- **Source code for Unix** (openRender-src-X.Y.Z.tgz)
- **Windows Installer** (openRender-X.Y.Z-Setup.exe)
  - Follow the installation instructions. The installer will create a ORENDERHOME for you
- **Windows binaries** (openRender-win-X.Y.Z.zip)
  - The zip file contains a openRender directory which will be your ORENDERHOME
- **Linux binaries** (openRender-linux-X.Y.Z.tgz)
  - The tgz file contains a openRender directory which will be your ORENDERHOME
- **Linux RPMs** (openRender-X.Y.Z.i586.rpm and openRender-X.Y.Z-devel.i586.rpm)

To compile openRender, you will need:

- flex / bison ( [http://www.gnu.org/software/flex/](http://www.gnu.org/software/flex/) and [http://www.gnu.org/software/bison/](http://www.gnu.org/software/bison/) )
- libtiff ( [http://www.libtiff.org](http://www.libtiff.org) )
- fltk ( [http://www.fltk.org](http://www.fltk.org) )
  - Optional, only needed if you want to build "oshow"
- OpenEXR ( [http://www.openexr.org](http://www.openexr.org) )
  - Optional, only needed if you want to build "openexr" display driver

Below are compilation instructions for various platforms:

## Compilation / Installation  for WINDOWS

You can download the openRender source code as a zip file. To compile openRender, you need Microsoft Visual Studio .NET 2005.

- Open ` openRender.sln` (in openRender/windows/vcXXX)
- Hit - `Build - Batch Build`
- Select the components you want to build
- Hit `Build All`

At this point, you can use this directory as your ORENDERHOME. Alternatively, you can run makeinst.bat which will create a openRender directory (that's right inside the openRender directory) and will copy all relevant files (except sources) into this new directory, which you can also use as your ORENDERHOME.

You can also use the Windows installer which directly installs binary distribution.

## Compilation / Installation  for UNIX

You can download the openRender source code as a tgz file. Do not download the zipped source code as the file permissions will be wrong.

- execute `./configure --prefix=/usr/local/openRender --enable-selfcontained`
- execute `make`
- execute `make install`

At this point you should have a `/usr/local/openRender` directory which contains the binary distribution. You can substitute `/usr/local/openRender` with whatever location you want openRender in. This directory will be your ORENDERHOME.

You can set `CXXFLAGS` to whatever compilation flags you want to have during the compilation.

## Compilation / Installation  for OS X

You can download the openRender source code as a tgz file. Do not download the zipped source code as the file permissions will be wrong.

You will need to install libtiff and it's headers from somewhere.  You can do this with fink [[1](http://www.finkproject.org/)], in which case, after following the installation instructions for fink, you can type:

- execute `fink install libtiff`

Having done this and opened a new shell,

- execute `./configure --prefix=/Applications/Graphics/openRender --enable-selfcontained LDFLAGS='-L/sw/lib/ -L/usr/X11R6/lib' CPPFLAGS='-I/sw/include/ -I/usr/X11R6/include'`
- execute `make`
- execute `make install`

At this point you should have a `/Applications/Graphics/openRender` directory which contains the binary distribution. You can substitute `Applications/Graphics/openRender` with whatever location you want openRender in. This directory will be your ORENDERHOME.

If you installed libtiff elsewhere (with headers say in `/path/to/tiffprefix/tiffincludes` and libraries at `/path/to/tiffprefix/tifflibs`, the the configure line needs to be altered appropriately

- execute `./configure --prefix=/Applications/Graphics/openRender --enable-selfcontained LDFLAGS='-L/path/to/tiffprefix/tifflibs -L/usr/X11R6/lib' CPPFLAGS='-I/path/to/tiffprefix/tiffincludes -I/usr/X11R6/include'`

You can set `CXXFLAGS` to whatever compilation flags you want to have during the compilation.

There is also an XCode project you can use for OSX.

## RPM Installation for UNIX

RPM is available in two packages.

- openRender-X.Y.Z.i586.rpm : the application itself (with shaders, doc, etc).
- openRender-X.Y.Z-devel.i586.rpm : all the development libraries and includes files.

Install it with your favourite package manager ( I use Yast on Suse/Linux )

Files are installed in /opt/openrender.

## Binary  Installation for MAC OSX

Currently openRender is compiled against `libtiff` from fink. Under OSX 10.2 or earlier, it will also require you to use fink's dlcompat and dlcompat-shlibs libraries.

Please install fink ( [http://fink.sourceforge.net](http://fink.sourceforge.net) ) to get a hold of the required libraries. I can recommend fink commander ( [http://finkcommander.sourceforge.net](http://finkcommander.sourceforge.net) ) to help ease installation of fink projects, but you'll still need to install fink. Follow the instructions over at fink for how to set it up.

Install `libtiff` from fink. Also, especially for 10.2 or earlier, ensure you install up to date `dlcompat` and `dlcompat-shlib` packages.

Get the binary distribution of openRender and unpack it at `/Applications/Graphics`.  There should be now a folder in `/Applications/Graphics` called `openRender`. You may have issues unpacking with some of the gui apps. Ensure you've got the latest stuffit, or use the command line:

(Assuming package is on your desktop)

```
 cd /Applications/Graphics/
 tar zxvf ~/Desktop/openRender-osx-1.3.xx.tar
```

This part is required for **10.2 ONLY. DO NOT PERFORM THE NEXT STEP ON PANTHER (10.3)**

openRender requires dlcompat. When installed via fink, these are in `/sw/lib/libdl.0.dylib`. openRender binaries are compiled against Panther which had `/usr/lib/libdl.dylib`. The solution is to create a symbolic link from where the library is expected to be, to where it is. One final warning. Don't do this on Panther - you've been warned.

```
sudo ln -s /sw/lib/libdl.0.dylib /usr/lib/libdl.dylib
```

If you don't want to do this, or can't, there's a work around, which is to set for tcsh

```
setenv DYLD_INSERT_LIBARARIES /sw/lib/libdl.0.dylib
```

for bash

```
export DYLD_INSERT_LIBARARIES=/sw/lib/libdl.0.dylib
```

before running openrender.

## Common instructions for using openRender

You must set **ORENDERHOME** (the render root directory) to the location of your installation. At install time this corresponds to the CMake install destination: for a self-contained install it is the same as the install prefix (e.g. `/usr/local` or `/usr/local/openrender`); for a system/FHS install it is typically `share/openRender` under the prefix (e.g. `/usr/local/share/openRender`). You may also want to add the `bin` directory to your `PATH` and the `lib` directory to `LD_LIBRARY_PATH` on Unix or `DYLD_LIBRARY_PATH` on macOS.

For Windows XP:

- Open Control Panel -> System -> Advanced -> Environment Variables
- Hit New
- Variable name: ORENDERHOME
- Variable value: <path-to-openrender-install>

For bash:

- export ORENDERHOME=<path-to-openrender-install>
- export PATH=$PATH:$ORENDERHOME/bin
- export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$ORENDERHOME/lib

You can add these lines in your ".profile" script in your home directory (~/.profile).

For tcsh:

- setenv ORENDERHOME <path-to-openrender-install>
- set path=($path $ORENDERHOME/bin)
- setenv LD_LIBRARY_PATH $LD_LIBRARY_PATH:$ORENDERHOME/lib

You can add these lines in your ".tcshrc" script in your home directory (~/.tcshrc).

### ORENDERHOME and default configuration (`.orenderrc`)

The renderer looks for a default configuration file at **`$ORENDERHOME/.orenderrc`**. When you run `make install` (or the equivalent), the project’s `.orenderrc` is installed into the ORENDERHOME destination (the same directory that holds shaders, ribs, etc. for self-contained installs, or `share/openRender` for system installs). You can override the render root at runtime by setting `ORENDERHOME` to a different directory; the renderer will then look for `.orenderrc` at `$ORENDERHOME/.orenderrc`. The file contains pure RIB and is parsed before your scene RIBs, so you can set default options (e.g. `Format`, `PixelSamples`, search paths) there.

To verify the installed configuration: install to a test prefix, set `ORENDERHOME` to that directory, and run `orender` on a simple RIB; with log level INFO (e.g. `orender -x 3 scene.rib`) you should see a message that `.orenderrc` was loaded.

You can also specify search paths for various external resources that openRender may need using:

```
Option "searchpath" "..." "..."
```

Your binary distribution should have the following structure:

|   | `openrender/` (or install prefix) |   |   | Set **ORENDERHOME** to this directory |
|---|---|---|---|---|
|   |   | `bin/` |   | The executables are here. |
|   |   |   | `orender` | The RIB renderer. |
|   |   |   | `oshader` | Shading language compiler |
|   |   |   | `rsloinfo` | Get information about a compiled shader |
|   |   |   | `otexmake` | Texture preparation tool. |
|   |   |   | `oshow` | A viewer for photon maps/irradiance caches etc.. |
|   |   | `include/` |   | The header files |
|   |   | `lib/` |   | The library files. |
|   |   | `displays/` |   | The display drivers. |
|   |   | `tutorials/` |   | The openRender tutorials/examples |
|   |   | `doc/` |   | Documentation for openRender |
|   |   | `.orenderrc` |   | Default renderer config (pure RIB); loaded at `RiBegin` when `ORENDERHOME` is set |

## Compiling Shaders

openRender uses the `oshader` compiler to compile RenderMan Shading Language (.sl) files into compiled object files (.rslo).

To compile a shader:
```bash
oshader my_shader.sl
```
This will produce `my_shader.rslo`.

### Legacy Compatibility
For backward compatibility, the renderer and tools also support the older `.sdr` extension. If you need to force the compiler to output `.sdr` files, use the `--legacy-sdr` flag:
```bash
oshader --legacy-sdr my_shader.sl
```

The renderer (`orender`) and shader info tool (`rsloinfo`) will automatically search for `.rslo` files first, and fall back to `.sdr` if the newer format is not found.