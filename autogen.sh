#!/bin/sh -e

rm -f CMakeCache.txt
cmake -DCMAKE_INSTALL_PREFIX=/usr -DWITH_NEAT=1 -DBUILD_TEST_PROGRAMS=1 -DBUILD_PLOT_PROGRAMS=1 $@ .

cores=`getconf _NPROCESSORS_ONLN`
make -j${cores}
