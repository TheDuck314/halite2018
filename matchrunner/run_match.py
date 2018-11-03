#!/usr/bin/python3
import subprocess
import random
import datetime
import os
import json
import glob
#from threading import Thread
#from queue import Queue
from multiprocessing import Process, Queue

HALITE_BINARY = "/home/greg/coding/halite/2018/starter_kit_cpp/halite"

BOT_COMMANDS = {
    "Zeta":   "/home/greg/coding/halite/2018/repo/bots/5_zeta/build/MyBot",
    "Eta":    "/home/greg/coding/halite/2018/repo/bots/6_eta/build/MyBot",
    "Theta":  "/home/greg/coding/halite/2018/repo/bots/7_theta/build/MyBot",
    "Iota":   "/home/greg/coding/halite/2018/repo/bots/8_iota/build/MyBot",
    "Kappa":  "/home/greg/coding/halite/2018/repo/bots/9_kappa/build/MyBot",
    "Lambda": "/home/greg/coding/halite/2018/repo/bots/10_lambda/build/MyBot",
    "Mu":     "/home/greg/coding/halite/2018/repo/bots/11_mu/build/MyBot",
    "Nu":     "/home/greg/coding/halite/2018/repo/bots/12_nu/build/MyBot",
}

def run_game(game_dir, seed, bot0, bot1, force_player_count):
    # The matchmaking server chooses 2 player and 4 player games with
    # equal probability. So conditional on us being in the game, there's
    # a 2/3 chance it's a four-player game.
    if force_player_count is not None:
        num_players = force_player_count
    else:
        num_players = 2 if random.random() < 1.0/3.0 else 4
    
    command = [
        HALITE_BINARY,
        "--results-as-json",
        "-s", str(seed),
        "--replay-directory", game_dir,
    ]

    player_id_to_bot = [bot0]
    for i in range(1, num_players):
        player_id_to_bot.append(bot1)
    # shuffle so bot0 isn't always player id 0
    random.shuffle(player_id_to_bot)

    for i in range(num_players):
        command.append(BOT_COMMANDS[player_id_to_bot[i]])

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
        rank_to_bot[int(stats["rank"])] = player_id_to_bot[int(player_id)]
    print("  ".join("{}: {}".format(rank, bot) for rank, bot in rank_to_bot.items()))
    return rank_to_bot


def worker(input_q, output_q, match_dir, run_game_kwargs):
    """ runs games indefinitely and puts the results in a Queue """
    print("workter start match_dir={} run_game_kwargs={}".format(match_dir, run_game_kwargs))
    while True:
        (game_num, seed) = input_q.get()        
        game_dir = os.path.join(match_dir, str(game_num))
        os.mkdir(game_dir)
        os.chdir(game_dir)  # so bot logs go in the game dir
        print("running game {} in {}".format(game_num, game_dir))
        result = run_game(game_dir, seed, **run_game_kwargs)
        output_q.put(result)



def print_bot_to_stats(bots, bot_to_stats):
    print("--------------------")
    for bot in bots:
        print("Bot {}".format(bot))
        for num_players in [2, 4]:
            print("  {} player games:".format(num_players))
            for rank in range(1, num_players+1):
                print("    Rank {}:  {} times".format(rank, bot_to_stats[bot][num_players][rank]))
    print("--------------------")

def get_seed():
    return random.randint(1, 999999)

def main():
    bot0 = "Nu"
    bot1 = "Mu"
    #force_player_count = 2
    force_player_count = 4
    #force_player_count = None

    run_game_kwargs = dict(
        bot0 = bot0,
        bot1 = bot1,
        force_player_count = force_player_count
    )

    bots = [bot0, bot1]

    bot_to_stats = {}
    for bot in bots:
        bot_to_stats[bot] = {}
        for num_players in [2, 4]:
            bot_to_stats[bot][num_players] = {}
            for rank in range(1, num_players+1):
                bot_to_stats[bot][num_players][rank] = 0

    match_dir = os.path.join(os.getcwd(), "matches", "{}_{}_{}".format(datetime.datetime.now().strftime("%Y%m%d_%H.%M.%S"), bot0, bot1))
    assert not os.path.isdir(match_dir)
    os.mkdir(match_dir)

    game_num = 0

    input_q = Queue()
    output_q = Queue()
    num_workers = 3
    for i in range(num_workers):
        t = Process(target=worker,
                   args=(input_q, output_q, match_dir, run_game_kwargs))
        t.daemon = True
        t.start()

    initial_q_size = num_workers + 5
    for i in range(initial_q_size):
        input_q.put((game_num, get_seed()))
        game_num += 1

    while True:
        rank_to_bot = output_q.get()
        num_players = len(rank_to_bot)
        for rank, bot in rank_to_bot.items():
            bot_to_stats[bot][num_players][rank] += 1

        print_bot_to_stats(bots, bot_to_stats)

        input_q.put((game_num, get_seed()))
        game_num += 1

if __name__ == "__main__":
    main()
