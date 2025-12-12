---
title: "FAQ"
date: 2025-12-08
---

# Frequently Asked Questions

## Troubleshooting

- [Why won't Pixie run?](#why-wont-pixie-run) - Common issues and solutions
- [What's wrong with my render?](whats-wrong-with-my-render.md) - Rendering problems and fixes
- [How do I install under OSX?](installing-on-osx.md) - Installation guide for macOS

## Why Won't Pixie Run?

This is a list of things to check if you can't get Pixie to run.

## Are the requirements satisfied

You will need libtiff (comes with the windows version).  You will also need some additional items for various other OSes:

- On OSX 10.2, you will need dlcompat installed via fink.  On later OSX version 10.3 and 10.4 this is unneeded, though fink keeps a placeholder installed to indicate that support is present.
- For linux and OSX, you will need an X11 implementation installed.
- For the interactive irradiance-cache viewer, you may need fltk for your platform.  This is not required to use Pixie though.

## Check that you unpacked pixie correctly

If you opted for a binary installation, did you unpack it and place it in the correct location for the OS?

- On OSX, this is `/Applications/Graphics/Pixie`
- for Linux, this can be anywhere you like, but the envionment variable`LD_LIBRARY_PATH` needs to be set to the location of the lib files.  In other words, if you put the unpacked contents of the tar file at `/my/path/to/pixie` then you need to do the folllowing `export LD_LIBRARY_PATH=/my/path/to/pixie/lib` for bash or `setenv LD_LIBRARY_PATH /my/path/to/pixie/lib` for tcsh.
- The windows installer selects a location and will set up environment variables for you.
- Make sure that the unpacking worked correctly.  In the `displays` and `lib` directory, there shold be several symbolic links of versioned libraries to the real ones.  Go to that directory and type: `ls -ln` and you should see something like

```
-rwxr-xr-x   1 501  80   9882144 19 Aug 18:17 libri.0.0.0.dylib
lrwxr-xr-x   1 501  80        17 19 Aug 18:17 libri.0.dylib -> libri.0.0.0.dylib
-rw-r--r--   1 501  80  10818716 19 Aug 18:17 libri.a
lrwxr-xr-x   1 501  80        17 19 Aug 18:17 libri.dylib -> libri.0.0.0.dylib
-rwxr-xr-x   1 501  80       893 19 Aug 18:17 libri.la
-rwxr-xr-x   1 501  80     66944 19 Aug 18:16 libsdr.0.0.0.dylib
lrwxr-xr-x   1 501  80        18 19 Aug 18:16 libsdr.0.dylib -> libsdr.0.0.0.dylib
-rw-r--r--   1 501  80    143840 19 Aug 18:16 libsdr.a
lrwxr-xr-x   1 501  80        18 19 Aug 18:16 libsdr.dylib -> libsdr.0.0.0.dylib
-rwxr-xr-x   1 501  80       844 19 Aug 18:16 libsdr.la
```

- If the permissions look cookie or there are no symbolic links, make sure you used something like command-line tar to extract ( eg `tar zxvf /path/to/pixie.tgz` ).

## Check that the `PIXIEHOME` environment variable is set

The environment variable `PIXIEHOME` is needed by various parts of pixie to find it's components.  Now there are several ways that Pixie can be installed, and which one you did will affect what you should set `PIXIEHOME` to.  The possible methods are - selfcontained and normal.  With each, you can set the target directory.

The OSX binary build is a selfcontained build which needs to be installed at `/Applications/Graphics/Pixie`,  Selfcontained builds are designed to live in a single directory.

The linux binary builds are designed to be placed in a single directory somewhere on your system, but be sure to set `LD_LIBRARY_PATH` as noted above.  For these selfcontained builds, set `PIXIEHOME` to be the location you installed to.

- For example, on OSX `export PIXIEHOME=/Applications/Graphics/Pixie` for bash or `setenv PIXIEHOME /Applications/Graphics/Pixie` for tcsh.
- For a linux build installed to `/my/path/to/pixie`, do `export LD_LIBRARY_PATH=/my/path/to/pixie/` for bash and `setenv LD_LIBRARY_PATH /my/path/to/pixie/` for tcsh.

The normal build type allows you to slot the installation into the standard "FHS" hierachy on a unix / linux installation.  This means that the binaries go to `<prefix>/bin`, libraries to `<prefix>/lib` and so on.  The important difference comes for the data pixie uses - the shaders, display drivers etc.  These will go to `<prefix>/lib/Pixie/`.  It is this location which you should use to define `PIXIEHOME`.  This is the default installation type if you do `./configure` then `make install` from source.  The default location to install to is `/usr/local/`.  If you selected another location when you configured (eg. `./configure --prefix=/path/to/prefix` for example ) then the prefix will be prepended on all installation paths.  You can also cause the configure to make a selfcontained build using `./configure --enable-selfcontained --prefix=/path/to/prefix`

- For any build which is not selfcontained, set `PIXIEHOME` to be the `prefix + /lib/Pixie`.  If you configured to `/path/to/prefix` for a non-selfcontained build, then use `export PIXIEHOME=/path/to/prefix/lib/Pixie` for tcsh or `setenv PIXIEHOME /path/to/prefix/lib/Pixie` for tcsh.
- This will be the case for fink builds, which should have `PIXIEHOME` set to `export PIXIEHOME=/sw/lib/Pixie` for tcsh or `setenv PIXIEHOME /sw/lib/Pixie` for tcsh.

## Errors about libtiff

Libtiff is needed by Pixie for reading and writing textures and final images.  Ensure you have a current version set up.  If you get complaints about incompatible versions, then try upgrading.

- On Linux, try getting a new rpm or configure and install from source.
- For windows, a version is included.
- For OSX, please use fink ( the build expects libtiff to be installed from fink ) to update using the line `fink update libtiff`  Currently the OSX build uses version 3.7.2-1 from the unstable branch (it has many important fixes).  You can see what version you have using `fink list libtiff`.  To enable the unstable tree in fink, see [this link](http://fink.sourceforge.net/faq/usage-fink.php?phpLang=en#unstable).

## Errors about X11

X11 is used on OSX and Linux to display the framebuffer.  Ensure it's open, you can open windows on that display and the `xauth` permissions are appropriate.  Ensure `DISPLAY` is set up and X11 is running before doing a render which will use the framebuffer.

## Compiling hangs

The file execute.cpp and to a certain extent stochatstic.cpp take a *long* time to compile.  This is expected - we use macros to instantiate code with various options, so that when the renderer runs, it can take an optimal path through the code, executing just what's needed.  The execute.cpp file takes about 20-30mins using gcc4 on my G5 1.6GHz / 1Gb machine.  If your processor is slower or you have less ram, it'll take longer.