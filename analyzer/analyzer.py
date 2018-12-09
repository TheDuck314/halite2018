#!/usr/bin/python3

# replay format
# zstd compressed json

# top level "game_statistics" looks like this:
# {'number_turns': 426, 
#  'player_statistics': [
#     {'ships_given': 0, 
#      'max_entity_distance': 33, 
#      'interaction_opportunities': 4219, 
#      'self_collisions': 9, 
#      'average_entity_distance': 10, 
#      'total_mined': 90915, 
#      'total_mined_from_captured': 0, 
#      'random_id': 44001816, 
#      'all_collisions': 43, 
#      'mining_efficiency': 1.4375075620084694, 
#      'rank': 1, 
#      'total_bonus': 80140, 
#      'player_id': 0, 
#      'number_dropoffs': 1, 
#      'final_production': 86691, 
#      'ships_captured': 0, 
#      'halite_per_dropoff': [[{'x': 30, 'y': 21}, 4258], 
#                            [{'x': 11, 'y': 11}, 126433]], 
#      'last_turn_alive': 426,
#      'total_production': 130691}, 
#     one of those dicts for each player
#  ]
# }
#
# top level "map_generator_seed" is just an int like 1540751759
#
# top level "number_of_players" is just an int like 4
#
# top level "players" object looks like this:
# [{'entities': [], 'factory_location': {'y': 11, 'x': 11}, 'name': 'Rachol v60', 'energy': 5000, 'player_id': 0}, 
#  {'entities': [], 'factory_location': {'y': 11, 'x': 28}, 'name': 'Belonogov v21', 'energy': 5000, 'player_id': 1}, 
#  {'entities': [], 'factory_location': {'y': 28, 'x': 11}, 'name': 'zxqfl v11', 'energy': 5000, 'player_id': 2}, 
#  {'entities': [], 'factory_location': {'y': 28, 'x': 28}, 'name': 'TheDuck314 v17', 'energy': 5000, 'player_id': 3}]
#
# top level "production_map" looks like
# {'height': 40, 'width': 40, 'grid': [[a 2d array of cells of the form {'energy': 3}]]}
#
# top level "ENGINE_VERSION":"1.0.2.112.g884f"
#
# top level "GAME_CONSTANTS": {
# 'INSPIRED_BONUS_MULTIPLIER': 2.0, 
# 'MAX_TURNS': 425, 
# 'PERSISTENCE': 0.7, 
# 'SHIPS_ABOVE_FOR_CAPTURE': 3, 
# 'MAX_PLAYERS': 16, 
# 'CAPTURE_ENABLED': False, 
# 'MAX_TURN_THRESHOLD': 64, 
# 'INSPIRED_EXTRACT_RATIO': 4,
# 'MIN_TURNS': 400, 
# 'MIN_CELL_PRODUCTION': 900, 
# 'INSPIRED_MOVE_COST_RATIO': 10, 
# 'STRICT_ERRORS': False, 
# 'INSPIRATION_SHIP_COUNT': 2, 
# 'MOVE_COST_RATIO': 10, 
# 'INSPIRATION_ENABLED': True, 
# 'MIN_TURN_THRESHOLD': 32, 
# 'DROPOFF_COST': 4000, 
# 'MAX_ENERGY': 1000, 
# 'MAX_CELL_PRODUCTION': 1000, 
# 'CAPTURE_RADIUS': 3, 
# 'EXTRACT_RATIO': 4, 
# 'NEW_ENTITY_ENERGY_COST': 1000, 
# 'DROPOFF_PENALTY_RATIO': 4, 
# 'DEFAULT_MAP_HEIGHT': 40, 
# 'INITIAL_ENERGY': 5000, 
# 'INSPIRATION_RADIUS': 4, 
# 'DEFAULT_MAP_WIDTH': 40, 
# 'FACTOR_EXP_1': 2.0, 
# 'FACTOR_EXP_2': 2.0}
#
# top level "REPLAY_FILE_VERSION":3
# top level "full_frames": list of frames
#                note that full_frames[0] is pretty lame and has empty moves/entities/events/cells
#
# frame format:
# "cells": list of new halite values for some cells
# "deposited": probably total cumulative amount of halite returned by each player id, looks like {"0":0, "1":0, "2":0, "3":0}
# "energy": presumably total halite, looks like {"0":5000, "1":5000, "2":5000, "3":5000}
# "entities": dict of player id -> player ships
# "events": list of events
# "moves": dict of moves
#
# example "cells":
# [{'production': 123, 'x': 11, 'y': 10}, 
#  {'production': 134, 'x': 29, 'y': 11}, 
#  {'production': 123, 'x': 11, 'y': 29}, 
#  {'production': 136, 'x': 27, 'y': 28}]
# production is the new halite value of the cell
#
#
# example "moves" dict:
# {"0":[{"type":"g"}],
#  "1":[{"type":"g"}],
#  "2":[{"type":"g"}],
#  "3":[{"type":"g"}]}
#
# example "entities" dict:
# {"0":{"2":{"energy":0,"is_inspired":false,"x":11,"y":10},
#       "6":{"energy":0,"is_inspired":false,"x":11,"y":11}},
#  "1":{"3":{"energy":0,"is_inspired":false,"x":29,"y":11},
#       "7":{"energy":0,"is_inspired":false,"x":28,"y":11}},
#  "2":{"1":{"energy":0,"is_inspired":false,"x":11,"y":29},
#       "5":{"energy":0,"is_inspired":false,"x":11,"y":28}},
#  "3":{"0":{"energy":0,"is_inspired":false,"x":27,"y":28},
#       "4":{"energy":0,"is_inspired":false,"x":28,"y":28}}}
# So, top level key is player id, next level key is ship id (what about dropoffs?)
#
# example events list:
#  [{"energy":0,"id":8,"location":{"x":28,"y":28},"owner_id":3,"type":"spawn"},
#   {"energy":0,"id":9,"location":{"x":11,"y":28},"owner_id":2,"type":"spawn"},
#   {"energy":0,"id":10,"location":{"x":11,"y":11},"owner_id":0,"type":"spawn"},
#   {"energy":0,"id":11,"location":{"x":28,"y":11},"owner_id":1,"type":"spawn"}]
# there's a "shipwreck" event type for collisions; here's an example event:
#   {"location":{"x":4, "y":37}, "ships":[10, 157], "type":"shipwreck"}
# there's also an event for constructing:
#   {'type': 'construct', 'owner_id': 1, 'location': {'y': 39, 'x': 32}, 'id': 59}
#
# example moves dict:
# {"0":[{"direction":"o","id":2,"type":"m"},
#       {"direction":"s","id":6,"type":"m"},
#       {"type":"g"}],
#  "1":[{"direction":"o","id":3,"type":"m"},
#       {"direction":"n","id":7,"type":"m"},
#       {"type":"g"}],
#  "2":[{"direction":"o","id":1,"type":"m"},
#       {"direction":"e","id":5,"type":"m"},
#       {"type":"g"}],
#  "3":[{"direction":"n","id":4,"type":"m"},
#       {"direction":"o","id":0,"type":"m"},
#       {"type":"g"}]}}
# here's what a dropoff construction move looks like:
#  {"id":49, "type":"c"}
#
#
#

import json
import subprocess
import pandas as pd

#def show_df()

def turn_player_df(turn_player_dict, player_names):
    """ Take a dictionary whose key is playerid and whose values are lists of values on each turn
    make a dataframe"""
    df = pd.DataFrame(turn_player_dict)
    df.sort_index(axis=1, inplace=True)
    df.columns = player_names
    return df


def compute_carried_halite(frames, num_players, player_names):
    pid_to_carried_halite = {pid:[] for pid in range(num_players)}
    for turn, frame in enumerate(frames):
        for pid in range(num_players):
            pid_to_carried_halite[pid].append(0)
        for pid_str, entities in frame["entities"].items():
            pid = int(pid_str)
            for _, entity in entities.items():
                pid_to_carried_halite[pid][turn] += entity["energy"]
    return turn_player_df(pid_to_carried_halite, player_names)


def compute_stored_halite(frames, num_players, player_names):
    # note: somehow the deposited numbers are shifted one turn
    # earlier than I think they ought to be. so I'm padding with
    # one initial row of 5000s and skipping the last frame
    pid_to_stored_halite = {pid:[5000] for pid in range(num_players)}
    for turn, frame in enumerate(frames[:-1]):
        for pid_str, collected in frame["energy"].items():
            pid = int(pid_str)
            pid_to_stored_halite[pid].append(collected)
    return turn_player_df(pid_to_stored_halite, player_names)


def compute_collected_halite(frames, num_players, player_names):
    # note: somehow the deposited numbers are shifted one turn
    # earlier than I think they ought to be. so I'm padding with
    # one initial row of 0s and skipping the last frame
    pid_to_collected_halite = {pid:[0] for pid in range(num_players)}
    for turn, frame in enumerate(frames[:-1]):
        for pid_str, collected in frame["deposited"].items():
            pid = int(pid_str)
            pid_to_collected_halite[pid].append(collected)
    return turn_player_df(pid_to_collected_halite, player_names)


def compute_spent_halite(frames, constants, num_players, player_names):
    # some fudging at the beginning and end to align with fluorine
    pid_to_collected_halite = {pid:[0, 0] for pid in range(num_players)}
    for turn, frame in enumerate(frames[1:-1]):
        for pid in range(num_players):
            pid_to_collected_halite[pid].append(pid_to_collected_halite[pid][-1])
        for event in frame["events"]:
            if event["type"] == "spawn":
                pid_to_collected_halite[event["owner_id"]][-1] += constants["NEW_ENTITY_ENERGY_COST"]
            elif event["type"] == "construct":
                # TODO: is this right, or is the "cost" reduced by the halite on the square?
                pid_to_collected_halite[event["owner_id"]][-1] += constants["DROPOFF_COST"]
    return turn_player_df(pid_to_collected_halite, player_names)


def main():
    replay_fn = "/home/greg/coding/halite/2018/live_games/1082487.hlt"
    decompress_cmd = "cat {} | zstd -d".format(replay_fn)
    raw_json = subprocess.check_output(decompress_cmd, shell=True).decode()
    replay = json.loads(raw_json)

    num_players = replay["number_of_players"]
    frames = replay["full_frames"]
    constants = replay["GAME_CONSTANTS"]

    player_names = [None] * num_players
    for player_dict in replay["players"]:
        name = player_dict["name"]
        # let's get rid of the version numbers for now
        name = " ".join(name.split()[:-1])
        player_names[player_dict["player_id"]] = name
    #print(player_names)

    carried = compute_carried_halite(frames, num_players, player_names)
    collected = compute_collected_halite(frames, num_players, player_names)
    stored = compute_stored_halite(frames, num_players, player_names)
    spent = compute_spent_halite(frames, constants, num_players, player_names)
    carried_plus_stored = carried + stored
    carried_plus_stored_plus_spent = carried_plus_stored + spent
    carried_plus_stored_plus_spent_minus_5000 = carried_plus_stored_plus_spent - 5000

    #print(carried_plus_stored_plus_spent.to_string())
    print(carried_plus_stored_plus_spent_minus_5000.to_string())

    #print(replay["GAME_CONSTANTS"])
    #print("\n".join(str(key) + " " + str(value) for (key, value) in data["GAME_CONSTANTS"].items()))
    #print("\n".join(sorted(data.keys())))
    #print(len(data["full_frames"]))
    #print(data["full_frames"][0])
#    for f in data["full_frames"]:
#        for player_id, player_entities in f["entities"].items():
#            for entity_id, entity in player_entities.items():
#                print(player_id, entity_id, entity)
#    for f in data["full_frames"]:
#        for e in f["events"]:
#            print(e)

if __name__ == "__main__":
    main()