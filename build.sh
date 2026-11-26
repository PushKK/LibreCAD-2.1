#!/bin/sh

# Little script to build
qmake -r
#make -j$(nproc)
make -j$(nproc) CXX="g++ -Wfatal-errors" && echo "Сборка закончена."
