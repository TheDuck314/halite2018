#!/usr/bin/python3
import subprocess
import random
import datetime
import os
import json
import glob
import argparse
from multiprocessing import Process, Queue

HALITE_BINARY = "/home/greg/coding/halite/2018/starter_kit_cpp/halite"

BOT_COMMANDS = {
    "Zeta":    "/home/greg/coding/halite/2018/repo/bots/5_zeta/build/MyBot",
    "Eta":     "/home/greg/coding/halite/2018/repo/bots/6_eta/build/MyBot",
    "Theta":   "/home/greg/coding/halite/2018/repo/bots/7_theta/build/MyBot",
    "Iota":    "/home/greg/coding/halite/2018/repo/bots/8_iota/build/MyBot",
    "Kappa":   "/home/greg/coding/halite/2018/repo/bots/9_kappa/build/MyBot",
    "Lambda":  "/home/greg/coding/halite/2018/repo/bots/10_lambda/build/MyBot",
    "Mu":      "/home/greg/coding/halite/2018/repo/bots/11_mu/build/MyBot",
    "Nu":      "/home/greg/coding/halite/2018/repo/bots/12_nu/build/MyBot",
    "Xi":      "/home/greg/coding/halite/2018/repo/bots/13_xi/build/MyBot",
    "Omicron": "/home/greg/coding/halite/2018/repo/bots/14_omicron/build/MyBot",
}

def run_game(game_dir, seed, bots):
    command = [
        HALITE_BINARY,
        "--results-as-json",
        "-s", str(seed),
        "--replay-directory", game_dir,
    ]

    for bot in bots:
        command.append(BOT_COMMANDS[bot])

    with open(os.path.join(game_dir, "command.txt"), "w") as f:
        f.write("\n".join(command) + "\n")

    results_string = subprocess.check_output(command).decode()
    with open(os.path.join(game_dir, "results.json"), "w") as f:
        f.write(results_string)

    replay_glob = os.path.join(game_dir, "replay-*.hlt")
    replays = glob.glob(replay_glob)
    assert len(replays) == 1, replay_glob
    print(replays[0])

    results = json.loads(results_string)
    rank_to_bot = {}
    for player_id, stats in results["stats"].items():
        rank_to_bot[int(stats["rank"])] = bots[int(player_id)]
    print("  ".join("{}: {}".format(rank, bot) for rank, bot in rank_to_bot.items()))
    return rank_to_bot


def worker(input_q, output_q, match_dir):
    """ runs games indefinitely and puts the results in a Queue """
    print("worker start match_dir={}".format(match_dir))
    while True:
        (game_num, seed, bots) = input_q.get()        
        game_dir = os.path.join(match_dir, str(game_num))
        os.mkdir(game_dir)
        os.chdir(game_dir)  # so bot logs go in the game dir
        print("running game {} in {}".format(game_num, game_dir))
        result = run_game(game_dir, seed, bots)
        output_q.put(result)



def print_bot_to_stats(bots, bot_to_stats):
    print("--------------------")
    for bot in bots:
        line = "{:>55}".format(bot)
        for num_players in [2, 4]:
            rank_to_counts = bot_to_stats[bot][num_players]
            count_str = "-".join(map(str, ["{:>2}".format(rank_to_counts[rank]) for rank in range(1, num_players+1)]))
            line += "    {}p:  {}".format(num_players, count_str)
        print(line)
#            for rank in range(1, num_players+1):
#                print("    Rank {}:  {} times".format(rank, [rank]))
    print("--------------------")

def get_seed():
    return random.randint(1, 999999)

def get_num_players(force_player_count):
    if force_player_count is not None:
        return force_player_count
    else:
        # The matchmaking server chooses 2 player and 4 player games with
        # equal probability. So conditional on us being in the game, there's
        # a 2/3 chance it's a four-player game.
        return 2 if random.random() < 1.0/3.0 else 4

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--players", type=int, help="force a given number of players")
    args = parser.parse_args()

    challenger_bots = ["Omicron"]
    ref_bot = "Xi"
    """
    challenger_base = "Omicron"
    param = "MINING_DIST_FROM_ME_MULT"
    values = [3.0, 2.0]
    challenger_bots = []
    for val in values:
        override = param + "=" + str(val)
        challenger_bot = challenger_base + "." + override       
        challenger_bots.append(challenger_bot)
        BOT_COMMANDS[challenger_bot] = BOT_COMMANDS[challenger_base] + " " + override

    ref_bot = challenger_bots.pop()
    """

    bots = challenger_bots + [ref_bot]

    bot_to_stats = {}
    for bot in bots:
        bot_to_stats[bot] = {}
        for num_players in [2, 4]:
            bot_to_stats[bot][num_players] = {}
            for rank in range(1, num_players+1):
                bot_to_stats[bot][num_players][rank] = 0

#    match_dir = os.path.join(os.getcwd(), "matches", "{}_{}".format(datetime.datetime.now().strftime("%Y%m%d_%H.%M.%S"), "_".join(bots)))
    match_dir = os.path.join(os.getcwd(), "matches", datetime.datetime.now().strftime("%Y%m%d_%H.%M.%S"))
    assert not os.path.isdir(match_dir)
    os.mkdir(match_dir)

    game_num = 0

    input_q = Queue()
    output_q = Queue()
    num_workers = 3
    for _ in range(num_workers):
        t = Process(target=worker,
                    args=(input_q, output_q, match_dir))
        t.daemon = True
        t.start()

    while True:
        # if necessary, queue up more games for the workers to play
        if input_q.qsize() < 3 * num_workers:
            print("queue is small, adding jobs")
            for _ in range(3):
                # Queue up one game for each challenger bot. All the games will use the same seed
                # and the challenger bot will have the same player id in each game, to reduce
                # variance.
                seed = get_seed()  # each challenger plays a game on the same seed, to reduce variance
                num_players = get_num_players(args.players)
                game_bots_template = [ref_bot] * num_players
                challenger_pid = random.randint(0, num_players - 1)
                for challenger_bot in challenger_bots:
                    game_bots = list(game_bots_template)
                    game_bots[challenger_pid] = challenger_bot
                    game_spec = (game_num, seed, game_bots)
                    input_q.put(game_spec)
                    game_num += 1

        # get the results of a game and update our stats
        rank_to_bot = output_q.get()
        num_players = len(rank_to_bot)
        for rank, bot in rank_to_bot.items():
            bot_to_stats[bot][num_players][rank] += 1

        print_bot_to_stats(bots, bot_to_stats)


if __name__ == "__main__":
    main()
