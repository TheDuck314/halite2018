#include "PredictorDataCollector.h"

#include "Game.h"
#include "Grid.h"
#include "PlayerAnalyzer.h"

PredictorDataCollector::PredictorDataCollector()
  : last_turn_halite(0)
{}


template<int R>
void PredictorDataCollector::Patch<R>::set(const PosMap<double> &pos_map, Vec center)
{
    int i = 0;
    for (int dy = -R ; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
            Vec pos = grid.add(center, dx, dy);
            this->data[i++] = (float)pos_map(pos);
        }
    }
}

vector<float> PredictorDataCollector::Inputs::as_flat_floats() const
{
    vector<float> ret(SIZE);
    size_t offset = 0;
    for (unsigned i = 0; i < NUM_PLANES; ++i) {
        std::copy(input_planes[i].data.begin(), input_planes[i].data.end(),
                  ret.begin() + offset);
        offset += Patch<R>::SIZE;
    }
    std::copy(input_scalars.begin(), input_scalars.end(),
              ret.begin() + offset);
    return ret;
}

vector<float> PredictorDataCollector::Outputs::as_flat_floats() const
{
    const float safe_as_float = safe ? 1.0f : 0.0f;
    return { safe_as_float };
}

template<int R>
string PredictorDataCollector::Patch<R>::toString() const
{
    string ret;
    int i = 0;
    for (int dx = -R ; dx <= R; ++dx) {
        for (int dy = -R; dy <= R; ++dy) {
            ret += stringf("%.3f ", this->data[i++]);
        }
        ret += "\n";
    }
    return ret;
}

string PredictorDataCollector::Inputs::toString() const
{
    string ret("Inputs(\n");
    for (unsigned i = 0; i < input_planes.size(); ++i) {
        ret += stringf("input plane %u:\n", i);
        ret += input_planes[i].toString();
    }
    ret += "input_scalars:\n";
    for (float f : input_scalars) {
        ret += stringf("%f\n", f);
    }
    ret += ")\n";
    return ret;
}

string PredictorDataCollector::Outputs::toString() const
{
    return stringf("Outputs(safe=%d)", safe);
}

string PredictorDataCollector::Instance::toString() const
{
    return stringf("Instance(inputs=%s,\noutputs=%s)", +inputs.toString(), +outputs.toString());
}

string PredictorDataCollector::HalfInstance::toString() const
{
    return stringf("Instance(inputs=%s,\npos=%s)", +inputs.toString(), +pos.toString());
}

void PredictorDataCollector::collect()
{
    // First determine the correct outputs for all the inputs we wrote down next turn
    finish_last_turn_instances();

    // Determine which squares we want to assess the safe of (squares adjacent to an ally
    // and also adjacent to an enemy)
    const vector<Vec> squares_to_evaluate = find_interesting_squares();

    // Write down the inputs for those squares, and remember them so we can calculate the
    // outputs next turn
    construct_inputs(squares_to_evaluate);

    remember_data_for_next_turn();
}

void PredictorDataCollector::finish_last_turn_instances()
{
    if (last_turn_half_instances.empty()) return;  // this will be true for the first turn

    // construct a map of which squares ended up being safe
    PosMap<bool> safe(true);

    // squares that now have enemy ships on them were unsafe
    for (Ship enemy_ship : Game::enemy_ships) {
        safe(enemy_ship.pos) = false;
        //Log::log("%s unsafe b/c enemy ship present", +enemy_ship.pos.toString());
    }

    // squares whose halite went up were unsafe, since there must have been a collision
    for (Vec pos : grid.positions) {
        if (grid(pos).halite > last_turn_halite(pos)) {
            safe(pos) = false;
            //Log::log("%s unsafe b/c halite went up", +pos.toString());
        }
    }

    // Now that we know which squares ended up being unsafe, go through all the inputs we wrote 
    // down last turn and associate them with the correct output, then add the completed instance
    // to the list of all completed instances.
    for (const HalfInstance &half : last_turn_half_instances) {
        Outputs outputs;
        outputs.safe = safe(half.pos);
        instances.push_back({half.inputs, outputs});
        //Log::log("Created full instance from half-instance at %s:\n%s", +half.pos.toString(), +instances.back().toString());
    }

    last_turn_half_instances.clear();
}

PosMap<bool> PredictorDataCollector::find_adjacent_squares(const vector<Ship> &ships)
{
    PosMap<bool> adjacent(false);
    for (Ship s : ships) {
        for (int d = 0; d < 5; ++d) {
            const Vec adj = grid.add(s.pos, (Direction)d);
            adjacent(adj) = true;
        }
    }
    return adjacent;
}

vector<Vec> PredictorDataCollector::find_interesting_squares()
{
    const PosMap<bool> adjacent_to_enemy = find_adjacent_squares(Game::enemy_ships);
    const PosMap<bool> adjacent_to_ally  = find_adjacent_squares(Game::me->ships);

    PosMap<bool> has_enemy_ship(false);
    for (Ship s : Game::enemy_ships) has_enemy_ship(s.pos) = true;
    PosMap<bool> has_allied_ship(false);
    for (Ship s : Game::me->ships) has_allied_ship(s.pos) = true;

    vector<Vec> ret;
    for (Vec v : grid.positions) {
        // for now, let's not worry about squares that actually have an enemy ship
        // on them. It's more interesting to predict squares that don't already have an
        // enemy ship. 
        // Also we're going to exclude squares that already have an allied ship b/c
        // ramming is quite rare in 4p
        if (adjacent_to_enemy(v) && adjacent_to_ally(v) && !has_enemy_ship(v) && !has_allied_ship(v)) {
            ret.push_back(v);
        }
    }
    return ret;
}

static double normalize_halite(int halite)
{
    return halite / 500.0 - 1.0;  // map [0, 1000] to [-1.0, 1.0]
}

void PredictorDataCollector::construct_inputs(const vector<Vec> &squares)
{
    // first, construct PosMaps of the features we care about
    PosMap<double> normalized_halite(0.0);
    PosMap<double> enemy_ship_present(0.0);
    PosMap<double> allied_ship_present(0.0);
    PosMap<double> enemy_ship_normalized_halite(0.0);
    PosMap<double> allied_ship_normalized_halite(0.0);
    PosMap<double> enemy_ship_unsafe_propensity(0.0);
    PosMap<double> enemy_ship_ram_propensity(0.0);
    for (Vec pos : grid.positions) {
        normalized_halite(pos) = normalize_halite(grid(pos).halite);  
    }
    for (Ship s : Game::enemy_ships) {
        enemy_ship_present(s.pos) = 1.0;
        enemy_ship_normalized_halite(s.pos) = normalize_halite(s.halite);
    }
    for (Ship s : Game::me->ships) {
        allied_ship_present(s.pos) = 1.0;
        allied_ship_normalized_halite(s.pos) = normalize_halite(s.halite);
    }
    for (const Player &enemy : Game::players) {
        if (enemy.id == Game::my_player_id) continue;
        const auto &stats = PlayerAnalyzer::getStats(enemy.id);
        // multiplying by 5.0 b/c even TonyK only gets up to ~20% unsafe propensity
        const double unsafe_propensity = 5.0 * stats.moved_to_unsafe_count / (double)(1 + stats.at_risk_count);
        // starts at 0, then climbs toward 1.0 the more times this enemy has rammed me
        const double ram_propensity = 1.0 - 1.0 / (1.0 + 0.5 * stats.num_times_rammed_my_still_ship);
        for (Ship s : enemy.ships) {
            enemy_ship_unsafe_propensity(s.pos) = unsafe_propensity;
            enemy_ship_ram_propensity(s.pos) = ram_propensity;
        }
    }


    for (Vec pos : squares) {
        Inputs inputs;
        // planes
        inputs.input_planes.at(0).set(normalized_halite, pos);
        inputs.input_planes.at(1).set(enemy_ship_present, pos);
        inputs.input_planes.at(2).set(allied_ship_present, pos);
        inputs.input_planes.at(3).set(enemy_ship_normalized_halite, pos);
        inputs.input_planes.at(4).set(allied_ship_normalized_halite, pos);
        inputs.input_planes.at(5).set(enemy_ship_unsafe_propensity, pos);
        inputs.input_planes.at(6).set(enemy_ship_ram_propensity, pos);
        // scalars
        inputs.input_scalars[0] = Game::turns_left() / 500.0f;  // normalized turns left
        last_turn_half_instances.push_back({inputs, pos});

        //Log::log("Created half instance:\n%s", +last_turn_half_instances.back().toString());
    }
}

void PredictorDataCollector::remember_data_for_next_turn()
{
    // TODO: once grid halite is a posmap this is simpler
    for (Vec v : grid.positions) {
        last_turn_halite(v) = grid(v).halite;
    }
}

static void write_int32(ofstream &f, int32_t x)
{
    f.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

static void write_tensor_rank_and_shape(ofstream &f, const vector<int32_t> &shape)
{
    // write rank
    write_int32(f, shape.size());
    // write shape
    for (int32_t x : shape) {
        write_int32(f, x);
    }
}

static void write_floats(ofstream &f, const vector<float> &floats)
{
    f.write(reinterpret_cast<const char*>(floats.data()), floats.size() * sizeof(float));
}

void PredictorDataCollector::save_training_data(const string &filename)
{
    Log::log("Saving training data to %s", +filename);
    ofstream f(+filename, std::ios::binary);
    if (f.fail()) Log::die("Couldn't open file to save training data! %s", +filename);

    // Let's use the following format:
    // HEADER:
    // - int32: number of instances
    // - rank of the input tensor for one instance (currently 1)
    // - <rank> integers giving the shape of the input tensor (currently (5*5*3,) = (75,))
    // - rank of the output tensor for one instance (current 1)
    // - <rank> integers giving the shape of the output tensor (currently (1,))
    // INPUTS: one batch (currently 75) float32's for each instance
    // OUTPUTS: one batch of float32's (well, just 1 currently) for each each instance

    // write the header:
    write_int32(f, instances.size());    
    write_tensor_rank_and_shape(f, Inputs::get_tensor_shape());
    write_tensor_rank_and_shape(f, Outputs::get_tensor_shape());

    // write the input tensors
    for (const Instance &instance : instances) {
        write_floats(f, instance.inputs.as_flat_floats());
    }

    // write the output tensors
    for (const Instance &instance : instances) {
        write_floats(f, instance.outputs.as_flat_floats());
    }
}
