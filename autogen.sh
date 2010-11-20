#!/bin/sh

./bootstrap && ./configure --enable-test-programs --enable-plotting-programs && make
