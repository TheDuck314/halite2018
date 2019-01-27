# halite2018

This is my entry for [Halite 3](https://halite.io/). My final uploaded version is in bots/41_aleph4 and will probably finish 6th.

`versions.txt` has brief notes on the changes in each uploaded version and also the mu of most versions. 

All the strategy is in `bots/41_aleph4/MyBot.cpp`, and supporting code is in `bots/41_aleph4/hlt/`

Below is a brief post-mortem with a discussion of the history of my bot, my process for working on it, and some details about how the various systems work.

## History

I started with the python starter kit. After a few days my bot started getting slow, so I rewrote everything from scratch in C++, this time not using a starter kit. Switching to a compiled language was definitely a great move, since it let me worry much less about optimization, and let me run test games faster.

Within the first week I wrote the mining system and traffic control/navigation system that I would use for the rest of the competition. For mining each ship scored every square on the board and targeted the square with the best score. Navigation/traffic control was responsible for figuring out the moves each ship should make this turn to make progress toward its target, while avoiding self-collisions. This plus some basic dropoff code was enough to get top 5. The four above me were Rachol-zxqfl-ColinWHart-Belonogov. 

During this period I could get huge improvements from incrementally improving the mining, mostly by improving the formula for scoring squares. A couple iterations of this brought me about even with Rachol at rank #1.

I pulled ahead of Rachol and into clear #1 mostly by adding combat. In 2p, I was fairly aggressive about ramming enemy ships when I had a local numbers advantage, and in 4p I added a little bit of ramming at the end of the game.

Around this time I also started thinking about collision avoidance in 4p, which I think is a pretty deep problem. I wrote some code to exploit very-cautious opponents in 4p, which I talk about a bit more below. This was a small boost.

All this got me to about 94-95 mu, around the second week of November. At this point I probably had my biggest lead over #2. From this point on I found it much harder to find improvements and they were all incremental changes to the existing systems. Teccles soon caught up and surpassed me and eventually so did a bunch of others.

## Process

fohristiwhirl's [flourine](https://github.com/fohristiwhirl/fluorine) replay viewer was invaluable, especially the flog feature which let you write log messages for individual ships or for individual squares on the map, or display information by coloring the squares on the map.

My process for finding improvements was a combination of close analysis of replays in fluorine to find things I could be doing better, and running large batches of self-play games on a spot AWS c5.18xlarge instance to test changes. I only committed changes that looked clearly better in self-play, or were even in self-play and I had a strong belief would help against real opponents online.

To determine the replays to analyze, I had a script that would download my online games and make a table of my losses categorized by 2p vs 4p/opponent/map size, and sorted by `(winner halite) / (my halite)`. Then I would look at the worst losses.

## Bot details

For anyone interested, here are details of how the various systems worked in my bot. You can also try reading `bots/41_aleph4/MyBot.cpp` itself; it is not very clean but I tend to write lots of comments explaining to myself how each piece of code is going to work, so it might be possible to make sense of it.

### Traffic control

The main strategy code determines a "purpose" and "destination" for every ship. Then the traffic control system decides which move each ship will make. The possible purposes are
1. **Mine**: go to a destination square and mine it
2. **Return**: bring halite back to a dropoff
3. **Flee**: run away from an enemy who we think might ram us
4. **Ram**: try to collide with an adjacent enemy
5. **Stuck**: for ships that don't have enough halite to move

We go through ships one by one and find paths to their destination. When two ships plan to move onto the same square the one with the higher-number "purpose" gets priority and the other one has to re-route.

There's a special case where if a Miner wants to stay still, but a Returner wants to move onto the miner's square, the Miner is forced to swap positions with the Returner. This is pretty effective at preventing traffic jams around dropoffs.

Traffic control is handled in `resolve_plans_into_commands()`.

### Mining

Mining is most important part of any bot. The idea is pretty simple: every miner scores every square on the map, and sets its destination to be the square with the best score. If two miners pick the same square, the one who is closer wins and the other one has to pick a different square.

The score for a square is an estimate of how many turns it would take to travel to that square, mine halite in that area until full, and then return to the nearest dropoff. So the lowest score is best because that means we'll deliver a full load of halite as fast as possible. The estimate of total trip time is

    (distance to square) + (estimate of time to fill up from mining) + (distance from square to dropoff)

To estimate the time to fill up from mining, we assume all nearby squares have about the same amount of halite, so that our mining rate once we get to that area will be proportional to the target square's halite, up to a fudge factor for the inefficiency of moving every few turns. So basically

    mining rate = (fudge factor) * (target square halite) / 4
    estimate of time to fill up from mining = [1000 - (ship halite)] / (mining rate)

A couple extra things: just using this formula the ships were way too eager to abandon the current square for a nearby richer square. So I multiplied `distance from square to dropoff` by a fudge factor of 1.75, and also added a rule: stay on the current square unless the target square has more than 3x the halite of the current square.

This is pretty simple but works well. At one point I spent quite a bit of effort trying to think of a way to do more sophisticated planning, where for example we would explicitly model the whole trip and search over possible mining paths to find the fastest one. But I didn't come up with anything good.

Inspiration is handled in the dumbest way possible, by treating inspired squares as if they had triple halite.

Once a ship gets to 950 halite, it goes into Return mode and heads back to the nearest dropoff. (Actually a ship may prefer to go to a slightly more distant dropoff if that dropoff is in a more halite-rich region). When returning it uses the minimum-halite-burn route out of all the routes that have the fastest travel time. 

Mining code is in `plan_mining()`. 

### Dropoffs

I have a target number of dropoffs which is `A + B * (# of ships)` where A and B were tuned for each map size and for 2p vs 4p. When we have less than the target number of dropoffs, we compute a dropoff score for every square on the map and plan to build a dropoff on the square with the highest score. The score is basically the halite in a 15x15 square centered on the square of interest. There are a couple of score modifiers:
- a penalty for squares with a high average distance to our ships
- In 4p, there is an extremely harsh penalty for squares far from any enemy ships. This is because I believed I was losing games by building a dropoff far away in a place where I would never get any inspiration.

The planned dropoff point counts as a real dropoff when computing the mining scores (in the `distance from square to dropoff` term). Also, returners can go to the planned dropoff if there are enough returners already heading to real dropoffs that we will end up with enough halite to build the planned dropoff.

I originally sent a dedicated ship to build the planned dropoff, but I switched to just building it with a returner when one eventually went there using the above logic.

I think dropoff logic is one place where my bot could have used a lot more work.

Dropoff planning code is in `consider_making_dropoff()`. 

### 2p Combat

My general principle for 2p combat was that we should like collisions when we have a local numbers advantage, since once all the collisions happen we will have some ships left over to pick up all the halite dropped on the ground.

So for each square I computed `(allied ships within radius 8) - (enemy ships within radius 8)`. When this score was positive I didn't mind collisions, and actively tried to ram higher-halite ships with lower-halite ships. When this score was negative I avoided moving adjacent to enemies and if I was adjacent to an enemy I assumed the enemy would ram and tried to run away.

I initial started with a radius much smaller than 8 but got big self-play improvements by expanding the radius to 8.

I eventually added some tweaks to this to discourage ramming my high-halite ships into enemy low-halite ships, but for a long time I was perfectly happy to ram an 800 ship into a 100 ship if I had a local numbers advantage.

2p (and 4p) combat is handled by a combination of `set_impassable()`, `consider_ramming()`, and `consider_fleeing()`. 

### 4p Combat and collision avoidance

During the 4p early and midgame there is a subtle minigame that is played over and over which is easy to miss but which can be pretty consequential. Suppose you have a low-halite ship next to an empty high-halite square. An enemy also has a low-halite ship next to the same high-halite square. Should you move onto the square to mine it?

- If you move on the square and the enemy ship stays where it is, you get to mine the square and get a nice bit of halite.
- If you stay and the enemy moves, the enemy gets the halite instead.
- If you both move, you collide and both lose a ship. Disaster for both!
- If neither you or the enemy moves, neither of you gets the halite, but at least you both keep your ships, which will be worth a lot over the course of the game.

You could formalize this with a classic 2x2 game theory payoff matrix, where there are two moves you can play, "stay", and "move".

If you know the enemy will stay, you should move; but if you know the enemy will move, you should stay. And the enemy has the same problem.

I started thinking about this because early on my 4p bot would never move adjacent to enemies, that is it would always play "stay". But I noticed that zxqfl had no such inhibition and his turtles would steadily push mine back in 4p. Always playing "move" will do very well against an opponent who always plays "stay". However two "move" players will annihilate each other. This happened pretty frequently when zxqfl and TonyK met in 4p games; it often turned out quite badly for both.

I figured that one strict improvement over my "always-stay" strategy would be:

    Never move adjacent to an enemy, unless you determine that a given player is following that 
    same "always-stay" policy, in which case always allow yourself to move adjacent to that opponent.

This exploits the cautiousness of always-stay opponents. I implemented this by writing a `PlayerAnalyzer` class that determined whether each opponent was an always-stay player.

This is better but it still doesn't fix the problem that we get exploited by always-move. Eventually I came up with a fun if complicated approach. I trained a small neural net to predict, for each opponent adjacent square, the probability that any opponent would move to that square this turn. Then I allowed my ships to move onto any square that the neural net thought was at least 98% safe. To make a prediction for a given square, the neural net used the local configuration of ships and halite, plus some stats that the PlayerAnalyzer collected for each opponent on how often they played "move" vs "stay". I trained the neural net offline using PyTorch on a few hundred of my games. Recent versions of PyTorch have a quite painless way to save a neural net constructed in python and then load and run it from C++.

This neural net idea was great fun to implement but seemed to have basically no impact on mu. Probably with some more work it could have been a net positive. It did make me more aggressive about playing "move" sometimes, so it probably did make my bot harder to exploit.

### Thoughts on the competition

The organizers did a great job of running the competition. The game was cleverly designed and fun to play, the website and matchmaking server worked great, and the organizers were very responsive and helpful. I do have some thoughts about how a Halite 4 could be even better:

- Halite 3 games can be hard to make sense of, because the final halite score arises from thousands of little decisions over the course of the game and it can be very hard to see what decisions gave an advantage to one bot over another. I found this a bit frustrating.

- I wish the game had been a bit richer in terms of allowing for more diverse strategies and a real metagame. Halite 3's simple rules are kind of elegant, but a lot of games come down to how well you can execute the single basic strategy of "mine a lot of halite really fast". There was a bit of a metagame around things like combat, dropoff blocking, and 4p collision avoidance, but these are kind of overshadowed by the mining game.

- I wouldn't mind a shorter competition; I ran out of ideas well before the end of the competition. And ideally Halite wouldn't overlap with [Battlecode](http://battlecode.org/) since plenty of people are interested in both games.

A lot of what makes a competition like this fun is the community of players -- I really enjoyed hanging out in the [discord](https://discordapp.com/invite/rbVDB4n). I also made great use of [mlomb's stats site](https://halite2018.mlomb.me/), especially the historical mu graphs.
