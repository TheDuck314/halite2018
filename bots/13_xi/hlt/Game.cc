#include "Game.h"

int Game::num_players;
int Game::my_player_id;
vector<Player> Game::players;
Player* Game::me;
int Game::turn;
vector<Vec> Game::enemy_structures;
vector<Ship> Game::enemy_ships;

void Game::post_update()
{
    // populate enemy_structures
    enemy_structures.clear();
    for (const Player &p : players) {
        if (p.id == my_player_id) continue;
        std::copy(p.structures.begin(), p.structures.end(), std::back_inserter(enemy_structures));
    }

    // populate enemy_ships
    enemy_ships.clear();
    for (const Player &p : players) {
        if (p.id == my_player_id) continue;
        std::copy(p.ships.begin(), p.ships.end(), std::back_inserter(enemy_ships));
    }

}