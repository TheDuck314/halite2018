#!/bin/bash

set -ex

#unzip libtorch-shared-with-deps-latest.zip
wget https://download.pytorch.org/libtorch/nightly/cpu/libtorch-shared-with-deps-latest.zip
unzip libtorch-shared-with-deps-latest.zip
TORCH_PATH=$(pwd)/libtorch
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=${TORCH_PATH} ..
make
ln -s $(pwd)/MyBot ../MyBot

