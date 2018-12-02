#!/bin/bash

cd ./build
binary=$(pwd)/MyBot
opp_binary=$(pwd)/../../28_omegatimestwo/build/MyBot

cd ../replays
/home/greg/coding/halite/2018/starter_kit_python3/halite \
    -s 591845 \
    --replay-directory . \
    "$opp_binary" "$binary DROPOFF_PENALTY_FRAC_PER_DIST=0.04" "$opp_binary" "$opp_binary"
