# GSSquared - A Complete Apple II Series Emulator.

GSSquared is a complete emulator for the Apple II series of computers. It is written in C++ and runs on Windows, Linux, and macOS.

It uses the SDL3 library for graphics, sound, and I/O. This is a video game-oriented library and suits an emulator well. And, it's cross-platform.

The code base builds and has been tested on:
* MacBook Pro M1
* Mac Intel
* Mac Mini M4
* Ubuntu Linux 22.04 (x64 AMD)

I am hoping I can get someone else to build on Windows. :-)

# Building

General Build Instructions:

## Build for Production

Production builds are optimized for performance.

```
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

## Build for Debug

Debug enables a variety of assertions, turns off optimizations, and enables memory leak and buffer overrun checking.

```
cmake -DCMAKE_BUILD_TYPE=Debug .
make
```

## macOS

GSSquared will build on both Intel and Apple Silicon Macs.

gs2 is currently built on a Mac using:

* clang
* SDL3 downloaded and built from the SDL web site.
* git

I use vscode as my IDE, but, this isn't required.

```
git clone https://github.com/jawaidbazyar2/gssquared.git
cd gssquared/vendored
git clone https://github.com/libsdl-org/SDL.git
git clone https://github.com/libsdl-org/SDL_image.git
cd ..
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

### Mac App Bundle

After building, you can create a Mac App Bundle and .dmg file. The packages are built into the packages/ directory.

```
make packages
```


## Linux

gs2 is currently build on Linux using:

* clang
* SDL3 downloaded and built from the SDL web site.
* SDL_image downloaded and built from the SDL web site.
* git
* You need the following libraries installed:
    * libasound2-dev
    * libpulse-dev
    * libudev-dev

```
git clone https://github.com/jawaidbazyar2/gssquared.git
cd gssquared
mkdir -p vendored
cd vendored
git clone https://github.com/libsdl-org/SDL.git
git clone https://github.com/libsdl-org/SDL_image.git
cd ..
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

### Linux App Distribution

After building, you can create a folder that contains libraries and assets for linux.

```
make packages
```

Creates packages/linux-cli/ directory that has a runnable binary.

## Windows

If you want to help create build tooling for Windows, let me know! All of this should work on all 3 platforms courtesy of SDL3.

## Creating Distribution Packages

```
make packages
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
