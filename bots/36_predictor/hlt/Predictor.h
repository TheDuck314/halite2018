#pragma once

#include "Geometry.h"

#include <map>
#include <memory>
using namespace std;

class PredictorDataCollector;

namespace torch {
    namespace jit {
        namespace script {
            class Module;
        }
    }
}

class Predictor
{
  public:
    Predictor(PredictorDataCollector &_collector, const string &model_fn);

    struct Prediction {
        Vec pos;
        double safety;
    };

    void predict(map<Vec, double> &safety_map);

  private:
    PredictorDataCollector &collector;

    shared_ptr<torch::jit::script::Module> module;
};
