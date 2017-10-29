#!/bin/bash

rm -f CMakeCache.txt
cmake -DCMAKE_INSTALL_PREFIX=/usr -DWITH_NEAT=0 -DBUILD_TEST_PROGRAMS=1 -DBUILD_PLOT_PROGRAMS=1 $@ .

# ------ Obtain number of cores ---------------------------------------------
# Try Linux
cores=`getconf _NPROCESSORS_ONLN 2>/dev/null`
if [ "$cores" == "" ] ; then
   # Try FreeBSD
   cores=`sysctl -a | grep 'hw.ncpu' | cut -d ':' -f2 | tr -d ' '`
fi
if [ "$cores" == "" ] ; then
   cores="1"
fi

gmake -j$cores || make -j$cores
