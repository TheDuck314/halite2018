#!/bin/bash

cat $1 | zstd -d | tr ',' '\n' | grep map_generator_seed | awk -F: '{print $2}'
