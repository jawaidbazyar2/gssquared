# GSSquared - A Complete Apple II Series Emulator.

GSSquared is a complete emulator for the Apple II series of computers. It is written in C++ and runs on Windows, Linux, and macOS.

It uses the SDL3 library for graphics, sound, and I/O. This is a video game-oriented library and suits an emulator well. And, it's cross-platform.

The code base builds and has been tested on:
* MacBook Pro M1
* Mac Intel
* Mac Mini M4
* Ubuntu Linux 22.04 (x64 AMD)

I am hoping I can get someone else to build on Windows. :-)

# Download Binary

A binary release is available at [https://github.com/jawaidbazyar2/gssquared/releases](https://github.com/jawaidbazyar2/gssquared/releases).

# Building

General Build Instructions:

To build GSSquared, you will need the following:
* clang C compiler
* SDL3 downloaded and built from the SDL web site. (Specific steps for this are below.)
* git

I use vscode as my IDE, but, this isn't required to build.

NOTE: the build process has been updated so that builds are done in and to the build/ directory, as opposed to prior versions of GS2 where builds were done into the project root directory.

## Build for Production

Production builds are optimized for performance.

```
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
```

## Build for Debug

Debug enables a variety of assertions, turns off optimizations, and enables memory leak and buffer overrun checking.

```
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build
```

## macOS

GSSquared will build on both Intel and Apple Silicon Macs.

You will need:
* Mac OSX 15 SDK, either Command-line or full Xcode.

```
git clone https://github.com/jawaidbazyar2/gssquared.git
cd gssquared
git submodule update --init --recursive
```

If you just want to do a standard build that will result in a Mac App Bundle,

```
cmake -DGS2_PROGRAM_FILES=OFF  -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build
cmake --install build
```

This will "install" an App Bundle into the build/ directory, named GSSquared.app. (Or in Finder, just GSSquared)

### Mac DMG Disk Image

To go further and build a Mac Disk Image (.dmg file), do this from project root after doing the above build,

```
cmake --build build --target package
```

The app bundle and .dmg file are built into the packages/ directory.

### Mac PROGRAM FILES Mode

PROGRAM FILES Mode will just build executable into build/ (named GSSquared) suitable for execution from the command-line.

```
cmake -DGS2_PROGRAM_FILES=ON  -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build
build/GSSquared
```

You may also use -DCMAKE\_BUILD\_TYPE=Debug , which will disable optimizations, and include debugger symbols in the result.

### Mac Architecture

By default, GSSquared builds as a "Fat Binary" or Dual-Architecture binary for both Apple Silicon and Intel CPUs.

If you want to build only for your native architecture:

```
cmake -DBUILD_NATIVE=ON  -DCMAKE_BUILD_TYPE=Release -S . -B build
```


## Linux

gs2 will currently build on Ubuntu Linux 22.04.

* You need the following libraries installed:
    * libasound2-dev
    * libpulse-dev
    * libudev-dev

```
git clone https://github.com/jawaidbazyar2/gssquared.git
cd gssquared
git submodule update --init --recursive
mkdir build
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build
```

Alternatively, build in the "GNU install directories" style:
```
cmake -DCMAKE_BUILD_TYPE=Release -DGS2_PROGRAM_FILES=OFF -S . -B build
cmake --build build
cmake --install build
```
Now you should be able to run gs2 at command line anywhere via a `GSSquared`
command, and it should also be available as an application from your
applications list.

### Linux App Distribution

After building, you can create a folder that contains libraries and assets for linux.

```
cmake --build build --target packages
```


Creates packages/linux-cli/ directory that has a runnable binary.

## Windows

We've successfully built for windows using the following environment:

* mingw
* VS Code
* clang

```
git clone https://github.com/jawaidbazyar2/gssquared.git
cd gssquared
git submodule update --init --recursive
mkdir build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B build -S .
mingw32-make.exe
```

### Windows App Distribution

After building, you can create a folder that contains libraries and assets for Windows.

```
mingw32-make.exe packages
```


## Creating Distribution Packages

```
cmake --build build --target packages
```

This will create a 'packages' directory with the following structure:

```
packages/
├── linux-cli/
├── mac-cli/
├── GSSquared.dmg
├── GSSquared.app/
```

The mac-package and mac-cli-package are the same, but the mac-cli-package is used to create a DMG file for the macOS app bundle.

Only the appropriate package types for the platform you're building on will be created.

"make", "make all", "make mac-dev" or "make linux-dev" will create a resources directory in the build tree, for local testing.

# Documentation

Check out the [KeyboardShortcuts.md](Docs/KeyboardShortcuts.md) file for a list of keyboard shortcuts needed to operate.

# Affiliated Programs

If you build from source, you will also get a couple of other programs:

## diskid

Analyzes a disk image and prints information about it.

## nibblizer

Convert a disk image file (140K 5.25 .do, .po, .dsk) to nibblized format (e.g. .nib). For testing. 

## gstrace



# Acknowledgements

## OpenEmulator

I took the concept of the DisplayNG code from OpenEmulator, under GPL 3. 
https://github.com/openemulator/openemulator/blob/master/AUTHORS

I also shamelessly copied the Disk II sound files from OpenEmulator.

## Mike Neil

for the lookup table approach to the new DisplayNG code.

## Wyatt Wong

for helping test in different build environments, and providing MacOS-Intel builds.

## SDL

All the amazing people in the SDL Discord, especially Sam!
