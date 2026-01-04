# SArena

__SArena__ is a tiny C header-only library that provides a simple to use arena allocator.

## Dependencies

This library is stand-alone.

## Makefile instructions:

This library can be used as a header-only library. In this case, using Make is unnecessary. The option to perform a proper install also exists - the steps involve compiling the library, generating a .pc file and placing them, together with the header, at the desired location on your system.

1. `make [PC_WITH_PATH=...] [LIB_TYPE=so/ar] [OPT={0...3}]` - This will compile the source files and build the library file. If the library depends on packages discovered via pkg-config, you can specify where to search for their .pc files, in addition to `PKG_CONFIG_PATH`.
2. `make install [LIB_TYPE=so/ar] [PREFIX=...] [PC_PREFIX=...]` - This will place the public headers inside `PREFIX/include` and the built library file inside `PREFIX/lib`. This will also place the .pc file inside `PC_PREFIX`.

Default options are `PREFIX=/usr/local`, `PC_PREFIX=PREFIX/lib/pkgconfig`, `OPT=3`, `LIB_TYPE=so`.

## Usage instructions:

If you are using the library as __header-only__, simply include the header in your project and define the implementation macro where needed.

If using the library in the __standard way__, compile your project with flags: `$(pkgconf --cflags nuterm)` and link with flags: `$(pkgconf --libs nuterm)`. For this to work, make sure that pkg-config searches in the directory of the .pc file generated in the installation process.
