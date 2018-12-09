#pragma once

#include "PosMap.h"

#include <array>
#include <vector>
using namespace std;

// Collect input features and output targets for safety prediction
class PredictorDataCollector
{
  public:
    PredictorDataCollector();

    void collect();

    void save_training_data(const string &filename);    

    // A 2R+1 x 2R+1 rectangular patch of floats
    template<int R>
    struct Patch 
    {
        // float to save space on disk
        static constexpr size_t WIDTH = 2 * R + 1;
        static constexpr size_t SIZE = WIDTH * WIDTH;
        array<float, SIZE> data;

        void set(const PosMap<double> &pos_map, Vec center);
        string toString() const;        
    };

    // The input that will be used by the model to predict the safety
    // of a single square
    struct Inputs
    {
        static constexpr size_t R = 2;
        static constexpr size_t NUM_PLANES = 7;
        array<Patch<R>, NUM_PLANES> input_planes;
        static constexpr size_t NUM_SCALARS = 1;
        array<float, NUM_SCALARS> input_scalars;
        static constexpr size_t SIZE = Patch<R>::SIZE * NUM_PLANES + NUM_SCALARS;
        static vector<int32_t> get_tensor_shape() { return { SIZE }; }

        vector<float> as_flat_floats() const;

        string toString() const;
    };

    struct Outputs
    {
        bool safe;
        static vector<int32_t> get_tensor_shape() { return {1}; }

        vector<float> as_flat_floats() const;

        string toString() const;
    };

    struct Instance
    {
        Inputs inputs;
        Outputs outputs;

        string toString() const;
    };

    struct HalfInstance
    {
        Inputs inputs;
        Vec pos;  // we're waiting to see whether this square ended up being safe

        string toString() const;
    };

    const vector<HalfInstance>& get_available_inputs() const { return last_turn_half_instances; }

  private:
    vector<HalfInstance> last_turn_half_instances;
    PosMap<int> last_turn_halite;
    
    vector<Instance> instances;

    void finish_last_turn_instances();
    PosMap<bool> find_adjacent_squares(const vector<Ship> &ships);
    vector<Vec> find_interesting_squares();
    void construct_inputs(const vector<Vec> &squares);
    void remember_data_for_next_turn();
};
