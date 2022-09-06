#!/bin/bash
mkdir libxmlb/build
mkdir libs
cd libxmlb/build
meson
ninja
cp src/libxmlb.so* ../../libs/
cd ../..
make
export LD_LIBRARY_PATH=./libs
valgrind --num-callers=25 ./test_xmlb
