#!/usr/bin/python3

import requests
import os
from collections import namedtuple

my_userid = 3191
#my_username = "TheDuck314"
min_version = 29
out_dir = "/home/greg/coding/halite/2018/training_data/raw_games"


games_per_request = 250  # 250 is api max I think
response = []
for i in range(10):
    url = "https://api.2018.halite.io/v1/api/user/{}/match?order_by=desc,time_played&limit={}&offset={}".format(my_userid, games_per_request, i * games_per_request)
    print(url)
    response.extend(requests.get(url).json())

GameInfo = namedtuple("GameInfo", ["game_id", "num_players", "my_pid"])
infos = []
seen_game_ids = set()

for game_dict in response:
    # let's discard challenge games
    if game_dict["challenge_id"] is not None:
        continue

    game_id = game_dict["game_id"]

    # I think we could potentially get duplicates b/c of the pagination; deal with that:
    if game_id in seen_game_ids:
        continue
    seen_game_ids.add(game_id)

    # Figure out which pid is me
    my_pid = None
    for userid_str, player_dict in game_dict["players"].items():
        if int(userid_str) == my_userid:
            my_pid = player_dict["player_index"]
    if my_pid is None:
        raise Exception("couldn't figure out my pid for game {}".format(game_id))

    infos.append(GameInfo(
        game_id = game_id,
        num_players = len(game_dict["players"]),
        my_pid  = my_pid
    ))

def download_game(out_dir, info):    
    out_fn = os.path.join(out_dir, "{}p".format(info.num_players), "{}.pid{}.hlt".format(info.game_id, info.my_pid))
    if os.path.isfile(out_fn):
        print("already downloaded game {}".format(info.game_id))
        return
    url = "https://api.2018.halite.io/v1/api/user/0/match/{}/replay".format(info.game_id)
    result = requests.get(url)
    # let's have the filename say which pid is me, so I don't have to later parse the 
    # game file to figure it out
    with open(out_fn, "wb") as f:
        f.write(result.content)
    print("wrote {}".format(out_fn))

infos.sort(key=lambda x: -x.game_id)

for i in range(1000):
    download_game(out_dir, infos[i])
