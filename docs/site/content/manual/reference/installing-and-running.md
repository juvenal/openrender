---
title: "Installing And Running"
date: 2025-12-08
---

# Installing And Running

**This tutorial shows what you need to do to render images with Pixie.**

Pixie is a photorealistic renderer which communicates with modelers or your application through a RenderMan - like interface. Pixie is not a modeler or an animation system. So it does not have any graphical user interface.

The scenes you want to render are described in a text file in a language very similar to Pixar's RenderMan. Pixie also comes as a C/C++ library which you can link against your application. In order to find the details of this interface, you should read RenderMan Companion or RenderMan interface on [http://www.pixar.com](http://www.pixar.com).

Pixie comes in following forms:

- **Source code for Windows** (Pixie-src-X.Y.Z.zip)
- **Source code for Unix** (Pixie-src-X.Y.Z.tgz)
- **Windows Installer** (Pixie-X.Y.Z-Setup.exe)
  - Follow the installation instructions. The installer will create a PIXIEHOME for you
- **Windows binaries** (Pixie-win-X.Y.Z.zip)
  - The zip file contains a Pixie directory which will be your PIXIEHOME
- **Linux binaries** (Pixie-linux-X.Y.Z.tgz)
  - The tgz file contains a Pixie directory which will be your PIXIEHOME
- **Linux RPMs** (Pixie-X.Y.Z.i586.rpm and Pixie-X.Y.Z-devel.i586.rpm)

To compile Pixie, you will need:

- flex / bison ( [http://www.gnu.org/software/flex/](http://www.gnu.org/software/flex/) and [http://www.gnu.org/software/bison/](http://www.gnu.org/software/bison/) )
- libtiff ( [http://www.libtiff.org](http://www.libtiff.org) )
- fltk ( [http://www.fltk.org](http://www.fltk.org) )
  - Optional, only needed if you want to build "show"
- OpenEXR ( [http://www.openexr.org](http://www.openexr.org) )
  - Optional, only needed if you want to build "openexr" display driver

Below are compilation instructions for various platforms:

## Compilation / Installation  for WINDOWS

You can download the Pixie source code as a zip file. To compile Pixie, you need Microsoft Visual Studio .NET 2005.

- Open ` Pixie.sln` (in Pixie/windows/vcXXX)
- Hit - `Build - Batch Build`
- Select the components you want to build
- Hit `Build All`

At this point, you can use this directory as your PIXIEHOME. Alternatively, you can run makeinst.bat which will create a Pixie directory (that's right inside the Pixie directory) and will copy all relevant files (except sources) into this new directory, which you can also use as your PIXIEHOME.

You can also use the Windows installer which directly installs binary distribution.

## Compilation / Installation  for UNIX

You can download the Pixie source code as a tgz file. Do not download the zipped source code as the file permissions will be wrong.

- execute `./configure --prefix=/usr/local/Pixie --enable-selfcontained`
- execute `make`
- execute `make install`

At this point you should have a `/usr/local/Pixie` directory which contains the binary distribution. You can substitute `/usr/local/Pixie` with whatever location you want Pixie in. This directory will be your PIXIEHOME.

You can set `CXXFLAGS` to whatever compilation flags you want to have during the compilation.

## Compilation / Installation  for OS X

You can download the Pixie source code as a tgz file. Do not download the zipped source code as the file permissions will be wrong.

You will need to install libtiff and it's headers from somewhere.  You can do this with fink [[1](http://www.finkproject.org/)], in which case, after following the installation instructions for fink, you can type:

- execute `fink install libtiff`

Having done this and opened a new shell,

- execute `./configure --prefix=/Applications/Graphics/Pixie --enable-selfcontained LDFLAGS='-L/sw/lib/ -L/usr/X11R6/lib' CPPFLAGS='-I/sw/include/ -I/usr/X11R6/include'`
- execute `make`
- execute `make install`

At this point you should have a `/Applications/Graphics/Pixie` directory which contains the binary distribution. You can substitute `Applications/Graphics/Pixie` with whatever location you want Pixie in. This directory will be your PIXIEHOME.

If you installed libtiff elsewhere (with headers say in `/path/to/tiffprefix/tiffincludes` and libraries at `/path/to/tiffprefix/tifflibs`, the the configure line needs to be altered appropriately

- execute `./configure --prefix=/Applications/Graphics/Pixie --enable-selfcontained LDFLAGS='-L/path/to/tiffprefix/tifflibs -L/usr/X11R6/lib' CPPFLAGS='-I/path/to/tiffprefix/tiffincludes -I/usr/X11R6/include'`

You can set `CXXFLAGS` to whatever compilation flags you want to have during the compilation.

There is also an XCode project you can use for OSX.

## RPM Installation for UNIX

RPM is available in two packages.

- Pixie-X.Y.Z.i586.rpm : the application itself (with shaders, doc, etc).
- Pixie-X.Y.Z-devel.i586.rpm : all the development libraries and includes files.

Install it with your favourite package manager ( I use Yast on Suse/Linux )

Files are installed in /opt/pixie.

## Binary  Installation for MAC OSX

Currently Pixie is compiled against `libtiff` from fink. Under OSX 10.2 or earlier, it will also require you to use fink's dlcompat and dlcompat-shlibs libraries.

Please install fink ( [http://fink.sourceforge.net](http://fink.sourceforge.net) ) to get a hold of the required libraries. I can recommend fink commander ( [http://finkcommander.sourceforge.net](http://finkcommander.sourceforge.net) ) to help ease installation of fink projects, but you'll still need to install fink. Follow the instructions over at fink for how to set it up.

Install `libtiff` from fink. Also, especially for 10.2 or earlier, ensure you install up to date `dlcompat` and `dlcompat-shlib` packages.

Get the binary distribution of Pixie and unpack it at `/Applications/Graphics`.  There should be now a folder in `/Applications/Graphics` called `Pixie`. You may have issues unpacking with some of the gui apps. Ensure you've got the latest stuffit, or use the command line:

(Assuming package is on your desktop)

```
 cd /Applications/Graphics/
 tar zxvf ~/Desktop/Pixie-osx-1.3.xx.tar
```

This part is required for **10.2 ONLY. DO NOT PERFORM THE NEXT STEP ON PANTHER (10.3)**

Pixie requires dlcompat. When installed via fink, these are in `/sw/lib/libdl.0.dylib`. Pixie binaries are compiled against Panther which had `/usr/lib/libdl.dylib`. The solution is to create a symbolic link from where the library is expected to be, to where it is. One final warning. Don't do this on Panther - you've been warned.

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

before running pixie.

## Common instructions for using Pixie

You must set PIXIEHOME environment variable to the location of your binary distribution. You may also want to add PIXIEHOME/bin to your path and add the lib directory into your LD_LIBRARY_PATH on Unix and DYLIB_LIBRARY_PATH on OSX.

For Windows XP:

- Open Control Panel -> System -> Advanced -> Environment Variables
- Hit New
- Variable name: PIXIEHOME
- Variable value: <path-to-pixie>

For bash:

- export PIXIEHOME=<path-to-pixie>
- export PATH=$PATH:$PIXIEHOME/bin
- export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PIXIEHOME/lib

You can add these lines in your ".profile" script in your home directory (~/.profile).

For tcsh:

- setenv PIXIEHOME <path-to-pixie>
- setp path=($path $PIXIEHOME/bin)
- setenv LD_LIBRARY_PATH $LD_LIBRARY_PATH:$PIXIEHOME/lib

You can add these lines in your ".tcshrc" script in your home directory (~/.tcshrc).

You can also specify search paths for various external resources that Pixie may need using:

```
Option "searchpath" "..." "..."
```

Your binary distribution should have the following structure:

|   | `Pixie/` |   |   | This is your PIXIEHOME |
|---|---|---|---|---|
|   |   | `bin/` |   | The executables are here. |
|   |   |   | `rndr` | The RIB renderer. |
|   |   |   | `sdrc` | Shading language compiler |
|   |   |   | `sdrinfo` | Get information about a compiled shader |
|   |   |   | `texmake` | Texture preparation tool. |
|   |   |   | `show` | A viewer for photon maps/irradiance caches etc.. |
|   |   | `include/` |   | The header files |
|   |   | `lib/` |   | The library files. |
|   |   | `displays/` |   | The display drivers. |
|   |   | `tutorials/` |   | The Pixie tutorials/examples |
|   |   | `doc/` |   | Documentation for Pixie |