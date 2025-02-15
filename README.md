# GSSquared - A Complete Apple II Series Emulator.

GSSquared is a complete emulator for the Apple II series of computers. It is written in C++ and runs on Windows, Linux, and macOS.

It uses the SDL3 library for graphics, sound, and I/O. This is a video game-oriented library and suits an emulator well. And, it's cross-platform.

To date, development has been Mac-only. The code base builds on a Mac M1.

Eventually (hopefully, soon) I will build for Linux. I am hoping I can get someone else to build on Windows. :-)

# Building

General Build Instructions:

## Build for Production

```
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

## Build for Debug

```
cmake -DCMAKE_BUILD_TYPE=Debug .
make
```

## MacOS

gs2 is currently built on a Mac using:

* clang
* SDL3 downloaded and built from the SDL web site.
* git

I use vscode as my IDE, but, this isn't required.

```
git clone xxxxxxx
cd xxxxxxx
mkdir build
mkdir vendored
cd vendored
git clone https://github.com/libsdl-org/SDL.git
git clone https://github.com/libsdl-org/SDL_image.git
cd ..
mkdir build
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

### Mac App Bundle

Gets installed into GSSquared.app/

```
cmake -DCMAKE_BUILD_TYPE=Release
make mac-package
```


## Linux

gs2 is currently build on Linux using:

* clang
* SDL3 downloaded and built from the SDL web site.
* SDL_image downloaded and built from the SDL web site.
* git

```
git clone ******
cd ******
mkdir vendored
cd vendored
git clone https://github.com/libsdl-org/SDL.git
git clone https://github.com/libsdl-org/SDL_image.git
cd ..
mkdir build
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

## Windows

If you want to help create build tooling for Windows, let me know! All of this should work on all 3 platforms courtesy of SDL3.

