#!/usr/bin/python3

import requests
import pandas as pd
from collections import defaultdict

my_userid = 3191
my_username = "TheDuck314"

response = requests.get(
    "https://api.2018.halite.io/v1/api/user/{}/match?order_by=desc,time_played&limit=500"
    .format(my_userid)
).json()


my_stats = []

opponent_counts = defaultdict(lambda: 0)

for game_dict in response:
    # let's discard challenge games
    if game_dict["challenge_id"] is not None:
        continue

    my_rank = -1
    my_version = -1
    players_present = []
    winner_halite = -1

    rank_to_halite = {}
    for player_stats_dict in game_dict["stats"]["player_statistics"]:
        rank = player_stats_dict["rank"]
        halite = player_stats_dict["final_production"]
        rank_to_halite[rank] = halite

    bot_to_rank = {}
    for userid_str, player_dict in game_dict["players"].items():
        version = player_dict["version_number"]
        bot = "{} v{}".format(player_dict["username"], version)
        rank = player_dict["rank"]
        bot_to_rank[bot] = rank
        username = player_dict["username"]
        players_present.append(username)
        if int(userid_str) == my_userid:
            my_rank = rank
            my_version = version            
        else:
            opponent_counts[username] += 1
    game_id = game_dict["game_id"]
    replay_link = "https://halite.io/play/?game_id={}".format(game_id)
    
    my_stats.append({
        "NumPlayers": len(bot_to_rank),
        "MapSize": game_dict["map_height"],
        "MyRank": my_rank,
        "Replay": replay_link,
        "Players": sorted(players_present),
        "MyVersion": my_version,
        "MyHalite": rank_to_halite[my_rank],
        "WinnerHalite": rank_to_halite[1],
    })

my_stats = pd.DataFrame(my_stats)
#print(my_stats)

#my_stats = my_stats[my_stats.MyVersion == 21]
#my_stats = my_stats[my_stats.MyVersion == 22]
my_stats = my_stats[my_stats.MyVersion == 23]

pd.set_option("display.width", 300)

def with_player(stats, opp):
    return stats[stats.Players.apply(lambda x: opp in x)]

def header(title):
    print("============ {} ============".format(title.upper()))

def show_stats(stats, title):
    header(title)
    print("Rank by NumPlayers")
    print(stats.groupby(["NumPlayers","MyRank"]).size().unstack("MyRank").fillna(0.0).astype(int))
    print()
    print("Rank by NumPlayers and map size")
    print(stats.groupby(["NumPlayers","MapSize","MyRank"]).size().unstack("MyRank").fillna(0.0).astype(int))    
    print()
    print()

show_stats(my_stats, "overall")

for opp in ["Rachol", "zxqfl", "shummie", "ColinWHart"]:
    show_stats(with_player(my_stats, opp), "Games including {}".format(opp))

header("2p losses against rachol")
tmp = with_player(my_stats, "Rachol")
print(tmp[(tmp.NumPlayers == 2) & (tmp.MyRank == 2)].to_string())
print()
print()

header("2p losses in general")
tmp = my_stats[(my_stats.NumPlayers == 2) & (my_stats.MyRank == 2)][["MyRank","MapSize","Players","WinnerHalite","MyHalite","Replay"]].copy()
tmp["HaliteRatio"] = tmp["WinnerHalite"] / tmp["MyHalite"].astype(float)
print(tmp.sort_values(by="HaliteRatio", ascending=False).to_string())
print()
print()

header("opponent counts")
print(pd.Series(opponent_counts).sort_values(ascending=False))

#print(ret)