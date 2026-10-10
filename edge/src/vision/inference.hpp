#pragma once

namespace edge::vision {

class InferenceEngine {
public:
    bool init();
    // IR cam + Coral TPU: motion / presence as 1 or 0
    int is_moving();
};

}  // namespace edge::vision
