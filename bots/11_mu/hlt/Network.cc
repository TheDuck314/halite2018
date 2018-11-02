#include "Network.h"

#include <iostream>
#include <sstream>
#include "Constants.h"
#include "Game.h"
#include "Log.h"
#include "Grid.h"

static const char* NAME = "Mu";

static string read_line()
{
    string line;
    std::getline(std::cin, line);
    //Log::log("read line: '%s'", line.c_str());
    return line;
}

static void send_line(const string& line)
{
    //Log::log("sending line: '%s'", line.c_str());
    std::cout << line << endl;
}

static vector<int> read_ints()
{
    stringstream input_stream(read_line());
    vector<int> values;

    while (true) {
        string token;
        if (!std::getline(input_stream, token, ' ')) {
            return values;
        }
        if (!token.empty()) {
            values.push_back(stoi(token));
        }
    }
}

void Network::init()
{
    string line;
    std::getline(std::cin, line);

    Constants::parse(line);

    // we get 'num_players my_player_id'
    vector<int> values = read_ints();
    Game::num_players = values[0];
    Game::my_player_id = values[1];
    Game::players.resize(Game::num_players);
    Game::me = &Game::players[Game::my_player_id];

    Log::init(Game::my_player_id);
    Log::log("Begin logging");

    // for each player we get 'playerid shipyard_x shipyard_y'
    for (int i = 0; i < Game::num_players; ++i) {
        values = read_ints();
        int player_id = values[0];
        if (player_id >= Game::num_players) {
            Log::die("player_id was %d >= Game::num_players = %d", player_id, Game::num_players);
        }
        Game::players[player_id].id = player_id;
        Game::players[player_id].shipyard.x = values[1];
        Game::players[player_id].shipyard.y = values[2];
    }

    Log::log("Constants.MAX_TURNS = %d", Constants::MAX_TURNS);
    Log::log("Game::num_players = %d", Game::num_players);
    Log::log("Game::my_player_id = %d", Game::my_player_id);
    Log::log("Game::players[0] = %s", Game::players[0].toString().c_str());

    // we get 'gridwidth gridheight'
    values = read_ints();
    grid.resize(values[0], values[1]);
    Log::log("Grid width=%d height=%d", grid.width, grid.height);

    // we get the width x height grid of halite on each cell
    for (int y = 0; y < grid.height; ++y) {
        values = read_ints();
        if ((int)values.size() != grid.width) {
            Log::die("reading initial grid, expected width=%d ints but got %d instead",
                     grid.width, (int)values.size());
        }
        for (int x = 0; x < grid.width; ++x) {
            grid(x, y).halite = values[x];
            //Log::log("grid(%d, %d).halite = %d", x, y, grid(x, y).halite);
        }
    }
    
    // That's all the initial info we get. Once we're done initializing the game expects us to respond with our name
    // TODO: could call a function to do some initial computation here.
    send_line(NAME);
}


void Network::begin_turn()
{
    // we get the turn number
    vector<int> values = read_ints();
    Game::turn = values[0];
    Log::log("============================ TURN %d ==========================", Game::turn);

    // For each player we get 'player num_ships num_dropoffs halite'
    for (int i = 0; i < Game::num_players; ++i) {
        values = read_ints();
        const int player_id = values[0];
        Player &player = Game::players[player_id];
        player.ships.resize(values[1]);
        player.dropoffs.resize(values[2]);
        player.halite = values[3];

        // for each ship of this player we get 'ship_id x_position y_position halite'
        for (Ship &ship : player.ships) {
            values = read_ints();
            ship.id = values[0];
            ship.pos.x = values[1];
            ship.pos.y = values[2];
            ship.halite = values[3];
        }

        // for each dropoff of this player we get 'id x_position y_position'
        for (Dropoff &dropoff : player.dropoffs) {
            values = read_ints();
            dropoff.id = values[0];
            dropoff.pos.x = values[1];
            dropoff.pos.y = values[2];
        }

        player.post_update();

        Log::log("read info for player %d: %s", player_id, player.toLongString().c_str());
    }

    // we get an integer telling us how many cells are about to have their halite updated:
    values = read_ints();
    const int num_cell_updates = values[0];
    for (int i = 0; i < num_cell_updates; ++i) {
        // Updates have the form 'cell_x cell_y cell_halite'
        values = read_ints();
        const int x = values[0];
        const int y = values[1];
        const int halite = values[2];
        grid(x, y).halite = halite;
    }

    Game::post_update();
}


void Network::end_turn(const vector<Command> &commands)
{
    string big_cmd_string;
    if (commands.size() > 0) {
        big_cmd_string += commands[0].encode();
    }
    for (unsigned i = 1; i < commands.size(); ++i) {
        big_cmd_string += " ";
        big_cmd_string += commands[i].encode();
    }
    send_line(big_cmd_string);
}