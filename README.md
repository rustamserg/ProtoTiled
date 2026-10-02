# ProtoTiled
Example of Tiled render game

Written in C23 on top of [raylib](https://www.raylib.com/), Tiled `.tmx`/`.tsx` files are read by a small built-in XML parser.

## Building
MSVC's C compiler does not support C23 yet, so the project uses the **ClangCL** platform toolset.
In Visual Studio Installer add the *C++ Clang Compiler for Windows* and *MSBuild support for LLVM (clang-cl) toolset* components, then restore NuGet packages and build.
