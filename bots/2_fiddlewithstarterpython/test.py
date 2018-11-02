#!/usr/bin/env python3

# Import the Halite SDK, which will let you interact with the game.
import hlt

# This library contains constant values.
from hlt import constants

# This library contains direction metadata to better interface with the game.
from hlt.positionals import Direction, Position

# This library allows you to generate random numbers.
import random

myset = {Position(1, 2), Position(1, 3)}

print(Position(1, 2))
print(myset)
print(Position(1, 2) in myset)
print(Position(1, 4) in myset)
