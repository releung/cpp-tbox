#!/bin/bash

cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.tbox
cmake --build build
cmake --install build

