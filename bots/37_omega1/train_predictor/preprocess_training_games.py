#!/usr/bin/python3
import os
import subprocess
import sys
import concurrent.futures

num_players = 4
game_dir = "/home/greg/coding/halite/2018/training_data/raw_games/{}p".format(num_players)
out_dir = "/home/greg/coding/halite/2018/training_data/preprocessed_games/{}p".format(num_players)
bot_binary = "/home/greg/coding/halite/2018/repo/bots/36_predictor/build/MyBot"

def preprocess_one_game(game_fn):
    bn = os.path.basename(game_fn)
    out_bn = bn.replace(".hlt", ".bin")
    out_fn = os.path.join(out_dir, out_bn)
    gzip_out_fn = out_fn + ".gz"

    if os.path.isfile(gzip_out_fn):
        print("skipping game {} b/c {} already exists".format(game_fn, gzip_out_fn))
        return

    my_pid = int(out_bn.split(".pid")[-1].split(".")[0])

    bot_cmd = "{} NOLOG=1 JUST_MAKE_TRAINING_DATA=1 TRAINING_DATA_SAVE_FILENAME={}".format(bot_binary, out_fn)

    cmd = [
        "/home/greg/coding/halite/2018/reloader/reload.py",
        game_fn,
        str(my_pid),
        bot_cmd
    ]
    print("running {}".format(cmd))
    subprocess.check_output(cmd)

    # gzip the resulting file to save space
    cmd = [
        "/bin/gzip",
        out_fn
    ]
    print("running {}".format(cmd))
    subprocess.check_output(cmd)

# parallelize. Since most work is done in a subprocess we can use threads
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as e:
    game_fns = [os.path.join(game_dir, bn) for bn in os.listdir(game_dir)]
    game_fns.sort()
    game_fns = game_fns[-300:]
    e.map(preprocess_one_game, game_fns)

