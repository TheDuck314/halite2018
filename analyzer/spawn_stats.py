#!/usr/bin/python3

import json
import subprocess
import os

class SpawnStats:
    def __init__(self, map_size, num_players):
        self.map_size = map_size
        self.num_players = num_players
        self.last_spawn_turn = 0
        self.profit_at_last_spawn_turn = 5000
        self.ships_at_last_spawn_turn = 0
        self.final_profit = 5000

    def __repr__(self):
        return "SpawnStats(map_size={}, num_players={}, last_spawn_turn={}, profit_at_last_spawn_turn={}, ships_at_last_spawn_turn={}, final_profit={})".format(
            self.map_size, self.num_players, self.last_spawn_turn, self.profit_at_last_spawn_turn, self.ships_at_last_spawn_turn, self.final_profit
        )


def analyze_one_game(replay_fn):
    decompress_cmd = "cat {} | zstd -d".format(replay_fn)
    raw_json = subprocess.check_output(decompress_cmd, shell=True).decode()
    replay = json.loads(raw_json)

    num_players = replay["number_of_players"]
    frames = replay["full_frames"]
    map_size = replay["production_map"]["width"]
    #constants = replay["GAME_CONSTANTS"]

    player_names = [None] * num_players
    for player_dict in replay["players"]:
        name = player_dict["name"]
        # let's get rid of the version numbers for now
        name = " ".join(name.split()[:-1])
        player_names[player_dict["player_id"]] = name

    spawn_stats = [SpawnStats(map_size, num_players) for _ in range(num_players)]

    for turn, frame in enumerate(frames):
        # update everyone's final_profit
        for pid_str, profit in frame["energy"].items():
            pid = int(pid_str)
            spawn_stats[pid].final_profit = profit

        # update at_last_spawn_turn stats for players who spawned
        for event in frame["events"]:
            if event["type"] == "spawn":
                pid = event["owner_id"]
                spawn_stats[pid].last_spawn_turn = turn
                spawn_stats[pid].profit_at_last_spawn_turn = spawn_stats[pid].final_profit
                num_ships = len(frame["entities"][str(pid)])
                spawn_stats[pid].ships_at_last_spawn_turn = num_ships

    for name, stats in zip(player_names, spawn_stats):
        if name == "TheDuck314":
            return stats


def main():
    game_dir = "/home/greg/coding/halite/2018/training_data/raw_games/4p/"
    for bn in sorted(os.listdir(game_dir))[::-1]:
        game_fn = os.path.join(game_dir, bn)
        stats = analyze_one_game(game_fn)
        ratio = (stats.final_profit - stats.profit_at_last_spawn_turn) / stats.ships_at_last_spawn_turn
        if stats.map_size == 48 and stats.num_players == 4:
            print("ratio = {:.1f}    {}".format(ratio, stats))

if __name__ == "__main__":
    main()
