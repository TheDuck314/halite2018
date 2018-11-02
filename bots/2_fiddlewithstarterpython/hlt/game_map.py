import queue

from . import constants
from .entity import Entity, Shipyard, Ship, Dropoff
from .positionals import Direction, Position
from .common import read_input


class Player:
    """
    Player object containing all items/metadata pertinent to the player.
    """
    def __init__(self, player_id, shipyard, halite=0):
        self.id = player_id
        self.shipyard = shipyard
        self.halite_amount = halite
        self._ships = {}
        self._dropoffs = {}

    def get_ship(self, ship_id):
        """
        Returns a singular ship mapped by the ship id
        :param ship_id: The ship id of the ship you wish to return
        :return: the ship object.
        """
        return self._ships[ship_id]

    def get_ships(self):
        """
        :return: Returns all ship objects in a list
        """
        return list(self._ships.values())

    def get_dropoff(self, dropoff_id):
        """
        Returns a singular dropoff mapped by its id
        :param dropoff_id: The dropoff id to return
        :return: The dropoff object
        """
        return self._dropoffs[dropoff_id]

    def get_dropoffs(self):
        """
        :return: Returns all dropoff objects in a list
        """
        return list(self._dropoffs.values())

    def has_ship(self, ship_id):
        """
        Check whether the player has a ship with a given ID.

        Useful if you track ships via IDs elsewhere and want to make
        sure the ship still exists.

        :param ship_id: The ID to check.
        :return: True if and only if the ship exists.
        """
        return ship_id in self._ships


    @staticmethod
    def _generate():
        """
        Creates a player object from the input given by the game engine
        :return: The player object
        """
        player, shipyard_x, shipyard_y = map(int, read_input().split())
        return Player(player, Shipyard(player, -1, Position(shipyard_x, shipyard_y)))

    def _update(self, num_ships, num_dropoffs, halite):
        """
        Updates this player object considering the input from the game engine for the current specific turn.
        :param num_ships: The number of ships this player has this turn
        :param num_dropoffs: The number of dropoffs this player has this turn
        :param halite: How much halite the player has in total
        :return: nothing.
        """
        self.halite_amount = halite
        self._ships = {id: ship for (id, ship) in [Ship._generate(self.id) for _ in range(num_ships)]}
        self._dropoffs = {id: dropoff for (id, dropoff) in [Dropoff._generate(self.id) for _ in range(num_dropoffs)]}


class MapCell:
    """A cell on the game map."""
    def __init__(self, position, halite_amount):
        self.position = position
        self.halite_amount = halite_amount
        self.ship = None
        self.structure = None

    @property
    def is_empty(self):
        """
        :return: Whether this cell has no ships or structures
        """
        return self.ship is None and self.structure is None

    @property
    def is_occupied(self):
        """
        :return: Whether this cell has any ships
        """
        return self.ship is not None

    @property
    def has_structure(self):
        """
        :return: Whether this cell has any structures
        """
        return self.structure is not None

    @property
    def structure_type(self):
        """
        :return: What is the structure type in this cell
        """
        return None if not self.structure else type(self.structure)

    def mark_unsafe(self, ship):
        """
        Mark this cell as unsafe (occupied) for navigation.

        Use in conjunction with GameMap.naive_navigate.
        """
        self.ship = ship

    def mark_safe(self):
        """
        Mark this cell as unsafe (occupied) for navigation.

        Use in conjunction with GameMap.naive_navigate.
        """
        self.ship = None

    def __eq__(self, other):
        return self.position == other.position

    def __ne__(self, other):
        return not self.__eq__(other)

    def __str__(self):
        return 'MapCell({}, halite={})'.format(self.position, self.halite_amount)


class GameMap:
    """
    The game map.

    Can be indexed by a position, or by a contained entity.
    Coordinates start at 0. Coordinates are normalized for you
    """
    def __init__(self, cells, width, height):
        self.width = width
        self.height = height
        self._cells = cells

    def __getitem__(self, location):
        """
        Getter for position object or entity objects within the game map
        :param location: the position or entity to access in this map
        :return: the contents housing that cell or entity
        """
        location = self.normalize(location)
        return self._cells[location.y][location.x]

    def calculate_distance(self, source, target):
        """
        Compute the Manhattan distance between two locations.
        Accounts for wrap-around.
        :param source: The source from where to calculate
        :param target: The target to where calculate
        :return: The distance between these items
        """
        source = self.normalize(source)
        target = self.normalize(target)
        resulting_position = abs(source - target)
        return min(resulting_position.x, self.width - resulting_position.x) + \
            min(resulting_position.y, self.height - resulting_position.y)

    def normalize(self, position):
        """
        Normalized the position within the bounds of the toroidal map.
        i.e.: Takes a point which may or may not be within width and
        height bounds, and places it within those bounds considering
        wraparound.
        :param position: A position object.
        :return: A normalized position object fitting within the bounds of the map
        """
        return Position(position.x % self.width, position.y % self.height)

    def offset(self, position, dx, dy):
        return self.normalize(Position(position.x + dx, position.y + dy))

    def add_dir(self, position, direction):
        dx, dy = direction
        return self.offset(position, dx, dy)

    def pos_delta(self, source, target):
        """ Returns the vector (dx, dy) such that source + (dx, dy) = target
        accounting for the torus. """
        dx = (target.x - source.x + self.width) % self.width 
        if dx >= self.width / 2:
            dx -= self.width

        dy = (target.y - source.y + self.height) % self.height
        if dy >= self.height / 2:
            dy -= self.height

        return (dx, dy)

    def approach_directions(self, source, target):
        dx, dy = self.pos_delta(source, target)

        dirs = []
        if dx > 0:
            dirs.append((-dx, Direction.East))
        elif dx < 0:
            dirs.append(( dx, Direction.West))
        if dy > 0:
            dirs.append((-dy, Direction.South))
        elif dy < 0:
            dirs.append(( dy, Direction.North))
        
        ret = [d for (_, d) in sorted(dirs)]
        if dx == 0 and dy != 0:
            ret.append(Direction.East)
            ret.append(Direction.West)
        if dy == 0 and dx != 0:
            ret.append(Direction.North)
            ret.append(Direction.South)
        return ret

    def get_1ball(self, center):
        return [self.add_dir(center, d) for d in Direction.get_all()]

    def naive_navigate(self, ship, destination):
        """
        Returns a singular safe move towards the destination.

        :param ship: The ship to move.
        :param destination: Ending position
        :return: A direction.
        """
        # No need to normalize destination, since get_unsafe_moves
        # does that
        for direction in self.get_unsafe_moves(ship.position, destination):
            target_pos = ship.position.directional_offset(direction)
            if not self[target_pos].is_occupied:
                self[target_pos].mark_unsafe(ship)
                return direction

        return Direction.Still

    @staticmethod
    def _generate():
        """
        Creates a map object from the input given by the game engine
        :return: The map object
        """
        map_width, map_height = map(int, read_input().split())
        game_map = [[None for _ in range(map_width)] for _ in range(map_height)]
        for y_position in range(map_height):
            cells = read_input().split()
            for x_position in range(map_width):
                game_map[y_position][x_position] = MapCell(Position(x_position, y_position),
                                                           int(cells[x_position]))
        return GameMap(game_map, map_width, map_height)

    def _update(self):
        """
        Updates this map object from the input given by the game engine
        :return: nothing
        """
        # Mark cells as safe for navigation (will re-mark unsafe cells
        # later)
        for y in range(self.height):
            for x in range(self.width):
                self[Position(x, y)].ship = None

        for _ in range(int(read_input())):
            cell_x, cell_y, cell_energy = map(int, read_input().split())
            self[Position(cell_x, cell_y)].halite_amount = cell_energy
