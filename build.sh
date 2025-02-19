#!/bin/bash

# cmake 这样编译是编译 lib, 不会编译 example
cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.tbox
cmake --build build
cmake --install build

ls $HOME/.tbox 
#include  lib

# 可以直接 make , 会编译 lib 和 examples
# ls .install .staging
# .install:
# bin  etc  lib
#
# .staging:
# include  lib
