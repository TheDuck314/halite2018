#!/usr/bin/env python3

# Logging allows you to save messages for yourself. This is required because the regular STDOUT
#   (print statements) are reserved for the engine-bot communication.
import logging


logging.basicConfig(
    filename="commdump.log",
    filemode="w",
    level=logging.DEBUG,
)

def read_line():
    line = input()
    logging.info("got line: '{}'".format(line))
    return line

def send_line(msg):
    logging.info("sending line: '{}'".format(msg))
    print(msg)


logging.info("first we get a JSON object with all the constants:")
read_line()
logging.info("next we get '<# of players> <my player id>'")
read_line()
logging.info("next we get a line for each player of the form 'playerid shipyard_x shipyard_y'")
read_line()
read_line()
logging.info("next we get a 'mapwidth mapheight'")
read_line()
logging.info("next we get the width x height grid of halite on each cell")
for i in range(32):
    read_line()
logging.info("That's all the initial info we get. Once we're done initializing the game expects us to respond with our name")
send_line("mysweetbot")

logging.info("Now we're into the main game loop")

while True:
    logging.info("")
    logging.info("Turn start")
    logging.info("We get the turn number:")
    read_line()
    logging.info("For each player we get 'player num_ships num_dropoffs halite'")
    for i in range(2):
        logging.info("Player start:")
        line = read_line()
        playerid, num_ships, num_dropoffs, halite = map(int, line.split())
        logging.info("Then for each ship of that player we get 'ship_id x_position y_position halite'")
        for j in range(num_ships):
            read_line()
        logging.info("Then for each dropoff of that player we get 'id x_position y_position'")
        for j in range(num_dropoffs):
            read_line()
    logging.info("Next we get an integer telling us how many cells are about to have their halite updated:")
    line = read_line()
    logging.info("Updates have the form 'cell_x cell_y cell_halite'")
    num_cell_updates = int(line.strip())
    for i in range(num_cell_updates):
        read_line()
    logging.info("Now the game expects our commands as a single space-separated line.")
    send_line("g")


logging.shutdown()

