#include "Predictor.h"

#include "PredictorDataCollector.h"

#include <torch/script.h>
#include <sstream>
using namespace std;

Predictor::Predictor(PredictorDataCollector &_collector, const string &model_fn)
  : collector(_collector)
{
    Log::log("model_fn = %s", +model_fn);
    module = torch::jit::load(model_fn);
    if (module == nullptr) Log::die("Couldn't load %s", model_fn);
}

void Predictor::predict(map<Vec, double> &safety_map)
{
    const vector<PredictorDataCollector::HalfInstance> &raw_inputs = collector.get_available_inputs();
    const size_t batch_size = raw_inputs.size();
    if (raw_inputs.empty()) return;

    at::Tensor tensor_inputs = torch::zeros({(long)batch_size,
                                             PredictorDataCollector::Inputs::SIZE});
    //Log::log("tensor_inputs.toString() = %s", tensor_inputs.toString());
    //Log::log("tensor_inputs.dim() = %ld", tensor_inputs.dim());
    //Log::log("tensor_inputs.sizes()[0] = %d", tensor_inputs.sizes()[0]);
    //Log::log("tensor_inputs.sizes()[1] = %d", tensor_inputs.sizes()[1]);
    for (size_t instance_i = 0; instance_i < batch_size; ++instance_i) {
        const vector<float> floats = raw_inputs[instance_i].inputs.as_flat_floats();
        for (size_t j = 0; j < PredictorDataCollector::Inputs::SIZE; ++j) {
            tensor_inputs[instance_i][j] = floats[j];
            //Log::log("instance_i = %lu  j = %lu  number = %f", instance_i, j, floats[j]);
        }
    }
    //Log::log("tensor_inputs[0][2][1][2].item() = %f", tensor_inputs[0][2][1][2].item<float>());
    //stringstream ss;
    //ss << tensor_inputs;
    //Log::log("ss = %s", +ss.str());

    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(tensor_inputs);
    at::Tensor output = module->forward(inputs).toTensor();
    //stringstream outss;
    //outss << output;
    //Log::log("outss = %s", +outss.str());
    
    safety_map.clear();

    for (unsigned i = 0; i < batch_size; ++i) {
        const Vec pos = raw_inputs[i].pos;
        const float safety = output[i].item<float>();
        safety_map[pos] = safety;

        const float color_scale = safety * 4.0f - 3.0f;
        const int red = max(0, min(255, (int)(255 * (1.0f - color_scale))));
        const int green = max(0, min(255, (int)(255 * (color_scale))));
        Log::flog_color(pos, red, green, 0, "safety = %f    rgb(%d, %d, %d)", (double)safety, red, green, 0);
    }
    //exit(-1);
}

