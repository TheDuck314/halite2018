#!/usr/bin/python3

import requests
import pandas as pd
from collections import defaultdict

my_userid = 3191; my_username = "TheDuck314"; only_version = -1
#my_userid = 134; my_username = "Rachol"; only_version = -1

games_per_request = 250  # 250 is api max I think
response = []
for i in range(2):
    url = "https://api.2018.halite.io/v1/api/user/{}/match?order_by=desc,time_played&limit={}&offset={}".format(my_userid, games_per_request, i * games_per_request)
    print(url)
    response.extend(requests.get(url).json())

print("response contains {} games".format(len(response)))

my_stats = []

opponent_counts = defaultdict(lambda: 0)

seen_game_ids = set()

for game_dict in response:
    # I think we could potentially get duplicates b/c of the pagination; deal with that:
    game_id = game_dict["game_id"]
    if game_id in seen_game_ids:
        print("discarding duplicate game_id {}".format(game_id))
        continue
    seen_game_ids.add(game_id)

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
    winner = "????"
    for userid_str, player_dict in game_dict["players"].items():
        version = player_dict["version_number"]
        bot = "{} v{}".format(player_dict["username"], version)
        rank = player_dict["rank"]
        bot_to_rank[bot] = rank
        username = player_dict["username"]
        if rank == 1:
            winner = username
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
        "Winner": winner,
    })

print("{} non-challenge games".format(len(my_stats)))

my_stats = pd.DataFrame(my_stats)
#print(my_stats)

if only_version == -1:
    only_version = my_stats.MyVersion.max()
    print("Using {} latest version: {}".format(my_username, only_version))

my_stats = my_stats[my_stats.MyVersion == only_version]
print("{} games for version {}".format(len(my_stats), only_version))

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

for opp in ["TheDuck314", "Rachol", "zxqfl", "shummie", "ColinWHart", "Belonogov", "SiestaGuru", "ArtemisFowl17"]:
    if opp == my_username:
        continue
    show_stats(with_player(my_stats, opp), "Games including {}".format(opp))

header("2p losses against rachol")
tmp = with_player(my_stats, "Rachol")
print(tmp[(tmp.NumPlayers == 2) & (tmp.MyRank == 2)].to_string())
print()
print()

header("2p losses in general")
tmp = my_stats[(my_stats.NumPlayers == 2) & (my_stats.MyRank == 2)][["MyRank","MapSize","Players","Winner","WinnerHalite","MyHalite","Replay"]].copy()
tmp["HaliteRatio"] = tmp["WinnerHalite"] / tmp["MyHalite"].astype(float)
print(tmp.sort_values(by="HaliteRatio", ascending=False).to_string())
print()
print()

header("4p last places")
tmp = my_stats[(my_stats.NumPlayers == 4) & (my_stats.MyRank == 4)][["MyRank","MapSize","Players","Winner","WinnerHalite","MyHalite","Replay"]].copy()
tmp["HaliteRatio"] = tmp["WinnerHalite"] / tmp["MyHalite"].astype(float)
print(tmp.sort_values(by="HaliteRatio", ascending=False).to_string())
print()
print()

header("4p third places")
tmp = my_stats[(my_stats.NumPlayers == 4) & (my_stats.MyRank == 3)][["MyRank","MapSize","Players","Winner","WinnerHalite","MyHalite","Replay"]].copy()
tmp["HaliteRatio"] = tmp["WinnerHalite"] / tmp["MyHalite"].astype(float)
print(tmp.sort_values(by="HaliteRatio", ascending=False).to_string())
print()
print()

header("4p second places")
tmp = my_stats[(my_stats.NumPlayers == 4) & (my_stats.MyRank == 2)][["MyRank","MapSize","Players","Winner","WinnerHalite","MyHalite","Replay"]].copy()
tmp["HaliteRatio"] = tmp["WinnerHalite"] / tmp["MyHalite"].astype(float)
print(tmp.sort_values(by="HaliteRatio", ascending=False).to_string())
print()
print()

header("2p results by opponent")
tmp = my_stats[(my_stats.NumPlayers == 2)].copy()
tmp["Opponent"] = tmp["Players"].apply(lambda ps: next(p for p in ps if p != my_username))
tmp = tmp.groupby(["Opponent", "MyRank"]).size().unstack("MyRank").fillna(0.0).astype(int)
tmp["TotalGames"] = tmp.sum(axis=1)
tmp.sort_values(by="TotalGames", ascending=False, inplace=True)
tmp.drop("TotalGames", axis=1, inplace=True)
print(tmp.to_string())
print()
print()

header("opponent counts")
print(pd.Series(opponent_counts).sort_values(ascending=False))

#print(ret)
