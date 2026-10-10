#include "inference.hpp"

namespace edge::vision {

bool InferenceEngine::init() {
    // TODO: open IR camera, load model from EDGE_MODELS_DIR, init Coral TPU
    return true;
}

int InferenceEngine::is_moving() {
    // Placeholder until vision pipeline is wired
    return 0;
}

}  // namespace edge::vision
