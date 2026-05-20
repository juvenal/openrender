---
title: "FAQ: Installing on OSX"
date: 2025-12-10
---

# FAQ: Installing on OSX

## Requirements

Currently openRender is compiled against libtiff from fink. Under OSX 10.2 or earlier, it will also require you to use fink's dlcompat and dlcompat-shlibs libraries.

Please install fink ( [http://fink.sourceforge.net](http://fink.sourceforge.net) ) to get a hold of the required libraries. I can recommend fink commander ( [http://finkcommander.sourceforge.net](http://finkcommander.sourceforge.net) ) to help ease installation of fink projects, but you'll still need to install fink. Follow the instructions over at fink for how to set it up.

Install libtiff from fink. Also, especially for 10.2 or earlier, ensure you installl up to date dlcompat and dlcompat-shlib packages.

Get the binary distribution of openRender and unpack it at `/Applications/Graphics`. There should be now a folder in `/Applications/Graphics` called `openRender`. You may have issues unpacking with some of the gui apps. Ensure you've got the latest stuffit, or use the command line:

```
(Assuming package is on your desktop)
cd /Applications/Graphics/
gunzip ~/Desktop/openRender-osx-1.3.xx.tgz
tar -xvf ~/Desktop/openRender-osx-1.3.xx.tar
```

## OSX 10.2 Stuff

Now, for the last part, which is required for 10.2 ONLY. **DO NOT PERFORM THE NEXT STEP ON PANTHER (10.3) or TIGER (10.4)**

openRender requires dlcompat. When installed via fink, these are in `/sw/lib/libdl.0.dylib`. openRender binaries are compiled against Panther which had `/usr/lib/libdl.dylib`. The solution is to create a symbolic link from where the library is expected to be, to where it is. One final warning. Don't do this on Panther or Tiger - you've been warned.

```
sudo ln -s /sw/lib/libdl.0.dylib /usr/lib/libdl.dylib
```

If you don't want to do this, or can't, there's a work arround, which is to set

for tcsh

```
setenv DYLD_INSERT_LIBARARIES /sw/lib/libdl.0.dylib
```

for bash

```
export DYLD_INSERT_LIBARARIES=/sw/lib/libdl.0.dylib
```

before running openrender. Please see the notes at the end of this page...

## For all OSX Versions

You should be ready to roll with renders now. Remember to set up the environment variable `ORENDERHOME`:

for tcsh

```
setenv ORENDERHOME /Applications/Graphics/openRender
```

for bash

```
export ORENDERHOME=/Applications/Graphics/openRender
```

Finally, you'll probably want to put openrender binaries in your path, which you do with

for tcsh

```
set path=($path /Applications/Graphics/openRender/bin)
```

for bash

```
export PATH=$PATH:/Applications/Graphics/openRender/bin
```

That's about it. I've put these commands in my login files. There are quite a few different oppinions/preferences on how to do this.

See also [FAQ: Why Won't openRender Run?](faq.md) for more tips on diagnosing a problem.

---

**Notes:**

I believe there to be an issue running openRender under 10.2.x with the fink dlcompat library. From Beque's email, here's what he did to get things to compile under 10.2.x. I'm hopfully going to fix up the binary distribution so the method above works correctly. It might not right now (openRender 1.4.1).

```
export CFLAGS="$CFLAGS -Ddlsym=dlsym_prepend_underscore"
export CXXFLAGS=$CFLAGS
export CPPFLAGS=$CFLAGS
```

Then I reconfigured, cleaned, rebuilt and installed openRender:

```
./configure --prefix=/Applications/Graphics/openRender
make clean
make
make install
```

Then it worked!

To summarize how I got openRender to work under OSX 10.2.8:

- Download and unpack the source tarball
- Make sure fink is installed with libtiff and dlcompat
- export environment variables to point to fink's include and library

directories

- Define _BSD_SOCKLEN_T_ before including sys/socket.h in src/common/os.h
- export c and c++ flags to define dlsym=dlsym_prepend_underscore
- configure and make as normal

I am sure some of the c and c++ flags I defined were redundant, but better safe
than sorry. I'm just glad I got it to work and hope that this might help someone
else.