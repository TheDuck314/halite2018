#!/usr/bin/env python3
# Python 3.6

# Import the Halite SDK, which will let you interact with the game.
import hlt

# This library contains constant values.
from hlt import constants

# This library contains direction metadata to better interface with the game.
from hlt.positionals import Direction, Position

# This library allows you to generate random numbers.
import random

# Logging allows you to save messages for yourself. This is required because the regular STDOUT
#   (print statements) are reserved for the engine-bot communication.
import logging

# This game object contains the initial game state.
game = hlt.Game()
# At this point "game" variable is populated with initial map data.
# This is a good place to do computationally expensive start-up pre-processing.
# As soon as you call "ready" function below, the 2 second per turn timer will start.
game.ready("MyPythonBot")

# Now that your bot is initialized, save a message to yourself in the log file with some important information.
#   Here, you log here your id, which you can always fetch from the game object by using my_id.
logging.info("Successfully created bot! My Player ID is {}.".format(game.my_id))

# You extract player metadata and the updated map metadata here for convenience.
me = game.me
game_map = game.game_map

returning_ship_ids = set()


# plans will be a list of tuples
# (ship, target position)
# where target position may be very far away.
# There will be exactly one plan per ship. At the end of the turn
# we'll resolve all the plans into actual commands.
plans = []

def plan(ship, d):
    plans.append((ship, d))

def log(ship, msg):
    logging.info("SHIP id {:>3} at ({:>2}, {:>2}): {}".format(ship.id, ship.position.x, ship.position.y, msg))


def deliver_halite_load(ship):
    plan(ship, me.shipyard.position)


"""
def pick_mining_target(ship):
    max_halite = -1
    target_cell = None
    radius = 2
    for dx in range(-radius, radius+1):
        for dy in range(-radius, radius+1):
            cell = game_map[game_map.offset(ship.position, dx, dy)]
            if cell.is_occupied and cell.position != ship.position:
                continue
            if cell.halite_amount > max_halite:
                max_halite = cell.halite_amount
                target_cell = cell
    if target_cell is None:
        return ship.position
    return target_cell.position
"""

def pick_mining_target(ship, invalid_mining_tgts):
    # let's iterate over all the squares on the map and rate them by
    # distance and halite
    best_score = -1e99
    best_tgt = None
    for x in range(game_map.width):
        for y in range(game_map.height):
            pos = Position(x, y)
            if pos in invalid_mining_tgts:
                continue
            cell = game_map[pos]
            if cell.halite_amount <= 0 or cell.is_occupied:
                continue
            dist_from_me = game_map.calculate_distance(cell.position, ship.position)
            dist_from_base = game_map.calculate_distance(cell.position, me.shipyard.position)            
            # let's use a crude estimate of halite/turn. roughly speaking we're gonna
            # get (cell halite) / EXTRACT_RATIO halite per turn. And let's assume we can
            # mine at a similar rate from nearby cells. Then it'll take this long to fill up:
            mine_rate = cell.halite_amount / float(constants.EXTRACT_RATIO)
            turns_to_fill_up = constants.MAX_HALITE / mine_rate
            # multiply the mining rate by the fraction of time we'll actually be mining
            overall_rate = (mine_rate * turns_to_fill_up) / (turns_to_fill_up + dist_from_base + 5 * dist_from_me)
            if overall_rate > best_score:
                best_score = overall_rate
                best_tgt = cell.position
    log(ship, "mining target {} score = {}".format(best_tgt, best_score))
    if best_tgt is None:
        best_tgt = ship.position        
    return best_tgt



def find_path(src, dest, blocked_positions):
#    logging.info("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")
    logging.info("find_path src={} dest={} blocked_positions={}".format(src, dest, blocked_positions))
    if src == dest:
#        logging.info("trivial b/c src = dest")
        return Direction.Still
    if dest in blocked_positions:
#        logging.info("trivial b/c dest is blocked!")
        return Direction.Still
    finished_positions = set(blocked_positions)
    finished_positions.add(src)
    cardinals = Direction.get_all_cardinals()
    pos_queue = []
    for d in cardinals:
        adj = game_map.add_dir(src, d)
        if adj == dest:
#            logging.info("adjacent case triggered, d = {}".format(d))
            return d
        if adj in finished_positions:
#            logging.info("adjacent thing adj={} d={} is finished already".format(adj, d))
            continue            
        pos_queue.append((adj, d))
        finished_positions.add(adj)
    i = 0
    while i < len(pos_queue):
        pos, initial_dir = pos_queue[i]
#        logging.info("popped pos = {}  initial_dir = {}".format(pos, initial_dir))
        i += 1
        for d in cardinals:
            adj = game_map.add_dir(pos, d)
#            logging.info("d = {}  adj = {}".format(d, adj))
            if adj in finished_positions:                
#                logging.info("adj is already finished, forget it")
                continue
            if adj == dest:
#                logging.info("found dest! initial_dir = {}".format(initial_dir))
                return initial_dir
            pos_queue.append((adj, initial_dir))
            finished_positions.add(adj)
#    logging.info("failed to find a route!!!!")
    return Direction.Still


def resolve_plans_into_commands():
    # keep a queue of ships that we need to route. do this loop:
    # - pop a ship off the queue
    # - find a move for this ship that doesn't hit a currently-blocked square
    # - if successful
    #     - dest square of this ship becomes blocked
    #     - remember the dest square -> ship mapping, because we may need to undo this
    # - if unsuccessful
    #     - stay put
    #     - if another ship is trying to come to this square, kick it out and put it back in the queue
    global plans
    ship_id_to_tgt = {ship.id: tgt for (ship, tgt) in plans}

    command_queue = []

    # build a set of positions we must not move onto.
    # for now, just the positions of enemy ships, or positions they
    # could move to next turn
    avoid_positions = set()
    for player_id, player in game.players.items():
        if player_id != game.my_id:
            for enemy_ship in player.get_ships():
                pos = enemy_ship.position
                structure = game_map[pos].structure 
                # don't avoid positions that contain our own structures
                # it's fine to ram enemy ships on those positions because the
                # halite will just fall onto our structure
                if not (structure and structure.owner == me.id):
                    for d in Direction.get_all():
                        avoid_positions.add(game_map.add_dir(enemy_ship.position, d))

    never_avoid_positions = set()
    if game.turn_number > constants.MAX_TURNS - 15:
        # pile onto the structures, even if it means collisions
        never_avoid_positions = set(me.get_dropoffs() + [me.shipyard.position])
        # hack: if there are multiple ships within 1 of a strucutre, they are all 
        # gonna go onto the same square, and our pos_to_arriving_ship framework 
        # doesn't really handle that. So just make commands for those guys right now,
        # and remove them from plans.
        handled_ship_ids = set()
        for ship in me.get_ships():
            for dest in never_avoid_positions:
                if game_map.calculate_distance(ship.position, dest) <= 1:
                    d = game_map.pos_delta(ship.position, dest)
                    command_queue.append(ship.move(d))
                    handled_ship_ids.add(ship.id)
        plans = [(ship, tgt) for (ship, tgt) in plans if ship.id not in handled_ship_ids]

    logging.info("going to resolve these plans into commands:\n{}\n".format("\n".join(map(repr, plans))))

    # the goal is to get every ship into this backwards map from 
    # "position we're moving onto this turn" to "ship"    
    pos_to_arriving_ship = {}

    # little optimization:
    # let's first handle all the plans that just require staying put.
    # this'll make pathfinding better since it will know to path around these guys
    unhandled_plans = []
    for (ship, tgt) in plans:
        if ship.position == tgt:
            pos_to_arriving_ship[tgt] = ship
        else:
            unhandled_plans.append((ship, tgt))
    plans = unhandled_plans

    i = 0  # gonna use plans as a crude queue with pop() implemented as i += 1
    while i < len(plans):
        ship, tgt = plans[i]
        i += 1
        logging.info("\n{} wants to go to {}".format(ship, tgt))

        staying_put = (ship.position == tgt)  # todo: can get rid of this b/c of the above optimization
        if not staying_put:
            blocked_positions = (set(pos_to_arriving_ship.keys()) | avoid_positions) - never_avoid_positions
            path_dir = find_path(ship.position, tgt, blocked_positions)
            if path_dir != Direction.Still:
                d_pos = game_map.add_dir(ship.position, path_dir)
                pos_to_arriving_ship[d_pos] = ship
                logging.info("tentatively moving toward {}".format(d_pos))
            else:
                # fall back to a dumber pathfinding algorithm
                cand_dirs = game_map.approach_directions(ship.position, tgt)
                staying_put = True  # unless we can actually find a move
                for d in cand_dirs:
                    d_pos = game_map.add_dir(ship.position, d)
                    if d_pos in blocked_positions:
                        continue  # can't move there
                    # found a valid move
                    pos_to_arriving_ship[d_pos] = ship
                    logging.info("tentatively moving toward {}".format(d_pos))
                    staying_put = False
                    break
        if staying_put:
            logging.info("staying put")
            # either we planned to stay put, or we couldn't find a move
            if ship.position in pos_to_arriving_ship:
                # make the interloper rethink his move -- put him back in the queue
                interloper = pos_to_arriving_ship[ship.position]
                logging.info("kicking out {} who thought he was going to come into my square!".format(interloper))
                plans.append((interloper, ship_id_to_tgt[interloper.id]))
            pos_to_arriving_ship[ship.position] = ship

    logging.info("\nOK, finally we have pos_to_arriving_ship = \n{}".format(pos_to_arriving_ship))

    # mark everything safe, except the squares to which ships are coming
    for x in range(game_map.width):
        for y in range(game_map.height):
            game_map[Position(x, y)].mark_safe()

    for dest, ship in pos_to_arriving_ship.items():
        # remember, Directions are actually just pos deltas
        direction = game_map.pos_delta(ship.position, dest)
        logging.info("{} moving in direction {} (to {})".format(ship, direction, dest))
        command_queue.append(ship.move(direction))
        game_map[dest].mark_unsafe(ship)
    return command_queue


def get_enemy_ships():
    ret = []
    for player_id, player in game.players.items():
        if player_id != game.my_id:
            ret.extend(player.get_ships())
    return ret

def should_become_dropoff(ship):
    # check that we can pay the cost
    if me.halite_amount < constants.DROPOFF_COST:
        return False

    # don't make dropoffs too near the end, or they won't pay for themselves
    if constants.MAX_TURNS - game.turn_number < 50:
        return False
    ships = me.get_ships()
    # probably dropoffs don't help much when we don't have many ships
    if len(ships) < 10:
        return False
    existing = me.get_dropoffs() + [me.shipyard.position]
    for e in existing:
        if game_map.calculate_distance(ship.position, e) < 20:
            return False
#    radius = 6
#    min_halite_in_radius = 5000  # probably too small
#    halite_in_radius = 0
#    for dx in range(-radius, radius+1):
#        for dy in range(-radius, radius+1):
#            halite_in_radius += game_map[game_map.offset(ship.position, dx, dy)].halite_amount
#    # todo: maybe include halite carried by ships
#    if halite_in_radius < min_halite_in_radius:
#        return False
    return True


def mainloop():
    spawned_last_turn = False
    my_halite_last_turn = 5000
    total_halite_collected = 0 

#    find_path(Position(10, 10), Position(13, 10), {Position(11, 10)})

    global plans
    while True:
        # This loop handles each turn of the game. The game object changes every turn, and you refresh that state by
        #   running update_frame().
        game.update_frame()

#        if game.turn_number == 300: raise Exception('done')

        halite_income_this_turn = me.halite_amount - my_halite_last_turn
        if spawned_last_turn:
            # don't interpret ship cost as negative income
            halite_income_this_turn += constants.SHIP_COST
        total_halite_collected += halite_income_this_turn
        my_halite_last_turn = me.halite_amount
        spawned_last_turn = False
        logging.info("me.halite_amount        = {}".format(me.halite_amount))
        logging.info("me.halite_amount        = {}".format(me.halite_amount))
        logging.info("halite_income_this_turn = {}".format(halite_income_this_turn))
        logging.info("total_halite_collected  = {}".format(total_halite_collected))

        turns_left = constants.MAX_TURNS - game.turn_number

        plans.clear()

        invalid_mining_tgts = set()
        for enemy_ship in get_enemy_ships():
            for pos in game_map.get_1ball(enemy_ship.position):
                invalid_mining_tgts.add(pos)

        made_dropoff = False

        command_queue = []

        for ship in me.get_ships():
            cell = game_map[ship.position]

            if not made_dropoff and should_become_dropoff(ship):
                log(ship, "making dropoff")
                command_queue.append(ship.make_dropoff())
                made_dropoff = True
                me.halite_amount -= constants.DROPOFF_COST
                continue

            # if we aren't carrying enough halite to move off this square then we have
            # to just stay put. check that first.
            move_cost = cell.halite_amount / constants.MOVE_COST_RATIO
            if ship.halite_amount < move_cost:
                plan(ship, ship.position)
                continue

            dist_to_delivery = game_map.calculate_distance(ship.position, me.shipyard.position)
            time_for_final_delivery = turns_left < dist_to_delivery + 15

            if ship.is_full or time_for_final_delivery:
                returning_ship_ids.add(ship.id)
            if ship.is_empty and not time_for_final_delivery:
                returning_ship_ids.discard(ship.id)

            if ship.id in returning_ship_ids:
                # return halite to base
                log(ship, "delivering halite load")
                deliver_halite_load(ship)
            else:
                tgt = pick_mining_target(ship, invalid_mining_tgts)
                tgt_halite = game_map[tgt].halite_amount
                here_halite = game_map[ship.position].halite_amount
                if tgt_halite > 10 * here_halite:
                    log(ship, "going to move toward mining target {}, {}".format(tgt.x, tgt.y))
                    plan(ship, tgt)
                else:
                    log(ship, "staying at {}, {}".format(ship.position.x, ship.position.y))
                    plan(ship, ship.position)

        # construct all the actual commands from our plans
        command_queue.extend(resolve_plans_into_commands())

        stop_ship_building_turn = constants.MAX_TURNS - 200

        # If the game is in the first 200 turns and you have enough halite, spawn a ship.
        # Don't spawn a ship if you currently have a ship at port, though - the ships will collide.
        if game.turn_number <= stop_ship_building_turn and me.halite_amount >= constants.SHIP_COST and not game_map[me.shipyard.position].is_occupied:
            logging.info("SPAWNING AT SHIPYARD {}, {}".format(me.shipyard.position.x, me.shipyard.position.y))
            command_queue.append(me.shipyard.spawn())
            spawned_last_turn = True

        # Send your moves back to the game environment, ending this turn.
        game.end_turn(command_queue)



if __name__ == "__main__":
    mainloop()
