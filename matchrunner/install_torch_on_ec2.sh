#!/bin/bash

if [ ! -d "./libtorch" ] ; then
    wget https://download.pytorch.org/libtorch/nightly/cpu/libtorch-shared-with-deps-latest.zip
    unzip libtorch-shared-with-deps-latest.zip
fi

