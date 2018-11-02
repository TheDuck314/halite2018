#!/bin/bash

set -ex

for fn in hlt/*.cpp ; do
    g++ -std=c++14 -c $fn
done

g++ -std=c++14 -o MyBot MyBot.cpp *.o

