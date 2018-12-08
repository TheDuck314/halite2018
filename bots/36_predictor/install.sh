#!/bin/bash

# get pytorch
wget https://download.pytorch.org/libtorch/nightly/cpu/libtorch-shared-with-deps-latest.zip 2>/dev/null
unzip libtorch-shared-with-deps-latest.zip

# make, pointing cmake to pytorch
cmake -DCMAKE_PREFIX_PATH=./libtorch .
make

# save some space or else the compile script complains
# about us using too much
rm libtorch-shared-with-deps-latest.zip
rm libtorch/lib/*.a

pwd
ls -la

