#!/bin/sh

# Little script to build
qmake -r
make -j$(nproc)