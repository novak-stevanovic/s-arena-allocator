# S-arena

__S-arena__ is a simple arena allocator written in C. It provides the basic features: allocation, rewinding, resetting.

## Makefile instructions:

1. make \[LIB\_TYPE=shared/archive\] - This will compile the source files and build the .so/.a file.
2. make install \[PREFIX={prefix}\] \[LIB\_TYPE=shared/archive\] - This will place the header file inside _prefix_/include and the built .so/.a file inside _prefix_/lib.

_Default options are PREFIX=/usr/local and default LIB\_TYPE=shared._
